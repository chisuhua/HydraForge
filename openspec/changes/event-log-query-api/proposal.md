# event-log-query-api

## Why

**Metis 评审修正**（评审报告 2026-08-20）：原 proposal 使用了 C++23 `std::expected`（项目 C++20，`CMakeLists.txt:13` `CMAKE_CXX_STANDARD 20`），且内部 `std::optional<ErrorCode>` 与 `std::expected<BusEvent, ErrorCode>` 自相矛盾。修正版统一为项目已有 `Result<T, LLMError>` 范式（`llm_types.h`）。

ADR-0080 v1.1 ✅ Approved D5 定义 EventLog API：

- `emit(event)` ✅ 已 ship
- `flush_sync()` ✅ 已 ship
- `read(agent_id, log_dir)` ✅ 已 ship（`src/core/event_log.h:47`，static 方法）
- `query(filter, max_count)` ❌ **stub**

EventLog 现有签名：`static std::vector<BusEvent> read(const std::string& agent_id, const std::filesystem::path& log_dir)`（按代理 ID + 日志目录读取全部事件，无时间窗过滤）。

**缺的是 query API**（按 topic / agent_id / time window 过滤 + 排序 + 分页）。离线分析闭环断裂：跨会话事件聚合、"本月 tool 执行统计"、agent 学习（训练数据 export）、trajectory replay 等场景全部依赖 query API。

## What Changes

**In Scope**:

- `src/core/event_log.cpp`：
  - `read(agent_id, start_ts, end_ts)` 实现（按 `causal_time` 时间窗读取 JSONL）
  - `query(filter, max_count)` 实现（内存中按 predicate 过滤）
- `src/core/event_log.h`：API 完整化（返回 `std::vector<BusEvent>` + `std::optional<ErrorCode>` 错误路径）
- `src/core/types/event_log_config.h`：扩展 query 相关字段（`default_time_window` / `max_result_count`）
- 新建 `tests/test_event_log_query.cpp`：read 路径（5 cases）+ query 路径（5 cases）+ 空结果 + 大结果集
- 新建 `tests/perf/test_event_log_query_perf.cpp`（**注意**：`tests/perf/` 目录不存在，需新建，列入工作项）：10000 events query 性能基准（目标 < 100ms）

**Out of Scope**:

- SessionWriter 与 EventLogWriter 并行订阅集成（提案 `session-writer-bridge`）
- SQLite sidecar（百万级事件，超出本提案 scope）
- OTel exporter（提案 `otel-exporter-skeleton`）

### 关键场景

- **GIVEN** EventLog 已写入 10000 events（topic/agent_id/session_id/causal_time 各异）
  **WHEN** 调用 `query(filter=[topic="llm.request", agent_id="agent-abc"], max_count=1000)`
  **THEN** 返回 ≤ 1000 events，按 `causal_time` 排序，< 100ms

- **GIVEN** EventLog 文件存在，JSONL 格式正确
  **WHEN** 调用 `read(agent_id="agent-abc", start_ts=0, end_ts=UINT64_MAX)`
  **THEN** 返回该 agent_id 全部 events，按 causal_time 排序

- **GIVEN** EventLog 文件不存在或 agent_id 不存在
  **WHEN** 调用 `read(agent_id="nonexistent")`
  **THEN** 返回空 vector（不抛异常）

- **GIVEN** EventLog 文件存在但格式损坏（partial line）
  **WHEN** 调用 `read(...)`
  **THEN** 返回损坏行之前 events + 错误日志（continue parsing）

- **GIVEN** query filter 含复杂 predicate（如 `topic="llm.*" AND causal_time > 1000`）
  **WHEN** 调用 `query(filter)`
  **THEN** 支持 glob 通配 + 比较运算 + 复合 predicate

**Out of Scope**:

- (no items specified)

## Capabilities

- **MUST** `read(agent_id, start_causal_time, end_causal_time)` 按因果时间窗读取（`causal_time` 而非 `ts_wall`，避免 bus 顺序问题）
- **MUST** `query(filter, max_count)` 内存过滤，max_count 默认 1000
- **MUST** 返回 `std::vector<BusEvent>` + `std::optional<ErrorCode>` 错误输出（非 `std::expected`）
- **MUST NOT** 使用 C++23 特性（`std::expected` 被禁止）
- **MUST NOT** 修改现有 `static read(agent_id, log_dir)` 签名
- **MUST NOT** 修改 write 路径（`emit` / `flush_sync` 行为不变）
- **SHOULD** 大日志场景（> 100k events）提供性能提示（warn 到 stderr，但不阻塞）
- **SHOULD** 支持 glob 通配（topic glob + agent_id glob）

## Impact

- **性能基础设施**：`tests/perf/` 需新建，Catch2 BENCHMARK 需启用（列入工作项）
- **与 ADR-0080 v1.1 D12 兼容**：opt-in + fail-closed 保持
- **解锁下游**：提案 `session-writer-bridge`（集成测试需 query API）

## Acceptance

- [ ] `read()` 成员方法实现（按时间窗过滤，非 static）
- [ ] `query()` 实现 + filter 表达式支持（topic / agent_id / time window / glob）
- [ ] `tests/test_event_log_query.cpp` ≥ 10 cases
- [ ] 新建 `tests/perf/test_event_log_query_perf.cpp` ≥ 3 cases（1k / 10k / 100k events）
- [ ] 性能基准：10000 events query < 100ms（Catch2 BENCHMARK）
- [ ] 现有 `static read(agent_id, log_dir)` 签名不改
- [ ] 编译通过（C++20，无 std::expected）
- [ ] ctest 全量零回归
- [ ] `docs/architecture/defect-truth-table-2026-08.md` 缺陷 2.1 状态更新