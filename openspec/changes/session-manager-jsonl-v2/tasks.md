# session-manager-jsonl-v2 Implementation Tasks

> 接续 archived `2026-08-04-session-manager-jsonl` + commit `155fafb` ship 的 C++ SessionManager 核心（v1 完成 90%），v2 收尾 3 项未完成事项：① `session.persisted` 事件实际发射 ② `session_manager.cpp` 接入 `agenticdsl_core` 生产库 ③ 从 stash@{0} 恢复 v1 in-progress 资源。v1 公开 API / fork/branch/compact/build_context_entries/migrate_legacy_json 等已 ship，本 v2 不重写。

## 1. 落地 `session.persisted` 事件发射

- [ ] 1.1 **TDD Write failing test**: 在 `tests/test_session_persisted_event.cpp` 新建空文件并添加 4 个 TEST_CASE 骨架（`session.persisted` 成功发射 / 失败路径不发射 / `bus_ == nullptr` 跳过 emit / payload 4 字段完整），仅声明 `TEST_CASE` 内容，**不实现**。
- [ ] 1.2 **Verify test fails**: `cmake --build build --target test_session_persisted_event && ctest -R test_session_persisted_event --output-on-failure` — 应当编译失败（test target 不存在）或全部 4 cases FAIL（v2 改动未实施，flush_append 不调 emit）。
- [ ] 1.3 **Implement**: 修改 `src/core/session_manager.cpp` 的 `flush_append` 函数（line 117-167），在 `::fsync(fd)` + `::close(fd)` 之后、`std::lock_guard<std::mutex> idx_lock(index_mutex_);` 之前，按以下伪代码插入 ~10 行：
  ```cpp
  // === v2: session.persisted event emission (ADR-0068 EventBuilder V2) ===
  if (bus_) {
    const std::string session_id = current_path_.stem().string();
    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    bus_->emit(
      EventBuilder("session.persisted", ToolResult{})
        .args({{"session_id", session_id},
               {"node_id", node.id},
               {"branch_id", node.branch_id},
               {"timestamp", now_ms}})
        .build()
    );
  }
  ```
  并在文件头 `#include` 段追加 `#include "agenticdsl/contract/event_builder.h"` + `#include "agenticdsl/types/tool_result.h"`。
- [ ] 1.4 **Verify test passes**: 重新跑 1.2 的 ctest — 4 cases 全部 PASS。
- [ ] 1.5 **Add edge-case tests**: 在 `tests/test_session_persisted_event.cpp` 追加 2 个 TEST_CASE — ① `flush_append_internal(BranchMeta)` 不发射事件（验证 private 路径不触发 emit）；② EventBuilder 构造异常时不传播给 `flush_append` 调用方（防御性测试，可选 mock EventBuilder 抛异常的 path）。
- [ ] 1.6 **Verify all 6 cases PASS** + **回归 ctest 全量**: `cmake --build build && ctest --output-on-failure` — 期望 116/118 + 6 新增 = 122/124 PASS（2 pre-existing failures 来自 `test_e2e_real_llm` 需 QIANFAN_API_KEY + `test_cost_tracking_decorator` commit `514c441`，与本 change 无关）。
- [ ] 1.7 提交：`git add src/core/session_manager.cpp tests/test_session_persisted_event.cpp && git commit -m "feat(core): emit session.persisted event in SessionManager::flush_append (ADR-0068)"`

## 2. `session_manager.cpp` 接入 `agenticdsl_core` 生产库

- [ ] 2.1 查找 `agenticdsl_core` target 注册位置（根 `CMakeLists.txt` 或 `src/CMakeLists.txt`）— `grep -rn "agenticdsl_core" CMakeLists.txt src/CMakeLists.txt`。
- [ ] 2.2 在 `agenticdsl_core` target 的 sources 列表追加 `${CMAKE_CURRENT_SOURCE_DIR}/src/core/session_manager.cpp`（或对应子模块 CMakeLists.txt 注册）。
- [ ] 2.3 **Verify build**: `rm -rf build && mkdir build && cd build && cmake .. -DAGENTICDSL_BUILD_TESTS=ON && make -j$(nproc) agenticdsl_core` — 应当成功编译 `agenticdsl_core` 静态库（含 `session_manager.cpp`）。
- [ ] 2.4 修改 `tests/CMakeLists.txt` 移除 v1 Task 1.11 + 9 临时绕过 — 删除 `if(TEST_NAME STREQUAL "test_session_manager" OR TEST_NAME STREQUAL "test_session_build_context") { target_sources(${TEST_NAME} PRIVATE .../session_manager.cpp) }` 整段（~6 行）。
- [ ] 2.5 **Verify test build**: 重新跑 2.3 后 `make -j$(nproc) test_session_manager test_session_build_context` — 应当成功（`agenticdsl_core` 含 `session_manager.cpp` 即可 link）。
- [ ] 2.6 提交：`git add CMakeLists.txt src/CMakeLists.txt tests/CMakeLists.txt && git commit -m "refactor(core): link session_manager.cpp into agenticdsl_core, remove test-target bypass"`

## 3. 从 stash@{0} 恢复 v1 in-progress 资源

- [ ] 3.1 在 worktree 内 `git stash pop stash@{0}` 恢复 3 个文件：
  - `tools/migrate_session_json.py`（v1 Task 4.4 Python CLI 迁移工具，204 行）
  - `tests/test_session_manager_legacy.cpp`（v1 Task 6.8-6.12 迁移等价性测试，241 行 / 4 cases）
  - `tests/CMakeLists.txt` 扩展（v1 Task 1.11 bypass 扩展，把 `test_session_manager_legacy` 加入 `if` 条件）
- [ ] 3.2 **Verify stash pop 无冲突**: 解决可能的 conflict（stash pop 应用到已修改的 `tests/CMakeLists.txt` — v2 步骤 2.4 已移除 v1 bypass，stash 的扩展无意义，应当保留 v2 干净版本）。
- [ ] 3.3 在 `tests/CMakeLists.txt` 注册新测试：`add_catch_test(test_session_manager_legacy SOURCES tests/test_session_manager_legacy.cpp)`（`add_catch_test` 函数已存在，v1 24 cases 用同模式注册）。
- [ ] 3.4 **Verify test build + run**: `cmake --build build --target test_session_manager_legacy && ctest -R test_session_manager_legacy --output-on-failure` — 4 cases 全部 PASS。
- [ ] 3.5 提交：`git add tools/migrate_session_json.py tests/test_session_manager_legacy.cpp tests/CMakeLists.txt && git commit -m "feat(session): ship migrate_session_json.py CLI and legacy migration tests (Wave 2 P1 carryover)"`

## 4. 文档同步

- [ ] 4.1 更新 `AGENTS.md` CODE MAP — 在 `SessionManager` 关键符号行追加 "事件发射已 ship (v2)" note。
- [ ] 4.2 更新 `docs/adr/adr-0068-event-emission-contract.md` 附录 A — `session.persisted` owner 字段从 `ChatSession / session_agent` 改为 `SessionManager`。
- [ ] 4.3 更新 `docs/adr/adr-0033-session-hierarchy.md` 实施范围说明 — 追加 "v2 完整 ship（事件发射 + 生产库集成）" note。
- [ ] 4.4 运行 `tools/adr_lint.py` exit 0 + `python3 tools/docs_drift_audit.py` 验证 0 DRIFT。
- [ ] 4.5 提交：`git add AGENTS.md docs/adr/adr-0068-event-emission-contract.md docs/adr/adr-0033-session-hierarchy.md && git commit -m "docs(session): mark session.persisted emission ship-complete + update ADR owner field"`

## 5. Ship gate 验证

- [ ] 5.1 运行 ctest 全量：`cmake --build build && ctest --output-on-failure` — 期望 124/124 PASS（116 baseline + 6 v2 new + 4 stash legacy = 126 - 2 pre-existing 无关失败 = 124）。
- [ ] 5.2 运行 ASan 测试：`cmake --preset asan -DAGENTICDSL_BUILD_TESTS=ON && ctest --output-on-failure`（验证 0 memory leak / 0 use-after-free）。
- [ ] 5.3 运行 TSan 测试：`cmake --preset tsan -DAGENTICDSL_BUILD_TESTS=ON && ctest --output-on-failure`（验证 `test_session_persisted_event` 0 data race）。
- [ ] 5.4 运行 `openspec validate session-manager-jsonl-v2 --strict` exit 0。
- [ ] 5.5 准备 ship 报告 `docs/audits/2026-08-XX-session-manager-jsonl-v2-ship-gate.md` 记录 5.1-5.4 结果。

## 6. 归档

- [ ] 6.1 提交 changes artifacts：`git add openspec/changes/session-manager-jsonl-v2/ && git commit -m "feat: fill session-manager-jsonl-v2 change artifacts (Wave 2 P2 carryover)"`。
- [ ] 6.2 worktree 合并到 main（`/guide-ship` 走 Phase 3 archive 流程：`openspec archive session-manager-jsonl-v2` + worktree merge + branch cleanup）。
- [ ] 6.3 验证：归档后 `openspec list` 显示 0 active changes，stash 列表无 `session-manager-jsonl` 相关条目。

## 7. Follow-up 提案

- [ ] 7.1 在 `proposal-suggestions.md` 提议"先在 ADR-0068 附录 A 注册 `session.before.switch` / `session.before.fork` / `session.before.compact` 三个会话生命周期主题"（命名遵循 `<domain>.<entity>.<verb>` 点号约定），注册完成后可追加 `session.before.*` 发射实现。
- [ ] 7.2 同步更新 ADR-0068 附录 A 反映新注册主题（owner 字段归 `SessionManager`）。
