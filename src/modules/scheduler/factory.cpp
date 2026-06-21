// src/modules/scheduler/factory.cpp
// Sprint 6 P2-7: TopoScheduler 工厂实现
#include "scheduler/factory.h"
#include "scheduler/topo_scheduler.h"
#include "modules/parser/markdown_parser.h"
#include "agenticdsl/contract/itool_registry.h"

namespace agenticdsl::scheduler {

std::unique_ptr<IScheduler> create(
    IToolRegistry& tool_registry,
    ILLMProvider* llm_provider,
    const std::vector<ParsedGraph>* full_graphs) {
  TopoScheduler::Config config;
  return std::make_unique<TopoScheduler>(std::move(config), tool_registry,
                                        llm_provider, full_graphs);
}

}  // namespace agenticdsl::scheduler

