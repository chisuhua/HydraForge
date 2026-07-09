// pdk/llama_engine/src/llama_engine_entry.cpp
// 功能描述：Llama Engine Plugin 入口 (C14, Phase 5 B2.1)
//           export extern "C" pdk_register_tools(IToolRegistry&),
//           聚合 12 个工具注册 (4 engine + 4 model + 4 arch)。
//           遵循 PDK Plugin 契约 (ADR-0021, ADR-0034 C7 范式)。
// 设计依据：openspec/changes/phase5-llama-engine-plugin/
//          proposal.md §2-4, tasks.md §2-4
// 参考范式：pdk/model_router/cost_strategy/cost_router.cpp (C7 Phase 1)
// 作者：C14 Oracle review session
// 最后修改日期：2026-07-07

#include "agenticdsl/contract/itool_registry.h"
#include "agenticdsl/plugin/plugin_info.h"

// 三个内部注册函数（实现分散在各自 .cpp 文件中）
// 声明而非 include，避免符号冲突
namespace agenticdsl::pdk::llama {

void register_engine_tools(::agenticdsl::IToolRegistry& registry);
void register_model_tools(::agenticdsl::IToolRegistry& registry);
void register_arch_tools(::agenticdsl::IToolRegistry& registry);

} // namespace agenticdsl::pdk::llama

// ============================================================================
// Plugin 入口：聚合 12 个推理工具注册
// ============================================================================

extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry) {
  agenticdsl::pdk::llama::register_engine_tools(registry);  // §2: 4 tools
  agenticdsl::pdk::llama::register_model_tools(registry);   // §3: 4 tools
  agenticdsl::pdk::llama::register_arch_tools(registry);    // §4: 4 tools
}

// ============================================================================
// Plugin 元数据 (PluginLoader 在 dlopen 后零代码执行读取)
// 数据符号格式 — 与 C7 model_router 一致 (非函数版本)
// ============================================================================

extern "C" const hydraforge::PluginInfo pdk_plugin_info = {
  hydraforge::CURRENT_ABI_VERSION,                              // abi_version = 2 (V2 with dependencies field)
  "hydraforge_llama_engine",                                    // name
  1, 0, 0,                                                      // semver 1.0.0
  "Llama.cpp reference engine plugin — 12 tools (engine/model/arch)", // description
  "engine,model,inference,llama,sampler",                          // capabilities
  ""                                                            // dependencies (无依赖, llm 由 host 注入)
};