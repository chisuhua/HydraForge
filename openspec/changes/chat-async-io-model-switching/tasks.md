## 1. ChatSession next_model_ state

- [x] 1.1 Add `std::atomic<std::string> next_model_{""}` to `ChatSession::Impl`
- [x] 1.2 Add public `void request_model_switch(const std::string& provider_name)` declaration
- [x] 1.3 Add public `std::string next_model() const` declaration
- [x] 1.4 Implement `request_model_switch` in chat_session.cpp:
  - Validate provider (mock-mode check + provider-dynamic-discovery route)
  - Store in `next_model_`
  - Log success / rejection

## 2. Per-turn model switch in chat()

- [x] 2.1 Modify `ChatSession::chat()` to check `next_model_` at entry
- [x] 2.2 If non-empty + live mode + different from current, swap provider via LLMProviderFactory
- [x] 2.3 Clear `next_model_` after swap
- [x] 2.4 Log provider swap to stderr

## 3. /model DECLARE_COMMAND

- [x] 3.1 Create `examples/pdk_chat_demo/commands/model_command.cpp/.h`
- [x] 3.2 Implement DECLARE_COMMAND pattern (matches chat-slash-commands-migration)
- [x] 3.3 Parse argument: `/model <name>` → provider_name
- [x] 3.4 Call `ChatSession::request_model_switch(provider_name)` via command globals
- [x] 3.5 Handle empty arg → return usage error
- [x] 3.6 Handle invalid provider → return error

## 4. Command Registration

- [x] 4.1 Update `examples/pdk_chat_demo/main.cpp` to register `model_command`
- [x] 4.2 Update `examples/pdk_chat_demo/CMakeLists.txt` to compile model_command
- [x] 4.3 Verify command appears in `/help` output

## 5. Session JSONL Persistence

- [x] 5.1 Update `ChatSession::save_to_disk()` to write `next_model_` to session_meta
- [x] 5.2 Update `ChatSession::load_from_disk()` to restore `next_model_` from session_meta
- [x] 5.3 Test that persistence round-trip works

## 6. Test Scaffold (TDD 5 steps)

- [x] 6.1 Create `examples/pdk_chat_demo/tests/test_model_switching.cpp`
- [x] 6.2 TEST_CASE: /model deepseek success (mock mode accepts mock)
- [x] 6.3 TEST_CASE: /model openai rejected in mock mode
- [x] 6.4 TEST_CASE: next_model_ persisted to session_meta
- [x] 6.5 TEST_CASE: per-turn swap doesn't interrupt current turn (next_model_ cleared after swap)
- [x] 6.6 Register test_model_switching in CMakeLists.txt

## 7. Verification

- [x] 7.1 Run new tests `test_model_switching` — 4 tests PASS
- [x] 7.2 Run full `ctest -j$(nproc)` — 147/150 PASS (3 pre-existing unchanged)
- [x] 7.3 Run ASan preset — 0 new errors
- [x] 7.4 Manual: start demo --mock, type `/model mock` → success; `/model openai` → rejection

## 8. Documentation + Ship Gate

- [x] 8.1 Update `AGENTS.md` with Wave 3-A Phase C ship record
- [x] 8.2 Update `docs/active-status.md` Phase C row
- [x] 8.3 Update `proposal-approved.md` — move change to "已实施"
- [x] 8.4 Atomic commit per worktree-archive-workflow convention
- [x] 8.5 `openspec archive chat-async-io-model-switching --yes`
- [x] 8.6 Sync iteration.json status

## 9. Wave 3-A Completion

- [x] 9.1 Wave 3-A 完整 ship (Phase 0 + A + B 5 steps + C)
- [x] 9.2 4-phase chat-async-io-steering 提案完整 ship
- [x] 9.3 chat-async-io-steering 提案状态翻转: 🔍 Proposed → ✅ Implemented
- [x] 9.4 解锁后续: Phase 6 评估 (ADR-0050)

## Out of Scope

- thinking_level dynamic switching (provider feature dependent, deferred)
- Provider auto-selection (benchmark-based)
- Real-time provider swap (mid-turn, requires thread-safe swap)
- Wave 3-B features (chaos testing, etc.)