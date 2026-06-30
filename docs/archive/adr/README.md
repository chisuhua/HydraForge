# 归档 ADR 索引 (Archived ADRs)

> 本目录存放已废弃、已替代、或已不活跃的 ADR 文件。
> 归档于 2026-06-12，作为 project-organization 计划 Stage 2 / Task 7 的产出。
> 文件保留完整可追溯；不删除。

## Phase 2: 记忆系统（已废弃）

| ADR | 议题 | 归档原因 |
|-----|------|----------|
| `adr-0010-memory-system.md` | 记忆系统标准接口 | ❌ Not Implemented (2026-05-13) |
| `adr-0011-knowledge-graph.md` | 知识图谱与 Meta-KG 导航 | ❌ Not Implemented |
| `adr-0012-vector-memory.md` | 向量语义记忆 | ❌ Not Implemented |
| `adr-0013-user-profile.md` | 用户画像管理 | ❌ Not Implemented |
| `adr-0014-conversation-context.md` | 对话上下文隔离 | ❌ Not Implemented (已被 `proposals/session-state/` 演进方向取代) |

## Phase 3: 推理能力（已废弃）

| ADR | 议题 | 归档原因 |
|-----|------|----------|
| `adr-0015-iper-loop.md` | IPER 闭环推理 | ❌ Not Implemented |
| `adr-0016-try-catch.md` | 异常自动快照回溯 | ❌ Not Implemented |
| `adr-0017-counterfactual.md` | 反事实推理 | ❌ Not Implemented |
| `adr-0018-graph-guided.md` | 图引导假设生成 | ❌ Not Implemented |

## Phase 5-8: 异步/策略/路由/内核（已废弃）

| ADR | 议题 | 归档原因 |
|-----|------|----------|
| `adr-0030-async-runtime-dual-layer.md` | 异步运行时双层架构 | ❌ Not Implemented (V1, **SUPERSEDED by V2** at 2026-06-26 — 见 [`docs/adr/adr-0030-async-runtime-v2.md`](../adr/adr-0030-async-runtime-v2.md)) |
| `adr-0032-cost-collector.md` | 成本收集器 | ✅ Approved (2026-06-30 提升, 4 核心类 ship 2026-06-14 `test_cost_collector.cpp`, `BudgetController::CostTracker` 集成推迟到 C8) |
| `adr-0036-hybrid-kernel-architecture.md` | 混合内核架构 | ❌ Not Implemented (愿景性, 依赖多个前置 ADR) |

> 2026-06-16 变更：`adr-0034-model-router.md` 因重新定位为「Plugin 化实施候选」移出本目录至 [`docs/adr/plugin/`](../adr/plugin/README.md)，状态由 ❌ Not Implemented 调整为 🔍 Proposed (plugin-candidate)。归档目录不再保留此 ADR。

## 维护规则

- 归档 ADR 永不删除
- 状态变更需在 `docs/adr-management/STATUS-GLOSSARY.md` 中记录
- 如需恢复某个 ADR, 通过 `git mv` 移回 `docs/adr/`
- 若 ADR 被重新定位为 plugin 候选，移至 `docs/adr/plugin/` 并按 `docs/adr/plugin/README.md` 维护规则调整 frontmatter