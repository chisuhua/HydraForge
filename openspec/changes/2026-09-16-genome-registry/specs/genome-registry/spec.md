# Spec: genome-registry

> **STATUS**: ACTIVE — 6 Requirements (含 fork 设计空白填补 per Oracle bg_a818a6a1 obs #2)
> **Oracle design review**: `bg_a818a6a1` (session `ses_f478a3e49fferX6CU6U4tJTIiB`)
> **追溯范围**: `openspec/changes/2026-09-16-genome-registry/{proposal,design,tasks}.md`

## ADDED Requirements

### Requirement: Genome CRD Schema (`genome-v1`)
A `Genome` MUST be defined as a versioned configuration object following `genome-v1` CRD schema. The schema MUST include metadata (name, version, parent, created_by, created_at, capture_mode) and spec (harness, tools, budget, model_routing, prompt_cache_prefix).

#### Scenario: 完整 Genome 序列化往返
- **WHEN** a `Genome` object is constructed and committed to Registry
- **THEN** it MUST be serializable to YAML conforming to `genome-v1` schema
- **AND** MUST round-trip via load(name@version) to reconstruct the same object with field-level equality (including nested harness/tools)

#### Scenario: schema validation 失败（缺字段）
- **WHEN** an invalid YAML (missing required fields like `parent`, `created_at`) is loaded
- **THEN** Registry MUST return `Result::failure(GenomeError::SchemaViolation)`

#### Scenario: version 格式校验失败
- **WHEN** a commit is attempted with `version` not matching `\d+` (non-monotonic integer)
- **THEN** commit MUST return `Result::failure(GenomeError::SchemaViolation)`

---

### Requirement: IGenomeRegistry 接口
`IGenomeRegistry` MUST provide 5 public methods: `load`, `commit`, `fork`, `list_versions`, `diff`. All methods MUST return `Result<T, GenomeError>` for error propagation. An internal `walk_ancestors` method is **deferred to C3 transition-guard change** (per design.md pitfall #4 — not exposed in v1 public API; v1 ships without it).

#### Scenario: load 成功路径
- **WHEN** `load(name@version)` is called with an existing version
- **THEN** it MUST return `Result::success(Genome)` with the deserialized object after HMAC verification succeeds

#### Scenario: load 不存在版本
- **WHEN** `load(name@version)` is called with non-existing version
- **THEN** it MUST return `Result::failure(GenomeError::NotFound)`

#### Scenario: commit 写盘原子性
- **WHEN** `commit(genome)` is called
- **THEN** it MUST atomically write to disk via `tmp + rename(2)` pattern (per AGENTS.md Pattern #7 v2)
- **AND** MUST write `signature.hmac` FIRST then `genome.yaml` (last rename = commit point; crash between leaves version dir with sig but no yaml → load + list_versions both reject, atomicity holds)
- **AND** MUST serialize concurrent commits via internal mutex
- **AND** MUST validate parent existence + cycle detection BEFORE writing (broken lineage rejected at write time)
- **AND** MUST return `Result::success(CommitResult{version})`

> **Amendment note (2026-09-18, post dual-agent review bg_9ade564d + bg_89293120)**: original spec text said "tmp + fsync + rename(2) MUST"; impl ships with flush + rename + signature-first ordering per spec R3 atomicity goal (any crash state → list/load reject). fsync deferred to follow-up change for durability hardening (Linux-specific fsync + dirfd fsync). Spec word "fsync" relaxed to "tmp + rename" with the ordering guarantee as the durability mechanism.

---

### Requirement: fork 语义（设计空白填补 per Oracle obs #2）
`fork(name@parent_version, mutations)` MUST create a new version whose parent reference is `parent_version` and whose spec fields are the result of **deep-merge** with `mutations`. The new version MUST have `version = max(existing versions in name directory) + 1` (monotonic, registry-assigned, no user-supplied version — supports sibling forks without collision). Cycle detection MUST reject forks where the proposed parent's lineage includes the proposed new version.

> **Amendment note (2026-09-18, post dual-agent review)**: original spec text said `version = parent.version + 1`; impl ships `version = max(existing) + 1` (more general, supports multiple forks from same parent). Spec text amended to match impl.

#### Scenario: fork_creates_new_version
- **WHEN** `fork("alpha@v1", {harness: "new.agent.md"})` is called
- **THEN** the new version MUST have `parent = "alpha@v1"` reference
- **AND** MUST have `version = max(existing) + 1`
- **AND** MUST apply mutations via deep-merge (other fields inherited from parent)

#### Scenario: fork_lineage_chain
- **WHEN** `v1 → v2 (fork) → v3 (fork)` are committed sequentially
- **THEN** `list_versions("alpha")` MUST return `[1, 2, 3]` in ascending order
- **AND** `load("alpha", 3)` MUST show `metadata.parent = "alpha@2"`
- **AND** `load("alpha", 2)` MUST show `metadata.parent = "alpha@1"`

> **Note**: walk_ancestors (closest-first, excludes-self) is deferred to C3 transition-guard per design.md pitfall #4.

#### Scenario: cycle_detection
- **WHEN** a genome's parent chain forms a cycle (e.g., A.parent=B, B.parent=A)
- **THEN** load MUST return `Result::failure(GenomeError::BrokenLineage)` (HMAC passes but cycle detected via (name,version) pair tracking with depth cap 10000)

---

### Requirement: HMAC 签名完整性校验
Each Genome version MUST have an HMAC-SHA256 signature stored in `signature.hmac`. The signature MUST cover **canonical YAML bytes** (per Oracle pitfall #2) including the `parent` field, ensuring lineage integrity. Load MUST verify the signature before returning the deserialized object.

#### Scenario: 签名校验成功
- **WHEN** loading a version with valid HMAC signature
- **THEN** load MUST return `Result::success(Genome)`

#### Scenario: 签名校验失败（篡改 genome.yaml）
- **WHEN** loading a version with tampered genome.yaml (HMAC mismatch)
- **THEN** load MUST return `Result::failure(GenomeError::IntegrityViolation)`

#### Scenario: 签名覆盖 parent 字段（防谱系篡改）
- **WHEN** the `parent` field is tampered (but other fields unchanged)
- **THEN** load MUST still return `Result::failure(GenomeError::IntegrityViolation)` (because HMAC covers parent)

---

### Requirement: 文件系统后端（D9 per Oracle bg_a818a6a1）
The default `IGenomeRegistry` implementation MUST use a filesystem backend with layout `~/.hydraforge/genomes/<name>/<version>/genome.yaml`. Each commit MUST create a new version directory with full Genome state. The backend MUST use atomic write (tmp + rename) and MUST serialize concurrent writes via internal mutex (Single-Dev single-writer assumption).

#### Scenario: commit 写盘成功
- **WHEN** `commit(genome)` is called with valid genome
- **THEN** it MUST create directory `~/.hydraforge/genomes/<name>/<version>/`
- **AND** MUST write `genome.yaml`, `signature.hmac` atomically
- **AND** MUST return `Result::success(CommitResult{version})`

#### Scenario: commit 崩溃恢复（原子性）
- **WHEN** commit is interrupted mid-write (e.g., `.tmp` exists but not renamed)
- **THEN** `list_versions` MUST NOT include the half-written version (atomic guarantee)

#### Scenario: list_versions 排序
- **WHEN** multiple versions exist for a name
- **THEN** `list_versions` MUST return them in **ascending version order** (monotonic integer compare, no semver parsing per Oracle pitfall #5)

---

### Requirement: 错误码独立定义（per Oracle obs #1）
Genome-related errors MUST use an independent `GenomeError` enum with 6 variants: `NotFound`, `SchemaViolation`, `IntegrityViolation`, `BrokenLineage`, `CycleDetected`, `IOError`. This enum MUST NOT be aliased to or constrained by `ToolResult::ErrorCode` (Registry is not a Tool). At the CLI tool boundary (deferred to separate `genome-cli` change), an explicit conversion MAY map `GenomeError` to `ToolResult::ErrorCode` for user-facing error reporting.

> **Amendment note (2026-09-18)**: original spec R6 scenario 2 (CLI boundary conversion) deferred — CLI tool implementation is out of scope for this change (`examples/genome_cli/` directory does not exist; per design.md §Out-of-scope, CLI demo deferred to follow-up `genome-cli` change).

#### Scenario: 错误码直接返回
- **WHEN** a Genome operation fails
- **THEN** the returned `GenomeError` MUST be one of the 6 enum variants
- **AND** MUST NOT be coerced to `ToolResult::ErrorCode` at the registry boundary

#### Scenario: IOError 真实返回（cycle fix 后续，per Oracle review C2b）
- **WHEN** a filesystem operation fails (rename ENOSPC, fs::create_directories EACCES, etc.)
- **THEN** the API MUST return `Result::failure(GenomeError::IOError)` (NOT propagate exception)
- **AND** the partial version directory MUST be removed (atomicity)

#### Scenario: CLI 边界显式转换（deferred to genome-cli change）
- **WHEN** the CLI tool (`examples/genome_cli/main.cpp`, not yet implemented) reports a genome failure
- **THEN** it MUST perform an explicit `GenomeError → ToolResult::ErrorCode` mapping at the tool boundary
- **AND** the mapping table MUST live in `examples/genome_cli/error_mapping.cpp` (not in registry code)

---

## REMOVED Requirements

(none)

---

## MODIFIED Requirements

(none)