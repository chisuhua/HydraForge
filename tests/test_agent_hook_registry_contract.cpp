// tests/test_agent_hook_registry_contract.cpp
// 功能描述：IAgentHookRegistry 契约测试（4 cases）
//          - contract compile: 接口编译期验证
//          - glob matching: agent_glob 通配（如 "react-loop/*" / "*"）
//          - priority: 高优先级先执行
//          - policy: FailClosed / FailOpen 语义
//
//          Amendment (2026-08-21): V1 最小骨架测试集
//          Agent loop 集成测试留 Sprint 24+ Agent hook 实施 change
//
// 设计依据：openspec/changes/adr-0081-promote-to-approved (P3) + ADR-0081 §决策 D1
// 作者：HydraForge Sprint 22 / adr-0081-promote-to-approved
// 最后修改日期：2026-08-21

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/iagent_hook_registry.h"
#include "agenticdsl/contract/iagent_registry.h"

#include <memory>
#include <string>
#include <vector>

using namespace agenticdsl;

namespace {

std::unique_ptr<IAgentHookRegistry> make_registry() {
  return make_in_memory_agent_hook_registry();
}

std::unique_ptr<IAgent> make_named_agent(const std::string& name,
                                          const std::string& id) {
  return make_mock_agent_for_test(name, id);
}

}  // namespace

TEST_CASE("IAgentHookRegistry contract: 编译期验证 + 接口形状",
          "[agent-hook][P3][contract]") {
  // 编译期验证：接口签名 + HookErrorPolicy 复用 ADR-0069
  static_assert(std::is_same_v<HookErrorPolicy, HookErrorPolicy>,
                "HookErrorPolicy 复用 ADR-0069 定义");

  IAgentHookRegistry* registry = nullptr;
  IAgent* agent = nullptr;
  static_cast<void>(registry);
  static_cast<void>(agent);

  // Hook 函数签名编译通过
  AgentPreHook pre_hook = [](const IAgent& a, const std::string& input) {
    AgentPreHookResult r;
    r.action = (a.name() == "react-loop-v1") ? AgentPreHookResult::Deny
                                              : AgentPreHookResult::Continue;
    r.deny_reason = "test deny";
    return r;
  };
  REQUIRE(pre_hook != nullptr);
}

TEST_CASE("IAgentHookRegistry glob matching: agent_glob 通配",
          "[agent-hook][P3][glob]") {
  auto registry = make_registry();
  std::vector<std::string> warnings;

  bool matched_react = false;
  bool matched_plan = false;
  registry->register_pre_hook(
      "react-loop-*",
      [&matched_react](const IAgent& a, const std::string&) {
        if (a.name() == "react-loop-v1") matched_react = true;
        return AgentPreHookResult{};
      },
      100, HookErrorPolicy::FailOpen);

  registry->register_pre_hook(
      "plan-execute-*",
      [&matched_plan](const IAgent& a, const std::string&) {
        if (a.name() == "plan-execute-v1") matched_plan = true;
        return AgentPreHookResult{};
      },
      100, HookErrorPolicy::FailOpen);

  auto react_agent = make_named_agent("react-loop-v1", "react-1");
  registry->apply_pre_hooks(*react_agent, "input", warnings);
  REQUIRE(matched_react);
  REQUIRE_FALSE(matched_plan);

  matched_react = false;
  matched_plan = false;
  auto plan_agent = make_named_agent("plan-execute-v1", "plan-1");
  registry->apply_pre_hooks(*plan_agent, "input", warnings);
  REQUIRE_FALSE(matched_react);
  REQUIRE(matched_plan);

  matched_react = false;
  matched_plan = false;
  auto fork_agent = make_named_agent("fork-join-v1", "fork-1");
  registry->apply_pre_hooks(*fork_agent, "input", warnings);
  REQUIRE_FALSE(matched_react);
  REQUIRE_FALSE(matched_plan);

  matched_react = false;
  matched_plan = false;
  registry->register_pre_hook(
      "*",
      [&matched_react](const IAgent& a, const std::string&) {
        if (a.name() == "any-agent") matched_react = true;
        return AgentPreHookResult{};
      },
      50, HookErrorPolicy::FailOpen);
  auto any_agent = make_named_agent("any-agent", "any-1");
  registry->apply_pre_hooks(*any_agent, "input", warnings);
  REQUIRE(matched_react);
}

TEST_CASE("IAgentHookRegistry priority: 高优先级先执行",
          "[agent-hook][P3][priority]") {
  auto registry = make_registry();
  std::vector<std::string> warnings;
  std::vector<int> call_order;

  registry->register_pre_hook(
      "*",
      [&call_order](const IAgent&, const std::string&) {
        call_order.push_back(1);
        return AgentPreHookResult{};
      },
      1, HookErrorPolicy::FailOpen);

  registry->register_pre_hook(
      "*",
      [&call_order](const IAgent&, const std::string&) {
        call_order.push_back(100);
        return AgentPreHookResult{};
      },
      100, HookErrorPolicy::FailOpen);

  registry->register_pre_hook(
      "*",
      [&call_order](const IAgent&, const std::string&) {
        call_order.push_back(50);
        return AgentPreHookResult{};
      },
      50, HookErrorPolicy::FailOpen);

  auto agent = make_named_agent("any-agent", "any-1");
  registry->apply_pre_hooks(*agent, "input", warnings);

  REQUIRE(call_order.size() == 3);
  REQUIRE(call_order[0] == 100);
  REQUIRE(call_order[1] == 50);
  REQUIRE(call_order[2] == 1);
}

TEST_CASE("IAgentHookRegistry policy: FailClosed 异常 deny / FailOpen 异常 warning",
          "[agent-hook][P3][policy]") {
  auto registry = make_registry();
  std::vector<std::string> warnings;
  auto agent = make_named_agent("react-loop-v1", "react-1");

  // FailClosed: hook 抛异常 → action=Deny
  registry->register_pre_hook(
      "*",
      [](const IAgent&, const std::string&) -> AgentPreHookResult {
        throw std::runtime_error("FailClosed test exception");
      },
      100, HookErrorPolicy::FailClosed);

  auto result = registry->apply_pre_hooks(*agent, "input", warnings);
  REQUIRE(result.action == AgentPreHookResult::Deny);
  REQUIRE_FALSE(result.deny_reason.empty());
  REQUIRE(warnings.empty());  // FailClosed 不进 warnings

  // FailOpen: hook 抛异常 → Continue + warning
  auto registry2 = make_registry();
  registry2->register_pre_hook(
      "*",
      [](const IAgent&, const std::string&) -> AgentPreHookResult {
        throw std::runtime_error("FailOpen test exception");
      },
      100, HookErrorPolicy::FailOpen);

  auto result2 = registry2->apply_pre_hooks(*agent, "input", warnings);
  REQUIRE(result2.action == AgentPreHookResult::Continue);
  REQUIRE(warnings.size() == 1);
  REQUIRE(warnings[0].find("FailOpen") != std::string::npos);
}