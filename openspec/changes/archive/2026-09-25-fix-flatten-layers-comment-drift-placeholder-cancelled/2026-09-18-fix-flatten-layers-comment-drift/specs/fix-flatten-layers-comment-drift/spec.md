# Spec: fix-flatten-layers-comment-drift

> **STATUS: PLACEHOLDER** — 5 placeholder Requirements with TBD Scenarios

## ADDED Requirements

### Requirement: Code comments MUST NOT reference `flatten_layers` + react/loop path
All source code comments (`src/`, `tests/`) MUST NOT imply that `flatten_layers` (per `include/agenticdsl/types/context_flatten.h`) is on the react agent loop path. The `DSLEngine::run(LayeredContext)` flow SHALL pass `ctx.working` flat top-level keys to scheduler; `flatten_layers` is unrelated to react loop data flow.

#### Scenario: grep audit returns zero false references
- **WHEN** `grep -rn "flatten_layers" src/ tests/ | grep -iE "react|loop|decide"` is executed
- **THEN** it MUST return zero matches outside `include/agenticdsl/types/context_flatten.h` definition site

#### Scenario: legacy comments marked as superseded
- **WHEN** a comment historically referenced `flatten_layers` as root cause
- **THEN** the comment SHALL be marked with 【初判错】 prefix or removed entirely
- **AND** SHALL reference Oracle session `ses_f4d05cdb0` as the corrected root cause (per Pattern #1 step 4)

### Requirement: Master Plan Drift Log MUST distinguish initial-judgment from confirmed root cause
The `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §十 Drift Log entries that initially misidentified a root cause MUST be tagged with 【初判错】 prefix to preserve audit trail while preventing future misreference.

#### Scenario: Drift Log grep returns correctly-tagged lines
- **WHEN** `grep "flatten_layers" docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` is executed
- **THEN** it MUST return only entries explicitly tagged 【初判错】 or referencing the corrected root cause
- **AND** the entry SHALL cite Oracle `ses_f4d05cdb0` for correctness

### Requirement: AGENTS.md Pattern #1 step 4 MUST add "initial-judgment separation" sub-entry
The `AGENTS.md` Engineering Pattern #1 step 4 (systematic recording of latent sites) SHALL be extended with a sub-entry explicitly addressing the separation of initial-judgment root cause from Oracle-confirmed root cause.

#### Scenario: Pattern #1 step 4 contains the new sub-entry
- **WHEN** AGENTS.md is read
- **THEN** Pattern #1 step 4 SHALL contain a "初判与确认根因分离" sub-section
- **AND** SHALL cite Oracle `ses_f4d05cdb0` as the canonical example

### Requirement: Test file comments MUST NOT reference initial-judgment root cause
The `tests/test_dsl_engine_ctx_bridge.cpp` and `tests/test_react_loop_real_llm.cpp` files (F1 ship test artifacts) MUST NOT contain comments referencing `flatten_layers` as the F1 root cause. Test comments SHALL reference the actual root cause (LLM empty text silent pass-through).

#### Scenario: test grep audit returns zero false references
- **WHEN** `grep -n "flatten_layers" tests/test_dsl_engine_ctx_bridge.cpp tests/test_react_loop_real_llm.cpp` is executed
- **THEN** it MUST return zero matches

### Requirement: Docs drift gate MUST remain clean
All drift detection tools MUST continue to report 0 DRIFT items after the comment drift fix is applied.

#### Scenario: docs_drift_audit returns 0 DRIFT
- **WHEN** `tools/docs_drift_audit.py` is executed
- **THEN** it MUST exit 0 with 0 DRIFT items

#### Scenario: openspec validate --strict returns valid
- **WHEN** `openspec validate --strict` is executed
- **THEN** it MUST report "Change is valid" for `2026-09-18-fix-flatten-layers-comment-drift`