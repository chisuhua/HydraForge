# fix-tool-registry-signal-handler-shutdown Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix pre-existing SIGSEGV in `pdk_chat_demo` mock mode by routing SIGINT/SIGTERM through an atomic flag → main-thread shutdown path, preserving `engine.h:199-205` member-order destruction guarantees.

**Architecture:** Signal handler becomes async-signal-safe (single atomic store). Main interactive loop observes flag after blocking operations and exits via the existing ordered cleanup path (`engine.reset()` → `unload_all_plugins(loader)`). No new abstractions; minimum delta to fix documented bug.

**Tech Stack:** C++20, Catch2 v3 (subprocess tests via `fork`/`exec`), `<atomic>` (C++20 std::atomic is async-signal-safe).

---

## Scope Adjustments vs proposal

The proposal assumes `g_loader` and `g_bus` are globals accessible from the signal handler. They are already declared in `main.cpp` (verified via `grep -n "g_loader\|g_bus" examples/pdk_chat_demo/main.cpp`).

**Adopted scope** (no deviation from proposal):
- Replace signal_handler body (lines 71-80) with single atomic store
- Add `g_shutdown_requested` atomic global (file scope, before signal handler at line 71)
- Modify main loop (line 466) to check flag after `std::getline`
- Subprocess regression tests for YAML failure + SIGTERM

**Deferred to follow-up** (not implemented here):
- Comprehensive async-signal-safe audit of all signal handlers (out of scope)
- Turn-mid cancellation during `session.chat()` (requires Phase B stop_token chain)

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `examples/pdk_chat_demo/main.cpp` (line 71-80) | Replace `signal_handler` body with atomic store only |
| `examples/pdk_chat_demo/main.cpp` (line 65-70 area) | Add `std::atomic<bool> g_shutdown_requested` global |
| `examples/pdk_chat_demo/main.cpp` (line 466-522) | Add flag check at top of main loop |

### Tests

| File | Responsibility |
|---|---|
| `examples/pdk_chat_demo/tests/test_signal_shutdown.cpp` (new) | 2 subprocess tests: YAML failure + SIGTERM shutdown |

### No CMake change required
Test files auto-discovered via `file(GLOB test_*.cpp)` (verify in `examples/pdk_chat_demo/tests/CMakeLists.txt` before plan execution).

---

## Task 1: Add Subprocess Test for YAML Validation Failure

**Files:**
- Create: `examples/pdk_chat_demo/tests/test_signal_shutdown.cpp`

- [ ] **Step 1: Write the failing test (TDD Step 1)**

Create file with content:

```cpp
#include <catch_amalgamated.hpp>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <string>
#include <array>

namespace {

// Run pdk_chat_demo with optional env overrides, capture exit code + stderr.
// Returns true if process exited cleanly via _exit() (no SIGSEGV).
struct SubprocessResult {
  int exit_code;
  bool signaled;
  int signal;
  std::string stderr_output;
};

SubprocessResult run_pdk_chat_demo(const std::string& config_body) {
  // Write config to /tmp/chat_demo_invalid_<pid>.json
  char config_path[64];
  snprintf(config_path, sizeof(config_path),
           "/tmp/chat_demo_invalid_%d.json", getpid());
  {
    std::ofstream f(config_path);
    f << config_body;
  }

  int pipefd[2];
  REQUIRE(pipe(pipefd) == 0);

  pid_t pid = fork();
  REQUIRE(pid >= 0);

  if (pid == 0) {
    // Child: redirect stderr → pipe, exec demo
    close(pipefd[0]);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[1]);
    setenv("PDK_CHAT_DEMO_CONFIG", config_path, 1);
    execl("./build/examples/pdk_chat_demo/pdk_chat_demo",
          "pdk_chat_demo", "--mock", (char*)nullptr);
    _exit(127);  // exec failed
  }

  // Parent: read stderr
  close(pipefd[1]);
  std::array<char, 4096> buf;
  std::string out;
  ssize_t n;
  while ((n = read(pipefd[0], buf.data(), buf.size())) > 0) {
    out.append(buf.data(), n);
  }
  close(pipefd[0]);

  int status = 0;
  waitpid(pid, &status, 0);

  SubprocessResult r;
  r.stderr_output = out;
  if (WIFSIGNALED(status)) {
    r.signaled = true;
    r.signal = WTERMSIG(status);
    r.exit_code = -1;
  } else {
    r.signaled = false;
    r.signal = 0;
    r.exit_code = WEXITSTATUS(status);
  }
  unlink(config_path);
  return r;
}

}  // namespace

TEST_CASE("YAML validation failure exits cleanly without SIGSEGV",
          "[signal_shutdown][regression]") {
  // Deliberately malformed YAML frontmatter to trigger DSL validator failure
  const std::string bad_config = R"({
    "schema_version":"1.0",
    "app_id":"test",
    "providers":{},
    "agent":{"provider":"mock","model":"test","system_prompt":"",
             "DSL_FILE":"nonexistent-malformed-file.md"}
  })";

  auto r = run_pdk_chat_demo(bad_config);

  INFO("stderr: " << r.stderr_output);
  CHECK_FALSE(r.signaled);  // MUST NOT die from SIGSEGV
  CHECK(r.signal != SIGSEGV);
  CHECK(r.exit_code != 0);  // Expected: validation error code
}

TEST_CASE("SIGTERM during startup exits cleanly without SIGSEGV",
          "[signal_shutdown][regression]") {
  // Start a valid demo, send SIGTERM mid-load, expect clean exit
  const std::string valid_config = R"({
    "schema_version":"1.0",
    "app_id":"test",
    "providers":{},
    "agent":{"provider":"mock","model":"test","system_prompt":"You are helpful."}
  })";

  int pipefd[2];
  REQUIRE(pipe(pipefd) == 0);

  char config_path[64];
  snprintf(config_path, sizeof(config_path),
           "/tmp/chat_demo_sigterm_%d.json", getpid());
  {
    std::ofstream f(config_path);
    f << valid_config;
  }

  pid_t pid = fork();
  REQUIRE(pid >= 0);
  if (pid == 0) {
    close(pipefd[0]);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[1]);
    setenv("PDK_CHAT_DEMO_CONFIG", config_path, 1);
    execl("./build/examples/pdk_chat_demo/pdk_chat_demo",
          "pdk_chat_demo", "--mock", (char*)nullptr);
    _exit(127);
  }
  close(pipefd[1]);

  // Give demo time to load plugins + start loop
  usleep(500000);  // 500ms
  kill(pid, SIGTERM);

  int status = 0;
  waitpid(pid, &status, 0);

  std::array<char, 4096> buf;
  std::string out;
  ssize_t n;
  while ((n = read(pipefd[0], buf.data(), buf.size())) > 0) {
    out.append(buf.data(), n);
  }
  close(pipefd[0]);

  INFO("stderr: " << out);
  CHECK_FALSE(WIFSIGNALED(status));
  CHECK(WTERMSIG(status) != SIGSEGV);
  unlink(config_path);
}
```

- [ ] **Step 2: Build and run to verify test compiles + runs** (TDD Step 2)

Run: `cd build && cmake .. -DAGENTICDSL_BUILD_EXAMPLES=ON && make test_signal_shutdown -j$(nproc) && ./examples/pdk_chat_demo/tests/test_signal_shutdown`

Expected: COMPILE PASS (subprocess helper works on its own). Tests may PASS or FAIL depending on current main behavior; we're locking in baseline.

- [ ] **Step 3: Verify YAML test currently FAILS with SIGSEGV** (TDD Step 3)

Run: `./examples/pdk_chat_demo/tests/test_signal_shutdown --test-case="YAML validation*"`

Expected: FAIL with `signaled=true, signal=SIGSEGV` (this proves the bug exists and the test reproduces it).

If test passes already, investigate — the current behavior may differ from documented.

---

## Task 2: Add Atomic Flag Global

**Files:**
- Modify: `examples/pdk_chat_demo/main.cpp:65-70` (before signal handler)

- [ ] **Step 1: Insert atomic flag global**

Edit `examples/pdk_chat_demo/main.cpp` — insert before `void unload_all_plugins(...)` at line 65:

```cpp
// Async-signal-safe shutdown flag. Set by signal_handler, observed by main loop.
// MUST be initialized before std::signal() is called at line 462-463.
std::atomic<bool> g_shutdown_requested{false};
```

- [ ] **Step 2: Build to verify syntax** (TDD Step 4)

Run: `cd build && make pdk_chat_demo -j$(nproc)`

Expected: BUILD PASS. Atomic flag is declared but unused, so no warnings expected (may need to verify with -Wall).

---

## Task 3: Rewrite Signal Handler

**Files:**
- Modify: `examples/pdk_chat_demo/main.cpp:71-80`

- [ ] **Step 1: Replace signal_handler body**

Edit `examples/pdk_chat_demo/main.cpp` — replace the entire `signal_handler` function (lines 71-80):

```cpp
void signal_handler(int /*sig*/) {
    // 仅设置 shutdown flag，触发由 main 线程在循环观察点执行。
    // 禁止调用 unload_all_plugins() / std::exit() —— 任何非 async-signal-safe
    // 操作都会绕过 engine.h:199-205 的成员析构顺序保证（plugin_loader_ 先于
    // tool_registry_ 声明 → 反向析构时 tool_registry_ 先析构 → ToolRegistry
    // 隐式析构 std::function 回调目标时 plugin .so 已被 dlclose() → SIGSEGV）。
    // 审计依据：docs/audits/2026-08-08-chat-async-io-steering-pre-approval.md
    g_shutdown_requested.store(true, std::memory_order_release);
}
```

- [ ] **Step 2: Build to verify compilation** (TDD Step 4)

Run: `cd build && make pdk_chat_demo -j$(nproc)`

Expected: BUILD PASS. Signal handler now contains only the atomic store.

- [ ] **Step 3: Verify YAML test now PASSES** (TDD Step 5)

Run: `./examples/pdk_chat_demo/tests/test_signal_shutdown --test-case="YAML validation*"`

Expected: PASS (signaled=false, signal!=SIGSEGV, exit_code != 0).

If FAIL: investigate — main loop must be observing flag (next task).

---

## Task 4: Main Loop Flag Observation

**Files:**
- Modify: `examples/pdk_chat_demo/main.cpp:466-522`

- [ ] **Step 1: Add flag check at top of main loop**

Edit `examples/pdk_chat_demo/main.cpp` — modify the `while (std::getline(...))` block at line 466 to check the flag before processing input:

Find:
```cpp
    std::string input;
    while (std::getline(std::cin, input)) {
        if (!input.empty() && input.front() == '/') {
```

Replace with:
```cpp
    std::string input;
    while (std::getline(std::cin, input)) {
        // 优先检查 shutdown flag —— signal_handler 仅置位，实际清理在 main 线程执行。
        if (g_shutdown_requested.load(std::memory_order_acquire)) {
            break;
        }
        if (!input.empty() && input.front() == '/') {
```

- [ ] **Step 2: Build to verify compilation** (TDD Step 4)

Run: `cd build && make pdk_chat_demo -j$(nproc)`

Expected: BUILD PASS.

- [ ] **Step 3: Verify SIGTERM test now PASSES** (TDD Step 5)

Run: `./examples/pdk_chat_demo/tests/test_signal_shutdown --test-case="SIGTERM during*"`

Expected: PASS (signaled=false, signal!=SIGSEGV).

- [ ] **Step 4: Verify YAML test still PASSES** (TDD Step 5 - no regression)

Run: `./examples/pdk_chat_demo/tests/test_signal_shutdown --test-case="YAML validation*"`

Expected: PASS (StartupCleanupGuard already handles early exits correctly).

---

## Task 5: Full Regression Validation

- [ ] **Step 1: Run full ctest suite**

Run: `cd build && ctest -j$(nproc) --output-on-failure`

Expected: pre-existing 5 failures unchanged (test_cost_tracking_decorator, test_pdk_chat_demo_cli_args, test_e2e_real_llm, test_session_tree_commands, test_pdk_chat_demo_session_tree_cli_flags). New test_signal_shutdown PASSES (2/2). No new failures.

- [ ] **Step 2: Run ASan preset**

Run: `cmake --preset asan -DAGENTICDSL_BUILD_TESTS=ON && ctest -R signal_shutdown --output-on-failure`

Expected: 2/2 PASS, 0 new ASan errors.

- [ ] **Step 3: Run interactive manual verification** (optional, if time permits)

```bash
./build/examples/pdk_chat_demo/pdk_chat_demo --mock
# Wait for "User>" prompt
# Press Ctrl+C
# Expected: prints "[main] shutdown flag set", exits with code 0
```

---

## Task 6: Documentation Sync

- [ ] **Step 1: Update AGENTS.md Recent Changes**

Add new entry at top of "## Recent Changes" section:

```markdown
- 2026-08-08 (Wave 3-A / fix-tool-registry-signal-handler-shutdown, ship): OpenSpec change 实施完成 — signal handler 仅置 atomic flag，main 循环观察后走正常有序清理路径（`engine.reset()` → `unload_all_plugins(loader)`），修复 mock 模式下 SIGINT/SIGTERM 触发的 use-after-unload SIGSEGV。1 commit，2 新增子进程回归测试 PASS，pre-existing 5 失败不变。直接解锁 chat-async-io-cancellation-chain (Phase B) E2E 测试稳定性。
```

- [ ] **Step 2: Update docs/active-status.md**

Add entry referencing this change's ship status.

- [ ] **Step 3: Commit documentation updates**

Run: `cd /workspace/project/HydraForge && git add -A && git commit -m "docs(sync): fix-tool-registry-signal-handler-shutdown ship record"`

---

## Task 7: Ship Gate

- [ ] **Step 1: Validate OpenSpec change**

Run: `openspec validate fix-tool-registry-signal-handler-shutdown --strict`

Expected: "Change 'fix-tool-registry-signal-handler-shutdown' is valid", exit 0.

- [ ] **Step 2: Run ADR lint**

Run: `python3 tools/adr_lint.py`

Expected: 0 errors.

- [ ] **Step 3: Run docs drift audit**

Run: `python3 tools/docs_drift_audit.py`

Expected: 0 DRIFT items.

- [ ] **Step 4: Aggregate commit (single atomic commit per worktree-archive-workflow rule)**

Run:
```bash
cd /workspace/project/HydraForge
git status --short
# Verify only signal_handler + flag + loop check + tests are modified
git add -A
git commit -m "fix(pdk-chat-demo): route SIGINT/SIGTERM via atomic flag to prevent SIGSEGV

Signal handler now sets g_shutdown_requested (async-signal-safe
atomic store). Main interactive loop observes flag and exits via
existing ordered cleanup path (engine.reset() → unload_all_plugins()).

Fixes use-after-unload SIGSEGV when Ctrl+C fires during plugin load
or early shutdown, preserving engine.h:199-205 destruction order.

Audit: docs/audits/2026-08-08-chat-async-io-steering-pre-approval.md
Unblocks: chat-async-io-cancellation-chain (Phase B) E2E tests"
```

- [ ] **Step 5: Archive change**

Run: `openspec archive fix-tool-registry-signal-handler-shutdown --yes`

Expected: "🎉 Change fix-tool-registry-signal-handler-shutdown 归档完成".

- [ ] **Step 6: Update proposal-approved.md**

Move `fix-tool-registry-signal-handler-shutdown` from "已批准提案" (if present) to "已实施" section with archive link.

- [ ] **Step 7: Sync iteration.json**

Run: `rddf status` — verify the change now shows `📦 archived`.

---

## Out of Scope (documented in proposal, NOT implemented here)

- Comprehensive async-signal-safe audit of all signal handlers
- Turn-mid cancellation during `session.chat()` (requires Phase B stop_token chain)
- New shutdown abstraction layer
- Plugin ownership tracking in ToolRegistry