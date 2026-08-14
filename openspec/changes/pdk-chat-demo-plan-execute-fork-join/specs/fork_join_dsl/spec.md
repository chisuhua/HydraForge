## ADDED Requirements

### Requirement: ForkJoinLoop DSL Example

The `examples/pdk_chat_demo/dsl/fork_join_example.agent.md` MUST be a valid ForkJoinLoop DSL example demonstrating 4-state Forking→Executing→Joining with multiple concurrent branches.

#### Scenario: ForkJoinLoop runs 3 branches concurrently

- GIVEN a ForkJoinLoop bound to a MockLLMProvider configured with 3 branches
- WHEN `ForkJoinLoop::run(branches, ctx)` is called with `num_threads≥3`
- THEN the loop MUST execute all 3 branches concurrently and return `LoopResult{success=true}` with merged results.

### Requirement: ForkJoinLoop handles branch failure gracefully

The ForkJoinLoop MUST surface branch failures in `LoopResult::failed_phase` rather than crashing.

#### Scenario: Loop reports failed branch

- GIVEN a ForkJoinLoop where one branch's MockLLMProvider returns an error
- WHEN the loop runs
- THEN the loop MUST mark `success=false` and report the failed branch in `LoopResult::failed_phase`.

### Requirement: ForkJoinLoop reads DSL example file

The ForkJoinLoop example DSL MUST be loadable from the disk path and parse without errors.

#### Scenario: DSL example file parses successfully

- GIVEN `examples/pdk_chat_demo/dsl/fork_join_example.agent.md` exists on disk
- WHEN the demo loads the DSL
- THEN the file MUST parse as a valid ForkJoinLoop workflow with at least one ForkJoin node listing 3+ branches.