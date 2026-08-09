# cancellation-chain-step5-e2e

**STATUS**: 🔍 Proposed
**日期**: 2026-08-09
**来源**: improvements/chat-async-io-cancellation-chain.md (Wave 3-A Phase B Step 5 of 5 - FINAL)
**类型**: feature (E2E test infrastructure)
**优先级**: P0 (闭环验证 Phase B 7 步 wiring)
**关联依赖**:
- Phase B Step 1+2 `chat-async-io-cancellation-chain` ✅ shipped (2026-08-09)
- Phase B Step 3 `cancellation-chain-step3-loop-agent` ✅ shipped (2026-08-09)
- Phase B Step 4 `cancellation-chain-step4-loop-apis` ✅ shipped (2026-08-09)

## Why

Phase B Step 1+2+3+4 已 ship 完整 7 步 wiring 基础设施，但**端到端取消行为尚未验证**——`docs/audits/2026-08-08-chat-async-io-steering-pre-approval.md` 中识别但仅静态审计，无动态 E2E 测试。

不实现 E2E 测试：
1. 无法验证 cancellation 链路真实可达（可能存在 wiring 错误未被发现）
2. 无法保证未来重构不破坏取消语义
3. Phase B 交付不完整（基础 wiring ship 但无回归保护）

Step 5 是 Phase B 闭环验证的最后一步。

## What Changes

### 核心改动

1. **Mock Blocking Provider** (test helper) — `ILLMProvider` 实现，`generate()` 循环检查 `token.stop_requested()` 模拟长 LLM 调用
2. **E2E mid-loop cancellation test** — 5 个 test cases 验证端到端取消
3. **Full verification** — ctest + ASan + lsp_diagnostics

### 不修改范围 (Non-goals)

- 不修改 Phase B Step 1+2+3+4 已有 wiring
- 不实现真实 LLM provider 的取消集成（已由 ILLMProvider token-aware API 支持）
- 不实现 SIGINT 集成到 mock 模式（mock 模式无 stdin 注入）

## Capabilities

### New Capabilities

- `cancellation-chain-step5-e2e`: Mock Blocking Provider + E2E mid-loop cancellation test 验证端到端取消行为

### Modified Capabilities

无

## Impact

### 受影响代码

- `examples/pdk_chat_demo/tests/mock_blocking_provider.h` (NEW) — Mock ILLMProvider
- `examples/pdk_chat_demo/tests/test_chat_session_cancellation.cpp` (NEW) — 5 E2E tests
- `examples/pdk_chat_demo/tests/CMakeLists.txt` — 测试注册

### API 影响

无 — 仅新增测试 helper 和测试文件

### 依赖系统

- CancellationRegistry (Step 1+2)
- ChatSession cancellation state (Step 1+2)
- loop_agent + NodeExecutor + ToolCoordinator token forwarding (Step 3)
- 3 loop APIs token params (Step 4)

### 测试影响

- 新增 `test_chat_session_cancellation`: 5 cases
  - request_stop interrupts blocking provider within 100ms
  - Token identity preserved through registry
  - Default token never cancels
  - cancellation_id resolves to valid token in loop_agent
  - Token forwarded through registry without modification
- 期望 ctest 增量: +1 测试文件 (~5 cases)

### 风险评估

- **风险等级**: 🟢 LOW
- **理由**:
  - 仅新增测试，不修改 production 代码
  - Mock provider 隔离性好（不影响其他测试）
  - Step 1+2+3+4 已提供稳定集成基础
- **回滚方案**: 删除测试文件即可（无 production 影响）