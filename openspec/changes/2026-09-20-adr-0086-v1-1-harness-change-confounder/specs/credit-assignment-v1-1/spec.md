# Spec: credit-assignment-v1-1

> **Target**: ADR-0086 v1.0 首次实施 + v1.1 amendment
> **STATUS**: DRAFT (post Oracle dual-agent review `bg_fed9d7c0`, critical fixes applied)
> **Created**: 2026-09-20
> **Last Updated**: 2026-09-20 (Oracle 🔴-6 修正: 决策 9 算法完整版 + excludes-self fast-path scenarios)

---

## ADDED Requirements

### Requirement: confounder-harness-change-kind
`ConfounderKind::HarnessChange` enum value MUST exist in `agenticdsl::evolution` namespace. `HarnessChangeRecord` struct MUST contain 5 fields (`kind`/`source_id`/`control_status`/`detection_method`/`evidence_refs`) per proposal §1 schema. JSON round-trip MUST preserve all fields.

#### Scenario: HarnessChange enum 值存在
- **WHEN** `agenticdsl::evolution::ConfounderKind::HarnessChange` is referenced in code
- **THEN** the compiler MUST accept it (enum value exists)

#### Scenario: HarnessChange JSON 序列化 round-trip
- **WHEN** a `HarnessChangeRecord` is serialized to JSON and parsed back
- **THEN** all 5 fields MUST equal the original

#### Scenario: HarnessChange source_id 格式
- **WHEN** a `HarnessChangeRecord.source_id` is constructed
- **THEN** it MUST contain both "@" and "→" separators (e.g., "genome_a@3→5") per 不变量 9

#### Scenario: ConfounderKind namespace 统一
- **WHEN** `ConfounderKind` is referenced
- **THEN** the namespace MUST be `agenticdsl::evolution` (NOT `agenticdsl`)

#### Scenario: 类型路径严格遵守 v1.0 不变量 6
- **WHEN** attribution types are created/extended
- **THEN** they MUST live in `include/agenticdsl/types/attribution_record.h` (NOT `contract/`)

---

### Requirement: baseline-min-samples
`agenticdsl::evolution::kMinBaselineSamples = 5` constant MUST exist. `VersionPairDiff::compare()` MUST fail-fast with `AttributionVerdict::Insufficient` when `parent.sample_count < kMinBaselineSamples`. Single-baseline evaluations MUST NOT proceed to `eval_delta` calculation.

#### Scenario: 单次基线 → fail-fast Insufficient
- **WHEN** `VersionPairDiff::compare()` is called with `parent.sample_count = 1`
- **THEN** the result MUST be `{ verdict: Insufficient, reason: containing "sample_count" and "5" }` and `eval_delta` MUST be 0.0

#### Scenario: 5 次基线 → 正常 proceed
- **WHEN** `VersionPairDiff::compare()` is called with `parent.sample_count = 5`
- **THEN** the algorithm MUST proceed to `eval_delta` calculation and NOT return Insufficient due to sample_count

#### Scenario: 4 次基线 → fail-fast Insufficient
- **WHEN** `VersionPairDiff::compare()` is called with `parent.sample_count = 4`
- **THEN** the result MUST be `{ verdict: Insufficient, reason: containing "sample_count" }`

#### Scenario: kMinBaselineSamples 常量可见
- **WHEN** `agenticdsl::evolution::kMinBaselineSamples` is referenced
- **THEN** the value MUST be exactly `5` (compile-time constant)

---

### Requirement: cross-framework-alignment-section (documentation-only)
The amended ADR-0086 v1.1 MUST include a documentation-only 决策 8 section that maps the ADR's decisions to three external RSI frameworks. The section MUST NOT introduce any new contract-level API or behavior.

#### Scenario: 决策 8 章节存在
- **WHEN** ADR-0086 v1.1 is read
- **THEN** the section "决策 8 — 与外部 RSI 框架的对位" MUST exist with all three framework mappings

#### Scenario: 决策 8 不引入新 API
- **WHEN** the amendment is reviewed for contract surface area
- **THEN** 决策 8 MUST NOT introduce any new function/method/type (only narrative text)

---

### Requirement: data-freshness-algorithm (修正 Oracle 🔴-6)
`judge_data_freshness(data, current, registry)` MUST detect HarnessChange confounder via complete lineage walk + harness string comparison. Algorithm MUST handle 4 cases: (1) data == current → Attributed fast-path; (2) data not in lineage → Confounded; (3) data in lineage but Harness changed after → Confounded; (4) data in lineage with no subsequent Harness change → Attributed.

#### Scenario: data == current → Attributed fast-path (修正 excludes-self 假阳性)
- **WHEN** `judge_data_freshness({name="g", version=5}, {name="g", version=5}, registry)` is called
- **THEN** it MUST return `AttributionVerdict::Attributed` WITHOUT calling `registry.walk_ancestors()` (fast-path)

#### Scenario: data 在谱系中无后续 Harness 变更 → Attributed (新增)
- **WHEN** `judge_data_freshness({name="g", version=3}, {name="g", version=5}, registry)` is called AND lineage = `[{v=4, harness="A"}, {v=3, harness="A"}]` (data v3 之后 v4 沿用 harness A)
- **THEN** it MUST return `AttributionVerdict::Attributed`

#### Scenario: data 在谱系中但后续 Harness 变更 → Confounded (修正 Oracle 🔴-6 假阴性)
- **WHEN** `judge_data_freshness({name="g", version=3}, {name="g", version=5}, registry)` is called AND lineage = `[{v=4, harness="B"}, {v=3, harness="A"}]` (v4 改了 harness)
- **THEN** it MUST return `AttributionVerdict::Confounded` with `reason: "harness changed after data generation"` and `confounders[0].kind == ConfounderKind::HarnessChange`

#### Scenario: data 不在谱系中 → Confounded
- **WHEN** `judge_data_freshness({name="g", version=2}, {name="g", version=5}, registry)` is called AND lineage = `[{v=4}, {v=3}]` (v2 不在 lineage)
- **THEN** it MUST return `AttributionVerdict::Confounded`

#### Scenario: walk_ancestors 失败 → Insufficient
- **WHEN** `judge_data_freshness(...)` is called and `registry.walk_ancestors(...)` returns `Result::failure(GenomeError::NotFound | BrokenLineage)`
- **THEN** it MUST return `AttributionVerdict::Insufficient`

#### Scenario: walk_ancestors 签名含 name 参数 (修正 Oracle 🔴-5)
- **WHEN** `judge_data_freshness` calls `registry.walk_ancestors`
- **THEN** the call MUST pass `current.name` as first arg (versions are per-name in C2 model)

#### Scenario: LineageWalk.intermediate_metadata 元素类型为 Genome (修正 Oracle 🔴-6)
- **WHEN** `judge_data_freshness` reads `lineage.intermediate_metadata[i]`
- **THEN** the element MUST be of type `Genome` (含 `spec.harness` 字段)，NOT `GenomeMetadata`

---

## ADDED Requirements (v1.0 首次实施)

### Requirement: v1.0-attribution-record-types
`AttributionRecord` / `AttributionMethod` / `AttributionVerdict` / `ConfounderRecord` / `VersionSnapshot` MUST exist per ADR-0086 v1.0 决策 1. All types MUST live in `agenticdsl::evolution` namespace, header `include/agenticdsl/types/attribution_record.h`.

#### Scenario: 类型定义完整
- **WHEN** the types are compiled
- **THEN** all 5 types MUST exist with fields per ADR-0086 v1.0 决策 1 schema

#### Scenario: namespace 与路径合规
- **WHEN** the types are referenced
- **THEN** namespace MUST be `agenticdsl::evolution` AND header path MUST be `agenticdsl/types/attribution_record.h`

---

### Requirement: v1.0-version-pair-diff-algorithm
`VersionPairDiff::compare(child, parent, confounders)` MUST exist per ADR-0086 v1.0 决策 2 algorithm. `compare()` MUST include sample_count check (集成 kMinBaselineSamples from v1.1).

#### Scenario: compare() 基础判定
- **WHEN** `compare()` is called with valid sample_count >= 5
- **THEN** it MUST return `AttributionRecord` with computed `eval_delta` and `verdict` per ADR-0086 v1.0 决策 2

---

### Requirement: v1.0-test-coverage
`tests/test_credit_assignment.cpp` MUST exist with ≥6 v1.0 test cases (per ADR-0086 v1.0 实施 §阶段 0 要求).

#### Scenario: 6 个 v1.0 测试 PASS
- **WHEN** `ctest -R test_credit_assignment` is run
- **THEN** ≥6 v1.0 test cases + 4 v1.1 test cases (10 total) MUST PASS

---

## MODIFIED Requirements (none)

## REMOVED Requirements (none)

---

## Cross-references

- ADR-0086 v1.0: `docs/adr/adr-0086-credit-assignment-contract.md`
- C2 genome-registry: `openspec/changes/archive/2026-09-19-2026-09-16-genome-registry/`
- C3 transition-guard (consumer): `openspec/changes/2026-09-16-h-d-m-transition-guard/`
- T14 Hotelling T²: `tests/test_behavioral_regression.cpp` (Sprint 25 ship)
- AGENTS.md §模式 8: OpenSpec dual-agent review
- Oracle review session: `bg_fed9d7c0` (2026-09-20, 7 Critical + 6 Major issues)
