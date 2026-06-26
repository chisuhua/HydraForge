# execution-policy Specification

> **Purpose**: 追踪 ADR-0031 IExecutionPolicy P1-P2 实施 (Sprint 13 主体)
> **STATUS: PLACEHOLDER** ⚠️ — 详细 Requirements 待 Sprint 12 启动前填充

## ADDED Requirements

### Requirement: execution-policy-interface (PLACEHOLDER)

`IExecutionPolicy` MUST 含 4 虚函数 (requires_approval / should_execute / can_skip / get_layer)

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: execution-policy-three-defaults (PLACEHOLDER)

Plan/Agent/YOLO 3 个默认 Policy MUST 可用

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: execution-policy-tool-metadata-v1 (PLACEHOLDER)

ToolMetadata V1 MUST 含 category / risk_level / approval_policy 字段

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: approval-coordinator-event-bus (PLACEHOLDER)

ApprovalCoordinator MUST 订阅 ApprovalRequired event + emit Granted/Denied

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: approval-tui-apply-bridge (PLACEHOLDER)

TUI `/apply <request_id>` 命令 MUST 桥接到 ApprovalCoordinator

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD
