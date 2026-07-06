# pre-existing-sanitizer-findings Specification

## Purpose
2026-06-25 P2.5 ship gate 复验 (`cmake --preset asan/tsan`) 发现 2 个 pre-existing 测试失败(非本 change 引入):(1) `test_cognitive_worker` (Sprint 2) ASan+TSan `stack-use-after-scope` at line 226 — Catch2 TEST_CASE 8 lambda 内构造 `std::vector<std::thread>` 引用已出 scope 的捕获变量;修复策略 `std::thread` → `std::jthread` (C++20 RAII)。(2) `test_domain_worker_pool` (Sprint 3) 12 TSan warnings Catch2 framework + std::jthread 已知交互,**文档化非修复** (产品代码 DomainWorkerPool Sprint 3 ship 已验证 18/18 并发断言无 race)。
## Requirements
### Requirement: cognitive-worker-stack-use-after-scope-fix

`tests/test_cognitive_worker.cpp` TEST_CASE 8 (Sprint 2 ship) MUST 修复 `stack-use-after-scope` ASan failure at line 226。修复策略: `std::vector<std::thread>` 替换为 `std::vector<std::jthread>` (C++20 RAII auto-join on destruction),避免 lambda 退出时 thread 仍 in-flight 引用已出 scope 的 stack 局部变量 (`bus` + `worker`)。

#### Scenario: 修复策略 C 实施 (jthread 替换)

- **WHEN** 实施 §2 修复
- **THEN** `tests/test_cognitive_worker.cpp:222` MUST 改为 `std::vector<std::jthread> threads`
- **AND** MUST 删除显式 `threads[i].join()` 调用 (jthread 自动 RAII)
- **AND** `cmake --preset asan && ctest -R test_cognitive_worker` MUST pass
- **AND** `cmake --preset tsan && ctest -R test_cognitive_worker` MUST pass
- **AND** 8 个 TEST_CASE 行为 MUST 保持 (无并发死锁/竞态)

### Requirement: domain-worker-pool-tsan-framework-interaction-doc

`tests/test_domain_worker_pool.cpp` MUST 文档化 TSan 12 data race warnings 的真实根因 (Catch2 `resetAssertionInfo()` + `std::jthread` worker 框架交互),非 `DomainWorkerPool` 产品代码 bug。`DomainWorkerPool` 产品代码 race safety MUST 已 Sprint 3 验证 (18/18 InMemoryBus 并发断言无 data race)。

#### Scenario: 注释追加

- **WHEN** 实施 §3 文档化
- **THEN** `tests/test_domain_worker_pool.cpp:430-460` MUST 追加注释:
  > TSan reports 12 data race warnings in this test case due to Catch2's `Catch::RunContext::resetAssertionInfo()` interacting with std::jthread workers. This is a pre-existing Catch2+jthread framework issue (not a DomainWorkerPool product code bug). Product code race safety was verified in Sprint 3 with 18/18 InMemoryBus concurrent assertions passing under TSan. See OpenSpec change `2026-06-25-pre-existing-sanitizer-findings` for full analysis.
- **AND** 注释 MUST NOT 影响 94 assertions 行为
- **AND** 注释 MUST 引用本 change 作为完整分析依据

### Requirement: ship-gate-validation-report

`docs/audits/2026-06-25-sanitizer-revalidation.md` MUST 存在并包含:
- §1 复验环境 (CMakePresets.json asan/tsan + gcc-13)
- §2 ASan 复验完整 ctest 输出 + 失败分析 + commit hash
- §3 TSan 复验完整 ctest 输出 + 失败分析 + commit hash
- §4 优雅降级决策 (per `engine-include-decoupling` spec)
- §5 跟踪 (本 change)
- §6 后续 (Sprint 10 ship gate 通过 + 候选修复工作)

#### Scenario: 报告文件存在

- **WHEN** 完成 §4 文档化
- **THEN** `docs/audits/2026-06-25-sanitizer-revalidation.md` MUST 存在
- **AND** MUST 包含 §1-§6 全部章节
- **AND** MUST 引用所有相关 commit hash (871b62d, e7306d9, 18ce4aa, 8f2ad54, a8abc35, 3681ba8)

### Requirement: roadmap-status-md-update

`docs/roadmap-status.md` §"ASan 验证" + §"TSan 验证" 表 MUST 追加 2026-06-25 P2.5 复验行,记录:
- ASan 33/34 PASS, 1 失败 (test_cognitive_worker pre-existing)
- TSan 32/34 PASS, 2 失败 (test_cognitive_worker + test_domain_worker_pool pre-existing)
- 引用本 change 作为 pre-existing 跟踪依据

#### Scenario: 表格更新

- **WHEN** 完成 §4 文档化
- **THEN** `docs/roadmap-status.md` MUST 含 2026-06-25 ASan 行
- **AND** MUST 含 2026-06-25 TSan 行
- **AND** 2 行 MUST 引用本 change `2026-06-25-pre-existing-sanitizer-findings`

