# async-runtime Specification

> **Purpose**: 追踪 ADR-0030 V2 Phase 2 实施 (Sprint 12 主体, 2026-07-XX ship)
> **状态**: 🟡 active (Oracle 咨询已完成 2026-06-27, 占位内容已填充)
> **Oracle 决议 session**: `ses_0f5541ebfffehKDxNVuYqB7bq4`

## ADDED Requirements

### Requirement: async-runtime-taskflow-dag-parallel

DAG 节点 MUST 支持并行派发 (Taskflow executor)

#### Scenario: DAG 节点并行派发 (无依赖关系)

- **WHEN** `TopoScheduler::execute_parallel(const Context&)` 被调用
- **AND** DAG 含 ≥ 2 个无依赖关系的节点
- **THEN** MUST 使用 `tf::Executor` 并行派发所有就绪节点
- **AND** 总体执行时间 MUST < 串行执行时间 (perf: 2 独立节点 < 2× 单节点时间)
- **AND** 节点完成顺序 MUST 符合 DAG 拓扑序

#### Scenario: Fork/Join 用 Taskflow Subflow

- **WHEN** DAG 含 FORK 节点带 N 个分支
- **THEN** MUST 用 `tf::Subflow` 并行执行 N 个分支
- **AND** JOIN 节点 MUST 等所有分支完成后才执行

#### Scenario: API 向后兼容

- **WHEN** 现有 `IScheduler` 实现调用 `execute()` 方法
- **THEN** MUST 保持原有行为不变 (顺序执行)
- **AND** 新增 `execute_parallel()` 为可选方法 (默认实现可调用 `execute()`)

### Requirement: async-runtime-context-fork-merge

Context MUST 支持 `fork()` 深拷贝 + `merge()` 策略合并 (不可变分支)

#### Scenario: Context::fork 深拷贝

- **WHEN** `Context::fork()` 被调用
- **THEN** MUST 深拷贝所有 Layer 字段
- **AND** ExecutionBudget (非拷贝, 含原子计数器) MUST 通过 std::optional 移动而非拷贝
- **AND** 副本 MUST 独立于原 Context, 修改副本不影响原 Context

#### Scenario: Context::merge 子覆盖父

- **WHEN** `child.merge(parent)` 被调用
- **AND** child 与 parent 有同名 key
- **THEN** MUST 采用 child 值 (子优先策略)
- **AND** child 独有的 key MUST 保留
- **AND** parent 独有的 key MUST 丢弃 (不合并 parent-only 字段)

#### Scenario: Fork/merge 在 DAG 节点派发中使用

- **WHEN** DAG 节点派发前
- **THEN** MUST 调用 `context.fork()` 创建副本
- **AND** 节点完成时 MUST `child_context.merge(parent_context)` 应用子节点修改

### Requirement: async-runtime-stream-to-bus-runner

IGenerationStream MUST 支持通过 bridge runner 转为 IInteractionBus 事件流 (Oracle Q2 决议)

#### Scenario: run_stream_to_bus 发送 token 事件

- **WHEN** `run_stream_to_bus(stream, bus, token, request_id)` 被调用
- **AND** `stream.next(token)` 返回非 nullopt
- **THEN** MUST `bus.emit("llm.token", ToolResult::success({{"token", *next_value}, {"request_id", request_id}}))`
- **AND** 必须按 next() 调用顺序 emit (token 顺序保持)

#### Scenario: run_stream_to_bus 发送流完成事件

- **WHEN** `stream.is_active()` 返回 false (流结束)
- **THEN** MUST `bus.emit("llm.token.done", ToolResult::success({{"finish_reason", "stop"}, {"token_count", N}}))`

#### Scenario: run_stream_to_bus 发送错误事件

- **WHEN** `stream.next(token)` 抛异常 或 `stream.is_error()` 返回 true
- **THEN** MUST `bus.emit("llm.token.error", ToolResult::error(LLMError{code, message, request_id}))`
- **AND** MUST 停止后续 emit (不调用更多 next())

#### Scenario: run_stream_to_bus 响应 stop_token

- **WHEN** `stop_token.stop_requested()` 返回 true (外部取消)
- **THEN** MUST 立即停止 pull-loop
- **AND** MUST emit `llm.token.done` (finish_reason="cancelled")
- **AND** MUST 返回当前累积的 GenerationResult (可能不完整)

#### Scenario: bridge 与 LLM provider 解耦

- **WHEN** `run_stream_to_bus` 实现
- **THEN** MUST NOT 在 `src/common/llm/` 内修改 LlamaAdapter/CloudAdapter/HttpAdapter/MockProvider
- **AND** MUST 接受任意实现 `IGenerationStream` 的对象 (duck typing)
- **AND** MUST 仅依赖 `agenticdsl::contract::IInteractionBus` (contract 层)

### Requirement: async-runtime-eventbus-backend

IInteractionBus 后端 MUST 切换为 EventBus MPMC 有界队列 (解决 bridge 背压)

#### Scenario: InMemoryBus emit 不阻塞

- **WHEN** `bus.emit("topic", payload)` 被调用
- **AND** subscriber 正在处理慢操作 (e.g., 终端渲染)
- **THEN** MUST 不阻塞调用线程
- **AND** payload MUST 入队等待 dispatch

#### Scenario: 1000x 并发 emit 无 race

- **WHEN** `cmake --preset tsan && ctest -R interaction_bus` 1000x 并发 emit
- **THEN** MUST 0 race (TSan clean)
- **AND** 所有事件 MUST 最终被 dispatch (无丢失)

#### Scenario: 公共 API 不变

- **WHEN** InMemoryBus 后端切换为 EventBus MPMC
- **THEN** `IInteractionBus` 公共 API MUST 不变 (emit/subscribe/unsubscribe 签名)
- **AND** 所有现有 caller MUST 无修改编译通过

### Requirement: async-runtime-no-async-simple

`external/async_simple/` CMake 依赖 MUST 移除 (V2 决策: std::jthread 替代 async_simple 协程层)

#### Scenario: async_simple CMake 引用清除

- **WHEN** `git grep "async_simple" CMakeLists.txt src/CMakeLists.txt` 被执行
- **THEN** MUST 返回 0 命中
- **AND** `external/async_simple/` 目录 MUST 标记 deprecated (README 警告 + git log 保留)

#### Scenario: 编译不依赖 async_simple

- **WHEN** `cmake --build build` 全新构建
- **THEN** MUST 不生成 async_simple 相关的 .a/.o 中间文件
- **AND** `find build -name "*async_simple*"` MUST 返回 0 文件

#### Scenario: 现有 ctest 不受影响

- **WHEN** `ctest --output-on-failure` 在移除 async_simple 后
- **THEN** MUST 41/41 PASS (35 baseline + 6 new C2 tests)
- **AND** 零回归

### Requirement: async-runtime-adr-0025-defer

Fleet 模式 16 路 LLM MUST DEFER 到 Phase 3+ (Oracle Q1 决议)

#### Scenario: C2 不交付 FleetOrchestrator

- **WHEN** Sprint 12 实施完成
- **THEN** `src/common/llm/fleet_orchestrator.h` MUST NOT 存在
- **AND** `tests/test_fleet_orchestrator.cpp` MUST NOT 存在
- **AND** `examples/slice_04_fleet/` MUST NOT 存在

#### Scenario: DomainWorkerPool 提供 16 路能力

- **WHEN** 调用 `DomainWorkerPool pool(16)` 构造
- **THEN** MUST 启动 16 个 std::jthread worker
- **AND** 16 个并发 domain task MUST 正确派发

#### Scenario: FleetOrchestrator 触发条件 (Phase 3+)

- **WHEN** 真实用例出现 (ensemble inference / multi-model voting / batch eval)
- **THEN** 创建新 OpenSpec change 实施 FleetOrchestrator
- **AND** 复用 DomainWorkerPool(16) 基础设施

## REMOVED Requirements

### ~~async-runtime-fleet-mode-16x~~ (Oracle Q1 DEFER)

- **原描述**: 16 路 LLM 调用 MUST 支持并行提交+聚合
- **移除原因**: 0 examples 使用并行 LLM, DomainWorkerPool 已提供 N-way 能力, FleetOrchestrator 是无消费者投机性工作
- **Defer 到**: Phase 3+ (ensemble inference / multi-model voting 触发)

### ~~async-runtime-streaming-yield (协程 yield)~~ (V1 方案 V2 不采用)

- **原描述**: LLM Token 流 MUST 支持协程 yield 或 stream 句柄
- **移除原因**: V2 决策 std::jthread (不引入 async_simple 协程), Token 流推送改用 IInteractionBus 事件

## REMOVED Requirements (deferred)

### async-runtime-approval-suspend (依赖 C3)

- **原描述**: 用户审批 MUST 支持协程 suspend 或 EventBus 阻塞
- **Defer 到**: Sprint 13+ (C3 ADR-0031 完整接口后, 用户审批依赖 IExecutionPolicy::request_approval)