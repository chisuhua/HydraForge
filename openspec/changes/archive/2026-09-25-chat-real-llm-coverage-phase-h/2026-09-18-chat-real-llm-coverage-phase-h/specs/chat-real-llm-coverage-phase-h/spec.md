# Spec: chat-real-llm-coverage-phase-h

> **STATUS: PLACEHOLDER** — 6 placeholder Requirements with TBD Scenarios

## ADDED Requirements

### Requirement: R1 — React Loop "Hello" 端到端
When invoked with prompt "Hello" against a real LLM provider (per `tests/test_helpers/real_llm_env.h`), the react agent loop MUST produce a non-empty Assistant response and execute at least 1 think → decide → end step. The F1 fail-fast MUST NOT trigger (real LLM should return non-empty).

#### Scenario: real LLM "Hello" → non-empty response
- **WHEN** react loop receives prompt "Hello" with real LLM provider
- **THEN** Assistant response SHALL be a non-empty string
- **AND** react loop SHALL execute >= 1 step
- **AND** no fail-fast exception SHALL be thrown

### Requirement: R2 — React Loop 中文 Prompt
When invoked with Chinese prompt "用一句话解释 std::jthread" against a real LLM provider, the react agent loop SHALL produce a response containing the keyword "jthread" (case-insensitive substring match). CJK encoding MUST be preserved (no mojibake).

#### Scenario: real LLM Chinese prompt → jthread keyword
- **WHEN** react loop receives Chinese prompt "用一句话解释 std::jthread"
- **THEN** Assistant response SHALL contain substring "jthread" (case-insensitive)
- **AND** CJK characters in prompt/response SHALL be correctly encoded (UTF-8 round-trip)

### Requirement: R3 — React Loop 多轮对话
When invoked across two turns with context propagation, the react agent loop SHALL demonstrate turn 2 context awareness (turn 2 response MUST reference information from turn 1).

#### Scenario: turn 1 "My name is Alice" → turn 2 "What is my name?"
- **WHEN** turn 1 prompt is "My name is Alice" and turn 2 prompt is "What is my name?"
- **THEN** turn 2 Assistant response SHALL contain "Alice" (case-insensitive substring)
- **AND** the response SHALL demonstrate context awareness from turn 1

### Requirement: R4 — PlanExecute 端到端
When invoked with prompt "研究量子计算" against a real LLM provider, the plan_execute loop MUST execute all 3 phases (plan → execute → verify) with verify phase succeeding.

#### Scenario: plan_execute 三阶段 verify success
- **WHEN** plan_execute receives prompt "研究量子计算"
- **THEN** plan phase SHALL produce a plan
- **AND** execute phase SHALL execute plan steps
- **AND** verify phase SHALL return success
- **AND** `LoopResult.success` SHALL equal true

### Requirement: R5 — ForkJoin 3 分支并行
When invoked with 3 parallel sub-tasks, the fork_join loop MUST execute all 3 branches concurrently and the synthesize node SHALL aggregate all 3 branch results into the final response.

#### Scenario: fork_join 3 分支 synthesize
- **WHEN** fork_join loop has 3 parallel sub-tasks (e.g., "compute A", "compute B", "compute C")
- **THEN** all 3 branches SHALL execute
- **AND** synthesize node SHALL aggregate all 3 results
- **AND** final response SHALL contain identifiers from all 3 branches

### Requirement: R6 — Stress Test (Optional, TBD)
When invoked with 100 sequential real LLM calls, the system SHALL achieve >= 95% success rate with median latency < 5s. This requirement MAY be deferred to Phase I if stress test infrastructure is deemed out of scope.

#### Scenario: 100 calls 顺序执行成功率
- **WHEN** 100 sequential real LLM calls are invoked
- **THEN** success rate SHALL be >= 95% (容许 LLM 偶发失败)
- **AND** median latency SHALL be < 5s (DEEPSEEK 平均响应时间)

#### Scenario: deferral decision
- **IF** R6 implementation cost exceeds 1.5h
- **THEN** R6 MAY be deferred to `chat-real-llm-coverage-phase-i` follow-up

### Requirement: Test Infrastructure MUST reuse existing helpers
All new test cases SHALL reuse existing helpers in `tests/test_helpers/real_llm_env.h` (`agenticdsl::test::real_llm_env_skipped()` + `real_llm_provider()` + `require_real_llm_env()`). A new `prompt_builder.h` helper MAY be introduced IF case complexity warrants it.

#### Scenario: 复用 real_llm_env helpers
- **WHEN** new test cases are written
- **THEN** they SHALL use `require_real_llm_env()` as first check
- **AND** SHALL use `real_llm_env_skipped()` to skip in sandbox without API key
- **AND** SHALL use `real_llm_provider()` for LLM provider construction

#### Scenario: prompt_builder helper optional
- **IF** cases need complex prompt + keyword matching (e.g., R2 jthread keyword)
- **THEN** `tests/test_helpers/prompt_builder.h` MAY be introduced
- **AND** SHALL be registered in CMakeLists.txt if created

### Requirement: Docs drift gate MUST remain clean
All drift detection tools MUST continue to report 0 DRIFT items after Phase H is applied.

#### Scenario: docs_drift_audit returns 0 DRIFT
- **WHEN** `tools/docs_drift_audit.py` is executed
- **THEN** it MUST exit 0 with 0 DRIFT items

#### Scenario: openspec validate --strict returns valid
- **WHEN** `openspec validate --strict` is executed
- **THEN** it MUST report "Change is valid" for `2026-09-18-chat-real-llm-coverage-phase-h`