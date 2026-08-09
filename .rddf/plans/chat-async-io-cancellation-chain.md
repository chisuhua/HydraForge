# chat-async-io-cancellation-chain Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire 7-step end-to-end `std::stop_token` cancellation chain from `ChatSession` through `loop_agent` to `NodeExecutor`/`ToolCoordinator`, enabling Agent turn interruption via `request_stop()`.

**Architecture:** ChatSession owns `std::stop_source` + cancellation registry (string id → shared stop_source). `chat()` accepts `std::stop_token`, registers handle, propagates `cancellation_id` through `loop/run` args. loop_agent entry resolves id, forwards real token (replacing `std::stop_token{}` at `pdk_entry.cpp:229`). 3 loop APIs gain `std::stop_token` param (BREAKING). NodeExecutor + ToolCoordinator forward token. E2E test uses MockBlockingProvider to verify 100ms cancellation.

**Tech Stack:** C++20, Catch2 v3, `<stop_token>` (C++20 standard), `<unordered_map>`, `<memory>` (shared_ptr).

---

## Scope Adjustments vs proposal

**Adopted scope** (no deviation):
- 7 步 wiring + cancellation registry + BREAKING 3 loop APIs + E2E test

**Deferred to follow-up** (not implemented here):
- Turn interruption point injection (LLM context) — separate change after Phase B
- main.cpp `while(getline)` integration — separate follow-up
- `/model` runtime switching — Phase C

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `examples/pdk_chat_demo/cancellation_registry.h` (new) | `class CancellationRegistry` (id → stop_source map) |
| `examples/pdk_chat_demo/chat_session.h` | Add `request_stop()` + modify `chat()` signature |
| `examples/pdk_chat_demo/chat_session.cpp` | Add Impl cancellation state + registry integration |
| `pdk/loop_agent/src/pdk_entry.cpp` | Parse `cancellation_id` + forward token (replace `std::stop_token{}`) |
| `include/agenticdsl/pdk/agent_loops/react_loop.h` | Add `std::stop_token` param to `run()` |
| `include/agenticdsl/pdk/agent_loops/plan_execute_loop.h` | Add `std::stop_token` param + replace internal `{}` |
| `include/agenticdsl/pdk/agent_loops/fork_join_loop.h` | Add `std::stop_token` param + CV wait predicate |
| `src/modules/executor/node_executor.cpp` | Forward token in `dispatch_to_tool` + YieldNode |
| `src/common/tools/tool_coordinator.cpp` | Add token param + short-circuit on cancellation |

### Tests

| File | Responsibility |
|---|---|
| `examples/pdk_chat_demo/tests/test_cancellation_registry.cpp` (new) | Registry unit tests |
| `examples/pdk_chat_demo/tests/mock_blocking_provider.h` (new) | Test helper: blocking provider that observes token |
| `examples/pdk_chat_demo/tests/test_chat_session_cancellation.cpp` (new) | E2E mid-loop cancellation |

---

## Task 1: Cancellation Registry Infrastructure

**Files:**
- Create: `examples/pdk_chat_demo/cancellation_registry.h`

- [ ] **Step 1: Create header with CancellationRegistry class**

```cpp
#pragma once
#include <memory>
#include <stop_token>
#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>

class CancellationRegistry {
 public:
  // Register a new stop_source, return unique id (timestamp_ms + counter)
  std::string register_source(std::shared_ptr<std::stop_source> source);
  
  // Resolve id to stop_token. Returns empty token if not found.
  std::stop_token resolve_token(const std::string& id);
  
  // Remove id from registry (called by ChatSession destructor or after chat() returns).
  void unregister(const std::string& id);
  
 private:
  std::mutex mutex_;
  std::unordered_map<std::string, std::shared_ptr<std::stop_source>> sources_;
  std::atomic<uint64_t> counter_{0};
};
```

- [ ] **Step 2: Create test file with unit tests**

```cpp
TEST_CASE("CancellationRegistry register/resolve round-trip", "[cancel_registry]") {
  CancellationRegistry reg;
  auto source = std::make_shared<std::stop_source>();
  std::string id = reg.register_source(source);
  REQUIRE_FALSE(id.empty());
  
  auto token = reg.resolve_token(id);
  REQUIRE_TRUE(token.stop_possible());
  REQUIRE_FALSE(token.stop_requested());
  
  source->request_stop();
  REQUIRE_TRUE(token.stop_requested());
}

TEST_CASE("CancellationRegistry resolve unknown id returns empty", "[cancel_registry]") {
  CancellationRegistry reg;
  auto token = reg.resolve_token("nonexistent");
  REQUIRE_FALSE(token.stop_possible());
}

TEST_CASE("CancellationRegistry unregister removes entry", "[cancel_registry]") {
  CancellationRegistry reg;
  auto source = std::make_shared<std::stop_source>();
  std::string id = reg.register_source(source);
  reg.unregister(id);
  REQUIRE_FALSE(reg.resolve_token(id).stop_possible());
}
```

- [ ] **Step 3: Build to verify tests compile + FAIL (no impl yet)**

Run: `cd build && make test_cancellation_registry -j$(nproc) 2>&1 | tail -5`

Expected: LINK FAIL (unresolved symbols).

- [ ] **Step 4: Implement CancellationRegistry methods in cancellation_registry.cpp**

[Implementation: register_source uses timestamp_ms from std::chrono + counter for uniqueness; resolve_token returns std::stop_token{} if not found.]

- [ ] **Step 5: Verify tests PASS**

Run: `cd build && make test_cancellation_registry -j$(nproc) && ./examples/pdk_chat_demo/tests/test_cancellation_registry`

Expected: 3/3 tests PASS.

---

## Task 2: ChatSession Cancellation State (Step 1)

**Files:**
- Modify: `examples/pdk_chat_demo/chat_session.h`
- Modify: `examples/pdk_chat_demo/chat_session.cpp`

- [ ] **Step 1: Add cancellation includes to chat_session.h**

```cpp
#include <memory>
#include "cancellation_registry.h"
```

- [ ] **Step 2: Add public `request_stop()` method declaration**

```cpp
class ChatSession {
public:
  // Request cancellation of any in-flight chat operation.
  void request_stop();
  
  // Existing API: now with optional stop_token
  ChatResult chat(const std::string& user_input,
                  std::stop_token token = {});
  
  // ... existing API ...
};
```

- [ ] **Step 3: Add Impl cancellation state**

```cpp
struct ChatSession::Impl {
  // ... existing members ...
  std::shared_ptr<CancellationRegistry> cancellation_registry_;
  std::string current_cancellation_id_;
  std::shared_ptr<std::stop_source> current_stop_source_;
};
```

- [ ] **Step 4: Initialize registry in Impl constructor**

```cpp
ChatSession::Impl::Impl(/* existing params */)
    : /* existing initializers */,
      cancellation_registry_(std::make_shared<CancellationRegistry>()) {
  // existing init code...
}
```

- [ ] **Step 5: Implement request_stop()**

```cpp
void ChatSession::request_stop() {
  if (impl_->current_stop_source_) {
    impl_->current_stop_source_->request_stop();
  }
}
```

- [ ] **Step 6: Modify chat() to accept and register token**

```cpp
ChatResult ChatSession::chat(const std::string& user_input, std::stop_token token) {
  std::string cancellation_id;
  if (token.stop_possible()) {
    auto source = std::make_shared<std::stop_source>(token);
    cancellation_id = impl_->cancellation_registry_->register_source(source);
    impl_->current_stop_source_ = source;
    impl_->current_cancellation_id_ = cancellation_id;
  }
  
  // ... build loop_args including cancellation_id ...
  nlohmann::json loop_args = {
    {"loop_type", "react"},
    {"prompt", user_input},
    {"cancellation_id", cancellation_id},
    // ... existing fields ...
  };
  
  auto result = impl_->registry->call_tool("loop/run", loop_args);
  
  // Cleanup
  if (!cancellation_id.empty()) {
    impl_->cancellation_registry_->unregister(cancellation_id);
    impl_->current_stop_source_.reset();
    impl_->current_cancellation_id_.clear();
  }
  return result_to_chat_result(result);
}
```

- [ ] **Step 7: Build + test (verify no regression in Phase A tests)**

Run: `cd build && make pdk_chat_demo test_chat_session_queues test_chat_session -j$(nproc) && ctest -R "chat_session" --output-on-failure`

Expected: All Phase A tests still PASS, no regression.

---

## Task 3: loop_agent Entry Point Update (Step 3 + 4)

**Files:**
- Modify: `pdk/loop_agent/src/pdk_entry.cpp`

- [ ] **Step 1: Parse cancellation_id from loop_args**

Modify the loop/run handler at `pdk_entry.cpp:170-190`:

```cpp
// Parse cancellation_id
std::string cancellation_id;
if (loop_args.contains("cancellation_id")) {
  cancellation_id = loop_args["cancellation_id"].get<std::string>();
}

// Resolve to stop_token via shared registry
// (For Phase B, use a simple file-static registry; production should use ChatSession's)
static CancellationRegistry g_loop_registry;
std::stop_token token = cancellation_id.empty() 
    ? std::stop_token{} 
    : g_loop_registry.resolve_token(cancellation_id);
```

- [ ] **Step 2: Replace `std::stop_token{}` at pdk_entry.cpp:229**

```cpp
// Before:
auto res = provider_.generate(req, std::stop_token{});

// After:
auto res = provider_.generate(req, token);
```

- [ ] **Step 3: Add cancellation check before provider call**

```cpp
if (token.stop_requested()) {
  output["cancelled"] = true;
  return output;
}
```

- [ ] **Step 4: Build + verify pdk_chat_demo still passes**

Run: `cd build && make pdk_chat_demo -j$(nproc) && ./build/examples/pdk_chat_demo/pdk_chat_demo --mock < /dev/null`

Expected: demo starts and exits cleanly on EOF.

---

## Task 4: ReactLoop Token Parameter (Step 5)

**Files:**
- Modify: `include/agenticdsl/pdk/agent_loops/react_loop.h`

- [ ] **Step 1: Add token parameter to run()**

```cpp
// Before:
LoopResult run(const std::string& prompt,
               const agenticdsl::LayeredContext& ctx);

// After:
LoopResult run(const std::string& prompt,
               const agenticdsl::LayeredContext& ctx,
               std::stop_token token = {});
```

- [ ] **Step 2: Update all callers of ReactLoop::run**

```bash
# Find all callers
grep -rn "ReactLoop\|\.run(prompt" --include="*.cpp" --include="*.h"
```

Update each caller to pass token (default `{}` if no upstream).

- [ ] **Step 3: In ReactLoop implementation, pass token to orchestrator**

```cpp
// In run() implementation, after token addition:
orch.process(prompt, callback);  // orchestrator needs token update in future
// For Phase B, document that orchestrator doesn't yet observe token
```

- [ ] **Step 4: Add unit test**

```cpp
TEST_CASE("ReactLoop::run with token forwards to internal calls", "[react_loop][cancel]") {
  // Use MockLLMProvider that records token.stop_requested()
  // Call run() with cancelled token
  // Verify provider observed cancellation
}
```

- [ ] **Step 5: Build + verify no regression**

Run: `cd build && make -j$(nproc) && ctest -R "react_loop" --output-on-failure`

---

## Task 5: PlanExecuteLoop Token Parameter (Step 5)

**Files:**
- Modify: `include/agenticdsl/pdk/agent_loops/plan_execute_loop.h`

- [ ] **Step 1: Add token parameter to run()**

- [ ] **Step 2: Replace `std::stop_token{}` at lines 206, 256 with token parameter**

- [ ] **Step 3: Add token.stop_requested() check between phases**

```cpp
if (token.stop_requested()) {
  result.cancelled = true;
  return result;
}
```

- [ ] **Step 4: Update callers + add unit test + verify build**

---

## Task 6: ForkJoinLoop Token Parameter (Step 5 + 6)

**Files:**
- Modify: `include/agenticdsl/pdk/agent_loops/fork_join_loop.h`

- [ ] **Step 1: Add token parameter to run()**

- [ ] **Step 2: Update CV wait predicate at lines 251-257**

```cpp
// Before:
tracker->cv.wait(lock, [&] {
  return tracker->results.size() >= branches.size() ||
         tracker->any_failed.load(std::memory_order_acquire);
});

// After:
tracker->cv.wait(lock, [&] {
  return tracker->results.size() >= branches.size() ||
         tracker->any_failed.load(std::memory_order_acquire) ||
         token.stop_requested();
});
```

- [ ] **Step 3: Call pool_->stop() on token cancellation**

```cpp
if (token.stop_requested()) {
  pool_->stop();
  result.cancelled = true;
  return result;
}
```

- [ ] **Step 4: Update callers + add unit test (multi-worker scenario) + verify**

---

## Task 7: NodeExecutor Token Forwarding (Step 6)

**Files:**
- Modify: `src/modules/executor/node_executor.cpp`

- [ ] **Step 1: Modify dispatch_to_tool signature**

```cpp
// Before:
std::pair<ToolResult, Context> NodeExecutor::dispatch_to_tool(
    const std::string& tool_name,
    const std::string& node_path,
    const std::unordered_map<std::string, std::string>& args);

// After: add token param (default {} for backward compat)
std::pair<ToolResult, Context> NodeExecutor::dispatch_to_tool(
    const std::string& tool_name,
    const std::string& node_path,
    const std::unordered_map<std::string, std::string>& args,
    std::stop_token token = {});
```

- [ ] **Step 2: Forward token to tool_coordinator_->execute()**

```cpp
if (tool_coordinator_) {
  tool_result = tool_coordinator_->execute(meta, tool_ctx, args, token);
}
```

- [ ] **Step 3: Modify YieldNode path at line 475**

```cpp
// Before:
auto stream = llm_provider_->generate_stream(req, std::stop_token{});

// After:
auto stream = llm_provider_->generate_stream(req, token);
```

- [ ] **Step 4: Update all dispatch_to_tool callers + verify build**

---

## Task 8: ToolCoordinator Token Parameter (Step 6)

**Files:**
- Modify: `src/common/tools/tool_coordinator.cpp`

- [ ] **Step 1: Add token parameter to execute()**

```cpp
ToolResult ToolCoordinator::execute(
    const ToolMetadata& meta,
    const ToolCallContext& ctx,
    const std::unordered_map<std::string, std::string>& args,
    std::stop_token token = {});
```

- [ ] **Step 2: Add short-circuit at entry**

```cpp
if (token.stop_requested()) {
  // Emit audit denied event
  emit(BusEventBuilder("tool.audit.denied")
       .args({{"tool", meta.name}, {"reason", "cancelled"}})
       .build());
  return ToolResult::cancelled();
}
```

- [ ] **Step 3: Forward token to registry_->call_tool() if supported**

- [ ] **Step 4: Update all callers + verify build**

---

## Task 9: Mock Blocking Provider (Step 7)

**Files:**
- Create: `examples/pdk_chat_demo/tests/mock_blocking_provider.h`

- [ ] **Step 1: Create MockBlockingProvider class**

```cpp
class MockBlockingProvider : public agenticdsl::ILLMProvider {
 public:
  ToolResult generate(const GenerationRequest& req, std::stop_token token) override {
    // Loop until cancelled
    while (!token.stop_requested()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return ToolResult::cancelled("MockBlockingProvider cancelled");
  }
  
  bool supports_cancellation() const override { return true; }
};
```

---

## Task 10: E2E Mid-Loop Cancellation Test (Step 7)

**Files:**
- Create: `examples/pdk_chat_demo/tests/test_chat_session_cancellation.cpp`

- [ ] **Step 1: Create E2E test file with 5 test cases**

```cpp
TEST_CASE("request_stop interrupts blocking provider within 100ms", "[chat_session][cancel][e2e]") {
  // Create ChatSession with MockBlockingProvider
  // Start chat() in thread A
  // After 50ms, call session.request_stop() in thread B
  // Measure time until chat() returns
  // Assert < 100ms
}

TEST_CASE("Token identity preserved through registry", "[chat_session][cancel][e2e]") {
  // Create ChatSession
  // Register source, get id
  // Resolve token, verify identity (same source)
}

// ... 3 more tests ...
```

---

## Task 11: Full Verification

- [ ] **Step 1: Run full ctest** - 135+ tests PASS, pre-existing 3 unchanged

- [ ] **Step 2: Run ASan preset** - 0 new errors

- [ ] **Step 3: lsp_diagnostics clean**

---

## Task 12: Documentation Sync + Ship Gate

[Same pattern as Phase 0 / Phase A — docs sync → commit → archive → merge → delete branch]

---

## Out of Scope

- Turn interruption point injection — separate change
- main.cpp integration — separate follow-up
- `/model` switching — Phase C
- Timeout configuration — defaults to 100ms E2E threshold