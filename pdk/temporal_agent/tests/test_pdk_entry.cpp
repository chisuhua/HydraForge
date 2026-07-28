// pdk/temporal_agent/tests/test_pdk_entry.cpp
// 功能描述：PDK 5 工具注册单元测试 (Task 3, TDD Step 1)。
//          覆盖:
//            1. 5 工具注册 (temporal/start_workflow, start_async, poll, signal, query)
//            2. ToolMetadata V2 字段 (category=Execute, allowed_layers, approval)
//            3. 工具调用验证 (通过 set_client 注入 MockTemporalClient)
//            4. extern "C" pdk_register_tools 入口点
// 设计依据：.rddf/plans/pkm-temporal-demo-scaffold.md Task 3 Step 1
//           参考范式: tests/test_llama_engine_plugin.cpp (MockToolRegistry)
// 作者：pkm-temporal-demo-scaffold Task 3
// 最后修改日期：2026-07-28

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/itool_registry.h"
#include "common/policy/execution_policy.h"
#include "mock_client.h"

#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;
using ::agenticdsl::IToolRegistry;
using ::agenticdsl::ToolMetadata;
using ::agenticdsl::ToolCategory;
using ::agenticdsl::LayerProfile;
using ::agenticdsl::ApprovalPolicy;

// 声明 pdk_entry.cpp 提供的注册函数 + set_client
namespace agenticdsl::pdk::temporal_agent {
void register_tools(IToolRegistry* registry);
void set_client(std::unique_ptr<::agenticdsl::pdk::ITemporalClient> c);
}  // namespace agenticdsl::pdk::temporal_agent

// extern "C" 声明 (pdk_entry.cpp 导出)
extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry);

// ============================================================================
// MockToolRegistry - 捕获注册的工具 + metadata + handler (不注册默认工具)
// ============================================================================
class MockToolRegistry : public IToolRegistry {
public:
  std::vector<std::string> registered_names;
  std::unordered_map<std::string, ToolMetadata> metas;
  std::unordered_map<std::string, ToolFunc> funcs;

  void register_tool_function(
      std::string name, ToolMetadata meta, ToolFunc fn) override {
    registered_names.push_back(name);
    metas[name] = std::move(meta);
    funcs[name] = std::move(fn);
  }

  bool has_tool(const std::string& name) const override {
    return funcs.count(name) > 0;
  }

  json call_tool(
      const std::string& name,
      const std::unordered_map<std::string, std::string>& args) override {
    auto it = funcs.find(name);
    if (it == funcs.end()) return {{"error", "not found"}};
    return it->second(args);
  }

  std::vector<std::string> list_tools() const override { return registered_names; }
  void register_llm_tool(std::string, std::unique_ptr<::agenticdsl::ILLMTool>,
                         const ::agenticdsl::LLMParams&) override {}
  bool is_llm_tool(const std::string&) const override { return false; }
  const ::agenticdsl::LLMParams& get_llm_params(const std::string&) const override {
    static const ::agenticdsl::LLMParams kEmpty{};
    return kEmpty;
  }
  json call_llm_tool(const std::string&, const std::string&,
                     const ::agenticdsl::LLMParams&) override { return {}; }
  void set_cost_callback(CostCallback) override {}
};

// ============================================================================
// Test 1: 注册 5 个 temporal/* 工具
// ============================================================================
TEST_CASE("PDK entry: registers 5 temporal tools",
          "[pdk][temporal][task3][register]") {
  MockToolRegistry registry;
  agenticdsl::pdk::temporal_agent::register_tools(&registry);

  REQUIRE(registry.has_tool("temporal/start_workflow"));
  REQUIRE(registry.has_tool("temporal/start_async"));
  REQUIRE(registry.has_tool("temporal/poll"));
  REQUIRE(registry.has_tool("temporal/signal"));
  REQUIRE(registry.has_tool("temporal/query"));
  REQUIRE(registry.registered_names.size() == 5);
}

// ============================================================================
// Test 2: ToolMetadata V2 - category + min_layer + approval
// ============================================================================
TEST_CASE("PDK entry: ToolMetadata V2 fields correct",
          "[pdk][temporal][task3][metadata]") {
  MockToolRegistry registry;
  agenticdsl::pdk::temporal_agent::register_tools(&registry);

  SECTION("start_workflow: Execute category, agent approval") {
    const auto& m = registry.metas.at("temporal/start_workflow");
    REQUIRE(m.category == ToolCategory::Execute);
    REQUIRE(m.min_layer == LayerProfile::Workflow);
    REQUIRE(m.approval.requires_approval_in_plan == true);
    REQUIRE(m.approval.requires_approval_in_agent == true);
    REQUIRE(m.approval.requires_approval_in_yolo == false);
  }

  SECTION("start_async: Execute category, agent approval") {
    const auto& m = registry.metas.at("temporal/start_async");
    REQUIRE(m.category == ToolCategory::Execute);
    REQUIRE(m.approval.requires_approval_in_yolo == false);
  }

  SECTION("poll: ReadOnly category, yolo approval") {
    const auto& m = registry.metas.at("temporal/poll");
    REQUIRE(m.category == ToolCategory::ReadOnly);
    REQUIRE(m.approval.requires_approval_in_yolo == true);
    REQUIRE(m.approval.requires_approval_in_plan == false);
    REQUIRE(m.approval.requires_approval_in_agent == false);
  }

  SECTION("signal: Execute category, agent approval") {
    const auto& m = registry.metas.at("temporal/signal");
    REQUIRE(m.category == ToolCategory::Execute);
    REQUIRE(m.approval.requires_approval_in_yolo == false);
  }

  SECTION("query: ReadOnly category, yolo approval") {
    const auto& m = registry.metas.at("temporal/query");
    REQUIRE(m.category == ToolCategory::ReadOnly);
    REQUIRE(m.approval.requires_approval_in_yolo == true);
  }

  SECTION("all 5 tools have allowed_layers set") {
    for (const auto& name : registry.registered_names) {
      const auto& m = registry.metas.at(name);
      REQUIRE_FALSE(m.allowed_layers.empty());
      REQUIRE(m.allowed_layers[0] == LayerProfile::Workflow);
    }
  }
}

// ============================================================================
// Test 3: 工具调用通过 MockTemporalClient (集成验证)
// ============================================================================
TEST_CASE("PDK entry: tool invocation routes to ITemporalClient",
          "[pdk][temporal][task3][invoke]") {
  // 注入 MockTemporalClient
  auto mock = std::make_unique<agenticdsl::pdk::MockTemporalClient>();
  agenticdsl::pdk::MockTemporalClient* mock_ptr = mock.get();
  agenticdsl::pdk::temporal_agent::set_client(std::move(mock));

  MockToolRegistry registry;
  agenticdsl::pdk::temporal_agent::register_tools(&registry);

  SECTION("start_async -> RUNNING") {
    std::unordered_map<std::string, std::string> args = {
      {"workflow_id", "wf-invoke-1"},
      {"args", "{\"task\":\"noop\"}"}
    };
    auto r = registry.call_tool("temporal/start_async", args);
    REQUIRE(r["state"] == "RUNNING");
    REQUIRE(r["workflow_id"] == "wf-invoke-1");
  }

  SECTION("poll -> COMPLETED (after advance_time)") {
    std::unordered_map<std::string, std::string> start_args = {
      {"workflow_id", "wf-invoke-2"}
    };
    registry.call_tool("temporal/start_async", start_args);
    mock_ptr->advance_time(std::chrono::milliseconds(200));

    std::unordered_map<std::string, std::string> poll_args = {
      {"workflow_id", "wf-invoke-2"}
    };
    auto r = registry.call_tool("temporal/poll", poll_args);
    REQUIRE(r["state"] == "COMPLETED");
  }

  SECTION("signal -> ack") {
    std::unordered_map<std::string, std::string> start_args = {
      {"workflow_id", "wf-sig"}
    };
    registry.call_tool("temporal/start_async", start_args);

    std::unordered_map<std::string, std::string> sig_args = {
      {"workflow_id", "wf-sig"},
      {"signal_name", "go"},
      {"payload", "{}"}
    };
    auto r = registry.call_tool("temporal/signal", sig_args);
    REQUIRE(r["ack"] == true);
    REQUIRE(r["signal_name"] == "go");
  }

  SECTION("query -> status") {
    std::unordered_map<std::string, std::string> start_args = {
      {"workflow_id", "wf-q"}
    };
    registry.call_tool("temporal/start_async", start_args);

    std::unordered_map<std::string, std::string> q_args = {
      {"workflow_id", "wf-q"},
      {"query_name", "status"}
    };
    auto r = registry.call_tool("temporal/query", q_args);
    REQUIRE(r["workflow_id"] == "wf-q");
    REQUIRE(r["state"] == "RUNNING");
  }
}

// ============================================================================
// Test 4: extern "C" pdk_register_tools 入口点
// ============================================================================
TEST_CASE("PDK entry: extern C pdk_register_tools works",
          "[pdk][temporal][task3][extern_c]") {
  MockToolRegistry registry;
  pdk_register_tools(registry);

  REQUIRE(registry.has_tool("temporal/start_workflow"));
  REQUIRE(registry.has_tool("temporal/poll"));
  REQUIRE(registry.registered_names.size() == 5);
}

// ============================================================================
// Test 5: 幂等性通过工具链路验证
// ============================================================================
TEST_CASE("PDK entry: idempotency via tool invocation",
          "[pdk][temporal][task3][idempotent]") {
  auto mock = std::make_unique<agenticdsl::pdk::MockTemporalClient>();
  agenticdsl::pdk::temporal_agent::set_client(std::move(mock));

  MockToolRegistry registry;
  agenticdsl::pdk::temporal_agent::register_tools(&registry);

  std::unordered_map<std::string, std::string> args = {
    {"workflow_id", "dup-id"}
  };

  auto first = registry.call_tool("temporal/start_async", args);
  REQUIRE(first["state"] == "RUNNING");

  auto second = registry.call_tool("temporal/start_async", args);
  REQUIRE(second["idempotent_replay"] == true);
  REQUIRE(second["original_workflow_id"] == "dup-id");
}
