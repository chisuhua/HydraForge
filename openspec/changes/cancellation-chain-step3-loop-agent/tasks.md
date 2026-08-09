## 1. loop_agent cancellation_id parsing (Task 3)

- [x] 1.1 Add `static CancellationRegistry g_loop_registry;` to `pdk_entry.cpp` file scope
- [x] 1.2 Parse `cancellation_id` from loop_args JSON in `loop/run` handler at `pdk_entry.cpp:170-190`
- [x] 1.3 Resolve to `std::stop_token` via `g_loop_registry.resolve_token(id)` (empty token if id absent)
- [x] 1.4 Add early-return check `if (token.stop_requested()) return cancelled_result;` before provider invocation
- [x] 1.5 Replace `std::stop_token{}` at `pdk_entry.cpp:229` with resolved token
- [x] 1.6 Build pdk_chat_demo and verify mock mode E2E (with cancel signal) does not crash

## 2. NodeExecutor dispatch_to_tool token param (Task 7)

- [x] 2.1 Modify `node_executor.cpp:349-351` to add `std::stop_token token = {}` parameter to `dispatch_to_tool`
- [x] 2.2 Forward token to `tool_coordinator_->execute(meta, tool_ctx, args, token)` at `node_executor.cpp:387`
- [x] 2.3 Update all internal callers of dispatch_to_tool (grep + verify)
- [x] 2.4 Modify YieldNode path at `node_executor.cpp:475` to forward real token to `generate_stream()`
- [x] 2.5 Verify existing NodeExecutor tests pass (no regression)

## 3. ToolCoordinator execute() token param (Task 8)

- [x] 3.1 Modify `tool_coordinator.cpp:195-203` to add `std::stop_token token = {}` parameter to `execute`
- [x] 3.2 Add `if (token.stop_requested())` check at entry with audit emit
- [x] 3.3 Emit `tool.audit.denied` event with `args = {"tool": meta.name, "reason": "cancelled"}` on short-circuit
- [x] 3.4 Update all internal callers of execute (grep + verify)
- [x] 3.5 Verify existing ToolCoordinator tests pass

## 4. Test scaffold (TDD 5 steps)

- [x] 4.1 Create `examples/pdk_chat_demo/tests/test_loop_agent_cancellation.cpp` (new file)
- [x] 4.2 TEST_CASE: cancellation_id resolves to valid token in loop_agent
- [x] 4.3 TEST_CASE: NodeExecutor forwards token to ToolCoordinator (identity check)
- [x] 4.4 TEST_CASE: ToolCoordinator short-circuits on cancelled token + emits audit event

## 5. Verification

- [x] 5.1 Run new tests `test_loop_agent_cancellation` — 3 tests PASS
- [x] 5.2 Run full `ctest -j$(nproc)` — 138+ tests PASS, 3 pre-existing unchanged
- [x] 5.3 Run ASan preset — 0 new errors
- [x] 5.4 Verify `lsp_diagnostics` clean

## 6. Documentation + Ship Gate

- [x] 6.1 Update `AGENTS.md` with Wave 3-A Phase B Step 3 ship record
- [x] 6.2 Update `docs/active-status.md` Step 3 row
- [x] 6.3 Update `proposal-approved.md` — move change to "已实施"
- [x] 6.4 Atomic commit per worktree-archive-workflow convention
- [x] 6.5 `openspec archive cancellation-chain-step3-loop-agent --yes`
- [x] 6.6 Sync iteration.json status

## Out of Scope

- 3 Loop APIs token params (ReactLoop/PlanExecuteLoop/ForkJoinLoop) — Step 4
- Mock provider + E2E mid-loop cancel test — Step 5
- main.cpp `while(getline)` integration with cancellation — separate follow-up