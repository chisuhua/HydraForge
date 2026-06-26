# toolregistry-security-v2 Specification

> **Purpose**: 追踪 ADR-0004 V2 (ToolRegistry Security Metadata + Approval) 实施 (Sprint 16 主体)
> **STATUS: PLACEHOLDER** ⚠️ — 详细 Requirements 待 C4 完成后填充

## ADDED Requirements

### Requirement: toolregistry-v2-metadata-full (PLACEHOLDER)

`ToolMetadata` V2 MUST 完整集成 (含 V1 字段 + C4 引入字段)

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: toolregistry-v2-register-validation (PLACEHOLDER)

`ToolRegistry` 注册时 MUST validation 元数据冲突 (e.g. dangerous + never approval)

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: pdk-declare-tool-v2-mandatory (PLACEHOLDER)

`DECLARE_TOOL` 宏 MUST 强制要求 category / risk_level / approval_policy 字段

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: pdk-declare-tool-v2-compile-check (PLACEHOLDER)

缺失必要字段 MUST 触发编译错误 (而非运行时错误)

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: approval-workflow-toolcoordinator-link (PLACEHOLDER)

`ToolCoordinator` + `IExecutionPolicy` MUST 联动, 审批后执行

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: layer-profile-permission-matrix (PLACEHOLDER)

Layer × Tool Category 权限矩阵 MUST 强制 enforcement (Cognitive/Thinking/Workflow)

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD
