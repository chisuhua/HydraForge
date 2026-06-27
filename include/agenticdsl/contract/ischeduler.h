// include/agenticdsl/contract/ischeduler.h
// 文件头注释
// 功能描述：DAG 调度器抽象接口（ADR-0019 §1.4 + plan Stage 4）。
//          Phase 1 单实例多 Agent 调度；Phase 5 AsyncRuntime 双层替换为协程实现。
// 设计依据：ADR-0019 §1.4（跨模块耦合识别）+ ADR-0020（线程模型）+ plan §Task 16。
// 作者：AgenticDSL Stage 4
// 最后修改日期：2026-06-12
#pragma once

#include "core/types/common.h"  // ExecutionResult, Node, ParsedGraph, Context
#include "agenticdsl/types/trace_record.h"

#include <memory>
#include <vector>

namespace agenticdsl {

/**
 * @brief DAG 调度器抽象接口
 *
 * 引擎只依赖此接口，不依赖具体 TopoScheduler 类。
 *
 * - MVP 实现: agenticdsl::contract::TopoScheduler
 *   （位置：include/agenticdsl/contract/toposcheduler.h，由 Task 17 引入；
 *    原 modules/scheduler/topo_scheduler.h 重命名/迁移在后续 Task 完成）
 * - Phase 5 替换: AsyncRuntime（Taskflow + async_simple 双层，ADR-0020）
 *
 * execute() 参数使用 flat `Context`（nlohmann::json 别名）以保持与现有
 * TopoScheduler::execute(Context) 签名兼容；Stage 4 后续 Task 19/20 在
 * 调用方完成向 LayeredContext 的平滑迁移（参考 ADR-0008）。
 */
class IScheduler {
 public:
  virtual ~IScheduler() = default;

  /**
   * @brief 注册一个节点到调度器（生命周期由调度器托管）
   */
  virtual void register_node(std::unique_ptr<Node> node) = 0;

  /**
   * @brief 构建依赖图（必须在 register_node 之后、execute 之前调用）
   */
  virtual void build_dag() = 0;

  /**
   * @brief 动态追加子图（由 GENERATE_SUBGRAPH 节点在执行期间触发）
   */
  virtual void append_dynamic_graphs(std::vector<ParsedGraph> new_graphs) = 0;

  /**
   * @brief 执行一次 DAG 调度
   * @param initial_context 入口上下文（flat nlohmann::json）
   * @return 执行结果（成功/失败 + 最终上下文 + 可选 paused_at）
   */
  virtual ExecutionResult execute(const Context& initial_context) = 0;

  /**
   * @brief 获取最近一次执行的 trace 记录列表
   * C1 Day 16 (2026-06-27): 添加到接口以避免 engine.cpp 需要 dynamic_cast
   */
  virtual std::vector<TraceRecord> get_last_traces() const = 0;
};

} // namespace agenticdsl