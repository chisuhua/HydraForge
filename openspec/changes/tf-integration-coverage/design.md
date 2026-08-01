## Context

TopoScheduler 在 Sprint 12 C2 Day 1-2 引入 `execute_parallel` 路径,使用 `tf::Executor` + `tf::Taskflow` 派发独立节点。Sprint 18 D-4/D-5 将其拆分为 `setup + execute_dag_loop` (≤50 行)。截至本 change 之前,该路径仅有 5 个 happy-path 测试 case (3 个无依赖 ToolCallNode + 2 个 TF 库 smoke test),未触发以下核心不变式:

1. `topo_scheduler.cpp:282-288` 依赖边构建 `tf_tasks[path].succeed(tf_tasks[dep])`
2. `topo_scheduler.cpp:246-252` 跨调用复用 `parallel_executor_/parallel_taskflow_`
3. `topo_scheduler.cpp:268-271` 错误处理分支 `process_jump / return`
4. `topo_scheduler.cpp:265-280` lambda 捕获 `state` / `locally_executed_mutex` 在并发下的争用
5. `TopoScheduler::Config` 无 worker 数注入,无法确定性验证并发度

Sprint 18 已在 ADr 0030 V2 ship gate 中标注 49/49 ctest 零回归,但**集成契约未验证**。后续任何对 `execute_parallel` 路径的重构都缺乏回归网,需要手工集成验证。

## Goals / Non-Goals

**Goals:**
- 验证 `execute_parallel` 路径的全部 9 个 GIVEN/WHEN/THEN 契约 (见 proposal.md)
- 通过 `Config::num_workers` 字段,使 worker 数可注入,P2 并发度测试确定性
- 维持 49/49 ctest 零回归,新增 15 case (test_execute_parallel.cpp 3→8,新建 test_execute_parallel_advanced.cpp 7 case)
- 维持 ASan/TSan 零失败,新增大 DAG case 零 data race

**Non-Goals:**
- 不修改 `execute_parallel / execute_dag_loop` 公开签名
- 不替换 `tf::Taskflow` 为其他库
- 不修改 `parallel_executor_/parallel_taskflow_` PIMPL 持有设计
- 不实现 `CancellationToken` API (留独立提案)
- 不重写 `TopoScheduler` 主循环 (Sprint 18 D-4/D-5 已完成)
- 不修改 `agenticdsl_common` 模块

## Decisions

### Decision 1: 字段命名 `num_workers` (而非 `max_workers` / `worker_count`)

**Rationale**:
- `std::thread::hardware_concurrency()` 命名风格一致
- 简短,与现有 `tf::Executor(N)` 参数语义对齐
- 字段含义明确 (确切 worker 数,非并发度上限)

**Alternatives Considered**:
- `max_workers` - 暗示软上限,与 `tf::Executor(N)` 的确切数语义不符
- `parallelism` - 抽象度太高,与"线程数"实际语义不符

### Decision 2: 默认值 `0` (= `hardware_concurrency()`)

**Rationale**:
- 0 是常见 sentinel value,在 `tf::Executor` API 中是 "auto" 语义
- 默认构造路径完全保持现有行为,零回归风险
- 配置文件加载代码无需特殊处理 (空 Config 即为 auto)

**Alternatives Considered**:
- 默认 1 - 改变行为,破坏现有 production 配置
- 默认 `std::thread::hardware_concurrency()` - 类型不是 compile-time constant,需要运行时初始化,Config 字段需函数调用而非字段赋值

### Decision 3: 退化逻辑 `num_workers == 0 ? max(1u, hw_concurrency()) : num_workers`

**Rationale**:
- 与 `topo_scheduler.cpp:247-248` 现状 `std::max(1u, std::thread::hardware_concurrency())` 行为一致
- 1 worker 兜底防止 `hardware_concurrency()` 返回 0 (嵌入式/容器化场景)
- 退化后 0 路径与现状字节级一致

### Decision 4: 测试文件分离 (`test_execute_parallel.cpp` 扩 + 新建 `test_execute_parallel_advanced.cpp`)

**Rationale**:
- 原文件 92 行,扩 5 case 后约 200 行,符合 AGENTS.md ≤200 行风格约束
- 高级 case (大 DAG / fork-join / 错误注入) 单独文件便于 TSan 单独跑
- CMake `file(GLOB test_*.cpp)` 自动收录,零 CMake 变更

**Alternatives Considered**:
- 单文件扩展至 ~300 行 - 违反 ≤200 行风格
- 按缺口 9 个 case 各一个文件 - 过度拆分,管理成本高

### Decision 5: 失败注入用 `try/catch` 包裹工具回调,而非改 Session 行为

**Rationale**:
- 保持 Session 契约不变 (零生产代码修改)
- 验证错误传播路径存在但不破坏
- 与 Sprint 19 D-8 `IApprovalHandler` 错误码 bridge 模式一致

**Alternatives Considered**:
- 修改 Session 异常处理 - 超出本 change 范围,且需独立 ADR

## Risks / Trade-offs

### Risk 1: `num_workers == 0` 退化路径的零回归保证

**Mitigation**:
- 退化逻辑写为 1 行 ternary expression,行为字节级与现状一致
- 默认 worker 退化 case 显式断言 `tf::Executor` 线程数 = `hardware_concurrency()`
- 49/49 → 64/64 零回归,ASan/TSan 100%

### Risk 2: 大 DAG 100 节点 case 的 CI 时长

**Mitigation**:
- 100 节点 + 8 worker + flat DAG,典型 4 核 CI 跑 ~2-3s
- elapsed 是软约束 (<5s 正常 / <10s + sanitizer),非硬性
- 若 CI 实际超时,可缩到 50 节点

### Risk 3: Fork/Join + parallel 模式的 race condition

**Mitigation**:
- TSan 必须 0 data race (Sprint 19 已建立 TSan 流程)
- 若 TSan 暴露真 race,记录为独立 follow-up (本 change 不修生产代码)
- fork/join 路径在 Sprint 17 C.3 已 ship,稳定性高

### Risk 4: test_async_bridge.cpp 的命名误导

**Mitigation**:
- 保持现状 (2 case smoke test),但加注释明确 "Taskflow 库 smoke test,async_simple 已 deprecated"
- 不强行重命名,避免无关 diff 噪音

### Trade-off 1: 字段追加 vs 重构 Config

**Trade-off**: Config 字段追加 1 个,序列化/反序列化零影响;但 Config 字段数持续增长。
**Decision**: 接受 (本 change 仅 1 字段,尚未触发重构阈值)

### Trade-off 2: 测试 case 数 (15) vs 现有 5

**Trade-off**: ctest 总数从 49 → 64 (+31%),测试时长增加。
**Decision**: 接受 (15 case 单 case 平均 <0.5s,总计 <8s,符合 CI 预算)
