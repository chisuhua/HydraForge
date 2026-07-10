# ADR-0003 Implementation Scope Audit

> **生成时间**: 2026-07-03 (C9 — Phase 4.5 impl-scope audit)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0003-dslengine-thread-safety.md](adr-0003-dslengine-thread-safety.md)
> **状态**: ✅ Approved (audit 后保持)

## 状态

**📋 Audit** (impl-scope-audit 文档, 与 docs-code-drift-audit 配套使用)

✅ Approved (audit 后保持 — 所有 11 个 ADR 核心契约类均已 Shipped 或 Evolved, 无需调整主 ADR 状态)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved, 但 3/6 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `DSLEngine` | ✅ Shipped | `src/core/engine.h` | 核心引擎已实现 |
| `TopoScheduler` | ✅ Shipped | `src/modules/scheduler/topo_scheduler.h` | DAG 调度器已实现 |
| `ToolRegistry` | ✅ Shipped | `src/common/tools/registry.h` | 工具注册表 (含 `shared_mutex` 读写锁) |
| `EngineResources` | 🔁 Evolved | — (功能分散) | 不可变共享资源的设计意图由 `IProviderFactory` + `IToolRegistry` + `LayeredContext` 抽象接口分摊实现 |
| `ExecutionState` | 🔁 Evolved | `src/modules/scheduler/execution_session.h` | per-execution 状态由 `ExecutionSession` 承载 (PIMPL-lite, Sprint 19 ship) |
| `AgentMessage` | 📅 Deferred | — | ADR §7 明确标注"Phase 2 预留"的 Agent 间 IPC 消息结构 |

## 分类详情

### 🔁 Evolved — `EngineResources`

ADR-0003 §2 描述 `EngineResources` 为持有不可变共享资源的 struct (`graphs` / `tools` / `llm_factory`)。实际实现走了不同的解耦路径:
- 不可变图谱 → `DSLEngine` 内部 `ParsedGraph` vector (PIMPL-lite)
- 共享工具注册表 → `IToolRegistry` 抽象接口 (ADR-0019 §1.4, Sprint 18 P1.T2 ship)
- LLM 工厂 → `IProviderFactory` 抽象接口 (Sprint 18 P1.T1 ship)

设计意图 (不可变共享 + 读写锁保护) 通过抽象接口 + PIMPL 实现, 但未提取为名为 `EngineResources` 的聚合 struct。

### 🔁 Evolved — `ExecutionState`

ADR-0003 §2 描述 `ExecutionState` 为 per-execution 状态 (`scheduler` / `session` / `traces`)。实际由 `ExecutionSession` 类承载 (Sprint 19 `decompose-execution-session-h` ship, PIMPL-lite 拆分)。功能等价, 命名不同。

### 📅 Deferred — `AgentMessage`

ADR-0003 §7 明确标注 `AgentMessage` 为"Phase 2 预留"的 Agent 间 IPC 消息结构。当前 Phase 4.5 未涉及多 Agent 直接通信 (通过 `IInteractionBus` 事件总线间接通信)。

**推迟理由**: 多 Agent 直接 IPC 留待 Phase 5+ 自举服务化。当前 `IInteractionBus` + `ToolResult` 事件载荷已满足间接通信需求。

## 决策

- **ADR 状态**: ✅ Approved (保持)
- **理由**: ADR-0003 核心目标 (多 DSLEngine 实例隔离 + `ToolRegistry` 读写锁 + `library_loader` 竞态修复 + FORK/JOIN `std::jthread` 并发) 全部 Shipped; 3 个缺失类中 2 个 Evolved (功能等价, 命名不同), 1 个 Deferred (Phase 2 明确预留)
- **风险**: 低 — `EngineResources`/`ExecutionState` 的设计意图已通过等价抽象实现

## 后续行动

- Phase 5+ 多 Agent 场景明确后, 评估是否需要 `AgentMessage` IPC 结构 (或继续使用 `IInteractionBus` 事件总线)
- 本 audit 文档供 Phase 5 backlog 参考
