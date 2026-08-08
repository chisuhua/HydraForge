# chat-async-io-cancellation-chain

**STATUS**: 🔍 Proposed
**日期**: 2026-08-08
**来源**: improvements/chat-async-io-cancellation-chain.md (Wave 3-A Phase B)
**类型**: feature (cross-cutting infrastructure)
**优先级**: P0 (阻塞 Phase C `/model` 切换)
**关联依赖**:
- Phase 0 `fix-tool-registry-signal-handler-shutdown` ✅ shipped (2026-08-08)
- Phase A `chat-async-io-queue-infra` ✅ shipped (2026-08-08)

## Why

`chat-async-io-cancellation-chain` 是 chat-async-io-steering Phase B（核心 wiring）。审计 (`docs/audits/2026-08-08-chat-async-io-steering-pre-approval.md`) 发现 **stop_token 链路完全未连通**——ChatSession 创建 token 但 8 处断开点（loop_agent 显式 `std::stop_token{}` 丢弃、NodeExecutor/ToolCoordinator 无 token 参数、SIGINT/SIGTERM 走 `std::exit(0)` 绕开停止语义）使任何取消请求无法到达 Agent turn 内部。

Phase A 提供了 queue 基础设施（steering/follow-up 注入目标），但**没有 cancellation chain，steering 消息无法中断当前 turn**。

Phase B 引入 7 步端到端 stop_token wiring，是 steering 行为可工作的关键路径。

## What Changes

### 核心改动 (7 步)

1. **ChatSession 持 `std::stop_source`** + `chat()` 接受 `std::stop_token` 参数
2. **`loop/run` args 加 `cancellation_id`** (因 `std::stop_token` 不可序列化跨工具/插件边界)
3. **创建 cancellation registry** (handle id → shared `stop_source`/`stop_token` state)
4. **loop_agent 解析 `cancellation_id`** + Provider bridge 转发 token (替换 `pdk_entry.cpp:229` 显式 `std::stop_token{}`)
5. **ReactLoop / PlanExecuteLoop / ForkJoinLoop API 加 `std::stop_token` 参数** (替换内部 `std::stop_token{}` 默认构造)
6. **NodeExecutor::dispatch_to_tool + ToolCoordinator::execute 转发 token** (YieldNode `generate_stream()` 改为传递 real token)
7. **E2E mid-loop cancel 测试** (blocking mock provider + `request_stop()` 后 100ms 内退出)

### 不修改范围 (Non-goals)

- 不修改 Phase A 已 ship 的 queue 基础设施 (复用 API)
- 不重写 main.cpp `while(getline)` 循环 (Phase A 已 ship 队列, main loop 集成留作 Phase 0 后续)
- 不实现 `/model` 切换 (→ Phase C)
- 不引入新的取消机制 (复用 `std::stop_token` + ADR-0001/0042 既有路径)

## Capabilities

### New Capabilities

- `chat-async-cancellation-chain`: ChatSession → loop_agent → NodeExecutor/ToolCoordinator 完整 stop_token 链路，定义 cancellation handle 注册表与 7 步 wiring 契约

### Modified Capabilities

无 (Phase A + Phase 0 提供的基础设施不变)

## Impact

### 受影响代码

- `examples/pdk_chat_demo/chat_session.h` — ChatSession 新增 `chat()` token 参数 + cancellation registry 引用
- `examples/pdk_chat_demo/chat_session.cpp` — Impl 持 `stop_source_` + cancel hook + token 注入到 loop_args
- `pdk/loop_agent/src/pdk_entry.cpp:170-262` — 入口解析 `cancellation_id` + Provider bridge 转发 token
- `include/agenticdsl/pdk/agent_loops/react_loop.h:80` — `ReactLoop::run()` 加 token 参数
- `include/agenticdsl/pdk/agent_loops/plan_execute_loop.h:198-256` — `PlanExecuteLoop` 加 token 参数 + 替换内部 `{}`
- `include/agenticdsl/pdk/agent_loops/fork_join_loop.h:138-139` — `ForkJoinLoop` 加 token 参数 + CV wait predicate 加 `token.stop_requested()`
- `src/modules/executor/node_executor.cpp:349-477` — `dispatch_to_tool` + YieldNode 转发 token
- `src/common/tools/tool_coordinator.cpp:195-203` — `execute()` 接受 token + 转发至 registry

### API 影响

- **BREAKING**: 3 个 loop API (`ReactLoop::run`, `PlanExecuteLoop::run`, `ForkJoinLoop::run`) 新增 `std::stop_token` 参数 (现有调用方需更新)
- **新增公开 API**: `ChatSession::chat(user_input, std::stop_token)` (Phase A 之上扩展)
- 无 schema / ABI 变更（plugin 接口已支持 token）

### 依赖系统

- Phase 0 ship: `g_shutdown_requested` atomic flag (signal handler 委托)
- Phase A ship: `steering_queue_` + `follow_up_queue_` (中断点注入目标)
- ADR-0001/0042: 既有 `std::stop_token` 取消语义（唯一中断机制）
- `ILLMProvider` token-aware API (已 ship, 提供 token 透传)
- `DomainWorkerPool` jthread 内部取消 (已 ship, 提供 worker 取消能力)

### 测试影响

- 新增 E2E mid-loop cancel 测试 (`tests/test_chat_session_cancellation.cpp`)
- 现有 3 pre-existing 失败不变
- 新增 3 loop APIs token 单元测试
- 期望 ctest 增量: +2 测试文件 (~5 cases)

### 风险评估

- **风险等级**: 🟡 MEDIUM-HIGH
- **理由**:
  - **7 步全链路依赖**: 任一断开点都会丧失取消能力
  - **BREAKING 3 个 loop APIs**: 现有调用方需更新
  - **跨模块 wiring**: ChatSession ↔ loop_agent ↔ NodeExecutor ↔ ToolCoordinator
  - **token 序列化问题**: `std::stop_token` 不可序列化,需引入 cancellation registry
- **缓解**:
  - TDD 5 步结构每步独立验证
  - cancellation registry handle 设计为 std::string id (易序列化)
  - BREAKING 变更最小化 (仅 token 参数添加,无方法签名重命名)
  - Phase 0 + Phase A 已 ship 提供稳定集成基础
- **回退方案**: 7 步可单独 revert,渐进式 rollback