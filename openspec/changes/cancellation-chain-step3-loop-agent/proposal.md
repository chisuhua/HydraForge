# cancellation-chain-step3-loop-agent

**STATUS**: 🔍 Proposed
**日期**: 2026-08-09
**来源**: improvements/chat-async-io-cancellation-chain.md (Wave 3-A Phase B Step 3 of 5)
**类型**: feature (cross-module cancellation wiring)
**优先级**: P0 (阻塞 Phase B Step 4/5 + Phase C)
**关联依赖**:
- Phase B Step 1+2 `chat-async-io-cancellation-chain` ✅ shipped (2026-08-09)
- Phase A `chat-async-io-queue-infra` ✅ shipped (2026-08-08)

## Why

Phase B Step 1+2 (Cancellation Registry + ChatSession state) 已 ship，但 **stop_token 链路到 loop_agent 入口、NodeExecutor、ToolCoordinator 仍断开**。Step 3 修复审计列出的 3 处关键断开点：

| Break | Location | Fix |
|-------|----------|-----|
| #3 | `pdk/loop_agent/src/pdk_entry.cpp:170-190` | Parse `cancellation_id` from loop_args |
| #4 | `pdk_entry.cpp:229` (forced `std::stop_token{}`) | Forward resolved token to provider bridge |
| #6 | `node_executor.cpp:349-477` (no token in dispatch/yield) | Forward token |
| #7 | `tool_coordinator.cpp:195-203` (no token) | Accept + short-circuit + audit |

不修复这些断开点，Phase B Step 1+2 的 cancellation registry 无法被消费，stop_token 链路仍然断开。

## What Changes

### 核心改动

1. **loop_agent entry point**: 解析 `cancellation_id` → 通过 file-static registry resolve → 转发 token 至 provider bridge + child DSLEngine
2. **NodeExecutor::dispatch_to_tool**: 新增 `std::stop_token` 参数 + 转发至 ToolCoordinator
3. **NodeExecutor YieldNode**: `generate_stream()` 改为传递 real token 而非 `std::stop_token{}`
4. **ToolCoordinator::execute**: 新增 `std::stop_token` 参数 + short-circuit + audit log

### 不修改范围 (Non-goals)

- 不修改 3 个 loop APIs (ReactLoop/PlanExecuteLoop/ForkJoinLoop) — Step 4
- 不实现 Mock provider / E2E 测试 — Step 5
- 不修改 main.cpp `while(getline)` 循环 — 单独 follow-up

## Capabilities

### New Capabilities

- `cancellation-chain-step3-loop-agent`: loop_agent entry + NodeExecutor + ToolCoordinator 端到端 cancellation wiring

### Modified Capabilities

无

## Impact

### 受影响代码

- `pdk/loop_agent/src/pdk_entry.cpp:170-262` (4 处修改)
- `src/modules/executor/node_executor.cpp:349-477` (2 处修改)
- `src/common/tools/tool_coordinator.cpp:195-203` (1 处修改 + 新增 audit emit)

### API 影响

- **BREAKING (minimal)**: `NodeExecutor::dispatch_to_tool` 新增 `std::stop_token token = {}` 参数
- **BREAKING (minimal)**: `ToolCoordinator::execute` 新增 `std::stop_token token = {}` 参数
- loop_agent plugin 接口无变化 (token 在 args JSON 中透传)

### 依赖系统

- CancellationRegistry (Step 1+2 提供)
- ChatSession cancellation_id 在 loop_args (Step 1+2 注入)
- ILLMProvider token-aware API (已 ship)
- YieldStreamBridge token-capable (已 ship)

### 测试影响

- 新增 `tests/test_loop_agent_cancellation.cpp`: 3 tests
  - cancellation_id resolves to valid token in loop_agent
  - NodeExecutor forwards token to YieldNode
  - ToolCoordinator short-circuits on cancelled token

### 风险评估

- **风险等级**: 🟡 MEDIUM
- **理由**: 
  - 3 个模块跨修改，需协同更新
  - NodeExecutor dispatch_to_tool 签名变更需更新所有调用方
- **缓解**: 
  - default 参数 `token = {}` 保持向后兼容
  - 编译错误强制更新调用方
  - TDD 5 步结构每步独立验证