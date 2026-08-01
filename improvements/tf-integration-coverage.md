# tf-integration-coverage

**优先级**: P1 | **来源**: 评估报告 (2026-08-01, TopoScheduler + Taskflow 集成缺口审计, 用户验收)
**阶段**: default | **分类**: core-test
**类型**: debt

## 架构依据

- ADR-0030 V2 (`docs/adr/adr-0030-async-runtime-v2.md`, 2026-06-27 ship): `Taskflow + std::jthread + IInteractionBus` 双层架构, async_simple 已 deprecated, `external/async_simple/` 仅保留 git 历史
- Sprint 12 C2 Day 1-2 (`e2fb89b`): `TopoScheduler::execute_parallel` 引入 `tf::Executor` + `tf::Taskflow` PIMPL 持有, `parallel_executor_` / `parallel_taskflow_` 跨调用复用
- Sprint 18 D-4/D-5: `execute_parallel` 拆分为 `setup + execute_dag_loop` (≤50 行), 行为保持
- 49/49 ctest 零回归 ship gate, 但 `tests/test_execute_parallel.cpp` 3 case + `tests/test_async_bridge.cpp` 2 case 全部覆盖"无依赖 happy path", 核心契约未验证:
  - `topo_scheduler.cpp:282-288` 依赖边构建 `tf_tasks[path].succeed(tf_tasks[dep])` 从未被触发
  - `topo_scheduler.cpp:246-252` 跨调用复用 `parallel_executor_` / `parallel_taskflow_` 的路径未测
  - `topo_scheduler.cpp:268-271` 错误处理分支 `process_jump / return` 未被失败注入触发
  - `topo_scheduler.cpp:265-280` lambda 捕获 `state` / `locally_executed_mutex` 在 100+ 节点并发下的争用未压测
  - `topo_scheduler.cpp:264-294` 整段逻辑对 `ForkNode / JoinNode` / `LLMNode` 混排场景未覆盖
- 阻塞下游: 任何对 `execute_parallel` 路径的重构 (目前 Sprint 18-19 持续中) 都缺乏回归网

## 范围

- **In Scope**:
  - `TopoScheduler::Config` 新增 `size_t num_workers = 0` (0 = `hardware_concurrency()`, 默认构造行为完全保持)
  - `topo_scheduler.cpp:247-248` 从 Config 读 `num_workers` 替代硬编码
  - `tests/test_execute_parallel.cpp` 扩到 8 case (在原 3 case 基础上追加 5)
  - 新建 `tests/test_execute_parallel_advanced.cpp` 7 case (大 DAG / fork-join 混排 / 错误注入 / 取消路径)
  - `tests/test_async_bridge.cpp` 保持 2 case 现状 (已是 TF 库 smoke test, 无需扩充)
- **Out Scope**:
  - CancellationToken / Request API (超出"补测试缺口"范畴, 留独立提案)
  - 替换 `tf::Taskflow` 为其他库
  - `execute_parallel` 生产代码重构 (Sprint 18 D-4/D-5 已完成)
  - `parallel_executor_` / `parallel_taskflow_` PIMPL 持有的设计变更
  - `agenticdsl_common` 模块新增 tool helper (本次只读不改)

## 关键场景

- **依赖链派发**: GIVEN 5 节点 A→B→C→D→E 线性链, WHEN `execute_parallel`, THEN 按拓扑序完成, `state.executed_` 大小递增 =1 (E2E 验证 `topo_scheduler.cpp:285` 的 `succeed` 边)
- **多调用复用**: GIVEN `execute_parallel` 已跑过 3 节点, WHEN 同 scheduler 再次调用跑 5 节点, THEN `parallel_executor_` 仍持有 (由 `&parallel_executor_` 指针地址恒定断言)
- **失败注入传播**: GIVEN ToolCallNode 注册的工具抛 `runtime_error("boom")`, WHEN `execute_parallel` 派发, THEN `tf::Task` 异常被 `try/catch` 吞掉 (per Session 契约), `locally_executed` 不含该节点, `finalize_execution` 返回 `success=false`
- **混合节点类型**: GIVEN 6 节点 (3 ToolCall + 1 LLM mock + 1 Fork + 1 Join), WHEN `execute_parallel`, THEN 6 个全部完成, fork 分支由 join 同步
- **Worker 注入**: GIVEN `Config{num_workers=2}`, WHEN 4 个独立 ToolCallNode 跑, THEN `max_concurrent ≤ 2` (注入 worker 数验证, 不是黑盒)
- **大 DAG 规模**: GIVEN 100 节点 flat DAG + `num_workers=8`, WHEN `execute_parallel`, THEN 全部 100 节点完成, `elapsed < 5s` (`max_concurrent ≈ 8`, 无锁死锁, 无 lambda 捕获遗漏)
- **Fork/Join 并行**: GIVEN 1 ForkNode 分 4 支, 每支 1 个 ToolCallNode + 1 个 JoinNode, WHEN `execute_parallel`, THEN join 等待 4 支全完成后才执行 (关键不变式)
- **默认 worker 退化**: GIVEN `Config{}` (未设 num_workers), WHEN `execute_parallel`, THEN `tf::Executor` 线程数 = `hardware_concurrency()` (保证默认行为零变化)
- **空 DAG 边界**: GIVEN 0 节点 (已存在测试), WHEN `execute_parallel`, THEN `success=true` 无 tf::Task 创建

## 技术约束

- MUST 在 `TopoScheduler::Config` 新增 `size_t num_workers = 0` 字段, **仅追加**, 不修改既有任何字段顺序/类型 (保证序列化/配置加载零回归)
- MUST 保留默认构造行为: `num_workers == 0` → `std::max(1u, std::thread::hardware_concurrency())`, 与 `topo_scheduler.cpp:247-248` 现状完全一致
- MUST NOT 修改 `execute_parallel / execute_dag_loop` 公开签名
- MUST NOT 引入新依赖 (只用现有 tf::Taskflow + Catch2)
- MUST 失败注入测试用 try/catch 包裹 node 调用, 验证异常被吞路径 (非改 Session 行为)
- MUST 大 DAG 压测的 elapsed 是软约束, 非硬性 (<5s 在 CI 标准 4 核 + sanitizer 下放宽到 <10s)
- SHOULD 混合节点类型 case 复用 `tests/test_execute_parallel.cpp:23-28` 现有 lambda 风格, 不引入新模式
- SHOULD 新建文件 `test_execute_parallel_advanced.cpp` 而非扩充原文件 (原文件保持 ≤ 200 行约束, 符合 AGENTS.md 风格)
- SHOULD 沿用 Sprint 18 D-4/D-5 的 tag 格式 `[scheduler][c2-day4]` + 补充 `[c2-coverage]` 标识本提案

## 验收标准

- `TopoScheduler::Config::num_workers` 字段存在, 默认 0, `grep -n "num_workers" src/modules/scheduler/topo_scheduler.h` 返回 1 行定义 + 1 行 cpp 引用
- ctest 从 49/49 → **64/64** 零回归 (+15 case: `test_execute_parallel.cpp` 3→8, `test_execute_parallel_advanced.cpp` 0→7)
- TSan: 新增 100 节点 case 零 data race
- ASan: 新增大 DAG / fork-join 零 leak / use-after-free
- `tools/adr_lint.py` exit 0 (本提案不修改 ADR, 仅扩展测试)
- `tools/docs_drift_audit.py` 0 DRIFT
- 现有依赖测试通过率: 0/49 → 49/49 维持 (本提案不删任何旧 case)
- 失败注入 case 100% 触发 `process_jump` 或 `success=false` 路径 (grep `process_jump` 出现 ≥ 1 次)
