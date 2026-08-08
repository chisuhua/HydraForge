## Context

chat-async-io-steering Phase B — the most critical wiring in the 4-phase decomposition. Without this, the Phase A queue infrastructure is unused because cancellation requests cannot propagate from `ChatSession` to the agent turn's internal `loop_agent` call.

Audit (`docs/audits/2026-08-08-chat-async-io-steering-pre-approval.md` Finding 1) verified **8 concrete breaks** in the current code:

| # | Location | Issue |
|---|----------|-------|
| 1 | `chat_session.h:95-107, 137-141` | ChatSession has no `stop_source`/`stop_token` |
| 2 | `chat_session.cpp:250-261` | `loop/run` args have no cancellation handle |
| 3 | `pdk/loop_agent/src/pdk_entry.cpp:170-190` | Entry doesn't parse token |
| 4 | `pdk_entry.cpp:229` | Provider bridge explicitly uses `std::stop_token{}` |
| 5 | `react_loop.h:80`, `plan_execute_loop.h:198-256`, `fork_join_loop.h:138-139` | Loop APIs lack token parameter |
| 6 | `node_executor.cpp:349-477` | `dispatch_to_tool` + YieldNode forward no token |
| 7 | `tool_coordinator.cpp:195-203` | `execute()` no token |
| 8 | `main.cpp:71-79` | SIGINT/SIGTERM bypass stop semantics via `std::exit(0)` |

Lower-level primitives (`ILLMProvider`, `run_stream_to_bus`, `DomainWorkerPool` jthread) are already token-aware and tested.

### Phase Dependencies

- **Phase 0** (✅ shipped): `g_shutdown_requested` atomic flag (signal handler now respects shutdown)
- **Phase A** (✅ shipped): `steering_queue_` + `follow_up_queue_` (injection target for cancelled messages)

## Goals / Non-Goals

**Goals:**
- 7 步端到端 stop_token wiring (ChatSession → loop_agent → loop → NodeExecutor → ToolCoordinator → provider)
- cancellation registry 跨工具/插件边界序列化
- E2E mid-loop cancel 测试验证链路可达
- BREAKING 变更最小化（仅 loop API token 参数添加）

**Non-Goals:**
- 不修改 Phase A queue 基础设施
- 不重写 main.cpp `while(getline)` 循环（Phase 0 后续集成留作单独 follow-up）
- 不实现 turn 中断点注入到 LLM context（与 cancellation 是正交特性，Phase B 后 ship 单独 change）
- 不实现 `/model` 切换（→ Phase C）
- 不引入第二套取消机制

## Decisions

### Decision 1: Cancellation Handle 序列化方案 — `std::string id` + Registry

**选择**: 引入 `std::unordered_map<std::string, std::shared_ptr<std::stop_source>>` registry；ChatSession 创建 cancellation handle 时分配 unique id（如 UUID 简化版），`loop/run` args 传 `cancellation_id` 字符串，loop_agent 入口查 registry 获取对应 token。

**理由**:
- **可序列化**: `std::stop_token` 不可序列化，字符串 id 易于跨边界传递
- **共享所有权**: `std::shared_ptr<stop_source>` 允许多个消费者共享同一取消源（ChatSession + loop_agent + 测试）
- **生命周期**: registry 由 ChatSession 管理，析构时自动清理
- **简化实现**: 不引入第三方库（无 UUID 依赖）

**替代方案考虑**:
- **整数 handle**: 易冲突，缺乏唯一性保证
- **进程全局单例**: 难以测试隔离，违反依赖注入
- **直接传递 shared_ptr**: 不可序列化

### Decision 2: Loop API Breaking Change 范围 — 仅 token 参数追加

**选择**: 3 个 loop APIs (`ReactLoop::run`, `PlanExecuteLoop::run`, `ForkJoinLoop::run`) 添加 trailing `std::stop_token` 参数，**不重命名**，**不改变返回类型**。

**理由**:
- **最小破坏**: 仅追加参数，调用方只需添加 `token` 实参
- **编译器辅助**: 类型不匹配时编译失败，强制所有调用方更新
- **零行为变化**: 无 token 默认值情况下，`std::stop_token{}` 保持现有行为（永不取消）

**替代方案考虑**:
- **新方法名（如 `run_with_token`）**: 引入方法重载歧义，违反 zero-overhead 原则
- **默认参数 `std::stop_token{}`**: 与决策 1 的序列化 handle 矛盾，调用方需显式提供
- **Builder 模式**: 过度抽象，3 个 API 过于复杂

### Decision 3: Cancellation Registry 生命周期 — ChatSession 拥有

**选择**: `ChatSession::Impl` 持有 `std::unordered_map<std::string, std::shared_ptr<std::stop_source>>` registry；`chat()` 调用入口创建新 entry（带 unique id），析构时清空所有 entry。

**理由**:
- **作用域清晰**: ChatSession 是 cancellation 的逻辑拥有者
- **RAII**: ChatSession 析构自动清理所有 stop_sources
- **测试友好**: 测试可创建 ChatSession + 手动 `request_stop()` 验证

**替代方案考虑**:
- **全局 registry**: 测试隔离差，多实例并发问题
- **DSLEngine 持有**: ChatSession 仅是消费者，registry 应由更高层管理 — 但当前架构 ChatSession 是顶层 entry

### Decision 4: Token 转发策略 — 全链路显式透传

**选择**: ChatSession 持 stop_source，`chat()` 入口 `request_stop()` 触发后，所有受影响的层级（loop_agent 入口 → Provider bridge → loop implementations → NodeExecutor → ToolCoordinator → provider）**显式**接收并转发 token。无静默 `{}` 默认构造。

**理由**:
- **链式取消**: 任一上游取消自动传播至下游
- **测试可观察**: 每个 hop 都可独立断言 token 状态
- **与审计发现一致**: 修复审计列出的 8 处显式 `std::stop_token{}` 丢弃

**替代方案考虑**:
- **异步 polling**: 增加延迟，复杂化
- **future/promise**: 与 `std::stop_token` 重复语义

### Decision 5: 测试策略 — Blocking Mock Provider + 中断断言

**选择**: 新增 `MockBlockingProvider`（测试 helper）实现 `ILLMProvider`，`generate()` 循环检查 `stop_requested()`；E2E 测试启动 chat → 后台线程调用 `request_stop()` → 验证 100ms 内 chat 返回 + `loop.done` 事件未触发。

**理由**:
- **真实场景**: 模拟 LLM 长 turn 中断
- **可断言**: 100ms 阈值明确，无 flaky 测试
- **不引入网络**: mock 模式自包含

**替代方案考虑**:
- **真实 LLM (deepseek)**: 依赖网络，flaky
- **sleep 模拟**: 无法精确测试中断边界

## Risks / Trade-offs

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| BREAKING 变更遗漏调用方 | 🟡 Medium | 🔴 High | 编译错误强制更新；grep all callers before merge |
| cancellation registry 内存泄漏 | 🟢 Low | 🟡 Medium | RAII + shared_ptr，ChatSession 析构时自动清理 |
| token 序列化 handle 冲突 | 🟢 Low | 🟡 Medium | unique id 算法 (timestamp + counter) 保证全局唯一 |
| Phase A 集成引入回归 | 🟢 Low | 🟡 Medium | Phase A 已 ship + 135/138 ctest 稳定基线 |
| ForkJoinLoop CV wait predicate 漏改 | 🟡 Medium | 🟡 Medium | 测试覆盖多 worker 场景 |
| Provider bridge 仍 `std::stop_token{}` | 🟢 Low | 🔴 High | E2E 测试 + grep 验收 (后续 change) |

## Migration Plan

### 部署

无 schema / ABI 变更 → 零迁移成本

### BREAKING 处理

3 个 loop APIs (`ReactLoop::run`, `PlanExecuteLoop::run`, `ForkJoinLoop::run`) 追加 `std::stop_token` 参数：

```cpp
// Before
LoopResult ReactLoop::run(prompt, ctx);

// After
LoopResult ReactLoop::run(prompt, ctx, std::stop_token token);
```

调用方更新模式：
```cpp
// Before
auto result = react_loop.run(prompt, ctx);

// After
auto result = react_loop.run(prompt, ctx, std::stop_token{});  // 或 chat 传入的 token
```

### 回滚

7 步可单独 revert（每步独立 commit）。Phase A + Phase 0 不受影响。

### 兼容性

- `ChatSession::chat(user_input)` 旧调用方：编译器强制更新签名（追加 token 默认值可保留兼容性）
- Plugin tools（loop_agent, provider_agent）：无变化（token 透传不影响 plugin ABI）

## Open Questions

1. **Cancellation handle id 格式**: timestamp + counter 还是 UUID? **决议**: timestamp + atomic counter (无 UUID 依赖)
2. **main.cpp 集成时序**: Phase B ship 后立即集成到 main loop 还是留作单独 change? **决议**: 单独 follow-up change (避免 Phase B 范围爆炸)
3. **测试时 cancellation timeout**: 100ms 是否合理? **决议**: 100ms (LLM 推理通常 ≥ 1s，mock provider 应更短)