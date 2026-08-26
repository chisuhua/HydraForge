# mutation-governance-contract Specification

## ADDED Requirements

### Requirement: IMutationGovernor 是变异治理的抽象接口

The IMutationGovernor MUST provide a pure virtual interface for proposing, committing, and reverting mutations on subject versions, with audit-event emission for each step.

#### Scenario: L1 prompt 变异 happy path (yolo 模式)

- **GIVEN** 一个 subject_version="prompt_v1" + MutationContext{whitelisted_source_id="R_T19_GEPA"}
- **AND** MyExecutor mode = yolo
- **AND** IEvaluator 评估 proposed_change 后返回 RewardSignal.quality=Excellent
- **AND** 行为回归 (ADR-0061-02) Verdict=Pass
- **WHEN** 调用 `governor->propose()` + `governor->commit()`
- **THEN** emit `mutation.proposed` (subject_version + parent_version + proposed_change)
- **AND** emit `mutation.committed` (version_id + evaluation_refs + mutation_kind="L1_prompt")

#### Scenario: L2 DSL 变异需要 plan 模式

- **GIVEN** subject_version="dsl_v1" + MyExecutor mode = yolo
- **WHEN** 调用 `governor->propose()` with mutation_kind="L2_dsl"
- **THEN** emit `mutation.denied` 含 denial_reason="plan_required"
- **AND** 不调用 `commit()`

#### Scenario: L3 SKILL.md 变异需要 agent 模式 + 人类复核

- **GIVEN** subject_version="skill_v1" + MyExecutor mode = plan
- **WHEN** 调用 `governor->propose()` with mutation_kind="L3_skill"
- **THEN** emit `mutation.denied` 含 denial_reason="human_review_required"
- **AND** 不调用 `commit()`

#### Scenario: L4 权重变异在 V1 显式拒绝

- **GIVEN** subject_version="weights_v1" + 任意 MyExecutor mode
- **WHEN** 调用 `governor->propose()` with mutation_kind="L4_weight"
- **THEN** 抛出明确异常 `std::runtime_error("L4 weight mutation forbidden in V1 boundary")`
- **AND** emit `mutation.denied` 含 denial_reason="l4_forbidden_v1"

### Requirement: mutation.* 事件主题完整发射

The IMutationGovernor MUST emit exactly one of the 4 mutation.* topics (proposed/committed/reverted/denied) for each propose/commit/revert attempt.

#### Scenario: audit_events_complete

- **GIVEN** L1 happy path（per Requirement #1 Scenario #1）
- **WHEN** 完整走 propose → commit 流程
- **THEN** EventLog 含 4 mutation.* 主题，按时间顺序：proposed → committed
- **AND** mutation.committed 事件 payload 含 `evaluation_refs` 字段

#### Scenario: revert 流程

- **GIVEN** mutation.committed 已发生 24h 内
- **WHEN** 调用 `governor->revert(target_version="prompt_v2")`
- **THEN** emit `mutation.reverted` 含 rollback_reason + target_version
- **AND** subject_version 恢复到 "prompt_v1" (parent_version)

#### Scenario: fail-closed on unknown source

- **GIVEN** MutationContext.whitelisted_source_id = "" (空字符串)
- **WHEN** 调用 `governor->propose()`
- **THEN** emit `mutation.denied` 含 denial_reason="non_whitelisted_source"
- **AND** 不调用 `commit()`

### Requirement: V1 边界 (per ADR-0084 §决策 1)

The IMutationGovernor MUST implement L1-L3 mutations and MUST explicitly reject L4 mutations in V1 boundary.

#### Scenario: V1 支持 L1/L2/L3 + 禁止 L4

- **GIVEN** ADR-0084 §决策 1 V1 边界
- **THEN** `governor->propose()` 接受 mutation_kind ∈ {"L1_prompt", "L2_dsl", "L3_skill"}
- **AND** `governor->propose()` 拒绝 mutation_kind="L4_weight"（抛出异常 + emit denied）

#### Scenario: V1 边界终止条件

- **GIVEN** ADR-0078 Fine-tune ✅ Approved 之后
- **THEN** V2 OpenSpec follow-up change 可扩展 L4 支持
- **AND** 本 change V1 边界解除（via ADR-0084 amendment）