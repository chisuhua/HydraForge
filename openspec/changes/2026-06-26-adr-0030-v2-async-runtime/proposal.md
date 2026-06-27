# Proposal: ADR-0030 V2 — Async Runtime (Phase 2 入口, Sprint 12 主体)

> **状态**: 🟡 active (Oracle 咨询已完成 2026-06-27, 占位内容已填充, 实施待启动)
> **Oracle 决议 session**: `ses_0f5541ebfffehKDxNVuYqB7bq4`
> **关联 ADR**: `docs/adr/adr-0030-async-runtime-v2.md` (C0 收官后新建, V2 草案, 🔍 Proposed)
> **追溯范围**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C2

---

## Why

ADR-0030 V1 (`docs/archive/adr/adr-0030-async-runtime-dual-layer.md`) 标注 ❌ Not Implemented 归档原因 = "Taskflow + async_simple 依赖未引入"。但 Slice 00 (`docs/implementation-roadmap.md` §Slice 00) 已 100% 完成, 依赖实际已引入 (2026-06-07 ship)。Phase 2 (异步架构) 是 `docs/implementation-roadmap.md` 定义的下一个里程碑, C0 已写 V2 草案, C1 (前置依赖) 已 ship (2026-06-27), Sprint 12 立即可启动 C2 实施。

## Oracle 决议 (2026-06-27, session `ses_0f5541ebfffehKDxNVuYqB7bq4`)

### Open Question 1: Fleet 模式 16 路 LLM 并行 → ✅ **DEFER**

**Oracle 建议**: Option B (defer to Phase 3+)

**理由**:
- 6 个 examples 中 **0 个** 使用并行 LLM 调用 (`agent_basic` 串行 `engine->run()`, `agent_simple`/`agent_loop`/`phase1_plugin_demo` 全部单次 `generate()`)
- **DomainWorkerPool 已提供 N-worker 并发派发能力** (默认 4, 可配置 16), 所谓"16 路 LLM 并行"能力**已存在于 DomainWorkerPool** —— 缺的只是一个 LLM 专用薄 wrapper (分片→submit→聚合)
- ADR-0030 V2 §Open Questions OQ2 自带审查标准 "如需结果流式聚合/部分失败恢复才需独立 FleetOrchestrator" —— 当前无任何业务需求触发此标准
- P1 (Taskflow DAG 并行化) 才是 Phase 2 真正价值所在, FleetOrchestrator 是 ~1 周的投机性工作

**Defer 范围**:
- `src/common/llm/fleet_orchestrator.{h,cpp}` (新建 FleetOrchestrator)
- `examples/slice_04_fleet/main.cpp` (16 路 LLM mock 端到端)
- `tests/test_fleet_orchestrator.cpp` (并行调用 + TSan)

**保留能力**: `DomainWorkerPool(16)` + `register_domain_handler("llm", ...)` 今日即可实现 16 路, 无新组件需求

**触发条件 (Phase 3+)**: ensemble inference / multi-model voting / batch eval 等真实用例出现时重启 FleetOrchestrator 设计

### Open Question 2: LLM Token 流式推送 → ✅ **Bridge Runner (`run_stream_to_bus`)**

**Oracle 建议**: Option B (factory/bridge function), 重命名为 "runner" 非 "callback" (因 IGenerationStream 是 pull-based)

**理由**:
- **IGenerationStream 是 pull-based 迭代器** (`llm_types.h:53-61` 接口 `next(stop_token) → optional<string>` + `is_active()`), 调用方主动拉取
- 保持 **LLM provider 纯净**: LlamaAdapter/CloudAdapter/HttpAdapter/MockProvider 四个 provider 无需依赖 IInteractionBus, 分层架构 (`common/llm` 低于 `contract/`) 得以保留
- 一个 bridge function 可跨所有 provider 复用 (它们都实现 IGenerationStream)
- 可用 `MockProvider + InMemoryBus` 单测, 无需触及真实 LLM 代码
- 当前无 TUI 消费者订阅 `llm.token` 事件 (grep IInteractionBus 在 `examples/` 零命中, 无 `examples/agent_chat/`)
- Bridge 是纯生产者 —— 未来 TUI 通过 `bus.subscribe("llm.token", ...)` 消费, 解耦彻底

**API 草图**:
```cpp
// src/common/llm/stream_to_bus.h (新增, ~40 行)
namespace agenticdsl {

/// Bridge runner: 将 pull-based IGenerationStream 转为 IInteractionBus 事件流
/// 行为:
///   1. 循环调用 stream.next(token) 拉取 token
///   2. 每个 token 通过 bus.emit("llm.token", ToolResult::success({{"token", *tok}, {"request_id", rid}}))
///   3. 流结束后 emit "llm.token.done" (含 finish_reason + token 计数)
///   4. 出错 emit "llm.token.error" (LLMError payload)
/// 返回: 聚合后的完整 GenerationResult (text + token 计数)
GenerationResult run_stream_to_bus(
    IGenerationStream& stream,
    IInteractionBus& bus,
    std::stop_token token,
    std::string_view request_id);

}
```

### Open Question 1 (C0 阶段锁定): 双层架构 → ✅ **std::jthread (Sprint 2/3 验证通过)**

- V2 ADR §决策 1 已锁定: Taskflow (DAG/compute) + std::jthread Worker Pool (control) + IInteractionBus (event transport)
- Sprint 2 CognitiveWorker (9/9 ctest) + Sprint 3 DomainWorkerPool (7/7 ctest, 1000x 并发 TSan clean) 已验证 std::jthread 足够
- **async_simple 依赖将在 P1 中移除** (`external/async_simple/` 当前已 ship 但未启用, CMake `add_subdirectory` 移除)

## What Changes (Sprint 12 实施范围, Oracle 决议后)

### P1: TopoScheduler Taskflow DAG 并行化 (Week 1)

1. **集成 Taskflow v4.0** (`external/taskflow/`, 已 ship)
   - 在 `src/modules/scheduler/topo_scheduler.cpp` 中引入 `tf::Executor`
   - 新增 `execute_parallel()` 方法, 与现有 `execute()` 并行 (API 兼容性)
   - DAG 节点并行派发 (无依赖关系的节点同时执行)
   - Fork/Join 改用 Subflow
   - **移除 `async_simple` 依赖**: `CMakeLists.txt` `add_subdirectory(external/async_simple)` 删除, `external/async_simple/` 目录标记 deprecated (保留 git 历史)

2. **Context fork/merge 不可变分支**
   - `src/core/types/context.h` 新增 `fork()` (深拷贝 Layer) + `merge()` (策略合并)
   - 替代当前"共享可变 Context, 按引用传递并就地修改"模式
   - 解决 ADR-0030 V2 §风险表 "共享可变 Context" 🔴 高风险

3. **`src/common/llm/stream_to_bus.{h,cpp}` + `tests/test_stream_to_bus.cpp`** (Oracle Q2 bridge runner, ~120 行)
   - 归入 P1 Week 1 (与 Taskflow 并行交付, bridge 不依赖 Taskflow)

### P2: IInteractionBus 后端切换 (Week 2-3)

1. **`src/common/contract/inmemory_bus.cpp` 后端切换为 EventBus (MPMC 有界队列)**
   - 解决 ADR-0030 V2 §风险 "bridge 背压" (当前 emit 同步通知所有 subscriber)
   - 缓解: EventBus MPMC 解耦 emit 与 subscriber 速度
   - InMemoryBus 公共 API 保持不变, 仅内部实现切换

2. **背压风险缓解**: bridge runner 在 P2 后端切换后无变更 (但行为更稳)

### ~~P3: Fleet 模式 16 路 LLM~~ → ❌ **DEFERRED** (Oracle Q1)

### ~~P4: 用户审批等待 (/apply)~~ → 暂不实现 (依赖 ADR-0031 完整接口, Sprint 13+ 才适用)

### ADR-0030 V2 文档漂移修正 (强制)

- §决策记录 line 292 "DomainWorkerPool 默认 16" → "默认 4, 可配置 16"
- §线程模型 line 226 "M (domain, 默认 16)" → "M (domain, 默认 4, 可配置 16)"

## Capabilities (基于 Oracle 决议)

### ADDED Requirements (spec.md 同步)

- `async-runtime-taskflow-dag-parallel`: DAG 节点 MUST 支持并行派发 (Taskflow executor)
- `async-runtime-context-fork-merge`: Context MUST 支持 fork() 深拷贝 + merge() 策略合并 (不可变分支)
- `async-runtime-stream-to-bus-runner`: IGenerationStream MUST 支持通过 bridge runner 转为 IInteractionBus 事件流
- `async-runtime-eventbus-backend`: IInteractionBus 后端 MUST 切换为 EventBus MPMC 有界队列 (P2)
- `async-runtime-no-async-simple`: `external/async_simple/` CMake 依赖 MUST 移除 (P1)

### REMOVED Requirements

- ~~`async-runtime-fleet-mode-16x`~~ (deferred to Phase 3+)
- ~~`async-runtime-streaming-yield` (协程 yield)~~ (V1 方案, V2 不采用)

## Impact (Sprint 12 实施文件清单)

**新增**:
- `src/common/llm/stream_to_bus.{h,cpp}` (~40 行 + ~80 行测试)
- `src/common/llm/CMakeLists.txt` (新增 stream_to_bus.cpp 到静态库)
- `tests/test_stream_to_bus.cpp` (4-6 测试用例)
- `tests/CMakeLists.txt` (GLOB 自动发现, 无需修改)

**修改**:
- `src/modules/scheduler/topo_scheduler.{h,cpp}` (Taskflow executor 集成)
- `src/core/types/context.{h,cpp}` (fork/merge 方法)
- `src/common/contract/inmemory_bus.cpp` (后端切换为 EventBus)
- `src/modules/scheduler/CMakeLists.txt` (Taskflow link)
- `CMakeLists.txt` (根, 移除 async_simple add_subdirectory)
- `docs/adr/adr-0030-async-runtime-v2.md` (修正文档漂移)

**API 稳定性**:
- `IScheduler` 接口保持向后兼容 (新增 `execute_parallel()` 默认实现调用 `execute()`)
- `IExecutionPolicy` 不在本 change 范围 (Sprint 13 C3 实施)
- LLM providers (LlamaAdapter/CloudAdapter/HttpAdapter/MockProvider) 无修改

## Non-goals

- **不重写** CognitiveWorker / DomainWorkerPool (Sprint 2/3 ship)
- **不实施 FleetOrchestrator** (defer 到 Phase 3+, Oracle Q1)
- **不实施 /apply 用户审批 suspend** (依赖 C3 ADR-0031 完整接口, Sprint 13+)
- **不修改 IInteractionBus 公共 API** (后端切换保持兼容)
- **不实施 context 压缩** (ADR-0007, 与本 change 并行不耦合)

## Estimated Effort

**Sprint 12 主体 (1.5-2 周)**, Oracle 校正后估时:

| 周 | Day | 内容 |
|---|------|------|
| 1 | Day 1-2 | P1 Taskflow 集成 + stream_to_bus bridge |
| 1 | Day 3 | P1 Context fork/merge + async_simple CMake 移除 |
| 1 | Day 4-5 | P1 测试 + TSAN 验证 |
| 2 | Day 6-8 | P2 InMemoryBus 后端切换为 EventBus MPMC |
| 2 | Day 9-10 | P2 测试 + 文档漂移修正 |
| 3 | Day 11-12 | Ship gate (47/47 + ASan/TSan) + ADR-0030 V2 → ✅ Approved |
| 3 | Day 13 | master plan §十一 C2 resolved + C3 启动前置 |

**总计**: ~10-12 工作日 (1.5-2 周), 较 ADR 0030 V2 §后续行动 估时 (3 周) 减少 1 周 (Fleet defer 节省)

## Sprint 11 收官前 Oracle 决议已全部应用

- ✅ OQ1: 双层架构 → std::jthread (C0 阶段锁定)
- ✅ OQ2: Fleet 16 路 → DEFER (C2 阶段 Oracle 决议)
- ✅ OQ3: Token 流推送 → bridge runner (C2 阶段 Oracle 决议)

**Sprint 12 启动无 Oracle 阻塞**, C2 可直接 `/opsx-apply 2026-06-26-adr-0030-v2-async-runtime`