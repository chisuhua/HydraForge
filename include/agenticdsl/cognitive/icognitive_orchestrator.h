// agenticdsl/cognitive/icognitive_orchestrator.h
// 功能描述：认知编排器接口（ReAct 入口）。定义 ICognitiveOrchestrator 抽象接口，
//           作为 CognitiveWorker 阶段的统一入口；具体编排逻辑（IPER 闭环、ReAct
//           循环、IInteractionBus 集成）在后续 Phase 给出实现。
// 设计依据：ADR-0019 (IInteractionBus) + ADR-0015 (IPER 闭环) + roadmap P0.1
// 作者：AgenticDSL Pre-Phase
// 最后修改日期：2026-06-07
#pragma once

#include <functional>
#include <string>

namespace agenticdsl {

// 前置声明：ExecutionResult 在 Phase 0 Track 0.2 实施时由 src/core/types/budget.h
// 提供完整定义；此处使用前向声明以保持本头文件可独立包含、避免不必要的传递依赖。
struct ExecutionResult;

/**
 * @brief 认知编排器抽象接口（ReAct 循环入口）
 *
 * 实现类（如 CognitiveOrchestrator）负责驱动一次会话的 ReAct 主循环：
 * 感知（Perceive）→ 推理（Reason）→ 行动（Execute）→ 反思（Reflect），
 * 并通过 IInteractionBus（ADR-0019）进行人机交互、通过 IExecutionPolicy
 * （ADR-0031）查询执行模式决策。
 *
 * 该接口为后续 Phase 的实现提供稳定契约；Phase 2 协程化后此方法签名
 * 保持不变（仍为回调式），由 NodeExecutor 的协程包装器进行适配。
 */
class ICognitiveOrchestrator {
 public:
  virtual ~ICognitiveOrchestrator() = default;

  /**
   * @brief 处理一次会话请求（ReAct 入口）
   *
   * @param session_id   会话唯一标识，用于跨模块关联 SubtaskSession/TaskSession
   *                     以及 IInteractionBus 的事件路由
   * @param on_complete  处理完成时的回调；执行结果（成功或失败）通过
   *                     ExecutionResult 传递
   *
   * 语义契约：
   * - 实现必须保证 on_complete 被调用**至少一次**（成功路径或失败路径）
   * - 多次回调（如阶段性进度汇报）由实现决定，但最终必须有一次"终态"回调
   * - 多线程并发调用的线程安全性由实现决定（参考 ADR-0003）
   * - 该方法在 Phase 2 协程化后签名不变；NodeExecutor 通过协程适配
   *
   * 异常安全：若实现内部抛出未捕获异常，必须在调用 on_complete 时
   * 通过 ExecutionResult 表达失败状态，而非让异常逃逸至调用方。
   */
  virtual void process(const std::string& session_id,
                       std::function<void(ExecutionResult)> on_complete) = 0;
};

}  // namespace agenticdsl
