// tests/test_async_bridge.cpp
// 功能描述：Taskflow 基础桥接验证 (async_simple 已 deprecated by C2 Day 1-2, ADR-0030 V2 决策: std::jthread 替代)
// 设计依据：Slice 00 (docs/implementation-roadmap.md)
// 作者：AgenticDSL Slice 00 / C2 Day 1-2 迁移
// 最后修改日期：2026-06-27
#include "catch_amalgamated.hpp"

#include <taskflow/taskflow.hpp>

// 验证 Taskflow 头文件可包含且基础功能（executor + task graph）工作正常
TEST_CASE("Taskflow header is includable and functional", "[async][slice00]") {
    tf::Executor executor;
    tf::Taskflow taskflow;

    int result = 0;
    // 构造两条任务 A、B，B 依赖 A 完成后执行
    auto A = taskflow.emplace([&](){ result = 42; });
    auto B = taskflow.emplace([&](){ result += 8; });
    B.succeed(A);

    // 同步等待 taskflow 执行完成
    executor.run(taskflow).wait();
    REQUIRE(result == 50);
}

// 验证 Taskflow DAG 并行派发 (C2 P1 准备)
TEST_CASE("Taskflow DAG parallel dispatch independent tasks", "[async][c2-day1]") {
    tf::Executor executor(4);  // 4 worker threads
    tf::Taskflow taskflow;

    std::atomic<int> counter{0};
    std::atomic<bool> a_done{false};

    auto A = taskflow.emplace([&](){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        a_done.store(true);
    });
    auto B = taskflow.emplace([&](){
        // B 不依赖 A，应该与 A 并行执行
        REQUIRE(a_done.load() == false);  // B 开始时 A 可能未完成
        counter.fetch_add(1);
    });
    auto C = taskflow.emplace([&](){
        counter.fetch_add(1);
    });

    executor.run(taskflow).wait();
    REQUIRE(counter.load() == 2);
}