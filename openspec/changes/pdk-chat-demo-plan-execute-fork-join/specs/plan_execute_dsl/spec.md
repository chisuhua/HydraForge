## ADDED Requirements

### Requirement: PlanExecuteLoop DSL Example

The `examples/pdk_chat_demo/dsl/plan_execute_example.agent.md` MUST be a valid PlanExecuteLoop DSL example demonstrating 3-state Planning→Executing→Verifying with retry on verification failure.

#### Scenario: PlanExecuteLoop runs end-to-end

- GIVEN a PlanExecuteLoop bound to a MockLLMProvider that returns a plan DSL followed by a "verify: yes" response
- WHEN `PlanExecuteLoop::run("user_input", ctx)` is called
- THEN the loop MUST execute Plan, Execute, Verify states and return `LoopResult{success=true, total_steps≥2}`.

### Requirement: PlanExecuteLoop retry on verify failure

The PlanExecuteLoop MUST retry the Execute+Verify cycle when verification fails, up to the configured retry budget.

#### Scenario: Loop retries on verify failure

- GIVEN a PlanExecuteLoop bound to a MockLLMProvider that returns a verify failure once followed by a success
- WHEN the loop runs
- THEN the loop MUST retry Execute+Verify and ultimately return `success=true` with `retries_used≥1`.

### Requirement: PlanExecuteLoop reads DSL example file

The PlanExecuteLoop example DSL MUST be loadable from the disk path and parse without errors.

#### Scenario: DSL example file parses successfully

- GIVEN `examples/pdk_chat_demo/dsl/plan_execute_example.agent.md` exists on disk
- WHEN the demo loads the DSL
- THEN the file MUST parse as a valid PlanExecuteLoop workflow with at least Start/ToolCall/End nodes.