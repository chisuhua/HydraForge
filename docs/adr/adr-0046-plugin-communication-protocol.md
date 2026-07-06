# ADR-0046: PDK 插件间通信协议

## 状态

🔍 Proposed (2026-07-06 — 架构方案讨论产出, 待 review; **2026-07-06 renumber**: 原编号 0037 因与旧 ADR-0037-跨 Worker 事件因果序冲突, 改为 0046)

## 领域

基座 / PDK / Plugin Communication

## 关联

- ADR-0035 (Inference Engine Plugin Spec) — 推理引擎 plugin 的工具集和事件
- ADR-0045 (Orchestration Plugin Spec) — 编排 plugin 的消费者视角
- ADR-0021 (PDK Design) — Plugin 开发框架
- ADR-0022 (Plugin Loading) — dlopen, pdk_register_tools, pdk_plugin_info
- ADR-0019 (IInteractionBus) — EventBus pub/sub
- ADR-0023 (ToolResult Standard) — 统一信封格式
- ADR-0031 (Execution Policy) — ToolCoordinator, approval pipeline

---

## 背景

### 问题

当前 PDK 生态有两个 Plugin 需要协作, 但缺少标准化的通信协议:

| 维度 | 现状 | 问题 |
|------|------|------|
| **同步调用** | ToolRegistry::call_tool | 可用, 但缺少标准参数/返回值 Schema 文档 |
| **异步通知** | IInteractionBus | 可用, 但 topic 命名和 payload 格式无规范 |
| **配置面** | 无标准机制 | 编排 plugin 需要动态调节推理 plugin 参数 |
| **查询面** | 无标准机制 | 编排 plugin 需要感知推理 plugin 状态/性能 |

### 目标

定义两个 PDK Plugin 之间的**四通道通信架构**:

```
编排 Plugin                         推理引擎 Plugin
  │                                     │
  ├─ ① Tool Layer ─────────────────────►│  sync: call_tool("inference/*", args) → json
  │                                     │
  │◄─ ② Event Layer ──────────────────┤  async: emit("inference/*", payload)
  │                                     │
  ├─ ③ Config Layer ──────────────────►│  sync: call_tool("inference/configure", args) → json
  │                                     │
  ├─ ④ Query Layer ◄───────────────────│  sync: call_tool("inference/get/*", args) → json
  │                                     │
```

**所有通道复用现有 HydraForge 基础设施, 零框架改动。** (参考 ADR-0021 P3: PDK 静态链接到 plugin, Runtime 不感知)

---

## 决策

### 1. 通道 ①: Tool Layer (同步操作面)

**机制**: `IToolRegistry::call_tool(name, unordered_map<string,string> args) → nlohmann::json`

**数据格式**:
- Args: `map<string,string>` — 所有参数值均为字符串, 嵌套结构通过 JSON 字符串传递
- Return: `nlohmann::json` — 由 ToolCoordinator 包装为 ToolResult 信封

**注册方式** (参考 ADR-0034 Model Router 模式):
```cpp
extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry) {
  ToolMetadata meta{name, desc, domain, category, min_layer, approval};
  registry.register_tool_function(name, meta,
    [](const unordered_map<string,string>& args) -> nlohmann::json {
      // parse args JSON → execute → return result JSON
    });
}
```

**工具命名约定** (P0 fix @Oracle review):

> PDK tool names use slash (`/`) as hierarchy delimiter. Format: `{namespace}/{component}/{action}`.

示例:
- `inference/engine/init`, `inference/generate`, `inference/get/status`
- `model_router/cost`, `model_router/quality`
- `orchestration/route`, `orchestration/execute`

**与已有先例的诚实说明**: 本约定与 ADR-0034 (Model Router) `model_router/cost` C7 已 ship 命名格式一致。ADR-0034 §命名约定 是 2026-07-06 同日追加, 同步落地本约定 — 并非"先例单向引用", 而是**同步新增约定并已存在的工具名强制遵循**。

**分隔符决策** (P0 fix @Oracle review):

| 用途 | 分隔符 | 例子 | 来源 |
|------|:------:|------|------|
| **PDK tool names** | slash `/` | `inference/engine/init` | 本 ADR + ADR-0034 |
| **EventBus topics** | dot `.` | `inference.lifecycle.idle` | ADR-0019 Sprint 2 (cognitive.task.started) |
| **C++ method calls** | dot `.` | `engine.generate()` | C++ 语法 |
| **DSL module namespace** | dot `.` 或 `::` | `inference::engine`, `inference.engine` | DSL 规范 |

**关键区别**: Tool names (PDK) 与 EventBus topics (Runtime events) 是不同作用域, 分隔符独立。PDK tool 注册到 ToolRegistry 时用 slash; Runtime 通过 IInteractionBus emit 时用 dot。消费者代码通过 `inferenceEngine->emit_event("inference.lifecycle.context_overflow")` 与 `registry.call_tool("inference/generate", args)` 分别访问。

**错误处理约定** (P1 fix @Oracle review):

- 成功: 返回 ToolResult (ok=true) 或直接返回 nlohmann::json (由 ToolRegistry 包装为 ToolResult)
- 业务错误: 返回 `ToolResult.error(ErrorCode, message, meta)` (Per ADR-0023 P2 enum ErrorCode, NOT 嵌套 JSON)
- 协议错误: 由 ToolCoordinator 处理 (工具未注册 → ErrorCode::ToolNotRegistered)
- **不再使用**嵌套 `{"error": {"code": ..., "message": ...}}` 格式 (ADR-0023 §C.7 已知遗留已修正)

### 2. 通道 ②: Event Layer (异步通知面)

**机制**: `IInteractionBus::emit(topic, ToolResult payload)` + `IInteractionBus::subscribe_topic(topic, callback)` (P0 fix: 当前接口扩展)

**Topic 命名规范** (延续 ADR-0019 `<module>.<verb>` 用 dot):
```
inference.lifecycle.{state}          ← 推理引擎状态变化 (idle, running, model_loaded, context_overflow)
inference.model.{action}             ← 模型生命周期事件 (loaded, unloaded, switched)
inference.error.{code}               ← 推理错误 (oom, network, cancelled, context_overflow) + retryable flag
orchestration.audit.internal.{tool}  ← 编排 Plugin 内部调用 audit 事件 (ADR-0045 §6.3)
orchestration.audit.llm.generate     ← 编排 ILLMProvider → inference/generate 内部调用 audit
```

**频率限制**: EventBus 承载**每推理 ~1-2 次**的生命周期事件。性能指标 (t/s, KV cache%, GPU mem) 通过通道 ④ (query) 按需拉取。1000+ events/sec 会触发 Sprint 12 bridge 背压问题, 严禁高频指标推送。

#### 2.1 IInteractionBus 接口扩展 (P0 fix @Oracle review, "零框架改动" 局部妥协)

ADR-0019 IInteractionBus 当前接口是 `subscribe_events(session_id, callback)` session-based, 不支持 topic-based subscribe。ADR-0046 Event Layer 需要 topic-based 订阅。

**扩展方案 (承认零框架改动是 aspirational)**:

```cpp
// include/agenticdsl/contract/iinteraction_bus.h (ADR-0019 同步追加)
class IInteractionBus {
 public:
  // 现有 (ADR-0019): session-based subscribe
  virtual size_t subscribe_events(session_id_t session_id, callback_t callback) = 0;

  // 新增 (ADR-0046 扩展): topic-based subscribe
  virtual size_t subscribe_topic(
      const std::string& topic_pattern,   // 支持 glob: "inference.lifecycle.*"
      callback_t callback) = 0;

  virtual void unsubscribe(size_t token) = 0;
  ...
};
```

**InMemoryBus 实现扩展**:
- topic-based subscribe 与现有 session subscribe 共存
- topic dispatch 与 session dispatch 复用现有 dispatch_thread + MPMC queue (ADR-0019 P2)
- glob 支持: `subscribe_topic("inference.*", cb)` 匹配所有 `inference.*` events

**修订依据**: 妥协"零框架改动"原则是有意识的决策。理由: topic-based event 是 PDK Plugin 间通信的**核心需求**, 不引入则 ADR-0046 Event Layer 不可实施。InMemoryBus 已 ship 的 MPMC + dispatch_thread 后端可复用, 仅添加 topic dispatch 路由, 实现成本 <1d。

**Permission 限制**: subscribe_topic 需传入 plan/agent 模式上下文, 与 ToolCoordinator 类似做 layer check (Cognitive 层不能订阅 Inference plugin 内部事件, 仅 Workflow 层可见)。

**Payload 标准格式** (ToolResult 信封, P1 fix 与 ADR-0023 §C.1 字段对齐):
```json
{
  "ok": true,
  "data": {},
  "meta": {"state": "running", "model_id": "qwen3-7b", "timestamp": 1750182400},
  "error_code": null,
  "latency_ms": null,
  "trace_id": "trace_abc123",
  "metadata": null
}
```

所有字段遵循 ADR-0023 §C.1 ToolResult Schema (P2 4 个 optional 字段全部包含)。

### 3. 通道 ③: Config Layer (动态调节面)

**机制**: `inference/configure` tool, 同步请求-响应, JSON in/out

**为什么选 Tool (Path A) 而非新框架接口 (Path B)** (P1 fix @Oracle review):

- 零框架改动 (除 IInteractionBus topic subscribe 扩展外) — 符合 ADR-0046 核心原则
- 等 2+ 个 Configurable Plugin 出现后再提取通用模式 (当前 scoping ADR-0041 未来引入)
- 现有 ToolRegistry 已提供审计、权限、layer check (ADR-0031)
- **编排 Plugin 内部调用豁免** (ADR-0045 §6): 自动调节免审批, 仅 emit `orchestration.audit.internal.inference/configure`

**Key insight**: `inference/configure` 的 ApprovalPolicy=`plan` 在编排 Plugin 内部调用时**通过 §6 internal_registry 豁免 ToolCoordinator 审批**, 但 ToolMetadata 元数据仍存在, 仅不强制审批。audit 事件记录到 `orchestration.audit.internal.*` (区别于外部 `tool.audit.*`)。

**Schema**:
```json
// Request
{
  "n_threads": "8",
  "n_threads_batch": "8",
  "default_temperature": "0.5",
  "default_top_k": "50",
  "prefer": "latency"
}

// Response
{
  "applied": {
    "n_threads": true,
    "n_threads_batch": true,
    "default_temperature": true,
    "default_top_k": true,
    "prefer": true
  },
  "requires_restart": {
    "n_gpu_layers": false
  },
  "current": {
    "n_threads": 8,
    "n_threads_batch": 8,
    "default_temperature": 0.5,
    "default_top_k": 50,
    "prefer": "latency"
  }
}
```

**参数分类 (ADR-0038 详细规范)**:
- **即时生效**: `n_threads`, `abort_callback`, `embeddings`, `warmup`, `LoRA adapters`
- **Next-request 生效**: `default_temperature`, `default_top_k`, `prefer`
- **需重启引擎**: `n_gpu_layers`, `split_mode`, `use_mmap`, `devices`

### 4. 通道 ④: Query Layer (状态查询面)

**机制**: `inference/get/status` + `inference/get/models` tools, 同步请求-响应

```
编排 Plugin                    推理引擎 Plugin
  │                                │
  ├── call_tool("inference/get/status", {})
  │                                │
  │◄──── atomic snapshot ──────────┤ (一次性读取全部指标)
  │                                │
  ├── call_tool("inference/get/models", {})
  │                                │
  │◄──── model list + capabilities ┤
```

**为什么是同步查询而非事件推送**: 编排 Plugin 做决策时 (模型选择、路由、负载均衡) 需要**当前快照**, 不需要历史指标流。按需查询避免 EventBus 背压。

**Schema** (详见 ADR-0039 性能元数据契约):
- `inference/get/status` → 引擎状态 (uptime, backend, device, sessions, performance)
- `inference/get/models` → 模型列表 (model_id, n_params, n_ctx, kv_cache_pct, t/s)

---

## 通信通道对比

| 维度 | Tool Layer | Event Layer | Config Layer | Query Layer |
|------|:---:|:---:|:---:|:---:|
| **基础设施** | IToolRegistry | IInteractionBus (topic) | IToolRegistry | IToolRegistry |
| **方向** | 编排→推理 | 推理→编排 | 编排→推理 | 编排→推理 |
| **同步性** | 同步 | 异步 (dispatch thread) | 同步 | 同步 |
| **频率** | 中 (每次任务) | 低 (每推理 1-2 次) | 低 (配置变化时) | 中 (每次决策时) |
| **走审批?** | ✅ ToolCoordinator (外部) / ❌ 豁免 (内部, ADR-0045 §6) | ❌ | ✅ ToolCoordinator (外部) / ❌ 豁免 (内部) | ❌ (ReadOnly tools) |
| **数据格式** | ToolResult envelope | ToolResult envelope | JSON (applied/requires_restart/current) | JSON (atomic snapshot) |

---

## 安全性

| 通道 | 安全机制 | 来源 |
|------|---------|------|
| **Tool Layer (外部)** | ToolMetadata.category + ApprovalPolicy → ToolCoordinator → audit | ADR-0031, ADR-0004 |
| **Tool Layer (内部)** | ToolCategory / Layer 检查生效; Approval 豁免; 自定义 `orchestration.audit.internal.*` emit | ADR-0045 §6 |
| **Event Layer** | subscribe 需 bus 引用 (非公开); subscribe_topic glob 受 layer check 限制 | ADR-0019, ADR-0046 §2.1 |
| **Config Layer** | (外部) ToolMetadata=StateModify + ApprovalPolicy=plan; (内部) ADR-0045 §6 豁免 + audit emit | ADR-0031, ADR-0045 §6 |
| **Query Layer** | ToolMetadata.category=ReadOnly + ApprovalPolicy=none | ADR-0004 |

---

## 实施顺序

1. 通道 ① (Tool Layer) — 已有基础设施, 仅需标准化 Schema 文档
2. 通道 ② (Event Layer) — IInteractionBus 扩展 `subscribe_topic` + InMemoryBus 实现 topic dispatch
3. 通道 ④ (Query Layer) — `inference/get/status` 和 `inference/get/models` tool 实现
4. 通道 ③ (Config Layer) — `inference/configure` tool 实现 + 分层参数 Schema

---

## 测试策略 (P1 fix @Oracle review)

| # | 测试名 | 覆盖 |
|---|--------|------|
| 1 | `tool_naming_slash_validation` | 注册时校验 slash 分隔符, 拒绝空 name / 含尾部 `/` |
| 2 | `tool_naming_backward_compat` | 旧 dot 命名工具 (`inference.engine_init`) 在 v0.2.0 后无法注册 |
| 3 | `tool_call_sync_returns_tool_result` | call_tool 返回 ToolResult (而非裸 json), error_code 正确序列化 |
| 4 | `event_topic_dot_validation` | emit 拒绝 slash topic, 强制 dot 格式 |
| 5 | `event_subscribe_topic_glob_match` | `inference.*` 匹配所有 inference.* topics |
| 6 | `event_subscribe_topic_no_match` | 订阅未 emit 的 topic 不调用 callback |
| 7 | `event_bus_backpressure_safe` | 1000+ emit/s 不导致 dispatch thread 阻塞 (已有 InMemoryBus P2 验证) |
| 8 | `event_payload_field_completeness` | ToolResult payload 字段对齐 ADR-0023 §C.1 (6 字段) |
| 9 | `configure_layer_classification` | L3a 即时生效 vs L3b 需 session 重建 vs 需重启参数 |
| 10 | `configure_response_requires_restart_field` | 返回 `requires_restart` 列出正确参数 |
| 11 | `query_get_status_atomic_snapshot` | 一次性 lock 读取多字段, 无 race |
| 12 | `internal_registry_bypasses_tool_coordinator` | 编排 Plugin 内部调用 `inference/*` 不触发外部 audit |

---

*创建日期*: 2026-07-06
*Oracle 审查*: ses_0c9e97925ffete0oXvgRmpLo12 (P0 review)
*修订*: 2026-07-06 (P0 fix 应用)
*依赖*: ADR-0035 (推理引擎 Plugin 规范), ADR-0038 (动态配置接口), ADR-0039 (性能元数据契约)
*关联修正*: IInteractionBus subscribe_topic 扩展, ADR-0019 同步追加

---

*创建日期*: 2026-07-06
*Oracle 审查*: ses_0ca3dce4fffeck5vmAQMs6R94m (四通道架构 + Config Layer Path A 推荐)
*依赖*: ADR-0035 (推理引擎 Plugin 规范), ADR-0038 (动态配置接口), ADR-0039 (性能元数据契约)