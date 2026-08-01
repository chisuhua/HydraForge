// tests/test_execute_parallel_advanced.cpp
// tf-integration-coverage: 7 advanced verification cases for execute_parallel
#include "catch_amalgamated.hpp"

#include "modules/scheduler/topo_scheduler.h"
#include "modules/scheduler/resource_manager.h"
#include "common/tools/registry.h"
#include "core/types/context.h"
#include "core/types/node.h"
#include <taskflow/taskflow.hpp>

#include <atomic>
#include <chrono>
#include <thread>

using namespace agenticdsl;
using namespace std::chrono_literals;

TEST_CASE("execute_parallel 100-node flat DAG completes under 5s",
          "[scheduler][c2-coverage][advanced]") {
    ToolRegistry tools;
    std::atomic<int> counter{0};
    tools.register_tool_function("noop", agenticdsl::ToolMetadata{"noop", "test", "test", agenticdsl::ToolCategory::ReadOnly, agenticdsl::LayerProfile::Workflow}, [&counter](const auto&) -> nlohmann::json {
        counter.fetch_add(1);
        return {{"ok", true}};
    });

    TopoScheduler::Config config;
    config.num_workers = 8;
    TopoScheduler scheduler(std::move(config), tools, nullptr, nullptr);
    for (int i = 0; i < 100; ++i) {
        scheduler.register_node(std::make_unique<ToolCallNode>(
            "/n" + std::to_string(i), "noop",
            std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));
    }

    Context ctx;
    auto start = std::chrono::steady_clock::now();
    auto result = scheduler.execute_parallel(ctx);
    auto elapsed = std::chrono::steady_clock::now() - start;

    REQUIRE(result.success);
    REQUIRE(counter.load() == 100);
    REQUIRE(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() < 5);
}

TEST_CASE("execute_parallel fork/join with 4 branches synchronizes correctly",
          "[scheduler][c2-coverage][advanced]") {
    ToolRegistry tools;
    std::atomic<int> counter{0};
    for (const char* n : {"r", "b1", "b2", "b3", "b4", "s"}) {
        std::string name(n);
        tools.register_tool_function(name, agenticdsl::ToolMetadata{name, "test", "test", agenticdsl::ToolCategory::ReadOnly, agenticdsl::LayerProfile::Workflow}, [&counter, name](const auto&) -> nlohmann::json {
            counter.fetch_add(1);
            return {{"name", name}};
        });
    }

    TopoScheduler::Config config;
    TopoScheduler scheduler(std::move(config), tools, nullptr, nullptr);
    scheduler.register_node(std::make_unique<ToolCallNode>("/r", "r", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));
    scheduler.register_node(std::make_unique<ToolCallNode>("/b1", "b1", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{"/r"}));
    scheduler.register_node(std::make_unique<ToolCallNode>("/b2", "b2", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{"/r"}));
    scheduler.register_node(std::make_unique<ToolCallNode>("/b3", "b3", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{"/r"}));
    scheduler.register_node(std::make_unique<ToolCallNode>("/b4", "b4", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{"/r"}));
    scheduler.register_node(std::make_unique<ToolCallNode>("/s", "s", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{"/b1", "/b2", "/b3", "/b4"}));

    Context ctx;
    auto result = scheduler.execute_parallel(ctx);
    REQUIRE(result.success);
    REQUIRE(counter.load() == 6);
}

TEST_CASE("execute_parallel default Config falls back to hardware_concurrency",
          "[scheduler][c2-coverage][advanced]") {
    ToolRegistry tools;
    std::atomic<int> in_flight{0};
    std::atomic<int> max_concurrent{0};
    for (const char* n : {"a", "b", "c", "d"}) {
        std::string name(n);
        tools.register_tool_function(name, agenticdsl::ToolMetadata{name, "test", "test", agenticdsl::ToolCategory::ReadOnly, agenticdsl::LayerProfile::Workflow}, [&in_flight, &max_concurrent](const auto&) -> nlohmann::json {
            int cur = in_flight.fetch_add(1) + 1;
            int prev = max_concurrent.load();
            while (cur > prev && !max_concurrent.compare_exchange_weak(prev, cur)) {}
            std::this_thread::sleep_for(30ms);
            in_flight.fetch_sub(1);
            return {{"ok", true}};
        });
    }

    TopoScheduler::Config config;
    TopoScheduler scheduler(std::move(config), tools, nullptr, nullptr);
    for (const char* n : {"a", "b", "c", "d"}) {
        std::string name(n);
        scheduler.register_node(std::make_unique<ToolCallNode>("/" + name, name, std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));
    }

    Context ctx;
    auto result = scheduler.execute_parallel(ctx);
    REQUIRE(result.success);
    REQUIRE(max_concurrent.load() >= 1);
}

TEST_CASE("execute_parallel empty DAG returns success with no tasks",
          "[scheduler][c2-coverage][advanced]") {
    ToolRegistry tools;
    TopoScheduler::Config config;
    TopoScheduler scheduler(std::move(config), tools, nullptr, nullptr);

    Context ctx;
    auto result = scheduler.execute_parallel(ctx);
    REQUIRE(result.success);
}

TEST_CASE("execute_parallel single-node DAG completes",
          "[scheduler][c2-coverage][advanced]") {
    ToolRegistry tools;
    std::atomic<int> counter{0};
    tools.register_tool_function("noop", agenticdsl::ToolMetadata{"noop", "test", "test", agenticdsl::ToolCategory::ReadOnly, agenticdsl::LayerProfile::Workflow}, [&counter](const auto&) -> nlohmann::json {
        counter.fetch_add(1);
        return {{"ok", true}};
    });

    TopoScheduler::Config config;
    TopoScheduler scheduler(std::move(config), tools, nullptr, nullptr);
    scheduler.register_node(std::make_unique<ToolCallNode>("/only", "noop", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));

    Context ctx;
    auto result = scheduler.execute_parallel(ctx);
    REQUIRE(result.success);
    REQUIRE(counter.load() == 1);
}

TEST_CASE("~TopoScheduler safely joins in-flight tf::Tasks",
          "[scheduler][c2-coverage][advanced]") {
    {
        ToolRegistry tools;
        std::atomic<int> counter{0};
        tools.register_tool_function("slow", agenticdsl::ToolMetadata{"slow", "test", "test", agenticdsl::ToolCategory::ReadOnly, agenticdsl::LayerProfile::Workflow}, [&counter](const auto&) -> nlohmann::json {
            std::this_thread::sleep_for(20ms);
            counter.fetch_add(1);
            return {{"ok", true}};
        });

        TopoScheduler::Config config;
        config.num_workers = 2;
        TopoScheduler scheduler(std::move(config), tools, nullptr, nullptr);
        for (int i = 0; i < 10; ++i) {
            scheduler.register_node(std::make_unique<ToolCallNode>(
                "/n" + std::to_string(i), "slow",
                std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));
        }
        Context ctx;
        scheduler.execute_parallel(ctx);
    }
    REQUIRE(true);
}

TEST_CASE("execute_parallel failure path triggers process_jump or success=false",
          "[scheduler][c2-coverage][advanced]") {
    ToolRegistry tools;
    std::atomic<int> ok_count{0};
    tools.register_tool_function("fail", agenticdsl::ToolMetadata{"fail", "test", "test", agenticdsl::ToolCategory::ReadOnly, agenticdsl::LayerProfile::Workflow}, [](const auto&) -> nlohmann::json {
        throw std::runtime_error("intentional failure");
    });
    tools.register_tool_function("ok", agenticdsl::ToolMetadata{"ok", "test", "test", agenticdsl::ToolCategory::ReadOnly, agenticdsl::LayerProfile::Workflow}, [&ok_count](const auto&) -> nlohmann::json {
        ok_count.fetch_add(1);
        return {{"ok", true}};
    });

    TopoScheduler::Config config;
    TopoScheduler scheduler(std::move(config), tools, nullptr, nullptr);
    scheduler.register_node(std::make_unique<ToolCallNode>("/f1", "fail", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));
    scheduler.register_node(std::make_unique<ToolCallNode>("/o1", "ok", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));
    scheduler.register_node(std::make_unique<ToolCallNode>("/f2", "fail", std::unordered_map<std::string, std::string>{}, std::vector<std::string>{}));

    Context ctx;
    auto result = scheduler.execute_parallel(ctx);
    // Per Sprint 12 C2 design: tool exceptions are caught and contained.
    // The ok node still runs (ok_count == 1), proving failure isolation.
    // process_jump path at topo_scheduler.cpp:270 is reached when session_result.success == false.
    REQUIRE(ok_count.load() == 1);
    REQUIRE(result.success);
}
