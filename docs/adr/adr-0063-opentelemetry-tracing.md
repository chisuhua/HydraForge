# ADR-0063: OpenTelemetry / Distributed Tracing 集成

## 状态

✅ Approved (2026-07-16, 架构评审确认)

## 领域

Agent-as-Plugin 架构 / 可观测性

## 关联

- `include/agenticdsl/types/trace_record.h` — 内部 TraceRecord（已有）
- [ADR-0019 — IInteractionBus](../adr-0019-iinteraction-bus-mvp.md) — 进程内事件总线
- [ADR-0059 — Cross-Process Protocol](./adr-0059-cross-process-protocol.md) — 跨进程协议
- [ADR-0060 — Agent 组合协议](./adr-0060-agent-composition.md) — 6 种协作模式
- [ADR-0064 — PDK Conformance Test Suite](./adr-0064-pdk-conformance-test-suite.md) — Level 3 性能 baseline
- MCP SEP-414: `traceparent` 标准化
- W3C Trace Context

## 背景

### 问题

HydraForge 内部有 `TraceRecord` 数据结构（`include/agenticdsl/types/trace_record.h`），但：

- 没有标准化的导出格式
- 无法与外部 observability 工具集成（Jaeger / Zipkin / Datadog / Honeycomb）
- 跨进程 Agent 调用无 trace propagation
- Marketplace 包的可观测性缺失（ADR-0064 Level 3 需要）
- 工具调用 args/value 不透明（仅 keys），缺乏 trace correlation

### 目标

集成 OpenTelemetry 标准，让 HydraForge 的 trace 可被任何 OTel 兼容工具消费。

## 决策

### 决策 1 — OpenTelemetryExporter

```cpp
class OpenTelemetryExporter : public TraceSink {
public:
    // 1. 启动 span
    std::shared_ptr<Span> start_span(const std::string& name, 
                                     const SpanContext& parent = {});
    
    // 2. 添加属性
    void set_attribute(const std::string& key, const AttributeValue& value);
    
    // 3. 添加事件
    void add_event(const std::string& name, const Attributes& attrs);
    
    // 4. 记录异常
    void record_exception(const std::exception& e);
    
    // 5. 关闭 span
    void end_span();
    
    // 6. 导出到 OTel Collector
    void export_to_collector(const std::string& endpoint = "http://localhost:4318");
};
```

**实现**：
- 基于 opentelemetry-cpp SDK
- BatchSpanProcessor（异步批量导出）
- OTLP HTTP/gRPC 协议
- 不重写内部 `TraceRecord`，仅作为 sink

### 决策 2 — W3C Trace Context 传播

**HTTP `traceparent` header**（W3C Trace Context 标准）：

```
traceparent: 00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01
             ^--version ^--trace-id (16 bytes)   ^--parent-id (8 bytes) ^--flags
```

**传播路径**：

| 场景 | 传播方式 |
|------|---------|
| 进程内 | `IInteractionBus` 在消息 payload 中携带 trace context |
| 跨进程 | MCP `_meta.traceparent`（与 MCP SEP-414 对齐） |
| 跨 Agent (call_tool/delegate/parallel) | 自动注入到 args |

**实现**：
```cpp
// RemoteAgentAdapter::call_remote() 自动注入 traceparent
nlohmann::json RemoteAgentAdapter::call_remote(
    const std::string& agent_id,
    const std::string& tool_name,
    const nlohmann::json& args
) {
    auto span = exporter->start_span("tool.call.remote");
    span->set_attribute("agent.id", agent_id);
    span->set_attribute("tool.name", tool_name);
    
    // 注入 W3C traceparent
    auto traceparent = exporter->inject_context(span->context());
    auto args_with_trace = args;
    args_with_trace["_meta"]["traceparent"] = traceparent;
    
    auto result = mcp_client->call(args_with_trace);
    span->end();
    return result;
}
```

### 决策 3 — 5 类核心 Span

| Span | 何时创建 | 关键属性 | 文件位置 |
|------|---------|---------|---------|
| `agent.run` | Agent 入口（任何 6 种协作模式） | agent_id, form, version, capability_tags | `OpenTelemetryExporter::start_agent_span` |
| `tool.call` | `call_tool` 调用 | tool_name, args_keys (not values), duration_ms, ok | `IToolRegistry::call_tool` 入口 |
| `llm.generate` | `ILLMProvider::generate` | model, prompt_tokens, completion_tokens, cached, temperature | `ILLMProvider::generate` 入口 |
| `bus.emit` | `IInteractionBus::emit` | topic, subscriber_count, payload_size | `IInteractionBus::emit` 入口 |
| `budget.consume` | `IBudgetController::try_consume_*` | amount, remaining_usd, scope, success | `IBudgetController` 入口 |

**Span 嵌套规则**：
```
agent.run
  ├── tool.call (1)
  │     └── llm.generate (1.1)
  ├── tool.call (2)
  │     ├── tool.call (2.1, 远程)
  │     └── tool.call (2.2, 远程, fork-join)
  └── bus.emit
```

### 决策 4 — 工具 args 安全处理

```cpp
// 仅记录 args 的 keys（防御敏感数据泄露，符合 ADR-0031 audit events 约定）
void set_tool_args_span(Span& span, const nlohmann::json& args) {
    nlohmann::json keys = nlohmann::json::array();
    for (auto it = args.begin(); it != args.end(); ++it) {
        keys.push_back(it.key());
    }
    span->set_attribute("tool.args_keys", keys);
}
```

**禁止记录**：
- args value（可能含密钥、PII）
- env var（API keys）
- file contents

### 决策 5 — 与 ADR-0064 Conformance Level 3 集成

```cpp
class ConformanceLevel3 {
    bool check_performance_baseline(const PluginHandle& plugin) {
        // 1. 运行 100 次标准用例
        auto traces = run_benchmark(plugin, test_cases, 100);
        
        // 2. 导出到 OTel
        for (auto& trace : traces) {
            exporter->export_trace(trace);
        }
        
        // 3. 计算 p99 latency
        auto p99 = compute_p99_latency(traces);
        
        // 4. 对比 baseline
        return p99 < BASELINE_THRESHOLD;
    }
};
```

### 决策 6 — Marketplace 包默认启用

```json
// manifest.json
{
  "observability": {
    "otel_enabled": true,             // 默认启用
    "endpoint": "http://localhost:4318",
    "sample_rate": 1.0,                // 100% 采样
    "export_format": "otlp+http"
  }
}
```

**用户可关闭**：通过环境变量 `HYDRAFORGE_OTEL_DISABLED=1`。

## 替代方案

### 方案 A：自研 trace 协议

**否决理由**：
- 重复造轮子
- 无法与外部工具集成

### 方案 B：只支持 Jaeger

**否决理由**：
- Jaeger 已支持 OTLP
- 直接走 OTel 更通用

### 方案 C：traceparent 走自定义 header

**否决理由**：
- 与 W3C 标准不一致
- 与 MCP SEP-414 不兼容

## 不变量

- 工具 args value 不写入 span attribute（仅 keys）
- env var / API key 不写入 span
- file contents 不写入 span
- 进程内 + 跨进程 trace ID 保持一致

## 权衡

| 决策 | 选择 | 理由 |
|------|------|------|
| 标准 | OpenTelemetry | 工业标准，工具最全 |
| 传播 | W3C Trace Context | 跨生态兼容 |
| 数据安全 | 仅 keys 不存 values | 防御 secret 泄露 |
| 默认 | 启用 | Marketplace 包可观测性 |
| 实现 | 包装内部 TraceRecord | 不重写已有 |

## 后续行动

- ADR-0065: 多语言 PDK 支持（trace 跨语言兼容）
- 集成 Sprint 18 P1.T3 execution_session.cpp 改造
- 文档：`docs/observability/` 增加 trace 查询示例

## 参考

- ADR-0019 / 0059 / 0060 / 0064
- OpenTelemetry: opentelemetry.io
- W3C Trace Context: w3.org/TR/trace-context
- MCP SEP-414: `traceparent` 标准化
- Jaeger OTLP: jaegertracing.io/docs/1.50/apis/
- SW4RM Agentic Protocol: sw4rm.ai/protocol/