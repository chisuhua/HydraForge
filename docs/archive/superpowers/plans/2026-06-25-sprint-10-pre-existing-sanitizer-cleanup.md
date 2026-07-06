# Sprint 10 Pre-existing Sanitizer Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix `test_cognitive_worker` ASan stack-use-after-scope and `test_domain_worker_pool` TSan 12 warnings so that `ctest --preset asan` and `ctest --preset tsan` both pass 34/34, then close Sprint 10 by archiving OpenSpec change `2026-06-25-pre-existing-sanitizer-findings` and updating all governance docs.

**Architecture:** Three-phase serial execution: (1) replace `std::thread` with `std::jthread` and harden synchronization in `tests/test_cognitive_worker.cpp`; (2) investigate `test_domain_worker_pool` TSan warnings, classify root cause into Catch2-framework vs product-code buckets, then implement the matching fix; (3) update `docs/audits/2026-06-25-sanitizer-revalidation.md`, `docs/roadmap-status.md`, and `AGENTS.md`, run final ship gate, and archive the OpenSpec change.

**Tech Stack:** C++20, CMake 3.20+, Catch2 (amalgamated), GCC-13, AddressSanitizer, ThreadSanitizer, OpenSpec CLI, clang-format.

---

## File Structure

| File | Role | Expected Change |
|---|---|---|
| `tests/test_cognitive_worker.cpp` | Unit test for `CognitiveWorker` | Replace `std::vector<std::thread>` with `std::vector<std::jthread>` in TEST_CASE 5; remove explicit join loop; verify ASan/TSan pass |
| `tests/test_domain_worker_pool.cpp` | Unit test for `DomainWorkerPool` | Depending on P2.1 classification: replace `std::vector<std::thread>` with `std::vector<std::jthread>` and/or isolate `1000x concurrent submit` in a `SECTION` with explicit stop/join |
| `src/modules/cognitive/domain_worker_pool.cpp` | Product implementation | Only modified if P2.1 reveals a product-code race (Task 2.3c) |
| `docs/audits/2026-06-25-sanitizer-revalidation.md` | Ship gate audit report | Append a new subsection documenting P2 TSan investigation and fix |
| `docs/audits/p2-tsan-investigation.md` | P2.1 raw investigation log | Created during P2.1; referenced from audit report |
| `docs/roadmap-status.md` | Sprint progress board | Update §ASan 验证 and §TSan 验证 tables with final 34/34 numbers |
| `AGENTS.md` | Project knowledge base | Append Sprint 10 ship entry under §Recent Changes |

---

## Pre-Flight: Baseline Verification

### Task 0.1: Confirm clean working tree and green baseline

**Files:** none

- [ ] **Step 1: Check git status**

Run:
```bash
cd /workspace/project/HydraForge
git status --short
```

Expected output: empty (nothing uncommitted).

- [ ] **Step 2: Build and run baseline tests**

Run:
```bash
cmake --preset release -DAGENTICDSL_BUILD_TESTS=ON
--build build --target agenticdsl_tests
ctest --test-dir build --output-on-failure
```

Expected output:
```
Total Test time (real) = ...
100% tests passed, 0 tests failed out of 34
```

- [ ] **Step 3: Record baseline commit hash**

Run:
```bash
git log --oneline -1
```

Record the hash. If any later step corrupts the tree, reset to this commit with:
```bash
git reset --hard <baseline-hash>
```

---

## Phase 1: Fix `test_cognitive_worker` ASan + TSan

### Task 1.1: Reproduce ASan failure

**Files:** `tests/test_cognitive_worker.cpp`

- [ ] **Step 1: Build with ASan and run the failing test**

Run:
```bash
cmake --preset asan -DAGENTICDSL_BUILD_TESTS=ON
--build build/asan --target agenticdsl_tests
ctest --test-dir build/asan -R test_cognitive_worker --output-on-failure
```

Expected output: `CognitiveWorker concurrent submit 10x100 TSan clean` fails with `stack-use-after-scope` at `test_cognitive_worker.cpp:226`.

- [ ] **Step 2: Capture the full ASan report**

Run:
```bash
ctest --test-dir build/asan -R test_cognitive_worker --output-on-failure 2>&1 | tee /tmp/cognitive_worker_asan.log
```

Expected: `/tmp/cognitive_worker_asan.log` contains `ERROR: AddressSanitizer: stack-use-after-scope`.

### Task 1.2: Replace `std::thread` with `std::jthread` in TEST_CASE 5

**Files:** `tests/test_cognitive_worker.cpp`

- [ ] **Step 1: Read the current TEST_CASE 5**

Run:
```bash
sed -n '206,237p' tests/test_cognitive_worker.cpp
```

Expected output shows lines 222-231:
```cpp
std::vector<std::thread> threads;
for (int i = 0; i < 10; ++i) {
  threads.emplace_back([&] { ... });
}
for (auto& t : threads) t.join();
```

- [ ] **Step 2: Add `#include <thread>` replacement — ensure `std::jthread` is available**

`std::jthread` is in `<thread>`. The file already uses `std::thread`, so `<thread>` is already transitively included; no header change needed.

- [ ] **Step 3: Replace the thread vector and join loop**

Edit `tests/test_cognitive_worker.cpp:222-231` from:
```cpp
std::vector<std::thread> threads;
for (int i = 0; i < 10; ++i) {
  threads.emplace_back([&] {
    for (int j = 0; j < 100; ++j) {
      worker.submit_task("t-" + std::to_string(i) + "-" + std::to_string(j),
                        "p");
    }
  });
}
for (auto& t : threads) t.join();
```

to:
```cpp
std::vector<std::jthread> threads;
for (int i = 0; i < 10; ++i) {
  threads.emplace_back([&] {
    for (int j = 0; j < 100; ++j) {
      worker.submit_task("t-" + std::to_string(i) + "-" + std::to_string(j),
                        "p");
    }
  });
}
// std::jthread RAII auto-joins on destruction
```

- [ ] **Step 4: Harden `wait_until` so worker events drain before stop**

Edit `tests/test_cognitive_worker.cpp:233-236` from:
```cpp
wait_until([&] { return completed_count.load() == 1000; });
worker.stop();

REQUIRE(completed_count.load() == 1000);
```

to:
```cpp
wait_until([&] { return completed_count.load() == 1000; },
           std::chrono::seconds(30));
worker.stop();
wait_until([&] { return worker.state() == CognitiveWorker::State::stopped; },
           std::chrono::seconds(5));

REQUIRE(completed_count.load() == 1000);
```

Note: only add the extra `wait_until` if `CognitiveWorker::state()` is public. If it is not, replace with a short sleep after `worker.stop()`:
```cpp
worker.stop();
std::this_thread::sleep_for(std::chrono::milliseconds(50));
```

- [ ] **Step 5: Verify the edit with sed**

Run:
```bash
sed -n '206,242p' tests/test_cognitive_worker.cpp
```

Expected: lines show `std::vector<std::jthread> threads;` and removed explicit `join()` loop.

### Task 1.3: Verify P1 fix

**Files:** `tests/test_cognitive_worker.cpp`

- [ ] **Step 1: Build and run baseline test for `test_cognitive_worker`**

Run:
```bash
cmake --preset release -DAGENTICDSL_BUILD_TESTS=ON
--build build --target agenticdsl_tests
ctest --test-dir build -R test_cognitive_worker --output-on-failure
```

Expected output: all 9 TEST_CASEs pass.

- [ ] **Step 2: Run ASan on `test_cognitive_worker`**

Run:
```bash
cmake --preset asan -DAGENTICDSL_BUILD_TESTS=ON
--build build/asan --target agenticdsl_tests
ctest --test-dir build/asan -R test_cognitive_worker --output-on-failure
```

Expected output: 0 AddressSanitizer errors.

- [ ] **Step 3: Run TSan on `test_cognitive_worker`**

Run:
```bash
cmake --preset tsan -DAGENTICDSL_BUILD_TESTS=ON
--build build/tsan --target agenticdsl_tests
ctest --test-dir build/tsan -R test_cognitive_worker --output-on-failure
```

Expected output: 0 ThreadSanitizer warnings.

### Task 1.4: Commit P1 fix

**Files:** `tests/test_cognitive_worker.cpp`

- [ ] **Step 1: Stage and commit**

Run:
```bash
cd /workspace/project/HydraForge
git add tests/test_cognitive_worker.cpp
git commit -m "test(cognitive): fix stack-use-after-scope in concurrent submit test

Sprint 10 P1: replace std::thread with std::jthread in
'CognitiveWorker concurrent submit 10x100 TSan clean'.

std::jthread RAII auto-joins on destruction, eliminating the race
between submitter threads and worker.stop()/worker destructor that
AddressSanitizer reported as stack-use-after-scope at line 226.

Also harden wait_until timeout and add small post-stop drain to ensure
all cognitive.task.completed events are observed before test scope
exits.

Verification:
- ctest baseline: 34/34 PASS
- ctest --preset asan -R test_cognitive_worker: 0 errors
- ctest --preset tsan -R test_cognitive_worker: 0 warnings"
```

Expected output: commit created, no uncommitted changes.

- [ ] **Step 2: Record the new baseline**

Run:
```bash
git log --oneline -1
```

Record this commit hash as the Phase 1 green state.

---

## Phase 2: Fix `test_domain_worker_pool` TSan

### Task 2.1: TSan stack investigation

**Files:** `tests/test_domain_worker_pool.cpp`, `docs/audits/p2-tsan-investigation.md` (create)

- [ ] **Step 1: Build TSan and run only `test_domain_worker_pool`**

Run:
```bash
cmake --preset tsan -DAGENTICDSL_BUILD_TESTS=ON
--build build/tsan --target agenticdsl_tests
ctest --test-dir build/tsan -R test_domain_worker_pool --output-on-failure 2>&1 | tee /tmp/domain_worker_pool_tsan.log
```

Expected output: log contains 12 ThreadSanitizer warnings.

- [ ] **Step 2: Re-run with `--track-origins=yes` for richer reports**

Run:
```bash
cd build/tsan
TSAN_OPTIONS="halt_on_error=0 track_origins=1" ctest -R test_domain_worker_pool --output-on-failure 2>&1 | tee /tmp/domain_worker_pool_tsan_track.log
```

Expected output: `/tmp/domain_worker_pool_tsan_track.log` contains detailed read/write origin stacks.

- [ ] **Step 3: Bucket the 12 warnings**

Parse `/tmp/domain_worker_pool_tsan_track.log` and classify each warning into one of three buckets by looking at the top non-Catch2 frames:

| Bucket | Pattern | Example frame |
|---|---|---|
| **B1: Catch2 framework race** | `Catch::RunContext::resetAssertionInfo` or `Catch::RunContext::invokeActiveTestCase` | catch_amalgamated.cpp |
| **B2: DomainWorkerPool product race** | `agenticdsl::DomainWorkerPool::process_task`, `submit_task`, `worker_thread`, or any `domain_worker_pool.cpp` frame | src/modules/cognitive/domain_worker_pool.cpp |
| **B3: Other** | bus subscribe, atomic op, or third-party frame | any other file |

Record counts for each bucket.

- [ ] **Step 4: Create `docs/audits/p2-tsan-investigation.md`**

Write:
```markdown
# DomainWorkerPool TSan Investigation (Sprint 10 P2.1)

## Run 1: ctest -R test_domain_worker_pool

```text
[paste /tmp/domain_worker_pool_tsan.log here]
```

## Run 2: track_origins=1

```text
[paste /tmp/domain_worker_pool_tsan_track.log here]
```

## Bucket Summary

| Bucket | Count | Representative frame | Location |
|---|---|---|---|
| B1 Catch2 framework race | X | `Catch::RunContext::resetAssertionInfo` | tests/test_domain_worker_pool.cpp:??? |
| B2 DomainWorkerPool product race | Y | [frame] | src/modules/cognitive/domain_worker_pool.cpp:??? |
| B3 Other | Z | [frame] | [file]:[line] |

## Decision

- If B1 > 50% of total warnings: proceed with Task 2.3a (test isolation).
- If B2 > 30% of total warnings: proceed with Task 2.3c (product code fix).
- Otherwise: proceed with Task 2.3a + 2.3b combination.

## Decision made

[To be filled by Task 2.2]
```

- [ ] **Step 5: Run baseline test to confirm no regressions from P1 commit**

Run:
```bash
ctest --test-dir build --output-on-failure
```

Expected: 34/34 PASS.

### Task 2.2: Choose P2 fix strategy

**Files:** `docs/audits/p2-tsan-investigation.md`

- [ ] **Step 1: Decide based on bucket counts**

Open `docs/audits/p2-tsan-investigation.md` and complete the "Decision made" section.

Decision rules:
- If **B1 dominates** (6 or more of 12 warnings): choose **Strategy A** — Task 2.3a (`jthread` submitter + `SECTION` isolation).
- If **B2 dominates** (4 or more of 12 warnings): choose **Strategy C** — Task 2.3c (product code race fix).
- If **mixed**: choose **Strategy A + B** — Task 2.3a then Task 2.3b (stronger isolation).

- [ ] **Step 2: Mark tasks.md §2.1 and §2.2 as completed**

Open `openspec/changes/2026-06-25-pre-existing-sanitizer-findings/tasks.md` and change all checklist items under `## 2.1 诊断` and `## 2.2 修复策略选择` from `- [ ]` to `- [x]`.

Also update the heading for §3 to reflect the new "fix, not just document" mandate if Strategy A or B is chosen later:

```markdown
## 3. P2 修复 `test_domain_worker_pool` TSan warnings (~30 min to 3h)
```

### Task 2.3a: Strategy A — test isolation and `jthread` submitter

**Files:** `tests/test_domain_worker_pool.cpp`

Use this strategy when the TSan warnings originate from Catch2 framework interaction with background `jthread` workers.

- [ ] **Step 1: Read current TEST_CASE 3**

Run:
```bash
sed -n '158,202p' tests/test_domain_worker_pool.cpp
```

- [ ] **Step 2: Replace `std::thread` submitter with `std::jthread`**

Edit `tests/test_domain_worker_pool.cpp:176-191` from:
```cpp
std::vector<std::thread> submitter_threads;
for (int t = 0; t < kNumThreads; ++t) {
  submitter_threads.emplace_back([&pool, t] {
    for (int i = 0; i < kTasksPerThread; ++i) {
      DomainTask task;
      task.domain = "code";
      task.tool_name = "code::noop";
      task.arguments = nlohmann::json{{"id", t * kTasksPerThread + i}};
      task.output_key = "out";
      pool.submit_task(std::move(task));
    }
  });
}
for (auto& t : submitter_threads) {
  t.join();
}
```

to:
```cpp
std::vector<std::jthread> submitter_threads;
for (int t = 0; t < kNumThreads; ++t) {
  submitter_threads.emplace_back([&pool, t] {
    for (int i = 0; i < kTasksPerThread; ++i) {
      DomainTask task;
      task.domain = "code";
      task.tool_name = "code::noop";
      task.arguments = nlohmann::json{{"id", t * kTasksPerThread + i}};
      task.output_key = "out";
      pool.submit_task(std::move(task));
    }
  });
}
// std::jthread RAII auto-joins before pool.stop()
```

- [ ] **Step 3: Wrap concurrent body in an inner `SECTION` and ensure strict stop/join**

Keep the test case named `DomainWorkerPool 1000x concurrent submit TSan clean`. Add an inner `SECTION` so Catch2 scope management is isolated:

Edit lines 158-202 to look like:
```cpp
TEST_CASE("DomainWorkerPool 1000x concurrent submit TSan clean",
          "[domain_worker_pool][sprint3][concurrent]") {
  SECTION("isolated concurrent submit") {
    DomainWorkerPool pool(4);
    std::atomic<int> handler_count{0};

    pool.register_domain_handler(
        "code", [&handler_count](const DomainTask& task) -> nlohmann::json {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
          handler_count.fetch_add(1, std::memory_order_relaxed);
          return nlohmann::json{{"id", task.arguments["id"]}};
        });

    pool.start();

    constexpr int kNumThreads = 10;
    constexpr int kTasksPerThread = 100;
    std::vector<std::jthread> submitter_threads;
    for (int t = 0; t < kNumThreads; ++t) {
      submitter_threads.emplace_back([&pool, t] {
        for (int i = 0; i < kTasksPerThread; ++i) {
          DomainTask task;
          task.domain = "code";
          task.tool_name = "code::noop";
          task.arguments = nlohmann::json{{"id", t * kTasksPerThread + i}};
          task.output_key = "out";
          pool.submit_task(std::move(task));
        }
      });
    }
    // jthreads auto-join here

    wait_until([&] {
      return handler_count.load() >= kNumThreads * kTasksPerThread;
    });

    REQUIRE(handler_count.load() == kNumThreads * kTasksPerThread);
    REQUIRE(pool.state() == DomainWorkerPool::State::running);

    pool.stop();
    wait_until([&] { return pool.state() == DomainWorkerPool::State::stopped; });
  }
}
```

If `DomainWorkerPool::state()` is public. If not, keep the original `REQUIRE(pool.state() == ...)` only if it already compiles; otherwise remove the state assertions.

- [ ] **Step 4: Run TSan on `test_domain_worker_pool`**

Run:
```bash
--build build/tsan --target agenticdsl_tests
ctest --test-dir build/tsan -R test_domain_worker_pool --output-on-failure 2>&1 | tee /tmp/domain_worker_pool_tsan_after_a.log
```

Expected: 0 ThreadSanitizer warnings.

### Task 2.3b: Strategy B — stronger isolation with explicit synchronization

**Files:** `tests/test_domain_worker_pool.cpp`

Use this strategy if Strategy A alone still leaves warnings, or if bucket analysis shows the race is between the `DomainWorkerPool` destructor and Catch2's assertion reset.

- [ ] **Step 1: Add an explicit post-stop yield before SECTION exit**

Edit `tests/test_domain_worker_pool.cpp` inside the SECTION from Task 2.3a so that after `pool.stop()` you also wait for the worker threads to finish and yield:

```cpp
pool.stop();
wait_until([&] { return pool.state() == DomainWorkerPool::State::stopped; });
// Allow any lingering TSan thread-local state to settle before Catch2
// resets assertion info.
std::this_thread::sleep_for(std::chrono::milliseconds(100));
```

- [ ] **Step 2: Run TSan again**

Run:
```bash
--build build/tsan --target agenticdsl_tests
ctest --test-dir build/tsan -R test_domain_worker_pool --output-on-failure 2>&1 | tee /tmp/domain_worker_pool_tsan_after_b.log
```

Expected: 0 ThreadSanitizer warnings.

### Task 2.3c: Strategy C — product code race fix

**Files:** `src/modules/cognitive/domain_worker_pool.cpp` (and possibly `include/agenticdsl/cognitive/domain_worker_pool.h`)

Use this strategy only if Task 2.1 bucket analysis shows 4 or more warnings originate from `domain_worker_pool.cpp` frames. Do **not** implement Task 2.3a or 2.3b if Strategy C is chosen.

- [ ] **Step 1: Capture product-code race frames**

From `/tmp/domain_worker_pool_tsan_track.log`, extract every stack frame that references `src/modules/cognitive/domain_worker_pool.cpp` or `include/agenticdsl/cognitive/domain_worker_pool.h`. Paste them into `docs/audits/p2-tsan-investigation.md` under a new section `## Product-code race frames`.

- [ ] **Step 2: Load systematic-debugging skill**

Run:
```bash
# This is an agent instruction, not a shell command. The worker should invoke:
# skill(name="systematic-debugging")
```

- [ ] **Step 3: Consult Oracle for product code race analysis**

Run:
```bash
# Agent instruction: invoke Oracle background task with prompt:
# "Analyze TSan warnings from /tmp/domain_worker_pool_tsan_track.log
# involving DomainWorkerPool product code. Propose minimal fix in
# src/modules/cognitive/domain_worker_pool.cpp. Wait for result."
```

- [ ] **Step 4: Implement the Oracle-approved fix**

Apply the exact code change recommended by Oracle. Do not improvise.

- [ ] **Step 5: Verify TSan**

Run:
```bash
--build build/tsan --target agenticdsl_tests
ctest --test-dir build/tsan -R test_domain_worker_pool --output-on-failure 2>&1 | tee /tmp/domain_worker_pool_tsan_after_c.log
```

Expected: 0 ThreadSanitizer warnings.

### Task 2.4: Verify full P2 ship gate and commit

**Files:** depends on strategy chosen

- [ ] **Step 1: Confirm strategy chosen**

If Task 2.2 chose Strategy A: complete Task 2.3a and skip 2.3b/2.3c.
If Task 2.2 chose Strategy A+B: complete Task 2.3a then 2.3b.
If Task 2.2 chose Strategy C: skip 2.3a/2.3b, complete 2.3c.

- [ ] **Step 2: Run full TSan suite**

Run:
```bash
ctest --test-dir build/tsan --output-on-failure 2>&1 | tee /tmp/full_tsan_after_p2.log
```

Expected output: 34/34 PASS, 0 ThreadSanitizer warnings.

- [ ] **Step 3: Run full ASan suite**

Run:
```bash
ctest --test-dir build/asan --output-on-failure 2>&1 | tee /tmp/full_asan_after_p2.log
```

Expected output: 34/34 PASS, 0 AddressSanitizer errors.

- [ ] **Step 4: Commit P2 fix**

Run:
```bash
git add tests/test_domain_worker_pool.cpp
if [ -f src/modules/cognitive/domain_worker_pool.cpp ]; then git add src/modules/cognitive/domain_worker_pool.cpp; fi
if [ -f include/agenticdsl/cognitive/domain_worker_pool.h ]; then git add include/agenticdsl/cognitive/domain_worker_pool.h; fi
git add docs/audits/p2-tsan-investigation.md
git commit -m "test(cognitive): fix TSan warnings in domain_worker_pool concurrent test

Sprint 10 P2: eliminate 12 ThreadSanitizer warnings from
'DomainWorkerPool 1000x concurrent submit TSan clean'.

Investigation: docs/audits/p2-tsan-investigation.md
Fix strategy: [A/B/C]

Verification:
- ctest baseline: 34/34 PASS
- ctest --preset asan: 34/34 PASS, 0 errors
- ctest --preset tsan: 34/34 PASS, 0 warnings"
```

- [ ] **Step 5: Mark tasks.md §2.2 and §2.3 as completed**

Open `openspec/changes/2026-06-25-pre-existing-sanitizer-findings/tasks.md` and change all checklist items under `## 2.2 修复策略选择` and `## 2.3 TDD 实施` from `- [ ]` to `- [x]`.

---

## Phase 3: Documentation Sync, Ship Gate, and Archive

### Task 3.1: Enhance audit report

**Files:** `docs/audits/2026-06-25-sanitizer-revalidation.md`, `docs/audits/p2-tsan-investigation.md`

- [ ] **Step 1: Append P2 TSan fix section**

Edit `docs/audits/2026-06-25-sanitizer-revalidation.md` and add a new section before the final ship gate decision:

```markdown
## P2 TSan 修复详情

### 调查

P2.1 调查: `docs/audits/p2-tsan-investigation.md`

12 warnings 分桶:
- B1 Catch2 framework race: X
- B2 DomainWorkerPool product race: Y
- B3 Other: Z

### 修复策略

选择策略 [A/B/C]:
- 策略 A: `std::jthread` submitter + `SECTION` 隔离 (test-only)
- 策略 B: 额外 `pool.stop()` 后 yield (test-only)
- 策略 C: 产品代码 race fix (domain_worker_pool.cpp)

### 修改文件

- `tests/test_domain_worker_pool.cpp` (全部策略)
- `src/modules/cognitive/domain_worker_pool.cpp` (策略 C 时)

### 验证

- `ctest --preset asan`: 34/34 PASS
- `ctest --preset tsan`: 34/34 PASS
- `docs/audits/p2-tsan-investigation.md`: 完整调查记录
```

- [ ] **Step 2: Update ship gate summary**

In the same file, update the final ship gate summary to:
```markdown
| 项目 | 状态 |
|---|---|
| ctest baseline | 34/34 PASS |
| ASan | 34/34 PASS |
| TSan | 34/34 PASS |
| pre-existing remaining | 0 |
| Sprint 10 ship | ✅ PASS |
```

### Task 3.2: Update roadmap status

**Files:** `docs/roadmap-status.md`

- [ ] **Step 1: Locate the ASan/TSan verification tables**

Run:
```bash
grep -n "ASan 验证\|TSan 验证\|2026-06-25" docs/roadmap-status.md | head -20
```

- [ ] **Step 2: Append the Sprint 10 final row**

Edit `docs/roadmap-status.md` to add a row under both tables (or a combined section if that is the current format):

```markdown
| 日期 | 变更 | 测试数 | 通过 | 失败 | 来源 / 备注 |
|---|---|---|---|---|---|
| 2026-06-25 | Sprint 10 P2.5 复验 | 34 | 33 | 1 | pre-existing tracked |
| 2026-06-25 | Sprint 10 P1+P2 修复后 | 34 | 34 | 0 | pre-existing 清零 |
```

Match the exact table style already present in the file.

### Task 3.3: Update AGENTS.md

**Files:** `AGENTS.md`

- [ ] **Step 1: Append Sprint 10 ship entry**

Edit `AGENTS.md` §Recent Changes to add:

```markdown
- 2026-06-25 (Sprint 10 ship): OpenSpec change `2026-06-25-pre-existing-sanitizer-findings` ship.
  - 修复 `test_cognitive_worker` ASan `stack-use-after-scope` (std::thread → std::jthread)
  - 修复 `test_domain_worker_pool` 12 TSan warnings ([strategy A/B/C])
  - `cmake --preset asan -DAGENTICDSL_BUILD_TESTS=ON && ctest` 34/34 PASS
  - `cmake --preset tsan -DAGENTICDSL_BUILD_TESTS=ON && ctest` 34/34 PASS
  - Sprint 10 起点 1 active change → archive 后 0 active change
  - 更新 `docs/audits/2026-06-25-sanitizer-revalidation.md` + `docs/roadmap-status.md`
```

### Task 3.4: Final ship gate and OpenSpec archive

**Files:** all modified docs

- [ ] **Step 1: Run final baseline ship gate**

Run:
```bash
--build build --target agenticdsl_tests
ctest --test-dir build --output-on-failure
```

Expected: 34/34 PASS.

- [ ] **Step 2: Run final ASan ship gate**

Run:
```bash
--build build/asan --target agenticdsl_tests
ctest --test-dir build/asan --output-on-failure
```

Expected: 34/34 PASS, 0 AddressSanitizer errors.

- [ ] **Step 3: Run final TSan ship gate**

Run:
```bash
--build build/tsan --target agenticdsl_tests
ctest --test-dir build/tsan --output-on-failure
```

Expected: 34/34 PASS, 0 ThreadSanitizer warnings.

- [ ] **Step 4: Verify clean git status**

Run:
```bash
git status --short
```

Expected: empty.

- [ ] **Step 5: Mark tasks.md §2-§5 as completed**

Open `openspec/changes/2026-06-25-pre-existing-sanitizer-findings/tasks.md` and change all remaining `- [ ]` items under `## 2. ...`, `## 3. ...`, `## 4. ...`, and `## 5. ...` to `- [x]`.

If Strategy A or B was chosen for P2, ensure the §3 heading reads:
```markdown
## 3. P2 修复 `test_domain_worker_pool` TSan warnings
```

Verify the checklist is fully checked before archive:

Run:
```bash
grep -E "^- \[ \]" openspec/changes/2026-06-25-pre-existing-sanitizer-findings/tasks.md | wc -l
```

Expected output: `0`.

- [ ] **Step 6: Commit all Phase 3 docs**

Run:
```bash
git add docs/audits/2026-06-25-sanitizer-revalidation.md
if [ -f docs/audits/p2-tsan-investigation.md ]; then git add docs/audits/p2-tsan-investigation.md; fi
git add docs/roadmap-status.md
git add AGENTS.md
git commit -m "docs(ship-gate): Sprint 10 pre-existing sanitizer cleanup closeout

- docs/audits/2026-06-25-sanitizer-revalidation.md: P2 TSan fix details
- docs/audits/p2-tsan-investigation.md: TSan warning buckets
- docs/roadmap-status.md: 2026-06-25 34/34 ASan/TSan rows
- AGENTS.md: Sprint 10 ship entry

Ship gate:
- ctest baseline 34/34 PASS
- ASan 34/34 PASS
- TSan 34/34 PASS
- 0 pre-existing remaining"
```

- [ ] **Step 7: Archive the OpenSpec change**

Run:
```bash
openspec archive 2026-06-25-pre-existing-sanitizer-findings
openspec list
```

Expected output of `openspec list`: 0 active changes.

- [ ] **Step 8: Final git log check**

Run:
```bash
git log --oneline -5
```

Expected: Phase 1 commit, Phase 2 commit, Phase 3 docs commit at the top.

---

## Self-Review Checklist (for plan author)

- [ ] **Spec coverage:** Every section of `docs/superpowers/specs/2026-06-25-sprint-10-pre-existing-sanitizer-cleanup-design.md` maps to tasks.
  - §2 architecture → Tasks 0.1, 1.1-1.4, 2.1-2.4, 3.1-3.4
  - §3 data flow → Tasks 1.2, 2.1, 2.2, 2.3a/2.3b/2.3c
  - §4 error handling → Task 1.4 rollback note, Task 2.3c Oracle fallback
  - §5 testing → Tasks 1.3, 2.4, 3.4
- [ ] **Placeholder scan:** No "TBD", "TODO", "implement later", "similar to Task N", or vague requirements.
- [ ] **Type consistency:** `std::jthread` used consistently; `wait_until` signature unchanged.
- [ ] **Exact file paths:** All paths verified against current repo.
- [ ] **Branching plan:** P2 strategy branches are explicit and each has concrete code/commands.

## Execution Handoff

**Plan complete and saved to `docs/superpowers/plans/2026-06-25-sprint-10-pre-existing-sanitizer-cleanup.md`.**

Two execution options:

**1. Subagent-Driven (recommended)** — Dispatch a fresh subagent per task, review between tasks, fast iteration.
- **REQUIRED SUB-SKILL:** Use `superpowers:subagent-driven-development`.

**2. Inline Execution** — Execute tasks in this session using `superpowers:executing-plans`, batch execution with checkpoints.

Which approach?
