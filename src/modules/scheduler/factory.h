// modules/scheduler/include/scheduler/factory.h
// C1 Day 8 (2026-06-27): TopoScheduler 工厂 — 抽象化调度器构造路径
//   配合 ADR-0019 §1.4: 减少 engine.cpp 对具体 TopoScheduler 类型的依赖
//   调用: `auto scheduler = agenticdsl::scheduler::create(config, *tools, provider.get(), &graphs);`
// 替代 engine.cpp:130-133 的直接构造
#ifndef AGENTICDSL_MODULES_SCHEDULER_FACTORY_H
#define AGENTICDSL_MODULES_SCHEDULER_FACTORY_H

#include "core/types/budget.h"  // C1 Day 8: std::optional<ExecutionBudget> 需要完整类型
#include <memory>
#include <optional>
#include <vector>

namespace agenticdsl {

class IScheduler;
class IToolRegistry;
class ILLMProvider;
struct ParsedGraph;
class ExecutionBudget;
// Sprint 19 (OpenSpec pimpl-node-executor-h): 改为 IApprovalHandler 抽象 (替代 ApprovalHandler)
class IApprovalHandler; // Sprint 19: 前向声明
class ToolCoordinator; // C4 Sprint 14 (ADR-0031 P3-P4): 前向声明

namespace scheduler {

struct SchedulerConfig {
    std::optional<ExecutionBudget> initial_budget;
    // Sprint 19: 改用 IApprovalHandler 抽象 (依赖倒置, ADR-0019 §1.4)
    IApprovalHandler* approval_handler{nullptr}; // ADR-0031 (2026-07-31): 审批处理器
    ToolCoordinator* tool_coordinator{nullptr}; // C4 Sprint 14 (ADR-0031 P3-P4): ToolCoordinator
};

std::unique_ptr<IScheduler> create(
    SchedulerConfig&& config,
    IToolRegistry& tools,
    ILLMProvider* provider,
    const std::vector<ParsedGraph>* dynamic_graphs = nullptr);

} // namespace scheduler
} // namespace agenticdsl

#endif // AGENTICDSL_MODULES_SCHEDULER_FACTORY_H
