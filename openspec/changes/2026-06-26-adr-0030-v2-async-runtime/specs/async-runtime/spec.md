# async-runtime Specification

> **Purpose**: 追踪 Phase 2 异步架构 (ADR-0030 V2) 实施进度
> **STATUS: PLACEHOLDER** ⚠️ — 详细 Requirements 待 C1 完成后填充

## ADDED Requirements

### Requirement: async-runtime-concurrent-dag-execution (PLACEHOLDER)

`TopoScheduler` MUST 支持节点级并行派发 (TBD: Taskflow executor / std::jthread 决策)

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: async-runtime-fleet-mode-16x (PLACEHOLDER)

Fleet 模式 MUST 支持 16 路 LLM 调用并行 (TBD: 真实业务场景验证)

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: async-runtime-streaming-yield (PLACEHOLDER)

LLM Token 流 MUST 支持协程 yield 或 IGenerationStream 句柄 (TBD: 决策点 1)

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: async-runtime-approval-suspend (PLACEHOLDER)

用户审批 MUST 支持协程 suspend 或 EventBus 阻塞 (TBD: 决策点 1)

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD
