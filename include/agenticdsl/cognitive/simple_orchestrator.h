// include/agenticdsl/cognitive/simple_orchestrator.h
// 文件头注释
// 功能描述：SimpleCognitiveOrchestrator — B 轨道（Track 0.2）单轮 ReAct 编排器。
//          通过 ToolRegistry + ILLMProvider 完成 "LLM 返回 JSON tool_call → 调用工具 → 包装为 ToolResult" 的最小闭环。
//          MVP 阶段：仅单轮，标 TODO(mvp)；多轮 + 状态机留待后续 Phase 1。
// 设计依据：ADR-0015（IPER 闭环）+ ADR-0019（IInteractionBus）+ plan §6。
// 作者：AgenticDSL Phase 0 / Track B
// 最后修改日期：2026-06-08
//
// 关于接口的说明：
// - Pre-Phase 已定义 agenticdsl::ICognitiveOrchestrator（process 回调为 ExecutionResult），
//   该接口面向高层 IPER 闭环（Phase 1+）。
// - 本类使用 ToolResult 作为回调载荷（更轻量，定位为 MVP/B 轨道层），因此以**具体类**形式提供，
//   不与 Pre-Phase ICognitiveOrchestrator 形成重定义冲突。
// - 后续可在不破坏 B 阶段契约的前提下，再为 SimpleCognitiveOrchestrator 适配到更高层接口。

#pragma once

#include "common/tools/registry.h"
#include "common/llm/llm_types.h"
#include "core/types/tool_result.h"

#include <functional>
#include <string>

namespace agenticdsl {

/**
 * @brief SimpleCognitiveOrchestrator — 单轮 ReAct 编排器
 *
 * 生命周期：构造时注入依赖（ToolRegistry* + ILLMProvider*），两个指针均可为 nullptr（MVP 允许）；
 * process() 内部会在依赖缺失时通过 ToolResult::error() 报告，而非抛异常。
 *
 * 单轮 ReAct 流程：
 *   1. 构造 prompt（硬编码工具列表与 JSON 格式约束）
 *   2. 调用 llm_->generate()
 *   3. 解析 JSON tool_call（含 tool 字段与 args 对象）
 *   4. 校验工具是否存在 → 调用 registry_->call_tool()
 *   5. 将工具原始输出包装为 ToolResult（ok=true，data=工具输出，meta.tool_name=工具名）
 *
 * 异常安全：所有失败均通过 callback 传回（不抛异常至调用方）。
 */
class SimpleCognitiveOrchestrator {
 public:
  /**
   * @brief 构造 SimpleCognitiveOrchestrator
   * @param registry 工具注册表指针（MVP 允许 nullptr，但 process() 将返回错误）
   * @param llm      LLM Provider 指针（MVP 允许 nullptr，但 process() 将返回错误）
   */
  explicit SimpleCognitiveOrchestrator(
      ToolRegistry* registry = nullptr,
      ILLMProvider* llm = nullptr);

  /**
   * @brief 启动一次会话（MVP：单轮 ReAct）
   * @param session_id  会话标识（MVP 暂未使用，预留用于多轮/状态关联）
   * @param on_complete 处理完成时的回调；调用一次（成功或失败），载荷为 ToolResult
   *
   * 语义契约：若 on_complete 为 nullptr 则不调用（避免空指针 deref）。
   * 异常安全：不抛任何异常至调用方（内部 std::exception 被捕获并包装）。
   */
  void process(const std::string& session_id,
               std::function<void(ToolResult)> on_complete);

 private:
  ToolRegistry* registry_;
  ILLMProvider* llm_;

  /**
   * @brief 单轮 ReAct 内部方法
   * @param user_prompt 用户提示（MVP：硬编码 "demo-session" 等价文本）
   * @return ToolResult（成功或失败均通过 ToolResult 表达）
   */
  ToolResult react_once(const std::string& user_prompt);
};

} // namespace agenticdsl