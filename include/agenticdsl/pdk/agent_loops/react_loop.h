// include/agenticdsl/pdk/agent_loops/react_loop.h
// 文件头注释
// 功能描述：ReactLoop — PDK Agent 循环实现 (Sprint 4 已 ship 模式升级,
//          ADR-0089 v1.3 amendment 2026-10-09 薄壳化)。
//          单轮: Plan 风格 LLM 生成 (Plan/Act 简化版, per ADR-0089 v1.3 D2)。
//          统一通过 LoopResult 接口返回, 与 PlanExecuteLoop / ForkJoinLoop 共享返回类型。
//          Sprint 20 升级: 之前 DEFINE_AGENT 宏内联此逻辑, 本次提取为独立 class
//          供 LoopDispatcher 模板分发使用 (ADR-0021 §3.2)。
// 设计依据：ADR-0021 §3.2 + ADR-0089 v1.3 (D2.inv.api: ReactLoop 薄壳委托 run_plan_phase)
//          + openspec/changes/pdk-plan-execute-fork-join + openspec/changes/consolidate-loop-phases-to-shared-helpers
// 作者：AgenticDSL Phase 1 Sprint 20 + Sprint 37+ (consolidate-loop-phases)
// 最后修改日期：2026-10-09

#pragma once

#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/pdk/agent_loops/loop_phases.h"
#include "agenticdsl/pdk/agent_loops/loop_result.h"
#include "agenticdsl/types/layered_context.h"
#include "core/engine.h"
#include "core/types/tool_result.h"

#include <memory>
#include <string>

namespace hydraforge::pdk {

/**
 * @brief ReactLoop — PDK Agent 单轮循环 (Sprint 4 MVP, Sprint 20 升级为 class,
 *        ADR-0089 v1.3 薄壳化委托 run_plan_phase)
 *
 * 状态机:
 *   Thinking → Acting → Observing → Done (单轮即 Done, 不循环)
 *
 * 行为契约:
 *   - run() 内部委托 loop_phases::run_plan_phase (Plan/Act 简化版, 单轮)
 *   - LLM 生成文本写入 final_context.working.data[agent_output]
 *   - success 字段映射: 生成非空 → success = true
 *   - 引擎或 LLM provider 为 null 时返回 success=false + message 描述原因
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
LoopResult run(const std::string& prompt, const agenticdsl::LayeredContext& ctx,
               std::stop_token token = {}) {
     LoopResult result;
     result.final_context = ctx;
     result.total_steps = 1;

     // Phase B Step 4: 取消 token 检查 — early exit
     if (token.stop_requested()) {
       result.success = false;
       result.message = "ReactLoop: cancelled before execution";
       result.failed_phase = "Thinking";
       state_ = State::Done;
       return result;
     }

     if (!engine_) {
      result.success = false;
      result.message = "ReactLoop: DSLEngine is null";
      result.failed_phase = "Thinking";
      state_ = State::Done;
      return result;
    }

    agenticdsl::ILLMProvider* llm = engine_->get_llm_provider();
    if (!llm) {
      result.success = false;
      result.message = "ReactLoop: LLM provider is null";
      result.failed_phase = "Thinking";
      state_ = State::Done;
      return result;
    }

    state_ = State::Thinking;
    // 薄壳委托: 单轮 Plan 风格 LLM 生成 (Plan/Act 简化版, per ADR-0089 v1.3 D2).
    // 单源 phase 逻辑在 loop_phases::run_plan_phase (含 req.params.model.clear()
    // NOT redundant 注释同步在 loop_phases.h:49-57). 公开 API 零变化 (D2.inv.api).
    state_ = State::Acting;
    std::optional<std::string> plan_output =
        loop_phases::run_plan_phase(*llm, prompt, ctx, token);

    state_ = State::Observing;
    // 保持 final_context.working["data"] 恒为 object (test_pdk_macros L135 断言
    // working["data"].is_object() 契约 + PlanExecuteLoop run() 同款初始化)
    result.final_context.working["data"] = nlohmann::json::object();
    // 同上: working["meta"] 恒为 object (test_pdk_macros L136 断言契约)
    result.final_context.working["meta"] = nlohmann::json::object();
    if (!plan_output.has_value() || plan_output->empty()) {
      result.success = false;
      result.message = "React loop failed";
      result.failed_phase = "Observing";
    } else {
      result.success = true;
      result.message = "React loop completed";
      result.final_context.working["data"]["agent_output"] = *plan_output;
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