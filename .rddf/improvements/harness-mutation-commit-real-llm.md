# harness-mutation-commit-real-llm

> **来源**: 2026-10-06 全量回归测试完备性审查 (`AGENTS.md §Recent Changes` entry 自识别盲点 G4)
> **生成**: 2026-10-06 via completeness-audit
> **AGENTS.md Pattern**: #10 hygiene (systematic recording of deferred work)

**优先级**: 🔴 P0 | **阶段**: 自由 (phase-n/a) | **分类**: 真实 LLM 验证盲点
**类型**: debt-tracking | **主题**: Harness mutation commit + IGenomeRegistry persist 真实 LLM 端到端
**状态**: pending (Genome wiring ship 后真实 LLM 闭环未验证)
**关联**:
- `openspec/changes/archive/genome-wiring-harness-rsi-gepa-2026-09-22/` (闭环第 7 环 ship)
- `tests/test_harness_rsi_pilot.cpp` (22/22 mock)
- `tests/test_genome_registry.cpp` (13/13 mock)
- `tests/test_harness_mutation_proposal_real_llm.cpp` (1/1 real, 仅 smoke)

## 背景

Genome wiring 闭环第 7 环"版本提交/发布"端到端 ship (2026-09-22), 但真实 LLM 验证仅覆盖
"突变提案"环节 (`test_harness_mutation_proposal_real_llm` smoke), 未覆盖完整路径:

```
LLM 生成 prompt_delta
  → MutationGate Gate 0-3 真实 LLM 评分
    → IGenomeRegistry fork() persist
      → apply_harness_mutation 内存应用
        → genome.committed 事件 + walk_ancestors 关联
```

## 缺口

Harness 子系统 (per `harness-architecture-2026-09.md` §十二 H1-H6):
- H1 mutation 提案生成: ✅ real LLM smoke
- H2 mutation 审批 (MutationGate): ⚠️ mock only
- H3 mutation commit + IGenomeRegistry 持久化: ⚠️ mock only
- H4 evolution.readiness.denied: ⚠️ mock only
- H5 反作弊 R9: ⚠️ mock only (见 G5)
- H6 4-tier gate 反向校验 + drop_ratio: ⚠️ mock only

按 AGENTS.md §Reverse Indicator Rule R3 元指标实证要求, 当前仅"提案生成"环节
真实 LLM 验证, 真实 LLM 端到端 commit → persist → reload → re-eval 链路未验证.

## 触发条件

任何下列条件满足即升级 P0 → 立即立项:
1. 用户要求 "Harness 真实 LLM 端到端" 验证
2. 自进化 v1.5 9 段闭环 §R3 元指标需要实证
3. Wave 3 Phase 2 D4 (LoRA 训练管线) 启动需要 harness 真实 LLM 闭环数据

## 实施建议 (非实施, 待立项 OpenSpec change)

新增 `openspec/changes/2026-XX-harness-mutation-commit-real-llm/`:
- 新增 `tests/test_harness_mutation_commit_real_llm.cpp` (must_realllm label)
  - Case 1: 真实 LLM 生成 mutation proposal → Gate 0-3 真实 LLM 评分 → commit → reload → 状态一致
  - Case 2: 真实 LLM mutation commit 触发 `genome.committed` 事件 + walk_ancestors 验证
  - Case 3: 真实 LLM mutation commit 失败回滚 (gate denied) → IGenomeRegistry 零状态变更
  - Case 4: 真实 LLM mutation commit + persist 后 reload, drop_ratio ∈ 合理范围
- 预估 effort: 2-3 天 (Genome wiring 已 ship, 只需补真实 LLM 验证层)
- 依赖: `tests/test_harness_rsi_pilot.cpp` 复用 + `tests/test_genome_registry.cpp` 复用
- 不需要新基础设施, 仅 test 覆盖补全
