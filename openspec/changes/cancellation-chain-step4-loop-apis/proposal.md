# cancellation-chain-step4-loop-apis

**STATUS**: 🔍 Proposed
**日期**: 2026-08-09
**来源**: improvements/chat-async-io-cancellation-chain.md (Wave 3-A Phase B Step 4 of 5)
**类型**: feature (BREAKING API change)
**优先级**: P0 (阻塞 Phase B Step 5 + Phase C)
**关联依赖**:
- Phase B Step 1+2 ✅ shipped (2026-08-09)
- Phase B Step 3 ✅ shipped (2026-08-09)

## Why

Phase B Step 3 已 ship loop_agent entry + NodeExecutor + ToolCoordinator 的 stop_token wiring。但**3 个 loop APIs 仍未接受 std::stop_token 参数**——这是审计断开点 #5（`react_loop.h:80`, `plan_execute_loop.h:198-256`, `fork_join_loop.h:138-139`）。

不修复此断开点：
1. Loop 内部 LLM 调用无法取消（`std::stop_token{}` 丢弃）
2. ForkJoinLoop 的 CV wait 无法响应取消
3. Phase B E2E 测试（Step 5）无法验证端到端取消

## What Changes

### 核心改动 (3 BREAKING)

1. **ReactLoop::run()**: 新增 `std::stop_token token = {}` 参数
2. **PlanExecuteLoop::run()**: 新增 `std::stop_token token = {}` 参数 + 替换内部 2 处 `std::stop_token{}` (lines 206, 256)
3. **ForkJoinLoop::run()**: 新增 `std::stop_token token = {}` 参数 + CV wait predicate 增加 `token.stop_requested()` + 取消时调用 `pool_->stop()`

### 不修改范围 (Non-goals)

- 不修改 Phase B Step 1+2+3 已有改动
- 不实现 Mock provider / E2E 测试（→ Step 5）
- 不修改其他 loop APIs（已 token-aware）
- 不重命名方法或修改返回类型

## Capabilities

### New Capabilities

- `cancellation-chain-step4-loop-apis`: 3 个 loop APIs (ReactLoop/PlanExecuteLoop/ForkJoinLoop) 接受 std::stop_token 参数 + 内部正确转发

### Modified Capabilities

无（这是 step 4 of 5 的拆分之一，不修改 spec-level behavior，仅添加参数）

## Impact

### 受影响代码

- `include/agenticdsl/pdk/agent_loops/react_loop.h:80` — 添加 token 参数
- `include/agenticdsl/pdk/agent_loops/plan_execute_loop.h:198-256` — 添加 token 参数 + 2 处替换
- `include/agenticdsl/pdk/agent_loops/fork_join_loop.h:138-257` — 添加 token 参数 + CV wait predicate

### 所有调用方需更新

- `examples/pdk_chat_demo/chat_session.cpp` — 调用 3 个 loop APIs
- `pdk/loop_agent/src/pdk_entry.cpp` — 调用 child DSLEngine.run() (间接)
- 其他潜在调用方（grep 验证）

### API 影响

- **BREAKING (3 处)**: 3 个 loop APIs 新增 `std::stop_token token = {}` 参数（default 保持向后兼容）
- 现有调用方编译错误强制更新（添加 `token` 实参）
- 无 schema / ABI 变更

### 测试影响

- 现有 loop API 测试需更新（添加 token 参数）
- 新增 3 unit tests 验证 token propagation

### 风险评估

- **风险等级**: 🟡 MEDIUM-HIGH
- **理由**:
  - 3 个 BREAKING API 同时修改，所有调用方需更新
  - PlanExecuteLoop 内部 2 处替换需精确（行号 206, 256）
  - ForkJoinLoop CV wait predicate 需正确添加 `token.stop_requested()`
- **缓解**:
  - default `token = {}` 保持向后兼容
  - 编译错误强制更新所有调用方
  - TDD 5 步结构验证每处改动
  - grep + 编译器交叉验证所有调用方