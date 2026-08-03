#include "catch_amalgamated.hpp"
#include "agenticdsl/contract/event_builder.h"
#include "agenticdsl/contract/bus_event.h"
#include "core/types/tool_result.h"

using namespace agenticdsl;

TEST_CASE("EventBuilder constructs BusEvent with args in data and meta in meta", "[contract][event]") {
    auto ev = EventBuilder("tool.execution.start")
                  .args({{"tool", "shell.exec"}, {"layer", "workflow"}})
                  .meta({{"trace_id", "tid-42"}})
                  .build();

    REQUIRE(ev.topic == "tool.execution.start");
    REQUIRE(ev.payload.ok == true);
    REQUIRE(ev.payload.data["tool"] == "shell.exec");
    REQUIRE(ev.payload.data["layer"] == "workflow");
    REQUIRE(ev.payload.meta["trace_id"] == "tid-42");
}

TEST_CASE("EventBuilder auto-fills monotonic timestamp", "[contract][event]") {
    auto t0 = std::chrono::steady_clock::now();
    auto ev = EventBuilder("x").build();
    auto t1 = std::chrono::steady_clock::now();
    REQUIRE(ev.timestamp >= t0);
    REQUIRE(ev.timestamp <= t1);
}

TEST_CASE("EventBuilder default args/meta are empty objects", "[contract][event]") {
    auto ev = EventBuilder("test.topic").build();
    REQUIRE(ev.payload.data.is_object());
    REQUIRE(ev.payload.data.empty());
    REQUIRE(ev.payload.meta.is_object());
    REQUIRE(ev.payload.meta.empty());
}
