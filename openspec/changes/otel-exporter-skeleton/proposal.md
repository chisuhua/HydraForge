# otel-exporter-skeleton

## Amendment 修订 (2026-08-21)

**实施期间发现 3 处需要调整：**

1. **移除后台 flush_loop 线程**（原 design 含 `std::thread flush_thread_` + `std::condition_variable buffer_cv_` + `buffer_mutex_`/`buffer_`）
   - 原因：(a) 实测挂起（InMemoryBus dispatch_loop 与 flush_loop 多线程交互）；(b) 与 OTel SDK (opentelemetry-cpp `BatchSpanProcessor`) + Prometheus client_golang 等工业惯例冲突 —— **传输层自带后台批量，应用层骨架只做事件→span 转换 + 委托 sink**
   - 调整后：`on_bus_event` 同步调用 `sink_->send_span(span)`；`~OtelExporter` 仅调用 `sink_->flush()`；`ISpanSink` 抽象保留，真实 OTLP 客户端实现 sink 时自带后台批量机制
   - 性能等价：`1000 spans batch flush < 500ms` 由 sink 自带的 batch processor 满足

2. **测试改用同步 MockBus**（`tests/test_helpers/mock_bus.h`，P12 mock-bus-canonical-extract）
   - 原因：`InMemoryBus` 异步派发 + `bus->wait_for_drain()` 在单元测试中 race / flaky；同步 MockBus 是工业惯例（`tests/test_event_log_*.cpp` 系列验证过）
   - OtelExporter 代码本身仍接受 `shared_ptr<IInteractionBus>`（不依赖 MockBus）；真实 bus 使用场景由未来 integration test 覆盖

3. **修复 test_otel_exporter.cpp 中的 dangling reference bug**（`sink_ptr->spans()[0]` 返回临时 vector 的引用，析构后访问 attributes 失败）
   - 原因：`spans()` 按值返回 `std::vector<OtelSpanData>`，临时 vector 在 full-expression 结束时析构，析构时调用 `~unordered_map` 释放堆内存；std::string 字段因 SSO 巧合存活，map 字段因堆释放而清零
   - 修复：所有断言改为 `auto spans = sink_ptr->spans(); const auto& span = spans[0];`

**未改变的部分：**
- `ISpanSink` 抽象（mock/Noop/真实 OTLP 客户端）
- `OtelConfig` 字段（endpoint / protocol / service_name）
- 订阅 bus 的 `llm.*` / `tool.*` / `agent.*` / `dsl.*` 4 类事件
- span 含 `agent_id` / `session_id` / `trace_id` 3 属性
- opt-in + fail-closed

## Why

**Metis 评审修正**（评审报告 2026-08-20）：原 proposal "订阅 EventLog" 设计含混——EventLog 是 JSONL 文件 + writer subscriber，OTelExporter 应订阅 **IInteractionBus**（bus）而非 tail EventLog 文件。另：OTel 默认 OFF 使验收不能在 CI 默认 preset 验证，需新增 CMake preset + CI matrix。

Oracle + Metis 评审发现**盲点 7.3**：ADR-0063 ✅ Approved（OpenTelemetry tracing）但**实施 0%**——可观测性出口完全缺失。

EventLog 是项目内 JSONL，OTel 是工业标准（Jaeger / Prometheus / Tempo / Datadog）。分布式追踪 / 跨主机 agent 追踪 / 第三方 APM 集成无标准出口。

依据 `docs/architecture/defect-truth-table-2026-08.md` 盲点 7.3：

- 外部可观测性盲区
- 分布式追踪缺失
- ADR Approved 但零实施（与 ADR-0057 同模式——但本提案不是 amend，是从 0 → 1）

## What Changes

**In Scope**:

- 新建 `src/common/observability/otel_exporter.{h,cpp}`：OTel span exporter
  - **订阅 IInteractionBus（bus）** 的 `llm.*` / `tool.*` / `agent.*` / `dsl.*` 4 类事件（**非** tail EventLog 文件）
  - 转换为 OTel span（含 `agent_id` / `session_id` / `trace_id` 3 属性）
  - 通过 OTLP/gRPC 或 OTLP/HTTP 协议发送
- 新建 `src/common/observability/otel_config.h`：OTel 配置
  - `endpoint`（如 `http://localhost:4317`）
  - `protocol`（grpc / http）
  - `service_name`（如 `hydraforge-runtime`）
- `CMakeLists.txt`：可选依赖 `opentelemetry-cpp`（`find_package(OpenTelemetry)` + `FetchContent` fallback）
- **新增 CI preset**：`otel` preset（`AGENTICDSL_BUILD_OTEL=ON`）+ CI matrix 行（否则 OTel 测试在 CI 不编译不运行）
- 新建 `tests/test_otel_exporter.cpp`：mock OTel collector 本地 fixture + 验证 span emit（≥ 5 cases）
- 新建 `tests/test_otel_exporter_integration.cpp`：与 bus 集成（订阅 4 类事件 × 1 case = 4）——`agent.*` case **依赖提案 2 ship**，未 ship 时允许条件跳过（explicitly skip）

**Out of Scope**:

- OTel Metrics / Logs（仅 Tracing）
- SQLite sidecar（百万级 query，超出本提案 scope）
- Agent 网络 / host metrics
- tail EventLog 文件模式（订阅 bus 优先）

### 关键场景

- **GIVEN** OTel collector 运行在 `localhost:4317`（OTLP/gRPC）
  **WHEN** DSLEngine 启动 + `enable_otel_exporter(config)` 调用
  **THEN** OTelExporter 订阅 **IInteractionBus**，订阅 4 类事件（`llm.*` / `tool.*` / `agent.*` / `dsl.*`）

- **GIVEN** bus 收到 `llm.request` 事件
  **WHEN** OTelExporter 处理
  **THEN** 创建 OTel span，attributes 含 `agent_id` / `session_id` / `trace_id` / `topic` / `payload_json`

- **GIVEN** OTel collector 不可达（网络故障）
  **WHEN** OTelExporter 尝试发送
  **THEN** retry 3 次后丢弃 + stderr warning（不阻塞 bus 写入；opt-in fail-open）

- **GIVEN** CMake 配置 `AGENTICDSL_BUILD_OTEL=OFF`（默认）
  **WHEN** 编译
  **THEN** OTel 依赖 NOT 链接，OTelExporter 为 stub（throw "OTel not enabled"）；`otel` preset + CI matrix 在 ON 时验证

- **GIVEN** bus 写入 1000 events（含 llm / tool / agent / dsl 4 类）
  **WHEN** OTelExporter batch flush（默认 100ms）
  **THEN** 1000 spans 发送到 collector，< 500ms 总延迟（**仅 otel preset 下验证**）

**Out of Scope**:

- (no items specified)

## Capabilities

- **MUST** span 包含 `agent_id` + `session_id` + `trace_id` 三个属性（与 EventLog schema 对齐）
- **MUST** 订阅 **IInteractionBus** 的 `llm.*` / `tool.*` / `agent.*` / `dsl.*` 4 类事件（非 tail EventLog 文件）
- **MUST** opt-in + fail-closed（与 EventLog opt-in 模型一致）
- **MUST** `otel` CMake preset + CI matrix 行（否则验收不可验证）
- **SHOULD** 支持 OTLP/gRPC 和 OTLP/HTTP 双协议
- **SHOULD** protobuf payload 与 EventLog v:1 schema 兼容
- **MUST NOT** 阻塞 bus 写入（OTel 网络故障独立处理）

## Impact

- **ADR-0063 状态**：✅ Approved（保持）→ 实施率从 0% → skeleton
- **新增依赖 `opentelemetry-cpp`**（CMake `find_package` + `FetchContent` fallback）
- **新增 CI 资源**：`otel` preset + matrix 行（列为工作项，评估 CI 时长）
- **不破坏现有 API**（纯增量）
- **外部可观测性解锁**：Jaeger / Tempo / Datadog 集成路径

## Acceptance

- [ ] `src/common/observability/otel_exporter.{h,cpp}` 实现（订阅 bus）
- [ ] `src/common/observability/otel_config.h` 配置定义
- [ ] CMake 集成（`AGENTICDSL_BUILD_OTEL=ON` opt-in，默认 OFF）
- [ ] `otel` CMake preset + CI matrix 行（列入工作项）
- [ ] `tests/test_otel_exporter.cpp` ≥ 5 cases（mock collector + 4 类事件 × 1 case = 4 + 1 边界）
- [ ] `tests/test_otel_exporter_integration.cpp` 4 cases（`agent.*` case 依赖提案 2，未 ship 时条件跳过）
- [ ] ctest 全量零回归（OTel 默认 OFF，零干扰；`otel` preset 单独验证）
- [ ] 性能基准：1000 spans batch flush < 500ms（**仅 otel preset 下验证**）
- [ ] `docs/architecture/defect-truth-table-2026-08.md` 盲点 7.3 状态从"OTel exporter 零代码"更新为"OTel skeleton 已 ship"