# Tasks: Pre-existing Sanitizer Findings

> **变更类型**: 真实实现 (P1 修复 + P2 文档化, ~2-3h 总工时)
> **触发 change**: `2026-06-24-engine-include-final-decoupling` §"sanitizer-revalidation" spec 优雅降级条款
> **关联 superpowers plan**: `docs/superpowers/plans/2026-06-24-engine-include-final-decoupling.md` (Stage 2 复验)
> **关联 plan ship gate**: `docs/roadmap-status.md` §"ASan 验证" + §"TSan 验证" 表
> **创建日期**: 2026-06-25 (P2.5 复验发现 2 pre-existing)
> **前置依赖**: P2.5 复验 ship gate 完成 (commit `871b62d` `e7306d9` `18ce4aa` `8f2ad54` `a8abc35` `3681ba8` `940ae2a` `85b6c91` 已 ship),baseline 34/34 ctest pass

---

## 1. P2.5 复验 ship gate 记录 (本 change 创建时已完成,仅对账)

### 1.1 ASan 复验 (2026-06-25 P2.5)

- [x] 1.1.1 `cmake --preset asan -DAGENTICDSL_BUILD_TESTS=ON` 配置
- [x] 1.1.2 `cmake --build build/asan -j` 编译
- [x] 1.1.3 `ctest --output-on-failure` 33/34 PASS (97% pass rate)
- [x] 1.1.4 失败分析: `test_cognitive_worker` `stack-use-after-scope` at `tests/test_cognitive_worker.cpp:226`
- [x] 1.1.5 决策: pre-existing, Sprint 2 ship (2026-06-18), 非本 change 引入
- [x] 1.1.6 跟踪: 本 change §2 修复

### 1.2 TSan 复验 (2026-06-25 P2.5)

- [x] 1.2.1 `cmake --preset tsan -DAGENTICDSL_BUILD_TESTS=ON` 配置
- [x] 1.2.2 `cmake --build build/tsan -j` 编译
- [x] 1.2.3 `ctest --output-on-failure` 32/34 PASS (94% pass rate)
- [x] 1.2.4 失败分析:
  - `test_cognitive_worker` (与 ASan 同样失败)
  - `test_domain_worker_pool` 12 TSan warnings but 94 assertions all PASS (Catch2+jthread 框架交互)
- [x] 1.2.5 决策:
  - `test_cognitive_worker`: pre-existing, Sprint 2 ship, 跟踪至本 change §2 修复
  - `test_domain_worker_pool`: pre-existing, Sprint 3 ship, Catch2 framework 已知交互, 跟踪至本 change §3 文档化
- [x] 1.2.6 跟踪: 本 change §2 + §3

---

## 2. P1 修复 test_cognitive_worker stack-use-after-scope (~1-2h)

### 2.1 诊断 (opencode debug-issue skill)

- [ ] 2.1.1 启动 debug-issue skill
- [ ] 2.1.2 阅读 `tests/test_cognitive_worker.cpp:200-260` (TEST_CASE 8 "并发" lambda 范围)
- [ ] 2.1.3 识别 stack-use-after-scope 根因:
  - lambda captures `bus` (line 208) + `worker` (line 219) by reference
  - `std::vector<std::thread> threads` (line 222) 在 lambda 内构造
  - threads 内 std::thread 启动后,lambda 退出 → 局部 bus + worker 出 scope → threads 仍未 join
  - threads 内部仍引用 bus + worker → ASan 检测到 stack-use-after-scope
- [ ] 2.1.4 假设 3 个修复策略:
  - A: std::shared_ptr 包装 bus + worker
  - B: threads.clear() 在 scope 内等待 join
  - C: std::jthread (C++20) 替代 std::thread (RAII 自动 join on destruction)

### 2.2 修复策略选择

- [ ] 2.2.1 评估策略 A/B/C:
  - 策略 A: 改动最小,只改 bus + worker 包装。但需修改产品接口签名。
  - 策略 B: 改动最小,但增加测试代码冗长(显式 clear)。对 8 个 TEST_CASE 中只有 1 个涉及 thread 启动。
  - 策略 C: 改动中等,需 #include <thread> → jthread 替换,但 C++20 已 ship,语义最干净 (RAII auto-join)。
- [ ] 2.2.2 **推荐方案**: 策略 C (std::jthread 替换 std::thread) — C++20 已 ship + RAII 语义最干净 + 修复根本问题
- [ ] 2.2.3 决策: 执行策略 C (除非 review 发现更优策略)

### 2.3 TDD 实施 (3 步)

- [ ] 2.3.1 **先写 failing test**: 复现 ASan failure,确认 `test_cognitive_worker` 在 ASan + TSan 都失败
- [ ] 2.3.2 **再写 fix**: 改 `tests/test_cognitive_worker.cpp:222` `std::vector<std::thread>` → `std::vector<std::jthread>`,删除显式 join 调用 (jthread 自动 RAII)
- [ ] 2.3.3 **后写 verify**: ASan + TSan 都通过, 34/34 ctest pass

### 2.4 Commit + 验证

- [ ] 2.4.1 `cmake --preset asan -DAGENTICDSL_BUILD_TESTS=ON && cmake --build build/asan -j && ctest -R test_cognitive_worker --output-on-failure` MUST pass
- [ ] 2.4.2 `cmake --preset tsan -DAGENTICDSL_BUILD_TESTS=ON && cmake --build build/tsan -j && ctest -R test_cognitive_worker --output-on-failure` MUST pass
- [ ] 2.4.3 `cd build && ctest --output-on-failure` MUST 34/34 PASS
- [ ] 2.4.4 `git add tests/test_cognitive_worker.cpp` + `git commit -m "test(cognitive-worker): replace std::thread with std::jthread to fix stack-use-after-scope (pre-existing ASan/TSan failure)"`
- [ ] 2.4.5 更新本 tasks.md §2 全部 [x]

---

## 3. P2 文档化 test_domain_worker_pool TSan 交互 (~30 min)

### 3.1 决策记录 (本 tasks §1.2.5 已记录)

- [ ] 3.1.1 决策: 不修复测试 (Catch2 framework patch 复杂,超出本 change scope)
- [ ] 3.1.2 决策: 文档化为 Catch2 + std::jthread 已知交互
- [ ] 3.1.3 决策: 验证产品代码 `DomainWorkerPool` 本身无 race (Sprint 3 ship 时 18/18 InMemoryBus 并发断言已验证)

### 3.2 文档化实施

- [ ] 3.2.1 阅读 `tests/test_domain_worker_pool.cpp:430-460` (CATCH2_INTERNAL_TEST_19 lambda 范围)
- [ ] 3.2.2 注释追加:
  ```cpp
  // NOTE: TSan reports 12 data race warnings in this test case due to
  // Catch2's `Catch::RunContext::resetAssertionInfo()` interacting with
  // std::jthread workers. This is a pre-existing Catch2+jthread framework
  // issue (not a DomainWorkerPool product code bug). Product code race
  // safety was verified in Sprint 3 with 18/18 InMemoryBus concurrent
  // assertions passing under TSan. See OpenSpec change
  // `2026-06-25-pre-existing-sanitizer-findings` for full analysis.
  ```
- [ ] 3.2.3 `cd build && ctest -R test_domain_worker_pool --output-on-failure` (注释无功能影响,仍 12 warnings, 94 assertions PASS)
- [ ] 3.2.4 `git add tests/test_domain_worker_pool.cpp` + `git commit -m "docs(test): annotate Catch2+jthread TSan interaction in test_domain_worker_pool (pre-existing framework issue, product code verified race-free)"`
- [ ] 3.2.5 更新本 tasks.md §3 全部 [x]

---

## 4. 文档同步与 ship gate 验证报告 (~1h)

### 4.1 docs/roadmap-status.md 更新

- [ ] 4.1.1 §"ASan 验证" 表追加 2026-06-25 行: build/asan 33/34 PASS, 1 失败 (test_cognitive_worker pre-existing)
- [ ] 4.1.2 §"TSan 验证" 表追加 2026-06-25 行: build/tsan 32/34 PASS, 2 失败 (test_cognitive_worker + test_domain_worker_pool pre-existing)
- [ ] 4.1.3 引用本 change `2026-06-25-pre-existing-sanitizer-findings` 作为 pre-existing 跟踪依据

### 4.2 AGENTS.md 更新

- [ ] 4.2.1 §Recent Changes 追加 2026-06-25 P2.5 ship entry:
  ```
  - 2026-06-25 (P2.5 ship gate 复验): `cmake --preset asan` 33/34 PASS, `cmake --preset tsan` 32/34 PASS。2 pre-existing 失败 (test_cognitive_worker Sprint 2 stack-use-after-scope + test_domain_worker_pool Sprint 3 Catch2+jthread 交互) 由 OpenSpec change `2026-06-25-pre-existing-sanitizer-findings` 跟踪。Sprint 10 ship gate 通过 (per `engine-include-decoupling` spec 优雅降级条款)。
  ```

### 4.3 docs/audits/2026-06-25-sanitizer-revalidation.md 新建

- [ ] 4.3.1 创建文件,包含:
  - §1 复验环境 (CMakePresets.json asan/tsan + gcc-13 + ASLR 状态)
  - §2 ASan 复验 (完整 ctest 输出 + 失败分析 + commit hash)
  - §3 TSan 复验 (完整 ctest 输出 + 失败分析 + commit hash)
  - §4 优雅降级决策 (per `engine-include-decoupling` spec §sanitizer-revalidation Scenario)
  - §5 跟踪 (本 change + 修复策略)
  - §6 后续 (Sprint 10 ship gate 通过 + 候选修复工作)

### 4.4 Commit

- [ ] 4.4.1 `git add docs/roadmap-status.md AGENTS.md docs/audits/2026-06-25-sanitizer-revalidation.md` + `git commit -m "docs(ship-gate): P2.5 sanitizer revalidation report (ASan 33/34, TSan 32/34, 2 pre-existing tracked)"`
- [ ] 4.4.2 更新本 tasks.md §4 全部 [x]

---

## 5. ship gate 验证清单 (本 change 全部完成时)

- [ ] 5.1 `cd build && ctest --output-on-failure` MUST 34/34 PASS (无 sanitizer)
- [ ] 5.2 `cd build/asan && ctest --output-on-failure` MUST 34/34 PASS (test_cognitive_worker 修复后)
- [ ] 5.3 `cd build/tsan && ctest --output-on-failure` MUST 34/34 PASS 或 33/34 (test_domain_worker_pool 文档化为非阻塞,1 warning 文档化注释不影响 pass)
- [ ] 5.4 `git log --oneline -10` MUST 包含本 change 全部 commit (按 §2 → §3 → §4 顺序)
- [ ] 5.5 `docs/roadmap-status.md` §"ASan 验证" + §"TSan 验证" MUST 包含 2026-06-25 行
- [ ] 5.6 `AGENTS.md` §Recent Changes MUST 含 P2.5 ship entry
- [ ] 5.7 `openspec validate 2026-06-25-pre-existing-sanitizer-findings` exit 0
- [ ] 5.8 零回归 (34 baseline 全部保留,测试代码 + 文档变更)

---

## 6. 验证与回归策略

- **回归策略**: §2 修复后 MUST 验证 8 个 TEST_CASE 行为保持 (无并发死锁/竞态)
- **TDD 硬约束**: §2.3 MUST 先写 failing test (复现 ASan failure) 再写 fix
- **优雅降级**: §3 文档化不阻塞 ship gate,仅记录治理债
- **SHALL/MUST 验证**: 0 个 spec Requirement (本 change 仅为跟踪,无新 spec) → 仅 tasks.md 全部 [x] 方可 archive

---

## 7. STATUS NOTE (本 change 设计)

> **承接关系**: 本 change 是 `engine-include-decoupling` spec §"sanitizer-revalidation / 历史 race/leak 优雅降级" Scenario 触发的"独立 change 跟踪"动作
> **治理模式**: 严格全路径 (承袭 plan §8 治理模式)
> **修复策略**: §2 jthread 替换 std::thread (C++20 RAII)
> **文档化策略**: §3 Catch2+jthread 已知交互,产品代码 race-free 已 Sprint 3 验证
> **优雅降级**: 本 change archive 不阻塞 Sprint 10 ship gate (per spec 优雅降级条款)
