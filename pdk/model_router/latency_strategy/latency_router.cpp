// pdk/model_router/latency_strategy/latency_router.cpp
// 功能描述：LatencyModelRouter Plugin 入口 (C7 Phase 2)。
//          export extern "C" pdk_register_tools(IToolRegistry&),
//          注册 model_router/latency 工具: lambda 解析 args → LatencyModelRouterPolicy::route()。
//          遵循 PDK Plugin 契约 (Sprint 4, DECLARE_TOOL 模式)。
// 设计依据：openspec/changes/2026-06-26-adr-0034-model-router-plugin/
//          design.md Decision 2, specs/model-router-plugin/spec.md model-router-plugin-entry
// 作者：C7 Phase 2
// 最后修改日期：2026-07-02

#include "latency_router.h"

#include "agenticdsl/contract/itool_registry.h"
#include "common/policy/execution_policy.h"

#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

using json = nlohmann::json;

namespace {

// 从 json args 解析 RoutingContext (latency 复用 budget_remaining 字段作为 max_latency 毫秒)
agenticdsl::pdk::RoutingContext parse_routing_context(const json& args) {
  agenticdsl::pdk::RoutingContext ctx;
  ctx.task_type = args.value("task_type", "completion");
  ctx.session_id = args.value("session_id", "");

  if (args.contains("max_tokens") && args["max_tokens"].is_number_integer()) {
    ctx.max_tokens = args["max_tokens"].get<int>();
  }
  // max_latency: 优先读取显式 max_latency 字段, 否则尝试 budget_remaining
  if (args.contains("max_latency") && args["max_latency"].is_number_integer()) {
    ctx.budget_remaining = static_cast<double>(args["max_latency"].get<int>());
  } else if (args.contains("budget_remaining")
             && args["budget_remaining"].is_number_integer()) {
    ctx.budget_remaining = args["budget_remaining"].get<double>();
  }
  if (args.contains("required_tags") && args["required_tags"].is_array()) {
    for (const auto& tag : args["required_tags"]) {
      ctx.required_tags.push_back(tag.get<std::string>());
    }
  }
  ctx.preferred_model = args.value("preferred_model", "");
  ctx.is_fleet_mode = args.value("is_fleet_mode", false);

  return ctx;
}

// 从 json args 解析候选模型列表 (与 cost/quality 一致)
std::vector<agenticdsl::pdk::ModelCapability> parse_candidates(const json& args) {
  std::vector<agenticdsl::pdk::ModelCapability> caps;
  if (!args.contains("candidates") || !args["candidates"].is_array()) {
    return caps;
  }
  for (const auto& c : args["candidates"]) {
    agenticdsl::pdk::ModelCapability cap;
    cap.model_id = c.value("model_id", "");
    cap.model_name = c.value("model_name", "");
    cap.n_ctx = c.value("n_ctx", 4096);
    cap.max_tokens = c.value("max_tokens", 4096);
    cap.supports_streaming = c.value("supports_streaming", true);
    cap.supports_function_call = c.value("supports_function_call", false);
    cap.per_token_cost = c.value("per_token_cost", 0.0);
    cap.avg_latency_ms = c.value("avg_latency_ms", 500);
    if (c.contains("tags") && c["tags"].is_array()) {
      for (const auto& tag : c["tags"]) {
        cap.tags.push_back(tag.get<std::string>());
      }
    }
    caps.push_back(std::move(cap));
  }
  return caps;
}

} // namespace

// Plugin 入口: 注册 model_router/latency 工具
extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry) {
  auto router = std::make_shared<agenticdsl::pdk::LatencyModelRouterPolicy>();

  // 构造 ToolMetadata V2
  ::agenticdsl::ToolMetadata meta{
    "model_router/latency",                      // name
    "延迟优先模型路由: 返回 avg_latency_ms 最低的 tag-matching 模型",  // description
    "model_router",                              // domain
    ::agenticdsl::ToolCategory::ReadOnly,        // category
    ::agenticdsl::LayerProfile::Workflow,        // min_layer
    ::agenticdsl::ApprovalPolicy{false, false, true, false}  // approval: yolo only
  };

  registry.register_tool_function(
    "model_router/latency",
    meta,
    [router](const std::unordered_map<std::string, std::string>& args_map)
        -> nlohmann::json {
      // 将 map<string,string> 转换为 json
      json args;
      for (const auto& [k, v] : args_map) {
        if (!v.empty() && (v.front() == '[' || v.front() == '{')) {
          try {
            args[k] = json::parse(v);
          } catch (...) {
            args[k] = v;
          }
        } else {
          args[k] = v;
        }
      }

      auto ctx = parse_routing_context(args);
      auto candidates = parse_candidates(args);

      try {
        auto model_id = router->route(ctx, candidates);
        return {{"model_id", model_id}, {"router", router->name()}};
      } catch (const agenticdsl::pdk::ModelRoutingError& e) {
        return {{"error", e.what()}, {"code", static_cast<int>(e.code)}};
      }
    }
  );
}