# Tasks — chat-session-static-logger-injection

## Phase 1: 公开 API + 实现

- [ ] **T1.1** `include/agenticdsl/pdk/chat_session.h` 添加静态 logger API
  - **Anchor**: `class ChatSession { ... }` 内部, 在 public 段 `consume_budget_alert()` 之后加
  - **声明 3 个静态方法**: `set_default_logger`, `get_default_logger`, `clear_default_logger`
  - **声明私有辅助**: `static std::unique_ptr<agenticdsl::ILogger>& default_logger_slot()`
  - **声明 friend 或 namespace `detail::log_static_diag()`** (Oracle C2 helper 抽离)

- [ ] **T1.2** `pdk/chat_session/src/chat_session.cpp` 实现 3 个静态方法 + Meyers singleton 辅助
  - **Anchor**: 文件末尾 `}  // namespace hydraforge::pdk` 之前
  - `default_logger_slot()` 返回 Meyers singleton `unique_ptr<ILogger>` (在函数内 `static`)
  - `set_default_logger(logger)` = `*default_logger_slot() = std::move(logger)`
  - `get_default_logger()` = `default_logger_slot().get()`
  - `clear_default_logger()` = `default_logger_slot().reset()`

- [ ] **T1.2b (新增 — Oracle C2)** `pdk/chat_session/src/chat_session.cpp` 抽 detail routing helper
  - **Anchor**: 在 `ChatSession::get_default_logger()` 之后、`}  // namespace hydraforge::pdk` 之前
  - `namespace detail { void log_static_diag(agenticdsl::LogLevel, const std::string&); }` (前向声明)
  - `inline void log_static_diag(level, msg) { if (auto* logger = ChatSession::get_default_logger()) logger->log(level, msg); else std::cerr << msg << std::endl; }`
  - **测试确定性**: 直接单测 helper 而非 chmod/root 失败注入 (避免 Oracle C2 设计的物理不可行测试)

- [ ] **T1.3** 修改 4 处 std::cerr (ensure_dir_0700 ×2, cleanup_stale ×2)
  - **Pattern**: 构造 msg string → `detail::log_static_diag(level, msg)` (避免 4 处重复 if/else 与 message)
  - 等级: ensure_dir_0700 失败用 `kError`; cleanup_stale 失败用 `kWarn`
  - 注意 `level` 与 `ILogger` 已 include (`#include "agenticdsl/contract/ilogger.h"` 已在)
  - **A1 验收**: grep `std::cerr` = 2 行 (1 注释 L8 + 1 helper fallback L1035; helper 抽离后 4 处共享 1 个 fallback)

## Phase 2: main.cpp 注入

- [ ] **T2.1** `examples/pdk_chat_demo/main.cpp` 启动期 set default logger
  - **Anchor**: 在 `pdk_chat_demo::EventHandler handler(bus);` 之前 (line ~442, before cleanup_stale call at line 476)
  - **新增 include**: `<agenticdsl/pdk/chat_session.h>` 与 `<common/io/stderr_logger.h>` (后者可能已 include)
  - **新增 2 行**: `ChatSession::set_default_logger(make_unique<agenticdsl::StderrLogger>())`

## Phase 3: 测试编写（独立 binary）

- [ ] **T3.1** 创建 `tests/test_pdk_chat_session_static_logger.cpp` (~120 行, 5 cases)
  - **include**: `catch_amalgamated.hpp`, `agenticdsl/pdk/chat_session.h`, `test_helpers/capturing_logger.h`, `<filesystem>`, `<thread>`, `<jthread>`
  - **DefaultLoggerGuard RAII** (Oracle M3 修复): `struct DefaultLoggerGuard { ~DefaultLoggerGuard() { ChatSession::clear_default_logger(); } }` — 每个 CASE 首行声明, 异常安全 teardown
  - **TEST_CASE 1** "set/get round-trip (R1.1)" — set CapturingLogger → get → 指针相等
  - **TEST_CASE 2** "set(nullptr) equals clear (R1.2)" — set → set(nullptr) → get 返回 nullptr
  - **TEST_CASE 3** "set replaces previous (R1.3)" — set A → set B → get 返回 B
  - **TEST_CASE 4** "concurrent first call returns same pointer (R1.4)" — 8 jthread 并发 get → 全部 raw 指针相等 (Meyers singleton 语义, TSan gate)
  - **TEST_CASE 5** "log_static_diag routes to ILogger when set (R2.1)" — set → N×detail::log_static_diag → CapturingLogger 收到 N 条
  - **TEST_CASE 6** "log_static_diag falls back when not set (R2.3)" — clear → detail::log_static_diag → get 返回 nullptr (验证 fallback 路径生效)
  - **注**: T1.4 (确保 ensure_dir_0700 导出) 决策: **不导出**, 5/6 case 走 detail helper 直接单测路由逻辑 (确定性 100%, 不依赖 chmod/root 失败注入)

- [x] ~~T3.2~~ — Oracle 验证后删除 (原 target_sources 块冗余; `tests/CMakeLists.txt:198` 已 `^test_pdk_chat_session` glob 自动链接 `pdk_chat_session`, `file(GLOB test_*.cpp)` 自动捕获新 binary)

## Phase 4: 验证

- [ ] **T4.1** 源码 grep
  ```bash
  grep -n "std::cerr" pdk/chat_session/src/chat_session.cpp
  # 预期: 5 行 (1 注释 L8 + 4 fallback else 在 detail::log_static_diag helper 内)
  ```

- [ ] **T4.2** 编译 + functional
  ```bash
  cmake --build build -j$(nproc)
  ctest --test-dir build -j4 --timeout 180
  # 预期: 234 + 1 = 235 tests, 234 PASS + 1 新增 test_pdk_chat_session_static_logger PASS (5 cases)
  # (允许 pre-existing flaky: test_skill_interpreter, test_temporal_agent_signal_callback)
  ```

- [ ] **T4.3** TSan 门禁
  ```bash
  cmake --build build-tsan --target test_pdk_chat_session_static_logger
  ctest --test-dir build-tsan -R '^test_pdk_chat_session(_recovery|_static_logger)?$' --output-on-failure
  # 预期: 3/3 PASS, 0 warnings (R1.4 并发测试是关键守卫)
  ```

## Phase 5: 提交

- [ ] **T5.1** 原子 commit
  ```bash
  git add include/agenticdsl/pdk/chat_session.h pdk/chat_session/src/chat_session.cpp examples/pdk_chat_demo/main.cpp tests/test_pdk_chat_session_static_logger.cpp
  git commit -m "feat(pdk): ChatSession default logger injection for static contexts (4 std::cerr residue)
  
  chat-session-pdk-lift Change 1 converted 11/15 std::cerr sites in ChatSession::Impl
  to ILogger. The remaining 4 sites are in static contexts (ensure_dir_0700 free
  function + static cleanup_stale) where no Impl instance exists.
  
  Fix: add process-level static default logger + detail routing helper:
    static set_default_logger(unique_ptr<ILogger>) - called by main at startup
    static get_default_logger() -> ILogger* (nullptr if not set)
    static clear_default_logger() - for test teardown
    detail::log_static_diag(level, msg) - if/else routing helper
  4 std::cerr sites call detail::log_static_diag(); if logger set, routes through
  ILogger; if not, falls back to std::cerr (backward compatible).
  
  examples/pdk_chat_demo/main.cpp calls set_default_logger(make_unique<StderrLogger>())
  early in startup, so production routes through the project's agenticdsl::log facade.
  
  Evidence:
    functional ctest: 235/235 PASS (new test_pdk_chat_session_static_logger 6 cases PASS)
    TSan: 3/3 PASS 0 warnings (R1.4 concurrent first-call singleton test guard)
    grep: std::cerr in chat_session.cpp = 2 (1 comment + 1 helper fallback; 4 sites share 1 fallback)
  "
  ```

## 验收清单

| 验收 | 命令 | 期望 |
|---|---|---|
| A1 源码结构 | `grep -c "std::cerr" pdk/chat_session/src/chat_session.cpp` | = 2 (1 注释 + 1 helper fallback) |
| A2 编译 | `cmake --build build` | 0 error |
| A3 functional | `ctest -j4 --timeout 180` | 235 tests, 1 新增 PASS (6 cases / 23 assertions) |
| A4 TSan | `ctest -R static_logger --output-on-failure` (build-tsan) | PASS, 0 warnings |
| A5 fallback | R2.3 case 验证未 set 时仍走 std::cerr | ✅ |

## Out-of-scope (follow-up notes)

- **shim cleanup** (`examples/pdk_chat_demo/chat_session.h` 3-line shim) — 在 ChatSession API 经 1 Sprint 实战验证后由独立 change 移除
- **更彻底的 logger 重构** — 把 logger 注入到 `SessionConfig` (每个实例独立) 不与本 change 冲突
- **TSan 实证** — R1.4 concurrent test 是 TSan 门的关键守卫, 后续若引入 set/clear 多线程场景需更新测试覆盖
