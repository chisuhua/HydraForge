## Why

TopoScheduler 与 Taskflow (`tf::Executor + tf::Taskflow`) 的集成核心契约 (依赖边、错误传播、跨调用复用、worker 注入) 缺乏测试覆盖。现 `tests/test_execute_parallel.cpp` (3 case) + `tests/test_async_bridge.cpp` (2 case) 全部覆盖"无依赖 happy path", 5 个核心契约点零验证。阻塞 Sprint 18-19 对 `execute_parallel` 路径的持续重构缺乏回归网,任何变动都需要手工验证集成行为。

## What Changes

- **新增** `TopoScheduler::Config::num_workers` 字段 (size_t, 默认 0)。`0` 表示使用 `std::max(1u, std::thread::hardware_concurrency())`,与现状完全一致;非 0 注入确切 worker 数,使并发度测试确定性。
- **修改** `topo_scheduler.cpp:247-248`,从 Config 读 `num_workers` 替代硬编码。
- **新增** `tests/test_execute_parallel.cpp` 5 个测试 case (依赖链派发 / 多调用复用 / 失败注入传播 / 混合节点类型 / worker 注入)。
- **新增** `tests/test_execute_parallel_advanced.cpp` 7 个测试 case (大 DAG 100 节点 / fork-join 混排 / 默认 worker 退化 / cancellation-via-destruction / 边界 0 节点 / 边界 1 节点 / 边界含 cycle 检测)。
- **不修改** `execute_parallel / execute_dag_loop` 公开签名。
- **不修改** `tf::Taskflow` PIMPL 持有 (`parallel_executor_/parallel_taskflow_`) 设计。

## Capabilities

### New Capabilities
- `taskflow-integration-test-coverage`: Taskflow+TopoScheduler.execute_parallel 集成测试契约,覆盖依赖边、错误传播、跨调用复用、worker 注入、混合节点类型、大 DAG 压测、fork-join 同步 7 个核心不变式。

### Modified Capabilities
- `dag-scheduler-pipeline`: 新增 `TopoScheduler::Config::num_workers` 字段,默认 0 (= `hardware_concurrency()`),允许测试和未来生产配置注入确切 worker 数。**不破坏** 现有 7 字段 DagState 契约。

## Impact

- **生产代码**:
  - `src/modules/scheduler/topo_scheduler.h:30-50` (Config 结构体追加 1 字段)
  - `src/modules/scheduler/topo_scheduler.cpp:247-248` (读 Config 替代硬编码)
- **测试代码**:
  - `tests/test_execute_parallel.cpp` (3 → 8 case, +5)
  - `tests/test_execute_parallel_advanced.cpp` (新建, 7 case)
- **API 兼容性**:
  - ✅ 公开 API 零修改 (`execute_parallel / execute_dag_loop` 签名不变)
  - ✅ 序列化/反序列化零影响 (Config 字段仅追加,有序)
  - ✅ 默认构造零行为变化 (`num_workers == 0` 路径与现状完全一致)
  - ✅ 现有 49/49 ctest 零回归
- **依赖**:
  - ✅ 无新依赖 (复用现有 tf::Taskflow + Catch2)
- **文档**:
  - `tests/AGENTS.md` 自动同步 (CMakeLists.txt 用 `file(GLOB)` 自动收录新测试文件)
  - `proposal-suggestions.md` 状态行从 "已批准" 标记为 "已创建 change"
- **风险**:
  - 低 - 仅追加字段 + 新增测试,无生产代码逻辑修改
  - 唯一改动是 `topo_scheduler.cpp:247-248` 的 1 行硬编码 → Config 读
