# t19-gepa-phase2-commit Specification

## ADDED Requirements

### Requirement: GEPALoop 是单 agent 反思 + commit 编排引擎

The `GEPALoop` class MUST orchestrate the failure→reflection→revision→regression→commit cycle using existing contracts (IEvaluator V2, MutationGovernor, TrajectoryIR, SkillCompiler, BehavioralRegressionGate, ILLMProvider). MUST NOT modify any existing contract public API.

#### Scenario: GEPALoop 契约骨架

- **WHEN** 静态检查 `grep "class GEPALoop" include/agenticdsl/cognitive/gepa_loop.h`
- **THEN** GEPALoop 必须在 `agenticdsl` 命名空间内
- **AND** 必须接受 `shared_ptr<IEvaluator>` + `shared_ptr<MutationGovernor>` + `shared_ptr<ILLMProvider>` 构造注入

#### Scenario: 既有契约零修改

- **WHEN** `git diff HEAD~1 -- include/agenticdsl/contract/ievaluator.h include/agenticdsl/contract/imutation_governance.h include/agenticdsl/ir/trajectory_ir.h include/agenticdsl/cognitive/skill_compiler.h include/agenticdsl/testing/behavioral_regression.h`
- **THEN** 不应有 V1 implementation 相关 diff（既有契约必须 0 修改）

### Requirement: 失败→反思→修订→回归→commit 完整循环

The `reflect_and_commit(failed_trace)` MUST execute the full cycle:
1. `MutationGovernor::propose()` (L1 gate)
2. `TrajectoryIR::from_parsed_graph()` 序列化失败轨迹
3. `ILLMProvider::generate()` 生成修订候选
4. `SkillCompiler::compile()` 生成新 skill
5. `BehavioralRegressionGate::compute_fingerprint + hotelling_t2_test()` 验证修订
6. `IEvaluator::evaluate()` 通过 CompositeEvaluator 加权评估
7. 若新 reward > 旧 reward + threshold → `MutationGovernor::commit()` + emit `gepa.commit.committed`
8. 否则 → emit `gepa.commit.denied` + 迭代

#### Scenario: 完整循环成功路径

- **WHEN** 运行 `test_gepa_phase2::reflection_loop_basic_flow`
- **THEN** 失败 trace → 修订 → 回归 Pass → commit success
- **AND** emit 6 个事件（started/proposed/committed + reflection.completed）

#### Scenario: 修订无改进终止循环

- **WHEN** 运行 `test_gepa_phase2::reflection_loop_no_improvement`
- **THEN** 3 次迭代仍无改进 → return success=false
- **AND** emit `gepa.reflection.failed`

### Requirement: 6 个 GEPA 事件主题注册到 ADR-0068 附录 A

The ADR-0068 附录 A MUST register 6 new topics:
- `gepa.reflection.started`
- `gepa.reflection.completed`
- `gepa.reflection.failed`
- `gepa.commit.proposed`
- `gepa.commit.committed`
- `gepa.commit.denied`

Each topic MUST include owner module, mandatory emission point, and payload schema.

#### Scenario: ADR-0068 附录 A v1.3

- **WHEN** 静态检查 `grep "gepa.reflection\|gepa.commit" docs/adr/adr-0068-event-emission-contract.md`
- **THEN** 6 个主题必须全部出现
- **AND** 每个主题必须有 owner + payload schema 字段

### Requirement: L1/L2 门禁通过 MutationGovernor

The `reflect_and_commit()` MUST go through `MutationGovernor::propose()` gate (V1 L1_prompt only). L2/L3/L4 mutations MUST be rejected by MutationGovernor V1 (per G11 contract).

#### Scenario: L1 commit 路径

- **WHEN** 运行 `test_gepa_phase2::gepa_e2e_with_real_mutation_governor`
- **THEN** L1 prompt 修订 → MutationGovernor::propose() Pass → MutationGovernor::commit() success
- **AND** mutation.committed event emitted

#### Scenario: L4 weight 拒绝

- **WHEN** MutationGovernor::propose() 收到 mutation_kind="L4_weight"
- **THEN** MutationGovernor V1 emit `mutation.denied` + throw std::runtime_error（per G11 emit-then-throw）

### Requirement: 修订后回归门禁

The `reflect_and_commit()` MUST verify that the revision does not introduce regression using `BehavioralRegressionGate::hotelling_t2_test()`. If Verdict::Fail, MUST abort commit (emit `gepa.commit.denied`).

#### Scenario: 修订导致回归终止

- **WHEN** 运行 `test_gepa_phase2::gepa_e2e_regression_decline_aborts`
- **THEN** 修订后 fingerprint 显著差异 → Verdict::Fail → abort commit
- **AND** emit `gepa.commit.denied` with denial_reason="regression_decline"

### Requirement: 评估信号使用 IEvaluator V2 CompositeEvaluator

The `reflect_and_commit()` MUST use `IEvaluator` (injected) for revision quality assessment. If V2 `CompositeEvaluator` is injected (wrapping V1 + BehavioralEquivalence), MUST use weighted aggregation.

#### Scenario: V2 CompositeEvaluator 集成

- **WHEN** 运行 `test_gepa_phase2::gepa_e2e_with_real_evaluator_v2`
- **THEN** CompositeEvaluator(V1 + BehavioralEquivalence) 加权评估修订
- **AND** scalar 加权平均 + quality 众数

### Requirement: Mock ILLMProvider 避免外部依赖

The E2E tests MUST use `MockILLMProvider` returning fixed prompt revision candidates. MUST NOT trigger real LLM API calls.

#### Scenario: Mock 修订候选

- **WHEN** `MockILLMProvider::generate()` 被调用
- **THEN** 返回 "Reflection note: Add error handling for {failure_mode}"
- **AND** 无外部 LLM API 调用

### Requirement: ctest 全量零回归

The `ctest --output-on-failure` MUST report ALL tests PASS with zero regressions relative to IEvaluator V2 ship baseline (188/189, 1 pre-existing timing flake).

#### Scenario: ctest 全量 PASS

- **WHEN** 运行 `ctest --output-on-failure`
- **THEN** 所有测试 PASS, 0 failures（pre-existing timing flake 不计入）
- **AND** 测试计数 ≥ IEvaluator V2 baseline + 1 (test_gepa_phase2 target)

#### Scenario: test_gepa_phase2 专项

- **WHEN** 运行 `ctest --output-on-failure -R test_gepa_phase2`
- **THEN** ≥ 8 cases PASS, ≥ 20 assertions

### Requirement: adr_lint + docs_drift_audit 全通过

The `python3 tools/adr_lint.py` MUST exit 0, and `python3 tools/docs_drift_audit.py` MUST NOT introduce new CRITICAL drift.

#### Scenario: adr_lint 全过

- **WHEN** 运行 `python3 tools/adr_lint.py`
- **THEN** 退出码 0, 无 ADR lint 错误

#### Scenario: docs_drift_audit 零新增

- **WHEN** 运行 `python3 tools/docs_drift_audit.py`
- **THEN** GEPA 相关 drift 为 0 项新增

### Requirement: cap-map §三 B7 ✅ Completed + §一 +1 新能力

The capability-application-map MUST update §三 B7 row from "✅ Closed (V1 code ship, 2026-08-26)" to "✅ Completed (V1 ship, 2026-08-27, GEPA MVP)" + §一 +1 (新能力 #27 GEPA MVP) + 头部 v2.0 → v2.1.

#### Scenario: §三 B7 行更新

- **WHEN** 静态检查 `grep "B7.*GEPA" docs/architecture/capability-application-map-2026-08.md`
- **THEN** B7 行必须显示 "✅ Completed" 标识

#### Scenario: §一 +1 新能力

- **WHEN** 静态检查 `grep "GEPA MVP" docs/architecture/capability-application-map-2026-08.md | head -3`
- **THEN** §一表格必须新增 GEPA MVP V1 能力行

### Requirement: T19 Phase 1 只读反思约束解除

The active-status.md §一 T19 跟踪段 MUST NOT contain "Phase 1 只读反思约束（不执行 commit(PromptEdit)）" declaration after T19 Phase 2 ship.

#### Scenario: T19 跟踪段同步

- **WHEN** 静态检查 `grep "Phase 1 只读" docs/active-status.md`
- **THEN** 不应出现 "Phase 1 只读反思约束" 声明
- **AND** 应出现 "T19 Phase 2 commit 已 ship" 声明

### Requirement: Phase 2 实际执行 commit(PromptEdit)

The T19 Phase 2 MUST actually execute `MutationGovernor::commit()` to apply prompt revisions (not just simulate). This is the key difference from Phase 1 read-only constraint.

#### Scenario: Phase 2 vs Phase 1 区分

- **WHEN** T19 reflect_and_commit() 成功路径
- **THEN** `MutationGovernor::commit()` 必须被调用（Phase 1 不调用）
- **AND** emit `gepa.commit.committed` with evaluation_refs（非空）
- **AND** mutation.committed event 必须由 MutationGovernor emit

### Requirement: V1 边界遵守 (L4 禁止 + 同步循环)

The T19 V1 MUST NOT support:
- L4 weight mutations (G11 V1 explicit ban)
- Asynchronous commit paths
- Multi-agent collaborative reflection
- Cross-session experience accumulation
- Online weight fine-tuning
- Pareto multi-objective evaluation (depends on IEvaluator V3+)
- TrajectoryFidelity evaluation (depends on T15 V2 schema)

#### Scenario: L4 weight 显式拒绝

- **WHEN** GEPALoop 尝试 mutation_kind="L4_weight"
- **THEN** MutationGovernor V1 emit-then-throw 拒绝
- **AND** GEPALoop 捕获异常后 emit `gepa.commit.denied`

#### Scenario: 同步循环（非异步）

- **WHEN** `reflect_and_commit()` 被调用
- **THEN** 同步阻塞至循环完成（V1 简化）
- **AND** 异步路径 deferred to V2