// include/agenticdsl/pdk/agent_loops/react_loop.h
// 文件头注释
// 功能描述：ReactLoop — PDK Agent 单轮 ReAct 循环实现 (Sprint 4 已 ship 模式升级)。
//          内部委托 agenticdsl::SimpleCognitiveOrchestrator 单轮 ReAct
//          (LLM 解析 tool_call JSON → 调用 ToolRegistry → 包装为 ToolResult)。
//          统一通过 LoopResult 接口返回, 与 PlanExecuteLoop / ForkJoinLoop 共享返回类型。
//          Sprint 20 升级: 之前 DEFINE_AGENT 宏内联此逻辑, 本次提取为独立 class
//          供 LoopDispatcher 模板分发使用 (ADR-0021 §3.2)。
// 设计依据：ADR-0021 §3.2 + ADR-0020 §2.2.1 SimpleCognitiveOrchestrator
//          + openspec/changes/pdk-plan-execute-fork-join
// 作者：AgenticDSL Phase 1 Sprint 20
// 最后修改日期：2026-08-01

#pragma once

#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/cognitive/simple_orchestrator.h"
#include "agenticdsl/pdk/agent_loops/loop_result.h"
#include "agenticdsl/types/layered_context.h"
#include "core/engine.h"
#include "core/types/tool_result.h"

#include <memory>
#include <string>

namespace hydraforge::pdk {

/**
 * @brief ReactLoop — PDK Agent 单轮 ReAct 循环 (Sprint 4 MVP, Sprint 20 升级为 class)
 *
 * 状态机:
 *   Thinking → Acting → Observing → Done (单轮即 Done, 不循环)
 *
 * 与 Sprint 4 DEFINE_AGENT 宏内联逻辑保持完全一致 — 仅形态从宏内联代码
 * 提升为独立 class, 以便 LoopDispatcher<AgentLoopType::React>::Type 引用。
 *
 * 行为契约:
 *   - run() 内部创建 SimpleCognitiveOrchestrator, 调用 process() 单轮 ReAct
 *   - ToolResult 写入 final_context.working.data[agent_output]
 *   - success 字段映射: ToolResult.ok = true → success = true
 *   - 引擎或工具为 null 时返回 success=false + message 描述原因
 */
class ReactLoop {
 public:
  /**
   * @brief 4 状态机 (单轮即 Done)
   */
  enum class State { Thinking, Acting, Observing, Done };

  /**
   * @brief 构造 ReactLoop
   * @param engine  DSLEngine unique_ptr (per-agent 隔离)
   * @param bus     IInteractionBus shared_ptr (空指针允许)
   *
   * 契约:
   *   - 构造时调用 engine_->set_interaction_bus(bus_) (F7 顺序, 与 CognitiveWorker 一致)
   *   - 构造后 state_ == Thinking
   */
  ReactLoop(std::unique_ptr<agenticdsl::DSLEngine> engine,
            std::shared_ptr<agenticdsl::IInteractionBus> bus)
      : engine_(std::move(engine)), bus_(std::move(bus)) {
    if (engine_ && bus_) {
      engine_->set_interaction_bus(bus_);
    }
    state_ = State::Thinking;
  }

  /**
   * @brief 单轮 ReAct
   * @param prompt 用户提示
   * @param ctx    LayeredContext (本循环不使用, 仅传递作为 final_context 起点)
   * @return LoopResult
   *
   * 行为:
   *   - Thinking: 准备 SimpleCognitiveOrchestrator
   *   - Acting:   委托 orch.process(prompt, callback)
   *   - Observing: 将 ToolResult 包装到 final_context.working.data
   *   - Done: 返回 LoopResult
   */
  LoopResult run(const std::string& prompt, const agenticdsl::LayeredContext& ctx) {
    LoopResult result;
    result.final_context = ctx;
    result.total_steps = 1;

    if (!engine_) {
      result.success = false;
      result.message = "ReactLoop: DSLEngine is null";
      result.failed_phase = "Thinking";
      state_ = State::Done;
      return result;
    }

    state_ = State::Thinking;
    agenticdsl::SimpleCognitiveOrchestrator orch(
        &engine_->get_tool_registry(), engine_->get_llm_provider());

    state_ = State::Acting;
    agenticdsl::ToolResult tool_result;
    bool invoked = false;
    orch.process(prompt, [&tool_result, &invoked](agenticdsl::ToolResult r) {
      tool_result = std::move(r);
      invoked = true;
    });

    state_ = State::Observing;
    if (!invoked) {
      result.success = false;
      result.message = "ReactLoop: orchestrator did not invoke callback";
      result.failed_phase = "Observing";
    } else {
      result.success = tool_result.ok;
      result.message = tool_result.ok ? "React loop completed" : "React loop failed";
      result.final_context.working["data"] = tool_result.data;
      if (tool_result.error_code.has_value()) {
        result.final_context.working["meta"]["error_code"] =
            tool_result.error_code.value();
      }
      if (tool_result.meta.is_object()) {
        result.final_context.working["meta"] = tool_result.meta;
      }
      if (!result.success) {
        result.failed_phase = "Observing";
      }
    }

    state_ = State::Done;
    return result;
  }

  /**
   * @brief 当前状态 (测试用)
   */
  State state() const { return state_; }

 private:
  std::unique_ptr<agenticdsl::DSLEngine> engine_;
  std::shared_ptr<agenticdsl::IInteractionBus> bus_;
  State state_ = State::Thinking;
};

} // namespace hydraforge::pdk