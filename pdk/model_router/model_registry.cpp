// pdk/model_router/model_registry.cpp
// 功能描述：ModelRegistry — 模型注册表查询工具 (C7 Phase 2)。
//          注册 model_router/registry 工具, 支持按 tag 过滤可用模型列表。
//          返回 JSON array, 每元素含 model_id / model_name / n_ctx / tags。
//          注: 使用 pdk_register_tools + ToolMetadata 模式 (与 cost/quality/latency 一致),
//              而非 DECLARE_TOOL 宏 (宏的 ##name 拼接不支持含 "/" 的字符串标识)。
// 设计依据：openspec/changes/2026-06-26-adr-0034-model-router-plugin/
//          design.md Decision 4, specs/model-router-plugin/spec.md
// 作者：C7 Phase 2
// 最后修改日期：2026-07-02

#include "agenticdsl/contract/itool_registry.h"
#include "agenticdsl/pdk/pdk.h"
#include "common/policy/execution_policy.h"

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

using nlohmann::json;

namespace {

// 默认可用模型列表 (与 mock_provider.cpp default 一致, 但使用 ModelCapability 字段格式)
std::vector<json> get_default_models() {
  return {
    {{"model_id", "gpt-4"}, {"model_name", "GPT-4"}, {"n_ctx", 8192},
     {"tags", json::array({"general", "reasoning", "code"})}},
    {{"model_id", "gpt-3.5-turbo"}, {"model_name", "GPT-3.5 Turbo"}, {"n_ctx", 4096},
     {"tags", json::array({"general", "fast"})}},
    {{"model_id", "claude-3-opus"}, {"model_name", "Claude 3 Opus"}, {"n_ctx", 16384},
     {"tags", json::array({"general", "reasoning", "code", "vision"})}},
  };
}

} // namespace

// Plugin 入口: 注册 model_router/registry 工具
extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry) {
  ::agenticdsl::ToolMetadata meta{
    "model_router/registry",                          // name
    "查询可用模型列表, 支持按 tag 过滤",              // description
    "model_router",                                   // domain
    ::agenticdsl::ToolCategory::ReadOnly,             // category (只读)
    ::agenticdsl::LayerProfile::Workflow,             // min_layer
    ::agenticdsl::ApprovalPolicy{true, true, false, false}  // approval: agent (plan + agent 需审批)
  };

  registry.register_tool_function(
    "model_router/registry",
    meta,
    [](const std::unordered_map<std::string, std::string>& args_map)
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

      std::vector<json> models = get_default_models();

      // 若 args 含 "tag" 字符串参数, 过滤
      if (args.contains("tag") && args["tag"].is_string()) {
        std::string required_tag = args["tag"].get<std::string>();
        std::vector<json> filtered;
        for (const auto& m : models) {
          if (m.contains("tags") && m["tags"].is_array()) {
            for (const auto& t : m["tags"]) {
              if (t.is_string() && t.get<std::string>() == required_tag) {
                filtered.push_back(m);
                break;
              }
            }
          }
        }
        return filtered;
      }

      return models;
    }
  );
}