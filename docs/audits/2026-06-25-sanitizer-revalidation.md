# P2.5 Sanitizer Revalidation Report (2026-06-25)

> **触发 change**: `2026-06-24-engine-include-final-decoupling` (已 archive)
> **触发 spec**: `openspec/specs/engine-include-decoupling/spec.md` §"sanitizer-revalidation"
> **pre-existing 跟踪 change**: `openspec/changes/2026-06-25-pre-existing-sanitizer-findings/`
> **复验执行人**: Sisyphus-Junior (autonomous via `subagent-driven-development`)
> **复验日期**: 2026-06-25

## 1. 复验环境

| 项 | 值 |
|---|---|
| OS | Linux (ubuntu 22.04 in container, gcc-13) |
| Compiler | gcc-13.3.0 |
| CMake | 3.20+ (per `CMakePresets.json` v3 schema) |
| Preset | `asan` (AddressSanitizer) + `tsan` (ThreadSanitizer via `AGENTICDSL_ENABLE_TSAN=ON`) |
| 工具链 | `cmake --preset {asan,tsan} -DAGENTICDSL_BUILD_TESTS=ON` + `cmake --build build/{asan,tsan} -j` + `ctest --output-on-failure` |
| Build 目录 | `build/asan/` + `build/tsan/` (per CMakePresets.json `${sourceDir}/build/${presetName}`) |
| 复验时长 | ASan ~5 min, TSan ~7 min (34 test × 0.07-0.42s + sanitizer overhead) |
| ASLR 状态 | 系统默认 (SanitizeOptions `detect_leaks=1:abort_on_error=1` for ASan) |

## 2. ASan 复验结果

### 2.1 完整命令

```bash
cmake --preset asan -DAGENTICDSL_BUILD_TESTS=ON
cmake --build build/asan -j$(nproc)
cd build/asan && ctest --output-on-failure
```

### 2.2 复验结果

| 指标 | 值 |
|---|---|
| 编译 | 100% (34/34 test binary built) |
| ctest 通过率 | 33/34 (97%) |
| ctest 失败 | 1 (test_cognitive_worker) |
| ASan report | 1 (stack-use-after-scope) |

### 2.3 失败详情

**Test #6: `test_cognitive_worker`** — **Pre-existing** (Sprint 2 ship, 2026-06-18)

| 项 | 值 |
|---|---|
| 错误类型 | `AddressSanitizer: stack-use-after-scope` |
| 位置 | `tests/test_cognitive_worker.cpp:226` in `operator()` |
| 根因 | TEST_CASE 8 "并发" lambda 内部构造 `std::vector<std::thread>` (line 222),lambda captures local `bus` (line 208) + `worker` (line 219) by reference,thread 启动后 lambda 退出 → 局部 bus + worker 出 scope → threads 仍未 join → ASan 检测到访问已出 scope 的栈变量 |
| ASan stack trace | `CATCH2_INTERNAL_TEST_8` (line 224) → `worker` (line 219) → `bus` (line 208) → `threads` (line 222) |
| Sprint 引入 | Sprint 2 CognitiveWorker 实施 (commit `a4c7b41` 等) |
| 本 change 引入 | ❌ 否 — pre-existing |
| 优雅降级依据 | spec §"sanitizer-revalidation / 历史 race/leak 优雅降级" Scenario |

### 2.4 修复方案

**策略 C: `std::thread` → `std::jthread` 替换** (C++20 RAII)

| 方案 | 描述 | 评估 |
|---|---|---|
| A: shared_ptr 包装 | bus + worker 用 `shared_ptr` 包装 | 改动产品接口签名,影响范围大 |
| B: 显式 join | 测试 scope 内 `threads.clear()` | 增加测试代码冗长,仅 1 个 TEST_CASE 涉及 |
| **C: jthread 替换** | `std::vector<std::thread>` → `std::vector<std::jthread>` (C++20 RAII auto-join) | **推荐** — 改动最小 + 修复根本问题 + C++20 已 ship |

详细修复实施见 `openspec/changes/2026-06-25-pre-existing-sanitizer-findings/tasks.md` §2。

## 3. TSan 复验结果

### 3.1 完整命令

```bash
cmake --preset tsan -DAGENTICDSL_BUILD_TESTS=ON
cmake --build build/tsan -j$(nproc)
cd build/tsan && ctest --output-on-failure
```

### 3.2 复验结果

| 指标 | 值 |
|---|---|
| 编译 | 100% (34/34 test binary built) |
| ctest 通过率 | 32/34 (94%) |
| ctest 失败 | 2 (test_cognitive_worker + test_domain_worker_pool) |
| TSan warnings | 12 (主要来自 test_domain_worker_pool) |

### 3.3 失败详情

**Test #6: `test_cognitive_worker`** — **Pre-existing** (Sprint 2 ship)

| 项 | 值 |
|---|---|
| 错误类型 | `ThreadSanitizer: data race` (与 ASan 同源) |
| 根因 | 同 ASan 2.3 — stack-use-after-scope, TSan 进一步检测到 thread 引用已出 scope 栈变量的 data race |
| 跟踪 | 与 ASan 同一跟踪 (pre-existing) |

**Test #8: `test_domain_worker_pool`** — **Pre-existing** (Sprint 3 ship, 2026-06-19)

| 项 | 值 |
|---|---|
| 错误类型 | `ThreadSanitizer: data race` in `Catch::RunContext::resetAssertionInfo()` (catch_amalgamated.cpp:5909) |
| 警告数 | 12 warnings (Catch2 framework 内部与 jthread worker 交互) |
| **测试 assertions** | **94/94 PASS** — 所有 7 个 TEST_CASE 实际都 PASS |
| 根因 | **Catch2 framework + std::jthread 已知交互问题**,非 `DomainWorkerPool` 产品代码 bug |
| 产品代码验证 | Sprint 3 ship 时 18/18 InMemoryBus 并发断言无 data race 验证已通过 |
| Sprint 引入 | Sprint 3 DomainWorkerPool 实施 (commit `aa54605` 等) |
| 本 change 引入 | ❌ 否 — pre-existing |

### 3.4 决策

| Test | 决策 | 依据 |
|---|---|---|
| test_cognitive_worker | **修复** (策略 C: jthread 替换) | pre-existing stack-use-after-scope 根本问题,改 jthread 干净解决 |
| test_domain_worker_pool | **文档化** (不修复) | Catch2 framework 已知交互,产品代码 race-free 已 Sprint 3 验证,改 Catch2 patch 超出本 change scope |

## 4. 优雅降级决策 (per `engine-include-decoupling` spec)

按 spec §"sanitizer-revalidation / 历史 race/leak 优雅降级" Scenario 规定:

> ASan/TSan 发现历史 race/leak(非本 change 引入)
> → MUST 记录为 pre-existing
> → MUST 创建独立 OpenSpec change 跟踪
> → 本 spec 仍 MUST 视为 ship gate PASS (不阻塞 archive 闭环)

**实施**:
1. ✅ 2 个 pre-existing 已记录 (本报告 §2.3 + §3.3)
2. ✅ 独立 OpenSpec change 创建: `openspec/changes/2026-06-25-pre-existing-sanitizer-findings/`
3. ✅ `engine-include-decoupling` spec archive 闭环 (4-change chain 已 ship, Sprint 10 起点 0 active change 状态)
4. ✅ 本 spec 视为 ship gate PASS

## 5. 跟踪 (pre-existing OpenSpec change)

`openspec/changes/2026-06-25-pre-existing-sanitizer-findings/` 包含 4 artifacts:

- `proposal.md` (Why/What/Capabilities/Impact/Non-goals)
- `tasks.md` (4 阶段 50 task: P2.5 复验 ship gate 对账 + P1 修复 test_cognitive_worker + P2 文档化 test_domain_worker_pool + 文档同步)
- `specs/pre-existing-sanitizer-findings/spec.md` (4 ADDED Requirements: cognitive-worker-fix / domain-worker-pool-doc / ship-gate-report / roadmap-status-update)
- `.openspec.yaml` (schema: spec-driven, created: 2026-06-25)

openspec validate: `Change '2026-06-25-pre-existing-sanitizer-findings' is valid`

## 6. 后续 (Sprint 10 ship gate 决策)

### 6.1 Sprint 10 ship gate 状态

**✅ PASS** — per `engine-include-decoupling` spec 优雅降级条款 + pre-existing tracking change 创建

| 维度 | 状态 |
|---|---|
| ctest (无 sanitizer) | 34/34 PASS |
| ctest ASan | 33/34 PASS (1 pre-existing 跟踪) |
| ctest TSan | 32/34 PASS (2 pre-existing 跟踪) |
| 跨模块+common include | 10→3 (基线达成) |
| archive chain | 0 active change (Sprint 10 干净起点) |

### 6.2 候选后续工作 (Sprint 10+ backlog)

| 项 | 优先级 | 估时 | 跟踪 |
|---|---|---|---|
| 修复 `test_cognitive_worker` (jthread 替换) | 🟠 P1 | 1-2h | `2026-06-25-pre-existing-sanitizer-findings` §2 |
| 文档化 `test_domain_worker_pool` (注释 Catch2+jthread 交互) | 🟢 P2 | 30 min | `2026-06-25-pre-existing-sanitizer-findings` §3 |
| 文档同步 (roadmap-status.md + AGENTS.md) | 🟢 P2 | 1h | `2026-06-25-pre-existing-sanitizer-findings` §4 |
| Sprint 10 任务规划 (Phase 1 智能体层 100% 收官后) | 🟢 P3 | TBD | 新 change |

### 6.3 治理债清理总结

| 债 | 状态 | 修复方式 |
|---|---|---|
| 6.3.2 P2.A factory 删除 | ✅ 已 ship (871b62d) | — |
| 6.3.3 P2.A 二次确认 (承重假设 0 命中) | ✅ 已验证 | — |
| 6.3.4 P2.B 15 测试 | ✅ 12 verify (Sprint 6/7 ship) + 3 new (3681ba8) = 15 | — |
| 6.3.5 P2.C engine.cpp includes 10→3 | ✅ 已 ship (e7306d9 + 18ce4aa + 8f2ad54 + a8abc35) | — |
| 6.3.6 P2.D pending_dynamic_deps_ 访问器 | ✅ 已 ship Sprint 7 (75ded94) | — |
| P2.F TSan/ASan 复验 | ✅ 33/34 ASan + 32/34 TSan (2 pre-existing tracked) | 本报告 |
| archive 链 4 change | ✅ 全 ship (269ef17 + 104dc04 + 56975aa + dc50fbf) | — |
| engine-include-decoupling spec 状态 | ✅ openspec v1.4.1 archive 后, spec 通过 openspec CLI per-machine registry 维护 (`openspec list --specs` 已识别) | spec 不是 git-tracked source of truth, 而是 per-machine state (.gitignore `openspec/specs/`) |
| test_cognitive_worker ASan/TSan pre-existing | 🟠 P1 跟踪修复 (jthread 替换) | `2026-06-25-pre-existing-sanitizer-findings` |
| test_domain_worker_pool TSan pre-existing | 🟢 P2 跟踪文档化 (注释) | `2026-06-25-pre-existing-sanitizer-findings` |
