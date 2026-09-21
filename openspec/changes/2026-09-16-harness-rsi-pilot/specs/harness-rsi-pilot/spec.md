# Spec: harness-rsi-pilot

> **STATUS**: DRAFT (REVISION 2026-09-21 per Oracle bg_3672cb57 + Metis bg_1f291bc4 dual-agent review)
> **3 项 Critical 修正**: C1 幻影 API 修正 (apply_harness_mutation 修订签名) + C2 core→PDK 依赖消除 (签名不取 ChatConfig&) + C3 IMutationGovernor veto 机制重写 (内部 is_tool_allowed policy check)
> **3 项 Major 修正**: M1 审计配对 (C4 不调 IMutationGovernor::propose, 无审计配对问题) + M2 签名扩展 (MutationGateContext + AppliedMutation) + M3 事件载荷 4/4 字段断言
> **Case 4 删除** (real LLM 推迟 Wave 3, per Metis 2.5)

---

## ADDED Requirements

### Requirement: apply-harness-mutation-lightweight-function
A lightweight function `apply_harness_mutation(GenomeMutations, std::string& system_prompt, std::vector<std::string>& tools, IToolRegistry&, const MutationGateContext&) -> Result<AppliedMutation, MutationError>` MUST be implemented (NOT an `IHarnessRSI` interface class — per ADR-0088 D4 line 96-98 explicit cancellation). The function MUST NOT include `agenticdsl/pdk/chat_session.h` from `src/evolution/` (per Oracle C2 — core→PDK reverse dependency elimination).

#### Scenario: prompt_delta apply (success path)
- **WHEN** `apply_harness_mutation(GenomeMutations{prompt_delta: "You are concise."}, system_prompt, tools, registry, ctx)` is called
- **AND** evaluate_readiness returns can_proceed=true
- **AND** is_tool_allowed returns true for all mutations
- **THEN** `system_prompt.find("You are concise.") != std::string::npos` MUST hold (deterministic string assertion)
- **AND** MUST return `Result::success(AppliedMutation{applied_prompts: ["You are concise."]})`

#### Scenario: tools_add apply (success path)
- **WHEN** `apply_harness_mutation(GenomeMutations{tools_add: ["trusted_tool"]}, system_prompt, tools, registry, ctx)` is called
- **AND** dual gate passes
- **THEN** `registry.has_tool("trusted_tool") == true` MUST hold (工具须预注册于共享 registry; apply 仅激活到 agent 的 tools vector, **不**调 `register_tool_function` — 无 ToolMetadata 来源, 避免伪造 metadata 违反 ADR-0004 V2 校验逻辑)
- **AND** `tools` vector MUST contain "trusted_tool" (verifiable via `std::find(tools.begin(), tools.end(), "trusted_tool") != tools.end()`)
- **AND** MUST return `Result::success(AppliedMutation{applied_tools_added: ["trusted_tool"]})`

#### Scenario: tools_remove apply (success path)
- **WHEN** `apply_harness_mutation(GenomeMutations{tools_remove: ["old_tool"]}, system_prompt, tools, registry, ctx)` is called
- **AND** dual gate passes
- **THEN** `registry.unregister_tool_function("old_tool")` MUST have been called (verifiable via `registry.has_tool("old_tool") == false` after)
- **AND** `tools` vector MUST NOT contain "old_tool"
- **AND** MUST return `Result::success(AppliedMutation{applied_tools_removed: ["old_tool"]})`

#### Scenario: workflow_patch NOT supported
- **WHEN** `apply_harness_mutation(GenomeMutations{workflow_patch: ...}, ...)` is called
- **THEN** MUST return `Result::failure(MutationError::UnsupportedVariant)` — workflow_patch path is out of scope (留后续提案, Wave 3)

---

### Requirement: dual-gate-integration
`apply_harness_mutation` MUST first invoke `evaluate_readiness(ctx.current, *ctx.attribution, *ctx.evaluator, *ctx.budget)` (per ADR-0088 D3, 4-condition check) and then internal `is_tool_allowed(meta, ctx.policy)` (per Oracle C3 — lightweight policy check, NOT `IMutationGovernor::propose`). If either check fails, the mutation MUST NOT be applied (zero state change to system_prompt + tools + registry) and MUST return `Result::failure`.

#### Scenario: evaluate_readiness denies (regression gate fail)
- **WHEN** `evaluate_readiness` returns `{can_proceed: false, failed_conditions: ["regression_gate_failed", "budget_exhausted"]}`
- **THEN** `apply_harness_mutation` MUST return `Result::failure(MutationError::NotReady{failed_conditions: [...]})`
- **AND** `system_prompt` MUST equal initial (deterministic: `system_prompt == initial_system_prompt`)
- **AND** `tools` vector MUST equal initial (deterministic: `tools == initial_tools`)
- **AND** `registry.has_tool("any_tool")` MUST equal initial state (zero tool changes)
- **AND** `evolution.readiness.denied` event MUST be emitted with payload containing **all 4 fields** (per ADR-0068 v2.2 line 253):
  - `failed_conditions` (array)
  - `attribution_verdict` (string)
  - `eval_quality` (string)
  - `budget_state` (string)

#### Scenario: is_tool_allowed denies (dangerous tool)
- **WHEN** `MutationGovernancePolicy{denied_tools: ["dangerous_tool"]}` is configured
- **AND** `apply_harness_mutation(GenomeMutations{tools_add: ["dangerous_tool"]}, ...)` is called
- **AND** evaluate_readiness passes
- **THEN** `apply_harness_mutation` MUST return `Result::failure(MutationError::GovernanceDenied{denial_reason: "dangerous_tool_denied"})`
- **AND** `registry.has_tool("dangerous_tool")` MUST be `false`
- **AND** `tools` vector MUST NOT contain "dangerous_tool"

#### Scenario: is_tool_allowed denies (low-trust tool)
- **WHEN** `MutationGovernancePolicy{denied_tools: ["low_trust_tool"]}` is configured
- **AND** `apply_harness_mutation(GenomeMutations{tools_add: ["low_trust_tool"]}, ...)` is called
- **THEN** MUST return `Result::failure(MutationError::GovernanceDenied{denial_reason: "low_trust_tool_denied"})`
- **AND** ToolRegistry MUST NOT contain `low_trust_tool`

---

### Requirement: mock-closed-loop-verification (4 cases, 含 positive)
The pilot MUST verify 4 mock closed-loop cases via deterministic assertions (NOT LLM response diffing — per Metis bg_139e388b confirmation bias warning). All 4 MUST pass for Go.

#### Scenario: case-1 prompt_delta apply
- **WHEN** `apply_harness_mutation(prompt_delta="Be concise.", system_prompt, tools, registry, ctx)` is called with mock setup
- **AND** evaluate_readiness returns can_proceed=true (mock setup: stub IEvaluator returns `agenticdsl::RewardSignal::Quality::Excellent` + stub IBudgetController.exceeded()=false + AttributionRecord{verdict=AttributedVerdict::Attributed})
- **AND** is_tool_allowed returns true (mock setup: empty policy)
- **THEN** `REQUIRE(system_prompt.find("Be concise.") != std::string::npos)` MUST PASS
- **AND** MUST return success

#### Scenario: case-2 readiness denied + 4/4 event payload assertion (per Oracle M3)
- **WHEN** evaluate_readiness is constructed to fail (per M3 措辞修正):
  - stub IEvaluator returns `Quality::Poor`
  - stub IBudgetController returns `exceeded()=true`
  - AttributionRecord{verdict=NotAttempted}
- **THEN** `apply_harness_mutation` MUST return failure (MutationError::NotReady)
- **AND** `REQUIRE(system_prompt == initial_system_prompt)` MUST PASS (zero state change)
- **AND** captured event MUST have topic `evolution.readiness.denied`
- **AND** captured event payload MUST contain **all 4 fields** (per M3 修正, NOT 仅 failed_conditions):
  - `REQUIRE(payload.contains("failed_conditions"))` AND `REQUIRE(payload["failed_conditions"].is_array())` AND `REQUIRE(payload["failed_conditions"].size() >= 1)`
  - `REQUIRE(payload.contains("attribution_verdict"))` AND `REQUIRE(payload["attribution_verdict"].is_string())`
  - `REQUIRE(payload.contains("eval_quality"))` AND `REQUIRE(payload["eval_quality"].is_string())`
  - `REQUIRE(payload.contains("budget_state"))` AND `REQUIRE(payload["budget_state"].is_string())`

#### Scenario: case-3a dangerous tool veto (per Oracle C3 重写)
- **WHEN** `apply_harness_mutation(tools_add=["dangerous_tool"], system_prompt, tools, registry, ctx)` is called
- **AND** MutationGovernancePolicy{denied_tools: ["dangerous_tool"]} is configured
- **AND** evaluate_readiness passes
- **THEN** MUST return `Result::failure(MutationError::GovernanceDenied)`
- **AND** `REQUIRE(registry.has_tool("dangerous_tool") == false)` MUST PASS
- **AND** `REQUIRE(std::find(tools.begin(), tools.end(), "dangerous_tool") == tools.end())` MUST PASS

#### Scenario: case-3b trusted tool add positive (per Metis 2.4 充分性)
- **WHEN** `apply_harness_mutation(tools_add=["trusted_tool"], system_prompt, tools, registry, ctx)` is called
- **AND** MutationGovernancePolicy{} (empty allow/deny lists) is configured
- **AND** evaluate_readiness passes
- **THEN** MUST return success
- **AND** `REQUIRE(registry.has_tool("trusted_tool") == true)` MUST PASS
- **AND** `REQUIRE(std::find(tools.begin(), tools.end(), "trusted_tool") != tools.end())` MUST PASS
- **AND** MUST return `Result::success(AppliedMutation{applied_tools_added: ["trusted_tool"]})`

---

### Requirement: go-no-go-decision-record-with-oracle-review
After pilot implementation, a Decision Record MUST be written to `docs/audits/2026-XX-XX-harness-rsi-pilot-go-no-go.md`. The record MUST list observed failures/frictions + ctest counts + Oracle independent review conclusion FIRST, then write Go/No-Go conclusion (缓解 confirmation bias per Metis bg_1f291bc4 §2.6).

#### Scenario: Go decision criteria (4+1 must hold)
- **WHEN** ALL of the following hold:
  1. Mock Case 1 (prompt_delta apply) PASS — `REQUIRE(system_prompt.find("Be concise.") != npos)`
  2. Mock Case 2 (readiness denied) PASS — system_prompt == initial + 事件载荷 4/4 字段
  3. Mock Case 3a (dangerous tool veto) PASS — `REQUIRE(registry.has_tool("dangerous_tool") == false)`
  4. Mock Case 3b (trusted tool add positive) PASS — `REQUIRE(registry.has_tool("trusted_tool") == true)`
  5. ctest 全量 251 + 4 new = 255 tests, zero regression
- **THEN** Decision Record MUST be created with `decision: Go` and rationale referencing each of the 5 criteria
- **AND** new OpenSpec change `adr-0078-model-rsi-pilot` (Wave 3) MUST be initiated
- **AND** Oracle 独立复核 (~30 min) MUST sign-off on raw ctest output (not author's conclusion)

#### Scenario: No-Go decision criteria (any 1 triggers)
- **WHEN** ANY of the following fails:
  - Mock Case 1, 2, 3a, or 3b (deterministic mock test fails)
  - ctest regression introduced
  - Dual-gate integration unable to prevent zero-state-change contract (state leaked on failure)
- **THEN** Decision Record MUST be created with `decision: No-Go` and rationale naming the specific failure
- **AND** Wave 2 skeleton (`apply_harness_mutation`) MUST be archived (NOT deleted, per YAGNI — YAGNI is valid decision, not failure)
- **AND** master plan §十 Drift Log MUST record the No-Go decision
- **AND** Wave 3 (ADR-0078) MUST be deferred to requirement-driven timeline

#### Scenario: No-Go is valid decision (not failure)
- **WHEN** Decision Record is `No-Go`
- **THEN** it MUST be framed as YAGNI-valid decision (avoid需求驱动不成熟时投资 Wave 3)
- **AND** MUST NOT be framed as "pilot failed"

---

### Requirement: ADR-0088-D4-cancellation-honored
The pilot implementation MUST NOT introduce `IHarnessRSI` / `IDatasRsi` / `IModelRSI` interface classes (per ADR-0088 D4 line 96-98 explicit cancellation). Lightweight function pattern is the sole acceptable design.

#### Scenario: no new interface class
- **WHEN** implementation completes
- **THEN** `grep -rnE "class IHarnessRSI|class IDatasRsi|class IModelRSI" src/ include/` MUST return 0 hits
- **AND** `apply_harness_mutation` MUST be a free function in `src/evolution/harness_rsi.cpp` (not a class method)

---

### Requirement: no-core-to-pdk-reverse-dependency (per Oracle C2)
`src/evolution/harness_rsi.cpp` and `include/agenticdsl/evolution/harness_rsi.h` MUST NOT include `agenticdsl/pdk/chat_session.h` directly (would create core→PDK→core cycle, since chat_session.h:108 includes `<core/engine.h>`).

#### Scenario: compile-time guard
- **WHEN** implementation completes
- **THEN** `grep -rnE "include.*pdk/chat_session" src/evolution/ include/agenticdsl/evolution/` MUST return 0 hits
- **AND** `apply_harness_mutation` MUST take `std::string& system_prompt` + `std::vector<std::string>& tools` instead of `ChatConfig&`

---

## REMOVED Requirements

### Requirement: real-llm-1turn-informational-only
**REMOVED** 2026-09-21 per Metis bg_1f291bc4 §2.5.

#### Scenario: removal-justification
- **WHEN** applying C4 scope clarification (per dual-agent review bg_3672cb57 + bg_1f291bc4)
- **THEN** `apply_harness_mutation` signature has no LLM invocation path
- **AND** Real LLM verification deferred to Wave 3 (ADR-0078 Model-RSI pilot)

---

## MODIFIED Requirements

(none)

---

## Cross-references

- `openspec/changes/archive/2026-09-20-2026-09-16-h-d-m-transition-guard/` (C3 ship)
- `openspec/changes/archive/2026-09-21-2026-09-20-adr-0068-appendix-a-evolution-themes/` (D8 ship)
- `docs/adr/adr-0088-h-d-m-transition-guard.md` D4 (line 96-98, IHarnessRSI cancellation)
- `docs/adr/adr-0068-event-emission-contract.md` v2.2 line 253 (evolution.readiness.denied 4-field payload schema)
- `docs/adr/adr-0084-mutation-governance-contract.md` (V1 ship, but C4 NOT using IMutationGovernor::propose)
- `include/agenticdsl/contract/itool_registry.h` (9 虚方法 + unregister_tool_function via Phase 4.0 DB1 fix)
- `include/agenticdsl/contract/imutation_governance.h` (propose/commit/revert, NOT used by C4)
- `include/agenticdsl/pdk/chat_session.h:115-127` (AgentConfig struct, system_prompt field line 123)
- `include/agenticdsl/evolution/transition_guard.h:55-59` (evaluate_readiness 实参)
- AGENTS.md 模式 #4 (SHIP-with-fixes) + 模式 #8 (pre-impl dual-agent review)
- tests/AGENTS.md mode #3 (WARN + SUCCEED + return pattern for CI-skip — N/A since Case 4 removed)
- Oracle bg_3672cb57 (2026-09-21, 7m 16s) — 实施路径审查 + 3 Critical + 3 Major + 3 RED FLAGS
- Metis bg_1f291bc4 (2026-09-21, 7m 18s) — 意图 + 5 DEAL-BREAKER + 6 必读文件 + Case 4 删除建议
