# Tasks: CognitiveWorker (Sprint 2)

> **变更类型**: 真实实现 — 全部 task 标 [ ],需实际执行
> **关联 plan**: `.omo/plans/phase1-execution.md` §Sprint 2
> **关联 ADR**: docs/adr/adr-0020-thread-model-isolation.md §2.2.1
> **测试基线**: 29/29 (P1 ship 后, 含 test_provider_factory + test_tool_registry_interface)
> **预估工作量**: 2.5 天 (单人)

## 任务依赖图

```
T1 CognitiveWorker 实施 (1.5 天)
  ├── T1.1 include/agenticdsl/cognitive/cognitive_worker.h (新建, ~50 行)
  ├── T1.2 src/modules/cognitive/cognitive_worker.cpp (新建, ~80 行)
  ├── T1.3 src/modules/cognitive/CMakeLists.txt (添加 cognitive_worker.cpp)
  ↓
T2 测试 (0.5 天)
  ├── T2.1 tests/test_cognitive_worker.cpp (新建, ≥ 4 case)
  ├── T2.2 多线程 + bus 事件验证
  ↓
T3 验证 + 同步 (0.5 天)
  ├── T3.1 ctest 验证 (33/29 = 33 测试零回归)
  ├── T3.2 同步 ADR-0020 §2.2.1 + ADR-0019 状态
  ├── T3.3 同步 docs (roadmap-status / phase1-roadmap / AGENTS)
  ├── T3.4 openspec validate + Single commit
```

## Tasks

### T1. CognitiveWorker 实施 (1.5 天)

- **文件**:
  - `include/agenticdsl/cognitive/cognitive_worker.h` (新建)
  - `src/modules/cognitive/cognitive_worker.cpp` (新建)
  - `src/modules/cognitive/CMakeLists.txt` (添加 cognitive_worker.cpp)
- **粒度**: 1.5 天
- **验收**:
  - [ ] `CognitiveWorker` 类声明完整 (per-agent DSLEngine + IInteractionBus)
  - [ ] 构造接受 `unique_ptr<DSLEngine>` + `shared_ptr<IInteractionBus>` (ADR-0020 per-agent 隔离)
  - [ ] start/stop 生命周期正确 (std::thread + condition_variable, join 无 hang)
  - [ ] submit_task() 通过 condition_variable 唤醒 Worker
  - [ ] worker_loop 内部委托 SimpleCognitiveOrchestrator (P1.T2 接受 IToolRegistry*)
  - [ ] bus_->publish("task.<id>.started" / ".completed") (P1.1 IInteractionBus)
  - [ ] CMakeLists.txt 添加 cognitive_worker.cpp

### T2. test_cognitive_worker 测试 (0.5 天)

- **文件**: `tests/test_cognitive_worker.cpp` (新建)
- **粒度**: 0.5 天
- **验收**:
  - [ ] 测试 1: 基本启动/停止 (start 后 stop 立即返回, thread join OK)
  - [ ] 测试 2: 任务提交 + 同步结果 (submit_task → InMemoryBus 验证 task.completed 事件)
  - [ ] 测试 3: 优雅停止 (worker 阻塞中 → stop 设置 running_=false → join)
  - [ ] 测试 4: 错误传播 (LLM error 通过 ToolResult 传递, bus publish 包含 error_code)
  - [ ] GLOB 自动注册 (tests/CMakeLists.txt:55 file(GLOB SINGLE_TEST_SOURCES test_*.cpp))
  - [ ] 33+ 测试零回归 (29 现有 + 4 新增)

### T3. 验证 + 文档同步 (0.5 天)

- **粒度**: 0.5 天
- **验收**:
  - [ ] `cmake --build build && ctest --output-on-failure` ≥ 33 PASS
  - [ ] `tools/adr_lint.py docs/adr/` exit 0
  - [ ] `openspec validate 2026-06-23-cognitive-worker` exit 0
  - [ ] ADR-0020 §2.2.1 状态: 🟡 Partial → 部分解决 (CognitiveWorker ship 注释)
  - [ ] ADR-0019 IInteractionBus 状态: Sprint 2 CognitiveWorker 集成说明
  - [ ] docs/roadmap-status.md line 44: Sprint 2 0% → 100%
  - [ ] docs/phase1-roadmap.md §Sprint 2 状态更新
  - [ ] AGENTS.md NOTES: Sprint 2 ship 注释
  - [ ] Single commit `feat(cognitive): implement CognitiveWorker (Sprint 2)`

## 总工作量

~2.5 工作日 (单人)
