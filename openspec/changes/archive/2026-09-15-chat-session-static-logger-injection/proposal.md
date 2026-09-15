## Why

`chat-session-pdk-lift Change 1` 把 `ChatSession` 与 `CancellationRegistry` 提升到 PDK `hydraforge::pdk` 命名空间，并引入 `IInputSource`/`ILogger` 抽象层。`Impl` 内的 11 处 `std::cerr` 已迁移到 `logger_->log(...)`（见 `docs/superpowers/plans/2026-09-11-chat-session-pdk-lift-change1.md` B3 偏差记录）。

**遗留 4 处 `std::cerr`** 位于**静态上下文**（无 `Impl` 实例 → 无 `logger_` 访问）：

| 行号 (`pdk/chat_session/src/chat_session.cpp`) | 上下文 | 原因 |
|---|---|---|
| L78 | `ensure_dir_0700` (anonymous namespace 自由函数) | 无 `Impl` 实例 → 无法 `impl_->logger_` |
| L83 | 同上 | 同上 |
| L717 | `static ChatSession::cleanup_stale` (public static 成员) | static 方法无 `this` → 无实例 logger |
| L725 | 同上 | 同上 |

**为什么这 4 处值得修**：
1. **一致性问题**：lift 后所有日志均经 `ILogger` 桥接 (`StderrLogger` → `agenticdsl::log::emit`)，唯独这 4 处走原生 `std::cerr`，**绕过项目的 log facade**（违反 ADR-0068 §3.1 "emit 必须经 EventBuilder / log facade"）
2. **可观测性问题**：测试不能拦截 `std::cerr` 输出（vs `ILogger` 有 `CapturingLogger` 替身）
3. **运维集成问题**：若项目接入集中式日志后端（OpenTelemetry, Loki 等），这 4 处不会出现在 trace 中

**为什么不扩大 scope**：
- 这是 **follow-up 而非 change 主线**：lift 的核心 11 处已迁移（覆盖率 73%），其余 4 处是 edge case
- 不修改 `ensure_dir_0700` 与 `cleanup_stale` 的**公共签名**（向后兼容）
- 不引入新依赖（用既有 `ILogger` 契约）

**Oracle 优先级**: `LOW`（生产输出确实会到达 stderr 但不影响功能正确性；lift 的 11/15 处已经解决主要问题）

## What Changes

- **新增** `ChatSession` 类的静态方法 `set_default_logger(std::unique_ptr<ILogger>)` 与静态访问 `get_default_logger()`，支持进程级 logger 注入
- **修改** `ensure_dir_0700` 与 `cleanup_stale` 内的 4 处 `std::cerr`：抽 detail 级路由 helper `hydraforge::pdk::detail::log_static_diag(level, msg)`，先查 `ChatSession::get_default_logger()`，若有则 `log(level, ...)`，否则 fallback `std::cerr`（向后兼容 — 旧调用方未 set 时行为不变）。等级选择: `ensure_dir_0700` 失败用 `kError`(disk/perm 严重错误), `cleanup_stale` 失败用 `kWarn`(transient cleanup)。
- **新增** main.cpp 启动期 `ChatSession::set_default_logger(std::make_unique<StderrLogger>())` 调用，确保生产环境走 ILogger 路径
- **新增** `tests/test_pdk_chat_session_static_logger.cpp`（独立 binary）：验证 default logger 注入后 4 处 `std::cerr` 不再直接输出

### Non-goals

- **不** 把 `ensure_dir_0700` 改为实例方法（是 free function，调用方多；保留 O(1) helper 形态）
- **不** 把 `cleanup_stale` 改为非 static（仅 main.cpp 调用，重构风险高于收益）
- **不** 引入线程局部（thread_local）logger（不必要的复杂度；进程级足够）
- **不** 影响 examples 兼容 shim（examples `chat_session.h` 仍是 3 行 alias）

## 影响面

| 类型 | 范围 |
|---|---|
| 生产代码 | `pdk/chat_session/src/chat_session.cpp` ~6 行修改 + `include/agenticdsl/pdk/chat_session.h` +2 行声明 |
| 新测试 | `tests/test_pdk_chat_session_static_logger.cpp` ~70 行 |
| main.cpp | +2 行 (`ChatSession::set_default_logger` 调用) |
| 公开 API | **+2 静态方法** (`set_default_logger` / `get_default_logger`)，**无 breaking** |
| ABI | **+2 符号** (静态方法) — 极小增量 |
| 现有测试 | **0 改动** (无 default logger 时 fallback `std::cerr` 行为不变) |

## 验收

- **A1** 结构性断言: 4 处 `std::cerr` 均被 `detail::log_static_diag()` helper 包裹 (即每处都是 `detail::log_static_diag(level, msg)` 调用)。grep 基线: `grep -c "std::cerr" pdk/chat_session/src/chat_session.cpp` = 2 (1 注释 L8 + 1 helper fallback L1035)。helper 抽离的好处: 4 处 std::cerr 共享 1 个 fallback 分支, 字符串构造集中, 可单测确定性 100%。
- **A2** `cmake --build build` 全部 target 编译通过
- **A3** `ctest --test-dir build` 234/234 PASS + 新增 `test_pdk_chat_session_static_logger` PASS
- **A4** `test_pdk_chat_session_static_logger` 通过 `CapturingLogger` 捕获: (a) R1.1-R1.3 set/get/clear round-trip 验证; (b) R1.4 N-thread 并发首调验证 Meyers singleton 一致性; (c) R2.x 通过 `detail::log_static_diag` 路由 helper 确定性单测（set → capture / unset → fallback），不依赖失败注入。
- **A5** 未调 `set_default_logger` 时，4 处仍输出到 `std::cerr`（向后兼容 — 旧 `tests/cleanup_stale` 等调用点不变）— 由 R2.3 fallback test 验证。

## 风险

| 风险 | 等级 | 缓解 |
|---|---|---|
| 静态成员初始化顺序（static `unique_ptr<ILogger>` 在 main 启动前被读） | 低 | Meyers singleton (C++11 magic statics) 保证线程安全初始化；R1.4 多线程并发首调测试实证 |
| `get_default_logger()` 返回裸指针，调用方误用导致悬垂 | 中 | spec R1 增加 NOTE: "返回指针仅在 set/clear 之间有效"; 调用方不得在持有指针期间并发执行 set/clear |
| 4 处 stderr 输出格式与旧略有差异（带 `[session]` 前缀 vs `log::emit` 的 `[INFO]`/`[WARN]` tag） | 低 | test cases 只断言消息内容含 `[session]`，不强制完整格式 |
| 旧调用方直接依赖 `std::cerr` 文本捕获（罕见） | 低 | fallback 路径保留，行为不变 |
| **singleton per binary**: `pdk_chat_session` 是 STATIC+PIC，链接到 host binary + `libLoopAgent.so`; 每个二进制/.so 拥有独立的 Meyers singleton 副本 | 低 | spec 约束文档化：`.so` 内调 `cleanup_stale`/`ensure_dir_0700` 不会看到 host set 的 logger; 当前 `.so` 未触发此路径 (loop_agent 仅引用 cancellation_globals)，但 spec 显式记录 |
