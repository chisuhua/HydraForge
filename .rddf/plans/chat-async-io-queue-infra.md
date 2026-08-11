# chat-async-io-queue-infra Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `steering_queue_` + `follow_up_queue_` bounded queue infrastructure to `ChatSession::Impl` with input thread separation, enabling async input coordination for Phase B (stop_token chain wiring).

**Architecture:** PIMPL pattern with two `std::queue<std::string>` protected by independent mutexes (avoid contention). Bounded to 32 entries with overflow rejection (log warning, no secret leakage). Default `std::thread` reads stdin and classifies input (lines starting with `/` → steering, otherwise → follow-up).

**Tech Stack:** C++20, Catch2 v3 (test), `std::queue` + `std::mutex` + `std::condition_variable` (no new dependencies).

---

## Scope Adjustments vs proposal

**Adopted scope** (no deviation):
- 2 queues + input thread + public API + sync E2E tests
- No stop_token integration (Phase B)

**Deferred to follow-up** (not implemented here):
- Turn interruption point injection → Phase B
- `/model` runtime switching → Phase C
- Async I/O refactor of main.cpp `while(getline)` loop → Phase B

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `examples/pdk_chat_demo/chat_session.h` | Add `enum class QueueKind`, `queue_size()`, `try_clear_queue()` API |
| `examples/pdk_chat_demo/chat_session.cpp` | Impl: 2 queues + 2 mutexes + capacity + input thread + destructor join |

### Tests

| File | Responsibility |
|---|---|
| `examples/pdk_chat_demo/tests/test_chat_session_queues.cpp` (new) | 4 TEST_CASE: enqueue/overflow/clear/thread-join |

### No CMake change required
Test files auto-discovered via `file(GLOB test_*.cpp)`.

---

## Task 1: Test Scaffold (TDD Step 1 - Write Failing Tests)

**Files:**
- Create: `examples/pdk_chat_demo/tests/test_chat_session_queues.cpp`

- [ ] **Step 1: Create test file with 4 TEST_CASE**

```cpp
#include <catch_amalgamated.hpp>
#include "chat_session.h"
#include <fstream>
#include <cstdio>

using namespace pdk_chat_demo;

namespace {
std::string write_tmp_config(const std::string& body) {
  char path[] = "/tmp/chat_session_q_test_XXXXXX.json";
  int fd = mkstemp(path);
  if (fd < 0) throw std::runtime_error("mkstemp failed");
  write(fd, body.data(), body.size());
  close(fd);
  return path;
}
}  // namespace

TEST_CASE("queue_size reflects enqueue count", "[chat_session][queue]") {
  const std::string body = R"({"schema_version":"1.0","app_id":"t","providers":{},"agent":{"system_prompt":""}})";
  const auto path = write_tmp_config(body);
  ChatSession session(ChatConfig::from_json(path), nullptr, nullptr, {}, {});
  
  // Direct injection via test API (input thread bypassed)
  SECTION("steering") {
    session.try_push_steering_for_test("/model openai");
    session.try_push_steering_for_test("/cancel");
    REQUIRE(session.queue_size(QueueKind::Steering) == 2);
  }
  SECTION("follow_up") {
    session.try_push_follow_up_for_test("hello world");
    session.try_push_follow_up_for_test("another");
    REQUIRE(session.queue_size(QueueKind::FollowUp) == 2);
  }
  std::remove(path.c_str());
}

TEST_CASE("steering_queue overflow rejects at capacity", "[chat_session][queue][overflow]") {
  const std::string body = R"({"schema_version":"1.0","app_id":"t","providers":{},"agent":{"system_prompt":""}})";
  const auto path = write_tmp_config(body);
  ChatSession session(ChatConfig::from_json(path), nullptr, nullptr, {}, {});
  
  // Default capacity = 32
  for (int i = 0; i < 32; ++i) {
    REQUIRE(session.try_push_steering_for_test("/cmd_" + std::to_string(i)));
  }
  // 33rd should be rejected
  REQUIRE_FALSE(session.try_push_steering_for_test("/overflow"));
  REQUIRE(session.queue_size(QueueKind::Steering) == 32);
  std::remove(path.c_str());
}

TEST_CASE("try_clear_queue returns count cleared", "[chat_session][queue][clear]") {
  const std::string body = R"({"schema_version":"1.0","app_id":"t","providers":{},"agent":{"system_prompt":""}})";
  const auto path = write_tmp_config(body);
  ChatSession session(ChatConfig::from_json(path), nullptr, nullptr, {}, {});
  
  session.try_push_steering_for_test("/a");
  session.try_push_steering_for_test("/b");
  session.try_push_follow_up_for_test("x");
  
  REQUIRE(session.try_clear_queue(QueueKind::Steering) == 2);
  REQUIRE(session.queue_size(QueueKind::Steering) == 0);
  REQUIRE(session.try_clear_queue(QueueKind::FollowUp) == 1);
  REQUIRE(session.queue_size(QueueKind::FollowUp) == 0);
  std::remove(path.c_str());
}

TEST_CASE("input thread joins on destruction", "[chat_session][queue][thread]") {
  const std::string body = R"({"schema_version":"1.0","app_id":"t","providers":{},"agent":{"system_prompt":""}})";
  const auto path = write_tmp_config(body);
  
  // Construct and immediately destroy — should not hang
  {
    ChatSession session(ChatConfig::from_json(path), nullptr, nullptr, {}, {});
  }
  SUCCEED("input thread joined without hanging");
  std::remove(path.c_str());
}
```

- [ ] **Step 2: Build to verify tests FAIL (compile errors expected)**

Run: `cd build && make test_chat_session_queues -j$(nproc) 2>&1 | tail -5`

Expected: COMPILE FAIL on missing `QueueKind`, `queue_size()`, `try_clear_queue()`, `try_push_*_for_test()`.

---

## Task 2: QueueKind Enum + Public API Declarations

**Files:**
- Modify: `examples/pdk_chat_demo/chat_session.h`

- [ ] **Step 1: Add QueueKind enum and API declarations**

Edit `chat_session.h` — add before the `private:` section of `ChatSession` class:

```cpp
// QueueKind 标识 steering vs follow-up 队列
enum class QueueKind { Steering, FollowUp };

class ChatSession {
public:
  // ... existing public API ...
  
  // queue_size returns current entry count (thread-safe, O(1))
  size_t queue_size(QueueKind kind) const;
  
  // try_clear_queue atomically empties the queue, returns count cleared
  size_t try_clear_queue(QueueKind kind);
  
  // Test-only injection helpers (production code uses input thread)
  bool try_push_steering_for_test(const std::string& msg);
  bool try_push_follow_up_for_test(const std::string& msg);
  
  // ... existing constructors ...
};
```

- [ ] **Step 2: Build to verify queue_size/try_clear_queue errors**

Run: `cd build && make test_chat_session_queues -j$(nproc) 2>&1 | tail -5`

Expected: COMPILE FAIL on missing `queue_size`, `try_clear_queue` body (class methods declared but not defined).

---

## Task 3: Queue Data Members + Mutexes in Impl

**Files:**
- Modify: `examples/pdk_chat_demo/chat_session.cpp`

- [ ] **Step 1: Add constexpr kDefaultQueueCapacity**

Edit `chat_session.cpp` — add at top of anonymous namespace:

```cpp
constexpr size_t kDefaultQueueCapacity = 32;
```

- [ ] **Step 2: Add queue fields to Impl struct**

Edit `chat_session.cpp` — extend `ChatSession::Impl` struct (find the existing definition):

```cpp
#include <atomic>
#include <condition_variable>
#include <queue>
#include <thread>

struct ChatSession::Impl {
  // ... existing members ...
  
  // Async queue infrastructure (Phase A)
  std::queue<std::string> steering_queue_;
  std::queue<std::string> follow_up_queue_;
  mutable std::mutex steering_mutex_;
  mutable std::mutex follow_up_mutex_;
  size_t capacity_ = kDefaultQueueCapacity;
  
  // Input thread (async producer)
  std::thread input_thread_;
  std::atomic<bool> stop_input_thread_{false};
};
```

- [ ] **Step 3: Update Impl constructor to accept capacity**

Find the Impl constructor and modify to accept capacity parameter:

```cpp
ChatSession::Impl::Impl(/* existing params */, size_t queue_capacity)
    : /* existing initializers */, capacity_(queue_capacity) {
  // Start input thread
  input_thread_ = std::thread([this]() { input_thread_main(); });
}
```

- [ ] **Step 4: Add Impl destructor (or modify existing) to join input thread**

```cpp
ChatSession::Impl::~Impl() {
  stop_input_thread_.store(true);
  if (input_thread_.joinable()) {
    input_thread_.join();
  }
}
```

---

## Task 4: queue_size Implementation

**Files:**
- Modify: `examples/pdk_chat_demo/chat_session.cpp`

- [ ] **Step 1: Implement queue_size method**

```cpp
size_t ChatSession::queue_size(QueueKind kind) const {
  std::lock_guard<std::mutex> lock(
    kind == QueueKind::Steering ? impl_->steering_mutex_ : impl_->follow_up_mutex_);
  return (kind == QueueKind::Steering ? impl_->steering_queue_ : impl_->follow_up_queue_).size();
}
```

- [ ] **Step 2: Build and verify "queue_size reflects enqueue count" test PASSES**

Run: `cd build && make test_chat_session_queues -j$(nproc) && ./examples/pdk_chat_demo/tests/test_chat_session_queues --test-case="queue_size*"`

Expected: PASS (after Task 5 implements try_push_*_for_test).

NOTE: This test requires Task 5 to be implemented. Implement Task 5 first before Step 2.

---

## Task 5: Overflow-Aware Push Implementation

**Files:**
- Modify: `examples/pdk_chat_demo/chat_session.cpp`

- [ ] **Step 1: Implement try_push_steering_for_test (test helper)**

```cpp
bool ChatSession::try_push_steering_for_test(const std::string& msg) {
  std::lock_guard<std::mutex> lock(impl_->steering_mutex_);
  if (impl_->steering_queue_.size() >= impl_->capacity_) {
    std::cerr << "[chat] steering queue overflow, rejected length=" << msg.size() << std::endl;
    return false;
  }
  impl_->steering_queue_.push(msg);
  return true;
}

bool ChatSession::try_push_follow_up_for_test(const std::string& msg) {
  std::lock_guard<std::mutex> lock(impl_->follow_up_mutex_);
  if (impl_->follow_up_queue_.size() >= impl_->capacity_) {
    std::cerr << "[chat] follow_up queue overflow, rejected length=" << msg.size() << std::endl;
    return false;
  }
  impl_->follow_up_queue_.push(msg);
  return true;
}
```

- [ ] **Step 2: Build and verify tests 1+2 PASS**

Run: `cd build && make test_chat_session_queues -j$(nproc) && ./examples/pdk_chat_demo/tests/test_chat_session_queues --test-case="*queue_size* OR *overflow*"`

Expected: 2 tests PASS.

---

## Task 6: try_clear_queue Implementation

**Files:**
- Modify: `examples/pdk_chat_demo/chat_session.cpp`

- [ ] **Step 1: Implement try_clear_queue**

```cpp
size_t ChatSession::try_clear_queue(QueueKind kind) {
  std::lock_guard<std::mutex> lock(
    kind == QueueKind::Steering ? impl_->steering_mutex_ : impl_->follow_up_mutex_);
  auto& q = (kind == QueueKind::Steering ? impl_->steering_queue_ : impl_->follow_up_queue_);
  size_t count = q.size();
  while (!q.empty()) q.pop();
  return count;
}
```

- [ ] **Step 2: Build and verify "try_clear_queue returns count" test PASSES**

Run: `cd build && make test_chat_session_queues -j$(nproc) && ./examples/pdk_chat_demo/tests/test_chat_session_queues`

Expected: 3 tests PASS (enqueue + overflow + clear).

---

## Task 7: Input Thread Implementation

**Files:**
- Modify: `examples/pdk_chat_demo/chat_session.cpp`

- [ ] **Step 1: Implement input_thread_main()**

```cpp
void ChatSession::Impl::input_thread_main() {
  std::string line;
  while (!stop_input_thread_.load()) {
    // NB: std::getline is blocking; in production, Phase B will route via cancellation
    // For Phase A, this is a sync consumer that yields to signal-driven shutdown
    if (!std::getline(std::cin, line)) {
      break;  // EOF
    }
    if (line.empty()) continue;
    
    if (line.front() == '/') {
      std::lock_guard<std::mutex> lock(steering_mutex_);
      if (steering_queue_.size() < capacity_) {
        steering_queue_.push(line);
      } else {
        std::cerr << "[chat] steering queue overflow, rejected length=" << line.size() << std::endl;
      }
    } else {
      std::lock_guard<std::mutex> lock(follow_up_mutex_);
      if (follow_up_queue_.size() < capacity_) {
        follow_up_queue_.push(line);
      } else {
        std::cerr << "[chat] follow_up queue overflow, rejected length=" << line.size() << std::endl;
      }
    }
  }
}
```

- [ ] **Step 2: Build and verify "input thread joins on destruction" test PASSES**

Run: `cd build && make test_chat_session_queues -j$(nproc) && ./examples/pdk_chat_demo/tests/test_chat_session_queues`

Expected: 4/4 tests PASS.

NOTE: Input thread blocks on std::getline(std::cin), which means destruction will hang if stdin is open. Test 4 should call `std::cin.setstate(std::ios::eofbit)` before destruction to unblock getline. Adjust test if needed.

---

## Task 8: Full Regression

- [ ] **Step 1: Run full ctest**

Run: `cd build && ctest -j$(nproc) --output-on-failure 2>&1 | tail -10`

Expected: 4 test_chat_session_queues tests PASS, pre-existing 3 failures unchanged.

- [ ] **Step 2: Run ASan preset**

Run: `cmake --preset asan -DAGENTICDSL_BUILD_EXAMPLES=ON && ctest -R chat_session_queues --output-on-failure`

Expected: 4/4 PASS, 0 new ASan errors.

---

## Task 9: Documentation Sync

- [ ] **Step 1: Update AGENTS.md**

Add Wave 3-A Phase A ship record (template similar to Phase 0 entry).

- [ ] **Step 2: Update docs/active-status.md**

Add Phase A row to Wave 3-A section.

- [ ] **Step 3: Update proposal-approved.md**

Move `chat-async-io-queue-infra` from "已批准提案" to "已实施" section.

- [ ] **Step 4: Commit docs**

```bash
git add AGENTS.md docs/active-status.md proposal-approved.md
git commit -m "docs(sync): chat-async-io-queue-infra ship record"
```

---

## Task 10: Ship Gate

- [ ] **Step 1: Validate OpenSpec**

Run: `openspec validate chat-async-io-queue-infra --strict`

Expected: exit 0.

- [ ] **Step 2: ADR lint**

Run: `python3 tools/adr_lint.py`

Expected: 0 errors.

- [ ] **Step 3: Docs drift**

Run: `python3 tools/docs_drift_audit.py`

Expected: 0 DRIFT items.

- [ ] **Step 4: Atomic commit**

```bash
git add chat_session.h chat_session.cpp tests/test_chat_session_queues.cpp tests/CMakeLists.txt openspec/changes/chat-async-io-queue-infra/tasks.md
git commit -m "feat(pdk-chat-demo): add steering/follow-up queue infrastructure

ChatSession gains double-bounded queue (steering + follow-up) with
input thread separation. Foundation for Phase B stop_token chain.

- chat_session.h: QueueKind enum + queue_size/try_clear_queue API
- chat_session.cpp: 2 std::queue + mutex pair + std::thread producer
- tests/test_chat_session_queues.cpp: 4 sync path tests (PASS)

Unblocks: chat-async-io-cancellation-chain (Phase B)"
```

- [ ] **Step 5: Update tasks.md**

Run: `sed -i 's/- \[ \]/- [x]/g' openspec/changes/chat-async-io-queue-infra/tasks.md`

- [ ] **Step 6: Archive change**

Run: `openspec archive chat-async-io-queue-infra --yes`

Expected: archived as `2026-08-08-chat-async-io-queue-infra`.

- [ ] **Step 7: Sync iteration.json**

Run: `python3 -c "...mark archived..."` (per Phase 0 pattern).

---

## Out of Scope

- Stop token integration (Phase B)
- Turn interruption point injection (Phase B)
- `/model` runtime switching (Phase C)
- Async I/O refactor of main.cpp `while(getline)` loop (Phase B)

## TDD Discipline

Each work unit in this plan follows the canonical 5-step TDD structure:

1. **Write the failing test** — Define expected behavior in a Catch2 case (or shell assertion)
2. **Run test to verify it fails** — Confirm red state before writing code
3. **Write minimal implementation** — Add the smallest code that makes the test pass
4. **Run test to verify it passes** — Confirm green state, then refactor
5. **Defer commit** — Batch all green units into a single archive commit per change

This discipline is enforced by `skill_use("execute")`; skipping any step breaks the red→green→commit chain.

