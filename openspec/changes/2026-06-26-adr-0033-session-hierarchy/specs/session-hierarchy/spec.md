# session-hierarchy Specification

> **Purpose**: 追踪 ADR-0033 Session Hierarchy 三层会话模型实施 (Sprint 15 主体)
> **STATUS: PLACEHOLDER** ⚠️ — 详细 Requirements 待 Sprint 14 启动前填充

## ADDED Requirements

### Requirement: session-hierarchy-three-layers (PLACEHOLDER)

`UserSession` / `TaskSession` / `SubtaskSession` MUST 完整实现 (含 weak_ptr 引用关系)

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: session-hierarchy-dslengine-overload (PLACEHOLDER)

`DSLEngine::run(session_id, ...)` MUST 提供, 替代当前 stateless `run(Context)`

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: session-hierarchy-fork-isolation (PLACEHOLDER)

Fork/Join 分支 MUST 自动创建/销毁 `SubtaskSession`, 隔离分支状态

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: session-hierarchy-iper-retry (PLACEHOLDER)

IPER retry MUST 复用同一 `TaskSession`, 失败计数挂在该 session

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: session-hierarchy-messages-append (PLACEHOLDER)

`UserSession.messages` MUST 追加写保护 (集成 ADR-0023)

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD
