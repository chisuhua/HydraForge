## 1. ReactLoop token parameter (Task 4)

- [x] 1.1 Read current `react_loop.h:80` and confirm `run(prompt, ctx)` signature
- [x] 1.2 Add `std::stop_token token = {}` parameter to `ReactLoop::run()` signature
- [x] 1.3 Verify implementation header is consistent (forward declaration if split)
- [x] 1.4 Build + verify no compilation errors (default parameter keeps callers working)

## 2. PlanExecuteLoop token parameter (Task 5)

- [x] 2.1 Read current `plan_execute_loop.h:198-256` and identify both `std::stop_token{}` lines
- [x] 2.2 Add `std::stop_token token = {}` parameter to `PlanExecuteLoop::run()` signature
- [x] 2.3 Replace `std::stop_token{}` at line 206 with token parameter
- [x] 2.4 Replace `std::stop_token{}` at line 256 with token parameter
- [x] 2.5 Add `token.stop_requested()` check between phases (planning, executing, verifying) — optional safety
- [x] 2.6 Build + verify no compilation errors

## 3. ForkJoinLoop token parameter (Task 6)

- [x] 3.1 Read current `fork_join_loop.h:138-257` and confirm CV wait predicate
- [x] 3.2 Add `std::stop_token token = {}` parameter to `ForkJoinLoop::run()` signature
- [x] 3.3 Update CV wait predicate at lines 251-257 to include `token.stop_requested()`
- [x] 3.4 On token cancellation, call `pool_->stop()` to terminate workers
- [x] 3.5 Add `result.cancelled = true` flag and return early
- [x] 3.6 Build + verify no compilation errors

## 4. Update all callers (grep + verify)

- [x] 4.1 grep all callers of `ReactLoop::run` in `examples/` and `pdk/`
- [x] 4.2 grep all callers of `PlanExecuteLoop::run` in `examples/` and `pdk/`
- [x] 4.3 grep all callers of `ForkJoinLoop::run` in `examples/` and `pdk/`
- [x] 4.4 Update each caller to pass `token` (or rely on default `{}`)
- [x] 4.5 Build + verify zero compilation errors

## 5. Verification

- [x] 5.1 Run full `ctest -j$(nproc)` — 141+ tests PASS, pre-existing 3 failures unchanged
- [x] 5.2 Run ASan preset — 0 new errors
- [x] 5.3 Verify `lsp_diagnostics` clean on all modified files

## 6. Documentation + Ship Gate

- [x] 6.1 Update `AGENTS.md` with Wave 3-A Step 4 ship record
- [x] 6.2 Update `docs/active-status.md` Step 4 row
- [x] 6.3 Update `proposal-approved.md` — move change to "已实施"
- [x] 6.4 Atomic commit per worktree-archive-workflow convention
- [x] 6.5 `openspec archive cancellation-chain-step4-loop-apis --yes`
- [x] 6.6 Sync iteration.json status

## Out of Scope

- Mock provider + E2E mid-loop cancel test — Step 5
- main.cpp `while(getline)` integration with cancellation — separate follow-up
- Phase C `/model` runtime switching — Phase C change