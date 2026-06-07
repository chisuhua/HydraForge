// agenticdsl/policy/iexecution_policy.h
// 功能描述：执行策略抽象接口。定义 IExecutionPolicy 抽象接口，作为执行模式
//           （Plan/Agent/Yolo 等）的策略对象契约；用于在不修改调用方代码的
//           前提下切换不同的执行行为，包含 IPER 行为控制与舰队并发上限。
// 设计依据：ADR-0031 (IExecutionPolicy) — 包含 ADR-0031 §1 列举的全部 8 个方法
// 作者：AgenticDSL Pre-Phase
// 最后修改日期：2026-06-07
#pragma once

#include <cstddef>
#include <string>

namespace agenticdsl {

// 前置声明（值类型，Phase 3 给出完整定义）：
// - ToolMetadata:    完整定义见 src/common/tools/tool_metadata.h   (ADR-0004 V2)
// - ToolCallContext: 完整定义见 src/common/policy/execution_policy.h (ADR-0031 §1)
// 此处以前向声明方式引用，Pre-Phase 编译独立通过；Phase 3 实现时由具体策略类
// 通过 #include 引入完整定义。切勿在此头文件中 #include 它们的完整定义，
// 否则将破坏 Pre-Phase 的零依赖契约。
struct ToolMetadata;
struct ToolCallContext;

/**
 * @brief 执行策略抽象接口
 *
 * 实现类（如 PlanModePolicy / AgentModePolicy / YoloModePolicy — Phase 3 交付）
 * 通过纯虚方法向调用方提供执行决策：是否需要人工审批、是否自动执行、
 * 是否展示计划与结果、IPER 行为控制以及舰队模式并发上限。
 *
 * 约定：
 * - 所有方法均为 const 查询，无副作用（策略是不可变状态对象）
 * - 调用方可在任意线程读取同一策略实例
 * - 策略由 DI 容器在 CognitiveWorker 初始化时注入，运行期可热切换
 */
class IExecutionPolicy {
 public:
  virtual ~IExecutionPolicy() = default;

  // ===== 核心决策 =====

  /**
   * @brief 判断工具调用是否需要人工审批
   *
   * @param meta 工具元数据（名称、风险等级、所属域等）
   * @param ctx  工具调用上下文（用户、Session、资源配额等）
   * @return true 表示需要等待用户审批；false 表示可自动放行
   *
   * 典型实现：Plan 模式对高风险工具返回 true；Yolo 模式对所有工具返回 false；
   * Agent 模式依据 meta.risk_level 与 ctx.user_trust 综合判定。
   */
  virtual bool requires_approval(const ToolMetadata& meta,
                                 const ToolCallContext& ctx) const = 0;

  // ===== 阶段流程控制 =====

  /**
   * @brief 当前模式是否自动执行（无需用户确认每一步）
   * @return true 自动执行；false 等待用户显式放行
   */
  virtual bool should_auto_execute() const = 0;

  /**
   * @brief 执行前是否向用户展示计划
   * @return true 展示计划；false 跳过
   */
  virtual bool should_show_plan() const = 0;

  /**
   * @brief 执行后是否向用户展示结果摘要
   * @return true 展示摘要；false 跳过
   */
  virtual bool should_show_result_summary() const = 0;

  /**
   * @brief 模式名称（用于日志、UI 展示与策略工厂分发）
   * @return 模式标识字符串，如 "plan" / "agent" / "yolo"
   */
  virtual std::string mode_name() const = 0;

  // ===== IPER 行为控制（ADR-0031 附录：议题 5 最小集成） =====

  /**
   * @brief 是否自动决策重试（Reflect 阶段产物）
   * @return true 自动重试失败步骤；false 等待用户确认
   */
  virtual bool should_auto_decide_retry() const = 0;

  /**
   * @brief 是否向用户展示反思内容（Reflect 阶段）
   * @return true 展示反思文本；false 仅内部记录
   */
  virtual bool should_show_reflection() const = 0;

  // ===== 舰队模式 =====

  /**
   * @brief 舰队模式最大并发度
   * @return 同时在飞的子任务上限；0 表示串行（无并发）；size_t 最大值为无并发限制
   *
   * 用于 CognitiveWorker 在调度多个 SubtaskSession 时控制并发度。
   * 单任务模式可固定返回 1；舰队模式依据硬件资源返回合理上限。
   */
  virtual size_t fleet_max_concurrency() const = 0;
};

}  // namespace agenticdsl
