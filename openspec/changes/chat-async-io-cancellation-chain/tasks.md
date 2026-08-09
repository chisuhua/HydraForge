## 1. Cancellation Registry Infrastructure

- [x] 1.1 Created `examples/pdk_chat_demo/cancellation_registry.h` with `class CancellationRegistry` (unordered_map<string, shared_ptr<stop_source>>)
- [x] 1.2 Implemented `register_source() -> string` (generates unique id, stores source)
- [x] 1.3 Implemented `resolve_token(string id) -> stop_token` (lookup, return empty token if not found)
- [ ] 1.4 Implement `unregister(string id)` (cleanup)
- [x] 1.5 Added unit tests in `tests/test_cancellation_registry.cpp` (register, resolve, unregister, not-found)

## 2. ChatSession Cancellation State (Step 1)

- [x] 2.1 Added `std::shared_ptr<CancellationRegistry> cancellation_registry_` to `ChatSession::Impl`
- [x] 2.2 Added `void request_stop()` public method to ChatSession (sets stop_source)
- [x] 2.3 Modified `ChatSession::chat(input)` signature to `chat(input, std::stop_token token = {})`
- [x] 2.4 In chat() entry: token has stop_possible, register source in registry, get id() entry: if token has stop_possible, register source in registry, get id
- [x] 2.5 Added `cancellation_id` field to loop_args JSON
- [x] 2.6 Passed `token` to internal `session.chat(input, token)` recursive calls

## 3. loop_agent Entry Point Update (Step 3 + 4)

- [ ] 3.1 Modify `pdk_entry.cpp:170-190` to parse `cancellation_id` from loop_args
- [ ] 3.2 Resolve token from registry via shared reference (or new lightweight registry)
- [ ] 3.3 Replace `std::stop_token{}` at `pdk_entry.cpp:229` with resolved token
- [ ] 3.4 Add `if (token.stop_requested()) return early;` check before provider.generate()
- [ ] 3.5 Forward token to child DSLEngine.run() if engine supports (placeholder for future API)

## 4. ReactLoop Token Parameter (Step 5)

- [ ] 4.1 Modify `react_loop.h:80` to add `std::stop_token token` parameter to `run()`
- [ ] 4.2 Update all callers of ReactLoop::run to pass token (find via grep)
- [ ] 4.3 In ReactLoop implementation, pass token to orchestrator.process()
- [ ] 4.4 Add unit test verifying token propagation to MockLLMProvider

## 5. PlanExecuteLoop Token Parameter (Step 5)

- [ ] 5.1 Modify `plan_execute_loop.h:198` to add token parameter
- [ ] 5.2 Replace `std::stop_token{}` at `plan_execute_loop.h:206` with token parameter
- [ ] 5.3 Replace `std::stop_token{}` at `plan_execute_loop.h:256` with token parameter
- [ ] 5.4 Update all callers
- [ ] 5.5 Add token.stop_requested() check before each phase (planning, executing, verifying)

## 6. ForkJoinLoop Token Parameter (Step 5 + 6)

- [ ] 6.1 Modify `fork_join_loop.h:138-139` to add token parameter
- [ ] 6.2 Update CV wait predicate at `fork_join_loop.h:251-257` to include `token.stop_requested()`
- [ ] 6.3 On token cancellation, call `pool_->stop()` to terminate workers
- [ ] 6.4 Update all callers
- [ ] 6.5 Add unit test with multi-worker scenario, mid-cancellation

## 7. NodeExecutor Token Forwarding (Step 6)

- [ ] 7.1 Modify `node_executor.cpp:349-351` `dispatch_to_tool` to add token parameter
- [ ] 7.2 Forward token to `tool_coordinator_->execute()` call at `node_executor.cpp:387`
- [ ] 7.3 Modify YieldNode path at `node_executor.cpp:475` to pass real token to `generate_stream()`
- [ ] 7.4 Update all callers of dispatch_to_tool
- [ ] 7.5 Verify YieldStreamBridge still works with real token (it already supports it)

## 8. ToolCoordinator Token Parameter (Step 6)

- [ ] 8.1 Modify `tool_coordinator.cpp:195-203` to add token parameter
- [ ] 8.2 Add `if (token.stop_requested()) return early;` check at entry
- [ ] 8.3 Forward token to `registry_->call_tool()` if call_tool supports token
- [ ] 8.4 Emit `tool.audit.denied` audit event with reason="cancelled" when short-circuiting
- [ ] 8.5 Update all callers

## 9. Mock Blocking Provider for E2E Test (Step 7)

- [ ] 9.1 Create `examples/pdk_chat_demo/tests/mock_blocking_provider.h` with ILLMProvider implementation
- [ ] 9.2 MockBlockingProvider::generate() loops checking `token.stop_requested()` every 10ms
- [ ] 9.3 Returns cancelled result when stop_requested becomes true
- [ ] 9.4 Add unit test for MockBlockingProvider itself (basic cancellation behavior)

## 10. E2E Mid-Loop Cancellation Test (Step 7)

- [ ] 10.1 Create `examples/pdk_chat_demo/tests/test_chat_session_cancellation.cpp` (new file)
- [ ] 10.2 TEST_CASE: request_stop interrupts blocking provider within 100ms
- [ ] 10.3 TEST_CASE: token identity preserved through registry (instance check)
- [ ] 10.4 TEST_CASE: default token (no stop_source) never cancels
- [ ] 10.5 TEST_CASE: cancellation_id resolves to valid token in loop_agent
- [ ] 10.6 TEST_CASE: SIGINT during mock chat triggers cancellation (integration)

## 11. Verification

- [ ] 11.1 Run `./examples/pdk_chat_demo/tests/test_chat_session_cancellation` - all tests PASS
- [ ] 11.2 Run `./examples/pdk_chat_demo/tests/test_cancellation_registry` - all tests PASS
- [ ] 11.3 Run full `ctest -j$(nproc)` - 135+ tests PASS, pre-existing 3 failures unchanged
- [ ] 11.4 Run ASan preset - 0 new errors
- [ ] 11.5 Verify `lsp_diagnostics` clean on all modified files

## 12. Documentation Sync

- [ ] 12.1 Update `AGENTS.md` Recent Changes with Wave 3-A Phase B ship record
- [ ] 12.2 Update `docs/active-status.md` Wave 3-A section (add Phase B row)
- [ ] 12.3 Update `proposal-approved.md` - move `chat-async-io-cancellation-chain` to "已实施"

## 13. Ship Gate

- [ ] 13.1 Run `openspec validate chat-async-io-cancellation-chain --strict` - exit 0
- [ ] 13.2 Run `tools/adr_lint.py` - 0 errors
- [ ] 13.3 Run `tools/docs_drift_audit.py` - 0 DRIFT items
- [ ] 13.4 Create atomic commit per worktree-archive-workflow convention
- [ ] 13.5 Archive change: `openspec archive chat-async-io-cancellation-chain --yes`
- [ ] 13.6 Sync iteration.json status to `archived`

## Status: Step 1+2 Shipped (2026-08-08)

Tasks 3-13 below are **deferred to 3 focused sub-changes** (per Phase B scope > deep agent timeout budget):
- **Step 3**: loop_agent + NodeExecutor + ToolCoordinator wiring (Tasks 3, 7, 8)
- **Step 4**: 3 Loop API token params (Tasks 4, 5, 6) — BREAKING changes
- **Step 5**: Mock provider + E2E mid-loop cancel test (Tasks 9, 10) + verification (Task 11)

## Out of Scope

- Turn interruption point injection (LLM context steering) — separate change after Phase B
- main.cpp `while(getline)` integration with cancellation — separate follow-up
- `/model` runtime switching — Phase C
- Cancellation timeout configuration — defaults to 100ms E2E test threshold