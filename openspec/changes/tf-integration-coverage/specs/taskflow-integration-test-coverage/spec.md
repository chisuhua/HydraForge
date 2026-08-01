# taskflow-integration-test-coverage Specification

## Purpose
定义 TopoScheduler 与 Taskflow (`tf::Executor + tf::Taskflow`) 集成路径的测试契约,覆盖 9 个 GIVEN/WHEN/THEN 不变式,确保 `execute_parallel` 路径在 Sprint 18-19 持续重构期间的回归网。

## ADDED Requirements

### Requirement: 依赖边派发契约

`execute_parallel` MUST 在多节点 DAG 中按拓扑序派发任务,所有 `tf_tasks[path].succeed(tf_tasks[dep])` 边生效,保证依赖节点完成后才执行后续节点。

#### Scenario: 5 节点线性链 A→B→C→D→E 顺序执行
- **WHEN** `execute_parallel` 派发 5 节点线性链
- **THEN** `state.executed_` 集合按 A→B→C→D→E 顺序递增,每步大小 +1

### Requirement: 跨调用复用契约

`TopoScheduler` MUST 在多次 `execute_parallel` 调用间复用同一 `tf::Executor` 实例 (`parallel_executor_` 指针地址恒定),不重新构造线程池。

#### Scenario: 同 scheduler 多次调用复用 Executor
- **GIVEN** 已跑过 3 节点 `execute_parallel` 的 scheduler
- **WHEN** 再次调用跑 5 节点
- **THEN** `&parallel_executor_` 指针地址在两次调用间保持恒定

### Requirement: 错误传播契约

`execute_parallel` MUST 在工具抛异常时不导致整个执行崩溃,`locally_executed` 不包含失败节点,`finalize_execution` 返回 `success=false`。

#### Scenario: ToolCallNode 工具抛 runtime_error
- **GIVEN** ToolCallNode 注册的工具实现抛 `runtime_error("boom")`
- **WHEN** `execute_parallel` 派发该节点
- **THEN** `tf::Task` 异常被 try/catch 吞掉,`locally_executed` 不含该节点,`finalize_execution` 返回 `success=false`

### Requirement: 混合节点类型契约

`execute_parallel` MUST 支持 `ToolCallNode / LLMNode (mock provider) / ForkNode / JoinNode` 混排,所有 6 节点完成,fork 分支由 join 同步。

#### Scenario: 6 节点混合 (3 ToolCall + 1 LLM + 1 Fork + 1 Join)
- **WHEN** `execute_parallel` 派发 6 节点混合 DAG
- **THEN** 6 个节点全部完成,fork 分支数 = fork_node.branches.size(),join 等待所有分支完成

### Requirement: Worker 数注入契约

`TopoScheduler::Config::num_workers` MUST 允许注入确切 worker 数,`0` 退化到 `max(1u, hardware_concurrency())`,非 0 时 `tf::Executor` 线程数 = 该值。

#### Scenario: Config{num_workers=2} 限制并发度 ≤ 2
- **GIVEN** `Config{num_workers=2}`
- **WHEN** 4 个独立 ToolCallNode 跑
- **THEN** `max_concurrent` 观察到 ≤ 2

#### Scenario: Config{} 默认 0 退化到 hardware_concurrency
- **GIVEN** `Config{}` (未设 num_workers)
- **WHEN** `execute_parallel` 初始化 `tf::Executor`
- **THEN** `tf::Executor` 线程数 = `std::max(1u, std::thread::hardware_concurrency())`

### Requirement: 大 DAG 规模契约

`execute_parallel` MUST 支持 100 节点 flat DAG 在 `num_workers=8` 下全部完成,`elapsed < 5s` (CI +sanitizer 时放宽到 <10s),无锁死锁,无 lambda 捕获遗漏。

#### Scenario: 100 节点 flat DAG + 8 worker
- **WHEN** `execute_parallel` 派发 100 节点 flat DAG + `Config{num_workers=8}`
- **THEN** 全部 100 节点完成,`elapsed < 5s` (软约束,CI 放宽到 <10s),`max_concurrent ≈ 8`

### Requirement: Fork/Join 并行契约

`execute_parallel` MUST 在 ForkNode 分支执行时正确同步 JoinNode,join 等待所有 fork 分支完成后才执行。

#### Scenario: 1 ForkNode 分 4 支 + JoinNode
- **GIVEN** 1 ForkNode 分 4 支,每支 1 ToolCallNode + 1 JoinNode
- **WHEN** `execute_parallel` 派发
- **THEN** join 等待 4 支全部完成后才执行,`state.executed_` 大小 = 6 (1 fork + 4 branch + 1 join)

### Requirement: 边界条件契约

`execute_parallel` MUST 在空 DAG 和单节点 DAG 下返回成功,不创建 tf::Task (空 DAG) 或创建 1 个 tf::Task (单节点)。

#### Scenario: 空 DAG 返回 success=true
- **GIVEN** 0 节点 scheduler
- **WHEN** `execute_parallel`
- **THEN** `success=true` 无 tf::Task 创建

#### Scenario: 单节点 DAG 返回 success=true
- **GIVEN** 1 节点 scheduler
- **WHEN** `execute_parallel`
- **THEN** `success=true` 创建 1 个 tf::Task,该节点执行完成

### Requirement: 析构安全契约

`TopoScheduler` 析构 MUST 正确 join 所有 in-flight `tf::Task`,无死锁,无 SIGSEGV。

#### Scenario: 析构时 in-flight task 全部 join
- **GIVEN** 存在 100 节点 in-flight `tf::Task`
- **WHEN** `~TopoScheduler()` 析构
- **THEN** `~tf::Executor()` 隐式 join 全部 in-flight task,析构返回后无后台线程,无 SIGSEGV
