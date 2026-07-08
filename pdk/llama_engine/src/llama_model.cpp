// pdk/llama_engine/src/llama_model.cpp
// 功能描述：B2.2 Model 工具注册 (C14 §3)
//           注册 4 个 inference/model/* 工具：
//           load / unload / list / switch
//           遵循 PDK Plugin 契约 (ADR-0021, ADR-0034 C7 范式)
// 设计依据：openspec/changes/phase5-llama-engine-plugin/
//          proposal.md §3, tasks.md §3, specs/llama-engine-plugin/spec.md
// 作者：C14 Oracle review session
// 最后修改日期：2026-07-07

#include "agenticdsl/contract/itool_registry.h"
#include "common/policy/execution_policy.h"
#include "llama_state.h"

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

using json = nlohmann::json;

namespace agenticdsl::pdk::llama {

// ============================================================================
// Model 工具: load / unload / list / switch
// ============================================================================

void register_model_tools(::agenticdsl::IToolRegistry& registry) {

  // ---- inference/model/load ----
  {
    ::agenticdsl::ToolMetadata meta{
      "inference/model/load",
      "加载 GGUF/Safetensors 模型到引擎",
      "inference",
      ::agenticdsl::ToolCategory::Execute,
      ::agenticdsl::LayerProfile::Workflow,
      ::agenticdsl::ApprovalPolicy{true, true, false, false}
    };

    registry.register_tool_function(
      "inference/model/load",
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

        if (!s.initialized) return {{"status", "error"}, {"error", "engine not initialized"}};

        std::string model_path = args.value("model_path", "/models/default.gguf");
        std::string quantization = args.value("quantization", "q4_0");
        std::string model_id = "model_" + std::to_string(s.loaded_models.size());

        s.loaded_models.push_back({model_id, model_path, quantization, true});
        return {
          {"status", "loaded"},
          {"model_id", model_id},
          {"model_path", model_path},
          {"quantization", quantization}
        };
      }
    );
  }

  // ---- inference/model/unload ----
  {
    ::agenticdsl::ToolMetadata meta{
      "inference/model/unload",
      "释放已加载的模型资源",
      "inference",
      ::agenticdsl::ToolCategory::Execute,
      ::agenticdsl::LayerProfile::Workflow,
      ::agenticdsl::ApprovalPolicy{true, false, false, false}  // agent only
    };

    registry.register_tool_function(
      "inference/model/unload",
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

        std::string model_id = args.value("model_id", "");
        auto it = std::find_if(s.loaded_models.begin(), s.loaded_models.end(),
          [&](const auto& m) { return m.model_id == model_id; });
        if (it != s.loaded_models.end()) s.loaded_models.erase(it);

        return {{"status", "unloaded"}, {"model_id", model_id}};
      }
    );
  }

  // ---- inference/model/list ----
  {
    ::agenticdsl::ToolMetadata meta{
      "inference/model/list",
      "列出当前引擎中已加载的模型",
      "inference",
      ::agenticdsl::ToolCategory::ReadOnly,
      ::agenticdsl::LayerProfile::Workflow,
      ::agenticdsl::ApprovalPolicy{false, false, true, false}
    };

    registry.register_tool_function(
      "inference/model/list",
      meta,
      [](const std::unordered_map<std::string, std::string>&) -> json {
        auto& s = engine_state();
        std::lock_guard<std::mutex> lock(s.mtx);
        json arr = json::array();
        for (const auto& m : s.loaded_models) {
          arr.push_back({{"model_id", m.model_id}, {"model_path", m.model_path}, {"active", m.active}});
        }
        return arr;
      }
    );
  }

  // ---- inference/model/switch ----
  {
    ::agenticdsl::ToolMetadata meta{
      "inference/model/switch",
      "切换活跃推理模型",
      "inference",
      ::agenticdsl::ToolCategory::Execute,
      ::agenticdsl::LayerProfile::Workflow,
      ::agenticdsl::ApprovalPolicy{true, true, false, false}
    };

    registry.register_tool_function(
      "inference/model/switch",
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

        std::string target = args.value("model_id", "");
        std::string previous = "none";
        for (auto& m : s.loaded_models) {
          if (m.active) { previous = m.model_id; m.active = false; }
        }
        auto it = std::find_if(s.loaded_models.begin(), s.loaded_models.end(),
          [&](auto& m) { return m.model_id == target; });
        if (it != s.loaded_models.end()) it->active = true;

        return {{"status", "switched"}, {"active_model", target}, {"previous_model", previous}};
      }
    );
  }

} // register_model_tools

} // namespace agenticdsl::pdk::llama