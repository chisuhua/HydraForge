## 1. Mock Blocking Provider (Task 9)

- [ ] 1.1 Create `examples/pdk_chat_demo/tests/mock_blocking_provider.h` with `class MockBlockingProvider : public ILLMProvider`
- [ ] 1.2 Implement `generate(req, token)` with while-loop checking `token.stop_requested()` every 10ms
- [ ] 1.3 Return `ToolResult` with cancelled status when stop_requested becomes true
- [ ] 1.4 Return success result if never cancelled (with reasonable timeout for test cleanup)
- [ ] 1.5 Implement `generate_stream(req, token)` with same cancellation logic
- [ ] 1.6 Build to verify mock_blocking_provider compiles

## 2. E2E Mid-Loop Cancellation Test (Task 10)

- [ ] 2.1 Create `examples/pdk_chat_demo/tests/test_chat_session_cancellation.cpp` (new file)
- [ ] 2.2 TEST_CASE: request_stop interrupts blocking provider within 100ms
- [ ] 2.3 TEST_CASE: token identity preserved through registry (resolve_source returns same shared_ptr)
- [ ] 2.4 TEST_CASE: default token never cancels (no registration in registry)
- [ ] 2.5 TEST_CASE: cancellation_id field present in loop_args
- [ ] 2.6 TEST_CASE: token forwarded through loop_agent and observed by MockBlockingProvider
- [ ] 2.7 Build to verify test_chat_session_cancellation compiles

## 3. Test Registration

- [ ] 3.1 Update `examples/pdk_chat_demo/tests/CMakeLists.txt` to register test_chat_session_cancellation
- [ ] 3.2 Verify CMakeLists.txt glob picks up mock_blocking_provider.h (header-only, no separate target needed)

## 4. Full Verification

- [ ] 4.1 Build everything: `cmake --build . -j$(nproc)`
- [ ] 4.2 Run new tests: `ctest -R chat_session_cancellation --output-on-failure`
- [ ] 4.3 Run full ctest: `ctest -j$(nproc)`
- [ ] 4.4 Verify 143/146 tests PASS (141 + 5 new = 146, with 3 pre-existing failures)
- [ ] 4.5 Run ASan preset: `cmake --preset asan -DAGENTICDSL_BUILD_EXAMPLES=ON && ctest -R chat_session_cancellation`
- [ ] 4.6 Verify `lsp_diagnostics` clean on mock_blocking_provider.h and test_chat_session_cancellation.cpp

## 5. Documentation + Ship Gate

- [ ] 5.1 Update `AGENTS.md` with Wave 3-A Step 5 ship record
- [ ] 5.2 Update `docs/active-status.md` Step 5 row
- [ ] 5.3 Update `proposal-approved.md` — move change to "已实施"
- [ ] 5.4 Atomic commit per worktree-archive-workflow convention
- [ ] 5.5 `openspec archive cancellation-chain-step5-e2e --yes`
- [ ] 5.6 Sync iteration.json status

## 6. Phase B Completion

- [ ] 6.1 Phase B 7-step wiring 完整 ship（Step 1+2 + 3 + 4 + 5）
- [ ] 6.2 取消链接 8 处断开点全部修复 (#1-#8 per audit)
- [ ] 6.3 验收标准 (per chat-async-io-steering 提案):
  - [ ] steering 中断注入 + follow-up 排队接续 E2E 测试通过 (mock 模式) — Step 5
  - [ ] 长 turn 中 Ctrl+C/中断经 stop_token 正确清理（无泄漏，ASan 验证）— Step 5
  - [ ] ctest 全量零回归 — Step 5
- [ ] 6.4 Phase C `/model` 运行时切换 可立即启动（依赖 Step 5 ship）

## Out of Scope

- Phase C `/model` runtime switching — separate change
- main.cpp `while(getline)` integration with cancellation — separate follow-up
- Real LLM cancellation integration (already supported by ILLMProvider token-aware API)
- SIGINT integration in mock mode (covered by test_signal_shutdown)