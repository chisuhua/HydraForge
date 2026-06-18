# Proposal: CognitiveWorker (Sprint 2)

> **变更类型**: 真实实现 (新功能)
> **作者**: Sisyphus (Phase 1 P1 后续 + Sprint 2 启动)
> **创建日期**: 2026-06-16 (placeholder) → 2026-06-18 (filled)
> **追溯范围**: `.omo/plans/phase1-execution.md` §Sprint 2
> **关联 ADR**: docs/adr/adr-0020-thread-model-isolation.md (P1) + ADR-0019 IInteractionBus
> **前置**: Sprint 1a (ToolResult P2-P4, archived 2026-06-16) + Sprint 1b (Bus 集成, archived 2026-06-17) + P1 解耦 (T1+T2+T3+T4 全部 ship, 2026-06-18, 20 commits)
> **amends**: ADR-0020 §2.2.1 (CognitiveWorker 集成 P1 抽象)

## Why

Sprint 1a (ToolResult) + Sprint 1b (IInteractionBus 集成) 已 ship. P1 解耦（17 commits, 2026-06-18）完成 engine.h 跨模块 include 全部移除（4→1, 仅 common/llm/llm_types.h types 例外），并提供：
- `IProviderFactory` 抽象 (T1) — DSLEngine 默认 LLM 通过 factory 注入
- `IToolRegistry` 抽象 (T2) — 工具注册通过接口多态分派
- `SimpleCognitiveOrchestrator` 改为 `IToolRegistry*` (T2.4) — Worker 持有抽象, 不依赖具体类型

CognitiveWorker 是 Phase 1 智能体层 (Sprint 2) 的核心抽象, 实现 ADR-0020 §2.2 "每 CognitiveWorker 拥有独立 DSLEngine 实例" 的 per-agent 隔离模型, 并通过 IInteractionBus 与主线程通信. 现有 `SimpleCognitiveOrchestrator` (C1 B-stage 实施的单轮 ReAct) 作为 MVP 内部实现, Sprint 2 升级为 CognitiveWorker 完整包装.

不解决此问题: (a) CognitiveWorker 之间负载均衡/路由 — Phase 1 后续 sprint; (b) 跨 Worker 状态共享 — ADR-0014 多轮对话; (c) DomainWorkerPool 实际并行执行 — Sprint 3; (d) 与 `phase1_model_router_plugin` 整合 — 后续.

## What Changes

### 决策 1: CognitiveWorker 作为 DSLEngine 的薄包装

```cpp
// include/agenticdsl/cognitive/cognitive_worker.h
class CognitiveWorker {
 public:
  // 构造: 接受独立 DSLEngine + IInteractionBus (per-agent 隔离)
  explicit CognitiveWorker(
      std::unique_ptr<DSLEngine> engine,
      std::shared_ptr<IInteractionBus> bus);

  ~CognitiveWorker();

  // 启动 Worker 线程 (单线程, ADR-0020 §2.2 per-agent 隔离)
  void start();

  // 提交一个任务 (异步, 通过 bus 通信)
  void submit_task(const std::string& task_id, const std::string& prompt);

  // 停止 Worker (清理 thread, 取消未完成 task)
  void stop();

 private:
  std::unique_ptr<DSLEngine> engine_;        // Worker 拥有独立 DSLEngine 实例
  std::shared_ptr<IInteractionBus> bus_;
  std::thread worker_thread_;
  std::atomic<bool> running_{false};
  std::queue<std::pair<std::string, std::string>> task_queue_;  // (task_id, prompt)
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
};
```

### 决策 2: SimpleCognitiveOrchestrator 作为内部实现 (P1 抽象集成)

CognitiveWorker 内部持有 `SimpleCognitiveOrchestrator` 实例 (依赖 P1.T2 已改 `IToolRegistry*`):

```cpp
void CognitiveWorker::worker_loop() {
  while (running_) {
    auto [task_id, prompt] = wait_for_task();
    
    // 通过 bus 推送 "task started" 事件 (P1.1 IInteractionBus)
    bus_->publish("task." + task_id + ".started", {});
    
    // 委托给内部 SimpleCognitiveOrchestrator (P1.T2 接受 IToolRegistry*)
    SimpleCognitiveOrchestrator orch(engine_->get_tool_registry(), 
                                      engine_->get_llm_provider());
    
    // 同步执行单轮 ReAct (MVP)
    ToolResult result;
    orch.process(task_id, [&result](ToolResult r) { result = std::move(r); });
    
    // 通过 bus 推送 "task completed" 事件
    bus_->publish("task." + task_id + ".completed", result.to_json());
  }
}
```

### 决策 3: P1 抽象集成 (engine.h 解耦后)

CognitiveWorker 通过 DSLEngine 公开 API 操作, 完整利用 P1 解耦:
- `engine_->get_tool_registry()` 返回 `IToolRegistry&` (P1.T4) — 注入 SimpleCognitiveOrchestrator
- `engine_->get_llm_provider()` 返回 `ILLMProvider*` (C1.4) — 注入 SimpleCognitiveOrchestrator
- `engine_->set_interaction_bus(bus_)` (Sprint 1b) — 转发所有 bus 事件
- `engine_->subscribe(topic, callback)` (Sprint 1b) — 注册 Worker 自身事件回调

### 决策 4: 单线程模型 + 任务队列 (MVP)

ADR-0020 §2.2.1: "每 CognitiveWorker 拥有独立 DSLEngine 实例". Sprint 2 MVP 采用单线程 + 任务队列 (后续 Sprint 3 升级为多线程):
- Worker 启动一个 `std::thread`, 在 `worker_loop` 中阻塞等待任务
- `submit_task()` 通过 `std::condition_variable` 唤醒 Worker
- `stop()` 设置 `running_ = false` 并 join thread
- 任务 FIFO 顺序处理 (单线程保证)

后续 Sprint 3 DomainWorkerPool 升级: 多线程 Worker 池 + 任务调度 (Phase 1 Sprint 3 范围).

### 代码侧 (新代码)

- `include/agenticdsl/cognitive/cognitive_worker.h` (新建, ~50 行)
  - `CognitiveWorker` 类声明 (per-agent DSLEngine + IInteractionBus)
- `src/modules/cognitive/cognitive_worker.cpp` (新建, ~80 行)
  - 构造 + start/stop + worker_loop + submit_task
- `tests/test_cognitive_worker.cpp` (新建, ≥ 4 case)
  - 基本启动/停止
  - 任务提交 + 同步结果 (通过 InMemoryBus 验证事件)
  - 优雅停止 (join thread, 取消未完成 task)
  - 错误传播 (LLM 错误通过 ToolResult 传递)
- 内部依赖:
  - `SimpleCognitiveOrchestrator` (P1.T2 已 ship, 接受 IToolRegistry*)
  - `IInteractionBus` (Sprint 1b 已 ship, InMemoryBus 实现)
  - `DSLEngine` (P1 已 ship, PIMPL-lite 化)
  - `ToolResult` (Sprint 1a 已 ship, P2-P4 完成)

### 文档侧

- 更新 `docs/adr/adr-0020-thread-model-isolation.md` §2.2.1: CognitiveWorker 实施状态 (Sprint 2 ship 后, 状态从 🟡 Partial → 部分解决)
- 更新 `docs/adr/adr-0019-iinteraction-bus-mvp.md`: Sprint 1b IInteractionBus 实施 (Sprint 2 CognitiveWorker 集成)
- 更新 `docs/roadmap-status.md` line 44: Sprint 2 状态 (0% → 进行中 → 完成)
- 更新 `docs/phase1-roadmap.md` §Sprint 2 详细任务
- 更新 `AGENTS.md` NOTES: CognitiveWorker ship 注释
- 不更新 `openspec/specs/tech-debt-cleanup/spec.md` (本 change 不修改 §1.4 退出标准)

## Impact

- **Affected specs**: 无 (CognitiveWorker 是新功能, 不修改既有 spec)
- **Affected ADRs**: 
  - `adr-0020-thread-model-isolation.md` §2.2.1 (CognitiveWorker 实施状态)
  - `adr-0019-iinteraction-bus-mvp.md` (Sprint 2 集成说明)
- **Affected code**:
  - `include/agenticdsl/cognitive/cognitive_worker.h` (新建)
  - `src/modules/cognitive/cognitive_worker.cpp` (新建)
  - `src/modules/cognitive/CMakeLists.txt` (添加 cognitive_worker.cpp)
  - `tests/CMakeLists.txt` (无需改, GLOB 自动注册 test_cognitive_worker.cpp)
- **Affected tests**: 现有 29 测试零回归 (Sprint 2 是新功能, 不修改现有代码) + 新增 4 测试
- **Breaking change**: 无 (纯新增类, 不改任何已有 API)

## Success Criteria

- [ ] `CognitiveWorker` 类编译通过 (P1 解耦后无 include 警告)
- [ ] `CognitiveWorker` start/stop 生命周期正确 (std::thread join 无 hang)
- [ ] 4+ test_cognitive_worker 测试通过 (含多线程 + bus 事件验证)
- [ ] 现有 29/29 ctest 零回归
- [ ] `tools/adr_lint.py docs/adr/` exit 0 (ADR-0020 + ADR-0019 状态更新)
- [ ] `openspec validate 2026-06-23-cognitive-worker` exit 0
- [ ] ADR-0020 §2.2.1 状态: 🟡 Partial → 部分解决
- [ ] Single commit `feat(cognitive): implement CognitiveWorker (Sprint 2)`

## Out of Scope (Non-goals)

- ❌ 不实现 DomainWorkerPool 多线程并行 (Sprint 3)
- ❌ 不实现 CognitiveWorker 路由/负载均衡 (Phase 1 后续)
- ❌ 不实现与 `phase1_model_router_plugin` 整合 (后续)
- ❌ 不修改 SimpleCognitiveOrchestrator (P1.T2 已 ship, 不再改)
- ❌ 不实现跨 Worker 状态共享 (ADR-0014 多轮对话范围)

## Dependencies

- **Block**: Sprint 1a (✅ shipped 2026-06-16) + Sprint 1b (✅ shipped 2026-06-17) + P1 解耦 (✅ shipped 2026-06-18)
- **Block by**: 无
- **Related**: `2026-06-30-domain-worker-pool` (Sprint 3, CognitiveWorker 是 DomainWorkerPool 的客户端)

## Estimated Effort

~2.5 天 (单人):
- T1 CognitiveWorker 头 + 实现: 1.5 天
- T2 test_cognitive_worker (4 case): 0.5 天
- T3 文档同步 + ADR 更新 + openspec validate: 0.5 天
