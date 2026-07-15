// pdk/g1_coding_assistant/src/g1_state.h
// 功能描述：G1 插件全局状态 — tool manifest + IToolRegistry 指针 + MockLLMProvider 引用
//          供 g1_entry.cpp 和 g1_agent.cpp 共享访问。
//          线程安全：std::mutex 保护写操作（manifest 加载 + registry 设置）
// 设计依据：openspec/changes/phase6-service-ification-v1/
//          tasks.md §3.3 (manifest), §3.6 (MockLLMProvider wiring)
// 参考范式：pdk/g3_knowledge_base/src/g3_state.h (SessionStore)
// 作者：Phase 6 W1 (Sisyphus-Junior)
// 最后修改日期：2026-07-15

#pragma once

#include "agenticdsl/contract/itool_registry.h"

#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace agenticdsl::pdk::g1 {

/// G1 全局状态 (单例, 线程安全)
struct G1State {
  std::mutex mtx;

  /// 工具清单: G1 声明依赖的工具列表 (v1 MVP 仅 knowledge_base/query)
  std::vector<std::string> tool_manifest;

  /// 指向注册时传入的 IToolRegistry (用于 handler 内调用 G3 工具)
  ::agenticdsl::IToolRegistry* registry = nullptr;

  /// LLM 回调 (prompt → answer) — 测试注入 MockLLMProvider 或 mock 实现
  /// 与 G3 的 g3_set_llm_callback 模式一致, 避免 .so 链接 agenticdsl_common
  std::function<std::string(const std::string& prompt)> llm_callback;

  /// LLM 调用计数 (测试验证 wiring)
  int llm_call_count = 0;
};

/// 全局状态单例
G1State& g1_state();

} // namespace agenticdsl::pdk::g1