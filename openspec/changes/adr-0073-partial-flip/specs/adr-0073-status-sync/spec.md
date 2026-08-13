## ADDED Requirements

### Requirement: ADR-0073 Status Sync Evidence

ADR-0073 (Tool JSON Schema Contract) MUST ship a documented implementation-scope audit and a status update that reflects the actual evidence available in the repository, without claiming that the ToolMetadata V3 or ToolCoordinator schema-validation implementation is complete.

#### Scenario: Status is updated to Partial with evidence

- GIVEN ADR-0073 is currently `Proposed`
- WHEN this change is shipped
- THEN `docs/adr/adr-0073-tool-json-schema-contract.md` status is `Partial`
- AND a ship-evidence section cites Phase 6a PDK manifest schema fields as the source of D1 partial adoption.

#### Scenario: Implementation-scope audit documents D1-D6

- GIVEN ADR-0073 contains decisions D1-D6
- WHEN this change is shipped
- THEN `docs/adr/adr-0073-impl-scope-audit.md` classifies each decision as Shipped, Partial, Deferred, or Not implemented
- AND D2, D3, and D4 are Deferred to Phase 6c C8/C9.

#### Scenario: No code changes are introduced

- GIVEN this change is documentation-only
- WHEN `ctest` is executed
- THEN the test count and outcomes remain unchanged (147/147)
- AND no source files in `src/`, `include/`, `pdk/`, or `tests/` are modified.

#### Scenario: Documentation gates pass

- GIVEN the change updates `docs/adr/adr-0073-tool-json-schema-contract.md`, `docs/README.md`, and `roadmap.md`
- WHEN the change is shipped
- THEN `tools/adr_lint.py` exits 0
- AND `tools/docs_drift_audit.py` reports no new DRIFT attributable to this change.
