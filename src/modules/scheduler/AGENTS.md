# scheduler 模块

**Generated:** 2026-05-11

## OVERVIEW
DAG 拓扑调度器，管理节点执行顺序、资源分配和执行会话。

## WHERE TO LOOK
| Task | Location |
|------|----------|
| DAG 构建 | `topo_scheduler.cpp` - `build_dag()` |
| 节点调度 | `topo_scheduler.cpp` - `schedule()` |
| 执行会话 | `execution_session.cpp` - `ExecutionSession` |
| 资源管理 | `resource_manager.cpp` - `ResourceManager` |
| 回调注册 | `topo_scheduler.cpp` - `append_dynamic_graphs()` |

## KEY TYPES
- `TopoScheduler::Config` - 调度器配置（含初始预算）
- `ExecutionSession` - 单次执行会话，管理上下文和回调
- `HardEndException` - 硬终点异常，用于分支终止

## ANTI-PATTERNS
- 不要直接创建 `Node*`，通过 `register_node()` 托管生命周期