// tests/test_llm_event_emission.cpp
// 功能描述：TracingDecorator 事件发射测试 — 验证 llm.request/llm.response 顺序
// 设计依据：openspec/changes/adr-0068-event-emission-contract/design.md Decision 3
//           + §2 LLM Decorator 链迁移任务 2.12
// 作者：AgenticDSL Phase 6a
// 最后修改日期：2026-08-03
#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/bus_event.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/inmemory_bus.h"
#include "common/llm/llm_types.h"
#include "common/llm/mock_provider.h"
#include "common/llm/tracing_decorator.h"

using namespace agenticdsl;

TEST_CASE("TracingDecorator emits llm.request before llm.response", "[llm][event]") {
  auto inner = std::make_unique<MockLLMProvider>();
  auto bus = std::make_shared<InMemoryBus>();
  auto dec = std::make_unique<TracingDecorator>(std::move(inner), bus);

  std::vector<std::string> topics;
  bus->subscribe("llm.request",
                 [&](const BusEvent& ev) { topics.push_back(ev.topic); });
  bus->subscribe("llm.response",
                 [&](const BusEvent& ev) { topics.push_back(ev.topic); });

  GenerationRequest req;
  req.prompt = "hello";
  req.params.model = "mock-v1";
  req.params.max_tokens = 10;
  auto result = dec->generate(req, {});

  bus->wait_for_drain();
  REQUIRE(topics.size() == 2);
  REQUIRE(topics[0] == "llm.request");
  REQUIRE(topics[1] == "llm.response");
  REQUIRE(result.has_value());
}

TEST_CASE("TracingDecorator llm.request payload contains model and prompt_hash",
          "[llm][event]") {
  auto inner = std::make_unique<MockLLMProvider>();
  auto bus = std::make_shared<InMemoryBus>();
  auto dec = std::make_unique<TracingDecorator>(std::move(inner), bus);

  std::vector<BusEvent> request_events;
  bus->subscribe("llm.request",
                 [&](const BusEvent& ev) { request_events.push_back(ev); });

  GenerationRequest req;
  req.prompt = "audit-this-prompt";
  req.params.model = "mock-v1";
  req.params.max_tokens = 16;
  dec->generate(req, {});

  bus->wait_for_drain();
  REQUIRE(request_events.size() == 1);
  REQUIRE(request_events[0].payload.data.contains("model"));
  REQUIRE(request_events[0].payload.data["model"] == "mock-v1");
  REQUIRE(request_events[0].payload.data.contains("prompt_hash"));
  REQUIRE(request_events[0].payload.data["prompt_hash"].is_string());
}
