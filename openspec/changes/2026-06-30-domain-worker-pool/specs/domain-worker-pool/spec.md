# Spec: DomainWorkerPool (Sprint 3 增量规格)

## ADDED Requirements

### Requirement: domain-worker-pool-lifecycle

`DomainWorkerPool` MUST 提供 start/stop 生命周期管理, 内部维护 N 个 `std::jthread` worker (默认 4, 构造时固定) + 共享 FIFO 任务队列 + 领域处理器注册表, 公开方法遵循 `idle / running / stopped` 显式状态机前置条件.

#### Scenario: Pool 默认构造与启动

- **WHEN** 构造 `DomainWorkerPool pool(4)` (无 bus)
- **THEN** pool 内部 MUST 创建 4 个 `std::jthread`, 全部 `joinable() == false` (未启动)
- **AND** pool.state() MUST 等于 `DomainWorkerPool::State::idle`
- **AND** pool MUST 持有空的 handlers_ 表与空的 task_queue_

#### Scenario: Pool 启动后 worker 进入等待

- **WHEN** 调用 `pool.start()`
- **THEN** pool.state() MUST 转换为 `DomainWorkerPool::State::running`
- **AND** 4 个 worker thread MUST 各自阻塞在 `condition_variable::wait` 内
- **AND** 启动后所有 worker 共享同一 FIFO 任务队列 (单队列多消费者)
- **AND** 若 start() 二次调用 MUST 抛 `std::logic_error` (状态机禁止重复 start)
- **AND** 若 pool 已 stopped, start() MUST 抛 `std::logic_error` (生命周期结束)

#### Scenario: Pool 优雅停止 (stop)

- **WHEN** 调用 `pool.stop()` 且 state == running
- **THEN** pool.state() MUST 转换为 `DomainWorkerPool::State::stopped`
- **AND** 所有 worker MUST 收到 `std::stop_token` 取消信号并退出循环
- **AND** 4 个 `std::jthread` MUST join 完成 (无 hang, 无 zombie thread)
- **AND** stop() 二次调用 MUST 为 no-op (幂等)
- **AND** in-flight task (worker 正在处理) MUST 正常完成 (handler 调用不中断)

#### Scenario: Pool 析构函数安全 (隐式 stop)

- **WHEN** `DomainWorkerPool` 析构时 state == running (用户未调 stop)
- **THEN** 析构函数 MUST 隐式调用 `stop()` 优雅关闭
- **AND** 析构函数 MUST 在 .cpp 中 out-of-line 定义 (PIMPL-lite 模式, 同 CognitiveWorker)
- **AND** 析构 MUST NOT 调用 `std::terminate()` (避免 std::jthread 析构陷阱)
- **AND** 单元测试 MUST 验证: start() 后直接析构 = 正常退出, 无 crash

### Requirement: domain-worker-pool-task-dispatch

`DomainWorkerPool` MUST 通过共享 FIFO 任务队列 + 条件变量实现多消费者派发, `submit_task()` 非阻塞且线程安全, N 个 worker 抢占同一队列实现负载均衡.

#### Scenario: 任务提交到共享队列

- **WHEN** 调用 `pool.submit_task(DomainTask{domain="code", tool_name="edit_file", ...})` 且 state == running
- **THEN** task MUST 在 `queue_mutex_` 保护下入队
- **AND** `condition_variable::notify_one` 唤醒一个 worker
- **AND** submit_task() MUST 立即返回 (非阻塞)
- **AND** 多个 worker 中最先 wait 的那个 MUST 抢到该 task

#### Scenario: 任务派发到 worker (InMemoryBus 验证)

- **WHEN** worker 抢到 task 并开始处理
- **THEN** 若 pool 持有 bus, MUST 推送 `domain.task.started` 事件:
  - payload.ok == true
  - payload.meta["domain"] == task.domain
  - payload.meta["tool_name"] == task.tool_name
  - payload.meta["output_key"] == task.output_key
- **AND** worker 查 handlers_ 表获取 handler 函数 (在 shared_lock 下查, 释放锁后调用)
- **AND** handler 抛异常时 MUST 推送 `domain.task.failed` 事件 (worker 继续)
- **AND** handler 正常返回时 MUST 推送 `domain.task.completed` 事件:
  - payload.ok == true
  - payload.data[output_key] == handler 返回的 nlohmann::json

### Requirement: domain-worker-pool-handler-registration

`DomainWorkerPool` MUST 提供线程安全的 `register_domain_handler()` 与 `unregister_domain_handler()`, 内部用 `std::shared_mutex` 保护 handlers_ 表, 重复注册抛异常, 未注册领域调用抛 `std::runtime_error`.

#### Scenario: 注册领域处理器

- **WHEN** 调用 `pool.register_domain_handler("code", [](const DomainTask&) { return json{...}; })`
- **THEN** handlers_["code"] MUST 存储该 callable
- **AND** 后续 `submit_task({domain="code", ...})` MUST 路由到该 handler
- **AND** 注册 MUST 在 `unique_lock` (写锁) 下完成, 阻塞所有读锁

#### Scenario: 重复注册抛异常

- **WHEN** `register_domain_handler("code", ...)` 已被调用过
- **AND** 再次调用 `register_domain_handler("code", ...)` (即使是相同 handler)
- **THEN** MUST 抛 `std::invalid_argument("domain already registered: code")`
- **AND** 原有 handler MUST NOT 被覆盖 (保持原 callable)

#### Scenario: 取消注册

- **WHEN** 调用 `pool.unregister_domain_handler("code")` 且 "code" 已注册
- **THEN** handlers_["code"] MUST 被移除
- **AND** 后续 `submit_task({domain="code", ...})` MUST 抛 `std::runtime_error("no handler for domain: code")` (由 worker 捕获, 推 domain.task.failed)
- **AND** 未注册的 domain 取消注册 MUST 抛 `std::out_of_range`

#### Scenario: 运行时注册/取消注册 + 并发 submit (TSan 验证)

- **WHEN** 一个线程持续调用 `register_domain_handler/unregister_domain_handler`
- **AND** 另一个线程持续调用 `submit_task` (1000 次)
- **THEN** MUST 无 data race (shared_mutex 保护)
- **AND** handlers_ 表操作 MUST 原子可见 (memory_order_acquire/release)

### Requirement: domain-worker-pool-exception-isolation

`DomainWorkerPool` MUST 在 worker_loop 内 try-catch handler() 调用, handler 抛出的任何异常 MUST NOT 导致 worker 退出或 pool 终止, 异常信息通过 `domain.task.failed` 事件发布.

#### Scenario: handler 抛 std::exception

- **WHEN** worker 调用 handler(task) 时 handler 抛 `std::runtime_error("file not found")`
- **THEN** worker_loop MUST 捕获异常, 设置 result.ok = false
- **AND** MUST 填充 result.error_code = `ErrorCode::Unknown`
- **AND** MUST 填充 result.meta["error_message"] = "file not found"
- **AND** MUST 推送 `domain.task.failed` 事件 (payload 含上述字段)
- **AND** worker MUST 继续 wait 下一个 task (worker 线程不退出)
- **AND** pool MUST 继续接受新的 submit_task (不进入 stopped 状态)

#### Scenario: handler 抛非 std::exception (如 int)

- **WHEN** handler 抛出非 std::exception 派生类型
- **THEN** worker_loop MUST 使用 `catch (...)` 兜底
- **AND** MUST 记录 "unknown exception" 错误并推送 `domain.task.failed`
- **AND** worker 继续运行

#### Scenario: 1000x handler 异常压力测试

- **WHEN** 10 个线程并发 submit 100 个 task (总计 1000), handler 50% 概率抛异常
- **THEN** 1000 个 task 全部 MUST 被处理 (异常不漏)
- **AND** 50% domain.task.failed + 50% domain.task.completed 事件 MUST 被推送
- **AND** 4 个 worker MUST 不崩溃, pool 状态 MUST 仍为 running (非 stopped)

### Requirement: domain-worker-pool-bus-integration

`DomainWorkerPool` MUST 支持构造时注入 `shared_ptr<IInteractionBus>`, 任务生命周期事件 (`domain.task.started/completed/failed`) MUST 通过该 bus 转发 (与 CognitiveWorker F7 契约一致).

#### Scenario: 构造时 bus 注入 (F7)

- **WHEN** `DomainWorkerPool pool(4, bus)` (双参数构造)
- **THEN** pool 内部 MUST 持有 bus 共享指针
- **AND** 后续 worker 推送的 `domain.task.*` 事件 MUST 经由该 bus 转发
- **AND** InMemoryBus 的 subscriber MUST 能 subscribe "domain.task.completed" 并收到事件

#### Scenario: 无 bus 构造 (向后兼容 MVP)

- **WHEN** `DomainWorkerPool pool(4)` (单参数构造, 无 bus)
- **THEN** pool 内部 bus_ MUST 为 nullptr
- **AND** worker MUST NOT 推送任何事件 (跳过 if (bus_) 检查)
- **AND** submit_task 与 handler 调用行为不变 (仅无事件)

#### Scenario: 事件 payload 字段 (Sprint 1b + ADR-0023 标准化)

- **WHEN** worker 推送 `domain.task.completed` 事件
- **THEN** payload MUST 包含:
  - `payload.ok` — bool (true 表示 handler 成功)
  - `payload.data[output_key]` — nlohmann::json (handler 返回值, P1 字段 ADR-0023)
  - `payload.meta["domain"]` — std::string
  - `payload.meta["tool_name"]` — std::string
  - `payload.meta["output_key"]` — std::string
  - `payload.meta["worker_id"]` — size_t (派发到该 task 的 worker 编号, 调试用)
- **AND** payload 字段 MUST 遵循 ADR-0023 P1-P4 标准化 (data / meta / error_code / trace_id)

### Requirement: domain-worker-pool-concurrent-safety

`DomainWorkerPool` MUST 在 1000x 并发任务下保持线程安全, 无 data race, 无 deadlock, 无优先级反转. CP.22 协议合规.

#### Scenario: 1000x 并发 submit_task (10 线程 × 100 task)

- **WHEN** 10 个线程各提交 100 个 task (总计 1000 个), 每个 task handler sleep 1ms
- **THEN** 1000 个 task 全部 MUST 被处理 (无丢失)
- **AND** 4 个 worker 各自 MUST 处理 ~250 个 task (负载均衡, ±10%)
- **AND** TSan (via Dockerfile.tsan) MUST 报告 0 data race
- **AND** pool 状态 MUST 仍为 running

#### Scenario: 1000x 并发 register + submit 混合

- **WHEN** 线程 A 持续 register/unregister_domain_handler (1000 次循环)
- **AND** 线程 B/C/D/E 持续 submit_task (各 250 次, 总 1000)
- **THEN** handlers_ 表操作 MUST 原子可见 (shared_mutex 保护)
- **AND** task 派发 MUST 不死锁 (锁顺序: queue_mutex_ → handlers_mutex_ 全局一致)
- **AND** TSan MUST 报告 0 data race, 0 deadlock

#### Scenario: CP.22 协议合规审计 (S3.T4)

- **WHEN** 审计 DomainWorkerPool 并发协议
- **THEN** 锁顺序 MUST 全局一致: `queue_mutex_` 总是先于 `handlers_mutex_` 获取 (S3.T4.1)
- **AND** MUST 无递归锁: handlers_ 持锁期间 MUST NOT 调用 handler() (S3.T4.2)
- **AND** MUST 无优先级反转: 所有 worker 同优先级, condition_variable 公平唤醒 (S3.T4.3)
- **AND** MUST 异常安全: handler() 异常 MUST 被捕获, worker 继续 (S3.T4.4)
- **AND** MUST 析构安全: ~DomainWorkerPool() 显式 stop() + join (S3.T4.5, 同 CognitiveWorker)
- **AND** MUST 使用 std::jthread 协作式取消: stop_token 优先于 notify (S3.T4.6)

### Requirement: domain-worker-pool-tsan-validation

`DomainWorkerPool` MUST 通过 `Dockerfile.tsan` 容器化 TSan 验证, 解决 ASLR 已知遗留 (plan §3.3 H1), 1000x 并发用例在容器内 TSan 干净.

#### Scenario: Dockerfile.tsan 构建 (S3.T5)

- **WHEN** `docker build -f Dockerfile.tsan -t hydraforge-tsan .` 执行
- **THEN** 构建 MUST 成功 (ubuntu:22.04 + gcc-13 + cmake + 项目源码)
- **AND** ASLR MUST 在容器内禁用 (`echo 0 > /proc/sys/kernel/randomize_va_space`)
- **AND** 编译选项 MUST 启用 TSan: `-fsanitize=thread -g -O1`

#### Scenario: Dockerfile.tsan 运行 ctest

- **WHEN** `docker run hydraforge-tsan ctest --output-on-failure` 执行
- **THEN** 退出码 MUST 为 0
- **AND** 50/50 test (30 baseline + 7 DomainWorkerPool + 9 CognitiveWorker + 4 其他新增) MUST PASS
- **AND** 1000x 并发用例 MUST TSan 干净 (0 data race 报告)
- **AND** 单元测试 (S3.T3 test case 3) MUST 1000x 任务零丢失

#### Scenario: CI 集成 (S3.T5 后续)

- **WHEN** CI 工作流运行 TSan 步骤
- **THEN** MUST 使用 Dockerfile.tsan 容器化执行
- **AND** MUST NOT 在宿主 ASLR 下直接运行 TSan (避免假阳性, 已知遗留)

### Requirement: domain-worker-pool-sprint4-contract-lock

`DomainWorkerPool` Sprint 3 实施 MUST 锁定对外契约, Sprint 4+ 仅在以下范围内变更.

#### Scenario: 锁定对外 API

- **THEN** 公开方法签名 `submit_task(DomainTask)`, `register_domain_handler(domain, handler)`, `unregister_domain_handler(domain)`, `start()`, `stop()` MUST 保持不变
- **AND** 事件 topic `domain.task.started/completed/failed` MUST 保持不变
- **AND** 事件 payload 字段 (`ok`, `data[output_key]`, `meta.domain/tool_name/output_key/worker_id/error_message`) MUST 保持不变
- **AND** 内部实现 (std::mutex + std::queue MVP vs LockFreeQueue Phase 2) MAY 在 Sprint 4+ 自由替换
- **AND** 替换实现 MUST 通过现有的 7 个 test_domain_worker_pool 测试 (零回归)
- **AND** Sprint 4 PDK 工具 (DEFINE_TOOL/AGENT 宏) 生成的 handler MUST 兼容此 API (handler 签名: `nlohmann::json(const DomainTask&)`)
