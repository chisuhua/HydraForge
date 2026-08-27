# Design: t19-gepa-phase2-commit

## Context

T19 GEPA MVP Phase 1（只读反思约束）已 ship，所有前置 2026-08-26/27 完成：
- G11 ✅ Closed（commit `a2b2d52` + `314561e` + issue #14 Closed）
- IEvaluator V1 ship（commit `21dd622`）+ V2 ship（commit `314561e`）
- T17 SkillCompiler ship（commit `21dd622`）
- T15 Trajectory IR ship（commit `9c7c6da`）
- T14 行为回归 ship
- ADR-0071 ✅ Approved（方向 ADR）

Phase 2 commit 启动**前置完全就绪**。本 change 实施 GEPA MVP V1 编排引擎，使用现有契约作为依赖，无既有契约修改。

## Scope Boundaries

### 范围 IN
- GEPALoop 编排引擎（`src/modules/cognitive/gepa_loop.{h,cpp}`）
- 6 个 GEPA 事件主题注册（ADR-0068 附录 A v1.2.2 → v1.3）
- ≥ 8 测试 cases（`tests/test_gepa_phase2.cpp`）
- MockILLMProvider 辅助类（避免外部依赖）
- 文档同步（ADR-0071 + cap-map + active-status）

### 范围 OUT
- 既有契约修改（IEvaluator / MutationGovernor / TrajectoryIR / SkillCompiler / BehaviorRegression）
- CognitiveWorker/DomainWorkerPool 修改
- L4 权重支持（G11 V1 显式禁止）
- 多 agent 协同反思循环（V2）
- 异步 commit 路径（V2）
- 跨 session 经验积累（V2）
- TrajectoryFidelity 评估（依赖 T15 V2 schema）
- Pareto 多目标评估（依赖 IEvaluator V3+）

## Design Decisions

### D1 — 编排层而非契约层

GEPALoop 是**编排层**（orchestrator），不是契约层。它调用现有 API：
- `IEvaluator::evaluate()` + `compare()` → 评估信号
- `MutationGovernor::propose()` + `commit()` → 变异授权
- `TrajectoryIR::from_parsed_graph()` → 失败轨迹序列化
- `SkillCompiler::compile()` → 修订候选生成
- `BehavioralRegressionGate::hotelling_t2_test()` → 回归门禁
- `ILLMProvider::generate()` → 反思 prompt 生成

理由：既有契约已 ship 且稳定，GEPALoop 纯组合，无需修改。

### D2 — 同步循环（V1 简化）

`reflect_and_commit()` 全同步阻塞：
1. propose → 序列化 → 生成 → 编译 → 回归 → 评估 → commit/deny
2. 迭代 max_iterations=3 次
3. 任一迭代成功即返回；全部失败返回 success=false

理由：V1 简化避免并发复杂度，V2 异步化 deferred。

### D3 — Mock ILLMProvider

E2E 测试使用 MockILLMProvider：
```cpp
class MockILLMProvider : public ILLMProvider {
    std::string generate(const std::string& prompt) override {
        return "Reflection note: Add error handling for " + last_failure_;
    }
};
```

理由：避免外部 LLM API 依赖，确保测试可重现且零外部状态。

### D4 — 默认 source_id = "R_T19_GEPA"

MutationGovernor::propose() 必须传入白名单 source_id，V1 默认 "R_T19_GEPA"：
- 通过 G11 MutationGovernor 白名单 fail-closed 门禁
- 评估信号归属明确（T19 任务）
- 与 cap-map §三 B7 行 "T19 GEPA spike" 一致

### D5 — 评估门禁使用 IEvaluator V2 CompositeEvaluator

建议注入顺序：
1. TaskSuccessEvaluator (V1, 权重 0.3) — 基线奖励
2. BehavioralEquivalenceEvaluator (V2, 权重 0.7) — 修订质量

加权聚合：
- scalar 加权平均 = 0.3 * task_success + 0.7 * behavioral_equivalence
- quality 众数
- confidence min

理由：BehavioralEquivalence 权重高于 TaskSuccess（修订质量更关键）。

### D6 — 6 个 GEPA 事件主题（ADR-0068 v1.3）

| 主题 | Owner | Payload |
|------|-------|---------|
| `gepa.reflection.started` | GEPALoop | `reflection_id`, `failure_mode`, `trace_id` |
| `gepa.reflection.completed` | GEPALoop | `reflection_id`, `candidate_skill`, `evaluation_refs` |
| `gepa.reflection.failed` | GEPALoop | `reflection_id`, `failure_reason` |
| `gepa.commit.proposed` | GEPALoop | `commit_id`, `mutation_kind`, `subject_ref`, `evaluation_refs` |
| `gepa.commit.committed` | GEPALoop | `commit_id`, `mutation_id`, `evaluation_refs` |
| `gepa.commit.denied` | GEPALoop | `commit_id`, `denial_reason`, `failed_step` |

理由：补全 GEPA 循环的完整事件流（启动 → 反思 → 提议 → 提交/拒绝）。

### D7 — V1 边界遵守

- ❌ L4 权重（MutationGovernor V1 显式禁止）
- ❌ 异步 commit（V2 简化）
- ❌ 多 agent 协同（V2）
- ❌ 跨 session 经验（V2）
- ❌ 在线权重微调（V2 + L4 解锁）
- ❌ Pareto 评估（V3+）
- ❌ TrajectoryFidelity 评估（T15 V2）

## Risks

| 风险 | 缓解 |
|---|---|
| LLM 修订候选质量差 | Mock 注入固定候选 + max_iterations 限制 |
| 回归门禁误判 | 复用 T14 已 ship 实现（6 cases PASS）|
| MutationGovernor 拒绝 | 白名单 source_id 默认 + G11 V1 已 ship 验证 |
| ctest 数字硬编码 | tasks.md 禁止 + 动态基线 |
| CognitiveWorker 误修改 | git diff 强制验证 |
| docs_drift_audit 引入新 drift | Phase 4 验证 |

## Verification Gates

- ≥ 8 cases test_gepa_phase2 PASS
- 既有契约 0 diff
- ctest 188+ 全量零回归（动态计数）
- adr_lint 82 ADR PASS
- docs_drift_audit 0 NEW drift
- ADR-0068 v1.3 附录 A 6 主题

## Dependencies

### 满足
- ✅ G11 变异治理 ship (commit `a2b2d52`)
- ✅ IEvaluator V1+V2 ship (commit `21dd622` + `314561e`)
- ✅ T17 SkillCompiler ship (commit `21dd622`)
- ✅ T15 Trajectory IR ship (commit `9c7c6da`)
- ✅ T14 行为回归 ship
- ✅ ADR-0071 ✅ Approved (方向 ADR)

### 不依赖
- T20 AFlow (T20 依赖 T19 完成后)
- T21 Prompt Evidence Gate (独立轨道)
- T22 Fine-tune (事件驱动，独立)

## Out of Scope (V2+ deferred)

- 多 agent 协同反思循环
- 异步 commit 路径
- 跨 session 经验积累
- 在线权重微调
- Pareto 多目标评估
- TrajectoryFidelity 评估
- 真实 LLM API 调用

## Success Criteria

- GEPA MVP V1 ✅ Shipped
- 既有契约 0 diff
- 8+ cases PASS
- ctest 189+ 全量零回归
- B7 自进化基础应用 MVP 落地
- OpenSpec archive 完成