# evidence-gate Specification

## Purpose

Evidence Gate is a Go/No-Go decision mechanism for Wave progression based on quantified thresholds (parse-valid ≥85%, task-success L1 ≥70%, L2 ≥50%, L3 ≥30%), producing a PASS/FAIL/CONDITIONAL/ABORT verdict that serves as the sole objective criterion for advancing from Wave 2 to Wave 3 per ADR-0074 §Decision D4.

## ADDED Requirements

### Requirement: evaluate_gate Decision Tree Function

The evaluate_gate(parse_valid, task_success_l1, task_success_l2, task_success_l3) function MUST accept four threshold inputs and return one of four enumerated decision states: PASS, FAIL, CONDITIONAL, or ABORT.

#### Scenario: parse-valid below 85% triggers FAIL

- GIVEN parse-valid = 82.1% (< 85%)
- WHEN evaluate_gate is invoked with any task-success values
- THEN the function returns FAIL
- AND the decision triggers ADR-0072 D2 ($var) implementation (C5).

#### Scenario: parse-valid in 85-90% band triggers CONDITIONAL

- GIVEN parse-valid = 87.5% (85% ≤ x < 90%)
- WHEN evaluate_gate is invoked with task-success L1 = 73.5% (≥70%)
- THEN the function returns CONDITIONAL
- AND the decision triggers ADR-0072 D3 declarative style (C6), not C5.

#### Scenario: parse-valid at or above 90% triggers PASS

- GIVEN parse-valid = 90.1% (≥ 90%)
- WHEN evaluate_gate is invoked with task-success L1 = 73.5%, L2 = 55%, L3 = 35%
- THEN the function returns PASS
- AND no C5/C6 triggering occurs.

#### Scenario: Missing or incomplete measurement data triggers ABORT

- GIVEN C3 measurement data is missing or incomplete (golden suite < 50 tasks / 3 models not all reported / YAML report fields missing)
- WHEN evaluate_gate is invoked
- THEN the function returns ABORT
- AND the audit document §Data Integrity Check section lists missing items.

### Requirement: Evidence Gate Audit Document Structure

Every Evidence Gate decision document MUST contain exactly five sections: §Data Plan, §Measurement Method, §Decision Tree, §Action Items, and §Verdict.

#### Scenario: Audit document contains all five required sections

- GIVEN a new Evidence Gate evaluation is executed
- WHEN the verdict is recorded in docs/audits/<date>-evidence-gate-v1.md
- THEN the document contains §Data Plan + §Measurement Method + §Decision Tree + §Action Items + §Verdict
- AND each section is non-empty.

#### Scenario: Verdict section reflects evaluate_gate output

- GIVEN evaluate_gate returns PASS/FAIL/CONDITIONAL/ABORT
- WHEN the audit document is finalized
- THEN the §Verdict section records the exact state returned by evaluate_gate
- AND all numeric values cite specific file:line evidence from C3 baseline report.

### Requirement: Active-Status Sync Within 24h

Within 24 hours of an Evidence Gate verdict, docs/active-status.md §一 (Phase 6c status line) and §四 (Phase 7 startup condition item #1) MUST be updated to reflect the decision.

#### Scenario: PASS verdict updates active-status.md correctly

- GIVEN Evidence Gate returns PASS
- WHEN 24 hours have elapsed since the verdict
- THEN docs/active-status.md §一 Phase 6c status line shows "Evidence Gate PASS = true"
- AND §四 Phase 7 startup condition item #1 shows "PASS (parse-valid ≥85% + task-success L1 ≥70%)".

#### Scenario: FAIL verdict updates active-status.md and references C5

- GIVEN Evidence Gate returns FAIL (parse-valid < 85%)
- WHEN 24 hours have elapsed since the verdict
- THEN docs/active-status.md §一 Phase 6c status line records FAIL with evidence citation
- AND §四 Phase 7 startup condition item #1 is not marked satisfied.

#### Scenario: CONDITIONAL verdict updates active-status.md and references C6

- GIVEN Evidence Gate returns CONDITIONAL (85% ≤ parse-valid < 90%)
- WHEN 24 hours have elapsed since the verdict
- THEN docs/active-status.md §一 records CONDITIONAL with C6 trigger note
- AND §四 Phase 7 startup condition item #1 remains unsatisfied until C6 ships.

### Requirement: C3 Baseline Data Citation Traceability

Every numeric value in the Evidence Gate verdict MUST cite a specific file:line reference to the C3 baseline report, enabling audit traceability.

#### Scenario: Numeric values cite C3 baseline report with file:line links

- GIVEN Evidence Gate evaluates with parse-valid = 88.2%
- WHEN the verdict document is generated
- THEN the parse-valid value includes a citation like "(docs/audits/<date>-execution-baseline-v1.md:42)"
- AND no numeric value in the verdict is uncited.

#### Scenario: Threshold definitions align with ADR-0074 D4

- GIVEN the Evidence Gate thresholds are parse-valid 85%, task-success L1 70%, L2 50%, L3 30%
- WHEN the audit document §Decision Tree is authored
- THEN the thresholds match ADR-0074 §Decision D4 verbatim
- AND no threshold value is adjusted without an ADR-0074 amendment.
