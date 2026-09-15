# Design — chat-session-static-logger-injection

## 现状

`pdk/chat_session/src/chat_session.cpp` 4 处 `std::cerr`（lift Change 1 后的剩余）：

```
L78: std::cerr << "[session] create_directories failed: " << dir
                << " (" << ec.message() << ")" << std::endl;
L83: std::cerr << "[session] chmod 0700 failed: " << dir << std::endl;

L717: std::cerr << "[session/cleanup] stat failed: " << entry.path() << std::endl;
L725: std::cerr << "[session/cleanup] remove failed: " << entry.path()
                  << ": " << rm_ec.message() << std::endl;
```

**问题**：
- 绕过项目 `agenticdsl::log::emit` facade（违反 ADR-0068 §3.1）
- 测试无法拦截（`CapturingLogger` 不捕获 `std::cerr`）
- 进程级日志后端集成（如 OpenTelemetry export）会漏掉这 4 处

**约束**：
- 这 4 处位于**静态上下文**（anonymous namespace 自由函数 + public static 成员函数），无 `Impl` 实例 → 无 `impl_->logger_`
- 不得修改 `cleanup_stale` 的**公共签名**（examples/pdk_chat_demo main.cpp 调用方依赖）

## 设计方案

### 进程级 static logger 持有者

在 `ChatSession` 类内添加**静态**成员 + 静态访问函数：

```cpp
// include/agenticdsl/pdk/chat_session.h (公开 API)
class ChatSession {
 public:
  // ... existing ...

  // 进程级 logger (用于静态上下文: ensure_dir_0700, cleanup_stale 等无 Impl 实例处)
  // nullptr 时 fallback std::cerr (向后兼容 — 旧代码未 set 时行为不变)
  static void set_default_logger(std::unique_ptr<agenticdsl::ILogger> logger);
  static agenticdsl::ILogger* get_default_logger();  // nullptr if not set
  static void clear_default_logger();              // 析构 reset 用

 private:
  // ... existing ...
  static std::unique_ptr<agenticdsl::ILogger>& default_logger_slot();
};
```

**为何 `static std::unique_ptr<ILogger>` 而非 `std::atomic<ILogger*>`**：
- `set_default_logger` 调用方（main 启动期 + 测试 setup/teardown）**不会**与 `get_default_logger` 并发
- `unique_ptr` 拥有权明确，避免 raw pointer 释放责任。**所有权语义**: `set_default_logger` 调用 `std::move(logger)` 接管 unique_ptr 所有权 — 调用方调用 set 后**不应**再持有/解引用该指针（其内部对象现在由 ChatSession 进程级 singleton 管理；重复 set 时旧 logger 被析构）
- 性能：mutex-free read 路径足够（原子操作在每次 log 调用都做 fence，对低频诊断日志可接受但非必要）

**唯一静态**: `default_logger_slot()` 返回 `std::unique_ptr<ILogger>&` —— Meyers singleton（`static` 局部变量），保证线程安全初始化（C++11 magic statics）。

### 修改的 4 处

**Pattern A (ensure_dir_0700 内部)** — **实际抽到 `detail::log_static_diag()` helper**（避免 4 处重复 if/else 与 message 字符串）:
```cpp
// pdk/chat_session/src/chat_session.cpp anonymous namespace
namespace detail {
inline void log_static_diag(agenticdsl::LogLevel level, const std::string& msg) {
  if (auto* logger = ChatSession::get_default_logger()) {
    logger->log(level, msg);
  } else {
    std::cerr << msg << std::endl;  // fallback 保留向后兼容
  }
}
}

// ensure_dir_0700 调用点改写为:
const std::string msg = "[session] create_directories failed: " + dir.string()
                      + " (" + ec.message() + ")";
detail::log_static_diag(agenticdsl::LogLevel::kError, msg);
```

**为何抽 helper 而非 4 处原地 if/else**:
- **测试可注入**: 单测 `detail::log_static_diag()` 路由逻辑确定性 100% (无需 chmod/root 失败注入)。Oracle C2 验证: chmod 000 对 stat/remove 无效 + root 免疫，原始失败注入测试按设计必然 FAIL。
- **避免字符串漂移**: 4 处的 message 构造集中在 helper 调用点，单一真相源
- **A1 验收可断言**: 4 处 `std::cerr` 均在 helper else 分支，结构性可 grep (else 分支数 = 1)

**Pattern B (cleanup_stale 内部)**: 同样经 `detail::log_static_diag(kWarn, msg)`。

### 启动期注入（main.cpp）

```cpp
// examples/pdk_chat_demo/main.cpp (修改后)
#include "agenticdsl/pdk/chat_session.h"
#include "common/io/stderr_logger.h"

// 在 ChatSession 构造**之前**:
hydraforge::pdk::ChatSession::set_default_logger(
    std::make_unique<agenticdsl::StderrLogger>());
```

**为何在 main 启动期 set 而非依赖 Impl 默认值**：
- `set_default_logger` 必须在**任何** ChatSession 创建前调用
- main.cpp 是单入口 → 集中 set 在所有早期 init 之后、`ChatSession::cleanup_stale` 之前

### 测试设计（独立 binary）

`tests/test_pdk_chat_session_static_logger.cpp`（~120 行，1 test binary，5 cases）。

**测试策略变更** (Oracle C2 + Metis P5/P6): **不**依赖失败注入 (chmod/root 不可移植)。改为**直接单测 detail routing helper**，确定性 100%。

**TEST_CASE 1**: "set_default_logger then get_default_logger returns same pointer (R1.1)"
- **GIVEN** `set_default_logger(make_unique<CapturingLogger>())`
- **WHEN** `get_default_logger()`
- **THEN** 返回非 nullptr 且 == set 时构造的 CapturingLogger 指针

**TEST_CASE 2**: "set_default_logger(nullptr) is equivalent to clear (R1.2)"
- **GIVEN** 已 set CapturingLogger
- **WHEN** `set_default_logger(nullptr)` 后 `get_default_logger()`
- **THEN** 返回 nullptr

**TEST_CASE 3**: "set_default_logger replaces previous logger (R1.3)"
- **GIVEN** 已 set CapturingLogger A
- **WHEN** `set_default_logger(make_unique<CapturingLogger>())` (B)
- **THEN** `get_default_logger()` 返回 B (非 A); A 在 set 时析构

**TEST_CASE 4**: "concurrent first call returns same pointer (R1.4)"
- **GIVEN** 未 set (fresh state via `clear_default_logger()`)
- **WHEN** 8 个 `std::jthread` 并发调 `get_default_logger()`
- **THEN** 8 个 raw 指针全部相等 (Meyers singleton 语义)
- **TSan gate**: 必须 0 warnings (无并发 set/get race)

**TEST_CASE 5**: "detail::log_static_diag routes to ILogger when set (R2.1)"
- **GIVEN** `DefaultLoggerGuard` RAII + set CapturingLogger
- **WHEN** `detail::log_static_diag(kError, "test msg")` × N 次
- **THEN** CapturingLogger 收到 N 条 kError + 消息精确匹配

**TEST_CASE 6**: "detail::log_static_diag falls back to stderr when not set (R2.3)"
- **GIVEN** `DefaultLoggerGuard` RAII + 不 set logger
- **WHEN** `detail::log_static_diag(kWarn, "[session/cleanup] test")`
- **THEN** `get_default_logger()` 返回 nullptr (验证 fallback 路径生效)

**DefaultLoggerGuard RAII** (Oracle M3 修复): 测试 teardown 异常安全 — Catch2 断言 FAIL 直接跳下 case 时仍执行 clear。
```cpp
struct DefaultLoggerGuard {
  ~DefaultLoggerGuard() { hydraforge::pdk::ChatSession::clear_default_logger(); }
};
```

每个 CASE 首行声明 `DefaultLoggerGuard guard;`，析构时无条件 clear（即使中间 REQUIRE 失败）。

## 验收命令

```bash
# A1: 源码 grep
grep -n "std::cerr" pdk/chat_session/src/chat_session.cpp
# 预期: 0 行（除 L8 注释）

# A2: 编译
cmake --build build -j$(nproc)
ctest --test-dir build -j4 --timeout 180  # 234 + 1 = 235 tests, 新增 PASS

# A4: TSan gate (继承 Change 1 D8 验收)
cmake --build build-tsan --target test_pdk_chat_session_static_logger
ctest --test-dir build-tsan -R static_logger --output-on-failure
# 预期: PASS, 0 warnings
```

## 备选方案（评估后未采纳）

| 方案 | 评估 | 决策 |
|---|---|---|
| **A. 给 `cleanup_stale` 加 optional `ILogger&` 参数** | 公开 API 扩展 + main.cpp 调用方需改 → 多个集成点 | ❌ |
| **B. 用 thread_local logger** | 启动期注入跨线程失效；input thread / main thread 需分别 set | ❌ |
| **C. 重构 `ensure_dir_0700` 为实例方法 + 实例 logger 调用** | 该函数被 `cleanup_stale` 间接调用；实例化改造巨大 | ❌ |
| **D. 接受 4 处 `std::cerr` 不修（lift Plan B3 偏差）** | 一致性问题长期债 | ❌（本次解决） |

**最终方案**: 进程级 static logger + main.cpp 启动期 set + fallback std::cerr (向后兼容)。
