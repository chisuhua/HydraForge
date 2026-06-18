# Spec: CognitiveWorker (Sprint 2 增量规格)

## ADDED Requirements

### Requirement: cognitive-worker-lifecycle

`CognitiveWorker` MUST 提供 start/stop 生命周期管理, 内部持有独立 `DSLEngine` 实例和 `IInteractionBus` 共享指针 (per-agent 隔离).

#### Scenario: Worker 启动与优雅停止

- **WHEN** 调用 `worker.start()` 后立即调用 `worker.stop()`
- **THEN** start() 必须异步启动 std::thread 阻塞在 worker_loop
- **AND** stop() 必须设置 `running_=false` 并通过 condition_variable 唤醒 Worker
- **AND** Worker 检测到 `running_=false` 后退出循环并 join thread
- **AND** stop() 调用后 std::thread::join() 必须立即返回 (无 hang)

#### Scenario: 任务提交与事件发布

- **WHEN** 调用 `worker.submit_task(task_id, prompt)`
- **THEN** Worker 内部 SimpleCognitiveOrchestrator 执行单轮 ReAct
- **AND** 通过 `bus_->publish("task.<task_id>.started", {})` 发布开始事件
- **AND** 任务执行完成后通过 `bus_->publish("task.<task_id>.completed", result.to_json())` 发布完成事件
- **AND** InMemoryBus 的 subscriber 可在事件循环中收到这些事件

### Requirement: cognitive-worker-p1-integration

`CognitiveWorker` MUST 集成 P1 解耦的抽象: `IToolRegistry*` (P1.T2) + `IProviderFactory` (P1.T1) + `IInteractionBus` (Sprint 1b).

#### Scenario: 注入抽象到 SimpleCognitiveOrchestrator

- **WHEN** Worker 处理任务
- **THEN** 内部构造 `SimpleCognitiveOrchestrator(engine_->get_tool_registry(), engine_->get_llm_provider())`
- **AND** `get_tool_registry()` 返回 `IToolRegistry&` (P1.T4 PIMPL-lite 化)
- **AND** `get_llm_provider()` 返回 `ILLMProvider*` (C1.4 抽象)
- **AND** Worker 不直接依赖 `ToolRegistry` 或 `MockLLMProvider` 具体类型 (编译时验证)

### Requirement: cognitive-worker-error-propagation

`CognitiveWorker` MUST 在 LLM 调用失败时通过 `ToolResult` + IInteractionBus 传播错误, 不抛异常至调用方.

#### Scenario: LLM 错误传播

- **WHEN** LLM provider 返回错误 (例如 `LLMError::Code::AuthenticationError`)
- **THEN** SimpleCognitiveOrchestrator 内部捕获异常并包装为 `ToolResult::error(...)`
- **AND** bus_->publish 包含 `error_code` 和 `error_message` 字段
- **AND** Worker thread 不崩溃, 继续处理下一个 task

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
- **AND** 无 data race (TSan 验证)
