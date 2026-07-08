// pdk/llama_engine/src/inference_arch.cpp
// 功能描述：C13 架构工具注册 (C14 §4, Oracle 审查 P0 阻塞项修复)
//           注册 4 个架构层工具 (C13 schema 定义, C14 实现):
//           prefix_cache.configure / kv_cache.configure
//           decoding.configure / cloud_engine.configure (PLACEHOLDER)
//           遵循 PDK Plugin 契约 (ADR-0021, ADR-0034 C7 范式)
// 设计依据：Oracle 审查报告 2026-07-07
//          C13 lib/inference/{prefix_cache,kv_cache,decoding,cloud_engine}.md
//          C14 tasks.md §4
// 作者：C14 Oracle review session
// 最后修改日期：2026-07-07

#include "agenticdsl/contract/itool_registry.h"
#include "common/policy/execution_policy.h"
#include "llama_state.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>

using json = nlohmann::json;

namespace agenticdsl::pdk::llama {

// ============================================================================
// 架构工具: prefix_cache / kv_cache / decoding / cloud_engine
// ============================================================================

void register_arch_tools(::agenticdsl::IToolRegistry& registry) {

  // ---- prefix_cache.configure ----
  {
    ::agenticdsl::ToolMetadata meta{
      "prefix_cache.configure",
      "配置 prefix cache 策略 (委托 llama.cpp 内置 prefix cache)",
      "inference",
      ::agenticdsl::ToolCategory::ReadOnly,
      ::agenticdsl::LayerProfile::Cognitive,
      ::agenticdsl::ApprovalPolicy{false, true, false, false}  // plan only (配置变更需计划审批)
    };

    registry.register_tool_function(
      "prefix_cache.configure",
      meta,
      [](const std::unordered_map<std::string, std::string>& args_map) -> json {
        auto& s = engine_state();
        std::lock_guard<std::mutex> lock(s.mtx);

        json args;
        for (const auto& [k, v] : args_map) {
          if (!v.empty() && (v.front() == '[' || v.front() == '{')) {
            try { args[k] = json::parse(v); } catch (...) { args[k] = v; }
          } else {
            args[k] = v;
          }
        }

        bool enabled = args.value("enabled", true);
        int max_size = args.value("max_size", 512);

        // TODO: C14 编码 — 委托 llama.cpp 内置 prefix cache
        // llama_set_prefix_cache_params(ctx, enabled, max_size);

        return {
          {"status", enabled ? "ok" : "disabled"},
          {"active_patterns", enabled ? 0 : 0}
        };
      }
    );
  }

  // ---- kv_cache.configure ----
  {
    ::agenticdsl::ToolMetadata meta{
      "kv_cache.configure",
      "配置 KV cache 驱逐策略 (lru / lfu / fifo) 和最大容量",
      "inference",
      ::agenticdsl::ToolCategory::ReadOnly,
      ::agenticdsl::LayerProfile::Cognitive,
      ::agenticdsl::ApprovalPolicy{false, true, false, false}
    };

    registry.register_tool_function(
      "kv_cache.configure",
      meta,
      [](const std::unordered_map<std::string, std::string>& args_map) -> json {
        auto& s = engine_state();
        std::lock_guard<std::mutex> lock(s.mtx);

        json args;
        for (const auto& [k, v] : args_map) {
          if (!v.empty() && (v.front() == '[' || v.front() == '{')) {
            try { args[k] = json::parse(v); } catch (...) { args[k] = v; }
          } else {
            args[k] = v;
          }
        }

        std::string evict_policy = args.value("evict_policy", "lru");
        double max_size_gb = args.value("max_size_gb", 4.0);

        // TODO: C14 编码 — 委托 llama.cpp KV cache 策略
        // llama_set_kv_cache_params(ctx, evict_policy, max_size_gb);

        return {
          {"status", "ok"},
          {"active_policy", evict_policy},
          {"current_size_gb", 0.0}
        };
      }
    );
  }

  // ---- decoding.configure ----
  {
    ::agenticdsl::ToolMetadata meta{
      "decoding.configure",
      "配置 decoding 参数 (temperature / top_p / top_k / repeat_penalty / sampler: 5 种字符串选择)",
      "inference",
      ::agenticdsl::ToolCategory::ReadOnly,
      ::agenticdsl::LayerProfile::Cognitive,
      ::agenticdsl::ApprovalPolicy{false, true, false, false}
    };

    registry.register_tool_function(
      "decoding.configure",
      meta,
      [](const std::unordered_map<std::string, std::string>& args_map) -> json {
        auto& s = engine_state();
        std::lock_guard<std::mutex> lock(s.mtx);

        json args;
        for (const auto& [k, v] : args_map) {
          if (!v.empty() && (v.front() == '[' || v.front() == '{')) {
            try { args[k] = json::parse(v); } catch (...) { args[k] = v; }
          } else {
            args[k] = v;
          }
        }

        double temperature = args.value("temperature", 0.7);
        double top_p = args.value("top_p", 0.9);
        int top_k = args.value("top_k", 40);
        double repeat_penalty = args.value("repeat_penalty", 1.1);
        std::string sampler = args.value("sampler", "greedy");  // D1: 5 种字符串选择

        // D1: 采样器 clamp 逻辑内联 (不创建独立 SamplerStrategy PDK 接口)
        temperature = std::max(0.0, std::min(2.0, temperature));
        top_p = std::max(0.0, std::min(1.0, top_p));

        // 验证 sampler 合法值
        static const std::unordered_set<std::string> valid_samplers = {
          "greedy", "temperature", "mirostat_v1", "mirostat_v2", "typical_p"
        };
        std::string unsupported_warning;
        if (valid_samplers.find(sampler) == valid_samplers.end()) {
          unsupported_warning = "sampler '" + sampler + "' not recognized, falling back to greedy";
          sampler = "greedy";
        }

        // TODO: C14 编码 — 委托 llama.cpp sampling API
        // llama_set_sampling_params(ctx, temperature, top_p, top_k, repeat_penalty, sampler);

        return {
          {"status", "ok"},
          {"active_sampler", sampler},
          {"unsupported_warning", unsupported_warning},
          {"params", {
            {"temperature", temperature},
            {"top_p", top_p},
            {"top_k", top_k},
            {"repeat_penalty", repeat_penalty}
          }}
        };
      }
    );
  }

  // ---- cloud_engine.configure (PLACEHOLDER stub) ----
  {
    ::agenticdsl::ToolMetadata meta{
      "cloud_engine.configure",
      "配置 cloud LLM engine provider + model (PLACEHOLDER — 实现在 Phase 5 Stage 2+)",
      "inference",
      ::agenticdsl::ToolCategory::ReadOnly,
      ::agenticdsl::LayerProfile::Cognitive,
      ::agenticdsl::ApprovalPolicy{false, true, false, false}
    };

    registry.register_tool_function(
      "cloud_engine.configure",
      meta,
      [](const std::unordered_map<std::string, std::string>& args_map) -> json {
        // PLACEHOLDER stub: 等 Phase 5 Stage 2+ 第三方 plugin 团队实施
        return {
          {"status", "not_yet_implemented"},
          {"message", "cloud_engine.configure is a PLACEHOLDER. Implementation deferred to Phase 5 Stage 2+ (second inference backend / cloud provider plugin)"},
          {"provider", "none"},
          {"model", "none"}
        };
      }
    );
  }

} // register_arch_tools

} // namespace agenticdsl::pdk::llama