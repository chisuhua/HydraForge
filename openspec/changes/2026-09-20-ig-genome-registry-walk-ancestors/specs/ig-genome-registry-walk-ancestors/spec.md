# Spec: ig-genome-registry-walk-ancestors

> **Target**: Sprint 34+ follow-up to ADR-0088 v1.0 ship (commit 0ffc637 + 7a15744)
> **STATUS**: DRAFT (follow-up registered per Oracle NEEDS_FIX verdict 2026-09-20)
> **Created**: 2026-09-20

---

## ADDED Requirements

### Requirement: ig-genome-registry-walk-ancestors-extension
`IGenomeRegistry` MUST be extended from 5 to 6 public methods, adding `walk_ancestors(name, from_version, to_version=nullopt)` returning `Result<LineageWalk, GenomeError>`. Per ADR-0088 D9, the default implementation MUST return `Result::failure(GenomeError::NotImplemented)` to avoid LSP cascade (AGENTS.md pattern #9 ITimerService precedent).

#### Scenario: walk_ancestors signature per Oracle 🔴-5
- **WHEN** `walk_ancestors(name, from_version, to_version=nullopt)` is called
- **THEN** signature MUST be `Result<LineageWalk, GenomeError> walk_ancestors(const std::string& name, uint64_t from_version, std::optional<uint64_t> to_version = std::nullopt)`
- **AND** versions are per-name in C2 model

#### Scenario: LineageWalk.intermediate_metadata type per Oracle 🔴-6
- **WHEN** `walk_ancestors` returns `Result::success(LineageWalk)`
- **THEN** `LineageWalk::intermediate_metadata[i]` MUST be type `agenticdsl::genome::Genome` (含 spec.harness 字段)
- **AND** it MUST NOT be `GenomeMetadata` (lacks spec.harness)

#### Scenario: walk_ancestors default implementation per Oracle Q6 CRITICAL
- **WHEN** a class derives `IGenomeRegistry` without overriding `walk_ancestors`
- **THEN** the inherited default MUST return `Result::failure(GenomeError::NotImplemented)`
- **AND** compilation MUST succeed (no pure virtual cascade)

#### Scenario: FilesystemGenomeRegistry override per ADR-0088 D5
- **WHEN** `FilesystemGenomeRegistry::walk_ancestors` is called
- **THEN** it MUST perform full lineage walk via filesystem traversal + lazy load + cache
- **AND** return `Result<LineageWalk, GenomeError>` with intermediate_versions + intermediate_metadata

#### Scenario: walk performance per ADR-0088 D5
- **WHEN** walk traverses lineage of ≥100 versions
- **THEN** total elapsed time MUST be < 100ms (per lazy load + cache assumption)

#### Scenario: judge_data_freshness full implementation per ADR-0088 D6
- **WHEN** `VersionPairDiff::judge_data_freshness(data, current, registry)` is called
- **THEN** 4 cases MUST be implemented:
  1. data == current → AttributionVerdict::Attributed fast-path
  2. data not in lineage → AttributionVerdict::Confounded
  3. data in lineage but Harness changed after → Confounded + confounders[0].kind == ConfounderKind::HarnessChange + reason "harness changed after data generation"
  4. data in lineage with no subsequent Harness change → Attributed
- **AND** the stub (returning Insufficient) MUST be removed

---

## Cross-references

- ADR-0088 D5/D6/D9 ship target
- ADR-0086 v1.1 (judge_data_freshness stub ownership)
- openspec/specs/transition-guard/spec.md §walk-ancestors-extension Requirement
- openspec/specs/credit-assignment-v1-1/spec.md §data-freshness-algorithm Requirement