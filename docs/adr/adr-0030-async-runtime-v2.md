# ADR-0030 V2: Phase 2 异步运行时（Taskflow DAG + std::jthread Worker Pool）

> 📋 **Phase 2: 并行执行与舰队模式** (Sprint 12 启动, 2026-06-26 创建) — V2 版，替代归档的 V1 (`docs/archive/adr/adr-0030-async-runtime-dual-layer.md`).
>
> **变更原因**: V1 (2026-05-27) 标注 "Taskflow + async_simple 依赖未引入"，归档至 `docs/archive/adr/`。2026-06-07 Slice 00 已 ship (`docs/implementation-roadmap.md` §Slice 00)，Taskflow v4.0 + async_simple v1.4 已引入；V1 归档理由过时。
> **变更范围**: V1 的"双层异步架构"假设（Taskflow 计算层 + async_simple 协程控制层）经 Sprint 2/3 CognitiveWorker + DomainWorkerPool 验证后调整为 "Taskflow DAG + std::jthread Worker Pool"，去掉 async_simple 协程层。详细决策见 §决策 1。

## 状态

**🔍 Proposed** (2026-06-26, OpenSpec change `2026-06-26-doc-alignment-adr-states` 收官产出)

> **Sprint 12 收官目标**: 状态变更 🔍 Proposed → ✅ Approved (随 C2 OpenSpec change `2026-06-26-adr-0030-v2-async-runtime` 实施完成)

## 领域

基座 / 并发执行模型 / 异步运行时

## 关联

- ADR-0002（EventBus 抽象层）— 事件分发支撑
- ADR-0019（IInteractionBus）— 跨 Worker 通信契约
- ADR-0020（线程模型与隔离）— Per-Worker 独占 DSLEngine / 共享 Worker Pool
- ADR-0025（并行子任务）— Fleet 模式 16 路并行的协议依据
- ADR-0036（混合内核架构）— 上下层契约（V2 替代 V1 作为 V3.6 §2 的引用源）

## 替代关系

**本 ADR V2 替代 ADR-0030 V1**（`docs/archive/adr/adr-0030-async-runtime-dual-layer.md`，V1.2，2026-05-27 归档）。

V1 提出的"双层异步架构"（Taskflow 计算层 + async_simple 控制层）经 Sprint 2/3 (2026-06-18/19) CognitiveWorker + DomainWorkerPool 实际验证后调整为：
- **保留 Taskflow**：DAG 节点并行调度（Phase 2 核心需求）
- **替换 async_simple 协程层**：Sprint 2/3 已采用 `std::jthread` + `std::stop_token` 实现 Per-Worker 生命周期管理（验证：`tests/test_cognitive_worker.cpp` 9/9 通过, `tests/test_domain_worker_pool.cpp` 7/7 通过），无需引入 async_simple 协程库
- **理由**：async_simple 引入需编译 ~10 个 .cpp（CMake 复杂度↑），而 `std::jthread` (C++20) RAII + stop_token 已能覆盖 Worker 生命周期场景；Token 流推送 / 用户审批 suspend 等长生命周期场景改用 `IInteractionBus` 事件推送（ADR-0019）

---

## 背景

### 当前代码库状态（2026-06-26）

| 组件 | 现状 | 评估 |
|------|------|------|
| `DSLEngine::run()` | 同步阻塞，无并发 | ✅ 符合 MVP；Phase 2 需引入并行 DAG |
| `TopoScheduler::execute()` | 单线程 `while` 循环串行 | 🟡 MVP 通过；Phase 2 需 Taskflow 并行化 |
| `Fork/Join` | 假并行——分支在 `while` 循环中逐个执行 | 🔴 Phase 2 P1 修复 |
| **线程原语** | `std::jthread` (Sprint 2/3)、`std::stop_token` | ✅ 已采用 C++20 RAII |
| `CognitiveWorker` (Sprint 2) | per-agent 隔离，状态机 idle/running/stopped | ✅ 9/9 ctest pass (commit `d69e2d9`) |
| `DomainWorkerPool` (Sprint 3) | N 个 std::jthread worker + 共享 FIFO 队列 | ✅ 7/7 ctest pass (commit `0c44a18`), 1000x 并发 TSan 干净 |
| **Taskflow v4.0** | header-only，external/ 已引入 | ✅ Slice 00 已 ship (S0.1) |
| **async_simple v1.4** | external/ 已引入但**未使用** | ⚠️ V1 假设的协程层未启用；本 V2 决策移除依赖 |
| `Context` | 共享可变状态，按引用传递并就地修改 | 🟡 Phase 2 P3 需 `fork()`/`merge()` 不可变分支 |
| `build_dag()` | 运行时全局重建（`clear()` + 全量重建） | 🔴 Phase 2 P4 需增量 DAG 更新 |

### 需求（与 V1 §需求 表一致）

| 需求 | 特征 | V2 方案归属 |
|------|------|------------|
| DAG 节点并行执行 | 短时计算、确定性拓扑 | **Taskflow** |
| 舰队模式 16 路 LLM 调用 | IO 密集、全部完成后聚合 | **DomainWorkerPool** (Sprint 3 已 ship, 16 路可配置) |
| LLM Token 流式推送 | 增量数据、长生命周期 | **IInteractionBus** 事件推送 (ADR-0019) |
| 用户审批等待 (/apply) | 外部事件、不确定时长 | **IInteractionBus** `tool.approval.requested/responded` 事件 |
| IPER 循环控制 | 有限状态机、条件转移 | **CognitiveWorker** (Sprint 2 已 ship) |
| 优先级响应（用户中断） | 可抢占、即时响应 | **std::stop_token** + CognitiveWorker stop() |

> **关键差异（V1 vs V2）**：V1 用 async_simple 协程 yield 处理"长生命周期/外部事件等待"；V2 用 `IInteractionBus` 事件推送 + Worker stop_token 处理相同场景——解耦更彻底，依赖更轻。

---

## 决策 1: Taskflow (计算) + std::jthread Worker Pool (控制)

### 双层架构保留（精简版）

```
┌──────────────────────────────────────────────────────────────┐
│  L2: 控制层 — std::jthread Worker Pool + IInteractionBus    │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │ • CognitiveWorker（per-agent 隔离，Sprint 2 ship）        │ │
│  │ • DomainWorkerPool（多消费者 FIFO，Sprint 3 ship）       │ │
│  │ • IInteractionBus 事件推送（ADR-0019 ship）              │ │
│  │ • std::stop_token 优先级中断（C++20 RAII）                │ │
│  └──────────────────────────────────────────────────────────┘ │
├──────────────────────────────────────────────────────────────┤
│  L1: 计算层 — Taskflow                                        │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │ • DAG 节点并行执行（Taskflow graph）                       │ │
│  │ • Fork/Join 子图（Subflow）                                │ │
│  │ • 纯计算并行（parallel_for：模板渲染、AST 解析）          │ │
│  │ • Context fork/merge 不可变分支（Phase 2 P3）             │ │
│  └──────────────────────────────────────────────────────────┘ │
├──────────────────────────────────────────────────────────────┤
│  L0: 基础设施                                                  │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │ • tf::Executor（work-stealing 线程池，可配置大小）         │ │
│  │ • std::jthread × N（Cognitive/Domain Worker）              │ │
│  │ • IInteractionBus（InMemoryBus MVP + Phase 2 EventBus）   │ │
│  └──────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

### 职责划分原则（V2 决策树）

```
需要中途挂起/恢复外部事件？── Yes ──→ IInteractionBus 事件推送 (ADR-0019)
     │
     No
     │
需要 DAG 依赖调度？──── Yes ──→ 计算层（Taskflow）
     │
     No
     │
是否 IO 阻塞 > 100ms？── Yes ──→ DomainWorkerPool（FIFO 队列）
     │
     No
     │
纯 CPU 计算 ────────────────→ 计算层（Taskflow executor.async()）
```

### V2 决策 1 vs V1 决策对比

| 维度 | V1 (2026-05-27) | **V2 (2026-06-26)** | 决策依据 |
|------|----------------|---------------------|---------|
| 计算层 | Taskflow v4.0 | **Taskflow v4.0** | ✅ 不变（S0.1 已 ship） |
| 控制层 | async_simple v1.4 协程 | **std::jthread + IInteractionBus** | Sprint 2/3 验证 std::jthread RAII 已足够；async_simple 协程层过度设计 |
| 长生命周期 Token 流 | `AsyncGenerator<T>` `co_yield` | **IInteractionBus `llm.token` 事件推送** | ADR-0019 已 ship；解耦更彻底 |
| 用户审批等待 | `co_await event` 协程挂起 | **IInteractionBus `tool.approval.requested/responded` 事件** | 同上 |
| 优先级中断 | 协程调度器 priority queue | **std::stop_token + Worker stop()** | C++20 原生，无第三方依赖 |
| 编译复杂度 | async_simple ~10 个 .cpp | **0 第三方协程库** | 头文件 + stdlib only |
| 测试覆盖 | V1 未实施 | **CognitiveWorker 9/9 + DomainWorkerPool 7/7** | Sprint 2/3 已 ship 验证 |

---

## 决策 2: Phase 2 实施范围

### P1: TopoScheduler Taskflow 并行化

| # | 文件 | 操作 | 状态 | 验证 |
|---|------|------|:----:|------|
| 2.1 | `src/modules/scheduler/topo_scheduler.h` | 修改 | [ ] | 增加 `execute_parallel(Context, tf::Executor&)` |
| 2.2 | `src/modules/scheduler/topo_scheduler.cpp` | 修改 | [ ] | Taskflow `emplace` DAG；Fork/Join 改 Subflow |
| 2.3 | `tests/test_parallel_scheduler.cpp` | 新建 | [ ] | 16 路并行 DAG < 100ms (mock), TSan 干净 |

### P2: 舰队模式 16 路并行（基于 DomainWorkerPool 扩展）

| # | 文件 | 操作 | 状态 | 验证 |
|---|------|------|:----:|------|
| 2.4 | `src/common/llm/fleet_orchestrator.h` | 新建 | [ ] | `FleetOrchestrator` 复用 DomainWorkerPool |
| 2.5 | `src/common/llm/fleet_orchestrator.cpp` | 新建 | [ ] | 分片→submit→聚合→FleetResult |
| 2.6 | `examples/slice_04_fleet/main.cpp` | 新建 | [ ] | 端到端 16 路 LLM mock 调用 < 500ms |
| 2.7 | `tests/test_fleet_orchestrator.cpp` | 新建 | [ ] | 并行调用单元测试 + TSan |

### P3: Context 线程安全 + 增量 DAG 更新

| # | 文件 | 操作 | 状态 | 验证 |
|---|------|------|:----:|------|
| 2.8 | `src/core/types/context.h` | 修改 | [ ] | 增加 `fork()` / `merge()` (深拷贝/合并策略) |
| 2.9 | `src/core/types/context.cpp` | 新建 | [ ] | `fork()` 实现 (Layer 不可变快照) |
| 2.10 | `src/modules/scheduler/topo_scheduler.cpp` | 修改 | [ ] | `append_node()` 增量 DAG 更新 |
| 2.11 | `tests/test_context_fork.cpp` | 新建 | [ ] | fork/merge 正确性 + 并发安全 |

### P4: IInteractionBus Phase 2 后端切换（EventBus 接入）

| # | 文件 | 操作 | 状态 | 验证 |
|---|------|------|:----:|------|
| 2.12 | `src/common/contract/inmemory_bus.cpp` | 修改 | [ ] | 后端切换为 EventBus (MPMC 有界队列) |
| 2.13 | `tests/test_interaction_bus.cpp` | 扩展 | [ ] | 1000x 并发 emit 无丢失（已有 28/28, 需扩展边界测试） |

---

## Open Questions（待 Sprint 12 启动时 Oracle 咨询）

> 以下 3 个决策点 V2 暂时锁定 Sprint 2/3 已 ship 的方案，**Sprint 12 启动前用 Oracle 审查**是否需要扩展：

| # | 问题 | 当前 V2 决策 | 待审查方向 |
|---|------|-------------|----------|
| OQ1 | 协程需求？async_simple 完全不引入？ | 否（用 IInteractionBus 替代） | 如发现需要背压控制 / 超时取消细粒度，可能需局部协程 |
| OQ2 | DomainWorkerPool 是否够用做 Fleet 16 路？ | 是（P2 复用） | 如发现需要结果流式聚合 / 部分失败恢复，需独立 FleetOrchestrator |
| OQ3 | Taskflow 协程扩展（v5.0 实验性）？ | 否（v4.0 已 ship） | 跟踪 upstream Taskflow v5 RFC |

---

## 与现有代码的集成策略

### Phase 0 已 ship（Slice 00, 2026-06-07）

- ✅ Taskflow v4.0 (`external/taskflow/`) header-only
- ✅ async_simple v1.4 (`external/async_simple/`) 引入但未使用（V2 决策不启用）
- ✅ `test_async_bridge.cpp` Taskflow 基础 + async_simple 协程并存验证

### Phase 2 实施（依赖 C1 ship gate, Sprint 12）

C1 OpenSpec change `2026-06-26-sprint-7-tech-debt-execution` (3 周) 完成 engine.cpp include 简化后，本 V2 启动实施：
1. **P1 Week 1**: Taskflow DAG 并行调度
2. **P2 Week 1-2**: 舰队模式 16 路
3. **P3 Week 2**: Context fork/merge
4. **P4 Week 3**: IInteractionBus 后端切换 + E2E 验证

---

## 线程模型与资源配置（V2 修订）

```
┌─────────────────────────────────────────────────────────┐
│  进程内线程布局 (V2)                                      │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  主线程 (Main Thread)                                    │
│  ├── 事件循环入口                                        │
│  └── DSLEngine 同步入口 (向后兼容)                        │
│                                                          │
│  CognitiveWorker × N (std::jthread, per-agent 隔离)      │
│  ├── IPER 循环 / 用户审批 suspend                         │
│  └── 持有 unique_ptr<DSLEngine> (Sprint 2 ship)          │
│                                                          │
│  DomainWorkerPool × M (std::jthread, 多消费者 FIFO)       │
│  ├── 16 路并行 LLM 调用（Fleet 模式）                     │
│  ├── 事件 handler 并行分发                                │
│  └── 共享 mutex 保护的 handler 注册表（Sprint 3 ship）    │
│                                                          │
│  Taskflow 计算池 × K (tf::Executor, K = CPU cores)       │
│  ├── DAG 节点并行执行                                    │
│  ├── 模板渲染 / AST 解析                                  │
│  └── Fork/Join Subflow                                   │
│                                                          │
└─────────────────────────────────────────────────────────┘

总线程数 ≈ N (cognitive) + M (domain, 默认 16) + K (taskflow)
N + M + K = ~2 + 16 + 8 = 26 (8核机器, M 可配置)
```

---

## 与纯方案的对比（V2 决策记录）

| 维度 | 纯 Taskflow | std::jthread only | **Taskflow + std::jthread + Bus** (V2) |
|------|:----------:|:-----------------:|:-------------------------------------:|
| DAG 并行 | ⭐⭐⭐⭐⭐ | ⭐⭐ | ⭐⭐⭐⭐⭐ |
| 16 路 LLM 并行 | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| Token 流式推送 | ⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ (IInteractionBus) |
| 用户审批等待 | ⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ (IInteractionBus) |
| 实现复杂度 | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ |
| 依赖重量 | 轻 | 轻 | **轻（无 async_simple 协程）** |
| 测试覆盖 | V1 未实施 | Sprint 2/3 16/16 | **Sprint 2/3 16/16 + Phase 2 增量** |

---

## 技术风险与缓解

| 风险 | 严重度 | 缓解措施 |
|------|--------|---------|
| std::jthread 停止延迟 | 中 | stop_token 主动轮询 + 5s timeout |
| DomainWorkerPool 任务饿死 | 中 | FIFO + 超时提交 + backpressure |
| Taskflow 子图依赖冲突 | 低 | Subflow 隔离 + 测试覆盖 |
| Context fork 内存占用 | 中 | Layer 浅拷贝 + immutable snapshot |
| IInteractionBus 后端切换回归 | 中 | 抽象稳定 + 28/28 测试基线 |

---

## 与 V1 的关键差异（迁移指南）

| V1 设计点 | V2 替换方案 | 影响代码 |
|----------|------------|---------|
| `async_simple::coro::Lazy<T>` | `std::future<T>` 或 IInteractionBus 回调 | 无（P1 实施时按 V2 设计） |
| `async_simple::coro::Generator<T>` | IInteractionBus `llm.token` 事件订阅 | `src/modules/executor/node_executor.cpp` |
| `async_simple::Executor` 协程调度 | std::jthread + DomainWorkerPool | `src/common/worker/*` |
| `await_taskflow()` 桥接 | tf::Future 直接 `.get()` 或回调 | 无（V2 不需要协程桥接） |
| `await_future<T>()` | `std::future<T>::wait_for` + timeout | 同步路径保持 |

**V2 移除 `external/async_simple/` 依赖**（CMake）：V1 的 `add_subdirectory(external/async_simple)` 在 P1 实施时移除（async_simple 引入但未启用，移除后 `test_async_bridge.cpp` 同步精简为仅 Taskflow 基础测试）。

---

## 后续行动

- [ ] Sprint 11 (C1) ship 后，启动 OpenSpec change `2026-06-26-adr-0030-v2-async-runtime` (C2) 实施 P1-P4
- [ ] Oracle 咨询 3 个 Open Questions（OQ1-OQ3）
- [ ] P1: TopoScheduler 并行化 + `test_parallel_scheduler.cpp` (Week 1)
- [ ] P2: FleetOrchestrator + `test_fleet_orchestrator.cpp` (Week 1-2)
- [ ] P3: Context fork/merge + `test_context_fork.cpp` (Week 2)
- [ ] P4: IInteractionBus 后端切换 (Week 3)
- [ ] E2E: `examples/slice_04_fleet` 端到端验证 (Week 3 末)
- [ ] 移除 `external/async_simple/` 依赖（CMake + `test_async_bridge.cpp` 精简）
- [ ] ADR-0030 V2 状态变更：🔍 Proposed → ✅ Approved (P1-P4 ship 后)

---

## 决策记录

| # | 问题 | 最终决策 |
|---|------|---------|
| 1 | 并发模型选择 | **Taskflow (DAG/计算) + std::jthread Worker Pool (控制) + IInteractionBus (事件)** |
| 2 | 协程库引入？ | **否 — 用 IInteractionBus 替代（ADR-0019）** |
| 3 | IO 线程池大小 | DomainWorkerPool 默认 16，可配置 |
| 4 | Future 是否支持 .then() | 不需要——IInteractionBus 事件订阅替代 |
| 5 | async_simple 依赖处置 | **移除**（V1 引入但未启用；V2 不用协程） |
| 6 | 与 DSLEngine 的关系 | Worker 持有 `unique_ptr<DSLEngine>` (Sprint 2 模式) |
| 7 | stop_token 传播 | per-Worker `std::stop_token` (C++20) |
| 8 | Taskflow 版本 | v4.0（header-only, Slice 00 ship） |
| 9 | 与 V1 关系 | V2 替代 V1；V1 标注 SUPERSEDED 保留归档 |

---

## 变更记录

| 版本 | 日期 | 变更内容 |
|------|------|---------|
| V2 | 2026-06-26 | 初版，基于 Sprint 2/3 (CognitiveWorker + DomainWorkerPool) 实际验证，移除 async_simple 协程层，调整为 Taskflow + std::jthread + IInteractionBus 双层架构 |
| V1 | 2026-05-27 | V1 双层架构（Taskflow + async_simple），已归档 → `docs/archive/adr/adr-0030-async-runtime-dual-layer.md` |

---

## 参考

- V1 归档: [docs/archive/adr/adr-0030-async-runtime-dual-layer.md](../archive/adr/adr-0030-async-runtime-dual-layer.md) — 历史决策保留
- [ADR-0002: EventBus 有界队列架构](./adr-0002-eventbus-bounded-queue.md)
- [ADR-0019: IInteractionBus 接口与 TUI Chat MVP 架构](./adr-0019-iinteraction-bus-mvp.md)
- [ADR-0020: 多智能体线程模型与隔离策略](./adr-0020-thread-model-isolation.md)
- [ADR-0025: 并行子任务](./adr-0025-parallel-subtasks.md) — Fleet 协议依据
- [ADR-0036: 混合内核架构总纲](./adr-0036-three-layer-service-protocol.md)
- [Slice 00 验证报告](../implementation-roadmap.md#slice-00--基础设施验证) — Taskflow + async_simple 引入
- OpenSpec change: `2026-06-26-doc-alignment-adr-states` (本 V2 产出)

---

*文档版本: V2.0*
*最后更新: 2026-06-26*