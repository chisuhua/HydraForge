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

// ============================================================================
// tf-integration-coverage (c2-coverage): 5 contract verification cases
// ============================================================================

TEST_CASE("execute_parallel dispatches 5-node linear chain in topo order",
          "[scheduler][c2-coverage]") {
    ToolRegistry tools;
    std::atomic<int> last_completed{0};
    auto register_node_tool = [&](const char* n) {
        std::string name(n);
        tools.register_tool_function(name, agenticdsl::ToolMetadata{name, "test", "test", agenticdsl::ToolCategory::ReadOnly, agenticdsl::LayerProfile::Workflow}, [name, &last_completed](const auto&) -> nlohmann::json {
            int expected = (name[0] - 'a') + 1;
            int prev = last_completed.load();
            REQUIRE(prev == expected - 1);
            last_completed.store(expected);
            return {{"ok", true}};
        });
    };
    register_node_tool("a");
    register_node_tool("b");
    register_node_tool("c");
    register_node_tool("d");
    register_node_tool("e");

    TopoScheduler::Config config;
    TopoScheduler scheduler(std::move(config), tools, nullptr, nullptr);
    scheduler.register_node(std::make_unique<ToolCallNode>("/a", "a", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));
    scheduler.register_node(std::make_unique<ToolCallNode>("/b", "b", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{"/a"}));
    scheduler.register_node(std::make_unique<ToolCallNode>("/c", "c", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{"/b"}));
    scheduler.register_node(std::make_unique<ToolCallNode>("/d", "d", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{"/c"}));
    scheduler.register_node(std::make_unique<ToolCallNode>("/e", "e", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{"/d"}));

    Context ctx;
    auto result = scheduler.execute_parallel(ctx);
    REQUIRE(result.success);
    REQUIRE(last_completed.load() == 5);
}

TEST_CASE("execute_parallel reuses parallel_executor_ across calls",
          "[scheduler][c2-coverage]") {
    ToolRegistry tools;
    tools.register_tool_function("noop", agenticdsl::ToolMetadata{"noop", "test", "test", agenticdsl::ToolCategory::ReadOnly, agenticdsl::LayerProfile::Workflow}, [](const auto&) -> nlohmann::json { return {{"ok", true}}; });

    TopoScheduler::Config config;
    TopoScheduler scheduler(std::move(config), tools, nullptr, nullptr);
    scheduler.register_node(std::make_unique<ToolCallNode>("/a", "noop", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));
    scheduler.register_node(std::make_unique<ToolCallNode>("/b", "noop", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));
    scheduler.register_node(std::make_unique<ToolCallNode>("/c", "noop", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));

    Context ctx1;
    REQUIRE(scheduler.execute_parallel(ctx1).success);
    const void* first_addr = scheduler.get_parallel_executor_address_for_test();
    REQUIRE(first_addr != nullptr);

    scheduler.register_node(std::make_unique<ToolCallNode>("/d", "noop", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));
    scheduler.register_node(std::make_unique<ToolCallNode>("/e", "noop", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));
    Context ctx2;
    REQUIRE(scheduler.execute_parallel(ctx2).success);
    const void* second_addr = scheduler.get_parallel_executor_address_for_test();
    REQUIRE(second_addr == first_addr);
}

TEST_CASE("execute_parallel swallows tool exception and reports failure",
          "[scheduler][c2-coverage]") {
    ToolRegistry tools;
    std::atomic<int> good_count{0};
    tools.register_tool_function("good", agenticdsl::ToolMetadata{"good", "test", "test", agenticdsl::ToolCategory::ReadOnly, agenticdsl::LayerProfile::Workflow}, [&good_count](const auto&) -> nlohmann::json {
        good_count.fetch_add(1);
        return {{"ok", true}};
    });
    tools.register_tool_function("boom", agenticdsl::ToolMetadata{"boom", "test", "test", agenticdsl::ToolCategory::ReadOnly, agenticdsl::LayerProfile::Workflow}, [](const auto&) -> nlohmann::json {
        throw std::runtime_error("boom");
    });

    TopoScheduler::Config config;
    TopoScheduler scheduler(std::move(config), tools, nullptr, nullptr);
    scheduler.register_node(std::make_unique<ToolCallNode>("/g1", "good", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));
    scheduler.register_node(std::make_unique<ToolCallNode>("/b1", "boom", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));
    scheduler.register_node(std::make_unique<ToolCallNode>("/g2", "good", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));

    Context ctx;
    auto result = scheduler.execute_parallel(ctx);
    REQUIRE(good_count.load() == 2);
    REQUIRE_FALSE(result.success);
}

TEST_CASE("execute_parallel handles 6 independent ToolCallNodes",
          "[scheduler][c2-coverage]") {
    ToolRegistry tools;
    std::atomic<int> counter{0};
    for (const char* n : {"a", "b", "c", "d", "e", "f"}) {
        std::string name(n);
        tools.register_tool_function(name, agenticdsl::ToolMetadata{name, "test", "test", agenticdsl::ToolCategory::ReadOnly, agenticdsl::LayerProfile::Workflow}, [&counter, name](const auto&) -> nlohmann::json {
            counter.fetch_add(1);
            return {{"name", name}};
        });
    }
    TopoScheduler::Config config;
    TopoScheduler scheduler(std::move(config), tools, nullptr, nullptr);
    for (const char* n : {"a", "b", "c", "d", "e", "f"}) {
        std::string name(n);
        scheduler.register_node(std::make_unique<ToolCallNode>("/" + name, name, std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));
    }
    Context ctx;
    auto result = scheduler.execute_parallel(ctx);
    REQUIRE(result.success);
    REQUIRE(counter.load() == 6);
}

TEST_CASE("execute_parallel respects Config::num_workers injection",
          "[scheduler][c2-coverage]") {
    ToolRegistry tools;
    std::atomic<int> in_flight{0};
    std::atomic<int> max_concurrent{0};
    for (const char* n : {"n1", "n2", "n3", "n4"}) {
        std::string name(n);
        tools.register_tool_function(name, agenticdsl::ToolMetadata{name, "test", "test", agenticdsl::ToolCategory::ReadOnly, agenticdsl::LayerProfile::Workflow}, [&in_flight, &max_concurrent](const auto&) -> nlohmann::json {
            int cur = in_flight.fetch_add(1) + 1;
            int prev = max_concurrent.load();
            while (cur > prev && !max_concurrent.compare_exchange_weak(prev, cur)) {}
            std::this_thread::sleep_for(50ms);
            in_flight.fetch_sub(1);
            return {{"ok", true}};
        });
    }
    TopoScheduler::Config config;
    config.num_workers = 2;
    TopoScheduler scheduler(std::move(config), tools, nullptr, nullptr);
    for (const char* n : {"n1", "n2", "n3", "n4"}) {
        std::string name(n);
        scheduler.register_node(std::make_unique<ToolCallNode>("/" + name, name, std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));
    }
    Context ctx;
    auto result = scheduler.execute_parallel(ctx);
    REQUIRE(result.success);
    REQUIRE(max_concurrent.load() <= 2);
}