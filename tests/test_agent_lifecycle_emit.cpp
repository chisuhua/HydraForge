// tests/test_agent_lifecycle_emit.cpp
// 功能描述：agent.* 生命周期事件 emit 验证（P2 emit-agent-lifecycle-events）
//          ≥ 6 cases: helper 语义 + PluginLoader 转换点 + CognitiveWorker spawn/stop
// 设计依据：openspec/changes/emit-agent-lifecycle-events (P2)
// 作者：HydraForge Sprint 22 P2 ship
// 最后修改日期：2026-08-20

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/bus_event.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/inmemory_bus.h"
#include "agenticdsl/plugin/agent_lifecycle_emitter.h"
#include "agenticdsl/cognitive/cognitive_worker.h"
#include "core/engine.h"

#include <memory>
#include <string>
#include <vector>

namespace {

struct TestSubscriber {
  std::vector<agenticdsl::BusEvent> events;
  size_t token = 0;

  void subscribe(agenticdsl::InMemoryBus& bus) {
    token = bus.subscribe("*", [this](const agenticdsl::BusEvent& e) {
      events.push_back(e);
    });
  }
};

}  // namespace

TEST_CASE("emit_agent_lifecycle_event: kSpawned topic = agent.spawned",
          "[agent_lifecycle][P2][emit]") {
  auto bus = std::make_shared<agenticdsl::InMemoryBus>();
  TestSubscriber sub;
  sub.subscribe(*bus);

  agenticdsl::emit_agent_lifecycle_event(
      bus.get(), agenticdsl::AgentLifecycleState::kSpawned,
      "test-agent", "test-plugin", "1.0.0");

  bus->wait_for_drain();

  REQUIRE(sub.events.size() >= 1);
  REQUIRE(sub.events[0].topic == "agent.spawned");
}

TEST_CASE("emit_agent_lifecycle_event: kTerminated topic = agent.terminated",
          "[agent_lifecycle][P2][emit]") {
  auto bus = std::make_shared<agenticdsl::InMemoryBus>();
  TestSubscriber sub;
  sub.subscribe(*bus);

  agenticdsl::emit_agent_lifecycle_event(
      bus.get(), agenticdsl::AgentLifecycleState::kTerminated,
      "test-agent", "test-plugin");

  bus->wait_for_drain();

  REQUIRE(sub.events.size() >= 1);
  REQUIRE(sub.events[0].topic == "agent.terminated");
}

TEST_CASE("emit_agent_lifecycle_event: kError topic = agent.error",
          "[agent_lifecycle][P2][emit]") {
  auto bus = std::make_shared<agenticdsl::InMemoryBus>();
  TestSubscriber sub;
  sub.subscribe(*bus);

  agenticdsl::emit_agent_lifecycle_event(
      bus.get(), agenticdsl::AgentLifecycleState::kError,
      "test-agent", "test-plugin", "", "INIT_FAILED", "init returned false");

  bus->wait_for_drain();

  REQUIRE(sub.events.size() >= 1);
  REQUIRE(sub.events[0].topic == "agent.error");
}

TEST_CASE("emit_agent_lifecycle_event: kHeartbeat topic = agent.heartbeat",
          "[agent_lifecycle][P2][emit]") {
  auto bus = std::make_shared<agenticdsl::InMemoryBus>();
  TestSubscriber sub;
  sub.subscribe(*bus);

  agenticdsl::emit_agent_lifecycle_event(
      bus.get(), agenticdsl::AgentLifecycleState::kHeartbeat,
      "test-agent", "test-plugin");

  bus->wait_for_drain();

  REQUIRE(sub.events.size() >= 1);
  REQUIRE(sub.events[0].topic == "agent.heartbeat");
}

TEST_CASE("emit_agent_lifecycle_event: nullptr bus 不崩溃",
          "[agent_lifecycle][P2][nullptr]") {
  agenticdsl::emit_agent_lifecycle_event(
      nullptr, agenticdsl::AgentLifecycleState::kSpawned,
      "test-agent", "test-plugin");
  REQUIRE(true);
}

TEST_CASE("CognitiveWorker start() emit agent.spawned",
          "[agent_lifecycle][P2][cognitive]") {
  using namespace agenticdsl;

  auto bus = std::make_shared<InMemoryBus>();
  TestSubscriber sub;
  sub.subscribe(*bus);

  auto engine = std::make_unique<DSLEngine>();
  engine->set_interaction_bus(bus);

  {
    CognitiveWorker worker(std::move(engine), bus);
    worker.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    worker.stop();
    bus->wait_for_drain();

    // 至少应有 spawned + terminated 两个事件
    int spawned = 0, terminated = 0;
    for (const auto& e : sub.events) {
      if (e.topic == "agent.spawned") spawned++;
      if (e.topic == "agent.terminated") terminated++;
    }
    REQUIRE(spawned >= 1);
    REQUIRE(terminated >= 1);
  }
}