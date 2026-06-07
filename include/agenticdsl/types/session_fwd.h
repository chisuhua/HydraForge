// agenticdsl/types/session_fwd.h
// 功能描述：Session 三级体系前向声明头文件。仅包含 UserSession、TaskSession、
//           SubtaskSession 的前向声明，供跨模块引用时降低编译耦合；完整定义
//           在 ADR-0033 实施时给出。
// 设计依据：ADR-0014 (对话上下文隔离) + ADR-0033 (Session 层级体系)
// 作者：AgenticDSL Pre-Phase
// 最后修改日期：2026-06-07
#pragma once

namespace agenticdsl {

/**
 * @brief 用户级会话（前向声明）
 *
 * 跨任务保持的长期会话对象，承担用户级长期记忆归属、跨 TaskSession 的
 * 偏好与历史聚合。生命周期独立于单次任务，由 SessionManager 管理。
 *
 * 完整定义在 ADR-0033 实施时给出。
 */
class UserSession;

/**
 * @brief 任务级会话（前向声明）
 *
 * 单次任务范围内的会话对象，由 CognitiveWorker 在一次任务执行期间持有；
 * 任务结束可归档至 UserSession 关联的长期存储。包含任务级上下文与
 * TaskPlan 等中间产物。
 *
 * 完整定义在 ADR-0033 实施时给出。
 */
class TaskSession;

/**
 * @brief 子任务级会话（前向声明）
 *
 * 单步执行范围内的会话对象，覆盖一次 tool_call 或一次 ReAct 迭代；
 * 包含该步的输入、工具调用记录、反思（Reflect）产物。是 TraceRecord
 * 的主要关联对象。
 *
 * 完整定义在 ADR-0033 实施时给出。
 */
class SubtaskSession;

}  // namespace agenticdsl
