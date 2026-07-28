// tests/test_temporal_agent_metadata.cpp
// 功能描述：Temporal Agent Plugin 工具注册元数据测试
//          验证 5/5 工具注册 + schema 校验 + ToolMetadata 完整性
// 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md §5.1
// 作者：pkgm-temporal-agent change
// 最后修改日期：2026-07-27

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/itool_registry.h"
#include "agenticdsl/plugin/plugin_info.h"
#include "common/policy/execution_policy.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;

namespace {

class MockToolRegistry : public ::agenticdsl::IToolRegistry {
 public:
  std::vector<std::string> registered_tools;
  std::unordered_map<std::string, ::agenticdsl::ToolMetadata> tool_metas;
  std::unordered_map<std::string, ToolFunc> tool_funcs;

  void register_tool_function(
      std::string name,
      ::agenticdsl::ToolMetadata meta,
      ToolFunc fn) override {
    registered_tools.push_back(name);
    tool_metas[name] = meta;
    tool_funcs[name] = fn;
  }

  bool has_tool(const std::string& name) const override {
    return tool_funcs.find(name) != tool_funcs.end();
  }

  json call_tool(
      const std::string& name,
      const std::unordered_map<std::string, std::string>& args) override {
    auto it = tool_funcs.find(name);
    if (it == tool_funcs.end()) {
      return {{"error", "tool not found"}, {"tool", name}};
    }
    return it->second(args);
  }

  std::vector<std::string> list_tools() const override { return registered_tools; }
  void register_llm_tool(std::string, std::unique_ptr<::agenticdsl::ILLMTool>,
                         const ::agenticdsl::LLMParams&) override {}
  bool is_llm_tool(const std::string&) const override { return false; }
  const ::agenticdsl::LLMParams& get_llm_params(const std::string&) const override {
    static const ::agenticdsl::LLMParams kEmpty{};
    return kEmpty;
  }
  json call_llm_tool(const std::string&, const std::string&,
                     const ::agenticdsl::LLMParams&) override { return {}; }
  void set_cost_callback(::agenticdsl::IToolRegistry::CostCallback) override {}
};

const std::vector<std::string> EXPECTED_TOOLS = {
  "temporal/start_workflow",
  "temporal/start_async",
  "temporal/poll",
  "temporal/signal",
  "temporal/query",
};

}  // namespace

// extern "C" 声明 (来自 pdk_entry.cpp)
extern "C" const hydraforge::PluginInfo pdk_plugin_info;
extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry);

TEST_CASE("temporal_agent: pdk_plugin_info ABI version", "[temporal_agent][metadata]") {
  REQUIRE(pdk_plugin_info.abi_version == hydraforge::CURRENT_ABI_VERSION);
  REQUIRE(pdk_plugin_info.abi_version == 2);
  REQUIRE(std::string(pdk_plugin_info.name) == "infra.temporal");
  REQUIRE(pdk_plugin_info.major_version == 0);
  REQUIRE(pdk_plugin_info.minor_version == 1);
  REQUIRE(pdk_plugin_info.patch_version == 0);
  REQUIRE(std::string(pdk_plugin_info.capabilities).find("temporal") != std::string::npos);
}

TEST_CASE("temporal_agent: registers all 5 tools", "[temporal_agent][metadata]") {
  MockToolRegistry registry;
  pdk_register_tools(registry);

  REQUIRE(registry.registered_tools.size() == 5);

  for (const auto& expected : EXPECTED_TOOLS) {
    INFO("checking tool: " << expected);
    REQUIRE(std::find(registry.registered_tools.begin(),
                      registry.registered_tools.end(),
                      expected) != registry.registered_tools.end());
    REQUIRE(registry.has_tool(expected));
  }
}

TEST_CASE("temporal_agent: ToolMetadata completeness", "[temporal_agent][metadata]") {
  MockToolRegistry registry;
  pdk_register_tools(registry);

  for (const auto& tool_name : EXPECTED_TOOLS) {
    INFO("checking metadata for: " << tool_name);
    auto it = registry.tool_metas.find(tool_name);
    REQUIRE(it != registry.tool_metas.end());

    const auto& meta = it->second;
    REQUIRE(meta.name == tool_name);
    REQUIRE(!meta.description.empty());
    REQUIRE(meta.domain == "temporal");
    REQUIRE(meta.min_layer == ::agenticdsl::LayerProfile::Workflow);
    REQUIRE(!meta.allowed_layers.empty());
    REQUIRE(meta.timeout_ms > 0);
  }
}

TEST_CASE("temporal_agent: category and approval schema", "[temporal_agent][metadata]") {
  MockToolRegistry registry;
  pdk_register_tools(registry);

  SECTION("start_workflow is Execute + requires approval") {
    const auto& meta = registry.tool_metas.at("temporal/start_workflow");
    REQUIRE(meta.category == ::agenticdsl::ToolCategory::Execute);
    REQUIRE(meta.approval.requires_approval_in_plan == true);
    REQUIRE(meta.approval.requires_approval_in_agent == true);
    REQUIRE(meta.approval.requires_approval_in_yolo == false);
  }

  SECTION("start_async is Execute + requires approval") {
    const auto& meta = registry.tool_metas.at("temporal/start_async");
    REQUIRE(meta.category == ::agenticdsl::ToolCategory::Execute);
    REQUIRE(meta.approval.requires_approval_in_plan == true);
  }

  SECTION("poll is ReadOnly + no approval") {
    const auto& meta = registry.tool_metas.at("temporal/poll");
    REQUIRE(meta.category == ::agenticdsl::ToolCategory::ReadOnly);
    REQUIRE(meta.approval.requires_approval_in_plan == false);
    REQUIRE(meta.approval.requires_approval_in_agent == false);
  }

  SECTION("signal is Execute + requires approval") {
    const auto& meta = registry.tool_metas.at("temporal/signal");
    REQUIRE(meta.category == ::agenticdsl::ToolCategory::Execute);
    REQUIRE(meta.approval.requires_approval_in_plan == true);
  }

  SECTION("query is ReadOnly + no approval") {
    const auto& meta = registry.tool_metas.at("temporal/query");
    REQUIRE(meta.category == ::agenticdsl::ToolCategory::ReadOnly);
    REQUIRE(meta.approval.requires_approval_in_plan == false);
  }
}

TEST_CASE("temporal_agent: allowed_layers all Workflow", "[temporal_agent][metadata]") {
  MockToolRegistry registry;
  pdk_register_tools(registry);

  for (const auto& tool_name : EXPECTED_TOOLS) {
    INFO("checking allowed_layers for: " << tool_name);
    const auto& meta = registry.tool_metas.at(tool_name);
    REQUIRE(meta.allowed_layers.size() == 1);
    REQUIRE(meta.allowed_layers[0] == ::agenticdsl::LayerProfile::Workflow);
  }
}
