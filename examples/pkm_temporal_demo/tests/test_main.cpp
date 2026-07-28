// examples/pkm_temporal_demo/tests/test_main.cpp
// 功能描述：Demo CLI 参数解析单元测试 (Task 4, TDD Step 1)。
//          覆盖:
//            1. --mock + --scenario blocking 解析
//            2. --real 模式解析
//            3. 默认值 (无参数 -> Mock + 空 scenario)
//            4. --scenario 单独使用
//            5. 未知参数忽略
// 设计依据：.rddf/plans/pkm-temporal-demo-scaffold.md Task 4 Step 1
// 作者：pkm-temporal-demo-scaffold Task 4
// 最后修改日期：2026-07-28

#include "catch_amalgamated.hpp"

#include "demo_args.h"

#include <string>
#include <vector>

using DemoMode = agenticdsl::pdk::demo::DemoMode;
using DemoArgs = agenticdsl::pdk::demo::DemoArgs;
using agenticdsl::pdk::demo::parse_args;

// ============================================================================
// Test 1: --mock + --scenario blocking
// ============================================================================
TEST_CASE("Demo CLI: --mock and --scenario <name> parsing",
          "[demo][task4][parse_args]") {
  auto args = parse_args({"--mock", "--scenario", "blocking"});
  REQUIRE(args.mode == DemoMode::Mock);
  REQUIRE(args.scenario == "blocking");
}

// ============================================================================
// Test 2: --real 模式
// ============================================================================
TEST_CASE("Demo CLI: --real mode parsing",
          "[demo][task4][parse_args]") {
  auto args = parse_args({"--real", "--scenario", "async-poll"});
  REQUIRE(args.mode == DemoMode::Real);
  REQUIRE(args.scenario == "async-poll");
}

// ============================================================================
// Test 3: 默认值 (无参数)
// ============================================================================
TEST_CASE("Demo CLI: defaults to Mock mode with empty scenario",
          "[demo][task4][parse_args]") {
  auto args = parse_args({});
  REQUIRE(args.mode == DemoMode::Mock);
  REQUIRE(args.scenario.empty());
}

// ============================================================================
// Test 4: --scenario 单独使用 (mode 默认 Mock)
// ============================================================================
TEST_CASE("Demo CLI: --scenario alone defaults to Mock",
          "[demo][task4][parse_args]") {
  auto args = parse_args({"--scenario", "signal"});
  REQUIRE(args.mode == DemoMode::Mock);
  REQUIRE(args.scenario == "signal");
}

// ============================================================================
// Test 5: 未知参数忽略 (--verbose 等)
// ============================================================================
TEST_CASE("Demo CLI: unknown args ignored gracefully",
          "[demo][task4][parse_args]") {
  auto args = parse_args({"--mock", "--verbose", "--scenario", "idempotent", "--debug"});
  REQUIRE(args.mode == DemoMode::Mock);
  REQUIRE(args.scenario == "idempotent");
}

// ============================================================================
// Test 6: --mock 和 --real 同时出现, 后者优先
// ============================================================================
TEST_CASE("Demo CLI: last mode flag wins",
          "[demo][task4][parse_args]") {
  auto args = parse_args({"--mock", "--real"});
  REQUIRE(args.mode == DemoMode::Real);

  auto args2 = parse_args({"--real", "--mock"});
  REQUIRE(args2.mode == DemoMode::Mock);
}
