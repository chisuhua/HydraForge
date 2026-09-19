# Spec: transition-guard

> **Target**: C3 h-d-m-transition-guard
> **STATUS**: DRAFT (post Oracle dual-agent review `bg_fed9d7c0`, critical fixes applied)
> **Created**: 2026-09-20
> **Last Updated**: 2026-09-20 (Oracle 🔴-7 修正: 完整重写以匹配 proposal，删除 4 条件/Blocked/governance 注入/degraded skip 等占位矛盾)

---

## ADDED Requirements

### Requirement: h-d-m-transition-rule
`TransitionGuard::can_transition(from, to, current, last_harness_change)` MUST return `{can_proceed: false, reason: "H→M forbidden, must run H→D→M"}` when `from == Harness && to == Model`. `constexpr_can_transition(from, to)` MUST return `false` for the same input (compile-time literal type). All other transitions (H→D, D→M, D→H, M→D, M→H, Idle→any, any→Idle, any→self, Done→Idle) MUST be allowed.

#### Scenario: H→M 直跳运行期被拒绝
- **WHEN** `can_transition(Harness, Model, current, last_harness_change)` is called
- **THEN** it MUST return `{can_proceed: false, reason: containing "H→M forbidden, must run H→D→M", failed_conditions: containing "h_to_m_direct_skip"}`

#### Scenario: H→D 允许
- **WHEN** `can_transition(Harness, Data, current, last_harness_change)` is called
- **THEN** it MUST return `{can_proceed: true, recommended_next: Data, reason: "transition allowed"}`

#### Scenario: 编译期 constexpr 验证（修正 Oracle 🔴-3：literal type bool）
- **WHEN** `static_assert(constexpr_can_transition(Harness, Model) == false)` is compiled
- **THEN** the compiler MUST accept the static_assert (constexpr returns bool, literal type)

#### Scenario: 自环 no-op
- **WHEN** `can_transition(Harness, Harness, current, last_harness_change)` is called
- **THEN** it MUST return `{can_proceed: true, reason: "transition allowed"}` (no-op)

#### Scenario: Done→Idle 重启合法
- **WHEN** `can_transition(Done, Harness, current, last_harness_change)` is called
- **THEN** it MUST return `{can_proceed: true, reason: "transition allowed"}` (Done 等价 Idle)

#### Scenario: Done→非 Idle 拒绝
- **WHEN** `can_transition(Done, Model, current, last_harness_change)` is called
- **THEN** it MUST return `{can_proceed: false, reason: containing "Done state is terminal, restart via Idle"}`

---

### Requirement: evaluate-readiness-3-plus-1（修正 Oracle 🔴-7）
`TransitionGuard::evaluate_readiness(attribution, eval_signal, budget)` MUST return `EvolutionVerdict` based on **3 independent conditions** + 1 derived note (condition 4 "no uncontrolled confounder" is NOT an independent gate; it is embedded in condition 1 algorithm per ADR-0086 v1.1 决策 2).

#### Scenario: 全 3 条件满足 → Ready
- **WHEN** attribution.verdict == Attributed AND eval_signal.quality != Poor AND budget.max_evolution_llm_calls allows next call
- **THEN** `evaluate_readiness` MUST return `{can_proceed: true, recommended_next: Model, reason: "all conditions met"}`

#### Scenario: 条件 1 不满足（attribution Not Attributed）→ NotReady
- **WHEN** `attribution.verdict != Attributed` (无论 Confounded / Insufficient / NotAttempted)
- **THEN** `evaluate_readiness` MUST return `{can_proceed: false, reason: attribution.reason, failed_conditions: ["attribution_not_attributed"]}`

#### Scenario: 条件 2 不满足（eval_signal Poor）→ NotReady
- **WHEN** `eval_signal.quality == RewardSignal::Quality::Poor` (修正 Oracle 🔴-4: 实际枚举值 Poor，非 Failed)
- **THEN** `evaluate_readiness` MUST return `{can_proceed: false, reason: "regression gate FAILED", failed_conditions: ["regression_gate_failed"]}`

#### Scenario: 条件 3 不满足（预算不足）→ NotReady
- **WHEN** `budget.max_evolution_llm_calls != -1 AND budget.evolution_llm_calls_used + estimated_llm_calls > budget.max_evolution_llm_calls` (修正 Oracle 🔴-4: 用显式维度而非 budget.remaining)
- **THEN** `evaluate_readiness` MUST return `{can_proceed: false, reason: "budget insufficient", failed_conditions: ["budget_insufficient"]}`

#### Scenario: 条件 4 不独立（修正 Oracle 🔴-7 + 🟠-2：derived note）
- **WHEN** condition 1 passes (attribution == Attributed)
- **THEN** condition 4 "no uncontrolled confounder" MUST be implied by ADR-0086 v1.1 决策 2 algorithm (未控制混杂 → verdict=Confounded)
- **AND** `evaluate_readiness` MUST NOT independently check confounders

#### Scenario: ADR-0086 不可用 → fail-closed（修正 Oracle 🔴-7：degraded-skip 反模式）
- **WHEN** ADR-0086 v1.1 is not yet shipped (still 🔍 Proposed or unavailable)
- **THEN** `evaluate_readiness` MUST return `{can_proceed: false, reason: "attribution_unavailable", failed_conditions: ["attribution_not_attributed"]}`
- **AND** `evaluate_readiness` MUST NOT skip condition 1 check (degraded-skip 模式严禁)

#### Scenario: estimated_llm_calls 锁定为 1（修正 Oracle Open Question 2）
- **WHEN** `evaluate_readiness` is called without explicit `estimated_llm_calls` parameter
- **THEN** it MUST default to `1` (one LLM call per state transition)
- **AND** if future expansion needed (token/duration estimation), a separate ADR MUST be raised

---

### Requirement: existing-contract-integration
`TransitionGuard` MUST integrate with: (a) `IEvaluator::evaluate()` (ADR-0083 V2) for condition 2; (b) `CreditAssignment::attribute()` (ADR-0086 v1.1) for condition 1; (c) `IGenomeRegistry::walk_ancestors()` (C3 contract extension on archived C2 interface) for data freshness; (d) `ExecutionBudget` + `IBudgetController` (Sprint 11 C1) for condition 3. `TransitionGuard` MUST NOT bind `MutationGovernance::authorize()` (per YAGNI + Oracle M2 spirit, governance 与 guard 职责分离).

#### Scenario: 评估层集成
- **WHEN** `evaluate_readiness` reads condition 2 input
- **THEN** it MUST consume `RewardSignal` from `IEvaluator::evaluate()` directly

#### Scenario: 归因层集成
- **WHEN** `evaluate_readiness` reads condition 1 input
- **THEN** it MUST consume `AttributionRecord` from `CreditAssignment::attribute()` directly

#### Scenario: 治理层不绑定
- **WHEN** `TransitionGuard` is invoked
- **THEN** it MUST NOT call `MutationGovernance::authorize()` (V1 不绑定)

#### Scenario: 无 Genome 版本锚点 fail-closed
- **WHEN** `can_transition` is called and no Genome version is available (no registry / registry NotFound)
- **THEN** it MUST return `{can_proceed: false, reason: "no_genome_anchor", failed_conditions: ["no_genome_anchor"]}`

---

### Requirement: enforcement-point-binding
Guard MUST be invoked at: (a) C4 Harness-RSI Pilot entry (强制); (b) future Model-RSI 训练管线入口 (前向约束). Guard MUST NOT be invoked at: (c) MutationGovernor.commit() (per V1, governance 与 guard 职责分离).

#### Scenario: C4 Pilot 入口强制调 guard
- **WHEN** C4 Harness-RSI Pilot submits a Genome mutation
- **THEN** it MUST call `evaluate_readiness()` before applying the mutation
- **AND** mutation MUST NOT be applied if guard returns `can_proceed: false`

#### Scenario: Model-RSI 训练管线前向约束
- **WHEN** ADR-0078 Model-RSI Pilot (Wave 3) is implemented in a future change
- **THEN** the new change MUST cite `evaluate_readiness()` as a precondition
- **AND** training MUST NOT start if guard returns `can_proceed: false`

#### Scenario: MutationGovernor 不绑定
- **WHEN** `MutationGovernance::commit()` is called
- **THEN** it MUST NOT call `evaluate_readiness()` (governance 与 guard 职责分离)

---

### Requirement: walk-ancestors-extension (修正 Oracle 🔴-5 + 🟠-4 + 🔴-6)
C3 change MUST extend `IGenomeRegistry` with a new public method `walk_ancestors(name, from_version, to_version)` returning `Result<LineageWalk, GenomeError>`. `LineageWalk` MUST contain `intermediate_versions: std::vector<uint64_t>` (closest-first, excludes-self) and `intermediate_metadata: std::vector<agenticdsl::genome::Genome>` (NOT GenomeMetadata, since GenomeMetadata lacks `spec.harness`).

#### Scenario: walk_ancestors 签名含 name 参数（修正 Oracle 🔴-5）
- **WHEN** `walk_ancestors(name, from_version, to_version = nullopt)` is called
- **THEN** the signature MUST accept `(const std::string& name, uint64_t from_version, std::optional<uint64_t> to_version)` (versions are per-name in C2 model)

#### Scenario: LineageWalk.intermediate_metadata 类型为 Genome（修正 Oracle 🔴-6）
- **WHEN** `walk_ancestors` returns `Result<LineageWalk, GenomeError>::success(...)`
- **THEN** `LineageWalk::intermediate_metadata[i]` MUST be of type `agenticdsl::genome::Genome` (含 `spec.harness` 字段)
- **AND** it MUST NOT be `GenomeMetadata` (lacks `spec.harness`)

#### Scenario: walk closest-first excludes-self
- **WHEN** `walk_ancestors("g", 5)` is called
- **THEN** returned `intermediate_versions` MUST be ordered closest-first (e.g., [4, 3, 2, ...]) and MUST NOT contain 5 (excludes-self)

#### Scenario: walk 失败映射
- **WHEN** `walk_ancestors` encounters NotFound / BrokenLineage / IntegrityViolation
- **THEN** it MUST return `Result::failure(corresponding GenomeError)` and `evaluate_readiness` condition 1 MUST treat as `Insufficient` (fail-closed direction)

#### Scenario: walk 10000 depth cap
- **WHEN** walk depth exceeds 10000
- **THEN** it MUST terminate with `Result::failure(GenomeError::BrokenLineage)` (per C2 ship M1 fix)

#### Scenario: BREAKING-lite 治理声明（修正 Oracle 🟠-4）
- **WHEN** C3 change is shipped
- **THEN** the change MUST include a MODIFIED Requirement in `openspec/changes/archive/.../specs/genome-registry/spec.md` ("5 public methods" → "6 public methods") declaring walk_ancestors extension

---

### Requirement: data-freshness-integration
Condition 1 of `evaluate_readiness` MUST be satisfied when `attribution.verdict == AttributionVerdict::Attributed`. The attribution layer (ADR-0086 v1.1 决策 9) internally calls `judge_data_freshness(data, current, registry)` which uses `walk_ancestors` + `GenomeSpec.harness` string comparison to detect HarnessChange confounders. C3 MUST NOT duplicate this check.

#### Scenario: 条件 1 信任 ADR-0086 边界（修正 Oracle 🟠-2）
- **WHEN** `attribution.verdict == Attributed`
- **THEN** `evaluate_readiness` MUST trust the attribution verdict without re-checking `parent.sample_count` or confounders
- **AND** `evaluate_readiness` MUST NOT call `walk_ancestors` directly (data freshness is ADR-0086's responsibility)

---

### Requirement: evolution-transition-denied-event（修正 Oracle 🟠-5 幻影主题）
When `can_transition()` returns `can_proceed: false`, `TransitionGuard` MUST emit `evolution.transition.denied` event with payload `{from, to, reason, current_genome, last_harness_change}`. The event topic MUST be registered in ADR-0068 v1.2.1 Appendix A (修正 Oracle 提到的 `evolution.scheduler.denied` 幻影主题).

#### Scenario: 事件主题注册
- **WHEN** C3 change is shipped
- **THEN** `openspec/changes/.../specs/event-emission-contract/spec.md` MUST include a MODIFIED Requirement declaring `evolution.transition.denied` topic registration in Appendix A

#### Scenario: 事件 payload 完整性
- **WHEN** `evolution.transition.denied` is emitted
- **THEN** payload MUST contain `from`, `to`, `reason`, `current_genome` (name+version), `last_harness_change` (name+version)

---

### Requirement: evolution-readiness-denied-event（修正 Oracle 🟠-5 幻影主题）
When `evaluate_readiness()` returns `can_proceed: false`, `TransitionGuard` MUST emit `evolution.readiness.denied` event with payload `{failed_conditions, attribution_verdict, eval_quality, budget_state}`. The event topic MUST be registered in ADR-0068 v1.2.1 Appendix A.

#### Scenario: 事件主题注册
- **WHEN** C3 change is shipped
- **THEN** `openspec/changes/.../specs/event-emission-contract/spec.md` MUST include a MODIFIED Requirement declaring `evolution.readiness.denied` topic registration in Appendix A

#### Scenario: 事件 payload 完整性
- **WHEN** `evolution.readiness.denied` is emitted
- **THEN** payload MUST contain `failed_conditions`, `attribution_verdict`, `eval_quality`, `budget_state`

---

## MODIFIED Requirements

### Requirement: evolution-state-machine（5 状态）
`enum class EvolutionState` MUST contain 5 values: `Idle`, `Harness`, `Data`, `Model`, `Done`. `Done` MUST be equivalent to `Idle` (restart legal). Transitions: Idle↔Harness↔Data↔Model all legal; H→M forbidden; Done→non-Idle forbidden.

#### Scenario: 5 状态枚举完整
- **WHEN** `EvolutionState` is referenced
- **THEN** it MUST contain exactly 5 values: `Idle`, `Harness`, `Data`, `Model`, `Done`

---

### Requirement: ig-genome-registry-contract-extension（修正 Oracle 🟠-4）
The `IGenomeRegistry` public interface MUST be extended from 5 to 6 methods, adding `walk_ancestors(name, from_version, to_version)` returning `Result<LineageWalk, GenomeError>`. The `FilesystemGenomeRegistry` (only existing implementation) MUST be updated accordingly. Test mocks (if any) MUST also implement the new method.

#### Scenario: 唯一实现同步改
- **WHEN** C3 change ships `walk_ancestors`
- **THEN** `src/core/genome/registry_filesystem.cpp` MUST implement the new method

#### Scenario: 测试 mock 同步改
- **WHEN** C3 change ships `walk_ancestors`
- **THEN** any test mock IGenomeRegistry implementer MUST add the new method (or ctest will fail to compile)

---

### Requirement: adr-0068-event-emission-contract-appendix-a（修正 Oracle 🟠-5）
ADR-0068 v1.2.1 Appendix A MUST register 2 new topics: `evolution.transition.denied` (H→M 禁则违规事件) and `evolution.readiness.denied` (3+1 条件矩阵失败事件). C3 change MUST include a MODIFIED Requirement in event-emission-contract spec delta.

#### Scenario: 2 个新主题注册
- **WHEN** C3 change is shipped
- **THEN** ADR-0068 Appendix A MUST contain entries for `evolution.transition.denied` and `evolution.readiness.denied`

---

## REMOVED Requirements (none)

---

## Cross-references

- Proposal: `openspec/changes/2026-09-16-h-d-m-transition-guard/proposal.md` (filled, Oracle 🔴 已修正)
- ADR-0086 v1.1 amendment: `openspec/changes/2026-09-20-adr-0086-v1-1-harness-change-confounder/` (filled, Oracle 🔴 已修正)
- C2 genome-registry: `openspec/changes/archive/2026-09-19-2026-09-16-genome-registry/` (待 C3 增补 genome-registry spec MODIFIED delta)
- ADR-0083 IEvaluator: `docs/adr/adr-0083-evaluator-reward-contract.md` ✅ V2
- ADR-0068 event-emission: `docs/adr/adr-0068-event-emission-contract.md` ✅ v1.2.1 (待 C3 增补 MODIFIED delta)
- AGENTS.md §模式 8: OpenSpec dual-agent review
- Oracle review sessions: `bg_ba048665` + `bg_fed9d7c0` (2026-09-20, 7+6 Critical/Major issues)
