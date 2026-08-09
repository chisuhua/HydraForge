## 1. ReactLoop token parameter (Task 4)

- [ ] 1.1 Read current `react_loop.h:80` and confirm `run(prompt, ctx)` signature
- [ ] 1.2 Add `std::stop_token token = {}` parameter to `ReactLoop::run()` signature
- [ ] 1.3 Verify implementation header is consistent (forward declaration if split)
- [ ] 1.4 Build + verify no compilation errors (default parameter keeps callers working)

## 2. PlanExecuteLoop token parameter (Task 5)

- [ ] 2.1 Read current `plan_execute_loop.h:198-256` and identify both `std::stop_token{}` lines
- [ ] 2.2 Add `std::stop_token token = {}` parameter to `PlanExecuteLoop::run()` signature
- [ ] 2.3 Replace `std::stop_token{}` at line 206 with token parameter
- [ ] 2.4 Replace `std::stop_token{}` at line 256 with token parameter
- [ ] 2.5 Add `token.stop_requested()` check between phases (planning, executing, verifying) — optional safety
- [ ] 2.6 Build + verify no compilation errors

## 3. ForkJoinLoop token parameter (Task 6)

- [ ] 3.1 Read current `fork_join_loop.h:138-257` and confirm CV wait predicate
- [ ] 3.2 Add `std::stop_token token = {}` parameter to `ForkJoinLoop::run()` signature
- [ ] 3.3 Update CV wait predicate at lines 251-257 to include `token.stop_requested()`
- [ ] 3.4 On token cancellation, call `pool_->stop()` to terminate workers
- [ ] 3.5 Add `result.cancelled = true` flag and return early
- [ ] 3.6 Build + verify no compilation errors

## 4. Update all callers (grep + verify)

- [ ] 4.1 grep all callers of `ReactLoop::run` in `examples/` and `pdk/`
- [ ] 4.2 grep all callers of `PlanExecuteLoop::run` in `examples/` and `pdk/`
- [ ] 4.3 grep all callers of `ForkJoinLoop::run` in `examples/` and `pdk/`
- [ ] 4.4 Update each caller to pass `token` (or rely on default `{}`)
- [ ] 4.5 Build + verify zero compilation errors

## 5. Verification

- [ ] 5.1 Run full `ctest -j$(nproc)` — 141+ tests PASS, pre-existing 3 failures unchanged
- [ ] 5.2 Run ASan preset — 0 new errors
- [ ] 5.3 Verify `lsp_diagnostics` clean on all modified files

## 6. Documentation + Ship Gate

- [ ] 6.1 Update `AGENTS.md` with Wave 3-A Step 4 ship record
- [ ] 6.2 Update `docs/active-status.md` Step 4 row
- [ ] 6.3 Update `proposal-approved.md` — move change to "已实施"
- [ ] 6.4 Atomic commit per worktree-archive-workflow convention
- [ ] 6.5 `openspec archive cancellation-chain-step4-loop-apis --yes`
- [ ] 6.6 Sync iteration.json status

## Out of Scope

- Mock provider + E2E mid-loop cancel test — Step 5
- main.cpp `while(getline)` integration with cancellation — separate follow-up
- Phase C `/model` runtime switching — Phase C change