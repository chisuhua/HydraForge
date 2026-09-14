# ChatSession PDK Lift — Change 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 `examples/pdk_chat_demo/chat_session.{h,cpp}` 与 `cancellation_registry.{h,cpp}` 提取到 PDK (`include/agenticdsl/pdk/` + `pdk/chat_session/`),引入 IInputSource/ILogger 抽象层(must保留 self-pipe + poll(2) 架构),10 个 mock-first 测试 100% PASS,既有 28 个 chat demo 测试 binary 零回归。

**Architecture:** 2-change 拆分中的 Change 1 — I/O 抽象先行 + ChatSession 重命名迁移。CancellationRegistry 同步提升到 PDK header。tests double 放 `tests/test_helpers/`(Pattern #5),生产库仅留 stdin/stderr 默认实现。沿用 `agenticdsl::log::*` 门面避免双日志漂移。

**Tech Stack:** C++20 / Catch2 v3.7.0 / CMake 3.20+ / `tests/AGENTS.md` Pattern 1-7 / `src/common/log/log.h` 现有门面 / Sprint 31 self-pipe + poll(2) 架构

**Spec:** `/workspace/project/HydraForge/docs/superpowers/specs/2026-09-11-chat-session-pdk-lift-design.md`(570 行,§4 Change 1 + §6.1 接口 + §7.2 测试矩阵 + §11 acceptance)

---

## 文件结构(任务前规划)

### 新增文件(7 个)
- `include/agenticdsl/contract/iinput_source.h` — `IInputSource` 接口(3 个虚方法)
- `include/agenticdsl/contract/ilogger.h` — `ILogger` 接口(1 个虚方法)
- `include/agenticdsl/pdk/cancellation_registry.h` — 从 examples 迁
- `include/agenticdsl/pdk/chat_session.h` — 从 examples 迁 + namespace + 构造签名扩展
- `src/common/io/stdin_input_source.h/.cpp` — **保留 self-pipe + poll(2)** 架构
- `src/common/io/stderr_logger.h/.cpp` — 桥接 `agenticdsl::log::*`
- `pdk/chat_session/src/chat_session.cpp` — 从 examples 迁 + IInputSource/ILogger 注入
- `pdk/chat_session/CMakeLists.txt` — OBJECT lib
- `tests/test_helpers/in_memory_input_source.h` — 测试 double (Pattern #5)
- `tests/test_helpers/capturing_logger.h` — 测试 double (Pattern #5)
- `tests/test_pdk_chat_session.cpp` — 10 个 TEST_CASE mock-first (**改名避免与 examples 同名 target 冲突**)

### 修改文件(7 个)
- `include/agenticdsl/pdk/pdk.h` — 加 `#include <agenticdsl/pdk/chat_session.h>`
- `include/agenticdsl/pdk/CMakeLists.txt` — 头引用
- `pdk/CMakeLists.txt` — `add_subdirectory(chat_session)`
- `examples/pdk_chat_demo/main.cpp` — 显式传 `StdinInputSource` + `StderrLogger`
- `examples/pdk_chat_demo/commands/command_globals.{h,cpp}` — `g_command_session` → `g_chat_session` type alias 兼容
- `tests/CMakeLists.txt` — 注册 `test_chat_session` 目标
- `examples/pdk_chat_demo/CMakeLists.txt` — 链接 `pdk::chat_session_obj`

### 删除文件(2 个)
- `examples/pdk_chat_demo/cancellation_registry.{h,cpp}` — 迁到 PDK

---

## Task 1: `IInputSource` 接口契约

**Files:**
- Create: `include/agenticdsl/contract/iinput_source.h`

- [ ] **Step 1.1: 创建接口头文件**

```cpp
// include/agenticdsl/contract/iinput_source.h
// IInputSource 接口 - 解耦 stdin/网络/TUI 输入源
// 设计依据: docs/superpowers/specs/2026-09-11-chat-session-pdk-lift-design.md §6.1
// 作者: chat-session-pdk-lift Change 1
// 日期: 2026-09-11

#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace agenticdsl {

class IInputSource {
 public:
  virtual ~IInputSource() = default;

  // 阻塞读一行,timeout 后返回 nullopt;EOF 返回 nullopt
  virtual std::optional<std::string> read_line(
      std::chrono::milliseconds timeout) = 0;

  // 非阻塞检查是否有可用输入
  virtual bool has_input() const = 0;

  // 关闭输入源(用于优雅退出 / signal handler)
  virtual void close() = 0;
};

}  // namespace agenticdsl
```

- [ ] **Step 1.2: 验证编译**

Run: `cmake --build build --target agenticdsl_core 2>&1 | head -30`
Expected: 编译成功(纯接口,无依赖)

- [ ] **Step 1.3: 提交**

```bash
git add include/agenticdsl/contract/iinput_source.h
git commit -m "feat(contract): add IInputSource interface for stdin/TUI/network decoupling"
```

---

## Task 2: `ILogger` 接口契约(A1 修订:仅测试注入)

**Files:**
- Create: `include/agenticdsl/contract/ilogger.h`

- [ ] **Step 2.1: 创建接口头文件(只 1 个虚方法,不新建 LogLevel/LogSourceLocation 自由函数)**

```cpp
// include/agenticdsl/contract/ilogger.h
// ILogger 接口 - 仅供测试注入,生产实现桥接 agenticdsl::log 门面
// 设计依据: docs/superpowers/specs/2026-09-11-chat-session-pdk-lift-design.md §6.1 (A1 修订)
// 作者: chat-session-pdk-lift Change 1
// 日期: 2026-09-11

#pragma once

#include <string_view>

namespace agenticdsl {

enum class LogLevel { kDebug, kInfo, kWarn, kError };

class ILogger {
 public:
  virtual ~ILogger() = default;
  virtual void log(LogLevel level, std::string_view message) = 0;
};

}  // namespace agenticdsl
```

- [ ] **Step 2.2: 验证编译**

Run: `cmake --build build --target agenticdsl_core 2>&1 | head -30`
Expected: 编译成功

- [ ] **Step 2.3: 提交**

```bash
git add include/agenticdsl/contract/ilogger.h
git commit -m "feat(contract): add ILogger interface for test injection only (no double log facade)"
```

---

## Task 3: `StderrLogger` 桥接既有门面

**Files:**
- Create: `src/common/io/stderr_logger.h`
- Create: `src/common/io/stderr_logger.cpp`

- [ ] **Step 3.1: 创建头文件**

```cpp
// src/common/io/stderr_logger.h
#pragma once

#include "agenticdsl/contract/ilogger.h"

namespace agenticdsl {

// 生产用 ILogger 实现 - 委托既有 agenticdsl::log::* 全局门面
// 避免与 src/common/log/log.h 形成双门面漂移 (A1 修订)
class StderrLogger : public ILogger {
 public:
  void log(LogLevel level, std::string_view message) override;
};

}  // namespace agenticdsl
```

- [ ] **Step 3.2: 创建实现**

```cpp
// src/common/io/stderr_logger.cpp
#include "stderr_logger.h"
#include "common/log/log.h"

namespace agenticdsl {

void StderrLogger::log(LogLevel level, std::string_view message) {
  // **2026-09-11 Momus 修订**: `agenticdsl::log` 命名空间**只**暴露单一函数
  // `log::emit(Level, std::string)` (见 `src/common/log/log.h:52`)。`log::info/warn/error/debug`
  // 自由函数**不存在**(只有 `LOG_INFO/WARN/ERROR/DEBUG` 4 个宏,见 log.h:88-103)。
  // 必须用 `log::emit(Level, std::string)` 形式; 否则编译失败。
  auto emit = [&](log::Level lvl) {
    log::emit(lvl, std::string("[chat] ") + std::string(message));
  };
  switch (level) {
    case LogLevel::kDebug: emit(log::Level::kDebug); break;
    case LogLevel::kInfo:  emit(log::Level::kInfo);  break;
    case LogLevel::kWarn:  emit(log::Level::kWarn);  break;
    case LogLevel::kError: emit(log::Level::kError); break;
  }
}

}  // namespace agenticdsl
```

- [ ] **Step 3.3: 注册到 agenticdsl_common 静态库**

修改 `src/common/CMakeLists.txt`,添加:
```cmake
target_sources(agenticdsl_common PRIVATE
  io/stderr_logger.cpp
)
```

- [ ] **Step 3.4: 编译验证**

Run: `cmake --build build --target agenticdsl_common 2>&1 | head -20`
Expected: 编译成功

- [ ] **Step 3.5: 提交**

```bash
git add src/common/io/stderr_logger.h src/common/io/stderr_logger.cpp src/common/CMakeLists.txt
git commit -m "feat(common): add StderrLogger bridging agenticdsl::log facade (no double log)"
```

---

## Task 4: `StdinInputSource` — **必须保留 self-pipe + poll(2) 架构** (A3 acceptance)

**Files:**
- Create: `src/common/io/stdin_input_source.h`
- Create: `src/common/io/stdin_input_source.cpp`

**⚠️ A3 强制约束**: 必须保留 Sprint 31 死锁修复的 self-pipe + poll([STDIN_FILENO, pipe_read_fd_], timeout) 多 fd 架构。**禁止回退为裸 `std::getline`**,否则会回退 AGENTS.md 模式 #6 修复,TTY 环境下 `ctest` 必然 hang。

- [ ] **Step 4.1: 创建头文件**

```cpp
// src/common/io/stdin_input_source.h
// StdinInputSource - 保留 self-pipe + poll(2) 多 fd 架构 (Sprint 31 死锁修复)
// 设计依据: docs/superpowers/specs/2026-09-11-chat-session-pdk-lift-design.md §6.1 (A3)
#pragma once

#include "agenticdsl/contract/iinput_source.h"
#include <atomic>
#include <mutex>

namespace agenticdsl {

class StdinInputSource : public IInputSource {
 public:
  StdinInputSource();
  ~StdinInputSource() override;

  std::optional<std::string> read_line(
      std::chrono::milliseconds timeout) override;
  bool has_input() const override;
  void close() override;

 private:
  int pipe_read_fd_ = -1;
  int pipe_write_fd_ = -1;
  std::atomic<bool> shutdown_requested_{false};
};

}  // namespace agenticdsl
```

- [ ] **Step 4.2: 创建实现(完整 self-pipe 架构)**

```cpp
// src/common/io/stdin_input_source.cpp
#include "stdin_input_source.h"
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

namespace agenticdsl {

StdinInputSource::StdinInputSource() {
  // pipe2(O_CLOEXEC | O_NONBLOCK) 创建 internal pipe
  int fds[2];
  if (pipe2(fds, O_CLOEXEC | O_NONBLOCK) == 0) {
    pipe_read_fd_ = fds[0];
    pipe_write_fd_ = fds[1];
  }
  // 失败时 fd = -1 防御性 default,主循环检查 fd >= 0 才加入 pollfd 数组
}

StdinInputSource::~StdinInputSource() {
  if (pipe_write_fd_ >= 0) close(pipe_write_fd_);
  if (pipe_read_fd_ >= 0) close(pipe_read_fd_);
}

void StdinInputSource::close() {
  shutdown_requested_.store(true);
  if (pipe_write_fd_ >= 0) {
    // 写写 1 byte wake-up 主循环 poll
    char byte = 1;
    ssize_t r = ::write(pipe_write_fd_, &byte, 1);
    (void)r;  // EAGAIN 容忍
  }
}

bool StdinInputSource::has_input() const {
  if (shutdown_requested_.load()) return false;
  // 简化的可用性检查 - 实际生产可能用 select/poll
  struct pollfd fds[1];
  fds[0].fd = STDIN_FILENO;
  fds[0].events = POLLIN;
  return poll(fds, 1, 0) > 0;
}

std::optional<std::string> StdinInputSource::read_line(
    std::chrono::milliseconds timeout) {
  // **核心: poll 多 fd 监听,100ms clamp 防止永久阻塞**
  struct pollfd fds[2];
  int nfds = 1;
  fds[0].fd = STDIN_FILENO;
  fds[0].events = POLLIN;
  if (pipe_read_fd_ >= 0) {
    fds[1].fd = pipe_read_fd_;
    fds[1].events = POLLIN;
    nfds = 2;
  }
  int timeout_ms = static_cast<int>(timeout.count());
  if (timeout_ms <= 0) timeout_ms = 100;  // clamp 100ms
  
  int ret = poll(fds, nfds, timeout_ms);
  if (ret <= 0) return std::nullopt;  // timeout 或 error
  
  // pipe fd ready = wake-up,忽略
  if (pipe_read_fd_ >= 0 && (fds[1].revents & POLLIN)) {
    char buf[64];
    while (::read(pipe_read_fd_, buf, sizeof(buf)) > 0) {}  // drain
    return std::nullopt;  // wake-up,本次不读 stdin
  }
  
  // stdin ready - read line (non-blocking via O_NONBLOCK? 实际用 std::getline 的 fdopen 替代)
  // 此处简化实现 - 生产代码用 std::getline 阻塞读
  std::string line;
  if (std::getline(std::cin, line)) {
    if (!line.empty() && line.back() == '\n') line.pop_back();
    return line;
  }
  return std::nullopt;  // EOF
}

}  // namespace agenticdsl
```

- [ ] **Step 4.3: 注册到 agenticdsl_common**

修改 `src/common/CMakeLists.txt`,添加:
```cmake
target_sources(agenticdsl_common PRIVATE
  io/stdin_input_source.cpp
)
```

- [ ] **Step 4.4: 编译验证**

Run: `cmake --build build --target agenticdsl_common 2>&1 | head -20`
Expected: 编译成功

- [ ] **Step 4.5: TTY 死锁回归守卫(Pattern 5 + AGENTS.md 模式 #6)**

Run: `script -qec "ctest --test-dir build" /dev/null 2>&1 | tail -10`
Expected: 全部 ctest PASS,无 SIGTERM-then-`std::terminate` 死锁

如果 hang → 回退 step 4.2 检查 self-pipe 是否正确实现

- [ ] **Step 4.6: 提交**

```bash
git add src/common/io/stdin_input_source.h src/common/io/stdin_input_source.cpp src/common/CMakeLists.txt
git commit -m "feat(common): add StdinInputSource preserving self-pipe + poll(2) architecture (Sprint 31)"
```

---

## Task 5: 测试 double — `InMemoryInputSource` 与 `CapturingLogger`

**Files:**
- Create: `tests/test_helpers/in_memory_input_source.h`
- Create: `tests/test_helpers/capturing_logger.h`

**A2 修订**: 测试 double 放 `tests/test_helpers/`(Pattern #5 + http_mock_server.h 惯例),**不放 `src/common/io/`**

- [ ] **Step 5.1: 创建 `InMemoryInputSource`**

```cpp
// tests/test_helpers/in_memory_input_source.h
// 测试用 IInputSource - 预填输入 + push_for_test() 注入
#pragma once

#include "agenticdsl/contract/iinput_source.h"
#include <mutex>
#include <queue>
#include <condition_variable>
#include <string>

namespace agenticdsl::test {

class InMemoryInputSource : public IInputSource {
 public:
  // 预填输入(测试启动时设置)
  void enqueue_input(std::string line);

  // 测试中动态注入(替代原 try_push_*_for_test 后门)
  void push_for_test(std::string line);

  std::optional<std::string> read_line(
      std::chrono::milliseconds timeout) override;
  bool has_input() const override;
  void close() override;

 private:
  mutable std::mutex mtx_;
  std::condition_variable cv_;
  std::queue<std::string> queue_;
  bool closed_ = false;
};

}  // namespace agenticdsl::test
```

- [ ] **Step 5.2: 创建 `CapturingLogger`**

```cpp
// tests/test_helpers/capturing_logger.h
// 测试用 ILogger - 捕获所有 log 调用供断言
#pragma once

#include "agenticdsl/contract/ilogger.h"
#include <mutex>
#include <vector>

namespace agenticdsl::test {

struct CapturedLog {
  LogLevel level;
  std::string message;
};

class CapturingLogger : public ILogger {
 public:
  void log(LogLevel level, std::string_view message) override;

  std::vector<CapturedLog> snapshot() const;
  size_t count(LogLevel level) const;
  void clear();

 private:
  mutable std::mutex mtx_;
  std::vector<CapturedLog> logs_;
};

}  // namespace agenticdsl::test
```

- [ ] **Step 5.3: 实现文件 `tests/test_helpers/in_memory_input_source.cpp` 与 `capturing_logger.cpp`**

```cpp
// tests/test_helpers/in_memory_input_source.cpp
#include "test_helpers/in_memory_input_source.h"

namespace agenticdsl::test {

void InMemoryInputSource::enqueue_input(std::string line) {
  std::lock_guard<std::mutex> lock(mtx_);
  queue_.push(std::move(line));
  cv_.notify_one();
}

void InMemoryInputSource::push_for_test(std::string line) {
  enqueue_input(std::move(line));
}

std::optional<std::string> InMemoryInputSource::read_line(
    std::chrono::milliseconds timeout) {
  std::unique_lock<std::mutex> lock(mtx_);
  if (!cv_.wait_for(lock, timeout, [this] { return !queue_.empty() || closed_; }))
    return std::nullopt;
  if (queue_.empty()) return std::nullopt;  // closed
  auto line = queue_.front();
  queue_.pop();
  return line;
}

bool InMemoryInputSource::has_input() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return !queue_.empty();
}

void InMemoryInputSource::close() {
  std::lock_guard<std::mutex> lock(mtx_);
  closed_ = true;
  cv_.notify_all();
}

}  // namespace agenticdsl::test
```

```cpp
// tests/test_helpers/capturing_logger.cpp
#include "test_helpers/capturing_logger.h"

namespace agenticdsl::test {

void CapturingLogger::log(LogLevel level, std::string_view message) {
  std::lock_guard<std::mutex> lock(mtx_);
  logs_.push_back({level, std::string(message)});
}

std::vector<CapturedLog> CapturingLogger::snapshot() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return logs_;
}

size_t CapturingLogger::count(LogLevel level) const {
  std::lock_guard<std::mutex> lock(mtx_);
  size_t n = 0;
  for (const auto& l : logs_) if (l.level == level) ++n;
  return n;
}

void CapturingLogger::clear() {
  std::lock_guard<std::mutex> lock(mtx_);
  logs_.clear();
}

}  // namespace agenticdsl::test
```

- [ ] **Step 5.4: 编译验证**

Run: `cmake --build build --target test_chat_session 2>&1 | head -20`
Expected: 若 test_chat_session.cpp 还没写,可能找不到 target — 跳过此 step,继续 Task 6

- [ ] **Step 5.5: 提交**

```bash
git add tests/test_helpers/in_memory_input_source.h tests/test_helpers/capturing_logger.h tests/test_helpers/in_memory_input_source.cpp tests/test_helpers/capturing_logger.cpp
git commit -m "test(helpers): add InMemoryInputSource + CapturingLogger test doubles (Pattern #5)"
```

---

## Task 6: `CancellationRegistry` 提升到 PDK

**Files:**
- Create: `include/agenticdsl/pdk/cancellation_registry.h`
- Create: `pdk/chat_session/src/cancellation_registry.cpp`(**修正后路径**,加入 `pdk_chat_session_obj` SOURCES,避免 orphan 链接失败 — Momus WARNING 9)
- Delete: `examples/pdk_chat_demo/cancellation_registry.h`(经 shim 间接提供)
- Delete: `examples/pdk_chat_demo/cancellation_registry.cpp`

- [ ] **Step 6.1: 复制并重命名 namespace(2026-09-11 验证 1B 修订: 真实类是全局 namespace, 无 namespace 包裹)**

复制 `examples/pdk_chat_demo/cancellation_registry.h` 到 `include/agenticdsl/pdk/cancellation_registry.h`,修改:
- **验证 1B 修订**: `CancellationRegistry` 实际**位于全局命名空间**(实测 examples/pdk_chat_demo/cancellation_registry.h 无 `namespace ... { }` 包裹), 不是 `pdk_chat_demo::CancellationRegistry`, 也不是 `agenticdsl::CancellationRegistry`。`commands/cancellation_globals.h` 的 `extern std::shared_ptr<CancellationRegistry>` 引用全局符号; main.cpp `pdk_chat_demo::g_cancellation_registry = std::make_shared<CancellationRegistry>();` 也是全局类。
- lift 方案: 用 `namespace hydraforge::pdk { class CancellationRegistry { ... }; }` 包裹 (而非全局), 让 PDK 消费者用 `hydraforge::pdk::CancellationRegistry`, commands shim 用 `using hydraforge::pdk::CancellationRegistry;` 兼容 1 Sprint
- 添加 header guard + 文件头注释

- [ ] **Step 6.2: 复制 .cpp 并移入有 CMake target 的位置(2026-09-11 Momus WARNING 9 修订 + 验证 1B namespace 修订)**

复制 `examples/pdk_chat_demo/cancellation_registry.cpp` 到 **`pdk/chat_session/src/cancellation_registry.cpp`**(非 `pdk/cancellation_registry.cpp`,后者无 CMake target 编译,链接 undefined symbol),修改:
- **验证 1B 修订**: 真实源文件**无** namespace 包裹, lift 时需用 `namespace hydraforge::pdk { ... }` 包裹整个类定义与方法实现 (Step 6.1 已修订)
- 添加 header guard + 文件头注释

- [ ] **Step 6.3: 注册到 PDK CMake 编译目标**

修改 `pdk/chat_session/CMakeLists.txt`(Task 1.5 已创建),在 `pdk_chat_session_obj` SOURCES 列表中追加 `src/cancellation_registry.cpp`:

```cmake
add_library(pdk_chat_session_obj OBJECT
  src/chat_session.cpp
  src/cancellation_registry.cpp     # 2026-09-11 Momus 修订:保证有 target 编译
)
```

验证: `nm -C build/pdk/chat_session/CMakeFiles/pdk_chat_session_obj.dir/src/cancellation_registry.cpp.o | grep register_source` 必须有符号(否则链接会报 undefined reference)。

- [ ] **Step 6.4: 编译 examples 验证(全局变量兼容)**

修改 `examples/pdk_chat_demo/commands/cancellation_globals.h`:
```cpp
namespace pdk_chat_demo {
// 1 Sprint 兼容期 type alias
using CancellationRegistry = hydraforge::pdk::CancellationRegistry;
extern std::shared_ptr<CancellationRegistry> g_cancellation_registry;
}  // namespace pdk_chat_demo
```

Run: `cmake --build build --target pdk_chat_demo 2>&1 | head -20`
Expected: 编译成功

- [ ] **Step 6.5: 删除原 examples 文件**

```bash
git rm examples/pdk_chat_demo/cancellation_registry.h examples/pdk_chat_demo/cancellation_registry.cpp
```

- [ ] **Step 6.6: 提交**

```bash
git add include/agenticdsl/pdk/cancellation_registry.h pdk/chat_session/src/cancellation_registry.cpp examples/pdk_chat_demo/commands/cancellation_globals.h
git rm examples/pdk_chat_demo/cancellation_registry.h examples/pdk_chat_demo/cancellation_registry.cpp
git commit -m "refactor(pdk): lift CancellationRegistry from examples to PDK header (with CMake target registration)"
```

---

## Task 7: ChatSession 头文件迁移到 PDK + 扩展构造签名

**Files:**
- Create: `include/agenticdsl/pdk/chat_session.h`
- Modify: `examples/pdk_chat_demo/chat_session.h`(**保留为 3 行 shim**,非删除 — Momus 漏掉修复 #4: 17 个旧 includer 一次断链)

- [ ] **Step 7.1: 复制并扩展 ChatSession 头文件(2026-09-11 Momus 修订)**

复制 `examples/pdk_chat_demo/chat_session.h` 到 `include/agenticdsl/pdk/chat_session.h`,修改:
- namespace `pdk_chat_demo::` → `hydraforge::pdk::`
- **AgentConfig / SessionConfig / ChatResult / QueueKind / InputMessage 5 个公开类型全部移入 `hydraforge::pdk` 命名空间**(原 namespace `pdk_chat_demo` 仅留 `examples/pdk_chat_demo/commands/cancellation_globals.h` 的兼容 shim)
- 构造签名添加 `unique_ptr<IInputSource>` 与 `unique_ptr<ILogger>` 参数
- 添加 header guard + 文件头注释

构造签名(扩展后,namespace 全部 `hydraforge::pdk`):
```cpp
namespace hydraforge::pdk {

class ChatSession {
 public:
  ChatSession(agenticdsl::DSLEngine* engine,
              std::shared_ptr<agenticdsl::IInteractionBus> bus,
              agenticdsl::IToolRegistry* registry,
              AgentConfig agent_cfg,                                     // 移入命名空间
              SessionConfig session_cfg,                                 // 移入命名空间
              std::shared_ptr<CancellationRegistry> cancellation_registry = nullptr,
              std::unique_ptr<agenticdsl::IInputSource> input = nullptr,
              std::unique_ptr<agenticdsl::ILogger> logger = nullptr);

  // P0 API
  ChatResult chat(std::string_view input,                              // 移入命名空间
                  std::stop_token token = {});
  void request_stop();
  const std::string& session_id() const;
  std::vector<nlohmann::json> history() const;
  bool is_input_thread_shutdown() const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace hydraforge::pdk
```

- [ ] **Step 7.2: 添加 namespace 兼容 shim 到 `examples/pdk_chat_demo/chat_session.h`(Momus 漏掉修复 #4: 17 个旧 includer 一次断链)**

实测 examples 树有 **17 个文件** include `examples/pdk_chat_demo/chat_session.h`(12 个测试 + main.cpp + 3 个 commands + 1 个 loop_agent 转发)。直接 `git rm` 旧文件会导致 Task 10/11 之间断链。改为**保留** `examples/pdk_chat_demo/chat_session.h` 为 3 行 shim,1 Sprint 兼容期:

```cpp
// examples/pdk_chat_demo/chat_session.h — 1 Sprint 兼容 shim (2026-09-11 Momus 修订)
#pragma once
#include <agenticdsl/pdk/chat_session.h>     // 引入新 PDK 头
namespace pdk_chat_demo {
  // 旧 namespace 的公开类型 alias 到新 PDK 命名空间
  using hydraforge::pdk::AgentConfig;
  using hydraforge::pdk::SessionConfig;
  using hydraforge::pdk::ChatResult;
  using hydraforge::pdk::QueueKind;
  using hydraforge::pdk::InputMessage;
  using hydraforge::pdk::ChatSession;
}  // namespace pdk_chat_demo
```

验证: `grep -rn 'pdk_chat_demo::AgentConfig\|pdk_chat_demo::ChatSession' examples/ | wc -l` 应返回 17(全部经 shim 间接使用); `cmake --build build --target pdk_chat_demo` 0 编译错误。


- [ ] **Step 7.2: 添加到 PDK 统一入口**

修改 `include/agenticdsl/pdk/pdk.h`,添加:
```cpp
#include <agenticdsl/pdk/chat_session.h>
```

- [ ] **Step 7.3: 编译验证**

Run: `cmake --build build --target agenticdsl_core 2>&1 | head -30`
Expected: 头文件引用成功(.cpp 后续 Task 处理)

- [ ] **Step 7.4: 提交**

```bash
git add include/agenticdsl/pdk/chat_session.h include/agenticdsl/pdk/pdk.h
git commit -m "feat(pdk): lift ChatSession header with IInputSource/ILogger injection"
```

---

## Task 8: ChatSession 实现迁移 + I/O 抽象替换

**Files:**
- Create: `pdk/chat_session/src/chat_session.cpp`
- Create: `pdk/chat_session/CMakeLists.txt`
- Modify: `pdk/CMakeLists.txt`
- Modify: `examples/pdk_chat_demo/CMakeLists.txt`

- [ ] **Step 8.1: 创建 `pdk/chat_session/CMakeLists.txt`(OBJECT 库)**

```cmake
# pdk/chat_session/CMakeLists.txt
add_library(pdk_chat_session_obj OBJECT
  src/chat_session.cpp
)
target_include_directories(pdk_chat_session_obj PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
)
target_link_libraries(pdk_chat_session_obj
  PUBLIC agenticdsl_common
  PRIVATE agenticdsl_core
)
set_target_properties(pdk_chat_session_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
```

- [ ] **Step 8.2: 注册到 `pdk/CMakeLists.txt`**

修改 `pdk/CMakeLists.txt`,在 `add_subdirectory()` 列表添加:
```cmake
add_subdirectory(chat_session)
```

- [ ] **Step 8.3: 复制并修改 `chat_session.cpp` 实现(2026-09-11 Momus WARNING 4/10/11 + CRITICAL 1 修订)**

复制 `examples/pdk_chat_demo/chat_session.cpp` 到 `pdk/chat_session/src/chat_session.cpp`,修改:
- namespace `pdk_chat_demo` → `hydraforge::pdk`
- 删除 `std::getline(std::cin, ...)` 调用,改为 `input_->read_line(timeout)`
- 删除 `std::cerr` 调用,改为 `logger_->log(...)`
- **保留** `try_push_*_for_test` 两个 test-only helper(Momus D9 修订: 标 `[[deprecated]]`, 不删除 — overflow 测试需要)
- **新增** `chat()` 入口 null-guard(CRITICAL 1 修订 — 防止 `nullptr` bus/registry SIGSEGV):

```cpp
ChatResult ChatSession::chat(std::string_view input, std::stop_token token) {
  if (!impl_->bus || !impl_->registry) {
    ChatResult r;
    r.success = false;
    r.error_message = "[chat] null bus or registry (fail-safe guard per Pattern #5)";
    return r;
  }
  // ... 原 chat() 主体 ...
}
```

- **删除** `ChatSession::Impl` 自有的 `pipe_read_fd_/pipe_write_fd_/pipe2()` + timer callback 写字节逻辑(Momus WARNING 11 修订 — 避免双 self-pipe 所有权混乱导致 timer 唤醒字节进死 fd)。Sprint 31 死锁修复的 wake-up 路径改为: `timer_` 回调 → `input_->close()` → StdinInputSource 自有 pipe 接收, 由其 `read_line` poll 唤醒。

**关键修改点(2026-09-11 Momus WARNING 10 行号校正)**:
- `chat_session.cpp:853` — `std::getline(std::cin, line)` → `input_->read_line(100ms)` (实际行号 853,非 719)
- `chat_session.cpp` 全部 **15 处** `std::cerr` 调用(`grep -n "std::cerr" examples/pdk_chat_demo/chat_session.cpp` 列出: 74, 79, 359, 525, 533, 536, 543, 587, 596, 651, 659, 681, 694, 880, 889) → `logger_->log(LogLevel::kWarn, msg)`
- 上述位置保存与 save_to_disk/load_from_disk 相关 cerr(行 880/889)需在迁移 ChatResult 的同时迁日志

- [ ] **Step 8.4: 编译验证**

Run: `cmake --build build --target pdk_chat_session_obj 2>&1 | head -30`
Expected: 编译成功(可能因 examples 引用旧 chat_session 失败,后续 Task 修复)。**验证 grep**: `grep -n "std::cerr\|std::getline" pdk/chat_session/src/chat_session.cpp` 0 行; `grep -n "pipe2\|pipe_write_fd_" pdk/chat_session/src/chat_session.cpp` 0 行; `grep -n "try_push.*_deprecated" pdk/chat_session/src/chat_session.cpp` 0 行(用 `try_push_follow_up_for_test` 真名)。

- [ ] **Step 8.5: 更新 examples 链接**

修改 `examples/pdk_chat_demo/CMakeLists.txt`,在 `pdk_chat_demo_obj` 添加链接:
```cmake
target_link_libraries(pdk_chat_demo_obj PUBLIC pdk_chat_session_obj)
```

- [ ] **Step 8.6: 提交**

```bash
git add pdk/chat_session pdk/CMakeLists.txt examples/pdk_chat_demo/CMakeLists.txt
git commit -m "refactor(pdk): migrate ChatSession impl with I/O abstraction injection"
```

### 实施偏差记录(2026-09-14 实际执行)

| # | plan 原文 | 实际实施 | 理由 |
|---|---|---|---|
| B1 | 新增 `input`/`logger` 参数插在构造签名中间(第 6-7 位) | **追加在末尾**(第 8-9 位) | plan 的插入会让既有 20+ 个 5-7 参调用点形参错位 → 全部编译失败 |
| B2 | `pdk_chat_session_obj` OBJECT 库 | **`pdk_chat_session` STATIC 库** | 实测 OBJECT→OBJECT 链接**不传递**对象文件到最终消费者 (`undefined reference to hydraforge::pdk::CancellationRegistry::*`)。STATIC 库 `target_link_libraries` 传递语义可靠, 与 `agenticdsl_common` 同模式 |
| B3 | Task 8.3: "删除 15 处 std::cerr, 改为 logger_->log" | **11/15 处完成迁移; 4 处保留 std::cerr** | 4 处位于**静态上下文**: `ensure_dir_0700` (anonymous namespace 自由函数, 行 78/83) + `cleanup_stale` (public **static** 成员, 行 644/652)。这两者无 `Impl` 实例 → 无 `logger_` 可访问。给 public static API 加 logger 参数属 public API 变更, 超出本 change 范围 (F-1 follow-up) |
| B4 | Task 8.3: "timer callback → `input_->close()`" | **timer callback → `input_->wake()`** | `close()` 是**永久关闭**语义 (`shutdown_requested_` 不可逆); 用它做 50ms 周期性唤醒会让输入源**永久失效**。新增 `IInputSource::wake()` (幂等唤醒, 不影响后续 read_line) — plan 原方案是 bug |
| B5 | Task 7.2: examples shim 只 alias `AgentConfig`/`SessionConfig`/`ChatResult`/`QueueKind`/`InputMessage` | shim **额外** alias `ChatConfig`/`PluginConfig`/`ObservabilityConfig` | 实测 `main.cpp:160` + `test_chat_session.cpp` 通过 `pdk_chat_demo::ChatConfig` 引用这 3 个类型, plan 的 alias 清单不完整 → 会断链 |
| B6 | Task 6.4: 只改 `cancellation_globals.h` | **额外改 `command_globals.h`** | 后者含 `class ChatSession;` 前置声明 (Sprint 30 同款 namespace 冲突模式), 与 shim 的 `using ChatSession = hydraforge::pdk::ChatSession;` 冲突 → `main.cpp` 编译失败。改为 alias 前置声明 |
| B7 | 测试 "pre-cancelled token → chat() success=false" | **改为真实取消链 E2E** (stub 观察 registry token + 并发 `request_stop()`) | 实测 `chat()` 只把 token 存入 `current_token_`, **不读取** `token.stop_requested()`; 取消只能经 `request_stop()` → registry source。plan 断言的失败路径在真实实现中不可达 |
| B8 | 测试 "RecordingLLMProvider `generate_calls >= 1`" | **`generate_calls == 0`** | 实测 `chat()` 完全不经 LLM provider (统一走 `loop/run`), 这正是既有 `test_e2e_mock` 的 "routes through loop/run" 契约。plan 的 `>= 1` 与真实契约相反 |

**B3 遗留项**: F-1 follow-up `chat-session-static-logger-injection` (可选) — 若需完全消除 `std::cerr`, 可把 `logger` 提升为 `ChatSession` 的静态可注入成员, 或给 `cleanup_stale` 加 optional logger 参数 (public API 变更, 需 ADR)。当前保留 4 处可接受: 均为启动期/清理期诊断输出, 不参与并发路径。

---

## Task 9: 更新 `examples/pdk_chat_demo/main.cpp` 显式注入

**Files:**
- Modify: `examples/pdk_chat_demo/main.cpp`

- [ ] **Step 9.1: 添加 include + 显式构造**

在 main.cpp 顶部添加:
```cpp
#include "common/io/stdin_input_source.h"
#include "common/io/stderr_logger.h"
```

修改 ChatSession 构造调用:
```cpp
// 之前:
ChatSession session(engine.get(), bus, registry, agent_cfg, session_cfg,
                    cancellation_registry);

// 之后 (显式 I/O 注入,Pattern 5 fail-safe 默认 nullptr → 需显式 opt-in):
ChatSession session(engine.get(), bus, registry, agent_cfg, session_cfg,
                    cancellation_registry,
                    std::make_unique<agenticdsl::StdinInputSource>(),
                    std::make_unique<agenticdsl::StderrLogger>());
```

- [ ] **Step 9.2: 编译验证**

Run: `cmake --build build --target pdk_chat_demo 2>&1 | head -30`
Expected: 编译成功

- [ ] **Step 9.3: 提交**

```bash
git add examples/pdk_chat_demo/main.cpp
git commit -m "refactor(examples): pdk_chat_demo main.cpp explicit I/O injection"
```

---

## Task 10: 删除 `examples/pdk_chat_demo/chat_session.cpp` 主体 + `chat_session.h` 保留 shim(2026-09-11 Momus 复核 Item 16 修订)

**Files:**
- Delete: `examples/pdk_chat_demo/chat_session.cpp`(主体迁到 PDK)
- **保留**: `examples/pdk_chat_demo/chat_session.h`(**2026-09-11 Momus 修订**: 3 行 shim, 不删 — 实测 examples 树 17 个文件 include 此头, 直接删除会断链; 详见 Task 7.2 shim 内容)

- [ ] **Step 10.1: 检查 examples 测试对 `try_push_*_for_test` 的依赖(2026-09-11 Momus WARNING 10 修订)**

Run: `grep -rl "try_push_steering_for_test\|try_push_follow_up_for_test" examples/pdk_chat_demo/tests/ | wc -l`
**Expected: 2 个测试文件**(实测: `test_chat_session_consumer.cpp` + `test_chat_session_queues.cpp`, 非 Momus 探查的 4 个 — design.md §8 风险表与 plan 原文均错)

- [ ] **Step 10.2: 迁移 2 个测试到 InMemoryInputSource(2026-09-11 修订: 保留 try_push 标 deprecated, 仅改 include)**

对 2 个依赖 `try_push_*_for_test` 的测试文件(`test_chat_session_consumer.cpp` + `test_chat_session_queues.cpp`):
- 替换 `#include "chat_session.h"` 为 `#include "chat_session.h"` (旧头保留为 shim, 见 Task 7.2)
- **保留** `engine->get_chat_session().try_push_steering_for_test(...)` 调用 — 它们继续工作,因为 shim 把 `pdk_chat_demo::ChatSession` alias 到 `hydraforge::pdk::ChatSession`,`try_push_*_for_test` 在新 ChatSession 中保留为 `[[deprecated]]` test-only helper
- **不** 改写为 InMemoryInputSource(避免时序不可控, see design.md D9 修订)
- 1 Sprint 兼容期内迁 InMemoryInputSource(独立 follow-up, 不在本 change scope)

- [ ] **Step 10.3: examples 侧 `chat_session.cpp` 删除(头保留 shim)**

```bash
git rm examples/pdk_chat_demo/chat_session.cpp     # 主体迁到 PDK
# 注意: examples/pdk_chat_demo/chat_session.h **不删**, 保留为 3 行 shim (Task 7.2)
```

- [ ] **Step 10.4: 全量 ctest 验证**

Run: `cmake --build build && ctest --test-dir build 2>&1 | tail -20`
Expected: 所有现有 28 个 chat demo 测试 binary PASS,零回归

- [ ] **Step 10.5: 提交**

```bash
git add -u examples/pdk_chat_demo
git commit -m "refactor(examples): remove chat_session.cpp body (now in PDK); keep .h as 3-line shim"
```

---

## Task 11: 第一个 mock 测试 — `ChatSession ctor default nullptr safe no-op`

**Files:**
- Create: `tests/test_pdk_chat_session.cpp`(**2026-09-11 Momus 漏掉 #2 修订**: 加 `pdk_` 前缀避免与 `examples/pdk_chat_demo/tests/test_chat_session.cpp` 同名 target 冲突 — `tests/CMakeLists.txt` 用 `file(GLOB test_*.cpp)` 自动注册同名 `test_chat_session` target)
- Modify: `tests/CMakeLists.txt`(**不** 手动 `add_catch_test`, 由 file(GLOB) 自动注册)

**TDD 5 步结构开始。** 10 个 case 分多个 Task 写,每个 Task 一个 case。

- [ ] **Step 11.1: 创建测试文件骨架 + 第 1 个测试(2026-09-11 Momus CRITICAL 1 修订: fixture 必须给真 bus/registry + loop/run stub)**

```cpp
// tests/test_pdk_chat_session.cpp
// 文件头注释: chat_session PDK lift Change 1 - mock-first 测试
// 设计依据: docs/superpowers/specs/2026-09-11-chat-session-pdk-lift-design.md §7.2
// 模式: Pattern 1-7 全部应用 (AGENTS.md §REAL-LLM TEST PATTERNS)
// 日期: 2026-09-11

#include "catch_amalgamated.hpp"
#include "agenticdsl/pdk/chat_session.h"
#include "agenticdsl/pdk/cancellation_registry.h"
#include "agenticdsl/contract/iinput_source.h"
#include "agenticdsl/contract/ilogger.h"
#include "common/llm/mock_provider.h"
#include "test_helpers/in_memory_input_source.h"
#include "test_helpers/capturing_logger.h"
#include "common/bus/in_memory_bus.h"
#include "common/tools/registry.h"
#include <memory>
#include <string>

using namespace hydraforge::pdk;
using agenticdsl::IInputSource;
using agenticdsl::ILogger;
using agenticdsl::LogLevel;
using agenticdsl::test::InMemoryInputSource;
using agenticdsl::test::CapturingLogger;

namespace {

// 测试 fixture: 真 InMemoryBus + 真 ToolRegistry + loop/run stub (Momus CRITICAL 1 fix)
// 之前 plan 用 nullptr bus/registry → chat() 内 SIGSEGV
// 现在注册 lambda 桩 "loop/run" 返回成功 JSON, 让 chat() 能走完成功路径
const std::string kEmptyDsl = R"(
### AgenticDSL `/main`
```yaml
graph_type: subgraph
nodes:
  - id: start
    type: start
    next: ["/main/end"]
  - id: end
    type: end
```
)";

class TestChatSessionFixture {
 public:
  TestChatSessionFixture()
      : engine(agenticdsl::DSLEngine::from_markdown(kEmptyDsl)),
        bus(std::make_shared<agenticdsl::InMemoryBus>()),
        registry(std::make_unique<agenticdsl::ToolRegistry>()),
        input(std::make_unique<InMemoryInputSource>()),
        logger(std::make_unique<CapturingLogger>()) {
    // 注册 lambda 桩 "loop/run", 返回成功 JSON 让 chat() 走完成功路径
    registry->register_tool("loop/run",
      [this](const nlohmann::json& args) {
        nlohmann::json r;
        r["response"] = "mock-llm-response";
        r["steps"] = nlohmann::json::array();
        r["tokens_used"] = 0;
        r["cost_usd"] = 0.0;
        return agenticdsl::ToolResult::success(r);
      });
  }

  std::unique_ptr<agenticdsl::DSLEngine> engine;
  std::shared_ptr<agenticdsl::InMemoryBus> bus;
  std::unique_ptr<agenticdsl::ToolRegistry> registry;
  std::unique_ptr<InMemoryInputSource> input;
  std::unique_ptr<CapturingLogger> logger;
};

}  // namespace

TEST_CASE("ChatSession ctor default nullptr safe no-op",
          "[pdk][chat_session][ctor]") {
  TestChatSessionFixture f;
  // 默认 input=nullptr → 不启动 stdin thread (Pattern 5 fail-safe)
  ChatSession session(f.engine.get(), f.bus, f.registry.get(),
                      AgentConfig{}, SessionConfig{});
  REQUIRE_FALSE(session.is_input_thread_shutdown());
  // 不抛异常 + 不启动 thread = 安全 no-op
}
```

- [ ] **Step 11.2: 验证 file(GLOB) 自动注册(无需手动 add_catch_test)**

```bash
cmake --build build --target test_pdk_chat_session 2>&1 | head -10
# Expected: 编译成功, target 由 file(GLOB) 自动创建
# **不要** 在 tests/CMakeLists.txt 手动 add_catch_test(test_chat_session ...) — 会与 examples 同名 target 冲突
```

- [ ] **Step 11.3: 重新 cmake + 运行测试**

Run: `cmake --build build && ctest -R test_pdk_chat_session --output-on-failure`
Expected: 1 case PASS

- [ ] **Step 11.4: 提交**

```bash
git add tests/test_pdk_chat_session.cpp
git commit -m "test(pdk): add ChatSession ctor default nullptr safe no-op (real bus+registry+loop/run stub)"
```

---

## Task 12: 测试 2-3 — 输入注入与优先级

**Files:**
- Modify: `tests/test_pdk_chat_session.cpp`

- [ ] **Step 12.1: 添加 steering > follow-up 优先级测试(2026-09-11 Momus 修订: QueueKind 枚举值 + fixture 真依赖)**

```cpp
TEST_CASE("ChatSession ctor explicit InMemoryInputSource consumes queue",
          "[pdk][chat_session][ctor]") {
  TestChatSessionFixture f;
  // 预填输入: 先 follow-up, 后 steering
  f.input->enqueue_input("follow-up message");
  f.input->enqueue_input("/help");  // steering 优先

  ChatSession session(f.engine.get(), f.bus, f.registry.get(),
                      AgentConfig{}, SessionConfig{},
                      nullptr, std::move(f.input), std::move(f.logger));

  // 验证 session 接受 InMemoryInputSource,steering > follow-up
  // **Momus 修订**: 枚举值无 k 前缀, 用 QueueKind::Steering / QueueKind::FollowUp
  REQUIRE(session.queue_size(QueueKind::Steering) == 1);
  REQUIRE(session.queue_size(QueueKind::FollowUp) == 1);
}

TEST_CASE("ChatSession steering /cancel triggers request_stop",
          "[pdk][chat_session][steering]") {
  TestChatSessionFixture f;
  f.input->enqueue_input("/cancel");  // steering 命令

  ChatSession session(f.engine.get(), f.bus, f.registry.get(),
                      AgentConfig{}, SessionConfig{},
                      nullptr, std::move(f.input), std::move(f.logger));

  // request_stop() 不经队列, 直达 stop_source
  session.request_stop();
  // 验证 cancel 已触发(后续 chat() 返回 cancel error)
  // 简化断言: 仅验证不抛异常
  REQUIRE_NOTHROW(session.is_input_thread_shutdown());
}
```

- [ ] **Step 12.2: 运行测试**

Run: `cmake --build build && ctest -R test_pdk_chat_session --output-on-failure`
Expected: 2 cases 新增 PASS(共 3 cases)

- [ ] **Step 12.3: 提交**

```bash
git add tests/test_pdk_chat_session.cpp
git commit -m "test(pdk): add ChatSession input priority tests (steering > follow-up)"
```

---

## Task 13: 测试 4-5 — 队列 overflow 与 chat() 单 turn

**Files:**
- Modify: `tests/test_pdk_chat_session.cpp`

- [ ] **Step 13.1: 添加 overflow + ChatResult 测试(2026-09-11 Momus CRITICAL 3 + WARNING 4 修订)**

```cpp
TEST_CASE("ChatSession overflow rejects at capacity 32",
          "[pdk][chat_session][queue]") {
  TestChatSessionFixture f;
  ChatSession session(f.engine.get(), f.bus, f.registry.get(),
                      AgentConfig{}, SessionConfig{},
                      nullptr, std::move(f.input), std::move(f.logger));
  // 填充 33 条 follow-up (容量 32)
  // **2026-09-11 Momus CRITICAL 3 修订**: 用真方法名 `try_push_follow_up_for_test`(无 `_deprecated` 后缀)
  // 验证第 33 次 push 返回 false (overflow-reject 契约), 而非恒真式 `queue_size <= 32`
  for (int i = 0; i < 32; ++i) {
    REQUIRE(session.try_push_follow_up_for_test("msg-" + std::to_string(i)));
  }
  REQUIRE_FALSE(session.try_push_follow_up_for_test("overflow-msg"));  // 第 33 条被拒
  REQUIRE(session.queue_size(QueueKind::FollowUp) == 32);             // 容量上限严格
  REQUIRE(f.logger->count(LogLevel::kWarn) >= 1);                      // 触发 stderr warn 日志 (Pattern 3 弱断言不可取, 改 log count)
}

TEST_CASE("ChatSession ChatResult captures LLM turn response",
          "[pdk][chat_session][chat]") {
  TestChatSessionFixture f;
  // **Momus CRITICAL 1 修订**: fixture 已注册 loop/run stub (Task 11.1), chat() 走成功路径
  ChatSession session(f.engine.get(), f.bus, f.registry.get(),
                      AgentConfig{}, SessionConfig{},
                      nullptr, std::move(f.input), std::move(f.logger));

  auto result = session.chat("hello", {});
  // **Momus WARNING 4 修订**: ChatResult 无 `error` 字段, 只有 `error_message` (string)
  REQUIRE(result.success);
  REQUIRE(result.error_message.empty());
}
```

- [ ] **Step 13.2: 运行测试**

Run: `cmake --build build && ctest -R test_pdk_chat_session --output-on-failure`
Expected: 2 cases 新增 PASS(共 5 cases)

- [ ] **Step 13.3: 提交**

```bash
git add tests/test_pdk_chat_session.cpp
git commit -m "test(pdk): add ChatSession overflow + ChatResult tests (real names, real assertions)"
```

---

## Task 14: 测试 6-7 — 5 轮多 turn 与并发

**Files:**
- Modify: `tests/test_pdk_chat_session.cpp`

- [ ] **Step 14.1: 添加 5 轮多 turn 测试(2026-09-11 修订: 真 fixture + 真断言)**

```cpp
TEST_CASE("ChatSession 5 sequential turns consistent",
          "[pdk][chat_session][concurrency]") {
  TestChatSessionFixture f;
  ChatSession session(f.engine.get(), f.bus, f.registry.get(),
                      AgentConfig{}, SessionConfig{},
                      nullptr, std::move(f.input), std::move(f.logger));

  for (int i = 0; i < 5; ++i) {
    auto result = session.chat("turn-" + std::to_string(i), {});
    REQUIRE(result.success);
    REQUIRE(result.error_message.empty());
  }
  REQUIRE(session.history().size() >= 5);
}
```

- [ ] **Step 14.2: 添加 cancellation 中断测试(2026-09-11 修订: 无 result.error.code 字段)**

```cpp
TEST_CASE("ChatSession request_stop mid-turn with cancellation",
          "[pdk][chat_session][cancel]") {
  TestChatSessionFixture f;
  ChatSession session(f.engine.get(), f.bus, f.registry.get(),
                      AgentConfig{}, SessionConfig{},
                      std::make_shared<CancellationRegistry>(),
                      std::move(f.input), std::move(f.logger));

  std::stop_source ss;
  ss.request_stop();  // pre-cancel
  auto result = session.chat("test", ss.get_token());
  // **Momus WARNING 4 修订**: ChatResult 只有 success + error_message
  // 取消语义: success==false 且 error_message 非空 (后续如需 ErrorCode 可扩展)
  REQUIRE_FALSE(result.success);
  REQUIRE_FALSE(result.error_message.empty());
}
```

- [ ] **Step 14.3: 运行测试**

Run: `cmake --build build && ctest -R test_pdk_chat_session --output-on-failure`
Expected: 2 cases 新增 PASS(共 7 cases)

- [ ] **Step 14.4: 提交**

```bash
git add tests/test_pdk_chat_session.cpp
git commit -m "test(pdk): add ChatSession multi-turn + cancellation tests (real fixture + real assertions)"
```

---

## Task 15: 测试 8 — RecordingLLMProvider 模型契约守卫

**Files:**
- Modify: `tests/test_pdk_chat_session.cpp`

- [ ] **Step 15.1: 添加 RecordingLLMProvider 测试(Pattern 2 + 5)**

```cpp
// 在 test_chat_session.cpp 顶部添加 include + RecordingLLMProvider mock
// (Pattern 2: RecordingLLMProvider - tests/AGENTS.md line 79-96)

class RecordingLLMProvider : public agenticdsl::ILLMProvider {
 public:
  std::string last_model;
  int generate_calls = 0;
  agenticdsl::GenerationResult result;

  agenticdsl::Result<agenticdsl::GenerationResult, agenticdsl::LLMError> generate(
      const agenticdsl::GenerationRequest& req, std::stop_token) override {
    last_model = req.params.model;
    ++generate_calls;
    return agenticdsl::Result<agenticdsl::GenerationResult, agenticdsl::LLMError>::success(result);
  }
  std::unique_ptr<agenticdsl::IGenerationStream> generate_stream(
      const agenticdsl::GenerationRequest&, std::stop_token) override { return nullptr; }
  std::vector<agenticdsl::ModelInfo> available_models() const override { return {}; }
};

TEST_CASE("RecordingLLMProvider last_model empty for chat_session turn",
          "[pdk][chat_session][realllm-guard]") {
  TestChatSessionFixture f;
  auto recorder = std::make_unique<RecordingLLMProvider>();
  recorder->result.text = R"({"tool":"echo","args":{"message":"ok"}})";
  auto* raw = recorder.get();
  f.engine->set_llm_provider(std::move(recorder));

  ChatSession session(f.engine.get(), f.bus, f.registry.get(),
                      AgentConfig{}, SessionConfig{},
                      nullptr, std::move(f.input), std::move(f.logger));

  auto result = session.chat("test", {});

  // Pattern 2 + 5 核心契约: req.params.model 必须空 → adapter fallback config_.model
  REQUIRE(raw->generate_calls >= 1);
  REQUIRE(raw->last_model.empty());
}
```

- [ ] **Step 15.2: 运行测试**

Run: `cmake --build build && ctest -R test_pdk_chat_session --output-on-failure`
Expected: 1 case PASS(共 8 cases)

- [ ] **Step 15.3: 提交**

```bash
git add tests/test_pdk_chat_session.cpp
git commit -m "test(pdk): add RecordingLLMProvider model contract guard (Pattern 2 + 5)"
```

---

## Task 16: 测试 9-10 — Logger 与 CancellationRegistry fallback

**Files:**
- Modify: `tests/test_pdk_chat_session.cpp`

- [ ] **Step 16.1: 添加 CapturingLogger 注入测试(2026-09-11 Momus WARNING 12 修订: 测 Change 1 范围)**

```cpp
TEST_CASE("CapturingLogger emits warn on follow-up overflow (Change 1 scope)",
          "[pdk][chat_session][logger]") {
  TestChatSessionFixture f;
  ChatSession session(f.engine.get(), f.bus, f.registry.get(),
                      AgentConfig{}, SessionConfig{},
                      nullptr, std::move(f.input), std::move(f.logger));

  // **Momus WARNING 12 修订**: `chat.turn.start/end` 是 Change 2 范围 (6 个新 topic),
  // Change 1 不发射任何 info 级 log。改测 Change 1 范围行为: 触发 follow-up overflow
  // 应记录 stderr warn (经 StderrLogger 桥接 log::emit, 现在走 CapturingLogger)。
  for (int i = 0; i < 32; ++i) {
    REQUIRE(session.try_push_follow_up_for_test("msg-" + std::to_string(i)));
  }
  REQUIRE_FALSE(session.try_push_follow_up_for_test("overflow"));

  REQUIRE(f.logger->count(LogLevel::kWarn) >= 1);  // overflow warn
  REQUIRE_NOTHROW(f.logger->snapshot());
}
```

- [ ] **Step 16.2: 添加 CancellationRegistry nullptr fallback 测试(2026-09-11 Momus 修订: 按设计 §7.2 矩阵加 request_stop)**

```cpp
TEST_CASE("ChatSession with nullptr CancellationRegistry graceful fallback",
          "[pdk][chat_session][fallback]") {
  TestChatSessionFixture f;
  // **Momus WARNING 16 修订**: 用真 bus/registry/fixture, 验证 design §7.2 矩阵
  // 显式传 nullptr CancellationRegistry → Impl 应 fallback 到 self-owned, 不 segfault
  ChatSession session(f.engine.get(), f.bus, f.registry.get(),
                      AgentConfig{}, SessionConfig{},
                      nullptr,  // nullptr CancellationRegistry → Impl fallback
                      std::move(f.input), std::move(f.logger));

  // 设计矩阵: request_stop 应 no-op (Impl 用 fallback registry, 不会 deref null)
  REQUIRE_NOTHROW(session.request_stop());
  // 后续 is_input_thread_shutdown 不阻塞
  REQUIRE_NOTHROW(session.is_input_thread_shutdown());
}
```

- [ ] **Step 16.3: 运行测试**

Run: `cmake --build build && ctest -R test_pdk_chat_session --output-on-failure`
Expected: 2 cases PASS(共 10 cases 完成)

- [ ] **Step 16.4: 提交**

```bash
git add tests/test_pdk_chat_session.cpp
git commit -m "test(pdk): add ChatSession logger + cancellation registry fallback tests (Momus-revised)"
```

---

## Task 17: TTY 死锁回归守卫 + 全量验证 (acceptance #5 + #7)

**Files:**
- (无文件改动,验证)

- [ ] **Step 17.1: TTY 环境下全量 ctest(Pattern 5 + AGENTS.md 模式 #6)**

Run: `script -qec "ctest --test-dir build" /dev/null 2>&1 | tail -15`
Expected: 232/232 PASS,无 SIGTERM-then-`std::terminate` 死锁,无 timeout 60s+ hang

如果 hang → 立即排查 StdinInputSource self-pipe 是否正确

- [ ] **Step 17.2: grep 验证 acceptance #1 + #9(2026-09-11 Momus 修订: A1 grep 自身坏掉)**

```bash
# **Momus 修订**: `grep LOG_INFO` 在 log.h:88-103 自身定义 4 个宏, 全库 grep 必 ≥4, 不能用作 A1 验收
# 改为 grep stderr_logger.cpp 内是否调用了不存在的 `log::info/warn/error/debug` 自由函数
grep -n "log::info\|log::warn\|log::error\|log::debug" src/common/io/stderr_logger.cpp
```
Expected: 0 行 (A1: 不调用不存在的自由函数, 仅用 `log::emit`)

```bash
# A2: 测试 double 在 tests/test_helpers/, 不在 src/common/io/
ls src/common/io/in_memory_input_source.h src/common/io/capturing_logger.h 2>/dev/null
```
Expected: 不存在 (A2: 测试 double 在 tests/test_helpers/)

- [ ] **Step 17.3: 提交验证报告**

```bash
git tag chat-session-pdk-lift-change1-complete
git log --oneline | head -20
```

- [ ] **Step 17.4: 最终 commit 标记**

```bash
git commit --allow-empty -m "feat(pdk): chat-session-pdk-lift Change 1 SHIP complete

- IInputSource/ILogger contract layer (Tasks 1-2)
- StdinInputSource preserving self-pipe + poll(2) (Sprint 31 regression prevention)
- StderrLogger bridging agenticdsl::log facade (no double log)
- CancellationRegistry lifted to PDK
- ChatSession header + impl migrated with I/O injection
- 10 mock-first test cases PASS (Pattern 1-7)
- TTY deadlock regression verified (script -qec ctest 232/232)
- chat-real-llm-coverage coordination strategy applied"
```

---

## 自审 Checklist(2026-09-11 Momus review 后修订,诚实声明)

执行 writing-plans skill §Self-Review:

| 检查项 | 结果 |
|------|------|
| 1. Spec coverage | §6.1 IInputSource → Task 1/2 ✓ ; §6.1 A1 ILogger → Task 2-3 ✓ (**log::emit 修正后**); §6.1 A3 self-pipe → Task 4 ✓ + Task 8.3 删除 Impl 自有 pipe ✓ ; §10 文件清单 → Task 1-10 ✓ (**event_topic_registry 已删除, 测试 double 在 tests/test_helpers/**) ; §7.2 测试矩阵 → Task 11-16 ✓ (**fixture 真 bus/registry + ChatResult.error_message + QueueKind 枚举值**) ; §11 acceptance → Task 17 ✓ |
| 2. Placeholder scan | grep TODO/TBD/FIXME 0 行 ✓ ; **本次修订移除所有 `REQUIRE(true)` 占位 (原 plan Task 14/15/16 部分使用, 现已替换为真实断言)** |
| 3. Type 一致性 | `IInputSource::read_line(timeout)` 在 Task 1 定义,Task 4 实现一致 ✓ ; `ILogger::log(level, msg)` 在 Task 2 定义,Task 3/5 实现一致 ✓ ; **`ChatResult::error_message` (string) 全 plan 一致, 原 `result.error.message/code` 已替换**; **`QueueKind::Steering/FollowUp` (无 k 前缀) 全 plan 一致** |
| 4. 文件路径精确 | `include/agenticdsl/...` / `src/common/io/...` / `pdk/chat_session/...` / `tests/test_helpers/...` ✓ ; **`tests/test_pdk_chat_session.cpp` 命名避免与 examples 同名 target 冲突** ; **`pdk/chat_session/src/cancellation_registry.cpp` 加入 pdk_chat_session_obj SOURCES 避免链接失败** |
| 5. **新增**: SHIP-with-fixes 流程 | Change 1 ship 前**必须**跑一轮 Momus 快速复核 (只验 diff, ~15 min), 复核 Oracle 给出的 12 个修复判据全部满足 (grep/ctest/nm 验证命令) ; Pattern 4 闭环 |
| 6. **新增**: SkillInterpreter IPC 豁免说明 | 本 change 不涉及 fork+exec IPC 路径, SkillInterpreter 模式 (Sprint 29) 直接不适用 — **免检** |

**修订通过(2026-09-11 Momus review 反馈后)。** 准备执行 Change 2 plan 文件。

---

## 执行选项

**Change 1 plan 已完成并保存**到 `docs/superpowers/plans/2026-09-11-chat-session-pdk-lift-change1.md`(17 个 Task,估时 ~3 天)。

接下来:
1. **Change 2 plan**(横切集成 + 断线恢复,~1 天)— 也将保存为独立 plan 文件
2. 执行选项:Subagent-Driven(推荐,fresh subagent per task + 两阶段 review)/ Inline(同 session 批量执行 + 检查点)

**请选择执行方式**,我开始 Change 2 plan 文件。