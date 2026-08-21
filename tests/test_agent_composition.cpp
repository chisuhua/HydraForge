// tests/test_agent_composition.cpp
// 功能描述：Agent 编排模式测试（P8 adr-0060-p2-p3-patterns）
//          3 模式 × 3 case = 9 + stream throws 1 = 10 cases
// 设计依据：openspec/changes/adr-0060-p2-p3-patterns (P8)
// 作者：HydraForge Sprint 22 P8 ship
// 最后修改日期：2026-08-20

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/iagent_composition.h"
#include "agenticdsl/contract/test_double_registry.h"

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <string>

using namespace agenticdsl;

namespace {

std::shared_ptr<TestDoubleAgentRegistry> make_registry() {
  auto registry = std::make_shared<TestDoubleAgentRegistry>();
  registry->register_agent("mock-agent", [](const std::string& config) {
    return std::make_unique<MockAgent>(config.empty() ? "mock-agent" : config);
  });
  return registry;
}

class ThrowingAgent : public MockAgent {
 public:
  ThrowingAgent() : MockAgent("bad-agent") {}
  std::string run(const std::string&) override {
    throw std::runtime_error("boom");
  }
};

}  // namespace

TEST_CASE("call: 同步调用已注册 agent 返回结果",
          "[agent_composition][P8][call]") {
  auto registry = make_registry();
  auto comp = make_agent_composition(registry);

  auto result = comp->call("mock-agent", "hello");

  REQUIRE(result.ok);
  REQUIRE(result.value.find("mock-agent") != std::string::npos);
}

TEST_CASE("call: 未注册 agent 返回 ToolNotRegistered",
          "[agent_composition][P8][call]") {
  auto registry = make_registry();
  auto comp = make_agent_composition(registry);

  auto result = comp->call("nonexistent-agent", "hello");

  REQUIRE_FALSE(result.ok);
  REQUIRE(result.error_code == ErrorCode::ToolNotRegistered);
}

TEST_CASE("call: agent 抛异常转 ErrorCode::Unknown",
          "[agent_composition][P8][call]") {
  auto registry = std::make_shared<TestDoubleAgentRegistry>();
  registry->register_agent("bad-agent", [](const std::string&) {
    return std::make_unique<ThrowingAgent>();
  });
  auto comp = make_agent_composition(registry);

  auto result = comp->call("bad-agent", "x");

  REQUIRE_FALSE(result.ok);
  REQUIRE(result.error_code == ErrorCode::Unknown);
}

TEST_CASE("call_async: 异步调用返回 future，callback 调用",
          "[agent_composition][P8][call_async]") {
  auto registry = make_registry();
  auto comp = make_agent_composition(registry);

  bool callback_called = false;
  auto future = comp->call_async(
      "mock-agent", "async-hello",
      [&callback_called](AgentResult<std::string> result) {
        REQUIRE(result.ok);
        callback_called = true;
      });

  auto result = future.get();
  REQUIRE(result.ok);
  REQUIRE(callback_called);
}

TEST_CASE("call_async: 未注册 agent 返回 ToolNotRegistered",
          "[agent_composition][P8][call_async]") {
  auto registry = make_registry();
  auto comp = make_agent_composition(registry);

  auto future = comp->call_async("missing", "x");
  auto result = future.get();

  REQUIRE_FALSE(result.ok);
  REQUIRE(result.error_code == ErrorCode::ToolNotRegistered);
}

TEST_CASE("call_async: 多并发调用互不干扰",
          "[agent_composition][P8][call_async]") {
  auto registry = make_registry();
  auto comp = make_agent_composition(registry);

  std::vector<std::future<AgentResult<std::string>>> futures;
  for (int i = 0; i < 10; ++i) {
    futures.push_back(comp->call_async("mock-agent", "task-" + std::to_string(i)));
  }
  for (auto& f : futures) {
    auto result = f.get();
    REQUIRE(result.ok);
  }
}

TEST_CASE("delegate: 返回 TaskHandle 含 task_id",
          "[agent_composition][P8][delegate]") {
  auto registry = make_registry();
  auto comp = make_agent_composition(registry);

  auto handle = comp->delegate("mock-agent", "some-task", "high");

  REQUIRE_FALSE(handle.task_id.empty());
}

TEST_CASE("delegate: priority 参数接受（FIFO 降级）",
          "[agent_composition][P8][delegate]") {
  auto registry = make_registry();
  auto comp = make_agent_composition(registry);

  auto h1 = comp->delegate("mock-agent", "t1", "high");
  auto h2 = comp->delegate("mock-agent", "t2", "normal");
  auto h3 = comp->delegate("mock-agent", "t3", "low");

  REQUIRE_FALSE(h1.task_id.empty());
  REQUIRE_FALSE(h2.task_id.empty());
  REQUIRE_FALSE(h3.task_id.empty());
  // FIFO：3 个不同 task_id
  REQUIRE(h1.task_id != h2.task_id);
  REQUIRE(h2.task_id != h3.task_id);
}

TEST_CASE("delegate: cancel API 可调用（no-op）",
          "[agent_composition][P8][delegate]") {
  auto registry = make_registry();
  auto comp = make_agent_composition(registry);

  auto handle = comp->delegate("mock-agent", "t");
  REQUIRE_NOTHROW(handle.cancel());
}

TEST_CASE("stream: Phase 2 占位 throw std::logic_error",
          "[agent_composition][P8][stream]") {
  auto registry = make_registry();
  auto comp = make_agent_composition(registry);

  REQUIRE_THROWS_AS(comp->stream("mock-agent", "x"), std::logic_error);
}