# Spec: provider-llm-tool-empty-passthrough

> **STATUS: PLACEHOLDER** — 5 placeholder Requirements with TBD Scenarios

## ADDED Requirements

### Requirement: ProviderLLMTool MUST fail-fast on empty text response
The `ProviderLLMTool` (per `pdk/loop_agent/src/pdk_entry.cpp:402/434/524`) SHALL throw a `runtime_error` (per F1 minimal fix principle) when the underlying LLM provider returns an empty text response. The error message SHALL include the provider name and node name for diagnostic purposes.

#### Scenario: empty text triggers fail-fast
- **WHEN** ProviderLLMTool invokes an LLM provider that returns `text == ""`
- **THEN** it MUST throw `runtime_error` with message format "[ProviderLLMTool] provider=X node=Y failed: empty text response"
- **AND** the caller's ctx MUST remain unchanged (no partial writes)

#### Scenario: non-empty text passes through
- **WHEN** ProviderLLMTool invokes an LLM provider that returns non-empty text
- **THEN** it MUST complete normally and write `ctx["text"] = response.text`
- **AND** MUST NOT throw any exception

### Requirement: ProviderLLMTool fail-fast MUST be defense-in-depth, not primary defense
The ProviderLLMTool fail-fast check is a defense-in-depth hardening (per AGENTS.md Pattern #1 step 4) and MUST NOT replace the primary fail-fast at `node_executor.cpp` main path `:194-205` + stream path `:147-157`. Both layers SHALL independently validate empty text.

#### Scenario: independent failure detection
- **WHEN** an empty text response bypasses node_executor (direct loop_agent invocation)
- **THEN** ProviderLLMTool fail-fast MUST still trigger
- **AND** the failure MUST surface to the caller

#### Scenario: no double-throw on concurrent paths
- **WHEN** both ProviderLLMTool and node_executor validate the same empty text response
- **THEN** exactly ONE throw SHALL occur (no double-throw / no silent duplicate failure)

### Requirement: Test coverage MUST verify fail-fast and regression
The test binary `tests/test_provider_llm_tool_empty.cpp` (or extension of `tests/test_loop_agent_plugin.cpp`) MUST contain at least 3 test cases verifying: empty text → throw, non-empty text → pass-through, and regression (F1 node_executor fix still works).

#### Scenario: empty text triggers expected throw
- **WHEN** `MockEmptyTextProvider` returns `text == ""`
- **THEN** ProviderLLMTool call MUST throw with expected error message
- **AND** ctx MUST remain unchanged

#### Scenario: non-empty text passes fail-fast
- **WHEN** `MockNonEmptyTextProvider` returns `text == "expected_response"`
- **THEN** ProviderLLMTool call MUST complete successfully
- **AND** `ctx["text"]` MUST equal "expected_response"

#### Scenario: F1 regression guard
- **WHEN** `tests/test_dsl_engine_ctx_bridge.cpp` Case 4 (GREEN guard via MockLLMEmptyTool) runs
- **THEN** it MUST still PASS (verifies node_executor fail-fast not regressed)

### Requirement: Latent Sites tracking MUST record the fix
The F1 design.md Latent Sites table (per `openspec/changes/archive/2026-09-18-fix-react-decide-empty-response/design.md`) SHALL be updated to mark Site #3 (ProviderLLMTool) as ✅ FIXED with commit hash and Oracle session reference.

#### Scenario: Latent Sites table has updated Site #3
- **WHEN** the F1 design.md Latent Sites table is read
- **THEN** Site #3 row SHALL be marked ✅ FIXED
- **AND** SHALL cite the commit hash + Oracle session id

### Requirement: AGENTS.md Pattern #1 step 4 MUST cite this change as canonical example
The `AGENTS.md` Engineering Pattern #1 step 4 (systematic latent sites recording) SHALL cite this change as a canonical example of defense-in-depth hardening across multiple layers.

#### Scenario: AGENTS.md references the change
- **WHEN** AGENTS.md Pattern #1 step 4 is read
- **THEN** it SHALL reference `2026-09-18-provider-llm-tool-empty-passthrough` as a defense-in-depth example
- **AND** SHALL explain the dual-layer (node_executor + ProviderLLMTool) rationale

### Requirement: Docs drift gate MUST remain clean
All drift detection tools MUST continue to report 0 DRIFT items after the fix is applied.

#### Scenario: docs_drift_audit returns 0 DRIFT
- **WHEN** `tools/docs_drift_audit.py` is executed
- **THEN** it MUST exit 0 with 0 DRIFT items

#### Scenario: openspec validate --strict returns valid
- **WHEN** `openspec validate --strict` is executed
- **THEN** it MUST report "Change is valid" for `2026-09-18-provider-llm-tool-empty-passthrough`