# Design: Phase 1 Sprint 1b — DSLEngine Bus 集成

> **关联**: [proposal.md](proposal.md)
> **ADR**: [ADR-0019 §1.4](../../docs/adr/adr-0019-iinteraction-bus-mvp.md) + K2 决策
> **前置**: [Sprint 1a 实施报告 — ToolResult P1-P4](../../docs/SPRINT-1A-COMPLETION-REPORT.md) (commits `fb67a9b`/`60b31b5`/`5c9ba18`)

## 架构合规性检查

| 约束 | 状态 | 备注 |
|------|------|------|
| 中文注释优先 | ✅ | 所有新增注释中文 |
| 文件头 4 项 | ✅ | proposal/design/specs/tasks 均有作者+日期 |
| C++20 + CMake 3.20+ | ✅ | 沿用现有 |
| 2 空格缩进 | ✅ | 沿用现有 |
| nlohmann_json 使用 | ✅ | ToolResult 序列化 (Sprint 1a) |
| Anti-pattern 避免 | ✅ | 不删失败测试, bus 注入默认 nullptr 保持向后兼容 |

## 关键设计决策

### 决策 1: bus 注入使用 std::shared_ptr (非 unique_ptr)

**问题**: DSLEngine 创建 InMemoryBus 后, NodeExecutor 持引用; 生命周期如何管理?

**方案**: `std::shared_ptr<IInteractionBus>` 注入到 DSLEngine,NodeExecutor 持 `IInteractionBus*` (非 owning)。

```cpp
// engine.h
class DSLEngine {
public:
  void set_interaction_bus(std::shared_ptr<IInteractionBus> bus);
  std::shared_ptr<IInteractionBus> get_interaction_bus() const;
  size_t subscribe(const std::string& topic,
                   std::function<void(const ToolResult&)> cb);
private:
  std::shared_ptr<IInteractionBus> bus_;  // 默认 nullptr
};

// node_executor.h
class NodeExecutor {
public:
  NodeExecutor(ToolRegistry& tool_registry, ILLMProvider* llm_provider = nullptr,
               IInteractionBus* bus = nullptr);  // Sprint 1b 新增
  // ...
};
```

**理由**:
- (a) `shared_ptr` 允许多个 consumer (CognitiveWorker、NodeExecutor、TraceExporter) 共享 bus
- (b) NodeExecutor 持 raw pointer 避免循环引用 (NodeExecutor 生命周期短于 DSLEngine)
- (c) 默认 nullptr 路径保持现有 25 测试零修改

### 决策 2: 事件 topic 命名规范 (kebab-case + 命名空间)

**问题**: Sprint 1a T3 完成的 `emit(event_type, payload)` 接口,`event_type` 命名需统一。

**方案**: `<category>.<action>` 格式 (e.g., `dsl.call.started`, `tool.completed`, `execution.failed`)

| Topic | 触发时机 | Payload |
|-------|---------|---------|
| `dsl.call.started` | `execute_dsl_node` 进入时 | `ToolResult::success({...}, {{"prompt", s}})` |
| `llm.token` | LLM 流式生成每个 token (Sprint 1b 范围: 调用 `get_next_token()` 接口) | `ToolResult::success({...}, {{"token", s}})` |
| `dsl.call.completed` | `execute_dsl_node` 退出时 | `ToolResult::success({...text, ...})` |
| `tool.completed` | `execute_tool_call` 完成后 | `ToolResult` envelope (含 error_code/latency_ms/trace_id) |
| `execution.failed` | error_code == Abort / 异常路径 | `ToolResult::error(ErrorCode::Abort, msg)` |

**理由**:
- (a) 点分命名空间便于 subscriber 过滤 (e.g., `subscribe("tool.*", cb)`)
- (b) 与 Sprint 1a 完成的 `meta["content"]` 包装策略一致 (向后兼容)

### 决策 3: 错误码 → 事件的映射 (REQ-TR-001 集成)

**问题**: `execute_tool_call` 返回 `ErrorCode::Retry` / `Abort` / `Skip` 时, 如何与 bus 事件配合?

**方案** (基于 Sprint 1a C.4 决策):

| ErrorCode | 事件 | 异常 |
|-----------|------|------|
| `Retry` | `execution.failed` (meta 含 error_code=Retry) | throw `[RETRY] ...` (Sprint 1a 占位) |
| `Abort` | `execution.failed` (meta 含 error_code=Abort) | throw `[ABORT] ...` (Sprint 1a 行为) |
| `Skip` | 不推送事件, output_keys 不写 | 不 throw, 返回原 context |
| 其他 | `execution.failed` (meta 含 error_code) | throw 通用异常 |

**理由**:
- (a) bus 推送先于 throw, 让 subscriber 在异常传播前捕获状态
- (b) `Skip` 不推送, 因为是软失败 (不破坏 graph)
- (c) 与 Sprint 1a C.4 error_code 分发逻辑保持一致

### 决策 4: 默认 bus 实例化时机

**问题**: DSLEngine 默认应该创建 InMemoryBus, 还是要求用户显式注入?

**方案**: DSLEngine 构造函数**不**自动创建 bus; `get_interaction_bus()` 返回 nullptr 时,emit 调用静默 no-op (不抛异常)。

```cpp
// engine.cpp
std::shared_ptr<IInteractionBus> DSLEngine::get_interaction_bus() const {
  return bus_;  // 可能为 nullptr
}

void DSLEngine::set_interaction_bus(std::shared_ptr<IInteractionBus> bus) {
  bus_ = std::move(bus);
}
```

**理由**:
- (a) 显式注入符合"依赖反转"原则 (Sprint 0 决策)
- (b) nullptr 路径零开销, 现有 25 测试零修改
- (c) 测试可注入 mock bus, 主路径集成测试可注入 InMemoryBus

### 决策 5: Subscribe 透传 vs 缓存

**问题**: DSLEngine 是否缓存 subscriber 列表, 还是直接透传到 bus?

**方案**: DSLEngine.subscribe() 直接透传 `bus_->subscribe()` 返回 token, 不缓存。

```cpp
// engine.cpp
size_t DSLEngine::subscribe(const std::string& topic,
                            std::function<void(const ToolResult&)> cb) {
  if (!bus_) return 0;  // 无 bus, 返回无效 token
  return bus_->subscribe(topic, std::move(cb));
}
```

**理由**:
- (a) token 由 bus 管理, DSLEngine 不持有 subscriber 状态
- (b) 简化生命周期 (DSLEngine 析构时 bus 析构 → subscriber 自动失效)
- (c) 与 Sprint 1a 完成的 InMemoryBus API 兼容

## 测试设计

### 单元测试 (test_engine_bus_integration.cpp)

1. **DSLEngine 注入 custom bus + subscribe → emit 验证**
   - 注册 MockBus (继承 IInteractionBus, 记录 emit 调用)
   - 注入到 DSLEngine
   - 调用 DSLEngine.subscribe("test.topic", cb)
   - 调用 bus->emit("test.topic", payload) (直接)
   - 验证 cb 被调用 + payload 正确

2. **DSLNode execute_dsl_node 期间 bus 收到 3 事件**
   - 注入 InMemoryBus
   - 加载简单 DSL (DSLNode with MockLLMProvider)
   - 订阅 dsl.call.started / llm.token / dsl.call.completed
   - 运行 DSL
   - 验证收到 1 + 0 (Sprint 1b 无 streaming) + 1 = 2 事件 (Sprint 1c 增加 token)

3. **ToolNode execute_tool_call 完成后 bus 收到 envelope**
   - 注入 InMemoryBus
   - 注册 mock tool (返回 ToolResult envelope with error_code=Retry)
   - 加载 ToolNode DSL
   - 订阅 tool.completed
   - 运行, 验证 envelope 含 error_code=Retry + latency_ms + trace_id

4. **Abort 错误码触发 execution_failed 事件 + 抛出异常**
   - Tool 返回 envelope error_code=Abort
   - 订阅 execution.failed
   - 运行, 验证:
     - 收到 1 execution.failed 事件 (meta 含 error_code=Abort)
     - 抛 std::runtime_error with "[ABORT]"

5. **默认 nullptr bus 路径 (零回归)**
   - DSLEngine 不注入 bus
   - 加载 + 运行简单 DSL
   - 验证不抛异常, 与 Sprint 1a 之前行为一致

6. **(Bonus) 1000x 并发 subscribe + emit 无死锁**
   - 10 线程 × 100 次 emit
   - 验证所有 callback 触发 + 无 race (TSan 干净)

## 接口变更示意

### src/core/engine.h (Sprint 1b 后)

```cpp
class DSLEngine {
public:
  // 现有 API (不变)
  static std::unique_ptr<DSLEngine> from_markdown(const std::string&);
  static std::unique_ptr<DSLEngine> from_file(const std::string&);
  ExecutionResult run(const Context& = {});
  // ...

  // === Sprint 1b 新增: IInteractionBus 集成 ===
  void set_interaction_bus(std::shared_ptr<IInteractionBus> bus);
  std::shared_ptr<IInteractionBus> get_interaction_bus() const;
  size_t subscribe(const std::string& topic,
                   std::function<void(const ToolResult&)> cb);

private:
  // 现有成员 (不变)
  std::vector<ParsedGraph> full_graphs_;
  ToolRegistry tool_registry_;
  std::unique_ptr<ILLMProvider> llm_provider_;
  // ...
  std::unique_ptr<BudgetController> budget_controller_;

  // Sprint 1b 新增
  std::shared_ptr<IInteractionBus> bus_;  // 默认 nullptr
};
```

### src/modules/executor/node_executor.h (Sprint 1b 后)

```cpp
class NodeExecutor {
public:
  // Sprint 1b: 新增可选 bus 参数 (Sprint 1a 已有 ILLMProvider 可选)
  NodeExecutor(ToolRegistry& tool_registry,
               ILLMProvider* llm_provider = nullptr,
               IInteractionBus* bus = nullptr);

  // ADR-0019 §1.4 + Stage 4 Task 20: IParser 注入 (不变)
  NodeExecutor(ToolRegistry& tool_registry,
               ILLMProvider* llm_provider,
               std::unique_ptr<IParser> parser,
               IInteractionBus* bus = nullptr);  // Sprint 1b 新增

  // ... 其他不变

private:
  // 现有成员
  ToolRegistry& tool_registry_;
  ILLMProvider* llm_provider_;
  // ...
  IInteractionBus* bus_;  // Sprint 1b 新增 (非 owning, 默认 nullptr)
};
```

## 实施步骤

1. **S1b.T1** (Day 1 上午): `engine.h` 移除 1 include (P1.T4 遗留: `common/tools/registry.h` → 前向声明) + 添加 bus 成员
2. **S1b.T2** (Day 1 下午): `engine.cpp` 实现 set/get/subscribe 方法
3. **S1b.T3** (Day 2 上午): `node_executor.h` + `node_executor.cpp` 集成 bus, 推送 5 个事件 topic
4. **S1b.T4** (Day 2 下午): `tests/test_engine_bus_integration.cpp` 5+ 测试 + ctest 验证
5. **T6 文档**: 更新 `roadmap-status.md` + `phase1-roadmap.md` + ADR-0019 §状态 + 提交

## 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| DSLEngine 27 测试回归 | 低 | 阻塞 | bus 注入默认 nullptr 路径不变 |
| NodeExecutor 11 测试回归 | 中 | 阻塞 | 构造函数默认参数 nullptr, 7 个现有调用点零修改 |
| 逐 token 推送 race | 中 | TSan 失败 | Sprint 1b 暂不实现逐 token (留 Sprint 1c) |
| Subscribe 透传 token 失效 | 低 | 测试 flaky | 由 InMemoryBus 统一管理 token 生命周期 |
| ToolResult envelope 序列化开销 | 低 | 性能 | Sprint 1a 已验证 (27 tests pass) |

## 引用

- [ADR-0019 §1.4 IInteractionBus 设计](../../docs/adr/adr-0019-iinteraction-bus-mvp.md)
- [ADR-0023 §C.5 IInteractionBus emit 重载 (Sprint 1a)](../../docs/adr/adr-0023-tool-result-standard.md)
- [Sprint 1a 实施报告 — ToolResult P1-P4](../../docs/SPRINT-1A-COMPLETION-REPORT.md)
- [InMemoryBus 实现](../../src/common/contract/inmemory_bus.cpp) (Sprint 1a 后, 含 std::string 重载)
