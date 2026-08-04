// tests/test_tool_coordinator_hooks.cpp
// 功能描述：ToolCoordinator pre/post hook 机制测试 (ADR-0069)
// 设计依据：ADR-0069 §决策 1-6 + ADR-0068 事件契约
// 作者：AgenticDSL Phase 6 / adr-0069-tool-coordinator-hooks change
// 最后修改日期：2026-08-04
#include "catch_amalgamated.hpp"
#include "agenticdsl/contract/itool_hook_registry.h"

TEST_CASE("itool_hook_registry_contract_compiles", "[tool_coordinator_hooks][stage1]") {
  agenticdsl::PreHookResult r;
  r.action = agenticdsl::PreHookResult::Deny;
  r.deny_reason = "blocked";
  REQUIRE(r.action == agenticdsl::PreHookResult::Deny);
  REQUIRE(r.deny_reason == "blocked");
}

#include "common/tools/tool_hook_registry.h"

TEST_CASE("tool_hook_registry_executes_matching_pre_hook", "[tool_coordinator_hooks][stage2]") {
  agenticdsl::ToolHookRegistry registry;
  bool called = false;
  registry.register_pre_hook(
      "*",
      [&](const auto&, const auto&, const auto&) {
        called = true;
        return agenticdsl::PreHookResult{};
      },
      0,
      agenticdsl::HookErrorPolicy::FailClosed);

  std::vector<std::string> warnings;
  agenticdsl::ToolMetadata meta;
  meta.name = "shell/exec";
  agenticdsl::ToolCallContext ctx;
  auto result = registry.apply_pre_hooks(meta, ctx, {}, warnings);

  REQUIRE(called);
  REQUIRE(result.action == agenticdsl::PreHookResult::Continue);
  REQUIRE(warnings.empty());
}
