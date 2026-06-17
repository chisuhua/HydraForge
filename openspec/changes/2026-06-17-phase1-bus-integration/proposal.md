# Phase 1 Sprint 1b: DSLEngine Bus 集成 (ADR-0019 P2)

> **变更类型**: 实施变更 (Sprint 1b 启动)
> **关联 plan**: `.omo/plans/phase1-execution.md` §Sprint 1b
> **关联 ADR**: [ADR-0019: IInteractionBus 接口与 TUI Chat MVP](../../docs/adr/adr-0019-iinteraction-bus-mvp.md) §1.4 + K2 决策 (S1a/S1b 拆分)
> **依赖**: Sprint 1a 完成 (ToolResult P1-P4, commits `fb67a9b`/`60b31b5`/`5c9ba18` 2026-06-16) + P1.T4 (engine.h 移除 3 include)
> **创建日期**: 2026-06-16
> **状态**: 🎯 准备就绪 (Ready)

## Why

Phase 0 X 阶段已交付 `IInteractionBus` + `InMemoryBus` (18/18 多线程测试通过),但 DSLEngine 和 NodeExecutor **尚未注入 bus 实例**,导致:

1. **CognitiveWorker 无法订阅 LLM 实时事件**: 决策延迟、error_code 重试策略、Abort 异常传播均依赖 bus 推送
2. **Plugin/Sprint 5 缺乏统一事件通道**: Plugin 加载完成、ToolResult 推送、错误传播无统一入口
3. **ADR-0019 P2 任务长期挂起**: `docs/phase1-roadmap.md` T1.6-T1.9 自 Phase 0 收官即标注 P0 优先级,但 0% 完成
4. **PDK 实施前置**: ADR-0021 PDK 依赖 `IInteractionBus` 作为插件事件流入口 (Sprint 4)
5. **TSan 真实并发验证缺位**: 1000x 并发 emit 测试在 ADR-0019 已有,但 DSLEngine 端无消费侧验证

Sprint 1a 已完成 ToolResult P1-P4 标准化,提供了 bus 推送所需的结构化 payload (Sprint 1a T1.4 完成 `IInteractionBus::emit(std::string)` 重载)。**Sprint 1b 立即接力,完成 bus 在 DSLEngine + NodeExecutor 的双向集成。**

## What Changes

### 1. DSLEngine 集成 IInteractionBus (P2.1-P2.2)

- `src/core/engine.h` 移除 3 个 common/ include 之一 (P1.T4 遗留: `common/tools/registry.h` 改为前向声明 + PIMPL-lite)
- `src/core/engine.h` 新增 3 个方法:
  - `void set_interaction_bus(std::shared_ptr<IInteractionBus> bus)` — 注入 bus
  - `std::shared_ptr<IInteractionBus> get_interaction_bus() const` — 访问
  - `size_t subscribe(const std::string& topic, std::function<void(const ToolResult&)> cb)` — 透传到 bus
- `src/core/engine.cpp` 实现上述方法 (委托 `bus_->emit` / `bus_->subscribe` / `bus_->unsubscribe`)
- 默认 bus: `InMemoryBus` 单例 (per-engine-instance,非全局单例)

### 2. NodeExecutor 集成 IInteractionBus (P2.3-P2.4)

- `src/modules/executor/node_executor.h` 构造函数新增 `IInteractionBus* bus = nullptr` 参数 (可选注入, nullptr 兼容现有 25+ 测试)
- `src/modules/executor/node_executor.cpp::execute_dsl_node` 在 LLM 调用前推送 `dsl_call_started` 事件,生成期间逐 token 推送 `llm_token` 事件,完成后推送 `dsl_call_completed` 事件
- `execute_tool_call` 完成后推送 `tool_call_completed` 事件 (ToolResult envelope)
- 异常路径: `error_code == Abort` / `Retry` 等推送 `execution_failed` 事件后 throw

### 3. 端到端 Bus 集成测试 (S1b.T4)

- 新建 `tests/test_engine_bus_integration.cpp`:
  - 5+ 集成测试:
    - DSLEngine 注入 custom bus + subscribe → emit 验证
    - DSLNode execute_dsl_node 期间 bus 收到 1 started + N token + 1 completed
    - ToolNode execute_tool_call 完成后 bus 收到 envelope (含 error_code/latency_ms/trace_id)
    - Abort 错误码触发 execution_failed 事件 + 抛出异常
    - 默认 InMemoryBus 1000x 并发 subscribe + emit 无死锁 (继承 Sprint 1a Test 1)
- 注册到 `tests/CMakeLists.txt` (自动 glob)

### 4. 文档同步 (与 Sprint 1a 模式一致)

- 更新 `docs/roadmap-status.md` Sprint 1b 状态 (完成后)
- 更新 `docs/phase1-roadmap.md` Sprint 1b 任务勾选
- 更新 ADR-0019 §实施状态 (P2 子任务完成)

## Impact

### Affected specs
- `docs/specs/architecture.md` (新增 DSLEngine ↔ IInteractionBus 章节)
- `docs/adr/adr-0019-iinteraction-bus-mvp.md` (P2 状态从 🟡 Partial → ✅ Approved)

### Affected code
| 文件 | 变更 |
|------|------|
| `src/core/engine.h` | 前向声明 ToolRegistry,移除 1 include,新增 3 方法 |
| `src/core/engine.cpp` | 实现 bus 注入/访问/订阅 |
| `src/modules/executor/node_executor.h` | 构造函数新增 bus 参数 |
| `src/modules/executor/node_executor.cpp` | execute_dsl_node + execute_tool_call 推送事件 |
| `src/core/engine.cpp` (DSLEngine ctor) | 默认创建 InMemoryBus |
| `tests/test_engine_bus_integration.cpp` | 新建, 5+ 测试 |
| `docs/adr/adr-0019-iinteraction-bus-mvp.md` | P2 状态更新 |
| `docs/roadmap-status.md` | Sprint 1b 状态更新 |
| `docs/phase1-roadmap.md` | Sprint 1b 任务勾选 |

### Affected behavior
- DSLEngine 现有 `run()` / `from_markdown()` API **不变** (bus 注入是可选)
- NodeExecutor 现有 7 个 execute_xxx 方法签名 **不变** (bus 注入是构造函数可选)
- IInteractionBus 现有 `emit(const std::string&, const ToolResult&)` **不变** (Sprint 1a T3 完成的 `std::string` 重载互补)
- 现有 27 测试 **零回归** (bus 为 nullptr 时走原有路径)

## Success Criteria

- [ ] DSLEngine 可注入自定义 `IInteractionBus` 实例
- [ ] DSLEngine 提供 `get_interaction_bus()` + `subscribe(topic, callback)` 公开方法
- [ ] NodeExecutor 构造函数接受可选 `IInteractionBus*` 参数
- [ ] DSLNode 执行期间推送 started/token/completed 事件 (3+ 事件/调用)
- [ ] ToolNode 执行完成后推送 envelope 事件 (含 4 个 P2-P4 字段)
- [ ] Abort 错误码触发 execution_failed 事件 + 抛出异常
- [ ] 5+ 端到端集成测试通过
- [ ] 全量 32+ 测试通过 (Sprint 1a 27 + Sprint 1b 5+)
- [ ] ASan + LSP 干净
- [ ] TSan 干净 (待 CI 验证)
- [ ] OpenSpec `openspec validate --strict` 通过
- [ ] 提交信息: `feat(bus): integrate IInteractionBus with DSLEngine + NodeExecutor`

## Out of Scope (Non-goals)

- ❌ 不引入新 bus 实现 (Kafka/Redis/ZeroMQ 等) — 仅 InMemoryBus
- ❌ 不修改 IInteractionBus 接口签名 — Sprint 1a 已完成 P2-P4 扩展
- ❌ 不实现 CognitiveWorker 订阅逻辑 — 属 Sprint 2
- ❌ 不实现 PDK 插件事件流 — 属 Sprint 4
- ❌ 不引入 TSan 调试 — 等待 CI 验证

## Risks & Mitigations

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| DSLEngine API 变更破坏现有 25 测试 | 低 | 回归 | bus 注入默认 nullptr, 现有路径不变 |
| NodeExecutor 构造函数签名变更破坏 11 测试 | 中 | 回归 | 默认参数 `IInteractionBus* = nullptr`, 老调用点零修改 |
| 逐 token 推送引入 race condition | 中 | TSan 失败 | InMemoryBus 已有 mutex + 锁外 callback 协议; 新增 emit 调用走同一路径 |
| Subscribe/Unsubscribe 跨实例状态泄漏 | 低 | 测试 flaky | 每 DSLEngine 实例独立 bus (非全局单例) |
| ToolResult envelope 序列化开销 | 低 | 性能 | 已有 28/28 tests 无性能 regression; nlohmann::json to_json 已在 Sprint 1a 验证 |

## 实施计划 (Sprint 1b, 2 天)

- **Day 1 (W1D6)**: S1b.T1 + S1b.T2 (DSLEngine 集成)
- **Day 2 (W1D7)**: S1b.T3 + S1b.T4 (NodeExecutor 集成 + 测试)

## References

- [ADR-0019: IInteractionBus 接口与 TUI Chat MVP](../../docs/adr/adr-0019-iinteraction-bus-mvp.md) §1.4
- [Sprint 1a 实施报告](../../docs/SPRINT-1A-COMPLETION-REPORT.md)
- [ADR-0023: ToolResult 标准化 §C.5](../../docs/adr/adr-0023-tool-result-standard.md) (Sprint 1a IInteractionBus emit 重载)
- [Phase 1 路线图](../../docs/phase1-roadmap.md) Sprint 1b 章节
- [Phase 1 入口决策](../../.omo/decisions/phase1-entry.md) K2 决策
