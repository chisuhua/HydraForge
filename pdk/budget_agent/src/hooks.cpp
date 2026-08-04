// pdk/budget_agent/src/hooks.cpp
// budget_agent pre-hook: deny any tool call when the global budget is exhausted
// 设计依据：ADR-0069 §决策 3 (FailClosed budget pre-hook)
// 作者：AgenticDSL Phase 6 / adr-0069-tool-coordinator-hooks change
// 最后修改日期：2026-08-04
#include <agenticdsl/contract/itool_hook_registry.h>
#include "budget_agent.h"

namespace {

agenticdsl::PreHook make_budget_pre_hook() {
  return [](const agenticdsl::ToolMetadata&,
            const agenticdsl::ToolCallContext&,
            const std::unordered_map<std::string, std::string>&)
            -> agenticdsl::PreHookResult {
    auto& store = pdk_budget_agent::BudgetStore::instance();
    if (store.remaining_usd() <= 0.0) {
      agenticdsl::PreHookResult r;
      r.action = agenticdsl::PreHookResult::Deny;
      r.deny_reason = "budget exceeded";
      return r;
    }
    return agenticdsl::PreHookResult{};
  };
}

}  // namespace

extern "C" void pdk_register_hooks(agenticdsl::IToolHookRegistry& registry) {
  registry.register_pre_hook(
      "*",
      make_budget_pre_hook(),
      0,
      agenticdsl::HookErrorPolicy::FailClosed);
}
