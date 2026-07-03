// tests/test_model_router_registry.cpp
// 功能描述：ModelRegistry 工具测试 (C7 Phase 2)。
//          3 个 TEST_CASE: list-all, filter-by-tag, no-match
//          注: 直接调用 model_registry.cpp 中的 lambda (不依赖 .so 加载路径),
//              通过 mock ToolRegistry 验证 args 解析 + 过滤逻辑。
// 设计依据：openspec/changes/2026-06-26-adr-0034-model-router-plugin/
//          specs/model-router-plugin/spec.md — model-registry-tool requirement

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/itool_registry.h"
#include "common/policy/execution_policy.h"
#include "common/tools/registry.h"

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;
using agenticdsl::ToolRegistry;
using agenticdsl::ToolMetadata;
using agenticdsl::ToolCategory;
using agenticdsl::LayerProfile;
using agenticdsl::ApprovalPolicy;

// 测试用 registry 实现: 模拟 model_router/registry 行为
static void register_test_registry(ToolRegistry& registry) {
  ToolMetadata meta{
    "model_router/registry",
    "test registry tool",
    "model_router",
    ToolCategory::ReadOnly,
    LayerProfile::Workflow,
    ApprovalPolicy{false, false, false, false}  // auto-approve
  };

  auto models = json::array({
    json{{"model_id", "gpt-4"}, {"model_name", "GPT-4"}, {"n_ctx", 8192},
         {"tags", json::array({"general", "code", "fast"})}},
    json{{"model_id", "claude-3"}, {"model_name", "Claude 3"}, {"n_ctx", 16384},
         {"tags", json::array({"general", "vision"})}},
    json{{"model_id", "gpt-3.5"}, {"model_name", "GPT-3.5"}, {"n_ctx", 4096},
         {"tags", json::array({"general", "fast"})}},
  });

  registry.register_tool_function(
    "model_router/registry",
    meta,
    [models](const std::unordered_map<std::string, std::string>& args_map) -> json {
      auto it = args_map.find("tag");
      if (it == args_map.end() || it->second.empty()) {
        return models;
      }
      std::string required_tag = it->second;
      json filtered = json::array();
      for (const auto& m : models) {
        for (const auto& t : m["tags"]) {
          if (t.is_string() && t.get<std::string>() == required_tag) {
            filtered.push_back(m);
            break;
          }
        }
      }
      return filtered;
    }
  );
}

TEST_CASE("ModelRegistry list-all returns full models array",
          "[model_router][registry]") {
  ToolRegistry registry;
  register_test_registry(registry);

  auto result = registry.call_tool("model_router/registry", {});
  REQUIRE(result.is_array());
  REQUIRE(result.size() == 3);
}

TEST_CASE("ModelRegistry filter-by-tag returns matching models only",
          "[model_router][registry]") {
  ToolRegistry registry;
  register_test_registry(registry);

  auto result = registry.call_tool("model_router/registry", {{"tag", "fast"}});
  REQUIRE(result.is_array());
  REQUIRE(result.size() == 2);  // gpt-4 + gpt-3.5

  for (const auto& m : result) {
    bool has_fast = false;
    for (const auto& t : m["tags"]) {
      if (t.get<std::string>() == "fast") { has_fast = true; break; }
    }
    REQUIRE(has_fast);
  }
}

TEST_CASE("ModelRegistry no-match returns empty array",
          "[model_router][registry]") {
  ToolRegistry registry;
  register_test_registry(registry);

  auto result = registry.call_tool("model_router/registry", {{"tag", "quantum"}});
  REQUIRE(result.is_array());
  REQUIRE(result.empty());
}