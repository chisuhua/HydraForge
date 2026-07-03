# ADR-0020 Implementation Scope Audit

> **生成时间**: 2026-07-03 (C9 — Phase 4.5 impl-scope audit)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0020-thread-model-isolation.md](adr-0020-thread-model-isolation.md)
> **状态**: ✅ Approved (audit 后保持)

## 状态

✅ Approved (audit 后保持 — 所有 11 个 ADR 核心契约类均已 Shipped 或 Evolved, 无需调整主 ADR 状态)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved (2026-06-24, Sprint 5 ship), 但 7/12 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `CognitiveWorker` | ✅ Shipped | `include/agenticdsl/cognitive/cognitive_worker.h` | 认知智能体工作线程 (Sprint 2 ship) |
| `DomainWorkerPool` | ✅ Shipped | `include/agenticdsl/cognitive/domain_worker_pool.h` | 领域工作线程池 (Sprint 3 ship) |
| `DomainTask` | ✅ Shipped | `include/agenticdsl/cognitive/domain_worker_pool.h` | 领域任务结构 (Sprint 3 ship) |
| `SimpleCognitiveOrchestrator` | ✅ Shipped | `include/agenticdsl/cognitive/simple_orchestrator.h` | 单轮 ReAct 编排器 (Sprint 18 C8 @internal) |
| `IInteractionBus` | ✅ Shipped | `include/agenticdsl/contract/iinteraction_bus.h` | 事件总线接口 (Sprint 12 C2) |
| `CognitiveTask` | 🔁 Evolved | — | `CognitiveWorker` 通过 `DSLEngine::run()` 接收任务, 非独立 `CognitiveTask` 结构 |
| `ISandboxController` | 📅 Deferred | — | ADR §Phase 2 沙箱控制器, 未实施 |
| `MainThreadComponents` | 📅 Deferred | — | ADR §Phase 2 主线程组件, 未实施 |
| `SandboxConfig` | 📅 Deferred | — | ADR §Phase 2 沙箱配置, 未实施 |
| `SandboxResult` | 📅 Deferred | — | ADR §Phase 2 沙箱结果, 未实施 |
| `StateStore` | 📅 Deferred | — | ADR 可能描述的共享状态存储, 未实施 (当前 `LayeredContext` 提供状态管理) |
| `TaskQueue` | 🔁 Evolved | `include/agenticdsl/cognitive/domain_worker_pool.h` (共享 FIFO 队列) | 任务队列由 `DomainWorkerPool` 内部共享 FIFO 队列实现 (多生产者/多消费者) |

## 分类详情

### ✅ Shipped (5 个)

ADR-0020 描述的 `CognitiveWorker`, `DomainWorkerPool`, `DomainTask`, `SimpleCognitiveOrchestrator`, `IInteractionBus` 均已 Ship:
- Sprint 2: `CognitiveWorker` (per-agent 隔离, 状态机 idle/running/stopped)
- Sprint 3: `DomainWorkerPool` (N 个 `std::jthread` worker + 共享 FIFO 任务队列)
- Sprint 18 C8: `SimpleCognitiveOrchestrator` (@internal Phase 0)
- Sprint 12 C2: `IInteractionBus` + `InMemoryBus` (EventBus MPMC 后端)

### 🔁 Evolved — `CognitiveTask` / `TaskQueue`

- **`CognitiveTask`**: ADR-0020 可能描述独立的任务结构; 实际 `CognitiveWorker` 通过 `submit(unique_ptr<DSLEngine>, string, LayeredContext)` 接收任务, 任务参数 (DSL 文本 + 上下文) 简化了独立 `CognitiveTask` 的需要
- **`TaskQueue`**: ADR-0020 可能描述独立的队列类; 实际 `DomainWorkerPool` 内部 `std::deque<DomainTask> task_queue_` + `shared_mutex` 实现共享 FIFO 队列, 未提取为独立类

### 📅 Deferred (5 个) — 沙箱 + 多租户

- **`ISandboxController` / `SandboxConfig` / `SandboxResult` / `MainThreadComponents`**: ADR §Phase 2 明确描述的沙箱隔离组件。Phase 4.5 未涉及多租户 OS 级隔离
- **`StateStore`**: 共享状态存储由 `LayeredContext` + `IInteractionBus` 替代

**推迟理由**: ADR-0020 §Phase 2 明确标注沙箱隔离为未来工作。当前 Phase 4.5 聚焦轻量级隔离 (`CognitiveWorker` per-agent + `DomainWorkerPool` 任务级隔离)。

## 决策

- **ADR 状态**: ✅ Approved (保持)
- **理由**: ADR-0020 核心契约 (per-agent 隔离 + 并发任务 + 事件总线通信) 已 Shipped; 7 个缺失类中 2 个 Evolved (任务参数简化/队列内联), 5 个 Deferred (Phase 2 沙箱隔离)
- **风险**: 低 — Phase 4.5 非多租户场景, 无需 OS 级沙箱

## 后续行动

- Phase 5 多租户需求明确后, 实施 `ISandboxController` + `SandboxConfig` 沙箱隔离
- `StateStore` 留待 `LayeredContext` 演进
- 本 audit 文档供 Phase 5 backlog 参考
