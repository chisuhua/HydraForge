# ADR-0068: 事件发射契约 (Event Emission Contract)

## 状态

✅ Approved (2026-08-03 — Wave 1 §1-§5 全部 ship + V2 EventBuilder 扩展覆盖 8 处 operation-result 事件. 7 个幻影主题全部真实发射, §5.11 grep 验收返回 0 行, EventBuilder 100% 覆盖生产代码 emit, `test_e2e_mock.cpp` 全面重写完成. 变更依据: `openspec/changes/archive/2026-08-03-adr-0068-event-emission-contract/` + `openspec/changes/archive/2026-08-03-promote-event-builder-fulltoolresult-support/`; **Appendix A v1.1 amendment (2026-08-13): 14 个 📡 主题注册完成, 状态更新为 ✅ registered**; **Appendix A v1.2 amendment (2026-08-26): 新增 5 个主题 (1 evaluation.result + 4 mutation.*) 用于 ADR-0083/ADR-0084 契约, 待代码 ship 后标记 ✅**) 

## 领域

L1 OS Services / 事件总线应用语义层 / 运行时生命周期事件治理

## 关联

- [ADR-0019 — IInteractionBus MVP](./adr-0019-iinteraction-bus-mvp.md) — 总线接口机制 (emit/subscribe/glob), 本 ADR 不重复管辖
- [ADR-0046 — 插件间通信协议](./adr-0046-plugin-communication-protocol.md) — 插件域事件 (`inference.*`/`temporal.*`) 命名与频率策略, 边界划分见 §决策 1
- [ADR-0023 — ToolResult 标准化](./adr-0023-tool-result-standard.md) — BusEvent payload 载荷标准 (P2-P4)
- [ADR-0037 — 因果排序](./adr-0037-causal-ordering.md) — CausalClock, 与本 ADR 正交
- [ADR-0002 — EventBus 有界队列](./adr-0002-eventbus-bounded-queue.md) — ❌ Not Implemented, 历史背景
- [`docs/architecture/layer-based-missing-capabilities-analysis.md`](../architecture/layer-based-missing-capabilities-analysis.md) §三 X1 — 决策源头 (2026-07-31 复核数据)
- [`docs/architecture/adr-implementation-status-gap-analysis.md`](../architecture/adr-implementation-status-gap-analysis.md) — ADR-0019 Partial 基线

## 背景

### 问题

事件总线**基础设施层是健康的**（BusEvent 信封 + InMemoryBus MPMC + subscribe_glob + CausalClock 均已 ship），但**应用语义层无契约**，2026-07-31 全量复核确认三个实证缺口：

**缺口 1 — 发射端无治理，三种构造方言并存。** 生产代码 28 处 emit 调用点（`python3 tools/doc_metrics.py --emit`），同一信封三种惯用法：

| 发射点 | 构造方言 |
|--------|---------|
| `tool_coordinator.cpp` (5 处) | 手搓裸 JSON payload (caller/callee/thread_id/nesting_depth) |
| `node_executor.cpp` (4 处) | `ToolResult::success(args, meta)` 双参数 |
| `chat_session.cpp` (5 处) | `ToolResult{.ok=true, .meta={...}}` 聚合初始化 |

数据放 args 还是 meta 无约定；ADR-0019 的"payload 遵守 ToolResult P2-P4"一行约定无执行、无 lint。

**缺口 2 — 订阅期望与发射现实倒挂。** `examples/pdk_chat_demo/event_handler.cpp` 订阅 12 个主题，其中 **7 个在生产代码零 emit**：

| 幻影主题 | 唯一出现位置 |
|---------|-------------|
| `loop.turn.start` / `loop.turn.end` | `tests/test_e2e_mock.cpp:124,131,133` (mock 伪造) |
| `llm.request` / `llm.response` | `tests/test_e2e_mock.cpp:125,126` (mock 伪造) |
| `loop.decision` | `tests/test_e2e_mock.cpp:127,129,132` (mock 伪造) |
| `tool.execution.start` / `tool.execution.end` | `tests/test_e2e_mock.cpp:128,130` (mock 伪造) |
| `session.persisted` | 无任何 emit (连 mock 都没有) |

demo 渲染管线为永不发射的事件接线；测试伪造生产行为——测试与实现关系倒置。

**缺口 3 — 反向缺口。** 约 10 个主题有 emit 但无文档化订阅方：`tool.audit.{invoked,completed,denied}`、`compliance.log`、`cognitive.task.*`、`domain.task.*`、`dsl.call.*`、`execution.failed`。审计链路（ADR-0031 §决策 7 设计的 tool.audit.*）无注册消费者。

### 目标

1. 每个 canonical 主题有唯一 owner、强制发射点、payload schema——"自愿 emit"变为"契约 emit"。
2. 7 个幻影主题获得真实发射点，Wave 1 下游项（L4-3 流式渲染、streaming、compaction hook）有事件可消费。
3. 构造方言统一为一个 EventBuilder，payload 字段分工可 lint。
4. 测试断言真实发射，消除 mock 伪造反模式。

## 决策

### 1. 管辖边界（与 ADR-0019 / ADR-0046 / ADR-0037 的划界）

| ADR | 管辖范围 | 本 ADR 关系 |
|-----|---------|------------|
| ADR-0019 | 总线**机制**（emit/subscribe/subscribe_glob/CausalClock 接口与实现） | 不重复管辖；本 ADR 只定义"在何处必须调用 emit" |
| **ADR-0068** (本) | **运行时生命周期事件**：`loop.*` / `llm.*` / `tool.execution.*` / `session.*` / `context.compact.*` / `user.input` / `app.*`（L0/L1 运行时 → L4 应用） | — |
| ADR-0046 | **插件域事件**：`inference.*` / `temporal.*` / 未来插件自定义域（L2 插件 ↔ L2 插件 / L4） | 其命名约定 (`<module>.<verb>` dot 分隔) 与频率策略 (严禁高频指标推送) 被本 ADR **引用沿用**，不重新定义 |
| ADR-0037 | 因果排序 (causal_time 填充) | 正交，本 ADR 不涉及 |

**边界条款**：ADR-0046 下次修订时应在其 Event Layer 节添加反向指针："运行时生命周期主题以 ADR-0068 为准"。两份 ADR 禁止各自维护对方域内的主题规范（单一事实源原则，docs/GOVERNANCE.md §一.5）。

### 2. Canonical Topic Registry（核心交付物）

建立受维护的主题注册表（附录 A），每个主题声明四元组：

- **Owner 模块**：唯一负责 emit 的模块（其他模块 emit 同主题视为契约违反）；
- **强制发射点**：生命周期阶段 + 代码位置（文件/函数级）；
- **Payload schema**：字段表，沿用 ADR-0023 ToolResult P2-P4（`error_code` enum / `trace_id` / `meta`）；
- **兼容政策**：additive-only（见 §决策 5）。

首批收录：12 个已订阅主题 + 10 个已发射主题 + 7 个幻影主题（去重后 22 个）。Registry 的维护方式：作为本 ADR 附录 A 的表格，**新增/修改主题必须 PR 修订本附录**（轻量仪式，Phase 6 plan+commit 模式下为 commit 内同步更新）。

### 3. 幻影主题强制发射点指定

| 主题 | Owner | 强制发射点 | 实施说明 |
|------|-------|-----------|---------|
| `llm.request` | L1 LLM Decorator 链 | `ILLMProvider::generate()` 调用前 | 新增 `TracingDecorator`（或复用现有链）：天然拦截点，零新管道；`ComplianceDecorator` emit `compliance.log` 已验证此模式可行 |
| `llm.response` | L1 LLM Decorator 链 | `generate()` 返回后（含 error 路径） | 同上；payload 含 `tokens`/`duration_ms`/`error_code` |
| `loop.turn.start` / `loop.turn.end` | **loop_agent (L4)** | 每轮 ReAct turn 开始/结束 | **依赖 L4-1 修复**：bypass 删除后 `loop/run` 进入调用路径，Agent 自发射生命周期事件（契合 Agent-as-Plugin 哲学） |
| `loop.decision` | loop_agent (L4) | 每轮决策点（tool_call / respond / give_up） | 同上；payload 含 `decision` + 关联 `tool` |
| `tool.execution.start` / `tool.execution.end` | L1 ToolCoordinator | `call_tool` 线性流首尾（与 `tool.audit.*` 同源点） | 现有 audit emit 同源位置补齐，成本极低 |
| `session.persisted` | session_agent (L2) / ChatSession | 持久化写盘成功后 | 与已有 `session.persist_request` 配对成闭环 |

### 4. EventBuilder 与 payload 方言统一

新增 `include/agenticdsl/contract/event_builder.h`（header-only, L1 契约层）：

```cpp
// 统一三种方言为一个 builder; args/meta 分工明确化:
//   args → 结构化业务字段 (schema 声明的必填字段)
//   meta → 附加上下文 (trace_id, session_id, debug 信息)
BusEvent ev = EventBuilder("tool.execution.start")
    .args({{"tool", name}, {"layer", layer}})
    .meta({{"trace_id", tid}})
    .build();   // timestamp 自动填充 steady_clock::now()
```

- 28 处现有 emit 在 Wave 1 期间迁移到 EventBuilder（允许分批，迁移完成为本 ADR 转 Approved 的条件之一）；
- 新增 emit 一律使用 EventBuilder（code review 检查项 + clang-tidy 候选规则）。

### 5. 兼容政策

- **Additive-only**：新主题随时可加（需登记附录 A）；已有主题 payload **只增字段不改语义**；
- **破坏性变更**：需新主题名 + 双发过渡期（旧主题标记 `deprecated` 至少 1 个 Sprint，订阅方迁移后删除）；
- **主题命名**：沿用 ADR-0046 约定 `<domain>.<entity>.<verb>`（dot 分隔，snake_case），运行时域限定为 §决策 1 列出的 7 个 domain。

### 6. 测试契约（消除缺口 2 的倒置）

- 每个 canonical 主题必须配一个**断言真实发射**的测试：触发对应生命周期路径，断言 bus 收到该主题且 payload 含 schema 必填字段；
- `test_e2e_mock.cpp` 伪造事件模式标记为**反模式**：Wave 1 期间将其替换为真实管线测试（loop_agent 真实执行 + 断言 7 个原幻影主题）— **已在 main `5c5037c` 完成**（Sprint 19/20 fix-loop-agent-bypass）；
- 新增主题无发射测试 = 契约违反，drift 审计候选检测项（`docs_drift_audit.py` 后续 Scenario 扩展）。

### 7. EventBuilder Operation-Result 扩展 (V2, 2026-08-03)

Wave 1 ship 阶段因 `EventBuilder` API 限制 (`build()` 强制 `payload.ok = true`, 无 `error_code`/`latency_ms`/`trace_id`/`metadata` setters), 8 处 operation-result 事件 (`tool.completed` / `execution.failed` / `cognitive.task.completed` / `domain.task.{completed,failed}` x3 / `tool.audit.denied` x2) 保留 raw `BusEvent{}` 构造, 虽不影响测试但 §5.11 grep 验收不满足。

**V2 扩展** (OpenSpec change `promote-event-builder-fulltoolresult-support`):
- 新增构造器 `EventBuilder(std::string topic, ToolResult payload)` — 接管完整 7 字段 (ok/data/meta/error_code/latency_ms/trace_id/metadata), 兼容 8 处 migration
- 新增 5 个 setters: `.ok(bool)` / `.error_code(ErrorCode)` / `.latency_ms(uint64_t)` / `.trace_id(std::string)` / `.metadata(json)`
- 8 处 raw `BusEvent{}` 全部迁移到 EventBuilder

**变量决策**: EventBuilder V2 通过 full-payload 构造函数承载完整 `ToolResult` (含 `ok=false` + 可选字段), subscriber 依赖 `payload.ok` 字段判定业务成败语义保持完整。

## 后果

### 正面

- Wave 1 下游项（L4-3 流式渲染、streaming、compaction hook、steering）获得可依赖的事件契约；
- 审计链路（tool.audit.*）与事件目录注册后可被 TUI/OTel exporter (ADR-0063) 消费；
- EventBuilder 消除方言，payload 可 lint 可校验；
- 测试从"伪造事件"转为"验证契约"，测试可信度提升。

### 负面 / 成本

- 附录 A Registry 需要维护纪律（靠 Sprint 收官 Last-Verified 检查 + code review 约束）；
- 28 处 emit 迁移有机械工作量（估 0.5 天，含在 Wave 1 X1 的 1 Sprint 估时内）；
- loop_agent 增加发射职责，与 L4-1 修复耦合排期。

### 转 ✅ Approved 条件

1. 7 个幻影主题全部有真实发射 + 发射测试通过 — **✅ 满足** (Wave 1 §2-§4 ship 5 个 + `fix-loop-agent-bypass` ship 3 个 `loop.*` = 7/7)
2. 附录 A Registry 22 个主题登记完成 — **✅ 满足** (附录 A 22 个主题全部登记)
3. EventBuilder 落地且 ≥80% 现有 emit 完成迁移 — **✅ 满足** (V2 扩展后 100% 迁移, §5.11 grep 0 行)
4. `test_e2e_mock.cpp` 伪造事件全部移除 — **✅ 满足** (Sprint 19/20 `fix-loop-agent-bypass` ship 全面重写)

## 替代方案

| 方案 | 排除理由 |
|------|---------|
| 扩 ADR-0019 吸收主题目录 | 0019 是接口机制 ADR 且已 Partial，混入主题目录会变 god-ADR；其 Partial 收尾 (subscribe_topic vs subscribe_glob 关系) 是独立事项不应捆绑 |
| 并入 ADR-0046 | 0046 定位插件↔插件通信且自身 Proposed/~35% 实施率；`loop.turn.*` 等运行时事件不属插件间通信 |
| 不立 ADR，仅靠 code review 约束 | 已被实证失败——ADR-0019 的 payload 约定单行文本 2 个月无执行，无 Registry 则无 lint 依据 |

## 交叉引用

- [ADR-0019](./adr-0019-iinteraction-bus-mvp.md) — 总线机制
- [ADR-0046](./adr-0046-plugin-communication-protocol.md) — 插件域事件 (边界条款见 §决策 1)
- [ADR-0023](./adr-0023-tool-result-standard.md) — payload P2-P4
- [ADR-0031](./adr-0031-execution-policy.md) §决策 7 — tool.audit.* 审计事件设计
- [ADR-0063](./adr-0063-opentelemetry-tracing.md) — OTel exporter (Registry 的未来消费者)
- [`docs/architecture/layer-based-missing-capabilities-analysis.md`](../architecture/layer-based-missing-capabilities-analysis.md) — X1/L1-1 缺口分析

---

## 附录 A：Canonical Topic Registry (v1.2, 2026-08-26)

> 维护规则：新增/修改主题必须同步修订本表。状态列：✅ 已注册 / 👻 幻影 (零生产 emit) / 📡 已发射但无注册订阅方 (本 amendment 后已全部注册)。

| 主题 | Owner 模块 | 强制发射点 | Payload 必填字段 | 状态 |
|------|-----------|-----------|-----------------|------|
| `user.input` | ChatSession | 用户输入提交后 | `session_id`, `input` | ✅ |
| `app.shutdown` | main / 应用入口 | 退出流程开始 | — | ✅ |
| `loop.turn.start` | loop_agent (L4) | 每轮 turn 开始 | `turn`, `step` | ✅ |
| `loop.turn.end` | loop_agent (L4) | 每轮 turn 结束 | `turn`, `decision` | ✅ |
| `loop.decision` | loop_agent (L4) | 决策点 | `decision`, `tool?` | ✅ |
| `loop.done` | ChatSession / loop_agent | 循环完成 | `session_id` | ✅ |
| `loop.error` | ChatSession / loop_agent | 循环异常 | `error_code`, `message` | ✅ |
| `llm.request` | L1 Decorator 链 | generate() 前 | `model`, `prompt_hash` | ✅ |
| `llm.response` | L1 Decorator 链 | generate() 后 | `tokens`, `duration_ms`, `error_code?` | ✅ |
| `tool.execution.start` | ToolCoordinator | call_tool 入口 | `tool`, `layer` | ✅ |
| `tool.execution.end` | ToolCoordinator | call_tool 返回 | `tool`, `ok`, `duration_ms` | ✅ |
| `session.persist_request` | ChatSession | 持久化请求发出 | `session_id` | ✅ |
| `session.persisted` | SessionManager | 写盘成功 | `session_id`, `node_id`, `branch_id`, `timestamp` | ✅ (v2 ship) |
| `budget.checked` | ChatSession / budget_agent | 预算检查后 | `remaining`, `exceeded` | ✅ |
| `context.compact.before` | ContextCompactor (L0) | on_compact_before() | `session_id`, `tokens_before` | ✅ `src/core/context_compactor.cpp:45-51` |
| `context.compact.after` | ContextCompactor (L0) | on_compact_after() | `session_id`, `tokens_before`, `tokens_after`, `compression_ratio` | ✅ `src/core/context_compactor.cpp:81-89` |
| `tool.audit.invoked` | ToolCoordinator | 审批通过后 | `tool`, `args_keys` | ✅ `src/common/tools/tool_coordinator.cpp:307` |
| `tool.audit.completed` | ToolCoordinator | 工具返回后 | `tool`, `ok` | ✅ `src/common/tools/tool_coordinator.cpp:376` |
| `tool.audit.denied` | ToolCoordinator | 拒绝时 | `tool`, `reason` | ✅ `src/common/tools/tool_coordinator.cpp:240,265,290` |
| `tool.coordinator.cycle_detected` | ToolCoordinator | 循环检测 | `caller`, `callee` | ✅ `src/common/tools/tool_coordinator.cpp:127` |
| `policy.approval.requested` | ApprovalHandler | 审批请求 | `tool`, `preview` | ✅ `src/common/policy/approval_callbacks.cpp:57` |
| `compliance.log` | ComplianceDecorator | 合规记录点 | `kind`, `hash` | ✅ `src/common/llm/compliance_decorator.cpp:75,88,130,150` |
| `cognitive.task.started` | CognitiveWorker | 任务开始 | `task_id` | ✅ `src/modules/cognitive/cognitive_worker.cpp:170` |
| `cognitive.task.completed` | CognitiveWorker | 任务完成 | `task_id`, `ok` | ✅ `src/modules/cognitive/cognitive_worker.cpp:200` |
| `domain.task.started` | DomainWorkerPool | 任务派发 | `domain`, `tool` | ✅ `src/modules/cognitive/domain_worker_pool.cpp:198` |
| `domain.task.completed` | DomainWorkerPool | 任务完成 | `domain`, `ok` | ✅ `src/modules/cognitive/domain_worker_pool.cpp:264` |
| `domain.task.failed` | DomainWorkerPool | 任务失败 | `domain`, `error_code` | ✅ `src/modules/cognitive/domain_worker_pool.cpp:228,266` |
| `dsl.call.started` | NodeExecutor | DSL 节点进入 | `node_path`, `llm_tool_name` | ✅ `src/modules/executor/node_executor.cpp:133` |
| `dsl.call.completed` | NodeExecutor | DSL 节点退出 | `node_path`, `output_key` | ✅ `src/modules/executor/node_executor.cpp:159` |
| `execution.failed` | NodeExecutor | 执行失败 | `node_path`, `error_code` | ✅ `src/modules/executor/node_executor.cpp:412` |
| `agent.spawned` | PluginLoader / CognitiveWorker | LOADED→initialized 转换 | `agent_descriptor`, `timestamp`, `causal_time` | ✅ (ADR-0057 §决策 6, 2026-08-20) |
| `agent.heartbeat` | PluginLoader (per-plugin jthread) | active 期间默认 30s | `state`, `uptime_ms` | ✅ (ADR-0057 §决策 6) |
| `agent.terminated` | PluginLoader / CognitiveWorker | active→inactive 转换 | `reason`, `exit_code` | ✅ (ADR-0057 §决策 6) |
| `agent.error` | PluginLoader / CognitiveWorker | 任意状态转换失败 | `error_code`, `diagnostic` | ✅ (ADR-0057 §决策 6) |
| `evaluation.result` | CognitiveWorker / DomainWorkerPool | 任务评估完成后 | `evaluation_id`, `trace_id`, `reward` (RewardSignal), `confidence` | ✅ (ADR-0083 ship, 2026-08-26) |
| `mutation.proposed` | MutationGovernor | 变异提议通过初步验证 | `mutation_id`, `source_id`, `subject_ref`, `mutation_kind`, `evaluation_refs` | ✅ (ADR-0084 ship, 2026-08-26) |
| `mutation.approved` | MutationGovernor | 变异通过授权门 | `mutation_id`, `approval_mode`, `approved_by` | ✅ (ADR-0084 ship, 2026-08-26) |
| `mutation.committed` | MutationGovernor | 变异提交完成 | `mutation_id`, `commit_timestamp` | ✅ (ADR-0084 ship, 2026-08-26) |
| `mutation.denied` | MutationGovernor | 变异被拒绝 | `mutation_id`, `denial_reason`, `denied_at` | ✅ (ADR-0084 ship, 2026-08-26) |
