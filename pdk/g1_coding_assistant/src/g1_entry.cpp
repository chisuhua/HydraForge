// pdk/g1_coding_assistant/src/g1_entry.cpp
// 功能描述：G1 Coding Assistant Plugin 入口 (Phase 6 W1)
//           export extern "C" pdk_register_tools(IToolRegistry&),
//           注册 coding_assistant/review 工具 + 发现 knowledge_base/query。
//           pdk_plugin_info 声明对 G3 的依赖 (dependencies = "hydraforge_g3_knowledge_base").
//           DEFINE_AGENT 用于 spec 合规 (定义 CodingAssistantAgent class)。
// 设计依据：openspec/changes/phase6-service-ification-v1/
//          tasks.md §3.1-3.7, specs/coding-assistant-agent/spec.md
// 参考范式：pdk/g3_knowledge_base/src/g3_entry.cpp (Phase 6 W1)
// 作者：Phase 6 W1 (Sisyphus-Junior)
// 最后修改日期：2026-07-15

#include "g1_state.h"

#include "agenticdsl/contract/itool_registry.h"
#include "agenticdsl/plugin/plugin_info.h"
#include "common/policy/execution_policy.h"

namespace agenticdsl::pdk::g1 {

/// 注册 G1 工具 (实现在 g1_agent.cpp)
void register_g1_tools(::agenticdsl::IToolRegistry& registry);

} // namespace agenticdsl::pdk::g1

// ============================================================================
// Plugin 入口：注册 coding_assistant/review 工具
// ============================================================================

extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry) {
  agenticdsl::pdk::g1::register_g1_tools(registry);
}

// ============================================================================
// Plugin 元数据 (PluginLoader 在 dlopen 后零代码执行读取)
// 依赖声明: hydraforge_g3_knowledge_base (G1 需要 G3 的 knowledge_base/query 工具)
// ============================================================================

extern "C" const hydraforge::PluginInfo pdk_plugin_info = {
  hydraforge::CURRENT_ABI_VERSION,                              // abi_version = 2
  "hydraforge_g1_coding_assistant",                             // name
  0, 1, 0,                                                      // semver 0.1.0
  "G1 Coding Assistant — 2-step ReAct loop orchestrating G3 knowledge base", // description
  "coding,review,react,orchestration",                          // capabilities
  "hydraforge_g3_knowledge_base"                                // dependencies (需要 G3)
};