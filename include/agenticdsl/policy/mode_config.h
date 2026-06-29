// include/agenticdsl/policy/mode_config.h
// 功能描述：ModeConfig 值结构体 — 三模式 (Plan/Agent/Yolo) 静态配置常量。
// 设计依据：ADR-0031 §决策 1 (Oracle 推荐, session ses_0faa4dabeffeHGFoLdXE7AqwH7)
// 作者：AgenticDSL Phase3 / Sprint 13 C3 ship
// 最后修改日期：2026-07-31
#pragma once

#include <cstddef>
#include <string>

namespace agenticdsl {

/**
 * @brief 三模式静态配置常量 (值类型，非虚接口)
 *
 * 从 IExecutionPolicy stub 8 方法移出 6 个常量 (per-mode 常量不属于虚接口契约).
 * 由 PlanModeConfig / AgentModeConfig / YoloModeConfig 三个 static constexpr 提供.
 * 实施依据: ADR-0031 §决策 1 (Oracle 2026-06-26 决议).
 */
struct ModeConfig {
  bool show_plan;                  // 执行前展示计划
  bool show_result_summary;        // 执行后展示结果摘要
  bool auto_decide_retry;          // IPER Reflect 后自动决策重试
  bool show_reflection;            // 展示反思内容
  std::size_t fleet_max_concurrency;  // 舰队模式最大并发度
  std::string mode_name;           // 模式标识 (log / UI / factory 分发)
};

/**
 * @brief Plan 模式: 保守模式 — 所有非 ReadOnly 需审批, 展示计划与结果, 不自动重试
 */
inline constexpr ModeConfig PlanModeConfig{
  /*.show_plan=*/true,
  /*.show_result_summary=*/true,
  /*.auto_decide_retry=*/false,
  /*.show_reflection=*/true,
  /*.fleet_max_concurrency=*/8,
  /*.mode_name=*/"plan"
};

/**
 * @brief Agent 模式: 平衡模式 — 遵循工具 ApprovalPolicy, 自动重试, 不展示反思
 */
inline constexpr ModeConfig AgentModeConfig{
  /*.show_plan=*/false,
  /*.show_result_summary=*/true,
  /*.auto_decide_retry=*/true,
  /*.show_reflection=*/false,
  /*.fleet_max_concurrency=*/16,
  /*.mode_name=*/"agent"
};

/**
 * @brief YOLO 模式: 高吞吐 — 仅 force_approval_always 需审批, 不展示计划与结果
 */
inline constexpr ModeConfig YoloModeConfig{
  /*.show_plan=*/false,
  /*.show_result_summary=*/false,
  /*.auto_decide_retry=*/true,
  /*.show_reflection=*/false,
  /*.fleet_max_concurrency=*/32,
  /*.mode_name=*/"yolo"
};

}  // namespace agenticdsl
