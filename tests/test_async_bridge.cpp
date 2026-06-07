// tests/test_async_bridge.cpp
// 功能描述：Taskflow + async_simple 桥接验证测试
//           验证两个异步库可以在同一编译单元中编译、链接和运行
// 设计依据：Slice 00 (docs/implementation-roadmap.md)
// 作者：AgenticDSL Slice 00
// 最后修改日期：2026-06-07
#include "catch_amalgamated.hpp"

#include <taskflow/taskflow.hpp>
#include <async_simple/coro/Lazy.h>
#include <async_simple/coro/SyncAwait.h>

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

// 验证 async_simple 的协程 Lazy<T> + syncAwait 基础链路工作正常
TEST_CASE("async_simple coroutine header compiles and executes", "[async][slice00]") {
    auto lazy_fn = []() -> async_simple::coro::Lazy<int> {
        co_return 42;
    };

    auto result = async_simple::coro::syncAwait(lazy_fn());
    REQUIRE(result == 42);
}

// 验证 Taskflow 与 async_simple 协程可共存于同一编译单元
// 模拟 Phase 2 协程桥接场景：协程内部驱动 Taskflow 子任务
TEST_CASE("Taskflow and async_simple coexist in same TU", "[async][slice00]") {
    tf::Executor executor;

    auto lazy_fn = [&executor]() -> async_simple::coro::Lazy<int> {
        tf::Taskflow taskflow;
        int value = 0;
        taskflow.emplace([&](){ value = 100; });
        // 同步等待 taskflow 完成
        executor.run(taskflow).wait();
        co_return value;
    };

    auto result = async_simple::coro::syncAwait(lazy_fn());
    REQUIRE(result == 100);
}
