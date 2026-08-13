# ADR-0073 W1 Implementation-Scope Audit Design

**Date**: 2026-08-13
**Status**: Design approved for review
**Roadmap task**: Phase 6b W1, `execution-plane-wave2`

## Goal

Reconcile ADR-0073's documented status with the implementation evidence that already exists in HydraForge. The change will establish an evidence-backed `Partial` status without claiming that the ToolMetadata V3 or ToolCoordinator schema-validation implementation is complete.

## Scope

In scope:

- Audit ADR-0073 decisions D1-D6 against the current repository.
- Record file, line, test, and commit evidence for shipped or partially shipped behavior.
- Update ADR-0073's status and implementation-scope note.
- Reconcile the ADR's review timing note with the roadmap's W1 timing.
- Update the single ADR-0073 row in `docs/README.md`.
- Correct the roadmap evidence wording so it refers to the actual Phase 6a manifest work rather than Sprint 21.
- Mark the W1 documentation task complete only after the audit and synchronization checks pass.

Out of scope:

- `ToolMetadata` V3 fields and `ValidationMode`.
- `ToolSchemaValidator` or any runtime schema-validation pipeline.
- `DECLARE_TOOL` V3 automatic schema generation.
- Changes to `ToolCoordinator`, `tool_macros.h`, `execution_policy.h`, or parser/runtime code.
- Rewriting `docs/specs/dsl.md` schema sections; that belongs to the later schema implementation work.
- Any change to the existing test count or production behavior.

## Evidence Model

The audit will distinguish the following categories:

- **Shipped**: behavior directly implemented and covered by repository evidence.
- **Partial**: only a bounded portion of the ADR decision is implemented.
- **Deferred**: explicitly owned by a later roadmap task.
- **Not implemented**: no corresponding implementation exists.

The primary evidence expected for D1 is the Phase 6a PDK manifest boundary:

- `include/agenticdsl/pdk/manifest.h` exposes JSON Schema 2020-12 `input_schema` and `output_schema` fields.
- `src/modules/pdk/manifest_validator.cpp` enforces the manifest input schema requirement.
- Existing manifest validator/type tests cover the behavior.

This evidence does not count as ToolMetadata V3 or ToolCoordinator validation. Those remain deferred to Phase 6c C8/C9.

## Status Reconciliation

ADR-0073 will move from `Proposed` to `Partial` only with an explicit evidence note stating:

- D1 has partial adoption at the PDK manifest boundary.
- D2, D3, and D4 remain unimplemented and are owned by Phase 6c C8/C9.
- Existing subgraph `output_schema` parsing is related evidence but is not ToolMetadata V3.
- The W1 flip is an evidence-based partial-status update; the final `Approved` gate remains tied to the complete ADR acceptance criteria.

The ADR review-timing text will be amended so it no longer conflicts with the roadmap's W1 partial-status transition.

## Documentation Changes

The implementation proposal will update only these documentation surfaces:

1. `docs/adr/adr-0073-tool-json-schema-contract.md`
2. `docs/adr/adr-0073-impl-scope-audit.md`
3. The ADR-0073 row in `docs/README.md`
4. The relevant W1/evidence wording in `roadmap.md`

No broad documentation rewrite is included.

## Verification

The change must demonstrate:

- ADR-0073 status and evidence are internally consistent.
- D2/D3/D4 ownership remains visible and deferred to C8/C9.
- The roadmap no longer attributes the evidence to Sprint 21.
- `tools/adr_lint.py` passes.
- `tools/docs_drift_audit.py` reports no new drift attributable to this change.
- `openspec validate --strict` passes for the resulting change artifacts.
- Existing C++ tests remain unchanged and the repository's current test baseline is not regressed.

## Risks and Mitigations

The manifest-level evidence is weaker than a ToolMetadata/ToolCoordinator implementation. The audit must state that limitation explicitly and avoid describing the ADR as fully implemented. If the project governance check rejects this evidence as insufficient for `Partial`, the fallback is to retain `Proposed` and correct the roadmap W1 claim instead of fabricating implementation progress.
