// pdk/g3_knowledge_base/src/g3_entry.cpp
// 功能描述：G3 Knowledge Base Plugin 入口 (Phase 6 W1)
//           export extern "C" pdk_register_tools(IToolRegistry&),
//           注册 knowledge_base/query 工具。
//           遵循 PDK Plugin 契约 (ADR-0021, ADR-0034 C7 范式)。
// 设计依据：openspec/changes/phase6-service-ification-v1/
//          tasks.md §2.1-2.2, specs/knowledge-base-agent/spec.md
// 参考范式：pdk/llama_engine/src/llama_engine_entry.cpp (C14)
// 作者：Phase 6 W1 (Sisyphus-Junior)
// 最后修改日期：2026-07-15

#include "agenticdsl/contract/itool_registry.h"
#include "agenticdsl/plugin/plugin_info.h"

namespace agenticdsl::pdk::g3 {

/// 注册 knowledge_base/query 工具 (实现在 g3_query.cpp)
void register_g3_tools(::agenticdsl::IToolRegistry& registry);

} // namespace agenticdsl::pdk::g3

// ============================================================================
// Plugin 入口：注册 knowledge_base/query 工具
// ============================================================================

extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry) {
  agenticdsl::pdk::g3::register_g3_tools(registry);
}

// ============================================================================
// Plugin 元数据 (PluginLoader 在 dlopen 后零代码执行读取)
// 数据符号格式 — 与 C7 model_router / C14 llama_engine 一致
// ============================================================================

extern "C" const hydraforge::PluginInfo pdk_plugin_info = {
  hydraforge::CURRENT_ABI_VERSION,                              // abi_version = 2
  "hydraforge_g3_knowledge_base",                               // name
  0, 1, 0,                                                      // semver 0.1.0
  "G3 Knowledge Base plugin — retrieval + LLM Q&A, multi-turn sessions", // description
  "knowledge,qa,retrieval",                                      // capabilities
  ""                                                             // dependencies (无依赖, MockLLM 自包含)
};