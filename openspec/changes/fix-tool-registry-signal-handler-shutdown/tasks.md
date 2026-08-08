## 1. Test Scaffold (TDD - Write Failing Tests First)

- [x] 1.1 Add `examples/pdk_chat_demo/tests/test_signal_shutdown.cpp` with 2 TEST_CASE skeletons
- [x] 1.2 Implement subprocess helper (fork + exec pdk_chat_demo --mock, capture exit code + stderr)
- [x] 1.3 Implement TEST_CASE: `YAML validation failure exits cleanly without SIGSEGV`
- [x] 1.4 Implement TEST_CASE: `SIGTERM during interactive loop exits cleanly without SIGSEGV`
- [x] 1.5 Verify both tests FAIL on current main (SIGSEGV observed)

## 2. Atomic Flag Infrastructure

- [x] 2.1 Add `std::atomic<bool> g_shutdown_requested{false};` global in `examples/pdk_chat_demo/main.cpp` (before signal handler registration at line 147)
- [x] 2.2 Add header comment documenting async-signal-safe contract
- [x] 2.3 Verify `std::atomic<bool>` is included (already in `<atomic>` via standard headers)

## 3. Signal Handler Rewrite

- [x] 3.1 Replace `signal_handler` body at `main.cpp:71-79` with single atomic store: `g_shutdown_requested.store(true, std::memory_order_release);`
- [x] 3.2 Remove `unload_all_plugins(*g_loader)` call from signal handler
- [x] 3.3 Remove `std::exit(0)` call from signal handler
- [x] 3.4 Verify signal handler now contains ONLY the atomic store (zero other operations)
- [x] 3.5 Add `extern std::atomic<bool> g_shutdown_requested;` declaration comment for clarity

## 4. Main Loop Flag Observation

- [x] 4.1 Modify `while (std::getline(std::cin, input))` at `main.cpp:466` to check flag at loop top
- [x] 4.2 Break loop if `g_shutdown_requested.load(std::memory_order_acquire)` returns true
- [x] 4.3 Verify flag check happens after `session.chat()` returns (turn completes naturally)
- [x] 4.4 Verify `g_shutdown_requested` extern declared at file scope (or anonymous namespace)

## 5. Verification - YAML Validation Test PASSES

- [x] 5.1 Rebuild `pdk_chat_demo` with new signal handler
- [x] 5.2 Run `ctest -R test_signal_shutdown --output-on-failure`
- [x] 5.3 Confirm `YAML validation failure exits cleanly` TEST_CASE PASSES
- [x] 5.4 Verify exit code != 0 (validation error code), stderr contains "DSL Schema Validation FAILED"
- [x] 5.5 Verify NO SIGSEGV / Segmentation fault / ASan error in output

## 6. Verification - SIGINT Test PASSES

- [x] 6.1 Run `ctest -R test_signal_shutdown --output-on-failure`
- [x] 6.2 Confirm `SIGTERM during interactive loop exits cleanly` TEST_CASE PASSES
- [x] 6.3 Verify exit code == 0, stderr contains graceful shutdown message
- [x] 6.4 Verify NO SIGSEGV / Segmentation fault / ASan error in output

## 7. Regression Validation

- [x] 7.1 Run full `ctest -j$(nproc)` and confirm pre-existing 5 failures unchanged
- [x] 7.2 Run ASan build: `cmake --preset asan -DAGENTICDSL_BUILD_TESTS=ON && ctest`
- [x] 7.3 Verify 0 new ASan errors
- [x] 7.4 Run existing `test_pdk_chat_demo_*` tests and confirm no regression
- [x] 7.5 Run `StartupCleanupGuard` covered paths (validation failure already tested) - confirm no regression

## 8. Documentation Sync

- [x] 8.1 Update `AGENTS.md` Recent Changes section with this ship record (date 2026-08-08)
- [x] 8.2 Update `docs/active-status.md` with change ship status
- [x] 8.3 Add inline comment in `main.cpp:71-79` referencing the audit document for future maintainers
- [x] 8.4 Verify `proposal-approved.md` is updated when change ships (move to "已实施" section)

## 9. Ship Gate

- [x] 9.1 Run `openspec validate fix-tool-registry-signal-handler-shutdown --strict` and confirm exit 0
- [x] 9.2 Run `tools/adr_lint.py` and confirm 0 errors
- [x] 9.3 Run `tools/docs_drift_audit.py` and confirm 0 DRIFT items
- [x] 9.4 Create atomic commits following existing project convention
- [x] 9.5 Archive change via `openspec archive fix-tool-registry-signal-handler-shutdown --yes`