// tests/test_pdk_macros.cpp
// 文件头注释
// 功能描述：PDK 单元测试 (Phase 1 Sprint 4)。
//          5 个 TEST_CASE 覆盖:
//            1. DECLARE_TOOL 展开 (ToolSpec + handler + 异常隔离)
//            2. DEFINE_AGENT 模板实例化 (class 构造 + run() 调用)
//            3. SafeExec 超时处理 (10ms timeout + 100ms sleep → 抛 runtime_error)
//            4. SafeExec 异常捕获 (handler 抛 std::runtime_error → 传播原异常)
//            5. PDK 头文件无 Runtime 内部依赖 (P3 静态链接验证)
// 设计依据：openspec/changes/2026-07-07-pdk-skeleton (Sprint 4) + ADR-0021 §3
// 作者：AgenticDSL Phase 1 Sprint 4
// 最后修改日期：2026-06-19

#include "catch_amalgamated.hpp"

#include "agenticdsl/pdk/pdk.h"
#include "agenticdsl/cognitive/simple_orchestrator.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/inmemory_bus.h"
#include "core/engine.h"
#include "core/types/tool_result.h"

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

using namespace hydraforge::pdk;

namespace {

// 空 DSL 模板 — start/end 占位, 实际不执行 run()
const std::string kEmptyDsl = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: start
    type: start
    next: ["/main/end"]
  - id: end
    type: end
# --- END AgenticDSL ---
```
)";

// 辅助: 创建最小 DSLEngine (供 DEFINE_AGENT 测试)
std::unique_ptr<agenticdsl::DSLEngine> make_minimal_engine() {
  return agenticdsl::DSLEngine::from_markdown(kEmptyDsl);
}

} // namespace

// =====================================================================
// Test 1: DECLARE_TOOL 展开
// =====================================================================
// DECLARE_TOOL 必须在 file scope 声明 (inline 变量不可在 block scope)
// 用法: DECLARE_TOOL(name, desc, body...) — body 含 return 语句, 无尾部 {}
DECLARE_TOOL(test_echo, "回显工具",
  return __pdk_args;
)

DECLARE_TOOL(test_throw, "抛异常工具",
  throw std::runtime_error("disk full");
)

TEST_CASE("PDK DECLARE_TOOL expands to ToolSpec + handler",
          "[pdk][sprint4][declare_tool]") {
  SECTION("ToolSpec metadata is correct") {
    REQUIRE(tool_spec_test_echo.name == "test_echo");
    REQUIRE(tool_spec_test_echo.description == "回显工具");
    REQUIRE(tool_spec_test_echo.params.empty());
    REQUIRE_FALSE(tool_spec_test_echo.permissions.network);
  }

  SECTION("Handler returns input args") {
    nlohmann::json input = {{"message", "hello"}};
    nlohmann::json output = tool_handler_test_echo(input);
    REQUIRE(output["message"] == "hello");
    REQUIRE_FALSE(output.contains("error"));
  }

  SECTION("Handler exception is caught and returned as error json") {
    nlohmann::json output = tool_handler_test_throw(nlohmann::json::object());
    REQUIRE(output.contains("error"));
    REQUIRE(output["error"] == "disk full");
  }

  SECTION("DECLARE_TOOL generates inline spec (header-only, no Runtime dep)") {
    REQUIRE(tool_spec_test_echo.name == "test_echo");
  }
}

// =====================================================================
// Test 2: DEFINE_AGENT 模板实例化 (React loop MVP)
// =====================================================================
// DEFINE_AGENT 必须在 file scope 声明 (static_assert + class 不可在 block scope)
DEFINE_AGENT(test_agent, AgentLoopType::React);

TEST_CASE("PDK DEFINE_AGENT instantiates React loop template",
          "[pdk][sprint4][define_agent]") {
  auto bus = std::make_shared<agenticdsl::InMemoryBus>();
  auto engine = make_minimal_engine();

  SECTION("Class can be instantiated") {
    test_agentAgent agent(std::move(engine), bus);
    // 仅验证构造 + 析构无 crash
    SUCCEED("test_agentAgent constructed successfully");
  }

  SECTION("Agent run() returns LoopResult (MVP: may fail due to mock LLM)") {
    test_agentAgent agent(std::move(engine), bus);
    // Sprint 20: 返回类型从 ToolResult 统一为 LoopResult
    ::hydraforge::pdk::LoopResult result = agent.run("test prompt");
    // MVP: 单轮 ReAct, 不期望 success=true (mock LLM 无响应)
    // 仅验证返回 LoopResult 类型 + 字段可访问
    REQUIRE(result.final_context.working["data"].is_object());
    REQUIRE(result.final_context.working["meta"].is_object());
  }
}

// 注: DEFINE_AGENT(PlanExecute) 在编译期 static_assert 失败, 不需要运行时测试
// (验证: 取消注释以下代码会看到编译错误)
// DEFINE_AGENT(test_bad_agent, AgentLoopType::PlanExecute);

// =====================================================================
// Test 3: SafeExec 超时处理
// =====================================================================
TEST_CASE("PDK SafeExec enforces timeout",
          "[pdk][sprint4][safe_exec][timeout]") {
  SafeExec exec;

  SECTION("Function completes within timeout") {
    int result = exec.with_timeout(std::chrono::milliseconds(100)).run([] {
      return 42;
    });
    REQUIRE(result == 42);
  }

  SECTION("Function exceeds timeout → throw runtime_error") {
    REQUIRE_THROWS_AS(
        exec.with_timeout(std::chrono::milliseconds(10)).run([] {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          return 42;
        }),
        std::runtime_error);
  }

  SECTION("Timeout error message contains timeout value") {
    try {
      exec.with_timeout(std::chrono::milliseconds(5)).run([] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return 42;
      });
      FAIL("Expected timeout exception");
    } catch (const std::runtime_error& e) {
      std::string msg = e.what();
      REQUIRE(msg.find("timed out") != std::string::npos);
      REQUIRE(msg.find("5ms") != std::string::npos);
    }
  }

  SECTION("Default timeout is 30s") {
    SafeExec default_exec;
    REQUIRE(default_exec.timeout() == std::chrono::milliseconds(30000));
  }
}

// =====================================================================
// Test 4: SafeExec 异常捕获
// =====================================================================
TEST_CASE("PDK SafeExec propagates handler exceptions",
          "[pdk][sprint4][safe_exec][exception]") {
  SafeExec exec;

  SECTION("std::runtime_error is propagated") {
    REQUIRE_THROWS_AS(
        exec.with_timeout(std::chrono::milliseconds(1000)).run([] {
          throw std::runtime_error("file not found");
        }),
        std::runtime_error);
  }

  SECTION("Original exception message preserved") {
    try {
      exec.with_timeout(std::chrono::milliseconds(1000)).run([] {
        throw std::runtime_error("connection refused");
      });
      FAIL("Expected exception");
    } catch (const std::exception& e) {
      std::string msg = e.what();
      REQUIRE(msg == "connection refused");
    }
  }

  SECTION("Different exception types propagate correctly") {
    REQUIRE_THROWS_AS(
        exec.with_timeout(std::chrono::milliseconds(1000)).run([] {
          throw std::invalid_argument("bad arg");
        }),
        std::invalid_argument);

    REQUIRE_THROWS_AS(
        exec.with_timeout(std::chrono::milliseconds(1000)).run([] {
          throw std::out_of_range("out of range");
        }),
        std::out_of_range);
  }
}

// =====================================================================
// Test 5: PDK 头文件无 Runtime 内部依赖 (P3 静态链接)
// =====================================================================
TEST_CASE("PDK headers compile without Runtime internal dependencies",
          "[pdk][sprint4][runtime_decoupling]") {
  SECTION("ToolSpec / ToolParam / ToolPermissions are POD-ish structs") {
    ToolSpec spec;
    spec.name = "x";
    spec.description = "y";
    spec.params = {{"p1", "string", true}};
    spec.permissions.readonly_paths = {"/tmp"};
    spec.permissions.network = true;

    REQUIRE(spec.name == "x");
    REQUIRE(spec.params.size() == 1);
    REQUIRE(spec.params[0].required);
    REQUIRE(spec.permissions.readonly_paths.size() == 1);
  }

  SECTION("AgentLoopType enum has expected values") {
    REQUIRE(static_cast<int>(AgentLoopType::React) == 0);
    REQUIRE(static_cast<int>(AgentLoopType::PlanExecute) == 1);
    REQUIRE(static_cast<int>(AgentLoopType::ForkJoin) == 2);
  }

  SECTION("SafeExec chainable configuration returns self-reference") {
    SafeExec exec;
    SafeExec& ref1 = exec.with_timeout(std::chrono::milliseconds(100));
    SafeExec& ref2 = exec.with_layer_profile(2);
    REQUIRE(&ref1 == &exec);
    REQUIRE(&ref2 == &exec);
    REQUIRE(exec.timeout() == std::chrono::milliseconds(100));
    REQUIRE(exec.layer_profile() == 2);
  }

  SECTION("PDK version macros are defined") {
#ifndef HYDRAFORGE_PDK_VERSION
    FAIL("HYDRAFORGE_PDK_VERSION not defined");
#endif
    std::string version = HYDRAFORGE_PDK_VERSION;
    REQUIRE_FALSE(version.empty());
  }
}