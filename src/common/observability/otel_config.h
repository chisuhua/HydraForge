// src/common/observability/otel_config.h
// 功能描述：OTel 配置（ADR-0063 OpenTelemetry tracing, P11 otel-exporter-skeleton）
//          endpoint / protocol / service_name
//          opt-in + fail-closed（与 EventLog opt-in 模型一致）
// 设计依据：openspec/changes/otel-exporter-skeleton (P11)
// 作者：HydraForge Sprint 22 P11 ship
// 最后修改日期：2026-08-20

#pragma once

#include <string>

namespace agenticdsl {

enum class OtelProtocol { kGrpc, kHttp };

struct OtelConfig {
  bool otel_enabled = false;             // 默认关，向后兼容
  std::string endpoint = "http://localhost:4317";  // OTLP 端点
  OtelProtocol protocol = OtelProtocol::kGrpc;
  std::string service_name = "hydraforge-runtime";

  // 批量 flush
  int batch_max_spans = 128;
  int flush_interval_ms = 100;
  int max_retries = 3;                   // collector 不可达时重试次数
};

}  // namespace agenticdsl