# control-plane-eval Specification

## Purpose

`scripts/control-plane-eval.py` evaluates six prerequisite conditions for Phase 7 (Control Plane) launch, producing a decision table that determines whether to proceed, descope, or defer based on the aggregated status of all conditions.

## ADDED Requirements

### Requirement: Six-Condition Status Auto-Detection

The `control-plane-eval.py` script MUST automatically detect the status of six prerequisite conditions using git log, openspec validate, and documentation grep without manual input.

#### Scenario: All conditions pass

- GIVEN 6 prerequisite conditions are all in PASS state
- WHEN `scripts/control-plane-eval.py` is executed
- THEN the script outputs "ALL PASS" for all six conditions
- AND exits with code 0 indicating readiness for Phase 7

#### Scenario: Some conditions fail

- GIVEN 6 prerequisite conditions where conditions 1, 2, 5, and 6 are FAIL, condition 3 is PASS, condition 4 is PARTIAL
- WHEN `scripts/control-plane-eval.py` is executed
- THEN the script outputs a decision table showing FAIL/PASS/PARTIAL for each condition
- AND reports a recommendation to descope or continue prerequisite shipping

#### Scenario: Condition evidence references

- GIVEN any condition in FAIL or PARTIAL state
- WHEN the script generates its report
- THEN each condition entry includes a file:line evidence reference (e.g., `docs/active-status.md:42`)
- AND the reference points to the authoritative source of that condition's status

### Requirement: Decision Tree Script

The `scripts/control-plane-eval.py` script MUST implement a deterministic decision tree with three output classes: ALL PASS, PARTIAL FAIL, and TOTAL FAIL.

#### Scenario: Decision tree - ALL PASS path

- GIVEN all six conditions return PASS
- WHEN the decision tree is evaluated
- THEN the output is "Decision: PROCEED — Phase 7 launch approved"
- AND no further prerequisites block Phase 7 initiation

#### Scenario: Decision tree - PARTIAL FAIL path

- GIVEN conditions where some are FAIL but none are blocking-critical
- WHEN the decision tree is evaluated
- THEN the output is "Decision: CONDITIONAL — ship remaining prerequisites before Phase 7"
- AND the script lists which conditions require additional work

#### Scenario: Decision tree - TOTAL FAIL path

- GIVEN conditions where ≥2 conditions are in FAIL state including blocking conditions (condition 1 or 2)
- WHEN the decision tree is evaluated
- THEN the output is "Decision: DESCOPE — Phase 7 prerequisites not met"
- AND the script recommends either postponing Phase 7 or descending scope

### Requirement: Audit Document Structure

The evaluation MUST produce a git-tracked audit document at `docs/audits/<date>-control-plane-eval-v1.md` containing condition references, a decision table, and subsequent paths.

#### Scenario: Audit document contains decision table

- GIVEN a successful execution of control-plane-eval.py
- WHEN the audit document is generated
- THEN it contains a decision table listing all six conditions with their status
- AND the table includes columns for Condition Name, Status, Evidence, and Recommendation

#### Scenario: Audit document lists subsequent paths

- GIVEN a PARTIAL FAIL or TOTAL FAIL decision
- WHEN the audit document is generated
- THEN it contains a "Subsequent Paths" section with at least two options
- AND each option includes an estimated effort and next steps

#### Scenario: Audit document cross-references active-status

- GIVEN the audit document is generated
- WHEN it is committed to the repository
- THEN `docs/active-status.md` is updated to reference the audit document
- AND the reference appears within 24 hours of document generation

### Requirement: Active-Status Cross-Reference

The `docs/active-status.md` file MUST reflect the six prerequisite condition statuses and link to the latest control-plane-eval audit document.

#### Scenario: Active-status reflects current condition states

- GIVEN the six prerequisite conditions have various states (PASS/FAIL/PARTIAL)
- WHEN `docs/active-status.md` is current
- THEN each condition's status is visible in §一 (Phase Status) or §四 (Pre-Launch Conditions)
- AND the status matches the latest control-plane-eval.py output

#### Scenario: Active-status updated within 24 hours

- GIVEN a new audit document is generated and committed
- WHEN 24 hours have elapsed
- THEN `docs/active-status.md` contains a reference to the new audit document
- AND the reference is traceable via git log
