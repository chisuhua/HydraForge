# dsl-extensions Specification

## Purpose

This specification defines three DSL extensions (D2 `$var` variable reference, D3 `exec:` declarative style, D5 dual-syntax coexistence lint) that are conditionally shipped based on the C4 Evidence Gate resolution. All extensions operate on top of the ADR-0074 baseline DSL and must preserve 100% backward compatibility with existing `.agent.md` files.

## ADDED Requirements

### Requirement: $var Variable Reference Syntax

The parser MUST support `$var` token as a compact output-reference syntax. `node_a → $output_b` MUST be parsed equivalently to the legacy `node_a → output_b` form where `output_b` refers to a named output of another node. Namespace collision between output names, input fields, and local variables MUST result in a parse error (not implicit selection).

#### Scenario: $var resolves to output node in same DAG

- GIVEN a DAG fragment with `node_b` having a named output `result`
- WHEN the parser encounters `node_a → $result`
- THEN the edge is created from `node_a` to `node_b`'s `result` output port
- AND the semantics are identical to the legacy `node_a → result` form.

#### Scenario: $var namespace collision raises parse error

- GIVEN a node with both an output named `val` and a local variable named `val`
- WHEN the parser encounters `other_node → $val`
- THEN a parse error is raised indicating the ambiguous namespace reference
- AND no edge is silently created.

#### Scenario: $var with non-existent output raises parse error

- GIVEN a reference `$missing` to an output that does not exist on any upstream node
- WHEN the parser processes the edge
- THEN a parse error is raised identifying the unknown output `missing`.

### Requirement: exec Declarative Style Fork/Join

The parser MUST support `exec: [shell/exec, fs/read]` as a declarative sugar that is transformed into an equivalent fork/join DAG structure. The generated fork/join MUST fire the join node only after all parallel sub-nodes have exited, with semantics identical to a manually written fork/join pair.

#### Scenario: exec list expands to fork/join DAG

- GIVEN `exec: [tool_a, tool_b, tool_c]` in a DSL node definition
- WHEN the parser processes this form
- THEN it generates a fork node with three parallel outgoing edges
- AND a join node that receives edges from all three parallel branches
- AND the join node fires only after all three sub-nodes complete.

#### Scenario: exec fork/join is semantically equivalent to manual fork/join

- GIVEN a manually written fork/join DAG with N parallel branches
- WHEN the same N branches are expressed as `exec: [...]`
- THEN both forms produce identical execution semantics (all branches complete before join fires)
- AND the linter reports no difference between the two forms.

#### Scenario: exec with single item behaves like no-op fork/join

- GIVEN `exec: [single_tool]`
- WHEN the parser processes this form
- THEN the single node is executed directly without fork/join overhead
- AND no fork or join node is generated.

### Requirement: Dual Syntax Coexistence Lint

The lint tool MUST detect when a `.agent.md` file mixes old and new syntax forms and MUST emit a line-level warning (not an error) for legacy parts, without blocking commit. Users MAY suppress warnings per line via `# lint:disable dual-syntax` comment. The lint MUST NOT re-report legacy syntax in files that were shipped before the D2/D3 ship date (heuristic: file mtime after D2/D3 ship timestamp and git log shows new submission).

#### Scenario: Mixed syntax triggers warning with line number and fix suggestion

- GIVEN a `.agent.md` containing both `$var` references and legacy `→ output_name` references in the same file
- WHEN the lint tool runs on this file
- THEN each legacy reference receives a warning line of the form `<file>:<line>: warning: legacy syntax '→ output_name'; consider '$output_name'`
- AND no error is raised (commit is not blocked).

#### Scenario: lint:disable comment suppresses warning on specific line

- GIVEN a `.agent.md` with a legacy reference on line 42
- WHEN line 41 contains `# lint:disable dual-syntax`
- THEN the lint tool does not emit a warning for line 42
- AND the disable comment itself does not trigger a lint warning.

#### Scenario: Historical shipped files are not re-reported

- GIVEN a `.agent.md` file with mtime before the D2/D3 ship timestamp
- OR the file's git history shows it was committed before D2/D3 ship
- WHEN the lint tool runs
- THEN no warnings are emitted for legacy syntax in that file
- AND the lint tool exits 0.

#### Scenario: New context legacy syntax heuristic detection

- GIVEN a `.agent.md` file with mtime after D2/D3 ship timestamp AND git log shows a recent commit
- WHEN the lint tool detects legacy syntax in a section that uses new D2/D3 features
- THEN a warning is emitted for the legacy part with a fix suggestion
- AND the new D2/D3 syntax parts are not flagged.
