// tests/test_execute_parallel.cpp
// C2 Day 4-5 (2026-06-27, Sprint 12 P1)
#include "catch_amalgamated.hpp"

#include "modules/scheduler/topo_scheduler.h"
#include "modules/scheduler/resource_manager.h" // Sprint 19 D-8: PIMPL-lite — TopoScheduler 持有 unique_ptr<ResourceManager>, test 销毁时需完整类型
#include "common/tools/registry.h"
#include "common/llm/mock_provider.h"
#include "agenticdsl/contract/itool_registry.h"
#include "core/types/context.h"
#include "core/types/node.h"
#include <taskflow/taskflow.hpp>  // C2 Day 4-5: test 创建 unique_ptr<TopoScheduler>, 销毁时需 tf::Executor/tf::Taskflow 完整类型

#include <atomic>
#include <chrono>
#include <thread>

using namespace agenticdsl;
using namespace std::chrono_literals;

TEST_CASE("execute_parallel runs all registered nodes",
          "[scheduler][c2-day4]") {
    ToolRegistry tools;
    std::atomic<int> counter{0};
    tools.register_tool_function("noop", agenticdsl::ToolMetadata{"noop", "test", "test", agenticdsl::ToolCategory::ReadOnly, agenticdsl::LayerProfile::Workflow}, [&](const auto&) -> nlohmann::json {
        counter.fetch_add(1);
        return {{"ok", true}};
    });

    TopoScheduler::Config config;
    TopoScheduler scheduler(std::move(config), tools, nullptr, nullptr);

    scheduler.register_node(std::make_unique<ToolCallNode>(
        "/a", "noop", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));
    scheduler.register_node(std::make_unique<ToolCallNode>(
        "/b", "noop", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));
    scheduler.register_node(std::make_unique<ToolCallNode>(
        "/c", "noop", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));

    Context ctx;
    auto result = scheduler.execute_parallel(ctx);
    REQUIRE(result.success);
    REQUIRE(counter.load() == 3);
}

TEST_CASE("execute_parallel dispatches independent nodes concurrently",
          "[scheduler][c2-day4]") {
    ToolRegistry tools;
    std::atomic<int> in_flight{0};
    std::atomic<int> max_concurrent{0};

    auto register_work = [&](std::string name) {
        tools.register_tool_function(name, agenticdsl::ToolMetadata{name, "test", "test", agenticdsl::ToolCategory::ReadOnly, agenticdsl::LayerProfile::Workflow}, [name, &in_flight, &max_concurrent](const auto&) -> nlohmann::json {
            int current = in_flight.fetch_add(1) + 1;
            int prev_max = max_concurrent.load();
            while (current > prev_max &&
                   !max_concurrent.compare_exchange_weak(prev_max, current)) {}
            std::this_thread::sleep_for(50ms);
            in_flight.fetch_sub(1);
            return {{"name", name}};
        });
    };
    register_work("node_a");
    register_work("node_b");
    register_work("node_c");

    TopoScheduler::Config config;
    TopoScheduler scheduler(std::move(config), tools, nullptr, nullptr);

    scheduler.register_node(std::make_unique<ToolCallNode>(
        "/a", "node_a", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));
    scheduler.register_node(std::make_unique<ToolCallNode>(
        "/b", "node_b", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));
    scheduler.register_node(std::make_unique<ToolCallNode>(
        "/c", "node_c", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));

    Context ctx;
    auto result = scheduler.execute_parallel(ctx);
    REQUIRE(result.success);
    REQUIRE(max_concurrent.load() >= 2);
}

TEST_CASE("execute_parallel returns success on empty DAG",
          "[scheduler][c2-day4]") {
    ToolRegistry tools;
    TopoScheduler::Config config;
    TopoScheduler scheduler(std::move(config), tools, nullptr, nullptr);

    Context ctx;
    auto result = scheduler.execute_parallel(ctx);
    REQUIRE(result.success);
}