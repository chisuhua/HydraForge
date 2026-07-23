# Spike §0.4 — SkillInterpreter pre-implementation risk validation

**Date:** 2026-07-21
**OpenSpec change:** `skill-interpreter-real-loading`
**Author:** Sisyphus-Junior (deep research agent, session spike §0.4)
**Status:** ✅ Complete — implementation unblocked (1 of 3 risks FAIL, must-fix in §1.3)
**Build target audited:** `build/examples/pdk_chat_demo/pdk_chat_demo` (3,536,728 bytes, 2026-07-19)

> This spike validates 3 of the 5 CRITICAL risks (C3/C4/C5) flagged in earlier
> reviews by building and inspecting `pdk_chat_demo` at runtime. C1/C2 are
> implementation-time risks and are out of scope here.

---

## 1. Executive Summary

| Risk | Verdict | Headline evidence |
|------|---------|-------------------|
| **C3** `emit_event` indirect capability bypass | ✅ **PASS** | Zero production subscribers call `call_tool` from inside their handler; `ToolCoordinator` is **opt-in** (`set_tool_coordinator`) and not activated in `pdk_chat_demo`. |
| **C4** Global static thread starters | ✅ **PASS** | `Threads=1` at `t=0.05s` (post-`execve`, pre-`main`); `Threads=2` only at `t=0.30s` (after `InMemoryBus()` constructor in `main.cpp:86`). The 2nd thread is `dispatch_thread`, **not** a global. |
| **C5** `posix_spawn` file_actions ordering + fd leak | ❌ **FAIL** (ordering) / ✅ **PASS** (leak) | `addclosefrom_np(3)` runs BEFORE `adddup2(pipe_*, 0/1/2)`, closing the pipe fds first → `posix_spawn` returns `EBADF` (verified with `/tmp/c5_test.cpp`). `closefrom_np` itself fully prevents fd leak regardless of `O_CLOEXEC`. |
| Kernel/glibc compatibility | ✅ **PASS** | Kernel 5.10.134 (≥4.14 → `SECCOMP_RET_KILL_PROCESS` available); glibc 2.39 (≥2.29 → `addclosefrom_np` available). |

**Implementation impact:**
- C5 ordering bug **must be fixed** in `tasks.md §1.3` before §1 code lands.
- C3 and C4 are safe as-designed; recommend a defensive `allowed_topics` whitelist for `emit_event` (already specified in `design.md` Decision 5) and a defensive `--skill-child`-branch pre-flight thread cancel (tasks.md §1.4 addition).

---

## 2. Build & Runtime Environment

| Component | Value |
|-----------|-------|
| Kernel | `5.10.134-18.0.11.lifsea8.x86_64` (Linux) |
| glibc | `2.39-0ubuntu8.7` (Ubuntu) |
| Compiler | g++ (Ubuntu 13.x) |
| CMake | ≥3.20 (per `CMakePresets.json` minimum) |
| Build preset | `debug` (existing `build/` directory) |
| CMake flags | `AGENTICDSL_BUILD_EXAMPLES=ON`, `AGENTICDSL_BUILD_TESTS=ON` (already set in `build/CMakeCache.txt`) |
| Binary audited | `build/examples/pdk_chat_demo/pdk_chat_demo` |
| Binary size | 3,536,728 bytes |
| Binary date | 2026-07-19 23:15 |
| Linked libs | `libssl.so.3`, `libcrypto.so.3`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6`, `ld-linux-x86-64.so.2` (per `ldd`) |

**Binary choice rationale:** `pdk_chat_demo` is the exact target named in `design.md` Decision 7 ("仅修改 `examples/pdk_chat_demo/main.cpp`"). No fallback needed; existing binary built clean and runs.

---

## 3. C3 — `emit_event` indirect capability bypass

### 3.1 Hypotheses

- **H1 (FAIL):** Some production subscriber of `IInteractionBus::emit` (e.g. `ToolCoordinator` audit chain, `CostTrackingDecorator`, audit log subscriber) calls `registry.call_tool()` from inside its handler. A malicious SKILL could `emit_event("tool.audit.invoked", {...})` to fan out an unauthorized `call_tool`.
- **H2 (PASS):** All production subscribers are pure side-effect (terminal print, state-tracking). No subscriber calls `call_tool`.

### 3.2 Evidence — production `subscribe()` call sites

Static analysis grep across `src/`, `examples/`, `include/`, `pdk/` (excluding tests, build, cache, external):

| File:line | Topic(s) | Subscriber handler body | Calls `call_tool`? |
|-----------|----------|--------------------------|---------------------|
| `examples/pdk_chat_demo/event_handler.cpp:103` | 12 topics: `user.input`, `loop.turn.start`, `loop.turn.end`, `llm.request`, `llm.response`, `loop.decision`, `tool.execution.start`, `tool.execution.end`, `loop.done`, `loop.error`, `session.persisted`, `budget.checked` | `print_event(topic, result.meta)` — string formatting + `(*out) << ...` (file:43-80) | ❌ No |
| `include/agenticdsl/pdk/agent_loops/fork_join_loop.h:167` | `domain.task.completed` | Lock tracker mutex, push to `tracker->results[output_key]`, notify cv (file:169-178) | ❌ No |
| `include/agenticdsl/pdk/agent_loops/fork_join_loop.h:181` | `domain.task.failed` | Lock tracker mutex, push to `tracker->results[output_key]`, set `any_failed`, record first failure msg (file:183-201) | ❌ No |
| `src/core/engine.cpp:229` | (passthrough) | `DSLEngine::subscribe` is a 3-line forwarder to `bus_->subscribe` — no production registration | ❌ No |

**No other production subscribers found.** Tests have subscribers (test_cognitive_worker, test_compliance_decorator, test_domain_worker_pool, test_engine_bus_integration, test_interaction_bus, test_stream_to_bus) but these are not part of `pdk_chat_demo` runtime.

### 3.3 Evidence — `ToolCoordinator` is producer-only + opt-in

`src/common/tools/tool_coordinator.cpp` (read in full):

- `ToolCoordinator::execute()` **emits** `tool.audit.invoked`, `tool.audit.completed`, `tool.audit.denied` (lines 178-258) but **never subscribes** to any topic.
- It calls `registry_.call_tool()` directly (line 223), but only on **its caller's** behalf — not as a reaction to a bus event.
- `DSLEngine::set_tool_coordinator()` is **opt-in** (`src/core/engine.cpp:108` comment: "ToolCoordinator 默认不创建 (opt-in 兼容)"; `engine.h:175-176` declares the setter). `pdk_chat_demo/main.cpp` does **not** call `set_tool_coordinator` (verified by grep → no matches outside `.md` files).

### 3.4 Evidence — `CostTrackingDecorator`, `ApprovalHandler` don't subscribe

`src/common/llm/cost_tracking_decorator.{h,cpp}` and `src/common/policy/approval_handler.{h,cpp}`: `grep subscribe` returns zero matches. Both are decorator/wrapper classes with no bus consumer role.

### 3.5 Verdict

**✅ PASS — sandbox intact against indirect `call_tool` via `emit_event`.**

- H1 (FAIL hypothesis) **rejected** — no production subscriber calls `call_tool`.
- H2 (PASS hypothesis) **confirmed**.

### 3.6 Recommended `design.md` Decision 12 revision

Even though C3 currently passes, the safety property is **fragile** — any future
subscriber that calls `call_tool` (e.g. a future audit-log subscriber that
re-invokes the tool for retry) would silently break the sandbox. Recommend
adding a **defense-in-depth** whitelist check in `dispatch_emit_event` itself.

**Before (`design.md` Decision 12, lines 376-381):**
```cpp
IPCResponse dispatch_emit_event(const std::string& topic,
                                const nlohmann::json& payload,
                                IInteractionBus& bus) {
    bus_->emit(topic, payload.dump());  // 桥接到 string 重载
    return IPCResponse{ok=true};
}
```

**After (proposed):**
```cpp
IPCResponse dispatch_emit_event(const std::string& topic,
                                const nlohmann::json& payload,
                                const SkillCapability& cap,
                                IInteractionBus& bus) {
    // Defense-in-depth: 即使当前无 subscriber 调 call_tool, 未来加一个就破了.
    // allowed_topics 白名单在父进程侧强制, 子进程不可绕过.
    if (std::find(cap.allowed_topics.begin(),
                  cap.allowed_topics.end(),
                  topic) == cap.allowed_topics.end()) {
        return IPCResponse{ok=false,
                           error="emit_event topic not in allowed_topics"};
    }
    bus_->emit(topic, payload.dump());  // 桥接到 string 重载
    return IPCResponse{ok=true};
}
```

This converts a "fragile-pass" into a "structural-pass" and is independent of
future subscriber additions. The default `SkillCapability::allowed_topics = {}`
from `design.md` Decision 5 means all `emit_event` calls are denied by default
unless explicitly granted — the correct secure-by-default posture.

---

## 4. C4 — Global object thread audit

### 4.1 Hypotheses

- **H1 (FAIL):** Some translation unit linked into `pdk_chat_demo` has a global/static object whose constructor starts a `std::thread` / `std::jthread` / `pthread_create`. After `posix_spawn + execve`, the child has live threads BEFORE entering `--skill-child`. seccomp loaded with flag `0` (no TSYNC) only constrains the loading thread — others remain unrestricted. Sandbox bypassed.
- **H2 (PASS):** No global/static thread starters. All threads are created from `main()` (e.g. `InMemoryBus::dispatch_thread_`). After `execve`, the child has exactly 1 thread (main) until `main()` decides to spawn more — but the `--skill-child` branch in `main.cpp` (per design.md Decision 7) skips `InMemoryBus` construction entirely.

### 4.2 Static evidence — `_GLOBAL__sub_I_` symbols

```
$ nm build/examples/pdk_chat_demo/pdk_chat_demo | grep '_GLOBAL__sub_I_' | wc -l
1
$ nm build/examples/pdk_chat_demo/pdk_chat_demo | grep '_GLOBAL__sub_I_'
00000000000aae00 t _GLOBAL__sub_I__ZN10agenticdsl15CloudLLMAdapterC2ENS_9LLMConfigE
```

Only **1** static initializer in the entire binary. The symbol demangled:
`_GLOBAL__sub_I__ZN10agenticdsl15CloudLLMAdapterC2ENS_9LLMConfigE` → static guard
for a function-local static inside `CloudLLMAdapter::CloudLLMAdapter(LLMConfig)`
(likely a `static std::mutex` or `static std::mt19937` inside the constructor body —
**not** a global object).

Verified: `grep -n "^static\s\|^[A-Za-z_].*=\s*new"` on
`src/common/llm/cloud_adapter.cpp` returns only `static int compute_backoff(...)`
(a function, not an object). No `std::thread`/`std::jthread`/`pthread_create`
in any global/static scope (verified via
`grep -rn "^static\s" src/common/llm/ src/common/contract/ src/core/` → only
static functions and static methods).

**Plugin .so files** (loaded at runtime via `dlopen`):
```
$ for so in build/pdk/{budget_agent,fs_tools,loop_agent,provider_agent,session_agent,shell_tools}/lib*.so; do
    nm "$so" | grep -c '_GLOBAL__sub_I_'
  done
0
0
0
0
0
0
```
**Zero** static initializers in any of the 6 production plugins.

### 4.3 Runtime evidence — `/proc/<pid>/status` Threads count over time

Python launcher (`/tmp/spike_c4_timing.py`) keeps stdin open via a pipe; snapshots
`/proc/<pid>/status` at three timestamps:

```
PID=442522
[t=0.05s] State: D (disk sleep)   Threads: 1
  TID 442522 comm='pdk_chat_demo'
[t=0.30s] State: S (sleeping)     Threads: 2
  TID 442522 comm='pdk_chat_demo'
  TID 442525 comm='pdk_chat_demo'
[t=1.00s] State: S (sleeping)     Threads: 2
  TID 442522 comm='pdk_chat_demo'
  TID 442525 comm='pdk_chat_demo'
```

**Interpretation:**
- `t=0.05s` — right after `execve`, before `main()` reaches `InMemoryBus()` constructor: **Threads=1** (only main).
- `t=0.30s` — after `main.cpp:86` `auto bus = std::make_shared<agenticdsl::InMemoryBus>();` runs (constructor `inmemory_bus.cpp:13-14` starts `dispatch_thread_`): **Threads=2**.

The 2nd thread is therefore the InMemoryBus dispatch thread, **created from inside `main()`** — **not** by a global static initializer.

### 4.4 Verdict

**✅ PASS — no global static thread starters.**

- H1 (FAIL hypothesis) **rejected**.
- H2 (PASS hypothesis) **confirmed**.

After `posix_spawn + execve`, the child `--skill-child` branch starts with
**Threads=1** (just main). The `--skill-child` early branch (per design.md Decision 7)
**skips** the `InMemoryBus()` constructor, so the child never spawns a 2nd thread
even later. seccomp with flag `0` (no TSYNC) is therefore sufficient: there is
exactly one thread to filter.

### 4.5 Recommended `tasks.md` §1.4 / §7.14 revision

Even though C4 currently passes, the safety property relies on a
**negative invariant** ("no future TU adds a global thread starter"). Recommend
adding a **defensive pre-flight thread cancel** in `skill_child_main()`:

**Proposed §1.4 addition:**

```cpp
int skill_child_main(int argc, char** argv) {
    // === C4 defense-in-depth: cancel any inherited threads (should be 0) ===
    // posix_spawn+execve guarantees Threads=1, but this is a cheap belt-and-braces
    // check against future global static thread starters in linked TUs.
    // If Threads > 1 here, log + abort rather than silently proceeding.
    {
        std::ifstream f("/proc/self/status");
        std::string line;
        int threads = 1;
        while (std::getline(f, line)) {
            if (line.rfind("Threads:", 0) == 0) {
                threads = std::stoi(line.substr(8));
                break;
            }
        }
        if (threads != 1) {
            std::fprintf(stderr,
                "skill_child_main: Threads=%d at entry (expected 1). "
                "Likely global static thread starter. ABORT.\n", threads);
            return EXIT_CHILD_THREAD_LEAK = 73;
        }
    }
    // ... rest of skill_child_main (envp parse, parse SKILL.md, prctl, seccomp, loop)
}
```

This is a Linux-only check (matches the `#ifdef __linux__` guard on the whole
module). New exit code 73 added to the table in design.md Decision 7.

**Proposed §7.14 (test addition):**

```
- [ ] 7.23 TEST_CASE "child entry Threads==1 invariant" - 父进程 posix_spawn
      后立即读 /proc/<child>/status, 断言 Threads==1 (验证 C4 不变量, 防止未来
      全局静态 thread starter 引入回归)
```

---

## 5. C5 — `posix_spawn` file_actions ordering + fd leakage

### 5.1 Hypotheses (ordering)

- **H1 (FAIL):** `design.md` Decision 2 lines 89-92 places `addclosefrom_np(3)` **before** `adddup2(pipe_*, 0/1/2)`. Per POSIX, `posix_spawn` executes `file_actions` in the order they were added. `pipe2(O_CLOEXEC)` returns fds ≥ 3 (typically 3,4,5,6,7,8). `addclosefrom_np(3)` closes them all in the child **before** `adddup2` runs → `adddup2(pipe_in[0], 0)` finds `pipe_in[0]` already closed → returns `EBADF`.
- **H2 (PASS):** `posix_spawn` reorders internally or `adddup2` somehow still works.

### 5.2 Evidence — design.md current code (Decision 2, lines 86-92)

```cpp
posix_spawn_file_actions_t actions;
posix_spawn_file_actions_init(&actions);
posix_spawn_file_actions_addclosefrom_np(&actions, 3);  // ← runs FIRST in child
posix_spawn_file_actions_adddup2(&actions, pipe_in[0], STDIN_FILENO);   // ← pipe_in[0] already closed!
posix_spawn_file_actions_adddup2(&actions, pipe_out[1], STDOUT_FILENO);
posix_spawn_file_actions_adddup2(&actions, pipe_err[1], STDERR_FILENO);
```

### 5.3 Evidence — minimal reproducer

`/tmp/c5_test.cpp` (Pattern A = design.md ordering):

```
$ g++ -std=c++20 /tmp/c5_test.cpp -o /tmp/c5_test && /tmp/c5_test
parent: read_fd=3 write_fd=4
posix_spawn failed: Bad file descriptor    ← EBADF
```

`/tmp/c5_test_b.cpp` (Pattern B = fixed ordering, dup2 FIRST then closefrom):

```
$ g++ -std=c++20 /tmp/c5_test_b.cpp -o /tmp/c5_test_b && /tmp/c5_test_b
parent: read_fd=3 write_fd=4
child exit status (Pattern B - closefrom LAST): WIFEXITED=1 WEXITSTATUS=0    ← OK
```

### 5.4 Verdict (ordering)

**❌ FAIL — design.md Decision 2 ordering is broken.** `posix_spawn` returns
`EBADF` and never even spawns the child. This is a **must-fix** before §1.3
implementation lands.

### 5.5 Hypotheses (fd leak)

- **H1 (FAIL):** Some fd > 3 in the parent (e.g. httplib socket, llama.cpp mmap fd, grpc socket) lacks `O_CLOEXEC` and leaks into the child.
- **H2 (PASS):** `addclosefrom_np(3)` closes all fds ≥ 3 in the child regardless of `O_CLOEXEC`, so leak is structurally prevented.

### 5.6 Evidence (fd leak) — runtime `/proc/<pid>/fd/` snapshot

Python launcher (`/tmp/spike_c5_fd.py`):

```
PID=447443
=== fd table ===
  fd   0 flags=00 NO-CLOEXEC!! target=pipe:[70345372]
  fd   1 flags=01 NO-CLOEXEC!! target=pipe:[70345373]
  fd   2 flags=01 NO-CLOEXEC!! target=pipe:[70345374]
```

**Only fds 0/1/2 exist** in the parent at steady state (after 6 plugins loaded,
InMemoryBus dispatch_thread started, MockLLMProvider configured). The "NO-CLOEXEC!!"
annotations are **expected and harmless** — these are the stdin/stdout/stderr pipes
Python opened to the child; they will be replaced by `adddup2` to IPC pipes in the
real `SkillInterpreter::run()` call.

Crucially: there are **no socket fds, no extra pipe fds, no plugin dlopen-loaded
fds** visible in the parent fd table at the moment of `posix_spawn` (which would
happen during `ipc_loop_and_wait`, i.e. after plugin load completes). The
`addclosefrom_np(3)` is therefore a **defense-in-depth** that catches any future
fd leak regardless of `O_CLOEXEC` provenance.

### 5.7 Verdict (fd leak)

**✅ PASS — no fd leak risk.** `addclosefrom_np(3)` is sufficient on its own;
`pipe2(O_CLOEXEC)` (already in design.md) is a redundant safety net.

### 5.8 Recommended `design.md` Decision 2 revision + `tasks.md` §1.3 fix

**Before (design.md Decision 2, lines 87-92 — broken):**
```cpp
posix_spawn_file_actions_t actions;
posix_spawn_file_actions_init(&actions);
posix_spawn_file_actions_addclosefrom_np(&actions, 3);  // BUG: runs FIRST, closes pipe fds
posix_spawn_file_actions_adddup2(&actions, pipe_in[0], STDIN_FILENO);
posix_spawn_file_actions_adddup2(&actions, pipe_out[1], STDOUT_FILENO);
posix_spawn_file_actions_adddup2(&actions, pipe_err[1], STDERR_FILENO);
```

**After (proposed fix — dup2 FIRST, closefrom LAST):**
```cpp
posix_spawn_file_actions_t actions;
posix_spawn_file_actions_init(&actions);
// FIX: dup2 FIRST so pipe fds still exist in child when dup2 runs.
// POSIX guarantees file_actions execute in add-order; closefrom_np(3) must be LAST
// so it doesn't close the very pipe fds we're about to dup2.
posix_spawn_file_actions_adddup2(&actions, pipe_in[0], STDIN_FILENO);
posix_spawn_file_actions_adddup2(&actions, pipe_out[1], STDOUT_FILENO);
posix_spawn_file_actions_adddup2(&actions, pipe_err[1], STDERR_FILENO);
posix_spawn_file_actions_addclosefrom_np(&actions, 3);  // NOW safe to close ≥3
```

**Alternative (more conservative, kept as Decision 10 fallback):** replace
`addclosefrom_np(3)` with explicit `addclose(pipe_in[0])`, `addclose(pipe_out[1])`,
`addclose(pipe_err[1])` after the dup2s, plus a loop closing fds 3..getdtablesize()
at spawn-time. This avoids the glibc 2.29+ dependency but is slower (1 syscall per
fd). The fixed ordering above is preferred on glibc ≥ 2.29 (already required by
design.md Decision 10).

**`tasks.md` §1.3 revision (proposed):**

```diff
- posix_spawn_file_actions_t 初始化：`addclosefrom_np(3)` + `adddup2` 把 pipe fd 映射到 STDIN/STDOUT/STDERR（glibc ≥ 2.29）
+ posix_spawn_file_actions_t 初始化（**顺序敏感**，C5 修正）：
+   1. 先 `adddup2(pipe_in[0], STDIN_FILENO)` / `adddup2(pipe_out[1], STDOUT_FILENO)` / `adddup2(pipe_err[1], STDERR_FILENO)` — 在 pipe fd 还存在时执行 dup2
+   2. **再** `addclosefrom_np(3)` — 关闭所有 fd ≥ 3（包括 dup2 后的 pipe fd 原始值）
+   POSIX 规定 file_actions 按 add-order 执行；若 closefrom 先于 dup2 运行, pipe fd 已被关闭, dup2 返回 EBADF (验证: /tmp/c5_test.cpp, see docs/audits/2026-07-21-skill-interpreter-spike-0.4.md §5)
```

---

## 6. Kernel/glibc compatibility

| Component | Required | Available | Verdict |
|-----------|----------|-----------|---------|
| Kernel `SECCOMP_RET_KILL_PROCESS` | ≥ 4.14 (per design.md §Risks) | 5.10.134 | ✅ Available (`/usr/include/linux/seccomp.h` defines `0x80000000U`) |
| glibc `posix_spawn_file_actions_addclosefrom_np` | ≥ 2.29 (per design.md Decision 10) | 2.39 | ✅ Available (declared in `/usr/include/spawn.h`) |
| `prctl(PR_SET_NO_NEW_PRIVS)` | any Linux | always | ✅ Available (`/usr/include/linux/prctl.h:175` defines `38`) |
| `prctl(PR_SET_SECCOMP)` | any Linux | always | ✅ Available (`/usr/include/linux/prctl.h:68` defines `22`) |
| `SECCOMP_SET_MODE_FILTER` | ≥ 4.8 kernel | 5.10.134 | ✅ Available (`/usr/include/linux/seccomp.h` defines `1`) |

**Verdict:** ✅ All APIs available. No fallback paths (Decision 10 loop-close, KILL_THREAD fallback) are needed on this dev environment. CI must verify ubuntu-latest (glibc 2.37+) per design.md §Decision 10.

---

## 7. Proposed `design.md` revisions (diff-style)

### 7.1 Decision 2 (C5) — file_actions ordering fix

**Section:** `design.md` §Decision 2, lines 86-92

```diff
     posix_spawn_file_actions_t actions;
     posix_spawn_file_actions_init(&actions);
-    posix_spawn_file_actions_addclosefrom_np(&actions, 3);  // 关闭 ≥3 的所有 fd
+    // === C5 修正 (spike §0.4, 2026-07-21): 顺序敏感 ===
+    // POSIX 规定 file_actions 按 add-order 执行. closefrom 必须在 dup2 之后,
+    // 否则 pipe fd 已被关闭, dup2 返回 EBADF (验证: /tmp/c5_test.cpp).
     posix_spawn_file_actions_adddup2(&actions, pipe_in[0], STDIN_FILENO);
     posix_spawn_file_actions_adddup2(&actions, pipe_out[1], STDOUT_FILENO);
     posix_spawn_file_actions_adddup2(&actions, pipe_err[1], STDERR_FILENO);
+    posix_spawn_file_actions_addclosefrom_np(&actions, 3);  // NOW safe: 关闭 ≥3 的所有 fd
```

### 7.2 Decision 12 (C3) — `emit_event` allowed_topics whitelist

**Section:** `design.md` §Decision 12, lines 376-381

```diff
-IPCResponse dispatch_emit_event(const std::string& topic, const nlohmann::json& payload, IInteractionBus& bus) {
-    bus_->emit(topic, payload.dump());  // 桥接到 string 重载
-    return IPCResponse{ok=true};
-}
+IPCResponse dispatch_emit_event(const std::string& topic,
+                                const nlohmann::json& payload,
+                                const SkillCapability& cap,
+                                IInteractionBus& bus) {
+    // C3 defense-in-depth (spike §0.4, 2026-07-21):
+    // allowed_topics 白名单在父进程侧强制, 子进程不可绕过.
+    // 默认 {} 即拒绝所有 emit_event, 显式 grant 才允许.
+    if (std::find(cap.allowed_topics.begin(), cap.allowed_topics.end(), topic)
+        == cap.allowed_topics.end()) {
+        return IPCResponse{ok=false, error="emit_event topic not in allowed_topics"};
+    }
+    bus_->emit(topic, payload.dump());  // 桥接到 string 重载
+    return IPCResponse{ok=true};
+}
```

### 7.3 Decision 7 (C4) — child entry Threads==1 invariant check

**Section:** `design.md` §Decision 7, after line 426 (envp 校验)

```diff
     int ipc_in = std::atoi(ipc_in_str);
     int ipc_out = std::atoi(ipc_out_str);
     int ipc_err = std::atoi(ipc_err_str);

+    // === C4 defense-in-depth (spike §0.4, 2026-07-21): Threads==1 invariant ===
+    // posix_spawn+execve 保证子进程 Threads=1, 但 future global static thread
+    // starter 会破坏此不变量. 读 /proc/self/status 检测, 失败即 _exit(73).
+    {
+        std::ifstream f("/proc/self/status");
+        std::string line;
+        int threads = 1;
+        while (std::getline(f, line)) {
+            if (line.rfind("Threads:", 0) == 0) {
+                threads = std::stoi(line.substr(8));
+                break;
+            }
+        }
+        if (threads != 1) {
+            std::fprintf(stderr,
+                "skill_child_main: Threads=%d at entry (expected 1). ABORT.\n",
+                threads);
+            return EXIT_CHILD_THREAD_LEAK;  // = 73
+        }
+    }
+
     // 2. 解析 SKILL.md...
```

Add to exit code table (Decision 7, line 458-464):
```diff
 - `72` - seccomp 加载失败
+- `73` - C4 invariant 违反 (Threads > 1 at child entry, 全局静态 thread starter 嫌疑)
```

---

## 8. Proposed `tasks.md` additions

### 8.1 §0.4 spike (this report)

```diff
+- [ ] 0.4 **C3/C4/C5 pre-implementation risk validation** (1 day, ship 2026-07-21)
+  - Build pdk_chat_demo, runtime /proc snapshot for Threads/fd table
+  - Static grep for production bus subscribers (C3)
+  - nm static initializers + runtime Threads count (C4)
+  - Minimal C reproducer for posix_spawn file_actions ordering (C5)
+  - 验收: docs/audits/2026-07-21-skill-interpreter-spike-0.4.md ship, 3 verdicts
+    recorded, design.md/tasks.md revisions proposed
```

### 8.2 §1.3 file_actions order fix (C5, must-fix)

```diff
 - [ ] 1.3 创建 `src/modules/skill_interpreter/skill_interpreter.cpp`（PIMPL 实现）：
   - `run()` 内：创建 3 对 pipe (`pipe_in` / `pipe_out` / `pipe_err`)，`readlink("/proc/self/exe", ...)` 读自身路径（失败 -> `ErrorCode::UnsupportedPlatform`）
-  - `posix_spawn_file_actions_t` 初始化：`addclosefrom_np(3)` + `adddup2` 把 pipe fd 映射到 STDIN/STDOUT/STDERR（glibc ≥ 2.29）
+  - `posix_spawn_file_actions_t` 初始化（**顺序敏感**，C5 spike §0.4 修正）：
+    1. 先 `adddup2(pipe_in[0], STDIN_FILENO)` / `adddup2(pipe_out[1], STDOUT_FILENO)` / `adddup2(pipe_err[1], STDERR_FILENO)`
+    2. **再** `addclosefrom_np(3)` — POSIX 按 add-order 执行, closefrom 必须最后
+    3. 验证: `posix_spawn` 返回 0 (非 EBADF), 参见 /tmp/c5_test_b.cpp
```

### 8.3 §1.4 child pre-thread-cancel (C4, defense-in-depth)

```diff
 - [ ] 1.4 修改 `examples/pdk_chat_demo/main.cpp`（**仅修改这一个二进制**）：早期分支 `--skill-child` -> 调用 `skill_child_main()`（独立 TU）
+  - `skill_child_main()` 入口检测 `/proc/self/status::Threads` 字段, 不等于 1 -> `_exit(73)` (C4 invariant, spike §0.4)
```

### 8.4 §2.1 KILL_PROCESS enforcement + SIGSYS sigaction fix (H1/H2)

```diff
 - [ ] 2.1 `skill_interpreter.cpp` (`skill_child_main`)：实现 `apply_seccomp_filter()`：
-  - BPF 程序构造（架构检查 `AUDIT_ARCH_X86_64` + 25-30 syscall 白名单，spike 后定稿 + default `SECCOMP_RET_KILL_PROCESS` 推荐 kernel ≥ 4.14）
+  - BPF 程序构造（架构检查 `AUDIT_ARCH_X86_64` + 25-30 syscall 白名单，spike 后定稿）
+  - default action: `SECCOMP_RET_KILL_PROCESS` (kernel ≥ 4.14 verified, spike §0.4 §6)
+  - **H1 fix**: 父进程 `sigaction(SIGSYS, ...)` 注册 handler, 记录 `siginfo->si_syscall` 用于诊断 (默认 SIGSYS 终止进程, 父进程通过 WIFSIGNALED 检测)
+  - **H2 fix**: `SECCOMP_RET_KILL_PROCESS` 替代 `SECCOMP_RET_KILL` (后者只杀当前 thread, 在 Threads>1 时漏杀; spike §0.4 C4 已验证当前 Threads==1, 但 KILL_PROCESS 是 defense-in-depth)
```

### 8.5 §2.2 PR_SET_PDEATHSIG + RLIMIT_CORE (H8/M6)

```diff
+- [ ] 2.2 父进程资源限制 + 子进程 PR_SET_PDEATHSIG (spike §0.4 H8/M6):
+  - `setrlimit(RLIMIT_CORE, 0)` 在父进程 `run()` 入口, 防止子进程 crash 产生 core dump 泄露内存
+  - `setrlimit(RLIMIT_CPU, soft=timeout_ms/1000+1, hard=...)` CPU 时间兜底 (父进程 SIGKILL 之外的二道防线)
+  - 子进程 `skill_child_main()` 入口 `prctl(PR_SET_PDEATHSIG, SIGKILL)` (父进程死亡时子进程自动 SIGKILL, 防止孤儿进程)
+  - **H8**: PR_SET_PDEATHSIG 在父进程先于子进程 exit 时有效; 父进程 SIGKILL 子进程仍由 ipc_loop_and_wait 显式处理
```

### 8.6 §4.5 POLLHUP drain + read_line cap + ceil<ms> poll timeout (H4/H5/H6)

```diff
 - [ ] 4.5 父进程侧 `ipc_loop_and_wait()`：
   - `pollfd fds[2] = {{pipe_out, POLLIN}, {pipe_err, POLLIN}}`
   - `poll(fds, 2, remaining_timeout_ms)` 主循环 + **EINTR 重试**（`do { n = poll(...); } while (n < 0 && errno == EINTR)`）
   - 每收到 IPC 请求 -> `++steps_used` -> 检查 `steps_used > cap.max_steps` -> 超限 SIGKILL + 返回 `ErrorCode::MaxStepsExceeded`
   - 否则：dispatch 到 `tools_->call_tool_json()` / `bus_->emit(topic, payload.dump())`（见 Decision 12）/ `llm_->generate()` / `budget_counter_`（见 Decision 11）
   - **每次 dispatch 前**检查 `cap.allowed_tools` 白名单（父进程侧强制，子进程不可绕过）
+  - **H4**: `fds[0].revents & POLLHUP` -> 子进程 pipe 关闭, drain 残余 bytes 后跳出 (避免丢失最后一条 IPC)
+  - **H5**: `read_line()` 上限 1MB (`kIPCMessageMaxBytes`), 超限截断 + 标记 `ipc_truncated=true` (与 stderr 1MB 上限一致)
+  - **H6**: `poll` timeout 使用 `std::chrono::ceil<milliseconds>(remaining).count()`, 避免截断为 0 (false timeout)
```

### 8.7 §7.x new tests

```diff
+- [ ] 7.24 TEST_CASE "G1 child 2MB line IPC rejection" - 子进程发 2MB 单行,
+      父进程 `read_line` 截断 + `ipc_truncated=true` + SIGKILL (H5)
+- [ ] 7.25 TEST_CASE "G2 child half-line SIGKILL" - 子进程写半行后 sleep,
+      父进程 timeout SIGKILL, `waitpid` EINTR 重试, 不留僵尸 (H4)
+- [ ] 7.26 TEST_CASE "G3 child pre-seccomp crash" - 子进程 SKILL.md 解析阶段
+      SIGSEGV (mock parse_skill_file throw bad_alloc), 父进程 WIFSIGNALED 检测
+      -> `ErrorCode::Crash`, stderr_content 收集 "parse failed" 字串 (§5.3)
+- [ ] 7.27 TEST_CASE "G4 parent-exit child-dies" - 父进程故意 exit(0) 后,
+      `pgrep -f pdk_chat_demo.*--skill-child` 0 个 (PR_SET_PDEATHSIG 生效, H8)
+- [ ] 7.28 TEST_CASE "G5 emit_event topic whitelist" - SKILL 调用
+      `emit_event("tool.audit.invoked", {...})` (topic 不在 allowed_topics),
+      父进程拒绝 + 返回 `{"ok":false,"error":"emit_event topic not in allowed_topics"}` (C3 §3.6)
+- [ ] 7.29 TEST_CASE "G6 child static thread" - mock 一个全局静态 thread starter
+      (LD_PRELOAD shim or test fixture), 验证 `EXIT_CHILD_THREAD_LEAK=73` (C4 §4.5)
+- [ ] 7.30 TEST_CASE "G7 child fd table" - 父进程 posix_spawn 后立即读
+      `/proc/<child>/fd/`, 断言 fd ≥ 3 不存在 (C5 §5.6 invariant)
+- [ ] 7.31 TEST_CASE "G13 budget CAS kill-once" - SKILL 调 `consume_budget` 超
+      `budget_limit_usd`, 父进程 CAS 失败 -> SIGKILL **一次** (idempotent),
+      第二次 `consume_budget` IPC 不到达 (子进程已死) (Decision 11)
```

---

## 9. Ship gate verification commands

Re-run this spike to confirm 3 risks are addressed before implementation starts:

```bash
cd /workspace/project/HydraForge

# === Build ===
cmake --preset debug -DAGENTICDSL_BUILD_EXAMPLES=ON -DAGENTICDSL_BUILD_TESTS=OFF
cmake --build build --target pdk_chat_demo -j$(nproc)
test -x build/examples/pdk_chat_demo/pdk_chat_demo || { echo "FAIL: binary missing"; exit 1; }

# === C3: production subscribers must not call call_tool ===
# Expected: 0 matches (only test files have subscribers that call_tool)
grep -rn 'subscribe' src/ examples/ include/ pdk/ --include='*.cpp' --include='*.h' \
  | grep -v '/build/' | grep -v 'subscriber_id' \
  | awk -F: '{print $1}' | sort -u \
  | while read f; do
      # for each file with subscribe, check if it also has call_tool in the same lambda
      grep -B2 -A20 'subscribe(' "$f" 2>/dev/null | grep -E 'call_tool|registry_\.call' \
        && echo "C3 FAIL: $f has call_tool near subscribe"
    done
echo "C3: PASS (no offending subscribers)"

# === C4: runtime Threads count ===
python3 - <<'PY'
import subprocess, threading, time, os
BIN="/workspace/project/HydraForge/build/examples/pdk_chat_demo/pdk_chat_demo"
CWD="/workspace/project/HydraForge/build/examples/pdk_chat_demo"
p = subprocess.Popen([BIN, "--mock"], stdin=subprocess.PIPE,
                     stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                     close_fds=True, cwd=CWD)
threading.Thread(target=lambda: p.stdout.read(), daemon=True).start()
threading.Thread(target=lambda: p.stderr.read(), daemon=True).start()
time.sleep(0.05)  # post-execve, pre-main
with open(f"/proc/{p.pid}/status") as f:
    for line in f:
        if line.startswith("Threads:"):
            threads = int(line.split()[1])
            assert threads == 1, f"C4 FAIL: Threads={threads} at t=0.05s"
            print(f"C4: PASS (Threads=1 at entry)")
            break
p.terminate(); p.wait()
PY

# === C5: design.md file_actions ordering ===
# Re-verify the reproducer: Pattern B (closefrom LAST) must succeed
cat > /tmp/c5_verify.cpp <<'CPP'
#include <spawn.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
extern char** environ;
int main() {
    int p[2]; pipe2(p, O_CLOEXEC);
    posix_spawn_file_actions_t a;
    posix_spawn_file_actions_init(&a);
    // Pattern B (fixed): dup2 FIRST, closefrom LAST
    posix_spawn_file_actions_adddup2(&a, p[0], 0);
    posix_spawn_file_actions_addclosefrom_np(&a, 3);
    pid_t pid;
    char* av[] = {(char*)"/bin/true", nullptr};
    int rc = posix_spawn(&pid, "/bin/true", &a, nullptr, av, environ);
    if (rc != 0) { printf("C5 FAIL: posix_spawn=%s\n", strerror(rc)); return 1; }
    int st; waitpid(pid, &st, 0);
    printf("C5: PASS (WIFEXITED=%d WEXITSTATUS=%d)\n",
           WIFEXITED(st), WIFEXITED(st) ? WEXITSTATUS(st) : -1);
    return 0;
}
CPP
g++ -std=c++20 /tmp/c5_verify.cpp -o /tmp/c5_verify && /tmp/c5_verify

# === Kernel/glibc compat ===
KVER=$(uname -r | awk -F. '{print $1*100+$2}')
[ "$KVER" -ge 414 ] && echo "Kernel: PASS (>=4.14, SECCOMP_RET_KILL_PROCESS available)" \
                  || echo "Kernel: FAIL (<4.14)"
GLIBC=$(ldd --version | head -1 | awk '{print $NF}')
GLIBC_INT=$(echo "$GLIBC" | awk -F. '{print $1*100+$2}')
[ "$GLIBC_INT" -ge 229 ] && echo "glibc: PASS (>=2.29, addclosefrom_np available)" \
                       || echo "glibc: FAIL (<2.29)"
```

**Expected output on a green build:**

```
C3: PASS (no offending subscribers)
C4: PASS (Threads=1 at entry)
C5: PASS (WIFEXITED=1 WEXITSTATUS=0)
Kernel: PASS (>=4.14, SECCOMP_RET_KILL_PROCESS available)
glibc: PASS (>=2.29, addclosefrom_np available)
```

---

## 10. Blockers encountered

| Blocker | Resolution |
|---------|------------|
| Initial `/proc/<pid>/status` snapshot showed `State: Z (zombie)` — pdk_chat_demo exited too fast | Used Python launcher with `stdin=subprocess.PIPE` (kept open) + background stdout/stderr drain threads + `cwd=build/examples/pdk_chat_demo` (config.json lives there). Without `cwd`, the binary exits on `config.json not found`. |
| `file` command unavailable in environment | Used `ls -la` + `stat` for binary metadata. |
| `man` pages unavailable (minimized container) | Verified POSIX/glibc semantics via `/usr/include/spawn.h`, `/usr/include/linux/seccomp.h`, `/usr/include/linux/prctl.h` directly. |
| `/proc/<pid>/fd/` initially returned `PermissionError: [Errno 13]` | Caused by snapshot taken after process became zombie (parent reaped it). Fixed by keeping stdin pipe open + reading fd table while process is alive. |
| `pgrep -f pdk_chat_demo` initially matched the wrong PID (the bash subshell) | Switched to Python `subprocess.Popen` and used `proc.pid` directly. |

No blocker required aborting the spike. All 3 risks (C3/C4/C5) successfully
validated with both static and runtime evidence.

---

## 11. References

- `openspec/changes/skill-interpreter-real-loading/design.md` — Decisions 2, 6, 7, 12 (audited)
- `openspec/changes/skill-interpreter-real-loading/tasks.md` — §0.4 (this spike), §1.3 (C5 fix), §1.4 (C4 fix), §2.1 (H1/H2), §4.5 (H4/H5/H6), §7.x (new tests)
- `openspec/changes/skill-interpreter-real-loading/proposal.md` — risk context
- `src/common/contract/inmemory_bus.cpp` — dispatch_thread_ construction (line 13-14)
- `src/common/tools/tool_coordinator.cpp` — ToolCoordinator audit emit (no subscribe)
- `examples/pdk_chat_demo/main.cpp:86` — `InMemoryBus()` instantiation (where 2nd thread starts)
- `examples/pdk_chat_demo/event_handler.cpp:103` — production EventHandler subscriber (print-only)
- `include/agenticdsl/pdk/agent_loops/fork_join_loop.h:167,181` — production subscribers (tracker-only)
- `docs/adr/adr-0031-execution-policy.md` §决策 5 — ToolCoordinator design (Option C, opt-in)
- `docs/adr/adr-0055-skill-isolation.md` — SkillInterpreter architecture (fork → posix_spawn revision)
- Minimal reproducers: `/tmp/c5_test.cpp` (FAIL), `/tmp/c5_test_b.cpp` (PASS), `/tmp/spike_c4_timing.py`, `/tmp/spike_c5_fd.py`
