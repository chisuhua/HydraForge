// tests/test_execution_policy.cpp
// 功能描述：IExecutionPolicy 5-method 接口三模式策略单元测试 (C3 ship)
// 设计依据：ADR-0031 §决策 1 (Oracle 5-method 接口, 2026-06-26)
// 作者：AgenticDSL Phase3 / Sprint 13 C3 ship
// 最后修改日期：2026-07-31
#include "catch_amalgamated.hpp"
#include "agenticdsl/policy/iexecution_policy.h"
#include "common/policy/plan_mode_policy.h"
#include "common/policy/agent_mode_policy.h"
#include "common/policy/yolo_mode_policy.h"
#include "common/policy/policy_factory.h"
#include "common/policy/approval_handler.h"
#include "common/policy/approval_callbacks.h"
#include "common/policy/mode_switch_dialog.h"

using namespace agenticdsl;

namespace {

ToolMetadata make_tool_meta(ToolCategory category,
 ApprovalPolicy approval = ApprovalPolicy{},
 const std::string& name = "test::tool",
 const std::string& domain = "test") {
 ToolMetadata meta;
 meta.name = name;
 meta.description = "test tool";
 meta.domain = domain;
 meta.category = category;
 meta.min_layer = LayerProfile::Workflow;
 meta.approval = approval;
 return meta;
}

ToolCallContext make_ctx(const std::string& session_id = "sess-1",
 const std::string& caller_layer = "workflow") {
 ToolCallContext ctx;
 ctx.session_id = session_id;
 ctx.caller_layer = caller_layer;
 ctx.target_path = "/tmp/test";
 ctx.is_in_fleet_mode = false;
 ctx.call_count_this_session = 0;
 return ctx;
}

ToolPreview make_preview(const std::string& cmd = "echo test") {
 ToolPreview preview;
 preview.command_line = cmd;
 preview.diff_text = "";
 preview.affected_paths = {"/tmp/test"};
 preview.estimated_duration = std::chrono::seconds(0);
 preview.risk_summary = "test";
 return preview;
}

} // namespace

// ============ §5.1.1 PlanModePolicy 审批判断 ============

TEST_CASE("plan_policy_requires_approval_for_writes", "[policy][plan]") {
 PlanModePolicy policy;

 ToolMetadata read_meta = make_tool_meta(ToolCategory::ReadOnly);
 ToolMetadata write_meta = make_tool_meta(ToolCategory::WriteFile);
 ToolMetadata exec_meta = make_tool_meta(ToolCategory::Execute);

 ToolCallContext ctx = make_ctx();

 SECTION("ReadOnly returns false") {
  REQUIRE(policy.requires_approval(read_meta, ctx) == false);
 }

 SECTION("WriteFile returns true") {
  REQUIRE(policy.requires_approval(write_meta, ctx) == true);
 }

 SECTION("Execute returns true") {
  REQUIRE(policy.requires_approval(exec_meta, ctx) == true);
 }

 SECTION("force_approval_always overrides ReadOnly") {
  ApprovalPolicy approval;
  approval.force_approval_always = true;
  ToolMetadata forced = make_tool_meta(ToolCategory::ReadOnly, approval);
  REQUIRE(policy.requires_approval(forced, ctx) == true);
 }
}

// ============ §5.1.2 AgentModePolicy 默认审批 ============

TEST_CASE("agent_policy_default", "[policy][agent]") {
 AgentModePolicy policy;
 ToolCallContext ctx = make_ctx();

 SECTION("ReadOnly no force_approval returns false") {
  ApprovalPolicy approval;
  approval.requires_approval_in_agent = false;
  ToolMetadata meta = make_tool_meta(ToolCategory::ReadOnly, approval);
  REQUIRE(policy.requires_approval(meta, ctx) == false);
 }

 SECTION("WriteFile with requires_approval_in_agent returns true") {
  ApprovalPolicy approval;
  approval.requires_approval_in_plan = false;
  approval.requires_approval_in_agent = true;
  approval.requires_approval_in_yolo = false;
  ToolMetadata meta = make_tool_meta(ToolCategory::WriteFile, approval);
  REQUIRE(policy.requires_approval(meta, ctx) == true);
 }

 SECTION("force_approval_always returns true regardless") {
  ApprovalPolicy approval;
  approval.requires_approval_in_agent = false;
  approval.force_approval_always = true;
  ToolMetadata meta = make_tool_meta(ToolCategory::ReadOnly, approval);
  REQUIRE(policy.requires_approval(meta, ctx) == true);
 }
}

// ============ §5.1.3 YoloModePolicy 最低审批 ============

TEST_CASE("yolo_policy_minimal_approval", "[policy][yolo]") {
 YoloModePolicy policy;
 ToolCallContext ctx = make_ctx();

 SECTION("force_approval_always returns true") {
  ApprovalPolicy approval;
  approval.force_approval_always = true;
  ToolMetadata meta = make_tool_meta(ToolCategory::WriteFile, approval);
  REQUIRE(policy.requires_approval(meta, ctx) == true);
 }

 SECTION("WriteFile no force_approval returns false") {
  ToolMetadata meta = make_tool_meta(ToolCategory::WriteFile);
  REQUIRE(policy.requires_approval(meta, ctx) == false);
 }

 SECTION("Execute no force_approval returns false") {
  ToolMetadata meta = make_tool_meta(ToolCategory::Execute);
  REQUIRE(policy.requires_approval(meta, ctx) == false);
 }
}

// ============ §5.1.4 should_execute 策略区分 ============

TEST_CASE("should_execute_distinguishes_plan", "[policy][exec]") {
 ToolMetadata meta = make_tool_meta(ToolCategory::WriteFile);
 ToolCallContext ctx = make_ctx();

 PlanModePolicy plan;
 AgentModePolicy agent;
 YoloModePolicy yolo;

 REQUIRE(plan.should_execute(meta, ctx) == false);
 REQUIRE(agent.should_execute(meta, ctx) == true);
 REQUIRE(yolo.should_execute(meta, ctx) == true);
}

// ============ §5.1.5 get_layer 全返回 Workflow ============

TEST_CASE("get_layer_dispatch", "[policy][layer]") {
 ToolMetadata meta = make_tool_meta(ToolCategory::WriteFile);

 PlanModePolicy plan;
 AgentModePolicy agent;
 YoloModePolicy yolo;

 REQUIRE(plan.get_layer(meta) == LayerProfile::Workflow);
 REQUIRE(agent.get_layer(meta) == LayerProfile::Workflow);
 REQUIRE(yolo.get_layer(meta) == LayerProfile::Workflow);
}

// ============ §5.1.6 PolicyFactory 默认 ============

TEST_CASE("policy_factory_default_is_agent", "[policy][factory]") {
 auto policy = PolicyFactory::create();
 REQUIRE(dynamic_cast<AgentModePolicy*>(policy.get()) != nullptr);

 auto default_policy = PolicyFactory::create_default();
 REQUIRE(dynamic_cast<AgentModePolicy*>(default_policy.get()) != nullptr);
}

// ============ §7.1.1 ApprovalHandler 回调 ============

TEST_CASE("approval_handler_auto_callback", "[policy][approval]") {
 ToolMetadata meta = make_tool_meta(ToolCategory::WriteFile);
 ToolCallContext ctx = make_ctx();
 ToolPreview preview = make_preview();

 SECTION("auto_approve returns true") {
  auto policy = std::make_shared<AgentModePolicy>();
  auto callback = make_test_auto_callback(true);
  ApprovalHandler handler(policy, callback);

  REQUIRE(handler.process_request(meta, ctx, preview) == true);
 }

 SECTION("auto_deny returns false") {
  auto policy = std::make_shared<AgentModePolicy>();
  auto callback = make_test_auto_callback(false);
  ApprovalHandler handler(policy, callback);

  REQUIRE(handler.process_request(meta, ctx, preview) == false);
 }
}

// ============ §7.1.3 ApprovalHandler request_id ============

TEST_CASE("approval_handler_propagates_request_id", "[policy][approval]") {
 auto policy = std::make_shared<AgentModePolicy>();
 std::string captured_id;
 auto capturing_callback = [&captured_id](const ApprovalRequest& req, int) -> bool {
  captured_id = req.request_id;
  return true;
 };
 ApprovalHandler handler(policy, capturing_callback);

 ToolMetadata meta = make_tool_meta(ToolCategory::WriteFile);
 ToolCallContext ctx = make_ctx();
 ToolPreview preview = make_preview();

 handler.process_request(meta, ctx, preview);

 REQUIRE(captured_id.empty() == false);
}

// ============ §7.2.1 YOLO 切换确认 ============

TEST_CASE("yolo_switch_requires_confirmation", "[policy][switch]") {
 auto mock_false = [](const std::string&) -> bool { return false; };
 auto mock_true = [](const std::string&) -> bool { return true; };

 SECTION("mock_false returns false") {
  REQUIRE(confirm_yolo_switch("agent", mock_false) == false);
 }

 SECTION("mock_true returns true") {
  REQUIRE(confirm_yolo_switch("plan", mock_true) == true);
 }
}

// ============ §7.2.2 模式切换确认规则 ============

TEST_CASE("plan_to_agent_silent", "[policy][switch]") {
 SECTION("plan to agent needs no confirmation") {
  REQUIRE(requires_yolo_confirmation("plan", "agent") == false);
 }

 SECTION("plan to yolo needs confirmation") {
  REQUIRE(requires_yolo_confirmation("plan", "yolo") == true);
 }

 SECTION("agent to yolo needs confirmation") {
  REQUIRE(requires_yolo_confirmation("agent", "yolo") == true);
 }

 SECTION("yolo to plan needs confirmation") {
  REQUIRE(requires_yolo_confirmation("yolo", "plan") == true);
 }

 SECTION("agent to plan needs no confirmation") {
  REQUIRE(requires_yolo_confirmation("agent", "plan") == false);
 }
}