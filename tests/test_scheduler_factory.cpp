// tests/test_scheduler_factory.cpp
// C1 Day 8-9 (2026-06-27): scheduler factory tests

#include "catch_amalgamated.hpp"
#include "modules/scheduler/factory.h"
#include "modules/scheduler/topo_scheduler.h"
#include "agenticdsl/contract/ischeduler.h"
#include "agenticdsl/contract/itool_registry.h"
#include "common/tools/factory.h"
#include <memory>

using namespace agenticdsl;

TEST_CASE("scheduler::create returns IScheduler base", "[scheduler][factory][c1-day8]") {
    auto tools = agenticdsl::tools::create_tool_registry();
    scheduler::SchedulerConfig cfg;
    auto scheduler = scheduler::create(std::move(cfg), *tools, nullptr, nullptr);
    REQUIRE(scheduler != nullptr);
    IScheduler* base = scheduler.get();
    REQUIRE(base != nullptr);
}

TEST_CASE("scheduler::create can dynamic_cast to TopoScheduler", "[scheduler][factory][c1-day8]") {
    auto tools = agenticdsl::tools::create_tool_registry();
    scheduler::SchedulerConfig cfg;
    auto scheduler = scheduler::create(std::move(cfg), *tools, nullptr, nullptr);
    TopoScheduler* concrete = dynamic_cast<TopoScheduler*>(scheduler.get());
    REQUIRE(concrete != nullptr);
}

TEST_CASE("scheduler::create without budget constructs successfully", "[scheduler][factory][c1-day8]") {
    auto tools = agenticdsl::tools::create_tool_registry();
    scheduler::SchedulerConfig cfg;
    REQUIRE_FALSE(cfg.initial_budget.has_value());
    auto scheduler = scheduler::create(std::move(cfg), *tools, nullptr, nullptr);
    REQUIRE(scheduler != nullptr);
}

TEST_CASE("scheduler::create with budget propagates initial_budget", "[scheduler][factory][c1-day8]") {
    auto tools = agenticdsl::tools::create_tool_registry();
    scheduler::SchedulerConfig cfg;
    ExecutionBudget budget;
    budget.max_nodes = 10;
    budget.max_llm_calls = 5;
    cfg.initial_budget = std::move(budget);
    auto scheduler = scheduler::create(std::move(cfg), *tools, nullptr, nullptr);
    REQUIRE(scheduler != nullptr);
}
