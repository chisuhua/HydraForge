// tests/test_context_compact_events_payload.cpp
// 功能描述：context.compact.* 事件 payload schema 契约验证
//          (ADR-0068 附录 A + ADR-0007 context-compression)
//          4 cases: before payload / after payload / pairing / null-bus skip
// 设计依据：openspec/changes/compact-events-emit (P10)
// 作者：HydraForge Sprint 22 P10 ship
// 最后修改日期：2026-08-20

#include "catch_amalgamated.hpp"

#include "core/context_compactor.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/bus_event.h"
#include "common/llm/llm_types.h"
#include "test_helpers/mock_bus.h"

#include <memory>

using namespace agenticdsl;

// =====================================================================
// Test 1: context.compact.before payload schema (ADR-0068 Appendix A)
// =====================================================================
TEST_CASE("context.compact.before payload schema (ADR-0068)",
          "[context_compact][topic][before]") {
  auto bus = std::make_shared<test::MockBus>();
  ContextCompactorImpl compactor(4096, nullptr, bus);

  compactor.on_compact_before("sess_xyz", 5000);

  REQUIRE(bus->events.size() == 1);
  REQUIRE(bus->topics[0] == "context.compact.before");

  const auto& e = bus->events[0];
  REQUIRE(e.payload.ok == true);
  REQUIRE(e.payload.data.contains("session_id"));
  REQUIRE(e.payload.data.contains("tokens_before"));
  REQUIRE(e.payload.data["session_id"] == "sess_xyz");
  REQUIRE(e.payload.data["tokens_before"] == 5000);

  // meta: component + trace_id (ADR-0068 EventBuilder V2)
  REQUIRE(e.payload.meta.contains("component"));
  REQUIRE(e.payload.meta["component"] == "context_compactor");
  REQUIRE(e.payload.meta.contains("trace_id"));
}

// =====================================================================
// Test 2: context.compact.after payload schema (含 compression_ratio)
// =====================================================================
TEST_CASE("context.compact.after payload schema (ADR-0068)",
          "[context_compact][topic][after]") {
  auto bus = std::make_shared<test::MockBus>();
  ContextCompactorImpl compactor(4096, nullptr, bus);

  compactor.on_compact_after("sess_xyz", 5000, 1000);

  REQUIRE(bus->events.size() == 1);
  REQUIRE(bus->topics[0] == "context.compact.after");

  const auto& e = bus->events[0];
  REQUIRE(e.payload.ok == true);
  REQUIRE(e.payload.data.contains("session_id"));
  REQUIRE(e.payload.data.contains("tokens_before"));
  REQUIRE(e.payload.data.contains("tokens_after"));
  REQUIRE(e.payload.data.contains("compression_ratio"));
  REQUIRE(e.payload.data["session_id"] == "sess_xyz");
  REQUIRE(e.payload.data["tokens_before"] == 5000);
  REQUIRE(e.payload.data["tokens_after"] == 1000);
  REQUIRE(e.payload.data["compression_ratio"] == 0.2);  // 1000/5000
}

// =====================================================================
// Test 3: before/after 配对 (同 session_id)
// =====================================================================
TEST_CASE("context.compact before/after pairing for same session_id",
          "[context_compact][pairing]") {
  auto bus = std::make_shared<test::MockBus>();
  ContextCompactorImpl compactor(4096, nullptr, bus);

  compactor.on_compact_before("sess_pair", 8000);
  compactor.on_compact_after("sess_pair", 8000, 2000);

  REQUIRE(bus->events.size() == 2);
  REQUIRE(bus->events[0].topic == "context.compact.before");
  REQUIRE(bus->events[1].topic == "context.compact.after");
  REQUIRE(bus->events[0].payload.data["session_id"] == "sess_pair");
  REQUIRE(bus->events[1].payload.data["session_id"] == "sess_pair");

  // 不变量: after.tokens_before == before.tokens_before
  REQUIRE(bus->events[0].payload.data["tokens_before"] ==
          bus->events[1].payload.data["tokens_before"]);
}

// =====================================================================
// Test 4: null bus 静默跳过 (impl_->event_bus_ == nullptr)
// =====================================================================
TEST_CASE("context.compact silent skip when bus is null (ADR-0068 opt-in)",
          "[context_compact][null_bus]") {
  // 构造 compactor 不传 bus → event_bus_ == nullptr
  ContextCompactorImpl compactor(4096, nullptr, nullptr);

  // 不抛异常, 不 emit (silent skip per ADR-0068 §决策 "emit 不阻塞状态转换")
  REQUIRE_NOTHROW(compactor.on_compact_before("sess_null", 5000));
  REQUIRE_NOTHROW(compactor.on_compact_after("sess_null", 5000, 1000));
}
