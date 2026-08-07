#include "catch_amalgamated.hpp"
#include "event_handler.h"

#include <agenticdsl/contract/bus_event.h>
#include <agenticdsl/contract/event_builder.h>
#include <agenticdsl/contract/inmemory_bus.h>
#include <core/types/tool_result.h>

#include <iostream>
#include <sstream>

using namespace pdk_chat_demo;
using namespace agenticdsl;

namespace {
BusEvent make_llm_response(const nlohmann::json& args, const std::string& trace_id = "trace-1") {
  return EventBuilder("llm.response").args(args).meta({{"trace_id", trace_id}}).build();
}

BusEvent make_loop_decision(const std::string& decision, const std::string& trace_id = "trace-d") {
  return EventBuilder("loop.decision")
      .args({{"decision", decision}, {"tool", "loop/run"}})
      .meta({{"trace_id", trace_id}})
      .build();
}
}  // namespace

TEST_CASE("EventHandler renders llm.response with completion_tokens field", "[event_handler][rendering]") {
  auto bus = std::make_shared<InMemoryBus>();
  std::ostringstream out;
  EventHandler handler(bus, &out);
  bus->emit(make_llm_response({{"tokens", 42}, {"completion_tokens", 17},
                                {"prompt_tokens", 25}, {"ok", true},
                                {"duration_ms", 250}}));
  bus->wait_for_drain();
  const auto rendered = out.str();
  CHECK(rendered.find("llm.response") != std::string::npos);
  CHECK(rendered.find("tokens=42") != std::string::npos);
  CHECK(rendered.find("ok=true") != std::string::npos);
}

TEST_CASE("EventHandler renders llm.response metadata-only without appending literal null", "[event_handler][rendering]") {
  auto bus = std::make_shared<InMemoryBus>();
  std::ostringstream out;
  EventHandler handler(bus, &out);
  bus->emit(make_llm_response({{"ok", false}, {"error_code", 503},
                                {"error_message", "service unavailable"}}));
  bus->wait_for_drain();
  const auto rendered = out.str();
  CHECK(rendered.find("llm.response") != std::string::npos);
  CHECK(rendered.find("ok=false") != std::string::npos);
}

TEST_CASE("EventHandler renders loop.decision with topic + trace_id metadata", "[event_handler][rendering]") {
  auto bus = std::make_shared<InMemoryBus>();
  std::ostringstream out;
  EventHandler handler(bus, &out);
  bus->emit(make_loop_decision("respond", "trace-abc-123"));
  bus->wait_for_drain();
  const auto rendered = out.str();
  CHECK(rendered.find("loop.decision") != std::string::npos);
  CHECK(rendered.find("respond") != std::string::npos);
  CHECK(rendered.find("trace-abc-123") != std::string::npos);
}

TEST_CASE("EventHandler renders budget alert as independent line, not interleaved", "[event_handler][rendering]") {
  auto bus = std::make_shared<InMemoryBus>();
  std::ostringstream out;
  EventHandler handler(bus, &out);
  bus->emit(make_llm_response({{"tokens", 100}, {"completion_tokens", 50},
                                {"prompt_tokens", 50}, {"ok", true},
                                {"duration_ms", 200}}));
  bus->emit(EventBuilder("budget.checked")
                .args({{"remaining_usd", 0.05}})
                .build());
  bus->wait_for_drain();
  const auto rendered = out.str();
  const auto llm_line = rendered.find("llm.response");
  const auto budget_line = rendered.find("budget.checked");
  REQUIRE(llm_line != std::string::npos);
  REQUIRE(budget_line != std::string::npos);
  CHECK(budget_line > llm_line);
  CHECK(rendered.find("\n", llm_line) < budget_line);
}

TEST_CASE("EventHandler tolerates unknown extra payload fields in loop.decision", "[event_handler][rendering]") {
  auto bus = std::make_shared<InMemoryBus>();
  std::ostringstream out;
  EventHandler handler(bus, &out);
  bus->emit(EventBuilder("loop.decision")
                .args({{"decision", "tool_call"}, {"tool", "fs/read"},
                        {"unknown_future_field", 42}})
                .meta({{"trace_id", "trace-future"}})
                .build());
  bus->wait_for_drain();
  const auto rendered = out.str();
  CHECK(rendered.find("loop.decision") != std::string::npos);
  CHECK(rendered.find("tool_call") != std::string::npos);
  CHECK(rendered.find("unknown_future_field") == std::string::npos);
}
