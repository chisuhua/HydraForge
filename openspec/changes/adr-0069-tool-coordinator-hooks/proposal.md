## Why

当前 `ToolCoordinator::execute()` 是 6 步线性流（NestingGuard → layer check → ApprovalHandler → audit.invoked → call_tool → audit.completed），既无 pre-hook 拦截/修改参数能力，也无 post-hook 修改结果能力。对照 pi-agent 的 `beforeToolCall`/`afterToolCall`，HydraForge 在工具调用链上存在明确的扩展性缺口：ApprovalHandler 仅能做二元批准/拒绝，无法修改 args 或 result。ToolCoordinator 是 Sprint 14 新 ship 的年轻组件，调用方仅 NodeExecutor 优先级链，此时增加注入点成本最低、破坏性最小。

## What Changes

- **新增** `include/agenticdsl/contract/itool_hook_registry.h` L3 契约头文件：定义 `PreHook`、`PostHook`、`HookErrorPolicy`（FailClosed / FailOpen）与 `IToolHookRegistry` 纯虚接口（`register_pre_hook` / `register_post_hook`）。
- **修改** `src/common/tools/tool_coordinator.h` 与 `src/common/tools/tool_coordinator.cpp`：在 `ToolCoordinator` 内部流程中插入 `pre_hooks[]` 与 `post_hooks[]`，形成 `pre_hooks → layer check → ApprovalHandler → audit.invoked → call_tool → post_hooks → audit.completed → return` 的新 8 步流。
- **新增** `tool.execution.start` 与 `tool.execution.end` 事件发射点，分别位于 pre_hooks 之后、post_hooks 之后，与 ADR-0068 事件发射 Registry 对齐（同源点原则）。
- **新增** hook Deny 路径的 `tool.audit.denied` 发射，payload reason 包含 hook 名称与 `deny_reason`。
- **新增** `budget_agent` plugin 真实 pre-hook 用例：在预算超限时通过 pre-hook 返回 Deny 实现工具调用降级。
- **新增** 5 类测试：`test_tool_coordinator_hooks.cpp` 覆盖 Deny / ModifyArgs / FailClosed / FailOpen / post-hook 后 audit.completed 内容一致性。
- **不修改** `ApprovalHandler` 公开 API（`process_request` 签名与语义保持不变）。
- **不修改** ADR-0031 §8 defer 的语义项（`min_layer` 强制、成本闭环、超时、审批历史）—— 这些留待 C6，但 C6 注入点必须复用本机制。
- **不修改** `transformContext` LLM 上下文钩子（归属 L1-4 decorator 链）与 session 生命周期钩子（归属 L1-3 / ADR-0068）。

## Capabilities

### New Capabilities

- `tool-hook-registry-contract`：工具调用链 pre/post 钩子 L3 契约。覆盖 `IToolHookRegistry` 接口、`HookErrorPolicy` 失败语义、priority 升序排序、`tool_glob` 匹配、hook 参数/结果修改、Deny 发射 `tool.audit.denied`。

### Modified Capabilities

- `tool-coordinator-pipeline`：`ToolCoordinator::execute()` 内部流程从 6 步扩展为 8 步（pre_hooks + post_hooks），在保持 layer check 与 ApprovalHandler 一等公民硬门地位的前提下增加可扩展注入点。**不破坏** 现有 `execute()` 公开签名。

## Impact

- **生产代码**：
  - `include/agenticdsl/contract/itool_hook_registry.h`（新建 L3 契约）
  - `src/common/tools/tool_coordinator.h`（增加 `IToolHookRegistry*` 成员与 setter）
  - `src/common/tools/tool_coordinator.cpp`（重构 execute 流，插入 pre/post hooks 与事件发射点）
- **PDK plugin 用例**：
  - 新增/扩展 `budget_agent` plugin，演示 pre-hook 预算超限降级
- **测试代码**：
  - 新建 `tests/test_tool_coordinator_hooks.cpp`（5 个核心 case + 向后兼容 case）
- **API 兼容性**：
  - ✅ `ToolCoordinator::execute()` 签名不变
  - ✅ 当 `IToolHookRegistry*` 为空时，行为与改造前逐字节一致（向后兼容）
  - ✅ `ApprovalHandler` 公开 API 零修改
- **依赖**：
  - ✅ 无新外部依赖（复用现有 `IInteractionBus`、`ToolMetadata`、`ToolResult`）
- **文档**：
  - `docs/adr/adr-0069-tool-coordinator-hooks.md` 状态可从 🔍 Proposed 转为 🟡 Partial（ Approved 条件见 ADR §决策 7）
- **风险**：
  - 低 - 注入点通过 nullable `IToolHookRegistry*` 引入，空路径零行为变化
  - 关键安全约束：layer check 与 ApprovalHandler 保持硬门，hook 无法绕过或禁用
