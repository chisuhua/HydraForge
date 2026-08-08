## 1. Test Scaffold (TDD - Write Failing Tests First)

- [ ] 1.1 Create `examples/pdk_chat_demo/tests/test_chat_session_queues.cpp` with 4 TEST_CASE skeletons
- [ ] 1.2 Implement `TEST_CASE: steering_queue_ enqueue and queue_size reflect count` (基础 enqueue)
- [ ] 1.3 Implement `TEST_CASE: follow_up_queue_ enqueue and queue_size reflect count` (基础 enqueue)
- [ ] 1.4 Implement `TEST_CASE: steering_queue_ rejects overflow at capacity 32` (overflow 语义)
- [ ] 1.5 Implement `TEST_CASE: try_clear_queue returns count cleared` (clear API)
- [ ] 1.6 Verify all 4 tests FAIL on current main (no queue exists)

## 2. QueueKind Enum + Public API

- [ ] 2.1 Add `enum class QueueKind { Steering, FollowUp }` to `examples/pdk_chat_demo/chat_session.h` (after ChatSession class declaration)
- [ ] 2.2 Add `size_t queue_size(QueueKind kind) const` declaration to ChatSession public API
- [ ] 2.3 Add `size_t try_clear_queue(QueueKind kind)` declaration to ChatSession public API
- [ ] 2.4 Verify tests FAIL with clear compile errors (missing members)

## 3. Queue Data Members in ChatSession::Impl

- [ ] 3.1 Add `kDefaultQueueCapacity = 32` constexpr to `chat_session.cpp` anonymous namespace
- [ ] 3.2 Add `std::queue<std::string> steering_queue_` to `ChatSession::Impl`
- [ ] 3.3 Add `std::queue<std::string> follow_up_queue_` to `ChatSession::Impl`
- [ ] 3.4 Add `mutable std::mutex steering_mutex_` to `ChatSession::Impl` (mutable for const queue_size)
- [ ] 3.5 Add `mutable std::mutex follow_up_mutex_` to `ChatSession::Impl`
- [ ] 3.6 Add `size_t capacity_` to `ChatSession::Impl` (configurable)
- [ ] 3.7 Update Impl constructor to initialize `capacity_(kDefaultQueueCapacity)` + accept capacity override

## 4. queue_size Implementation

- [ ] 4.1 Implement `size_t ChatSession::queue_size(QueueKind kind) const` (lock-protected, O(1))
- [ ] 4.2 Build and verify test 1.2 (`steering_queue_ enqueue`) and 1.3 (`follow_up_queue_ enqueue`) PASS
- [ ] 4.3 Verify test 1.4 (`overflow`) still FAILS (no overflow logic yet)

## 5. Overflow Rejection Logic

- [ ] 5.1 Add `bool try_push_steering(std::string)` private method to Impl (returns true on success)
- [ ] 5.2 Add `bool try_push_follow_up(std::string)` private method to Impl
- [ ] 5.3 Implement overflow check: if `queue.size() >= capacity_`, log warning (length only) + return false
- [ ] 5.4 Implement successful push: lock mutex + push + unlock
- [ ] 5.5 Build and verify test 1.4 (`overflow`) PASSES

## 6. try_clear_queue Implementation

- [ ] 6.1 Implement `size_t ChatSession::try_clear_queue(QueueKind kind)` (lock-protected, swap to empty, return old size)
- [ ] 6.2 Build and verify test 1.5 (`try_clear_queue returns count`) PASSES
- [ ] 6.3 Run all 4 tests + verify all PASS

## 7. Input Thread (Async Producer)

- [ ] 7.1 Add `std::thread input_thread_` member to `ChatSession::Impl`
- [ ] 7.2 Add `std::atomic<bool> stop_input_thread_{false}` for thread shutdown signal
- [ ] 7.3 Implement `void input_thread_main()` private method (loop with stop_token check)
- [ ] 7.4 Implement input classification: lines starting with `/` → steering, others → follow-up
- [ ] 7.5 Start input thread in Impl constructor
- [ ] 7.6 Join input thread in Impl destructor (signal stop + join)
- [ ] 7.7 Add test `TEST_CASE: input thread joins on destruction` (verify no hang)

## 8. Verification

- [ ] 8.1 Run `./examples/pdk_chat_demo/tests/test_chat_session_queues` - all 4 tests PASS
- [ ] 8.2 Run full `ctest -j$(nproc)` - pre-existing 3 failures unchanged, no new failures
- [ ] 8.3 Run ASan preset - 0 new errors
- [ ] 8.4 Verify `lsp_diagnostics` clean on `chat_session.h` and `chat_session.cpp`

## 9. Documentation Sync

- [ ] 9.1 Update `AGENTS.md` Recent Changes with Wave 3-A Phase A ship record
- [ ] 9.2 Update `docs/active-status.md` Wave 3-A section (add Phase A row)
- [ ] 9.3 Update `proposal-approved.md` - move `chat-async-io-queue-infra` from "已批准提案" to "已实施"

## 10. Ship Gate

- [ ] 10.1 Run `openspec validate chat-async-io-queue-infra --strict` - exit 0
- [ ] 10.2 Run `tools/adr_lint.py` - 0 errors
- [ ] 10.3 Run `tools/docs_drift_audit.py` - 0 DRIFT items
- [ ] 10.4 Create atomic commit per worktree-archive-workflow convention
- [ ] 10.5 Archive change: `openspec archive chat-async-io-queue-infra --yes`
- [ ] 10.6 Sync iteration.json status to `archived`