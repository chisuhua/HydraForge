# Spec: harness-rsi-pilot

> **STATUS: PLACEHOLDER** — 6 placeholder Requirements with TBD Scenarios

## ADDED Requirements

### Requirement: IHarnessRSI 首个真实实现
`IHarnessRSI` MUST be implemented to accept `GenomeMutations{prompt_delta, tools_add/remove, workflow_patch}` and produce a new `Genome`. The implementation MUST call `ChatConfig::override_*` methods, `ToolRegistry::register/unregister` API, and `IGenomeRegistry::commit` directly.

#### Scenario: prompt_delta 应用
- **WHEN** `IHarnessRSI.apply({prompt_delta: "You are a ..."})` is called
- **THEN** it MUST update `ChatConfig.system_prompt` and return `{success: true, new_genome_version: "1.0.1"}`

#### Scenario: tool add 应用
- **WHEN** `IHarnessRSI.apply({tools_add: ["new_tool"]})` is called
- **THEN** it MUST call `ToolRegistry::register_tool("new_tool", ...)` and return success

#### Scenario: tool remove 应用
- **WHEN** `IHarnessRSI.apply({tools_remove: ["old_tool"]})` is called
- **THEN** it MUST call `ToolRegistry::unregister_tool("old_tool")` and return success

---

### Requirement: 双重门禁集成
`IHarnessRSI::apply` MUST first call `TransitionGuard::evaluate_readiness()` (4-condition check) and then `MutationGovernance::authorize()` (hard veto). If either check fails, the mutation MUST NOT be applied and MUST return `Result::failure`.

#### Scenario: evaluate_readiness 失败
- **WHEN** `evaluate_readiness` returns `{verdict: NotReady, reason: "regression gate FAILED"}`
- **THEN** `IHarnessRSI::apply` MUST return `Result::failure(EvolutionError::NotReady)`
- **AND** MUST NOT modify ChatConfig or ToolRegistry

#### Scenario: MutationGovernance Deny
- **WHEN** `MutationGovernance.authorize()` returns `Deny` (e.g., dangerous tool add)
- **THEN** `IHarnessRSI::apply` MUST return `Result::failure(EvolutionError::GovernanceDenied)`

---

### Requirement: Mock 闭环端到端
The pilot MUST verify a mock end-to-end loop: Genome mutation → dual gate → ChatSession reload → 1 turn response.

#### Scenario: prompt_delta 端到端
- **WHEN** a prompt_delta mutation passes dual gate and is applied
- **THEN** the new ChatSession MUST be reloaded with the new system_prompt
- **AND** a mock user input MUST produce a response that reflects the new prompt

#### Scenario: tool add 端到端
- **WHEN** a tool_add mutation passes dual gate
- **THEN** the new tool MUST be callable in the next ChatSession turn

#### Scenario: tool remove 端到端
- **WHEN** a tool_remove mutation passes dual gate
- **THEN** the removed tool MUST NOT be callable in the next ChatSession turn

---

### Requirement: 真实 LLM 1 turn 验证
The pilot MUST verify Harness-RSI works with a real LLM provider (deepseek configured per `config.json`).

#### Scenario: real LLM prompt_delta 验证
- **WHEN** a prompt_delta mutation is applied and a real LLM call is made
- **THEN** the LLM response MUST reflect the new system_prompt
- **AND** the response MUST differ from the pre-delta response

#### Scenario: 真实 LLM 测试本地执行
- **WHEN** running this test
- **THEN** it MUST be marked as CI-skip (real LLM requires API key)
- **AND** MUST be runnable locally with `DEEPSEEK_API_KEY` exported

---

### Requirement: ApprovalPolicy 拦截测试
The pilot MUST verify that `MutationGovernance` correctly vetoes dangerous mutations.

#### Scenario: low-trust 工具 add 被拦截
- **WHEN** `IHarnessRSI.apply({tools_add: ["low_trust_tool"]})` is called
- **THEN** `MutationGovernance.authorize()` MUST return `Deny`
- **AND** the tool MUST NOT be registered

#### Scenario: dangerous 工具 add 被拦截
- **WHEN** `IHarnessRSI.apply({tools_add: ["dangerous_tool"]})` is called
- **THEN** `MutationGovernance.authorize()` MUST return `Deny`
- **AND** the tool MUST NOT be registered

---

### Requirement: Go/No-Go 决策记录
The pilot outcome MUST be recorded as a Decision Record with Go/No-Go classification and rationale.

#### Scenario: Go 决策
- **WHEN** all 6 pilot test cases pass AND ctest 零回归
- **THEN** Decision Record MUST be created with `decision: Go, rationale: "Harness-RSI 价值验证, 立项 Wave 3 (ADR-0078 Model-RSI pilot)"`
- **AND** new OpenSpec change `adr-0078-model-rsi-pilot` MUST be initiated

#### Scenario: No-Go 决策
- **WHEN** any pilot test case fails OR ctest has regressions
- **THEN** Decision Record MUST be created with `decision: No-Go, rationale: "<具体失败原因>"`
- **AND** Wave 2 skeleton MUST be archived
- **AND** master plan §十 Drift Log MUST record the No-Go decision

---

## REMOVED Requirements

(none)

---

## MODIFIED Requirements

(none)
