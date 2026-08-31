# ADR-0080: AppendOnlyEventLog as Core Fact Source

## 状态
✅ Approved（v1.1 amendment 2026-08-12，原始 v1 文本 2026-01-19）

> **修订记录**：v1.1 amendment 针对 Agent 蒸馏与自进化的数据需求修订 D2/D6/D9，
> 追加 D10（模型可见字节条件捕获）与附录 E（消费侧边界）。**v1 决策不变**。

> **⏳ tracking: pending — ADR-TRACKING-01 豁免 (B6 修正, 2026-08-31)**: ADR-0080 ✅ Approved 24h+ 无 tracking OpenSpec change。理由: EventLog + EventBuilder + CaptureMode + IDistillationWriter (Phase 1) 全部 ship 经多 change  (capcap § §一 # #31 Distillation Data 1 ship 2026-08-29; ADR-0068 v1.2.2 已注册 27+ 主题含 bus_event bus + capture_mode capture 相关; 阶段 1 Phase 2 deferred per v1.1 amendment D10). 待 Phase 2 启动时补 `2026-09-XX-adr-0080-eventlog-phase-2` tracking change. 详见 `tools/adr_lint.py` 警告关联 commit `1f25821`.

## 上下文

### 为什么需要 EventLog

当前所有事件通过 `IInteractionBus::emit()` 发射，但 bus 本身**无持久化**：
- 重启后所有事件丢失
- 无法进行 crash 后 trace 重放
- 无法支持跨会话事件聚合（如：统计本月 tool 执行次数）
- 无法支撑 agent 学习（因为无历史事件可分析）

### EventLog 的目标

EventLog 作为**追加-only 的事件日志**，提供：
1. 持久化：所有事件写入 JSONL 文件
2. 原子性：每条事件一次 write + fsync
3. 审计：crash 后可从日志恢复完整执行轨迹
4. 分析：支持离线 query 和 agent 学习

### ADR-0068 的事件基础设施

ADR-0068 定义了：
- `BusEvent` 结构（topic + ToolResult payload）
- `EventBuilder` 链式构造
- 主题命名约定 `<module>.<verb>`
- 8 个 operation-result 事件已迁移

**限制**：ADR-0068 只定义了**事件结构**，未定义**持久化格式**。

## 决策

### 决策 D1：EventLog 物理形态

**采用**：单目录单文件 per agent，append-only JSONL。

**路径**：`~/.hydraforge/event_log/<agent_id>.v1.jsonl`

**文件命名**：
```
<agent_id>.v1.jsonl         # 主事件日志
<agent_id>.v1.jsonl.rotation  # rotation 后的旧日志
```

**优势**：
- 与 Session 文件分离（EventLog = 高频事件流，Session = 消息 + 结构元数据）
- 每个 agent 独立日志，避免不同 agent 事件混杂
- 单文件 append + fsync 简单可靠

### 决策 D2：Schema v1

EventLog 行格式：

```jsonl
{"v":1, "event_id":"evt-<ts>-<counter>", "ts":1737281400, "topic":"llm.request", "agent_id":"agent-abc", "session_id":"ses-xyz", "payload":{...}}
```

**字段说明**：

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `v` | int | ✅ | Schema 版本（当前 = 1） |
| `event_id` | string | ✅ | 全局唯一事件 ID |
| `ts_wall` | int64 | ✅ | Unix ms（`system_clock`，由 EventLogWriter.emit() 在落盘边界取，**不是** BusEvent.timestamp） |
| `causal_time` | uint64 | ✅ | ADR-0037 CausalClock 值（复制自 BusEvent，已由 `inmemory_bus.cpp:62` 自动填充） |
| `topic` | string | ✅ | 事件主题（`module.verb` 格式） |
| `agent_id` | string | ✅ | 产生事件的 agent 实例 ID |
| `session_id` | string | ❌ | 关联的会话 ID（可能为空） |
| `payload` | json | ✅ | 事件载荷（ADR-0068 EventBuilder 产出） |

**v1.1 字段变更说明**：
- 移除 `ts`（"ISO8601 或 Unix ms"二选一表述与 EventBuilder 实际产出不兼容——`BusEvent.timestamp` 是 `steady_clock` 不可序列化）
- 新增 `ts_wall`（墙钟，仅用于人类可读展示，不参与排序）
- 新增 `causal_time`（参与排序的关键，是 replay 正确性的基础）

**解析原则**：
- 未知字段**忽略**（forward-compatible）
- `v` 字段**必读**（未来版本分发）
- 排序键：`(causal_time, event_id)`

### 决策 D3：EventLog 生效时机

EventLog 在 **2 个地方**写入：

#### 场景 A：系统级 EventLog（启动时写入）

当 `EventLogWriter::start()` 被调用时：
1. 创建 `~/.hydraforge/event_log/` 目录（如果不存在）
2. 创建/打开 `<agent_id>.v1.jsonl` 文件
3. 订阅 bus 全部事件（`bus_->subscribe("*", ...)`）
4. 启动写入线程（异步，buffered write + periodic fsync）

**生命周期**：与 DSLEngine 实例同寿命，engine 销毁时 flush + close。

#### 场景 B：SessionWriter 事件子集（会话级）

SessionWriter（ADR-0079）**也**订阅 bus，但只写：
- `conversation.*` / `attempt.*` / `phase.*` / `step.*` / `execution.*` / `convergence.*`

**分工**：
- EventLog = **全量事件**（高频率，用于审计/分析/学习）
- SessionWriter = **会话结构事件**（低频率，用于恢复/context 构建）

### 决策 D4：写入性能优化

#### 批量写入
EventLog Writer 使用**线程安全队列 + 批量刷写**：
- 事件先入内存队列（lock-free queue）
- 每 100ms 或积累 100 条事件，批量写入磁盘
- 保证**每 100ms 内的事件至少一次 fsync**

**为什么不用 per-event fsync**：
- 高频事件（llm.request + llm.response）可达每秒数十次
- per-event fsync 会严重影响性能（尤其在 HDD 上）
- 批量 fsync 以最多 100ms 事件丢失为代价，换取性能

**crash 恢复保证**：
- 最近 100ms 内的事件可能丢失（可接受：非 mission-critical 场景）
- 如果需要 0 丢失，使用 `EventLog::flush_sync()` 显式同步

#### 文件 Rotation
- 单文件超过 100MB 或 24h → rotation（rename to `.rotation.1`）
- 保留最近 3 个 rotation 文件
- 超出的 rotation 文件自动删除

### 决策 D5：EventLog API

```cpp
class EventLogWriter {
public:
  EventLogWriter(const std::string& agent_id, 
                 const std::filesystem::path& log_dir);
  ~EventLogWriter();
  
  // 写入事件（线程安全）
  void emit(const BusEvent& event);
  
  // 同步刷新（阻塞，等待 fsync 完成）
  void flush_sync();
  
  // 读取事件（离线分析用）
  static std::vector<BusEvent> read(const std::string& agent_id,
                                     const std::filesystem::path& log_dir);
  
  // 查询事件（内存索引，适用于小日志）
  std::vector<BusEvent> query(std::function<bool(const BusEvent&)> filter,
                              size_t max_count = 1000);

private:
  std::string agent_id_;
  std::filesystem::path log_path_;
  std::ofstream file_;
  std::queue<BusEvent> buffer_;
  mutable std::mutex buffer_mutex_;
  std::thread flush_thread_;
  std::atomic<bool> running_;
};
```

**关键约束**：
- `emit()` 是线程安全的（内部加锁）
- `flush_sync()` 阻塞等待所有缓冲事件落盘
- `read()` 是静态方法，不依赖运行中的 writer
- `query()` 是内存中过滤，仅适用于小日志（大日志用数据库或离线工具）

### 决策 D7：Step 0 — BusEvent 信封扩展

在实现 EventLog 之前，需要先扩展 `BusEvent` 结构以支持 Session 路由。

**扩展字段**：
```cpp
struct BusEvent {
  std::string topic;
  ToolResult payload;
  std::chrono::steady_clock::time_point timestamp;
  
  // 新增字段（Step 0）
  std::string session_id{};  // 关联的会话 ID
  std::string agent_id{};    // 产生事件的 agent ID
};
```

**影响**：
- `EventBuilder` 需新增 `.session_id()` 和 `.agent_id()` setter
- 现有事件发射点（~50 处）**默认为空**，不影响功能
- EventLog 和 SessionWriter 依赖这两个字段做路由

**为什么是 Step 0**：
- 没有 `agent_id`，EventLog 无法知道事件属于哪个 agent
- 没有 `session_id`，SessionWriter 无法过滤当前会话事件
- 这是 ADR-0079 和 ADR-0080 的共同前置

### 决策 D8：Step 0 — GenerationRequest.purpose 字段

在 LLM 请求中加入 `purpose` 字段，用于区分不同阶段的 LLM 调用。

```cpp
struct GenerationRequest {
  std::string prompt;
  LLMParams params;
  std::string purpose{"unknown"};  // 新增：react|plan|execute|verify|generate_subgraph|compact|unknown
};
```

**用途**：
- `TracingDecorator` 在 `llm.request` 事件中记录 `purpose`
- 后续 Agent 的 attempt 步骤解析依赖 `purpose` 区分 phase

**ABI 兼容**：`ILLMProvider::generate()` 按 `const GenerationRequest&` 传参，字段新增不影响符号名。

### 决策 D9：Topic 命名约定

| Event Topic | 触发时机 | 文件 |
|---|---|---|
| `llm.request` | LLM 调用开始 | TracingDecorator |
| `llm.response` | LLM 调用返回 | TracingDecorator |
| `llm.token` | 流式 token | stream_to_bus |
| `llm.token.done` | 流式完成 | stream_to_bus |
| `llm.token.error` | 流式错误 | stream_to_bus |
| `tool.execution.start` | 工具调用开始 | NodeExecutor |
| `tool.execution.end` | 工具调用返回 | NodeExecutor |
| `tool.audit.invoked` | 审计点触发 | ToolCoordinator |
| `dsl.call.started` | DSL 节点执行开始 | NodeExecutor |
| `dsl.call.completed` | DSL 节点执行完成 | NodeExecutor |
| `attempt.started` | Agent 循环开始 | PlanExecuteLoop/ReactLoop |
| `attempt.ended` | Agent 循环结束 | PlanExecuteLoop/ReactLoop |
| `phase.completed` | Plan/Execute/Verify 阶段完成 | PlanExecuteLoop |
| `branch.created` | 分支创建 | ForkJoinLoop |
| `execution` | 图执行开始/完成 | TopoScheduler |
| `convergence` | 收敛决策 | ConvergenceManager |
| `conversation.user_message` | 用户输入 | ChatSession |
| `conversation.assistant_message` | Agent 响应 | ChatSession |

**原则**：
- 所有 Topic 遵循 `<module>.<verb>` 格式
- Topic 不含 agent_id / session_id（通过 event 字段传递）
- EventLog 和 SessionWriter 通过 `payload` 字段区分事件类型

**v1.1 修订**：新增 `llm.token` / `llm.token.done` / `llm.token.error` 三行（v1 时已 ship 于 `stream_to_bus.cpp`，D9 表漏列，v1.1 补齐以保证 EventLog 订阅 `"*"` 时不漏记）

---

### 决策 D10：模型可见字节的条件持久化（Distillation Capture）— v1.1 追加

**背景**：v1 的 `TracingDecorator::emit_request`（`src/common/llm/tracing_decorator.cpp:48-51`）仅记录 `prompt_hash`，`emit_response` 仅记 token 计数与耗时——意味着 agent 运行三个月后 EventLog 里**没有任何可蒸馏的字节**。DSH 的核心原则："模型可见字节是唯一不可替代的核心"，在我们这里恰恰是**不落盘的那部分**。本决策填补此缺口。

#### D10.1 `llm.request` 增强字段

| 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|
| `prompt_text` | string | **off** | 完整装配后 prompt（含 system prompt） |
| `available_tools_schema` | json array | **off** | 决策时模型可见的工具 schema 快照 |
| `system_prompt_source` | string | **on** | 来源追踪（如 `"cli:--system-prompt"` / `"config:default"`） |
| `params` | json | **on** | `LLMParams` 全量（temperature / seed / top_p / ...）— **replay 确定性必须** |

#### D10.2 `llm.response` / `llm.token.done` 增强字段

| 字段 | 类型 | 说明 |
|---|---|---|
| `response_text` | string | 非流式路径必填；流式路径由 `llm.token` 聚合重建，记 `text_hash` 校验 |

#### D10.3 开关与隐私

```cpp
struct EventLogConfig {
  // ... D4 原有字段 ...
  bool capture_prompt_bytes = false;  // 默认关，向后兼容
  // 开启后：
  //   max_file_size 默认 1GB（vs D4 的 100MB）
  //   rotation 保留数从 3 提到 10（蒸馏数据宁可多留）
};
```

`capture_prompt_bytes=false` 时行为与 v1 完全一致（仅 hash），**无需 scrubbing**（无字节落盘）。
开启后依赖 ADR-0081 pre-step hook 在 emit 前 scrub（先 scrub 后落盘）——**未 ship 时不暴露**。

**fail-closed**：未配置 `agent_id` = EventLog 不启用（见 D6 修订）。

#### D10.4 性能预算与边界

| 字段 | 上限 | 超限行为 |
|---|---|---|
| `prompt_text` | 64 KB | ERROR log + 丢弃该字段，其余字段仍记录 |
| `response_text` | 1 MB | 同上 |
| `llm.token` | 不适用（每 token 一事件） | — |
| `available_tools_schema` | 256 KB | 同上 |

**不变量新增**：
- D10.5：**capture-off 默认**：所有 v1 测试与默认 examples 不开启 `capture_prompt_bytes`，行为零变化。

#### D10.6 与现有 audit 防线的协调

`tool.execution.start/end` payload 仅含 `{tool, layer, ok, duration_ms}`（`tool_coordinator.cpp:82-110`），不落 args 值——这是 ADR-0031 audit 防线的体现。

D10 不涉及 tool args 落地（属 Phase 2 范畴）。V1 可从 prompt 字节中间接恢复部分工具调用语义。

---

### 决策 D11：EngineConfig 新增字段（v1.1 追加）

v1 的 D6 引用了 `config.agent_id` / `config.log_dir`，但**当前代码无此字段**（grep 验证：`src/core/` 下零命中）。本决策填补此缺口：

```cpp
// src/core/types/engine_config.h (新文件)
struct EngineConfig {
  bool event_log_enabled = false;            // 默认关
  std::string event_log_agent_id;            // 必填（启用时）
  std::filesystem::path event_log_dir =       // 默认 ~/.hydraforge/event_log
      std::filesystem::path("~/.hydraforge/event_log");
  bool capture_prompt_bytes = false;         // D10
  size_t max_file_size = 100 * 1024 * 1024;  // D4
  size_t max_rotation_files = 3;             // D4
  std::chrono::milliseconds flush_interval{100};  // D4
};
```

`DSLEngine::set_engine_config()` 接受 EngineConfig（替代当前构造函数零散参数），D6 opt-in 启用逻辑依赖此结构。

---

### 决策 D12：显式 opt-in + bus 注入后启用（v1.1 修订 D6）

**原 D6 问题**：
- D6 在 DSLEngine 构造函数里调用 `bus_->subscribe(...)`，但 bus 是构造后经 `set_interaction_bus()` 注入的（`engine.h:220` 默认 nullptr）——**null deref**
- 引用虚构的 `config.agent_id` / `config.log_dir` 字段

**修订 D6（替换 v1 D6）**：

#### D12.1 新流程

```cpp
// 1. 构造 DSLEngine（不创建 EventLogWriter）
DSLEngine engine(...);
engine.set_interaction_bus(bus);  // 先注入 bus

// 2. 显式 opt-in（默认关闭，零行为破坏）
if (config.event_log_enabled) {
    if (config.event_log_agent_id.empty()) {
        throw std::runtime_error(
            "EventLog enabled but agent_id not set (fail-closed)");
    }
    engine.enable_event_log(config);  // 内部完成 subscribe
}

// 3. enable_event_log() 内部：
//    - make_unique<EventLogWriter>(config)
//    - bus_->subscribe("*", [this](const BusEvent& e) {
//          if (event_log_) event_log_->emit(e, causal_time_);
//      });
```

#### D12.2 测试隔离

所有现有 29 个测试 / examples 默认不启用 EventLog（`EngineConfig::event_log_enabled=false`），**零回归**。

#### D12.3 fail-closed 原则

未配置 `agent_id` = EventLog 不启用。**不静默合并到 `"default"`**——多 agent 场景下会污染审计域（v1 风险表自评"概率高/影响低"，v1.1 修正为"概率高/影响高"）。

---

## 不变量

1. **追加-only**：EventLog 文件只追加，不修改已写入行
2. **原子性**：每条事件一次 `write()` + `fsync()`（批量模式下每 100ms 一次 fsync）
3. **事件顺序**：日志顺序按 `causal_time` 单调不减，写入端负责排序；不假设 bus FIFO 分发顺序。排序键：`(causal_time, event_id)`
4. **事件完整性**：每条事件包含 `event_id`、`ts_wall`、`causal_time`、`topic`、`agent_id`（v1.1 修订）
5. **Agent 隔离**：不同 agent 的事件写入不同文件
6. **Step 0 前置**：EventLog 实现前，必须先完成 `BusEvent` 信封扩展（决策 D7）和 `GenerationRequest.purpose`（决策 D8）
7. **capture-off 默认**：v1 行为零变化，所有现有测试与 examples 不需修改（v1.1 D10.5）

## 不变量

1. **追加-only**：EventLog 文件只追加，不修改已写入行
2. **原子性**：每条事件一次 `write()` + `fsync()`（批量模式下每 100ms 一次 fsync）
3. **事件顺序**：日志顺序按 `causal_time` 单调不减，写入端负责排序；不假设 bus FIFO 分发顺序。排序键：`(causal_time, event_id)`
4. **事件完整性**：每条事件包含 `event_id`、`ts_wall`、`causal_time`、`topic`、`agent_id`（v1.1 修订）
5. **Agent 隔离**：不同 agent 的事件写入不同文件
6. **Step 0 前置**：EventLog 实现前，必须先完成 `BusEvent` 信封扩展（决策 D7）和 `GenerationRequest.purpose`（决策 D8）

## 后果

### 正面后果
- ✅ **持久化**：重启后事件不丢失
- ✅ **审计**：crash 后可恢复完整执行轨迹
- ✅ **分析**：离线 query 支持 agent 学习
- ✅ **解耦**：EventLog 与 SessionWriter 独立订阅 bus，职责清晰
- ✅ **可扩展**：v1 追加格式，未来可加压缩/索引/分片

### 负面后果
- ⚠️ **磁盘 IO**：高频事件（llm.request/response）增加写开销（批量优化缓解）
- ⚠️ **存储成本**：长会话日志文件可能数 MB/天（rotation 机制控制）
- ⚠️ **Step 0 复杂性**：需要先实现 BusEvent 信封扩展 + GenerationRequest.purpose

## 迁移路径

### Phase 0: Step 0 — BusEvent + GenerationRequest 扩展（2-3 天）
- `BusEvent` 加 `session_id` / `agent_id` 字段
- `EventBuilder` 加对应 setter
- `GenerationRequest` 加 `purpose` 字段
- `TracingDecorator` 在 `llm.request` 中记录 `purpose`
- 现有事件发射点更新（默认空 agent_id，不影响功能）

### Phase 1: EventLogWriter 核心（3-4 天）
- 实现 `EventLogWriter` 类
- 实现线程安全队列 + 批量写入
- 实现 rotation（100MB / 24h）
- 单元测试：写入/读取/rotation

### Phase 2: EventLog 与 bus 集成（1-2 天）
- `DSLEngine` 构造时创建 `EventLogWriter`
- 注册 bus subscriber
- 测试：启动 → 事件写入 → 崩溃恢复

### Phase 3: EventLog 查询 API（1-2 天）
- 实现 `EventLogReader::read()` 静态方法
- 实现 `EventLog::query()` 内存过滤
- 测试：读取/过滤/分页

### Phase 4: 与 SessionWriter 集成（1-2 天）
- SessionWriter 订阅 bus，过滤会话相关事件
- 与 EventLog 并行写入，测试无冲突

**总估时**：8-11 天（1.5 Sprint）

## 风险登记

| 风险 | 概率 | 影响 | 缓解 |
|---|---|---|---|
| 高频事件导致磁盘 IO 瓶颈 | 中 | 中 | 批量写入（100ms / 100 条）；可配置 flush interval |
| EventLog 文件过大 | 中 | 低 | rotation 机制（100MB / 24h） |
| 批量写入丢失最近事件 | 中 | 低 | `flush_sync()` API 显式同步；文档说明 crash 语义 |
| BusEvent 信封扩展 break 现有代码 | 低 | 高 | 字段默认空，现有代码无需改动 |
| Agent ID 未配置导致默认 "default" | 高 | 低 | 文档明确要求配置 agent_id |
| 读写线程竞争 | 低 | 高 | 所有锁操作用 mutex + try_lock 防死锁 |

## 与其他 ADR 的关系

- **ADR-0079**（Session 4-Scope）：EventLog 与 SessionWriter 并行订阅 bus，职责不同
- **ADR-0081**（Pre-Step Hook，推迟）：EventLog 记录 hook 触发事件（`agent.pre_step.*`）
- **ADR-0082**（Agent Registry）：每个 agent 有独立 EventLog 文件
- **ADR-0068**（EventBuilder）：EventLog 使用 EventBuilder 产出的 BusEvent

## 附录 A：EventLog JSONL 示例

```jsonl
{"v":1, "event_id":"evt-1737281400-001", "ts":1737281400, "topic":"llm.request", "agent_id":"agent-abc", "session_id":"ses-xyz", "payload":{"model":"deepseek-chat", "purpose":"plan"}}
{"v":1, "event_id":"evt-1737281401-002", "ts":1737281401, "topic":"llm.response", "agent_id":"agent-abc", "session_id":"ses-xyz", "payload":{"ok":true, "tokens":150}}
{"v":1, "event_id":"evt-1737281402-003", "ts":1737281402, "topic":"tool.execution.start", "agent_id":"agent-abc", "session_id":"ses-xyz", "payload":{"tool":"file.write", "args":{"path":"/tmp/test.py"}}}
{"v":1, "event_id":"evt-1737281403-004", "ts":1737281403, "topic":"tool.execution.end", "agent_id":"agent-abc", "session_id":"ses-xyz", "payload":{"tool":"file.write", "ok":true}}
{"v":1, "event_id":"evt-1737281404-005", "ts":1737281404, "topic":"attempt.ended", "agent_id":"agent-abc", "session_id":"ses-xyz", "payload":{"attempt_id":"attempt-1737281400-001", "ok":true, "steps":3}}
```

## 附录 B：批量写入配置

```cpp
struct EventLogConfig {
  std::filesystem::path log_dir = "~/.hydraforge/event_log";
  size_t max_file_size = 100 * 1024 * 1024;  // 100MB
  size_t max_rotation_files = 3;
  size_t batch_size = 100;                      // 每批最多 100 条
  std::chrono::milliseconds flush_interval{100}; // 每 100ms 批量刷写
};
```

## 附录 C：EventLog 与 SessionWriter 的职责边界

| 特性 | EventLog | SessionWriter |
|---|---|---|
| 目的 | 审计/分析/学习 | 会话恢复/context 构建 |
| 写入时机 | 实时（批量刷写） | 会话结束时 |
| 事件覆盖 | 全量 bus 事件 | 会话结构事件（15 个 topic） |
| 文件格式 | JSONL（单 agent） | JSONL（单 session） |
| 压缩 | rotation | compact_session() |
| 查询 | 内存过滤（小日志）| 离线工具 |

## 附录 D：EventLog 生命周期

```
DSLEngine 构造
  → 创建 EventLogWriter(agent_id, log_dir)
  → 注册 bus subscriber
  → 启动 flush 线程
  
DSLEngine::run()
  → 事件通过 bus 发射
  → EventLogWriter.emit() 入队
  → flush 线程批量写入磁盘
  
DSLEngine 析构
  → EventLogWriter::flush_sync() 显式同步
  → EventLogWriter::stop() 停止 flush 线程
  → 文件 close
```

---

### 附录 E：消费侧工具边界（v1.1 追加）

SFT 导出器（`event_log + session.jsonl → OpenAI messages 格式`）、跨 session 查询（`_index.jsonl` 或 SQLite sidecar）、replay 引擎均为离线工具，消费本 ADR 的 v1 schema。

**v1 schema 唯一约束**：上述工具必须能从落盘数据完整重建，**不需要运行时补数据**。

| 消费侧工具 | 何时 ship | 依赖 |
|---|---|---|
| SFT 导出器 | Phase 2 | EventLog v1.1 (D10 + D2) + Session 4-Scope (0079 v1.1) |
| 跨 session 查询 | Phase 2 | EventLog + `_index.jsonl` sidecar |
| Replay 引擎 | Phase 2 | D10 `params` 字段（replay 确定性）+ causal_time 排序 |
| Pre-step scrub hook | 后续 ADR | D10 开启时启用 |

工具本体不在本 ADR 范围，按消费侧需求独立 ship。

---

**审批记录**：
- v1 提议：2026-01-19
- v1 审批：2026-01-19
- v1.1 修订：2026-08-12（Distillation Capture + schema 修订 + opt-in）
- 实施：待 Phase 0（Step 0）启动
