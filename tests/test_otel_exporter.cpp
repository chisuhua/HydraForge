// tests/test_otel_exporter.cpp
// 功能描述：OTel span exporter 单元测试（P11 otel-exporter-skeleton, ADR-0063）
//          6 cases: 订阅过滤 + span 转换 + sink 注入 + 4 类事件 + 边界
//
//          Amendment (2026-08-21):
//          - 移除后台 flush_loop + 改用同步 MockBus（test_helpers/mock_bus.h）
//          - InMemoryBus 异步派发 + wait_for_drain 在单元测试中 race / flaky，
//            同步 MockBus 是工业惯例（P12 mock-bus-canonical-extract 一致，
//            test_event_log_* 系列验证过）
//          - 修复原 use-after-free：捕获 sink_ptr 为 InMemorySpanSink* 而非
//            ISpanSink*，所有断言必须在 inner block 内完成
//
// 设计依据：openspec/changes/otel-exporter-skeleton (P11)
// 作者：HydraForge Sprint 22 P11 ship
// 最后修改日期：2026-08-21

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/bus_event.h"
#include "otel_exporter.h"
#include "otel_config.h"
#include "test_helpers/mock_bus.h"

#include <memory>
#include <string>

using namespace agenticdsl;

namespace {

std::shared_ptr<test::MockBus> make_bus() {
  return std::make_shared<test::MockBus>();
}

BusEvent make_event(const std::string& topic, const nlohmann::json& data = {}) {
  BusEvent e;
  e.topic = topic;
  e.payload.ok = true;
  e.payload.data = data;
  return e;
}

OtelConfig make_config() {
  OtelConfig cfg;
  cfg.otel_enabled = true;
  cfg.flush_interval_ms = 10;
  return cfg;
}

}  // namespace

TEST_CASE("OtelExporter 构造析构不挂起（最小化）",
          "[otel][P11][exporter][smoke]") {
  auto bus = make_bus();
  auto sink = std::make_unique<InMemorySpanSink>();
  {
    OtelExporter exporter(make_config(), bus, std::move(sink));
  }
  REQUIRE(true);
}

TEST_CASE("OtelExporter 订阅并转换 llm.* 事件为 span",
          "[otel][P11][exporter]") {
  auto bus = make_bus();
  auto sink = std::make_unique<InMemorySpanSink>();
  InMemorySpanSink* sink_ptr = sink.get();
  // Amendment: 断言必须在 inner block 内完成（exporter 析构前），
  // 避免 sink_ unique_ptr 销毁后 use-after-free。
  {
    OtelExporter exporter(make_config(), bus, std::move(sink));
    bus->emit(make_event("llm.request", {{"agent_id", "a1"}}));
    REQUIRE(sink_ptr->size() == 1);
    auto spans = sink_ptr->spans();
    REQUIRE(spans[0].topic == "llm.request");
    REQUIRE(spans[0].agent_id == "a1");
  }
  REQUIRE(true);
}

TEST_CASE("OtelExporter 过滤非 4 类事件（4 类前缀外不产生 span）",
          "[otel][P11][exporter]") {
  auto bus = make_bus();
  auto sink = std::make_unique<InMemorySpanSink>();
  InMemorySpanSink* sink_ptr = sink.get();
  {
    OtelExporter exporter(make_config(), bus, std::move(sink));
    bus->emit(make_event("conversation.user_message"));
    bus->emit(make_event("unknown.event"));
    bus->emit(make_event("random.topic"));
    REQUIRE(sink_ptr->size() == 0);
  }
  REQUIRE(true);
}

TEST_CASE("OtelExporter 订阅 llm.* tool.* agent.* dsl.* 全部 4 类事件",
          "[otel][P11][exporter]") {
  auto bus = make_bus();
  auto sink = std::make_unique<InMemorySpanSink>();
  InMemorySpanSink* sink_ptr = sink.get();
  {
    OtelExporter exporter(make_config(), bus, std::move(sink));
    bus->emit(make_event("llm.request", {{"agent_id", "a2"}}));
    bus->emit(make_event("tool.completed", {{"agent_id", "a2"}}));
    bus->emit(make_event("agent.spawned", {{"agent_id", "a2"}}));
    bus->emit(make_event("dsl.call.started", {{"agent_id", "a2"}}));
    REQUIRE(sink_ptr->size() == 4);
    auto spans = sink_ptr->spans();
    REQUIRE(spans[0].topic == "llm.request");
    REQUIRE(spans[1].topic == "tool.completed");
    REQUIRE(spans[2].topic == "agent.spawned");
    REQUIRE(spans[3].topic == "dsl.call.started");
  }
  REQUIRE(true);
}

TEST_CASE("OtelExporter span 含 agent_id / session_id / trace_id 属性",
          "[otel][P11][exporter]") {
  auto bus = make_bus();
  auto sink = std::make_unique<InMemorySpanSink>();
  InMemorySpanSink* sink_ptr = sink.get();
  BusEvent ev = make_event("llm.response",
                            {{"agent_id", "agent-x"},
                             {"session_id", "session-y"}});
  ev.payload.trace_id = "trace-123";
  {
    OtelExporter exporter(make_config(), bus, std::move(sink));
    bus->emit(ev);
    REQUIRE(sink_ptr->size() == 1);
    auto spans = sink_ptr->spans();
    const auto& span = spans[0];
    REQUIRE(span.agent_id == "agent-x");
    REQUIRE(span.session_id == "session-y");
    REQUIRE(span.trace_id == "trace-123");
    REQUIRE(span.attributes.at("agent_id") == "agent-x");
    REQUIRE(span.attributes.at("session_id") == "session-y");
    REQUIRE(span.attributes.at("trace_id") == "trace-123");
  }
  REQUIRE(true);
}

TEST_CASE("OtelExporter 默认 NoopSink（无 sink 注入不崩溃）",
          "[otel][P11][exporter]") {
  auto bus = make_bus();
  {
    OtelExporter exporter(make_config(), bus, nullptr);
    bus->emit(make_event("llm.request"));
    REQUIRE(exporter.sink() != nullptr);  // 默认 NoopSink
  }
  REQUIRE(true);
}

TEST_CASE("OtelExporter 批量 emit 10 事件全部转 span（同步路径，无丢事件）",
          "[otel][P11][exporter]") {
  auto bus = make_bus();
  auto sink = std::make_unique<InMemorySpanSink>();
  InMemorySpanSink* sink_ptr = sink.get();
  {
    OtelExporter exporter(make_config(), bus, std::move(sink));
    for (int i = 0; i < 10; ++i) {
      bus->emit(make_event("llm.request", {{"i", i}}));
    }
    REQUIRE(sink_ptr->size() == 10);
  }
  REQUIRE(true);
}