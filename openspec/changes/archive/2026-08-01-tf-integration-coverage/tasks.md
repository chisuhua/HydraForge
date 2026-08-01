## 1. Config 字段扩展

- [ ] 1.1 在 `src/modules/scheduler/topo_scheduler.h` `Config` 结构体追加 `size_t num_workers = 0;` 字段
- [ ] 1.2 修改 `src/modules/scheduler/topo_scheduler.cpp:247-248`,从 `Config.num_workers` 读 worker 数,0 时退化到 `max(1u, hardware_concurrency())`
- [ ] 1.3 验证 `Config{}` 默认构造路径字节级与现状一致 (运行 ctest 49/49 应保持)

## 2. 基础测试扩充 (test_execute_parallel.cpp)

- [ ] 2.1 新增 case: 依赖链派发 - 5 节点 A→B→C→D→E 线性链,验证 `tf_tasks[path].succeed(tf_tasks[dep])` 边生效
- [ ] 2.2 新增 case: 多调用复用 - 同 scheduler 跑 3 节点 + 5 节点两次,验证 `parallel_executor_` 指针地址恒定
- [ ] 2.3 新增 case: 失败注入传播 - ToolCallNode 注册工具抛 `runtime_error("boom")`,验证 `locally_executed` 不含该节点且 `success=false`
- [ ] 2.4 新增 case: 混合节点类型 - 6 节点 (3 ToolCall + 1 LLM mock + 1 Fork + 1 Join),验证 6 个全部完成
- [ ] 2.5 新增 case: Worker 注入 - `Config{num_workers=2}` + 4 独立节点,验证 `max_concurrent ≤ 2`

## 3. 高级测试 (新建 test_execute_parallel_advanced.cpp)

- [ ] 3.1 新建文件 `tests/test_execute_parallel_advanced.cpp` 包含 test_helpers 注册
- [ ] 3.2 case: 大 DAG 规模 - 100 节点 flat DAG + `num_workers=8`,断言 100 节点完成 + `elapsed < 5s` (CI +sanitizer 时放宽到 <10s)
- [ ] 3.3 case: Fork/Join 并行 - 1 ForkNode 分 4 支,每支 1 ToolCallNode + 1 JoinNode,验证 join 等待 4 支完成
- [ ] 3.4 case: 默认 worker 退化 - `Config{}` (未设 num_workers),验证 `tf::Executor` 线程数 = `hardware_concurrency()`
- [ ] 3.5 case: 边界 0 节点 - 空 DAG,验证 `success=true` 无 tf::Task 创建 (与 test_execute_parallel.cpp 现有 case 共享)
- [ ] 3.6 case: 边界 1 节点 - 1 节点 DAG,验证单节点路径
- [ ] 3.7 case: cancellation-via-destruction - scheduler 析构时,验证 `~tf::Executor()` join 行为无死锁 (析构前确保 inflight tasks 完成)

## 4. CMake 与 AGENTS.md 同步

- [ ] 4.1 验证 `tests/CMakeLists.txt` 的 `file(GLOB test_*.cpp)` 自动收录新文件
- [ ] 4.2 运行 `cmake --preset tests` 重新配置,确认新测试出现在 ctest 列表
- [ ] 4.3 检查 `tests/AGENTS.md` 的 15 个测试文件列表是否需要更新 (新文件 = 16 个)

## 5. 验证 (TDD 5 步)

- [ ] 5.1 运行 ctest 验证 49/49 → 64/64 零回归 (`cmake --build build && ctest --output-on-failure`)
- [ ] 5.2 运行 TSan 验证大 DAG / fork-join case 零 data race (`cmake --preset tsan && ctest`)
- [ ] 5.3 运行 ASan 验证零 leak / use-after-free (`cmake --preset asan && ctest`)
- [ ] 5.4 运行 `tools/adr_lint.py` exit 0 (本 change 不修改 ADR)
- [ ] 5.5 运行 `tools/docs_drift_audit.py` 0 DRIFT
- [ ] 5.6 运行 `openspec validate tf-integration-coverage --json` 验证 change artifacts 通过
- [ ] 5.7 运行失败注入 case 时 `grep "process_jump\|success=false"` 出现 ≥ 1 次,确认错误路径被触发

## 6. 收尾

- [ ] 6.1 更新 `proposal-suggestions.md` 末行状态: `已批准 → proposal-approved.md` → `已创建 change → openspec/changes/tf-integration-coverage/`
- [ ] 6.2 git add 并 commit 本 change 所有 artifacts (proposal.md, design.md, tasks.md, .openspec.yaml)
- [ ] 6.3 验证 `tests/AGENTS.md` 同步 (CMake 自动 + 文档手动)
- [ ] 6.4 准备 plan-done handoff 写 `.rddf/state/.plan-handoff.json`
