## ADDED Requirements

### Requirement: ReactLoop::run accepts std::stop_token

The `ReactLoop::run()` method SHALL accept an optional `std::stop_token` parameter and forward it to internal LLM calls.

The default parameter `std::stop_token{}` preserves backward compatibility.

#### Scenario: ReactLoop with default token preserves legacy behavior
- **WHEN** `ReactLoop::run(prompt, ctx)` is called without explicit token
- **THEN** the default `std::stop_token{}` SHALL be used (never cancels)
- **AND THEN** behavior is identical to pre-change

#### Scenario: ReactLoop with cancelled token breaks loop
- **WHEN** `ReactLoop::run(prompt, ctx, token)` is called with `token.stop_requested() == true`
- **THEN** the loop SHALL exit within one iteration
- **AND THEN** return a `LoopResult` with cancelled status

### Requirement: PlanExecuteLoop::run accepts std::stop_token

The `PlanExecuteLoop::run()` method SHALL accept an optional `std::stop_token` parameter and forward it to internal LLM calls at lines 206 and 256.

The default parameter `std::stop_token{}` preserves backward compatibility.

#### Scenario: PlanExecuteLoop cancels during verify phase
- **WHEN** `PlanExecuteLoop::run(...)` enters verify phase
- **AND WHEN** `token.stop_requested()` is observed
- **THEN** the verify phase SHALL exit immediately
- **AND THEN** return a `LoopResult` with cancelled status (no retry)

#### Scenario: PlanExecuteLoop with default token preserves legacy behavior
- **WHEN** `PlanExecuteLoop::run(...)` is called without explicit token
- **THEN** default `std::stop_token{}` SHALL be used at lines 206 and 256
- **AND THEN** behavior is identical to pre-change

### Requirement: ForkJoinLoop::run accepts std::stop_token

The `ForkJoinLoop::run()` method SHALL accept an optional `std::stop_token` parameter and propagate it through:
1. The condition variable wait predicate (include `token.stop_requested()` as exit condition)
2. Worker pool termination (call `pool_->stop()` on cancellation)

The default parameter `std::stop_token{}` preserves backward compatibility.

#### Scenario: ForkJoinLoop cancels worker pool on token
- **WHEN** `ForkJoinLoop::run(branches, ctx, token)` is called
- **AND WHEN** `token.stop_requested()` is observed in the CV wait predicate
- **THEN** the worker pool SHALL receive `pool_->stop()` call
- **AND THEN** the loop SHALL return without waiting for all branches

#### Scenario: ForkJoinLoop with default token preserves legacy behavior
- **WHEN** `ForkJoinLoop::run(branches, ctx)` is called without explicit token
- **THEN** default `std::stop_token{}` SHALL be used
- **AND THEN** CV wait predicate does NOT include token stop check
- **AND THEN** behavior is identical to pre-change

### Requirement: All callers updated with token argument

All call sites of `ReactLoop::run`, `PlanExecuteLoop::run`, and `ForkJoinLoop::run` SHALL pass a token argument (either explicit or via default parameter).

The default parameter ensures compilation succeeds for any call site, but production code paths SHALL pass a real token when available (e.g., from ChatSession's cancellation chain).

#### Scenario: All callers compile after API change
- **WHEN** the project is rebuilt after Step 4 ships
- **THEN** no compilation errors related to missing token arguments
- **AND THEN** all existing tests pass (with default token preserving legacy behavior)