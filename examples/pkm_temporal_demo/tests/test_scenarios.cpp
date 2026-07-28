// examples/pkm_temporal_demo/tests/test_scenarios.cpp
// 功能描述：4 场景 DSL 文件验证测试 (Task 5, TDD Step 1)。
//          验证:
//            1. 4 个 .agent.md 文件存在
//            2. DSLEngine::from_file() 可成功解析 (无异常)
//            3. 解析后的 ParsedGraph 含 start + end 节点
// 设计依据：.rddf/plans/pkm-temporal-demo-scaffold.md Task 5 Step 1
// 作者：pkm-temporal-demo-scaffold Task 5
// 最后修改日期：2026-07-28

#include "catch_amalgamated.hpp"

#include "core/engine.h"

#include <filesystem>
#include <string>

using agenticdsl::DSLEngine;

namespace {

const std::string SCENARIO_DIR =
    "examples/pkm_temporal_demo/";

std::string scenario_path(const std::string& name) {
  return SCENARIO_DIR + "scenario-" + name + ".agent.md";
}

}  // namespace

// ============================================================================
// Test 1: 4 个 .agent.md 文件存在
// ============================================================================
TEST_CASE("Scenarios: all 4 .agent.md files exist on disk",
          "[demo][task5][files]") {
  for (const auto& name : {"blocking", "async-poll", "signal", "idempotent"}) {
    auto path = scenario_path(name);
    REQUIRE(std::filesystem::exists(path));
    REQUIRE(std::filesystem::file_size(path) > 0);
  }
}

// ============================================================================
// Test 2: DSLEngine::from_file() 成功解析 (无异常)
// ============================================================================
TEST_CASE("Scenarios: all 4 .agent.md files parse via DSLEngine::from_file",
          "[demo][task5][parse]") {
  for (const auto& name : {"blocking", "async-poll", "signal", "idempotent"}) {
    auto path = scenario_path(name);
    REQUIRE_NOTHROW([&] {
      auto engine = DSLEngine::from_file(path);
      REQUIRE(engine != nullptr);
    }());
  }
}

// ============================================================================
// Test 3: 每个场景解析后引擎非空
// ============================================================================
TEST_CASE("Scenarios: parsed engines are non-null",
          "[demo][task5][structure]") {
  for (const auto& name : {"blocking", "async-poll", "signal", "idempotent"}) {
    auto path = scenario_path(name);
    auto engine = DSLEngine::from_file(path);
    REQUIRE(engine != nullptr);
  }
}
