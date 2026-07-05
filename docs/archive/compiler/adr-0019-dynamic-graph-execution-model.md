# ADR-0019: 动态图执行模型

**状态**: 讨论中
**关联决策点**: D1 (Fork/Join 并发), D2 (动态图注入)

---

## 背景

SKILL Compiler 的自举闭环依赖两个运行时能力：

1. **Fork/Join 并发执行**——refs 懒加载需要并行读取多个参考文件，phase 分段执行需要独立上下文
2. **运行时图注入**——编译器 Phase 7 通过 `generate_subgraph` 生成新图后，必须能在同一轮对话内被调度器加载并跳转执行

当前代码库状态：

- **`execute_fork()`** (node_executor.cpp:241-246) — 抛出 `"ForkNode execution requires concurrent scheduler support, not implemented in NodeExecutor"`。实际顺序仿真在 `TopoScheduler::execute_fork_branches()` 中，通过快照恢复 + 顺序执行完成，**无真正并发**。
- **`append_dynamic_graphs()`** (topo_scheduler.cpp:442-447) — 将新图暂存到 `dynamic_graphs_`，但主执行循环 (`execute()`) **不会自动拾取**。节点需在循环下一次迭代时手动检查。
- **`append_graphs()`** (engine.cpp:145-149) — 仅追加到 `full_graphs_` vector，不触发重新调度。

---

## D1: Fork/Join 并发模型

### 问题

SKILL Compiler 的 refs 懒加载依赖 fork/join 并行读取多个 refs。当前顺序仿真导致：
- refs 加载变为串行，token 延迟叠加
- refs 全部就绪前无法进入下一 phase
- 无法利用并行 I/O 加速

### 选项

| 选项 | 描述 | 复杂度 | 风险 |
|------|------|--------|------|
| **A) std::thread + mutex** | 每个 fork 分支在新线程执行，共享 Context 加锁 | 中 | 数据竞争、死锁、Context 深拷贝开销 |
| **B) 协程/纤程 (stackful)** | 协作式调度，用户态切换，共享同一地址空间 | 高 | C++20 协程不直接支持 stackful；需 libco/boost.context |
| **C) 事件驱动单线程 (异步回调)** | fork 分支注册为待完成任务，主循环轮询完成状态 | 低 | 与 Kahn 调度器模型天然契合；单线程无需加锁 |
| **D) 继续顺序仿真 + 上下文隔离** | 保持现有方式，但在 fork 点做 context snapshot + 顺序执行分支 | 最低 | 最安全但无并行加速 |

### 决定（2026-05-25）

**决策**: Option C + jthread。ForkNode→JoinNode 边界内，每个分支在独立 jthread 中执行。保留后续升级为 boost.fiber 的架构可能性。

**选择依据**:
- 现有 `execute_fork_branches()` 的顺序 `while` 循环可直接替换为 `std::jthread` 并行启动
- 每个分支已获取独立的 context snapshot（`Context branch_initial_ctx = *fork_snapshot`）
- 每个分支运行独立的迷你调度器（`execute_single_branch()` 使用局部 `in_degree_`/`reverse_edges_`）
- jthread（C++20）零外部依赖
- 不影响动态图注入（dynamic_graphs_ 在分支完成后、主循环继续前处理）

**约束**:
- 每个分支需要自己的 `ExecutionSession` 拷贝（原 `ExecutionSession` 非线程安全）
- 预算管理：每个分支获得预算切片，join 时汇总
- Trace 收集：join 时拼接分支 trace
- 异常处理：任一分支的异常应终止所有分支
- `execute_fork()` 和 `execute_join()` 从抛异常改为正常执行

---

## D2: 动态图注入机制

### 问题

编译器 Phase 7 (`generate_subgraph`) 生成新 DSL 后，执行器必须：
1. 解析生成的 DSL 文本为新 `ParsedGraph`
2. 将新图注册到当前调度器
3. 确保新图的所有节点 next 指针有效
4. 跳转到新图入口开始执行

当前 `generate_subgraph` (node_executor.cpp:267-339) 能解析 DSL 并调用回调，但回调 (`AppendGraphsCallback`) 最终到达 `TopoScheduler::append_dynamic_graphs()`，只是暂存到 vector 中，主调度循环不会自动拾取。

### 选项

| 选项 | 描述 | 复杂度 | 风险 |
|------|------|--------|------|
| **A) 新执行周期** | `execute()` 可重入——自举完成后返回，调用方检查动态图队列，重新调用 `execute()` | 低 | 丢失当前执行状态；重新调度开销 |
| **B) 同进程双调度器** | 主调度器处理当前图，子调度器处理动态注入图；子调度器完成后结果合并回主上下文 | 中 | 上下文穿透复杂 |
| **C) 内联执行** | `generate_subgraph` 节点内直接执行生成的 DSL（递归调用 execute），注入的图作为子图原地执行 | 中 | 调用栈深度不可控；预算控制复杂 |
| **D) 调度器重建** | `append_dynamic_graphs` 触发调度器重建 DAG（重新拓扑排序），主循环继续 | 中 | 破坏当前执行状态；入度/就绪队列需重建 |
| **E) 混合：generate_subgraph 返回 + 调度器在循环头部检查 dynamic_graphs_** | 当前近似实现，需修复：主循环每次迭代开始时检查 `dynamic_graphs_` 是否非空，如非空则执行注册+重建 | 低 | 最增量地修复现有实现；不影响其他功能 |

### 决定（2026-05-25）

**决策**: Option A — 加固现有机制。保持单执行周期模型，修复当前 DAG 重建的安全性问题，后续优化为增量注册。

**两步路径**:

**Step 1（修复 — 最小安全变更）**:
- 修复 `build_dag()` 后 `in_degree_[next_path]` 默认构造问题：动态节点注册后需预填其 in_degree
- 允许 `build_dag()` 中跳过不存在的 next 目标，标记为"待解析动态依赖"（pending dynamic dep）
- DAG 重建后重新解析当前节点的 next 指针
- Fork/Join 进行中的动态图延迟到分支完成后处理
- 与 `check_and_requeue_dynamic_deps()` 机制集成

**Step 2（优化 — 增量注册）**:
- 替换全量克隆 + `build_dag()` 为增量 `register_node()` + 局部 in_degree 更新
- 只在当前节点的 next 指向动态路径时才触发重建
- 当前 Step 1 优先，Step 2 延迟到性能瓶颈出现时

**选择依据**:
- 现有机制已布线（行 363-404），`next: "/dynamic/compiled/xxx/start"` 模式在 DAG 重建后自然生效
- 自举闭环需要编译产物在同一执行周期内立即执行
- 状态连续（Context、Registry、Traces 无缝流转）
- 多执行周期方案（Option B）破坏了 `next` 指针语义

**约束**:
- Fork 分支内不允许 generate_subgraph 注入新图
- 动态节点路径必须使用 `/dynamic/` 命名空间前缀
- 全量重建的性能问题在 Step 2 解决，Step 1 优先保证正确性

---

## 决策

（讨论后填写）

---

## 影响

（讨论后填写）
