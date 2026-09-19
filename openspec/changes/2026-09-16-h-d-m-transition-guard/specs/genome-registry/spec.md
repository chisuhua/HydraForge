# Spec Delta: genome-registry — walk_ancestors extension

> **Change**: C3 h-d-m-transition-guard (consumes IGenomeRegistry extension)
> **STATUS**: DRAFT (post Oracle dual-agent review `bg_fed9d7c0`, 🟠-4 修正)
> **Target delta**: `docs/specs/genome-registry.md` (C2 ship spec)
> **Created**: 2026-09-20

---

## MODIFIED Requirements

### Requirement: ig-genome-registry-public-interface-6-methods（5 → 6 methods）

The `IGenomeRegistry` public interface MUST contain 6 pure virtual methods (extended from 5 in C2 ship spec per `archive/2026-09-19-2026-09-16-genome-registry/`).

#### Scenario: 6 个公开方法存在
- **WHEN** `IGenomeRegistry` is compiled
- **THEN** it MUST contain the following 6 methods:
  1. `load(name, version)` (existing from C2)
  2. `commit(genome)` (existing from C2)
  3. `fork(name, parent_version, mutations)` (existing from C2)
  4. `list_versions(name)` (existing from C2)
  5. `diff(name, v1, v2)` (existing from C2)
  6. **`walk_ancestors(name, from_version, to_version)` (NEW in C3)** ← 本 spec delta 引入

#### Scenario: walk_ancestors 签名（修正 Oracle 🔴-5）
- **WHEN** `walk_ancestors` is invoked
- **THEN** the signature MUST be:
  ```cpp
  virtual Result<LineageWalk, GenomeError> walk_ancestors(
      const std::string& name,
      uint64_t from_version,
      std::optional<uint64_t> to_version = std::nullopt) = 0;
  ```
- **AND** `name` parameter MUST be the first parameter (versions are per-name in C2 model)

#### Scenario: LineageWalk schema（修正 Oracle 🔴-6）
- **WHEN** `walk_ancestors` returns `Result<LineageWalk, GenomeError>::success(...)`
- **THEN** `LineageWalk` MUST contain:
  ```cpp
  struct LineageWalk {
      std::vector<uint64_t> intermediate_versions;         // closest-first, excludes-self
      std::vector<agenticdsl::genome::Genome> intermediate_metadata;  // 必须是 Genome (含 spec.harness)，NOT GenomeMetadata
  };
  ```
- **AND** `intermediate_metadata[i]` MUST be of type `Genome` (NOT `GenomeMetadata`), because `GenomeMetadata` lacks the `spec.harness` field required by ADR-0086 v1.1 决策 9 data freshness algorithm

#### Scenario: walk_ancestors 失败映射
- **WHEN** `walk_ancestors` encounters errors
- **THEN** it MUST return `Result::failure(GenomeError)` mapped as:
  - `GenomeError::NotFound` — name 不存在 / from_version 不存在
  - `GenomeError::BrokenLineage` — depth > 10000 cap 或 cycle detected
  - `GenomeError::IntegrityViolation` — HMAC 校验失败（C2 ship ship 已有 hmac_verify）

#### Scenario: walk closest-first + excludes-self
- **WHEN** `walk_ancestors("g", 5, nullopt)` is called on lineage `g@5 ← g@4 ← g@3 ← g@2 ← g@1`
- **THEN** returned `intermediate_versions` MUST be `[4, 3, 2, 1]` (closest-first)
- **AND** it MUST NOT contain `5` (excludes-self per C2 ship spec)

#### Scenario: walk to_version 上界（可选）
- **WHEN** `walk_ancestors("g", 5, 2)` is called
- **THEN** returned `intermediate_versions` MUST be `[4, 3, 2]` (stop at to_version inclusive)

#### Scenario: 唯一实现同步改
- **WHEN** C3 change ships
- **THEN** `src/core/genome/registry_filesystem.cpp` MUST implement `walk_ancestors` (cycle detection + 10000 depth cap + HMAC per-version verify + excludes-self)

#### Scenario: BREAKING-lite 治理合规
- **WHEN** IGenomeRegistry public interface is extended with `walk_ancestors`
- **THEN** the change MUST declare BREAKING-lite (pure virtual addition) in proposal
- **AND** the only existing implementation (FilesystemGenomeRegistry) MUST be updated synchronously
- **AND** test mocks (if any) MUST also implement the new method

---

## Cross-references

- **Consumer**: `openspec/changes/2026-09-16-h-d-m-transition-guard/` (C3 h-d-m-transition-guard)
- **ADR-0086 v1.1 consumer**: `openspec/changes/2026-09-20-adr-0086-v1-1-harness-change-confounder/` 决策 9 (data freshness algorithm)
- **C2 ship spec**: `openspec/changes/archive/2026-09-19-2026-09-16-genome-registry/` (base, walk deferred)
- **C2 header**: `include/agenticdsl/genome/genome.h` (to be modified)
- **Oracle review**: `bg_fed9d7c0` (2026-09-20, 🟠-4 contract extension governance)
