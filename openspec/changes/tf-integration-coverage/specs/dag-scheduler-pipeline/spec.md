# dag-scheduler-pipeline Delta

## ADDED Requirements

### Requirement: Config::num_workers 字段

`TopoScheduler::Config` MUST 新增 `size_t num_workers = 0;` 字段,默认 `0` 退化到 `max(1u, std::thread::hardware_concurrency())`,非 0 时作为 `tf::Executor` 确切线程数。**字段追加不修改** 现有 Config 其他字段,序列化/反序列化零影响,默认构造行为零变化。

#### Scenario: 字段定义与默认行为
- **WHEN** `TopoScheduler::Config` 构造
- **THEN** `num_workers == 0` (默认值)
- **AND** `topo_scheduler.cpp:247-248` 退化逻辑使用 `Config.num_workers` 替代硬编码
- **AND** `num_workers == 0` 路径与现状字节级一致
- **AND** 49/49 ctest 零回归
