// src/modules/cognitive/simple_orchestrator.cpp
// 文件头注释
// 功能描述：SimpleCognitiveOrchestrator 的骨架实现（仅构造 + 空 process）。
//          完整 ReAct 循环将在 Task 8-9 中实现。
// 设计依据：plan §6（Task 6 骨架阶段）
// 作者：AgenticDSL Phase 0 / Track B
// 最后修改日期：2026-06-08
//
// 注意：本文件作为骨架版本（Task 6），仅完成 ctor 与空 process，
//      以便 CMake target 可立即构建；Task 8-9 将填充 react_once + 完整 ReAct。

#include "modules/cognitive/simple_orchestrator.h"

namespace agenticdsl {

SimpleCognitiveOrchestrator::SimpleCognitiveOrchestrator(
    ToolRegistry* registry,
    ILLMProvider* llm)
    : registry_(registry), llm_(llm) {}

void SimpleCognitiveOrchestrator::process(
    const std::string& /*session_id*/,
    std::function<void(ToolResult)> on_complete) {
  // 骨架版本：仅在依赖缺失时返回错误，正常路径留待 Task 8-9
  if (!registry_ || !llm_) {
    if (on_complete) {
      on_complete(ToolResult::error(
          "ERR_ORCHESTRATOR.NOT_INITIALIZED",
          "Missing dependencies (registry or llm)"));
    }
    return;
  }
  // TODO(mvp): implement react_once() in Task 8-9
  if (on_complete) {
    on_complete(ToolResult::error(
        "ERR_ORCHESTRATOR.NOT_IMPLEMENTED",
        "ReAct loop pending Task 8-9 implementation"));
  }
}

ToolResult SimpleCognitiveOrchestrator::react_once(const std::string& /*user_prompt*/) {
  // 骨架版本：返回 NOT_IMPLEMENTED；Task 8-9 填充
  return ToolResult::error(
      "ERR_ORCHESTRATOR.NOT_IMPLEMENTED",
      "react_once pending Task 8-9 implementation");
}

} // namespace agenticdsl