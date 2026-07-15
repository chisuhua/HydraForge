// pdk/g1_coding_assistant/src/g1_agent.cpp
// 功能描述：G1 工具注册 + DEFINE_AGENT + 2-step ReAct handler (Phase 6 W1)
//          Commit 1 骨架 — 仅提供 register_g1_tools 空实现。
//          完整实现在 Commit 2。
// 设计依据：openspec/changes/phase6-service-ification-v1/
//          tasks.md §3.2-3.7
// 作者：Phase 6 W1 (Sisyphus-Junior)
// 最后修改日期：2026-07-15

#include "g1_state.h"

#include "agenticdsl/contract/itool_registry.h"
#include "agenticdsl/pdk/agent_macros.h"
#include "common/policy/execution_policy.h"

// DEFINE_AGENT — spec 合规 (定义 CodingAssistantAgent class, 2 参数形式)
DEFINE_AGENT(CodingAssistant, ::hydraforge::pdk::AgentLoopType::React)

namespace agenticdsl::pdk::g1 {

G1State& g1_state() {
  static G1State s;
  return s;
}

void register_g1_tools(::agenticdsl::IToolRegistry& registry) {
  // Commit 2 will implement: discover knowledge_base/query + register coding_assistant/review
  (void)registry;
}

} // namespace agenticdsl::pdk::g1