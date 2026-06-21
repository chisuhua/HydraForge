// src/modules/scheduler/factory.h
#pragma once

#include "agenticdsl/contract/ischeduler.h"
#include "common/llm/llm_types.h"

#include <memory>
#include <vector>

namespace agenticdsl {
class IToolRegistry;
struct ParsedGraph;
}

namespace agenticdsl::scheduler {

std::unique_ptr<IScheduler> create(
    IToolRegistry& tool_registry,
    ILLMProvider* llm_provider = nullptr,
    const std::vector<ParsedGraph>* full_graphs = nullptr);

}  // namespace agenticdsl::scheduler

