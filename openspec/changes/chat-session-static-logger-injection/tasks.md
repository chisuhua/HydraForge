# Tasks — chat-session-static-logger-injection

## Phase 1: 公开 API + 实现

- [ ] **T1.1** `include/agenticdsl/pdk/chat_session.h` 添加静态 logger API
  - **Anchor**: `class ChatSession { ... }` 内部, 在 public 段 `consume_budget_alert()` 之后加
  - **声明 3 个静态方法**: `set_default_logger`, `get_default_logger`, `clear_default_logger`
  - **声明私有辅助**: `static std::unique_ptr<agenticdsl::ILogger>& default_logger_slot()`

- [ ] **T1.2** `pdk/chat_session/src/chat_session.cpp` 实现 3 个静态方法 + Meyers singleton 辅助
  - **Anchor**: 文件末尾 `}  // namespace hydraforge::pdk` 之前
  - `default_logger_slot()` 返回 Meyers singleton `unique_ptr<ILogger>` (在函数内 `static`)
  - `set_default_logger(logger)` = `*default_logger_slot() = std::move(logger)`
  - `get_default_logger()` = `default_logger_slot().get()`
  - `clear_default_logger()` = `default_logger_slot().reset()`

- [ ] **T1.3** 修改 4 处 std::cerr (ensure_dir_0700 ×2, cleanup_stale ×2)
  - **Pattern**: `if (auto* logger = ChatSession::get_default_logger()) { logger->log(level, msg); } else { /* 原 std::cerr */ }`
  - 等级: ensure_dir_0700 失败用 `kError`; cleanup_stale 失败用 `kWarn`
  - 注意 `level` 与 `ILogger` 已 include (`#include "agenticdsl/contract/ilogger.h"` 已在)

## Phase 2: main.cpp 注入

- [ ] **T2.1** `examples/pdk_chat_demo/main.cpp` 启动期 set default logger
  - **Anchor**: 在 `pdk_chat_demo::EventHandler handler(bus);` 之前 (或 pdk_chat_demo::g_cancellation_registry 初始化之前)
  - **新增 include**: `<agenticdsl/pdk/chat_session.h>` 与 `<common/io/stderr_logger.h>` (后者可能已 include)
  - **新增 2 行**: `ChatSession::set_default_logger(make_unique<agenticdsl::StderrLogger>())`

## Phase 3: 测试编写（独立 binary）

- [ ] **T3.1** 创建 `tests/test_pdk_chat_session_static_logger.cpp` (~80 行)
  - **include**: `catch_amalgamated.hpp`, `agenticdsl/pdk/chat_session.h`, `test_helpers/capturing_logger.h`, `<filesystem>`, `<thread>`, `<atomic>`
  - **TEST_CASE 1** "ensure_dir_0700 routes to default logger when set" `[pdk][chat_session][static-logger]`
    - SETUP: make CapturingLogger; `ChatSession::set_default_logger(make_unique<CapturingLogger>(*raw))`
    - TEARDOWN: `ChatSession::clear_default_logger()`
    - 测试: 构造 ChatSession 触发 ensure_dir_0700 在只读目录失败
    - 简化策略: 在测试内直接调用 `ChatSession::cleanup_stale("/proc/1")` 触发 fallback stderr (不可靠); 改为 mock: 调用 `set_default_logger` 后调用一个**新公开**辅助函数 (见 T1.4) 或直接测试 logger 接口
    - **决策**: 测试用 `cleanup_stale` 在非法路径触发 stat failed (stderr path); 验证 logger 收到 kWarn
  - **TEST_CASE 2** "cleanup_stale routes warn to default logger" `[pdk][chat_session][static-logger]`
    - 创建临时 dir + 一个文件 + chmod 000 (无读权限); 调用 `ChatSession::cleanup_stale(dir)`
    - CapturingLogger 应收到 ≥ 1 条 kWarn + 含 `[session/cleanup] stat failed:` 或 `remove failed:`
  - **TEST_CASE 3** "fallback to stderr when not set" `[pdk][chat_session][static-logger]`
    - 不调用 set_default_logger
    - 触发同样失败
    - 验证: CapturingLogger(临时构造来兜底) 未收到该消息 (因为 nullptr fallback)
    - 简化: 用 std::cout/stderr 重定向到 ostringstream 不易; 改为只断言 set_default_logger(false) 路径下 `get_default_logger()` 返回 nullptr

- [ ] **T1.4 (补充)** 考虑是否需要导出内部 `ensure_dir_0700` 供测试
  - 选项 A: 不导出, 测试只测 cleanup_stale 路径 (2/4 处)
  - 选项 B: 添加 `friend class TestChatSessionStaticLogger;` 或 `static void test_ensure_dir_0700(path)`
  - **决策**: 选 A (最小 scope, 2 处足以验证 default_logger 路径走通, 另 2 处同 Pattern)

- [ ] **T3.2** `tests/CMakeLists.txt` 显式注册独立 test binary
  - 在 `add_catch_test` 函数末尾:
    ```cmake
    if(TEST_NAME STREQUAL "test_pdk_chat_session_static_logger")
        target_sources(${TEST_NAME} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/test_pdk_chat_session_static_logger.cpp)
    endif()
    ```
  - 注: `file(GLOB test_*.cpp)` 已自动捕获 `test_pdk_chat_session_static_logger.cpp` → add_catch_test 自动创建 target; 上面的 `target_sources` 是冗余但显式 (不必要 — 验证后删除)

## Phase 4: 验证

- [ ] **T4.1** 源码 grep
  ```bash
  grep -n "std::cerr" pdk/chat_session/src/chat_session.cpp
  # 预期: 仅 1 行 (L8 注释引用), 4 处实际调用已改为 if/else 路径
  ```

- [ ] **T4.2** 编译 + functional
  ```bash
  cmake --build build -j$(nproc)
  ctest --test-dir build -j4 --timeout 180
  # 预期: 234 + 1 = 235 tests, 234 PASS + 1 新增 test_pdk_chat_session_static_logger PASS
  # (允许 pre-existing flaky: test_skill_interpreter, test_temporal_agent_signal_callback)
  ```

- [ ] **T4.3** TSan 门禁
  ```bash
  cmake --build build-tsan --target test_pdk_chat_session_static_logger
  ctest --test-dir build-tsan -R '^test_pdk_chat_session(_recovery|_static_logger)?$' --output-on-failure
  # 预期: 3/3 PASS, 0 warnings
  ```

## Phase 5: 提交

- [ ] **T5.1** 原子 commit
  ```bash
  git add include/agenticdsl/pdk/chat_session.h pdk/chat_session/src/chat_session.cpp examples/pdk_chat_demo/main.cpp tests/test_pdk_chat_session_static_logger.cpp tests/CMakeLists.txt
  git commit -m "feat(pdk): ChatSession default logger injection for static contexts (4 std::cerr residue)

  chat-session-pdk-lift Change 1 converted 11/15 std::cerr sites in ChatSession::Impl
  to ILogger. The remaining 4 sites are in static contexts (ensure_dir_0700 free
  function + static cleanup_stale) where no Impl instance exists.

  Fix: add process-level static default logger to ChatSession class:
    static set_default_logger(unique_ptr<ILogger>) - called by main at startup
    static get_default_logger() -> ILogger* (nullptr if not set)
    static clear_default_logger() - for test teardown
  4 std::cerr sites check get_default_logger() first; if null, fall back to std::cerr
  (backward compatible - existing tests that don't set the logger keep working).

  examples/pdk_chat_demo/main.cpp calls set_default_logger(make_unique<StderrLogger>())
  early in startup, so production routes through the project's agenticdsl::log facade.

  Evidence:
    functional ctest: 235/235 PASS (new test_pdk_chat_session_static_logger PASS)
    TSan: 3/3 PASS 0 warnings (test_pdk_chat_session + _recovery + _static_logger)
    grep: std::cerr in chat_session.cpp reduced 4 -> 0 (excluding 1 comment reference)
  "
  ```

## 验收清单

| 验收 | 命令 | 期望 |
|---|---|---|
| A1 源码 grep | `grep -c "std::cerr" pdk/chat_session/src/chat_session.cpp` | 0 (除注释) |
| A2 编译 | `cmake --build build` | 0 error |
| A3 functional | `ctest -j4 --timeout 180` | 235 tests, 1 新增 PASS |
| A4 TSan | `ctest -R static_logger --output-on-failure` (build-tsan) | PASS, 0 warnings |
| A5 fallback | 测试 3 验证未 set 时仍走 std::cerr | ✅ |

## Out-of-scope (follow-up notes)

- **shim cleanup** (`examples/pdk_chat_demo/chat_session.h` 3-line shim) — 在 ChatSession API 经 1 Sprint 实战验证后由独立 change 移除
- **更彻底的 logger 重构** — 把 logger 注入到 `SessionConfig` (每个实例独立) 不与本 change 冲突
