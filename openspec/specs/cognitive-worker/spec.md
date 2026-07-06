# cognitive-worker Specification

## Purpose
Phase 1 Sprint 2 升级 `SimpleCognitiveOrchestrator` (C1 单轮 ReAct MVP) → `CognitiveWorker` 完整包装,实施 ADR-0020 §2.2 per-agent 隔离模型(每 worker 独立 DSLEngine 实例 + 共享 IInteractionBus 与主线程通信);构造签名 `(unique_ptr<DSLEngine>, shared_ptr<IInteractionBus>)`,通过 IToolRegistry/IProviderFactory 抽象与 P1 解耦保持零依赖。
## Requirements
### Requirement: cognitive-worker-lifecycle

`CognitiveWorker` MUST 提供 start/stop 生命周期管理, 内部持有独立 `DSLEngine` 实例和 `IInteractionBus` 共享指针 (per-agent 隔离), 并通过显式状态机约束公开方法的前置条件.

#### Scenario: Worker 启动与优雅停止

- **WHEN** 调用 `worker.start()` 后立即调用 `worker.stop()`
- **THEN** start() 必须异步启动 std::thread 阻塞在 worker_loop
- **AND** stop() 必须设置状态机为 `stopped` 并通过 condition_variable 唤醒 Worker
- **AND** Worker 检测到状态为 `stopped` 后退出循环并 join thread
- **AND** stop() 调用后 std::thread::join() 必须立即返回 (无 hang)

#### Scenario: 状态机前置条件 (F6)

- **WHEN** Worker 处于 `idle` 状态
- **THEN** 调用 `submit_task(...)` MUST 抛 `std::logic_error` (Worker 未启动, 不接受任务)
- **AND** 调用 `start()` 二次 MUST 抛 `std::logic_error` (状态机禁止重复 start)
- **AND** 调用 `stop()` 为 no-op (幂等)

#### Scenario: 析构函数安全 (TD-CW-02 修复)

- **WHEN** `CognitiveWorker` 析构时 `state_ == running` (用户未调 stop())
- **THEN** 析构函数 MUST 隐式调用 `stop()` 优雅关闭 + join thread
- **AND** MUST NOT 调用 `std::terminate()` (避免 std::thread 析构陷阱)
- **AND** MUST 析构 `engine_` (unique_ptr) 和 `bus_` (shared_ptr) 按声明逆序
- **AND** 析构函数 MUST 在 .cpp 中 out-of-line 定义 (unique_ptr<DSLEngine> 需完整类型)
- **AND** 头文件 MUST 仅前向声明 `class DSLEngine;` (避免引入 core/engine.h)
- **AND** 单元测试 (T2.9) MUST 验证: start() 后直接析构 = 正常退出, 无 crash, 无泄漏

#### Scenario: 任务提交与事件发布

- **WHEN** 调用 `worker.submit_task(task_id, prompt)` 且当前状态为 `running`
- **THEN** Worker 内部 SimpleCognitiveOrchestrator 执行单轮 ReAct
- **AND** 通过 `bus_->emit("cognitive.task.started", ToolResult)` 发布开始事件
- **AND** task_id 写入 ToolResult::meta 字段 (事件 payload 关联)
- **AND** 任务执行完成后通过 `bus_->emit("cognitive.task.completed", result)` 发布完成事件
- **AND** result.trace_id = task_id (P3 字段, ADR-0023 标准化) 用于订阅者关联
- **AND** InMemoryBus 的 subscriber 可在事件循环中收到这些事件

### Requirement: cognitive-worker-construction-contract

`CognitiveWorker` 构造时 MUST 强制将 `IInteractionBus` 注入到 `DSLEngine` 内部, 保证 engine 触发的所有事件 (e.g. `dsl.call.started`, `tool.completed`) 通过 Worker 持有的 bus 转发.

#### Scenario: 构造时 bus 注入 (F7)

- **WHEN** `CognitiveWorker(unique_ptr<DSLEngine>, shared_ptr<IInteractionBus>)` 构造完成
- **THEN** 构造函数内部 MUST 调用 `engine_->set_interaction_bus(bus_)` (F7 顺序契约)
- **AND** 构造后调用 `engine_->get_interaction_bus()` MUST 返回与 Worker 持有的 bus 同一 shared_ptr 实例
- **AND** Worker thread 后续通过 engine 触发的事件 (Sprint 3+ integration) MUST 经由 Worker bus 转发

### Requirement: cognitive-worker-p1-integration

`CognitiveWorker` MUST 集成 P1 解耦的抽象: `IToolRegistry*` (P1.T2) + `IProviderFactory` (P1.T1) + `IInteractionBus` (Sprint 1b).

#### Scenario: 注入抽象到 SimpleCognitiveOrchestrator

- **WHEN** Worker 处理任务
- **THEN** 内部构造 `SimpleCognitiveOrchestrator(engine_->get_tool_registry(), engine_->get_llm_provider())`
- **AND** `get_tool_registry()` 返回 `IToolRegistry&` (P1.T4 PIMPL-lite 化)
- **AND** `get_llm_provider()` 返回 `ILLMProvider*` (C1.4 抽象)
- **AND** Worker 不直接依赖 `ToolRegistry` 或 `MockLLMProvider` 具体类型 (编译时验证)

### Requirement: cognitive-worker-error-propagation

`CognitiveWorker` MUST 在 LLM 调用失败时通过 `ToolResult` (P2-P4 标准化字段) + IInteractionBus 传播错误, 不抛异常至调用方.

#### Scenario: LLM 错误传播 (S2 修复 + TD-CW-03 bridge)

- **WHEN** LLM provider 返回错误 (例如 `LLMError::Code::AuthenticationError`)
- **THEN** SimpleCognitiveOrchestrator 内部捕获异常并包装为 `ToolResult::error(string, string)` deprecated 重载
- **AND** `CognitiveWorker::worker_loop` 在 bus emit 前 MUST 调用 bridge 函数
  `error_code_from_string(meta["error_code"])`, 将 legacy string 映射为 `ErrorCode` enum
- **AND** bus_->emit 事件 payload MUST 包含:
  - `payload.error_code` — `std::optional<ErrorCode>` enum (P2 字段, ADR-0023 11 个值之一)
  - `payload.meta.error_message` — 字符串描述
  - `payload.meta.error_code` — 原始 string 保留 (向后兼容)
  - `payload.trace_id` — task_id 关联键
- **AND** bridge 映射表 MUST 覆盖 SimpleCognitiveOrchestrator 9 处 error 路径 (T2.7 验证至少 2 个):
  - `ERR_LLM.NETWORK` → `ErrorCode::Retry`
  - `ERR_LLM.AUTH` → `ErrorCode::PermissionDenied`
  - 其他未列出 → `ErrorCode::Unknown` 或 `nullopt`
- **AND** Worker thread 不崩溃, 继续处理下一个 task
- **AND** 后续 Sprint 1c 扩展 ErrorCode enum + 升级 SimpleCognitiveOrchestrator 9 处 string 重载 (Sprint 2 不修, 由 bridge 兜底)

### Requirement: cognitive-worker-mvp-single-thread

`CognitiveWorker` MVP 实现 MUST 使用单线程 + 任务队列 (std::queue + std::mutex + std::condition_variable).

#### Scenario: 单线程 FIFO 处理

- **WHEN** 多个 task 顺序 submit
- **THEN** Worker 严格按 FIFO 顺序处理 (单线程保证)
- **AND** submit_task() 立即返回 (非阻塞, 仅入队 + notify_one)
- **AND** worker_loop 阻塞等待 condition_variable, 被唤醒后出队一个 task

#### Scenario: 多线程 submit 线程安全

- **WHEN** 多个线程同时调用 `worker.submit_task(...)`
- **THEN** 任务队列在 mutex 保护下安全追加
- **AND** condition_variable 通知保证 Worker 唤醒
- **AND** 无 data race (TSan 验证: 10 线程 × 100 次 = 1000 次并发)

### Requirement: cognitive-worker-sprint3-contract-lock

`CognitiveWorker` Sprint 2 实施 MUST 锁定对外契约, Sprint 3 仅在以下范围内变更.

#### Scenario: 锁定对外 API (S1 修复)

- **THEN** 公开方法签名 `submit_task(task_id, prompt)` MUST 保持不变
- **AND** 事件 topic `cognitive.task.started` / `cognitive.task.completed` MUST 保持不变
- **AND** 事件 payload 字段 (`trace_id`, `error_code`, `meta.error_message`) MUST 保持不变
- **AND** 内部实现 (SimpleCognitiveOrchestrator MVP vs `engine_->run_async()` Phase 2) MAY 在 Sprint 3+ 自由替换
- **AND** 替换实现 MUST 通过现有的 8 个 test_cognitive_worker 测试 (零回归)
