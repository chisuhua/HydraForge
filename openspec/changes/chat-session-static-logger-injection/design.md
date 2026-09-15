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
- `unique_ptr` 拥有权明确，避免 raw pointer 释放责任
- 性能：mutex-free read 路径足够（原子操作在每次 log 调用都做 fence，对低频诊断日志可接受但非必要）

**唯一静态**: `default_logger_slot()` 返回 `std::unique_ptr<ILogger>&` —— Meyers singleton（`static` 局部变量），保证线程安全初始化（C++11 magic statics）。

### 修改的 4 处

**Pattern A (ensure_dir_0700 内部)**:
```cpp
// 修改前:
std::cerr << "[session] create_directories failed: " << dir
          << " (" << ec.message() << ")" << std::endl;

// 修改后:
if (auto* logger = ChatSession::get_default_logger()) {
  logger->log(agenticdsl::LogLevel::kError,
              "[session] create_directories failed: " + dir.string() +
                  " (" + ec.message() + ")");
} else {
  std::cerr << "[session] create_directories failed: " << dir
            << " (" << ec.message() << ")" << std::endl;
}
```

**注**：`kError` vs `kWarn` 选择：directory 创建失败是**严重**问题（disk full, perm denied），非 transient warning。

**Pattern B (cleanup_stale 内部)**: 同样 if/else 模式，`kWarn`。

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

`tests/test_pdk_chat_session_static_logger.cpp`（~80 行，1 test binary）：

**TEST_CASE 1**: "ensure_dir_0700 routes to default logger when set"
- **GIVEN** 创建 `CapturingLogger`，`set_default_logger(make_unique<CapturingLogger>(...))`
- **AND** 创建**只读**目录（无写权限），构造 ChatSession
- **WHEN** `ChatSession` 构造触发 `ensure_dir_0700` 失败
- **THEN** `captured_logs` 包含 ≥ 1 条 `kError` level + 含 `[session]`

**TEST_CASE 2**: "cleanup_stale routes to default logger when set"
- **GIVEN** 创建 CapturingLogger + set_default_logger
- **AND** 在临时 dir 创建只读 `*.json` 文件
- **WHEN** `ChatSession::cleanup_stale(dir)` 触发 `stat failed` 或 `remove failed`
- **THEN** captured 含 ≥ 1 条 kWarn + 含 `[session/cleanup]`

**TEST_CASE 3**: "fallback to stderr when default logger not set"
- **GIVEN** 不调用 set_default_logger
- **WHEN** 触发 ensure_dir_0700 失败
- **THEN** 行为不变（std::cerr 输出）—— 不破坏现有 test cleanup_stale 用例

**重要**: 测试结束时必须 `clear_default_logger()`（避免污染其他测试）。

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
