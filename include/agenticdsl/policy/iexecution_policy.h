// agenticdsl/policy/iexecution_policy.h
// 功能描述：执行策略抽象接口。定义 IExecutionPolicy抽象接口，作为执行模式
// （Plan/Agent/Yolo 等）的策略对象契约；用于在不修改调用方代码的
//前提下切换不同的执行行为，包含 IPER行为控制与舰队并发上限。
//配套值类型（ToolMetadata / ToolCallContext / ToolCategory /
// ApprovalPolicy / LayerProfile）的规范定义位于
// src/common/policy/execution_policy.h，本头文件通过 #include引入。
// 设计依据：ADR-0031 (IExecutionPolicy) —包含 ADR-0031 §1列举的全部8 个方法
// 作者：AgenticDSL Pre-Phase
// 最后修改日期：2026-06-10
#pragma once

#include <cstddef>
#include <string>

//引入值类型规范定义（ADR-0031 §1 / ADR-0004 §6-9）
// 注意：本头文件仅作为公开契约入口；值类型的 canonical 定义在 src/common/policy
#include "common/policy/execution_policy.h"

namespace agenticdsl {

/**
 * @brief 执行策略抽象接口
 *
 * 实现类（如 PlanModePolicy / AgentModePolicy / YoloModePolicy — Phase3交付）
 * 通过纯虚方法向调用方提供执行决策：是否需要人工审批、是否自动执行、
 * 是否展示计划与结果、IPER行为控制以及舰队模式并发上限。
 *
 *约定：
 * - 所有方法均为 const 查询，无副作用（策略是不可变状态对象）
 * - 调用方可在任意线程读取同一策略实例
 * -策略由 DI容器在 CognitiveWorker初始化时注入，运行期可热切换
 */
class IExecutionPolicy {
 public:
 virtual ~IExecutionPolicy() = default;

 // =====核心决策 =====

 /**
 * @brief 判断工具调用是否需要人工审批
 *
 * @param meta工具元数据（名称、风险等级、所属域等）
 * @param ctx工具调用上下文（用户、Session、资源配额等）
 * @return true 表示需要等待用户审批；false 表示可自动放行
 *
 *典型实现：Plan模式对高风险工具返回 true；Yolo模式对所有工具返回 false；
 * Agent模式依据 meta.risk_level 与 ctx.user_trust 综合判定。
 */
 virtual bool requires_approval(const ToolMetadata& meta,
 const ToolCallContext& ctx) const =0;

 // =====阶段流程控制 =====

 /**
 * @brief 当前模式是否自动执行（无需用户确认每一步）
 * @return true 自动执行；false等待用户显式放行
 */
 virtual bool should_auto_execute() const =0;

 /**
 * @brief 执行前是否向用户展示计划
 * @return true展示计划；false跳过
 */
 virtual bool should_show_plan() const =0;

 /**
 * @brief 执行后是否向用户展示结果摘要
 * @return true展示摘要；false跳过
 */
 virtual bool should_show_result_summary() const =0;

 /**
 * @brief模式名称（用于日志、UI展示与策略工厂分发）
 * @return模式标识字符串，如 "plan" / "agent" / "yolo"
 */
 virtual std::string mode_name() const =0;

 // ===== IPER行为控制（ADR-0031附录：议题5最小集成） =====

 /**
 * @brief 是否自动决策重试（Reflect阶段产物）
 * @return true 自动重试失败步骤；false等待用户确认
 */
 virtual bool should_auto_decide_retry() const =0;

 /**
 * @brief 是否向用户展示反思内容（Reflect阶段）
 * @return true展示反思文本；false 仅内部记录
 */
 virtual bool should_show_reflection() const =0;

 // =====舰队模式 =====

 /**
 * @brief舰队模式最大并发度
 * @return 同时在飞的子任务上限；0 表示串行（无并发）；size_t 最大值为无并发限制
 *
 * 用于 CognitiveWorker 在调度多个 SubtaskSession 时控制并发度。
 * 单任务模式可固定返回1；舰队模式依据硬件资源返回合理上限。
 */
 virtual size_t fleet_max_concurrency() const =0;
};

} // namespace agenticdsl
