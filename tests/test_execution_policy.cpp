// tests/test_execution_policy.cpp
// 功能描述：IExecutionPolicy三模式策略单元测试。覆盖 PlanModePolicy /
// AgentModePolicy / YoloModePolicy 的核心决策、阶段流程控制、IPER
//行为控制与舰队并发度。
// 设计依据：ADR-0031 §2 / ADR-0004 §6-9
// 作者：AgenticDSL Phase3 / Track A
// 最后修改日期：2026-06-10
#include "catch_amalgamated.hpp"
#include "agenticdsl/policy/iexecution_policy.h"
#include "common/policy/plan_mode_policy.h"
#include "common/policy/agent_mode_policy.h"
#include "common/policy/yolo_mode_policy.h"

using namespace agenticdsl;

namespace {

//构造测试用 ToolMetadata辅助函数
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

//构造测试用 ToolCallContext辅助函数
ToolCallContext make_ctx(const std::string& session_id = "sess-1",
 const std::string& caller_layer = "workflow") {
 ToolCallContext ctx;
 ctx.session_id = session_id;
 ctx.caller_layer = caller_layer;
 ctx.target_path = "/tmp/test";
 ctx.is_in_fleet_mode = false;
 ctx.call_count_this_session =0;
 return ctx;
}

} // namespace

// ============ PlanModePolicy ============

TEST_CASE("PlanModePolicy skips approval for ReadOnly tools", "[policy][plan]") {
 PlanModePolicy policy;

 ToolMetadata meta = make_tool_meta(ToolCategory::ReadOnly);
 ToolCallContext ctx = make_ctx();

 REQUIRE(policy.requires_approval(meta, ctx) == false);
}

TEST_CASE("PlanModePolicy requires approval for WriteFile tools", "[policy][plan]") {
 PlanModePolicy policy;

 ToolMetadata meta = make_tool_meta(ToolCategory::WriteFile);
 ToolCallContext ctx = make_ctx();

 REQUIRE(policy.requires_approval(meta, ctx) == true);
}

TEST_CASE("PlanModePolicy requires approval for Execute tools", "[policy][plan]") {
 PlanModePolicy policy;

 ToolMetadata meta = make_tool_meta(ToolCategory::Execute);
 REQUIRE(policy.requires_approval(meta, make_ctx()) == true);
}

TEST_CASE("PlanModePolicy requires approval for Network tools", "[policy][plan]") {
 PlanModePolicy policy;

 ToolMetadata meta = make_tool_meta(ToolCategory::Network);
 REQUIRE(policy.requires_approval(meta, make_ctx()) == true);
}

TEST_CASE("PlanModePolicy requires approval for StateModify tools", "[policy][plan]") {
 PlanModePolicy policy;

 ToolMetadata meta = make_tool_meta(ToolCategory::StateModify);
 REQUIRE(policy.requires_approval(meta, make_ctx()) == true);
}

TEST_CASE("PlanModePolicy stage flow control", "[policy][plan]") {
 PlanModePolicy policy;

 REQUIRE(policy.should_auto_execute() == false);
 REQUIRE(policy.should_show_plan() == true);
 REQUIRE(policy.should_show_result_summary() == true);
 REQUIRE(policy.mode_name() == "plan");
}

TEST_CASE("PlanModePolicy IPER and fleet settings", "[policy][plan]") {
 PlanModePolicy policy;

 REQUIRE(policy.should_auto_decide_retry() == false);
 REQUIRE(policy.should_show_reflection() == true);
 REQUIRE(policy.fleet_max_concurrency() ==8);
}

// ============ AgentModePolicy ============

TEST_CASE("AgentModePolicy respects approval.requires_approval_in_agent=true",
 "[policy][agent]") {
 AgentModePolicy policy;

 ApprovalPolicy approval;
 approval.requires_approval_in_plan = false;
 approval.requires_approval_in_agent = true;
 approval.requires_approval_in_yolo = false;
 approval.force_approval_always = false;

 ToolMetadata meta = make_tool_meta(ToolCategory::WriteFile, approval);
 REQUIRE(policy.requires_approval(meta, make_ctx()) == true);
}

TEST_CASE("AgentModePolicy skips approval when requires_approval_in_agent=false",
 "[policy][agent]") {
 AgentModePolicy policy;

 ApprovalPolicy approval;
 approval.requires_approval_in_plan = false;
 approval.requires_approval_in_agent = false;
 approval.requires_approval_in_yolo = false;
 approval.force_approval_always = false;

 ToolMetadata meta = make_tool_meta(ToolCategory::WriteFile, approval);
 REQUIRE(policy.requires_approval(meta, make_ctx()) == false);
}

TEST_CASE("AgentModePolicy ignores force_approval_always for normal checks",
 "[policy][agent]") {
 AgentModePolicy policy;

 // force_approval_always=true 但 requires_approval_in_agent=false
 // Agent模式仅读 in_agent字段，不额外触发审批
 ApprovalPolicy approval;
 approval.requires_approval_in_agent = false;
 approval.force_approval_always = true;

 ToolMetadata meta = make_tool_meta(ToolCategory::Execute, approval);
 REQUIRE(policy.requires_approval(meta, make_ctx()) == false);
}

TEST_CASE("AgentModePolicy stage flow control", "[policy][agent]") {
 AgentModePolicy policy;

 REQUIRE(policy.should_auto_execute() == true);
 REQUIRE(policy.should_show_plan() == false);
 REQUIRE(policy.should_show_result_summary() == true);
 REQUIRE(policy.mode_name() == "agent");
}

TEST_CASE("AgentModePolicy IPER and fleet settings", "[policy][agent]") {
 AgentModePolicy policy;

 REQUIRE(policy.should_auto_decide_retry() == true);
 REQUIRE(policy.should_show_reflection() == false);
 REQUIRE(policy.fleet_max_concurrency() ==16);
}

// ============ YoloModePolicy ============

TEST_CASE("YoloModePolicy skips approval when force_approval_always=false",
 "[policy][yolo]") {
 YoloModePolicy policy;

 // 默认 ApprovalPolicy：force_approval_always=false
 ToolMetadata meta = make_tool_meta(ToolCategory::WriteFile);
 REQUIRE(policy.requires_approval(meta, make_ctx()) == false);
}

TEST_CASE("YoloModePolicy requires approval when force_approval_always=true",
 "[policy][yolo]") {
 YoloModePolicy policy;

 ApprovalPolicy approval;
 approval.force_approval_always = true;

 ToolMetadata meta = make_tool_meta(ToolCategory::WriteFile, approval,
 "code::delete_file", "code");
 REQUIRE(policy.requires_approval(meta, make_ctx()) == true);
}

TEST_CASE("YoloModePolicy ignores in_yolo field for normal tools",
 "[policy][yolo]") {
 YoloModePolicy policy;

 // 即使 requires_approval_in_yolo=true，只要 force_approval_always=false
 // YOLO模式仅依赖 force_approval_always 安全底线
 ApprovalPolicy approval;
 approval.requires_approval_in_yolo = true;
 approval.force_approval_always = false;

 ToolMetadata meta = make_tool_meta(ToolCategory::Execute, approval);
 REQUIRE(policy.requires_approval(meta, make_ctx()) == false);
}

TEST_CASE("YoloModePolicy stage flow control", "[policy][yolo]") {
 YoloModePolicy policy;

 REQUIRE(policy.should_auto_execute() == true);
 REQUIRE(policy.should_show_plan() == false);
 REQUIRE(policy.should_show_result_summary() == false);
 REQUIRE(policy.mode_name() == "yolo");
}

TEST_CASE("YoloModePolicy IPER and fleet settings", "[policy][yolo]") {
 YoloModePolicy policy;

 REQUIRE(policy.should_auto_decide_retry() == true);
 REQUIRE(policy.should_show_reflection() == false);
 REQUIRE(policy.fleet_max_concurrency() ==32);
}

// ============ 基类契约 ============

TEST_CASE("All policies are valid IExecutionPolicy instances",
 "[policy][contract]") {
 PlanModePolicy plan;
 AgentModePolicy agent;
 YoloModePolicy yolo;

 // 通过基类指针验证多态契约
 const IExecutionPolicy* policies[3] = {&plan, &agent, &yolo};
 const std::string expected_modes[3] = {"plan", "agent", "yolo"};
 const size_t expected_concurrency[3] = {8,16,32};

 for (int i =0; i <3; ++i) {
 REQUIRE(policies[i]->mode_name() == expected_modes[i]);
 REQUIRE(policies[i]->fleet_max_concurrency() == expected_concurrency[i]);
 }
}

TEST_CASE("Default ApprovalPolicy values are conservative",
 "[policy][types]") {
 // 默认策略：plan/agent 均审批，yolo 不审批，安全底线关闭
 ApprovalPolicy default_policy;
 REQUIRE(default_policy.requires_approval_in_plan == true);
 REQUIRE(default_policy.requires_approval_in_agent == true);
 REQUIRE(default_policy.requires_approval_in_yolo == false);
 REQUIRE(default_policy.force_approval_always == false);
}
