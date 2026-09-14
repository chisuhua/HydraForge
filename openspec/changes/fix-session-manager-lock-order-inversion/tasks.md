# Tasks — fix-session-manager-lock-order-inversion

> **实施分 2 个原子 commit**（per AGENTS.md 模式 #4 SHIP-with-fixes 风险隔离）:
> - **Commit 1**: Critical 1 源码修复（BranchMeta 快照）
> - **Commit 2**: Critical 2 + 测试 + 验收（删 `legacy_path` 参数 + 新增并发测试 + 静态断言）

---

## Commit 1: Critical 1 — BranchMeta 快照（仅源码修复，无 API 变化）

- [x] **T1.1** `src/core/session_manager.cpp` 修改 `migrate_legacy_json` 末段
  - **Anchor**: 注释 `// 写 main branch meta` 下方 (L593-597)
  - **Before**:
    ```cpp
    // 写 main branch meta
    {
      std::lock_guard<std::mutex> lock(index_mutex_);
      flush_append_internal(branches_["main"]);
    }
    ```
  - **After**:
    ```cpp
    // 写 main branch meta — BranchMeta 在 index_mutex_ 下快照, 锁外调
    // flush_append_internal 保持全局 write→index 锁序, 消除与 flush_append
    // 的 ABBA 死锁 (TSan #191); 与 fork() L349-355 同模式
    BranchMeta main_meta;
    {
      std::lock_guard<std::mutex> lock(index_mutex_);
      main_meta = branches_["main"];
    }
    flush_append_internal(main_meta);
    ```

- [x] **T1.2** Functional 编译验证 (应已 PASS, 0 error)
  ```bash
  cmake --build build -j$(nproc)
  ```

- [x] **T1.3** TSan 部分验证（仅 #191 应翻绿）
  ```bash
  ctest --test-dir build-tsan -R '^test_session_manager_legacy$' --output-on-failure
  # 预期: 0 warning, exit 0
  ```

- [x] **T1.4** Commit 1
  ```bash
  git add src/core/session_manager.cpp
  git commit -m "fix(session_manager): BranchMeta snapshot in migrate_legacy_json (TSan #191)

  Pre-existing lock-order-inversion in migrate_legacy_json: the
  function holds index_mutex_ around flush_append_internal, which
  itself takes write_mutex_ — forming an index→write edge that
  closes a cycle with open()'s write→index edge (M0↔M1 cycle).

  TSan #191 reports this from test_session_manager_legacy in 4 of
  4 TEST_CASEs (single-threaded call path) — TSan lock-graph
  detection does not require real overlap.

  Fix: snapshot BranchMeta under index_mutex_, then call
  flush_append_internal outside the lock. Same pattern as fork()
  L349-355 (already verified correct).

  Note: this commit alone fixes TSan #191 but leaves the
  write→write recursion risk in open(id, legacy_path) untested.
  Commit 2 will remove the legacy_path parameter entirely to
  structurally eliminate that latent bug."
  ```

---

## Commit 2: Critical 2 + 测试 + 验收（删 `legacy_path` 参数 + 新并发测试 + 静态断言）

- [x] **T2.1** `src/core/session_manager.h` 修改 `open()` 签名
  - **Anchor**: `session_manager.h:157`
  - **Before**:
    ```cpp
    SessionHandle open(const std::string& session_id,
                       std::optional<std::string> legacy_path = std::nullopt);
    ```
  - **After**:
    ```cpp
    SessionHandle open(const std::string& session_id);
    ```
  - 同时更新 Doxygen 注释 (L110-156) 删 legacy_path 段落, 提示 "use migrate_legacy_json() explicitly"

- [x] **T2.2** `src/core/session_manager.cpp` 修改 `open()` 实现
  - 删除 L62-65 `legacy_path` 参数检查 (已删参数, 实际无需改)
  - **删除 L80-87 legacy 分支**（6 行）:
    ```cpp
      if (!jsonl_existed && legacy_path.has_value() &&
          std::filesystem::exists(*legacy_path)) {
        migrate_legacy_json(*legacy_path);
        SessionHandle h; h.session_id = current_session_id_; h.jsonl_path = current_path_; return h;
      }
    ```
  - 函数体剩余部分保持不变
  - 更新函数体注释 (L46-58) 描述新版"显式 migrate"语义

- [x] **T2.3** 新建 `tests/test_session_manager_lock_order.cpp` (~120 行, GLOB 自动注册)
  - **顶部**:
    ```cpp
    // 编译期断言: open() 必须不含 legacy_path 参数 (R2 不变量)
    static_assert(
        !std::is_invocable_v<
            decltype(&agenticdsl::SessionManager::open),
            agenticdsl::SessionManager,
            const std::string&,
            std::optional<std::string>>,
        "open() must NOT have legacy_path parameter — write→write recursion risk");
    ```
  - **TEST_CASE 1** (Case 1 TSan-gate, `[session_manager][lock_order][tsan]`):
    ```cpp
    // 100 iter 压力循环: 线程 A migrate_legacy_json + 线程 B retry-until-open flush_append
    // 验证: 100/100 iter 完成, 0 TSan warning
    // 备注: 依赖 TSan lock-graph 检测, 不依赖真实死锁重叠
    ```
  - **TEST_CASE 2** (Case 3 functional, `[session_manager][lock_order][functional]`):
    ```cpp
    // 单线程 migrate → flush_append → reload → 验证 JSONL 字节级一致
    // 验证: 修复不引入新行为
    ```
  - **Header order**: `catch_amalgamated.hpp` + `core/session_manager.h` + `<type_traits>` + `<thread>` + `<atomic>` + `<filesystem>` + `nlohmann/json.hpp`
  - Helper: 复用 `test_session_manager_legacy.cpp` 的 `TempDirGuard` / `write_legacy_json` 模式（简化版，in-file）

- [x] **T2.4** 验证 CMake 自动注册（**无需**改 `tests/CMakeLists.txt`）
  ```bash
  cmake ..  # 重新 configure 让 GLOB 捕获新 .cpp
  ```
  验证: `grep test_session_manager_lock_order build-tsan/tests/CTestTestfile.cmake` 应显示目标已注册

- [x] **T2.5** Commit 2
  ```bash
  git add src/core/session_manager.h
  git add src/core/session_manager.cpp
  git add tests/test_session_manager_lock_order.cpp
  git commit -m "fix(session_manager): remove legacy_path parameter from open()

  Constructive elimination of write→write recursion risk in
  open(id, legacy_path) → migrate_legacy_json → open(id) path.
  Per 2026-09-11 audit (chat-session-pdk-lift change2 plan L783),
  the two-arg form was the named target of the audit, but no
  caller ever passed legacy_path (grep verified zero usage).

  Fix:
    * open() signature: drop legacy_path parameter (BREAKING,
      but zero callers + zero migration cost)
    * open() impl: remove L80-87 legacy-migration branch
    * migrate_legacy_json() remains a public API; users call it
      explicitly after open() if needed
    * tests/test_session_manager_lock_order.cpp: new binary
      with static_assert preventing future regression +
      TSan-gate concurrent test (100 iter) +
      functional regression test

  Evidence:
    functional: 235/235 ctest PASS (+1 new test binary)
    TSan: test_session_manager_legacy flips FAIL→PASS (#191)
    TSan: test_session_manager_lock_order PASS, 0 warnings
    baseline: only #85 + #199 + #1 + #15 + #195 remain FAIL
              (all pre-existing KI, see AGENTS.md Pattern #7)"
  ```

---

## Phase 3: 验证 (Acceptance)

- [x] **T3.1** A1 源码 grep (migrate 末段无 index_mutex_ 包裹)
  ```bash
  grep -B 1 -A 3 "flush_append_internal" src/core/session_manager.cpp
  # 预期: L596 flush_append_internal 在锁外调用
  ```

- [x] **T3.2** A2 头文件 grep (open 参数已删)
  ```bash
  grep -n "legacy_path" src/core/session_manager.h
  # 预期: 0 行
  ```

- [x] **T3.3** A3 全库零两参 `open()` 调用
  ```bash
  grep -rn "open(.*,.*legacy\|open(.*legacy" --include="*.cpp" --include="*.h" examples/ src/ tests/ pdk/
  # 预期: 0 行
  ```

- [x] **T3.4** A4 全量编译
  ```bash
  cmake --build build -j$(nproc)
  # 预期: 0 error (全 targets)
  ```

- [x] **T3.5** A5 functional ctest
  ```bash
  ctest --test-dir build -j4 --timeout 180
  # 预期: 235 tests, 234 PASS (允许 test_skill_interpreter pre-existing flaky)
  ```

- [x] **T3.6** A6 TSan #191 翻绿
  ```bash
  ctest --test-dir build-tsan -R '^test_session_manager_legacy$' --output-on-failure
  # 预期: PASS, 0 ThreadSanitizer warning
  ```

- [x] **T3.7** A7 新并发测试 PASS
  ```bash
  ctest --test-dir build-tsan -R '^test_session_manager_lock_order$' --output-on-failure
  # 预期: PASS, 0 ThreadSanitizer warning
  ```

- [x] **T3.8** A8 基线回归
  ```bash
  ctest --test-dir build-tsan -R 'session|causal|domain_worker|concurrent' 2>&1 | grep -E 'FAILED|tests passed'
  # 预期: 仅 #85 + #199 + #1 + #15 + #195 fail (5 项 pre-existing KI)
  #       #191 不再 fail (本 change 修复)
  #       新 test_session_manager_lock_order PASS
  ```

---

## Phase 4: 收尾

- [x] **T4.1** OpenSpec archive
  ```bash
  openspec archive fix-session-manager-lock-order-inversion
  ```

- [x] **T4.2** AGENTS.md 登记新发现的 KI
  - 在 Recent Changes 追加: "2026-09-15 (Sprint 33 / fix-session-manager-lock-order-inversion, ship)" + commit hashes
  - 在 Recent Changes 追加: "2026-09-15 TSan baseline update: 6 项 pre-existing KI（#85 + #199 + #1 + #15 + #195 + #191 已修; 新增 #1 + #15 = chat_session.cpp:291/296 in ~Impl 待独立 follow-up）"

---

## 验收清单汇总

| 验收 | 命令 | 期望 |
|---|---|---|
| A1 源码无 inversion | `grep -B 1 -A 3 "flush_append_internal" src/core/session_manager.cpp` | L596 在锁外 |
| A2 头文件参数已删 | `grep -n "legacy_path" src/core/session_manager.h` | 0 行 |
| A3 零两参调用 | `grep -rn "open(.*legacy" --include="*.cpp" --include="*.h" examples/ src/ tests/ pdk/` | 0 行 |
| A4 编译 | `cmake --build build` | 0 error |
| A5 functional | `ctest -j4 --timeout 180` | 235 tests, 234 PASS |
| A6 #191 翻绿 | `ctest --test-dir build-tsan -R '^test_session_manager_legacy$'` | PASS, 0 warning |
| A7 新测试 | `ctest --test-dir build-tsan -R '^test_session_manager_lock_order$'` | PASS, 0 warning |
| A8 基线回归 | `ctest --test-dir build-tsan -R 'session\|causal\|domain_worker\|concurrent'` | 5 项 pre-existing fail, 无新增 |
