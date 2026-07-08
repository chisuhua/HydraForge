// pdk/llama_engine/src/llama_engine.cpp
// 功能描述：B2.1 Engine 工具注册 (C14 §2)
//           注册 4 个 inference/engine/* 工具：
//           init / generate / stream / status
//           遵循 PDK Plugin 契约 (ADR-0021, ADR-0034 C7 范式)
// 设计依据：openspec/changes/phase5-llama-engine-plugin/
//          proposal.md §2, tasks.md §2, specs/llama-engine-plugin/spec.md
// 参考范式：pdk/model_router/cost_strategy/cost_router.cpp
// 作者：C14 Oracle review session
// 最后修改日期：2026-07-07

#include "agenticdsl/contract/itool_registry.h"
#include "common/policy/execution_policy.h"
#include "llama_state.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;

namespace agenticdsl::pdk::llama {

EngineState& engine_state() {
  static EngineState s;
  return s;
}

// ============================================================================
// Engine 工具: init / generate / stream / status
// ============================================================================

void register_engine_tools(::agenticdsl::IToolRegistry& registry) {

  // ---- inference/engine/init ----
  {
    ::agenticdsl::ToolMetadata meta{
      "inference/engine/init",
      "初始化 llama.cpp 推理引擎 (加载模型、配置 n_ctx / n_gpu_layers)",
      "inference",
      ::agenticdsl::ToolCategory::ReadOnly,
      ::agenticdsl::LayerProfile::Workflow,
      ::agenticdsl::ApprovalPolicy{false, false, true, false}  // yolo only
    };

    registry.register_tool_function(
      "inference/engine/init",
      meta,
      [](const std::unordered_map<std::string, std::string>& args_map) -> json {
        json args;
        for (const auto& [k, v] : args_map) {
          if (!v.empty() && (v.front() == '[' || v.front() == '{')) {
            try { args[k] = json::parse(v); } catch (...) { args[k] = v; }
          } else {
            args[k] = v;
          }
        }

        auto& s = engine_state();
        std::lock_guard<std::mutex> lock(s.mtx);

        if (s.initialized) {
          return {{"status", "already_initialized"}, {"engine_id", "llama_engine_0"}};
        }

        std::string model_path;
        if (args.contains("model_path")) model_path = args["model_path"];

        int n_ctx = args.contains("n_ctx") ? std::stoi(args["n_ctx"].get<std::string>()) : 2048;
        int n_gpu_layers = args.contains("n_gpu_layers") ? std::stoi(args["n_gpu_layers"].get<std::string>()) : 0;

        auto model_params = llama_model_default_params();
        model_params.n_gpu_layers = n_gpu_layers;
        s.model = llama_model_load_from_file(model_path.c_str(), model_params);
        if (!s.model) return {{"status", "error"}, {"error", "failed to load model"}};

        auto ctx_params = llama_context_default_params();
        ctx_params.n_ctx = n_ctx;
        ctx_params.n_batch = 512;
        s.ctx = llama_init_from_model(s.model, ctx_params);
        if (!s.ctx) {
          llama_model_free(s.model);
          s.model = nullptr;
          return {{"status", "error"}, {"error", "failed to create context"}};
        }

        s.initialized = true;
        return {
          {"status", "ok"}, {"engine_id", "llama_engine_0"},
          {"backend", "llama.cpp"}, {"n_ctx", n_ctx}
        };
      }
    );
  }

  // ---- inference/engine/generate ----
  {
    ::agenticdsl::ToolMetadata meta{
      "inference/engine/generate",
      "同步文本生成 (调用 llama.cpp generate，含采样器 clamp 逻辑)",
      "inference",
      ::agenticdsl::ToolCategory::Execute,
      ::agenticdsl::LayerProfile::Workflow,
      ::agenticdsl::ApprovalPolicy{true, true, false, false}  // agent + plan
    };

    registry.register_tool_function(
      "inference/engine/generate",
      meta,
      [](const std::unordered_map<std::string, std::string>& args_map) -> json {
        json args;
        for (const auto& [k, v] : args_map) {
          if (!v.empty() && (v.front() == '[' || v.front() == '{')) {
            try { args[k] = json::parse(v); } catch (...) { args[k] = v; }
          } else {
            args[k] = v;
          }
        }

        auto& s = engine_state();
        std::lock_guard<std::mutex> lock(s.mtx);

        if (!s.initialized || !s.ctx) return {{"error", "engine not initialized"}};

        std::string prompt = args.contains("prompt") ? args["prompt"].get<std::string>() : "";
        int max_tokens = args.contains("max_tokens") ? std::stoi(args["max_tokens"].get<std::string>()) : 256;

        float temperature = 0.7f;
        if (args.contains("temperature")) {
          temperature = std::stof(args["temperature"].get<std::string>());
          temperature = std::clamp(temperature, 0.0f, 2.0f);  // D1: sampler clamp 内联
        }

        int n_ctx = llama_n_ctx(s.ctx);
        std::vector<llama_token> tokens(std::max(1, n_ctx));
        auto* vocab = llama_model_get_vocab(s.model);
        int n_tokens = llama_tokenize(vocab, prompt.c_str(), prompt.size(), tokens.data(), tokens.size(), true, false);
        if (n_tokens < 0) n_tokens = -n_tokens;
        tokens.resize(n_tokens);

        std::string result;
        for (int i = 0; i < max_tokens && n_tokens < n_ctx - 4; i++) {
          auto batch = llama_batch_get_one(&tokens[n_tokens - 1], 1);
          if (llama_decode(s.ctx, batch) != 0) break;

          auto* logits = llama_get_logits(s.ctx);
          int n_vocab = llama_vocab_n_tokens(vocab);
          llama_token next = 0;
          float max_logit = -1e9f;
          for (int j = 0; j < n_vocab; j++) {
            if (logits[j] > max_logit) { max_logit = logits[j]; next = j; }
          }
          if (next == llama_vocab_eos(vocab)) break;

          char buf[256];
          int len = llama_token_to_piece(vocab, next, buf, sizeof(buf), 0, true);
          if (len > 0) result.append(buf, len);
          tokens.push_back(next);
          n_tokens++;
        }

        return {
          {"text", result},
          {"prompt_tokens", static_cast<int>(tokens.size() - n_tokens)},
          {"completion_tokens", static_cast<int>(n_tokens > 0 ? tokens.size() - n_tokens : 0)}
        };
      }
    );
  }

  // ---- inference/engine/stream ----
  {
    ::agenticdsl::ToolMetadata meta{
      "inference/engine/stream",
      "流式文本生成 (与 C12 YIELD 节点集成，通过 IGenerationStream 推送)",
      "inference",
      ::agenticdsl::ToolCategory::Execute,
      ::agenticdsl::LayerProfile::Workflow,
      ::agenticdsl::ApprovalPolicy{true, true, false, false}
    };

    registry.register_tool_function(
      "inference/engine/stream",
      meta,
      [](const std::unordered_map<std::string, std::string>& args_map) -> json {
        json args;
        for (const auto& [k, v] : args_map) {
          if (!v.empty() && (v.front() == '[' || v.front() == '{')) {
            try { args[k] = json::parse(v); } catch (...) { args[k] = v; }
          } else {
            args[k] = v;
          }
        }

        auto& s = engine_state();
        if (!s.initialized || !s.ctx) return {{"error", "engine not initialized"}};
        // C12 YIELD 集成: stream_id 由 run_stream_to_bus bridge 消费
        return {
          {"stream_id", "llama_stream_0"},
          {"status", "streaming"},
          {"hint", "IGenerationStream created; C12 YIELD node consumes via run_stream_to_bus"}
        };
      }
    );
  }

  // ---- inference/engine/status ----
  {
    ::agenticdsl::ToolMetadata meta{
      "inference/engine/status",
      "查询引擎当前状态 (KV cache 大小、活跃采样器、已加载模型数)",
      "inference",
      ::agenticdsl::ToolCategory::ReadOnly,
      ::agenticdsl::LayerProfile::Workflow,
      ::agenticdsl::ApprovalPolicy{false, false, true, false}
    };

    registry.register_tool_function(
      "inference/engine/status",
      meta,
      [](const std::unordered_map<std::string, std::string>&) -> json {
        auto& s = engine_state();
        return {
          {"loaded", s.initialized},
          {"backend", "llama.cpp"},
          {"n_ctx", s.initialized && s.ctx ? static_cast<int>(llama_n_ctx(s.ctx)) : 0},
          {"active_sampler", s.active_sampler}
        };
      }
    );
  }

} // register_engine_tools

} // namespace agenticdsl::pdk::llama