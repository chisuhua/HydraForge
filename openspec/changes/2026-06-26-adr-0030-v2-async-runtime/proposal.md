# Proposal: ADR-0030 V2 — Async Runtime (Phase 2 入口)

> **STATUS: PLACEHOLDER** ⚠️
> **本 change 详细 design/spec/tasks 待前置依赖 C1 (Sprint 11) 完成后填充**
> **触发条件**: C1 (`2026-06-26-sprint-7-tech-debt-execution`) ship + archive
> **关联 ADR**: docs/adr/adr-0030-async-runtime-v2.md (C0 收官后新建, V2 草案)
> **追溯范围**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C2

## Why

ADR-0030 V1 (docs/archive/adr/adr-0030-async-runtime-dual-layer.md) 标注 ❌ Not Implemented 归档原因 = "Taskflow + async_simple 依赖未引入". 但 Slice 00 (docs/implementation-roadmap.md §Slice 00) 已 100% 完成, 依赖实际已引入 (2026-06-07 ship).

Phase 2 (异步架构) 是 docs/implementation-roadmap.md 定义的下一个里程碑, 缺失一个有效 ADR 阻碍 C2 启动. C0 (`2026-06-26-doc-alignment-adr-states`) 将写 ADR-0030 V2 草案, 取代 V1.

**Sprint 12 启动前必须决策的 3 个 Open Questions** (用 Oracle 咨询):

> ⚠️ **2026-06-26 状态修正 (C0 收官 + master plan §十一.1 resolved)**: 以下方案 A 已**过时** — C0 写 ADR-0030 V2 (`docs/adr/adr-0030-async-runtime-v2.md`) 已明确决策 std::jthread (C++20 RAII) 替代 async_simple 协程层, 经 Sprint 2 CognitiveWorker + Sprint 3 DomainWorkerPool 9/9 + 7/7 ctest pass 验证足够. C2 实施时**直接按 V2 ADR 落地**,无需在 3 个 Open Questions 中重决策.

1. ~~**双层架构是否仍适用?**~~
   - ~~方案 A: Taskflow 处理 DAG 节点并行 (短时计算) + async_simple 处理 LLM Token 流 + 用户审批 suspend (长生命周期协程)~~
   - ~~方案 B: std::jthread + std::stop_token 替代 async_simple (Sprint 2 CognitiveWorker + Sprint 3 DomainWorkerPool 已采用)~~
   - **✅ 已决策 (2026-06-26, ADR-0030 V2 §决策 1)**: 采用 **方案 B** (Taskflow + std::jthread Worker Pool + IInteractionBus), async_simple 依赖在 P1 实施时从 CMake 移除 (external/async_simple/ 当前已 ship 但未启用)
2. **Fleet 模式 16 路 LLM 并行的真实业务场景** — 是否有实际使用需求? (待 Oracle 验证 — 决策影响 C2 估时是否含 Slice 04)
3. **LLM Token 流式推送是否用协程 yield** — 现有 `IGenerationStream` 已支持 stream, 因决策 1 选方案 B, 协程 yield 不再需要, 改用 IInteractionBus `llm.token` 事件推送 (ADR-0030 V2 §决策 1)

## What Changes (待 C1 完成后详细制定)

### 大方向 (placeholder)

1. **并行 DAG executor** — topo_scheduler 异步重构, 支持节点级并行派发
2. **Fleet 模式 16 路 LLM 并行** — Slice 04 实施, 依赖 Phase 2 异步基础
3. **LLM Token 流式推送** — 协程 yield (如选方案 A) 或 IGenerationStream 扩展 (如选方案 B)
4. **用户审批等待 (`/apply`)** — 协程 suspend (如选方案 A) 或 EventBus 阻塞 (如选方案 B)
5. **ADR-0030 V2 → ✅ Approved** — C2 实施完成后状态升级

## Capabilities (待详细制定)

### ADDED Requirements (placeholder)

- `async-runtime-concurrent-dag-execution`: DAG 节点 MUST 支持并行派发 (TBD: Taskflow executor / std::jthread)
- `async-runtime-fleet-mode-16x`: 16 路 LLM 调用 MUST 支持并行提交+聚合 (TBD: 真实业务场景)
- `async-runtime-streaming-yield`: LLM Token 流 MUST 支持协程 yield 或 stream 句柄 (TBD: 决策点 1)
- `async-runtime-approval-suspend`: 用户审批 MUST 支持协程 suspend 或 EventBus 阻塞 (TBD: 决策点 1)

## Impact (待 C1 完成后评估)

**预期修改文件**:
- `src/modules/scheduler/topo_scheduler.{h,cpp}` (异步重构)
- `include/agenticdsl/cognitive/cognitive_worker.h` (扩展支持并行)
- `src/common/llm/llm_types.h` (stream 句柄增强)
- 新增可能: `src/common/async/` (Taskflow/async_simple 集成层)

**API 稳定性**:
- `TopoScheduler` 公共 API 必须保持向后兼容
- `CognitiveWorker` 扩展需新方法, 不改旧方法签名

## Non-goals (placeholder)

- **不重写** CognitiveWorker / DomainWorkerPool (Sprint 2/3 ship)
- **不实质化** ADR-0007 (上下文压缩) — 与本 change 并行, 不耦合
- **不重新 base** ADR-0019 / ADR-0020 / ADR-0021 / ADR-0022 (仅追加状态)

## Estimated Effort (placeholder)

参考 docs/implementation-roadmap.md §Phase 2 估时: **1.5-2 周** (Sprint 12 主体)

**决策前置**: Sprint 11 收官前必须用 Oracle 咨询 3 个 Open Questions, 决策结果决定方案 A vs B

## 详细制定 TODO (待 C1 完成后执行)

- [ ] 1. 咨询 Oracle: 3 个 Open Questions 决策
- [ ] 2. 写 ADR-0030 V2 完整 design (基于决策结果)
- [ ] 3. 完善本 change proposal.md (What Changes 详细化)
- [ ] 4. 写 design.md (5 个 Decision: Taskflow 集成模式 / async_simple 用法 / Fleet 16 路 / 协程 vs jthread / approval suspend)
- [ ] 5. 写 tasks.md (10-15 sections, 30-50 tasks)
- [ ] 6. 写 specs/async-runtime/spec.md (5-8 ADDED Requirements)
- [ ] 7. 移除所有 "PLACEHOLDER" 标记, 更新 STATUS 行
- [ ] 8. `openspec validate 2026-06-26-adr-0030-v2-async-runtime` exit 0
- [ ] 9. 更新 master plan C2 状态: ⚪ placeholder → 🟡 active
- [ ] 10. 启动 Sprint 12 实施
