// src/common/observability/otel_exporter.h
// 功能描述：OTel span exporter 骨架（P11 otel-exporter-skeleton, ADR-0063）
//          订阅 IInteractionBus 的 llm.* / tool.* / agent.* / dsl.* 4 类事件，
//          转换为 OTel span（含 agent_id / session_id / trace_id 属性），
//          通过 ISpanSink 发送（默认 NoopSink，可注入 mock sink 测试）。
//
//          Amendment (2026-08-21): 移除后台 flush_loop 线程 + buffer_cv_
//          理由: (1) 后台线程 + InMemoryBus 异步派发 + cv.wait_for 多线程交互
//                实测导致挂起（dispatch_loop 与 flush_loop 等待状态竞态）
//          (3) OTel SDK (opentelemetry-cpp BatchSpanProcessor) 与 Prometheus
//                client_golang 等工业惯例: 传输层自带后台批量 + 重试，应用层
//                骨架仅做事件→span 转换 + 委托 sink.send()
//                与 ADR-0063 §"OTelExporter 是应用层"语义对齐
//
// 设计依据：openspec/changes/otel-exporter-skeleton (P11)
// 作者：HydraForge Sprint 22 P11 ship
// 最后修改日期：2026-08-21

#pragma once

#include "agenticdsl/contract/bus_event.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "otel_config.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace agenticdsl {

// Span 数据（与 Otel span 概念对齐，含 agent_id / session_id / trace_id）
struct OtelSpanData {
  std::string name;           // 派生自 event.topic
  std::string trace_id;
  std::string span_id;
  std::string parent_span_id;
  std::int64_t start_time_ms;
  std::int64_t end_time_ms;
  std::string agent_id;
  std::string session_id;
  std::string topic;          // 原始事件 topic
  std::string payload_json;   // 事件 payload 的 JSON 字符串
  std::unordered_map<std::string, std::string> attributes;
};

// 传输层抽象接口（可注入: mock / Noop / 真实 OTLP）
//
// 实现可自由选择: 同步（如 InMemorySpanSink 用于测试）或异步
// （如 opentelemetry-cpp OTLP exporter + BatchSpanProcessor，自带后台线程）。
class ISpanSink {
 public:
  virtual ~ISpanSink() = default;
  virtual void send_span(const OtelSpanData& span) = 0;
  virtual void flush() = 0;
};

// Noop sink（默认，不发送任何东西）
class NoopSpanSink : public ISpanSink {
 public:
  void send_span(const OtelSpanData& /*span*/) override {}
  void flush() override {}
};

// 内存 sink（测试用，记录到 vector，同步 push）
class InMemorySpanSink : public ISpanSink {
 public:
  void send_span(const OtelSpanData& span) override {
    std::lock_guard<std::mutex> lock(mutex_);
    spans_.push_back(span);
  }
  void flush() override {}
  std::vector<OtelSpanData> spans() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return spans_;
  }
  size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return spans_.size();
  }
 private:
  mutable std::mutex mutex_;
  std::vector<OtelSpanData> spans_;
};

// OTel span exporter（订阅 bus，按 topic 过滤，转换 span，发送给 sink）
//
// Amendment (2026-08-21): 无后台线程，on_bus_event 同步调用 sink.send_span()
// 由 sink 自行决定是否异步批量（如 OTel SDK BatchSpanProcessor）。
class OtelExporter {
 public:
  OtelExporter(OtelConfig config,
               std::shared_ptr<IInteractionBus> bus,
               std::unique_ptr<ISpanSink> sink = nullptr);
  ~OtelExporter();

  OtelExporter(const OtelExporter&) = delete;
  OtelExporter& operator=(const OtelExporter&) = delete;

  // 强制同步落盘（委托给 sink.flush()）
  void flush_sync();

  // 获取当前 sink（测试用）
  ISpanSink* sink() const { return sink_.get(); }

 private:
  void subscribe_to_bus();
  void on_bus_event(const BusEvent& event);
  bool is_watched_topic(const std::string& topic) const;
  OtelSpanData event_to_span(const BusEvent& event);
  std::string next_span_id();

  OtelConfig config_;
  std::shared_ptr<IInteractionBus> bus_;
  std::unique_ptr<ISpanSink> sink_;
};

}  // namespace agenticdsl