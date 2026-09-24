# l2-evolution-deferred — Spec

> **Status**: 🔍 Proposed Spec
> **关联 Proposal**: [`../../proposal.md`](../../proposal.md)
> **关联 Design**: [`../../design.md`](../../design.md)
> **关联 L2 parent**: [`../../archive/2026-09-24-pdk-chat-demo-evolution-reference-example/`](../../archive/2026-09-24-pdk-chat-demo-evolution-reference-example/)

---

## Purpose

本 change 定义 L2 `pdk-chat-demo-evolution-reference-example` 的 7 项 deferred 事项的升级路径和验收标准。分为 3 个实施阶段 + 1 个 research 路径，各阶段可独立立项、独立 ship。

---

## ADDED Requirements

### Requirement: r8-full-metrics-release-metrics

**Phase 1** (Sprint 37+). `--release-metrics` flag MUST upgrade from stub to real IEvaluator V2 invocation:

- MUST execute Phase 2-5 per ContextRequest
- MUST collect attribution_verdict distribution per IEvaluator V2 BehavioralEquivalence
- MUST compute `drop_ratio = fail_count / total_contexts`
- MUST exit non-zero when `drop_ratio > 0.05` (R8.1 red-line)
- MUST output `metrics.json`: `{ "new_up": int, "new_down": int, "old_up": int, "old_down": int, "drop_ratio": float, "context_ids": [string] }`

#### Scenario: r8-metrics-real-drop-ratio-block

- **WHEN** `--release-metrics` is enabled AND IEvaluator V2 返回 ≥5% contexts 为 Confounded/NotAttempted
- **THEN** binary MUST exit non-zero (R8.1 mechanism activated)
- **AND** `metrics.json` MUST contain `drop_ratio > 0.05`

#### Scenario: r8-metrics-zero-regression

- **WHEN** `--release-metrics` is enabled AND ALL contexts 返回 Attributed
- **THEN** binary MUST exit 0
- **AND** `metrics.json` MUST have `drop_ratio == 0.0`

### Requirement: r8-full-metrics-ablation-mode

`--ablation-mode=full` MUST output 3-segment ablation_report.json:

- **Segment 1** MUST report same-task-different-Harness attribution_verdict distribution comparison (new Harness vs legacy)
- **Segment 2** MUST report same-Harness-different-task cross-task_class consistency matrix
- **Segment 3** MUST report failure-sample-retention-rate using r8_failure_fixtures.jsonl vs expected_eval_quality

#### Scenario: ablation-3-segments-valid

- **WHEN** `--ablation-mode=full` is enabled
- **THEN** ablation_report.json MUST contain 3 segments
- **AND** each segment MUST contain `attribution_verdict` distribution array
- **AND** each segment MUST contain `response_edit_distance` metric

### Requirement: r8-failure-event-format-v2

`--failure-event-format=v2` MUST extend failure trace to 5-field chain:

- MUST include `failure_event` (string)
- MUST include `rule_id` (string)
- MUST include `rule_shipped_commit` (string, valid git commit hash)
- MUST include `reproduce_in_new_task_demo` (string, reproduce steps)
- MUST include `context_id` (string, linking to ContextRequest)

#### Scenario: failure-event-v2-5-fields

- **WHEN** failure event occurs AND `--failure-event-format=v2` is set
- **THEN** trace line MUST include all 5 fields
- **AND** `rule_shipped_commit` MUST be a valid git commit hash

### Requirement: wave4-sandbox-network-isolation

**Phase 2** (Wave 4). MUST implement true sandbox network isolation for R9.3:

- `DockerBackendConfig.network_mode` SHALL be added via ADR + config field
- SHALL default to `"bridge"` (backward compatible)
- SHALL enforce no external network when `network_mode=none`
- L2 test_anti_cheat_sandbox_escape SHALL upgrade from keyword-rejection to true sandbox verification

#### Scenario: sandbox-network-none-container-isolated

- **WHEN** `DockerBackendConfig.network_mode = "none"`
- **THEN** 容器内 `curl http://example.com` MUST timeout
- **AND** 容器内 `wget http://example.com` MUST fail

#### Scenario: sandbox-keyword-fallback

- **WHEN** sandbox infra 不可用（非 Docker 环境）
- **THEN** L2 MUST 降级回 keyword-rejection (L2 原有行为)
- **AND** MUST emit `turn_input_network_keyword_rejected` event

### Requirement: workflow-patch-v1

**Phase 3** (Wave 3 Phase 2 D4-D7). `harness_rsi::apply_harness_mutation` MUST remove `UnsupportedVariant` fallback and support workflow_patch:

- `workflow_patch` mutation SHALL allow `HarnessMutation` to rewrite DSL subgraphs
- 5-tier gate SHALL add G4.5 workflow_patch-specific validation
- evolution_session::phase3_mutation SHALL add workflow_patch path

#### Scenario: workflow-patch-applies-successfully

- **WHEN** `apply_harness_mutation(workflow_patch)` is called
- **THEN** MUST NOT return `UnsupportedVariant`
- **AND** `AppliedMutation.workflow` MUST contain the patched DSL graph

### Requirement: baseline-samples-statistical-test

BehavioralEquivalence.compare() SHALL output valid statistical verdict when ≥5 baseline samples available:

- `kMinBaselineSamples` SHALL be 5
- `<5 samples` SHALL return "Insufficient" (honest label)
- `≥5 samples` SHALL return "Attributed" or "Confounded" based on statistical test
- `run_6_phase_demo` SHALL support baseline sample accumulation

#### Scenario: baseline-less-than-5-returns-insufficient

- **WHEN** Phase 5 compare runs with <5 baseline samples
- **THEN** `attribution_verdict` MUST be "Insufficient" or "NotAttempted"

#### Scenario: baseline-5-or-more-returns-verdict

- **WHEN** Phase 5 compare runs with ≥5 baseline samples
- **THEN** `attribution_verdict` MUST NOT be "Insufficient"
- **AND** MUST be "Attributed" or "Confounded" based on statistical test

---

## Cross-Doc References

| SoT 文档 | 更新内容 |
|---------|--------|
| [`docs/architecture/rsi-architecture-2026-09.md`](../../../architecture/rsi-architecture-2026-09.md) §11.8 | R9.3 sandbox upgrade from degraded to full |
| [`docs/architecture/harness-architecture-2026-09.md`](../../../architecture/harness-architecture-2026-09.md) §DockerBackend | network_mode config field |
| [`AGENTS.md` Reverse Indicator Rule](../../../AGENTS.md) | R8.1 full metrics + R8.2 failure tracing |
| L2 archive `design.md` deferred items | D1-D7 source of truth |