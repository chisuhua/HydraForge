# chat-async-io-queue-infra

**STATUS**: 🔍 Proposed
**日期**: 2026-08-08
**来源**: improvements/chat-async-io-queue-infra.md (Wave 3-A Phase A)
**类型**: feature (infrastructure)
**优先级**: P1 (无阻塞依赖, 可独立 ship)
**关联**: Phase 0 `fix-tool-registry-signal-handler-shutdown` (✅ shipped, 提供干净 shutdown 路径)
**关联**: Phase B `chat-async-io-cancellation-chain` (依赖本 change 的 queue 基础设施)

## Why

`pdk_chat_demo` 当前使用 sync `std::getline` 循环 (`main.cpp:466`)，Agent turn 执行期间用户无法输入。Wave 3-A 目标是为 pdk_chat_demo 添加 pi-agent 风格的 `agent.steer()` / `agent.followUp()` 借鉴路径，但需要先建立**双生产者协作基础设施**（steering vs follow-up 队列 + input 线程分离）。

本 change (Phase A) 仅添加**数据结构 + sync 测试路径**，不依赖 stop_token 链路（Phase B 引入）。这样可独立 ship 与验证，奠定 Phase B 的注入目标。

**为何现在**：
- Phase 0 已 ship，mock 模式崩溃路径已修复，E2E 测试可靠性已恢复
- Phase B 7 步 stop_token 链路 wiring 依赖 queue 基础设施作为 turn 中断点注入目标
- 提前 ship Phase A 降低 Phase B 单 change 复杂度

## What Changes

### 核心改动

- **`ChatSession::Impl` 添加 `steering_queue_` + `follow_up_queue_`**：双有界队列（默认上限 32 条），producer-consumer 模式，分别承担"中断调整"和"排队接续"语义
- **Input 线程分离**：从 main 交互循环分离 stdin 读取到独立线程，分类入队
- **队列 overflow 策略文档化**：steering 拒绝新 + log warning，follow-up 拒绝新 + log warning
- **队列状态 API**：`queue_size(steering/follow_up)` + `clear_queue(steering/follow_up)`，便于测试与调试
- **sync 路径验证**：mock 模式 E2E 测试覆盖 queue 行为（enqueue/dequeue/overflow）

### 不修改范围 (Non-goals)

- 不修改 `ChatSession::chat()` 接口签名（Phase B 引入 `std::stop_token` 参数时统一处理）
- 不引入 async I/O 框架（仅 std::thread + mutex + condition_variable）
- 不实现 turn 中断点注入（→ Phase B）
- 不实现 `/model` 切换（→ Phase C）
- 不动 TUI 渲染层（chat-streaming-slash-tui 已 ship）

## Capabilities

### New Capabilities

- `chat-async-queue-infra`: ChatSession 内置 steering/follow-up 双队列 + 输入线程分离，定义 producer-consumer 协作原语

### Modified Capabilities

无 — 本 change 不修改任何已有 spec 的 REQUIREMENTS（仅新增基础设施）

## Impact

### 受影响代码

- `examples/pdk_chat_demo/chat_session.h` — Impl 新增 2 个 queue 成员 + input 线程
- `examples/pdk_chat_demo/chat_session.cpp` — Impl 构造函数启动 input 线程，析构函数 join
- `examples/pdk_chat_demo/chat_session.h` — 公开 API: `queue_size(QueueKind)` + `try_clear_queue(QueueKind)`
- `examples/pdk_chat_demo/tests/test_chat_session_queues.cpp` (新建) — sync 路径 E2E 测试

### API 影响

- 新增公开 API: `ChatSession::queue_size(QueueKind)` + `try_clear_queue(QueueKind)`
- `ChatSession::Impl` 私有成员扩展（不影响外部 API）
- 无 schema / ABI 变更

### 依赖系统

- `ChatSession::Impl` 现有成员（engine, bus, registry, config, messages, provider）
- `std::thread` + `std::mutex` + `std::condition_variable` (C++20 标准库)
- `examples/pdk_chat_demo/commands/` (下一步 Phase B 消费队列)

### 测试影响

- 新增测试: `tests/test_chat_session_queues.cpp` 覆盖 4 个场景
  - steering 入队 + 出队
  - follow-up 入队 + Agent turn 完成后接续
  - steering 队列满时拒绝新输入
  - follow-up 队列满时拒绝新输入
- 期望 ctest 增量: +1 测试文件 (4 cases)
- 现有 3 个 pre-existing 失败不变

### 风险评估

- **风险等级**: 🟡 LOW-MEDIUM
- **理由**:
  - 新增数据结构 + 线程, 最复杂部分是线程生命周期 (RAII 析构 join)
  - sync 路径可独立测试 (不依赖 stop_token 链路)
  - 与主交互循环可能有 race condition (input 线程 vs main 线程同时访问 cin)
- **缓解**: 
  - 双 mutex 保护 (避免单 mutex 死锁)
  - 析构函数显式 join (避免 use-after-free)
  - 同步模式 e2e 测试覆盖线程安全
- **回退方案**: 删除 ChatSession::Impl 新增字段即可 (无 API 变更)