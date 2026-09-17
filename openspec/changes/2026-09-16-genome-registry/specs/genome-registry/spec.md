# Spec: genome-registry

> **STATUS: PLACEHOLDER** — 5 placeholder Requirements with TBD Scenarios

## ADDED Requirements

### Requirement: Genome CRD Schema
A `Genome` MUST be defined as a versioned configuration object following `genome-v1` CRD schema. The schema MUST include metadata (name, version, parent, created_by, capture_mode) and spec (harness, tools, budget, model_routing, prompt_cache_prefix).

#### Scenario: 完整 Genome 序列化往返
- **WHEN** a `Genome` object is constructed and committed to Registry
- **THEN** it MUST be serializable to YAML conforming to `genome-v1` schema
- **AND** MUST round-trip via load(name@version) to reconstruct the same object

#### Scenario: schema validation 失败
- **WHEN** an invalid YAML (missing required fields) is loaded
- **THEN** Registry MUST return `Result::failure(GenomeError::SchemaViolation)`

---

### Requirement: IGenomeRegistry 接口
`IGenomeRegistry` MUST provide 5 methods: `load`, `commit`, `fork`, `list_versions`, `diff`. All methods MUST return `Result<T, GenomeError>` for error propagation.

#### Scenario: load 成功路径
- **WHEN** `load(name@version)` is called with an existing version
- **THEN** it MUST return `Result::success(Genome)` with the deserialized object

#### Scenario: load 不存在版本
- **WHEN** `load(name@version)` is called with non-existing version
- **THEN** it MUST return `Result::failure(GenomeError::NotFound)`

---

### Requirement: 文件系统后端
The default `IGenomeRegistry` implementation MUST use a filesystem backend with layout `~/.hydraforge/genomes/<name>/<version>/genome.yaml`. Each commit MUST create a new version directory with full Genome state.

#### Scenario: commit 写盘
- **WHEN** `commit(genome)` is called
- **THEN** it MUST create directory `~/.hydraforge/genomes/<name>/<version>/`
- **AND** MUST write `genome.yaml` and `signature.hmac` files
- **AND** MUST return `Result::success(CommitResult{version})`

---

### Requirement: HMAC 签名完整性校验
Each Genome version MUST have an HMAC signature. Load MUST verify the signature before returning the deserialized object.

#### Scenario: 签名校验成功
- **WHEN** loading a version with valid HMAC signature
- **THEN** load MUST return `Result::success(Genome)`

#### Scenario: 签名校验失败
- **WHEN** loading a version with tampered genome.yaml (HMAC mismatch)
- **THEN** load MUST return `Result::failure(GenomeError::IntegrityViolation)`

---

### Requirement: 错误码对齐 ADR-0023
Genome-related errors MUST use error codes aligned with `ToolResult::ErrorCode` from ADR-0023.

#### Scenario: 错误码映射
- **WHEN** a Genome operation fails
- **THEN** the returned `GenomeError` MUST map to:
  - `NotFound` → `ToolResult::ErrorCode::Unknown` (closest: "NotFound" 不在枚举，诚实映射到 Unknown)
  - `SchemaViolation` → `ToolResult::ErrorCode::InvalidParams` (ADR-0073 D3: 工具入参校验失败)
  - `IntegrityViolation` → `ToolResult::ErrorCode::PermissionDenied` (line 31)
  - `BrokenLineage` → `ToolResult::ErrorCode::InvalidState` ⚠️ **remap pending**: `InvalidState` 不在当前 `ToolResult::ErrorCode` 枚举（详见 sibling change fix-loop-run-return-contract 评审）。临时映射到 `Unknown` (line 28 兜底)；后续 ADR-0023 扩枚举或 GenomeError 自定义 string 值待决议。

---

## REMOVED Requirements

(none)

---

## MODIFIED Requirements

(none)
