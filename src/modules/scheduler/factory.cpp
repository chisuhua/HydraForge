// modules/scheduler/factory.cpp
// C1 Day 8 (2026-06-27): TopoScheduler 工厂实现
#include "factory.h"
#include "topo_scheduler.h" // 引入完整 TopoScheduler 类型
#include "core/types/budget.h" // 引入 ExecutionBudget 完整类型
#include "agenticdsl/contract/ischeduler.h"
#include "agenticdsl/contract/itool_registry.h"
#include "modules/parser/markdown_parser.h" // 引入 ParsedGraph

namespace agenticdsl {
namespace scheduler {

std::unique_ptr<IScheduler> create(
    SchedulerConfig&& config,
    IToolRegistry& tools,
    ILLMProvider* provider,
    const std::vector<ParsedGraph>* dynamic_graphs)
{
    TopoScheduler::Config ts_config;
    ts_config.initial_budget = std::move(config.initial_budget);
    return std::make_unique<TopoScheduler>(
        std::move(ts_config),
        tools,
        provider,
        dynamic_graphs);
}

} // namespace scheduler
} // namespace agenticdsl
