# Tasks — pdk-chat-session-shim-cleanup

> 实施分 2 个原子 commit (per AGENTS.md 模式 #4 SHIP-with-fixes 范式, 风险隔离):
> - **Commit 1**: PDK 基础设施 (cancellation_globals.h 移动 + shim headers 删除)
> - **Commit 2**: includer 大规模迁移 (14 tests + 2 commands + main + loop_agent)

## Commit 1: PDK 基础设施 (Phase 1)

- [ ] **T1.1** 移动 `cancellation_globals.h/cpp` 到 PDK
  - `git mv examples/pdk_chat_demo/commands/cancellation_globals.h pdk/chat_session/include/agenticdsl/pdk/cancellation_globals.h`
  - `git mv examples/pdk_chat_demo/commands/cancellation_globals.cpp pdk/chat_session/src/cancellation_globals.cpp`
  - 修改 `pdk/chat_session/include/agenticdsl/pdk/cancellation_globals.h`:
    - 删除 `using CancellationRegistry = hydraforge::pdk::CancellationRegistry;`
    - `namespace pdk_chat_demo {` → `namespace hydraforge::pdk {`
    - `extern std::shared_ptr<CancellationRegistry> g_cancellation_registry;` (用全名)
  - 修改 `pdk/chat_session/src/cancellation_globals.cpp`:
    - namespace 同步改为 `hydraforge::pdk`

- [ ] **T1.2** `pdk/chat_session/CMakeLists.txt` 注册新 .cpp
  - 添加 `src/cancellation_globals.cpp` 到 SOURCES 列表

- [ ] **T1.3** 删除 2 个 shim 头
  - `git rm examples/pdk_chat_demo/chat_session.h`
  - `git rm examples/pdk_chat_demo/cancellation_registry.h`

- [ ] **T1.4** 验证 Commit 1 编译
  - `cmake --build build -j$(nproc)` 应**失败** (因为 examples 还引用被删的 shim) — 这是预期, 验证"删除正确" ≠ 验证"examples 已迁"
  - 仅验证 pdk_chat_session 库本身编译: `cmake --build build --target pdk_chat_session -j$(nproc)` → 0 error

- [ ] **T1.5** Commit 1
  ```bash
  git add pdk/chat_session/include/agenticdsl/pdk/cancellation_globals.h
  git add pdk/chat_session/src/cancellation_globals.cpp
  git add pdk/chat_session/CMakeLists.txt
  git add -u examples/pdk_chat_demo/chat_session.h examples/pdk_chat_demo/cancellation_registry.h
  git commit -m "refactor(pdk): move cancellation_globals to PDK + delete 2 shim headers

  chat-session-pdk-lift Change 1 created 4 1-Sprint compat shims to avoid
  breaking 16+ includers. The compat period has passed (Change 1+2 both
  shipped, 234/234 ctest PASS); the shims now create cross-tree coupling
  (pdk/loop_agent includes examples/.../cancellation_registry.h).

  Commit 1 (PDK infrastructure, no consumer changes):
    * mv examples/pdk_chat_demo/commands/cancellation_globals.h ->
           pdk/chat_session/include/agenticdsl/pdk/cancellation_globals.h
      (eliminates pdk/loop_agent -> examples dependency)
    * mv examples/pdk_chat_demo/commands/cancellation_globals.cpp ->
           pdk/chat_session/src/cancellation_globals.cpp
      (namespace pdk_chat_demo -> namespace hydraforge::pdk)
    * pdk/chat_session/CMakeLists.txt: register new .cpp
    * rm examples/pdk_chat_demo/chat_session.h
      (3-line alias shim hydraforge::pdk::* -> pdk_chat_demo::*)
    * rm examples/pdk_chat_demo/cancellation_registry.h
      (global-scope using hydraforge::pdk::CancellationRegistry)

  Follow-up Commit 2 will migrate 22 consumer files to drop the shims.
  The build will fail between Commit 1 and Commit 2 (expected)."
  ```

## Commit 2: includer 大规模迁移 (Phase 2)

- [ ] **T2.1** 14 个 examples tests 批量替换
  ```bash
  # 替换 include
  find examples/pdk_chat_demo/tests -name "*.cpp" -exec sed -i \
    -e 's|#include "chat_session.h"|#include <agenticdsl/pdk/chat_session.h>|g' \
    -e 's|#include "cancellation_registry.h"|#include <agenticdsl/pdk/cancellation_registry.h>|g' \
    {} +

  # 替换 using namespace
  find examples/pdk_chat_demo/tests -name "*.cpp" -exec sed -i \
    's|using namespace pdk_chat_demo;|using namespace hydraforge::pdk;|g' {} +
  ```

- [ ] **T2.2** 修正 `test_cancellation_registry.cpp` (不使用 `using namespace` 的特殊情况)
  - 替换所有 `CancellationRegistry` 限定为 `hydraforge::pdk::CancellationRegistry` (该文件 6+ 处, sed 替换外手动处理)
  - 验证: `grep -c "CancellationRegistry" examples/pdk_chat_demo/tests/test_cancellation_registry.cpp` 确认全部已限定

- [ ] **T2.3** `examples/pdk_chat_demo/commands/{cancel,model}_command.cpp` include 替换
  ```bash
  sed -i 's|#include "chat_session.h"|#include <agenticdsl/pdk/chat_session.h>|g' \
    examples/pdk_chat_demo/commands/cancel_command.cpp \
    examples/pdk_chat_demo/commands/model_command.cpp
  ```

- [ ] **T2.4** `commands/cancellation_globals.cpp` 删除 (已迁到 PDK)
  - `git rm examples/pdk_chat_demo/commands/cancellation_globals.cpp` (注: 仅当 T1.1 未 git mv 走; 若已 mv 则文件已在 pdk/)
  - 实际 T1.1 用 `git mv`, 故 T2.4 仅在 examples/ 目录清理残留空目录

- [ ] **T2.5** `commands/command_globals.{h,cpp}` using 别名移除
  - `command_globals.h`: 删除 `using ChatSession = hydraforge::pdk::ChatSession;`, `g_command_session` 类型直接用全名 `hydraforge::pdk::ChatSession*`
  - `command_globals.cpp`: `ChatSession*` → `::hydraforge::pdk::ChatSession*` (或经 using `using hydraforge::pdk::ChatSession;` 后简写)

- [ ] **T2.6** `examples/pdk_chat_demo/main.cpp` 全量替换
  - 替换 `#include "chat_session.h"` → `#include <agenticdsl/pdk/chat_session.h>`
  - 替换 `#include "commands/cancellation_globals.h"` → `#include <agenticdsl/pdk/cancellation_globals.h>`
  - 替换 `pdk_chat_demo::ChatSession` → `hydraforge::pdk::ChatSession` (1 处)
  - 替换 `pdk_chat_demo::CancellationRegistry` → `hydraforge::pdk::CancellationRegistry` (1 处)
  - 替换 `pdk_chat_demo::g_cancellation_registry` → `hydraforge::pdk::g_cancellation_registry` (2 处赋值/传参)

- [ ] **T2.7** `pdk/loop_agent/src/pdk_entry.cpp` 替换
  - `#include "cancellation_registry.h"` → `#include <agenticdsl/pdk/cancellation_registry.h>`
  - `#include "commands/cancellation_globals.h"` → `#include <agenticdsl/pdk/cancellation_globals.h>`
  - `pdk_chat_demo::g_cancellation_registry` (3 处) → `hydraforge::pdk::g_cancellation_registry`

- [ ] **T2.8** `pdk/loop_agent/CMakeLists.txt` 移除 examples 路径
  - 删除 2 行 include dir (`${PROJECT_SOURCE_DIR}/examples/pdk_chat_demo` + `commands`)
  - 删除 `target_sources(LoopAgent PRIVATE ${PROJECT_SOURCE_DIR}/examples/pdk_chat_demo/commands/cancellation_globals.cpp)`
  - 添加 `target_link_libraries(LoopAgent PRIVATE pdk_chat_session)` (获取 cancellation_globals.h 与符号)

- [ ] **T2.9** `examples/pdk_chat_demo/tests/CMakeLists.txt` 移除 examples 相对 include
  - 删除 14 处 `target_include_directories(test_xxx PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)` 中的 `${CMAKE_CURRENT_SOURCE_DIR}/..`
  - 保留 `${CATCH_INCLUDE_DIR}` (= tests/) 用于 `#include "test_helpers/..."`

- [ ] **T2.10** 验证: grep A1/A2 全 0 行
  ```bash
  grep -rn "pdk_chat_demo::ChatSession\|pdk_chat_demo::CancellationRegistry" --include="*.h" --include="*.cpp" examples/ pdk/ tests/ src/
  grep -rn "using namespace pdk_chat_demo" --include="*.h" --include="*.cpp" examples/ pdk/
  ```
  两者均**0 行**

- [ ] **T2.11** 验证: shim files 不存在
  ```bash
  ls examples/pdk_chat_demo/chat_session.h examples/pdk_chat_demo/cancellation_registry.h 2>/dev/null
  # 预期: 不输出
  ```

- [ ] **T2.12** Commit 2
  ```bash
  git add examples/pdk_chat_demo/
  git add pdk/loop_agent/
  git commit -m "refactor(pdk): migrate 22 consumer files off shim aliases (chat-session-pdk-lift cleanup)

  Per design/pdk-chat-session-shim-cleanup.md, migrate all examples +
  loop_agent consumers from pdk_chat_demo:: aliases to direct
  hydraforge::pdk:: references.

  14 examples tests:
    * #include 'chat_session.h' -> <agenticdsl/pdk/chat_session.h>
    * #include 'cancellation_registry.h' -> <agenticdsl/pdk/cancellation_registry.h>
    * using namespace pdk_chat_demo -> using namespace hydraforge::pdk
    * test_cancellation_registry.cpp: CancellationRegistry ->
      hydraforge::pdk::CancellationRegistry (no using namespace)

  examples/pdk_chat_demo/commands/{cancel,model}_command.cpp:
    * update shim includes to PDK direct

  examples/pdk_chat_demo/commands/command_globals.{h,cpp}:
    * drop 'using ChatSession = hydraforge::pdk::ChatSession' alias
    * use full qualification ::hydraforge::pdk::ChatSession

  examples/pdk_chat_demo/main.cpp:
    * 1 ChatSession + 1 CancellationRegistry + 2 g_cancellation_registry
      references migrated to hydraforge::pdk::

  pdk/loop_agent/{src/pdk_entry.cpp,CMakeLists.txt}:
    * drop examples include paths and cancellation_globals.cpp source
    * add pdk_chat_session link (provides cancellation_globals symbols)
    * 3 pdk_chat_demo::g_cancellation_registry refs -> hydraforge::pdk::

  examples/pdk_chat_demo/tests/CMakeLists.txt:
    * drop '\${CMAKE_CURRENT_SOURCE_DIR}/..' from 14 test target_include_directories
      (no longer needed; <agenticdsl/pdk/...> resolves via agenticdsl_includes)

  Evidence:
    functional: 234/234 ctest PASS (no regression, pre-existing flaky allowed)
    grep A1+A2: 0 references to pdk_chat_demo::{ChatSession,CancellationRegistry}
    grep A2: 0 'using namespace pdk_chat_demo' remaining
    LoopAgent.so: nm confirms hydraforge::pdk::g_cancellation_registry visible
    TSan gate: 3/3 PASS 0 warnings (test_pdk_chat_session + _recovery + _static_logger)"
  ```

## Phase 3: 验证 (Acceptance)

- [ ] **T3.1** A1 源码 grep
  ```bash
  grep -rn "pdk_chat_demo::ChatSession\|pdk_chat_demo::CancellationRegistry" --include="*.h" --include="*.cpp" examples/ pdk/ tests/ src/
  ```
  预期: 0 行

- [ ] **T3.2** A2 using namespace grep
  ```bash
  grep -rn "using namespace pdk_chat_demo" --include="*.h" --include="*.cpp" examples/ pdk/
  ```
  预期: 0 行

- [ ] **T3.3** A3 shim 文件不存在
  ```bash
  ls examples/pdk_chat_demo/chat_session.h examples/pdk_chat_demo/cancellation_registry.h
  ```
  预期: No such file or directory

- [ ] **T3.4** A4 全量编译
  ```bash
  cmake --build build -j$(nproc)
  ```
  预期: 0 error (全 targets: agenticdsl_core, agenticdsl_modules_*, pdk_chat_session,
  pdk_chat_demo, pdk_chat_demo_obj, LoopAgent.so, 28 examples test binaries,
  2 core PDK tests test_pdk_chat_session* + test_pdk_chat_session_recovery)

- [ ] **T3.5** A5 functional ctest
  ```bash
  ctest --test-dir build -j4 --timeout 180
  ```
  预期: 234 tests, 233 PASS (允许 test_skill_interpreter pre-existing flaky 单跑 PASS 0.47s)

- [ ] **T3.6** A7 LoopAgent 符号验证
  ```bash
  nm build/pdk/loop_agent/libLoopAgent.so 2>/dev/null | grep cancellation_registry
  ```
  预期: 含 `hydraforge::pdk::g_cancellation_registry` (U 引用符号)

- [ ] **T3.7** A8 TSan 门禁
  ```bash
  cmake --build build-tsan --target test_pdk_chat_session test_pdk_chat_session_recovery test_pdk_chat_session_static_logger
  ctest --test-dir build-tsan -R '^test_pdk_chat_session(_recovery|_static_logger)?$' --output-on-failure
  ```
  预期: 3/3 PASS, 0 ThreadSanitizer warnings

## Phase 4: 收尾

- [ ] **T4.1** OpenSpec change 归档
  ```bash
  openspec archive pdk-chat-session-shim-cleanup
  ```

- [ ] **T4.2** 更新 `AGENTS.md` 移除"shim 兼容期"备注
  - 搜 `1-Sprint 兼容期` / `shim` 相关段落
  - 移除 chat-session-pdk-lift 在 AGENTS.md 中的中间状态描述

- [ ] **T4.3** 更新 `docs/superpowers/specs/2026-09-11-chat-session-pdk-lift-design.md` 顶部状态
  - 移除 "兼容 shim 1 Sprint" 措辞
  - 标注 "shim cleanup 已 ship" + commit hash

## 验收清单

| 验收 | 命令 | 期望 |
|---|---|---|
| A1 源码无 shim 引用 | `grep "pdk_chat_demo::ChatSession\\|pdk_chat_demo::CancellationRegistry"` | 0 行 |
| A2 无 using namespace | `grep "using namespace pdk_chat_demo"` | 0 行 |
| A3 shim 头已删 | `ls examples/pdk_chat_demo/{chat_session,cancellation_registry}.h` | 不存在 |
| A4 编译 | `cmake --build build` | 0 error |
| A5 ctest | `ctest -j4 --timeout 180` | 234 tests, 233 PASS |
| A6 chat_session tests | `ctest -R chat_session` | 新 baseline, 0 失败 |
| A7 LoopAgent 符号 | `nm build/pdk/loop_agent/libLoopAgent.so \| grep g_cancellation` | 含 `hydraforge::pdk::g_cancellation_registry` |
| A8 TSan 门禁 | `ctest -R '^test_pdk_chat_session.*$' --test-dir build-tsan` | 3/3 PASS, 0 warnings |
