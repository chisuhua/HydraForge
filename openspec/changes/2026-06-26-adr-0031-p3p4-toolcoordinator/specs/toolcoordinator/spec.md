# toolcoordinator Specification

> **Purpose**: 追踪 ADR-0031 P3-P4 (ToolCoordinator + Layer Profile) 实施 (Sprint 14 主体)
> **STATUS: PLACEHOLDER** ⚠️ — 详细 Requirements 待 C3 完成后填充

## ADDED Requirements

### Requirement: toolcoordinator-middleware (PLACEHOLDER)

`ToolCoordinator` MUST 包装所有 tool 调用 + 集成 IExecutionPolicy + ApprovalCoordinator

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: toolcoordinator-executor-integration (PLACEHOLDER)

`NodeExecutor` MUST 改用 `ToolCoordinator::call_tool_with_policy()` 替换直接 `call_tool`

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: toolcoordinator-audit-log (PLACEHOLDER)

工具调用 MUST 记录审计日志到 EventBus (tool.audit event)

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: layer-profile-three-tiers (PLACEHOLDER)

`Layer` MUST 含 Cognitive/Thinking/Workflow 三层

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: tool-metadata-v2-extensions (PLACEHOLDER)

`ToolMetadata` V2 MUST 扩展含 `allowed_layers` / `cost_estimate` / `timeout_ms` 字段

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD
