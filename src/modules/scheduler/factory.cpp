// modules/scheduler/factory.cpp
// C1 Day 8 (2026-06-27): TopoScheduler 工厂实现
#include "factory.h"
#include "topo_scheduler.h" // 引入完整 TopoScheduler 类型
#include "core/types/budget.h" // 引入 ExecutionBudget 完整类型
#include "agenticdsl/contract/ischeduler.h"
#include "agenticdsl/contract/itool_registry.h"
#include "modules/parser/markdown_parser.h" // 引入 ParsedGraph
#include <taskflow/taskflow.hpp> // C2 Day 1-2: factory.cpp 创建 unique_ptr<TopoScheduler>, 销毁时需 tf::Taskflow 完整类型
// Sprint 19 D-8: PIMPL-lite — execution_session.h 不再拖入 ResourceManager, factory.cpp
// 创建 unique_ptr<TopoScheduler> 销毁时需 ResourceManager 完整类型 (Sprint 17 C.4 同模式)
#include "modules/scheduler/resource_manager.h"
// Sprint 19 D-8: PIMPL-lite — get_last_traces() 内联函数 (topo_scheduler.h:62) 调用 TraceExporter::get_traces,
// factory.cpp 实例化该内联函数需 TraceExporter 完整类型
#include "modules/trace/trace_exporter.h"

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
    ts_config.approval_handler = config.approval_handler;
    ts_config.tool_coordinator = config.tool_coordinator;
    return std::make_unique<TopoScheduler>(
        std::move(ts_config),
        tools,
        provider,
        dynamic_graphs);
}

} // namespace scheduler
} // namespace agenticdsl
