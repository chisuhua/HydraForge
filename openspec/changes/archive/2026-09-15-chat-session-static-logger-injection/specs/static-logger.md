# Spec — chat-session-static-logger-injection

## ADDED Requirements

### R1: ChatSession 进程级 default logger API

`ChatSession` 类提供静态方法管理一个**进程级**的 `ILogger` 共享实例。该实例被静态上下文（`ensure_dir_0700`, `cleanup_stale` 等无 `Impl` 实例的方法）使用。

#### API

```cpp
namespace hydraforge::pdk {

class ChatSession {
 public:
  // 设置进程级 default logger。重复调用会替换前一个 logger
  // (unique_ptr 所有权转移)。nullptr 表示清空 (fallback 到 std::cerr)。
  // 调用方负责生命周期: 通常在 main 启动期 set 一次, 进程结束前不必 clear。
  static void set_default_logger(std::unique_ptr<agenticdsl::ILogger> logger);

  // 获取当前 default logger。返回 nullptr 表示未 set。
  // 用于 ensure_dir_0700, cleanup_stale 等静态上下文判断是否走 ILogger 路径。
  static agenticdsl::ILogger* get_default_logger();

  // 显式清空 default logger (主要用于测试 teardown)。
  static void clear_default_logger();

  // ... existing public API ...
};

}  // namespace hydraforge::pdk
```

**实现约束**:
- 使用 Meyers singleton (`static` 局部变量在函数内), C++11 magic statics 保证线程安全初始化
- 静态成员为 `std::unique_ptr<agenticdsl::ILogger>`, 不裸指针
- 无锁访问 (假设 set 与 get 不并发; main 启动期 set, 之后只读)

**NOTE (指针生命周期)**: `get_default_logger()` 返回的裸指针**仅在**当前没有并发的 `set_default_logger()` / `clear_default_logger()` 调用期间有效。调用方必须保证: (a) 持有指针期间不调 set/clear; (b) 在持有指针前完成一次 `get_default_logger()` 调用取快照; (c) 多线程场景下使用原子读写或外部同步。本约束的典型应用模式: main 启动期 `set_default_logger(make_unique<StderrLogger>())` 一次性设置, 之后所有线程只读 (本项目当前模式); 测试 setup/teardown 严格串行 (Catch2 单 binary 顺序执行)。

**NOTE (singleton per binary)**: Meyers singleton 存在于每个 link 单元独立。`pdk_chat_session` 是 STATIC+PIC，host binary + `libLoopAgent.so` 各自持有副本。当前 host binary 是唯一调用方; 若未来 `.so` 内调 `cleanup_stale`，需在 `.so` 内独立 set。

**SCENARIO R1.1**: 设置非 nullptr logger
- **GIVEN** `ChatSession::set_default_logger(make_unique<CapturingLogger>(...))`
- **WHEN** 调用 `get_default_logger()`
- **THEN** 返回 CapturingLogger 指针 (非 nullptr)
- **AND** 该指针与 set 时构造的 CapturingLogger 是同一对象

**SCENARIO R1.2**: 设置 nullptr 等价于 clear
- **GIVEN** 已 set 一个 logger
- **WHEN** `set_default_logger(nullptr)`
- **THEN** `get_default_logger()` 返回 nullptr

**SCENARIO R1.3**: 替换前一个 logger
- **GIVEN** 已 set logger A
- **WHEN** `set_default_logger(logger_B)` (logger_B 与 logger_A 不同)
- **THEN** `get_default_logger()` 返回 logger_B
- **AND** logger_A 被析构 (无泄漏)

**SCENARIO R1.4**: 线程安全一次性初始化
- **GIVEN** 多个线程 (N=8) 同时首次调用 `get_default_logger()`
- **WHEN** 每个线程观察到结果
- **THEN** 均返回同一指针 (Meyers singleton 语义)
- **由** `tests/test_pdk_chat_session_static_logger.cpp` `TEST_CASE "concurrent first call returns same pointer"` 验证 (8 jthread 并发 `get_default_logger()`, 断言全部 raw 指针相等)。

### R2: 4 处 std::cerr 静态上下文走 default logger

`ensure_dir_0700` 与 `cleanup_stale` 内的 4 处诊断输出经 `detail::log_static_diag(level, msg)` 路由 helper 统一管理: 已 set logger 时走 ILogger, 未 set 时 fallback `std::cerr`。

**SCENARIO R2.1**: 路由 helper — set 路径走 ILogger
- **GIVEN** 已 set CapturingLogger 作为 default logger
- **WHEN** 调用 `detail::log_static_diag(kError, "[session] create_directories failed: /x")`
- **THEN** CapturingLogger 收到 1 条 `LogLevel::kError` 级日志 + 消息精确等于输入
- **由** `TEST_CASE "log_static_diag routes to ILogger when set"` 单测验证 (确定性强, 不依赖失败注入)
- **注**: 原始 spec R2.1 GIVEN 直接调 `ensure_dir_0700("/proc/1/protected")` 触发失败 — 该函数在 `chat_session.cpp` anonymous namespace，测试不可达; tasks T1.4 已决策 Option A (不导出)。本 SCENARIO 改走 detail helper 直接单测路由逻辑，等价覆盖。

**SCENARIO R2.2**: cleanup_stale stat failed → log path
- **GIVEN** 已 set default logger
- **AND** 临时 dir 含损坏的 `.json` 文件 (stat 失败)
- **WHEN** `ChatSession::cleanup_stale(dir)`
- **THEN** CapturingLogger 收到 ≥ 1 条 `kWarn` 级日志
- **AND** 消息含 `[session/cleanup] stat failed:`

**SCENARIO R2.3**: fallback std::cerr (向后兼容)
- **GIVEN** 未 set default logger
- **WHEN** 触发 ensure_dir_0700 失败 / cleanup_stale 失败
- **THEN** 行为**不变**: 仍输出到 `std::cerr`
- **AND** `get_default_logger()` 返回 nullptr

**SCENARIO R2.4**: set 后立即生效
- **GIVEN** 未 set
- **WHEN** `set_default_logger(logger)` 后立即 `cleanup_stale(dir)` 触发
- **THEN** logger 收到日志 (无需任何额外同步)

## MODIFIED Requirements

### M1: examples/pdk_chat_demo/main.cpp 启动期 set default logger

**新增** `main.cpp` 启动早期（约 `EventHandler handler(bus);` 之前）:
```cpp
// chat-session-static-logger-injection: 启动期 set 进程级 default logger,
// 覆盖 ensure_dir_0700 与 cleanup_stale 的 4 处 std::cerr (lift 残余)
hydraforge::pdk::ChatSession::set_default_logger(
    std::make_unique<agenticdsl::StderrLogger>());
```

**SCENARIO M1.1**: `--mock` 运行验证
- **GIVEN** 已 set default logger (main.cpp)
- **WHEN** 启动 pdk_chat_demo 与无效 `--session <bad-id>` (触发 ensure_dir_0700 失败或 cleanup_stale)
- **THEN** 诊断经 `agenticdsl::log::emit` 输出 `[WARN]/[ERROR]` tag (与项目其它日志格式一致)
- **AND** 进程级 stderr 仍可见 (StderrLogger 桥接回 log facade)

## UNCHANGED Requirements

- `ChatSession` 构造 / `chat()` / `request_stop()` / 队列 / 持久化 API **不变**
- 现有所有 ctest 不修改; 默认 nullptr 时所有行为字节级一致
- examples `chat_session.h` shim / `cancellation_registry.h` shim **不变**

## 风险验收

| 风险 | 验收 |
|---|---|
| 静态成员初始化顺序问题 | 测试 R1.4 多线程并发首次访问均得同一指针 ✅ |
| set 与 get 在不同线程并发 | 当前使用模式 (main 启动期 set, 之后只读) 无此场景; 文档化约束 |
| 旧调用方依赖 std::cerr 文本 | R2.3 fallback 路径保留 ✅ |
