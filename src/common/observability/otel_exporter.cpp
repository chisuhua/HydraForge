// src/common/observability/otel_exporter.cpp
// 功能描述：OTel span exporter 实现（P11 otel-exporter-skeleton, ADR-0063）
//          订阅 bus 的 llm.* / tool.* / agent.* / dsl.* 4 类事件，
//          转换为 OtelSpanData（含 agent_id / session_id / trace_id），
//          通过 ISpanSink 发送（默认 NoopSink，可注入 mock sink 测试）。
//
//          Amendment (2026-08-21): 移除后台 flush_loop 线程
//          on_bus_event 同步调用 sink.send_span()，由 sink 自管批量/重试/flush
//
// 设计依据：openspec/changes/otel-exporter-skeleton (P11)
// 作者：HydraForge Sprint 22 P11 ship
// 最后修改日期：2026-08-21

#include "otel_exporter.h"
#include "otel_config.h"

#include "agenticdsl/contract/bus_event.h"
#include "agenticdsl/contract/iinteraction_bus.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>
#include <unordered_set>

namespace agenticdsl {

namespace {

std::int64_t now_unix_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::string hex_random() {
  static thread_local std::mt19937_64 gen{std::random_device{}()};
  std::uniform_int_distribution<std::uint64_t> dis;
  std::ostringstream oss;
  oss << std::hex << dis(gen) << dis(gen);
  return oss.str();
}

// 4 类 watched topic 前缀
const std::unordered_set<std::string>& watched_prefixes() {
  static const std::unordered_set<std::string> prefixes = {
      "llm.", "tool.", "agent.", "dsl."
  };
  return prefixes;
}

}  // namespace

bool OtelExporter::is_watched_topic(const std::string& topic) const {
  for (const auto& prefix : watched_prefixes()) {
    if (topic.find(prefix) == 0) return true;
  }
  return false;
}

std::string OtelExporter::next_span_id() {
  static std::atomic<std::uint64_t> counter{0};
  auto n = counter.fetch_add(1);
  return "span-" + std::to_string(now_unix_ms()) + "-" + std::to_string(n);
}

OtelSpanData OtelExporter::event_to_span(const BusEvent& event) {
  OtelSpanData span;
  span.name = event.topic;
  span.topic = event.topic;
  span.trace_id = event.payload.trace_id.has_value()
                      ? *event.payload.trace_id
                      : "trace-" + hex_random();
  span.span_id = next_span_id();
  span.start_time_ms = now_unix_ms();
  span.end_time_ms = span.start_time_ms;

  // 从 payload data 提取 agent_id / session_id
  if (event.payload.data.is_object()) {
    if (event.payload.data.contains("agent_id") &&
        event.payload.data["agent_id"].is_string()) {
      span.agent_id = event.payload.data["agent_id"];
    }
    if (event.payload.data.contains("session_id") &&
        event.payload.data["session_id"].is_string()) {
      span.session_id = event.payload.data["session_id"];
    }
    span.payload_json = event.payload.data.dump();
  }

  span.attributes["topic"] = event.topic;
  span.attributes["trace_id"] = span.trace_id;
  span.attributes["agent_id"] = span.agent_id;
  span.attributes["session_id"] = span.session_id;

  return span;
}

OtelExporter::OtelExporter(OtelConfig config,
                           std::shared_ptr<IInteractionBus> bus,
                           std::unique_ptr<ISpanSink> sink)
    : config_(std::move(config)),
      bus_(std::move(bus)),
      sink_(std::move(sink) ? std::move(sink)
                             : std::make_unique<NoopSpanSink>()) {
  if (bus_) {
    subscribe_to_bus();
  }
}

OtelExporter::~OtelExporter() {
  // Amendment (2026-08-21): 无后台线程，无需 join。
  // 仅 sink.flush() 让异步 sink（如 OTel SDK）强制落盘。
  if (sink_) {
    sink_->flush();
  }
}

void OtelExporter::subscribe_to_bus() {
  if (!bus_) return;
  bus_->subscribe("*", [this](const BusEvent& event) {
    on_bus_event(event);
  });
}

void OtelExporter::on_bus_event(const BusEvent& event) {
  if (!is_watched_topic(event.topic)) return;
  OtelSpanData span = event_to_span(event);
  sink_->send_span(span);
}

void OtelExporter::flush_sync() {
  if (sink_) {
    sink_->flush();
  }
}

}  // namespace agenticdsl