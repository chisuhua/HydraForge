# Harness-RSI Remove Governance — Proposal

## Why

C4 `harness-rsi-pilot` 的 `tools_remove` 路径存在 3 个已验证的安全/并发债务（Oracle bg_afa84d4d 独立审查确认）：**remove 完全绕过 `is_tool_allowed` 治理检查**（Gate 2 只查 tools_add）、**`SecureToolRegistry::unregister_tool_function` 裸委托无安全检查**、**ToolRegistry 全类无 mutex**（unregister 引入运行期全局变异）。这些是真实安全洞而非文档措辞问题——若 Wave 3 (ADR-0078 Model-RSI pilot) 直接启动，会把未治理的 remove 当作已验证模式复制。同时 `harness_rsi.cpp:121` 的 `trace_id: ""` 空值切断因果链（模式 #7 教训：语义字段必须有稳定标识符）。

## What Changes

- **remove 路径过治理检查**: `apply_harness_mutation` Gate 2 扩展为同时检查 `tools_remove`（与 tools_add 对称），`GovernanceDenied` 拒绝时零状态变更
- **SecureToolRegistry 安全校验**: `SecureToolRegistry::unregister_tool_function` 增加 `check_security` 检查（与 `call_tool` 一致），disabled 工具拒绝 unregister
- **ToolRegistry 并发保护**: 为 `register_tool_function` / `unregister_tool_function` 增加 `std::mutex`（最小并发面），或显式文档化"单线程 mutation 约束"（Oracle 最低成本方案）
- **trace_id 透传**: `MutationGateContext` 增加 `trace_id` 字段，`harness_rsi.cpp` 发射事件时从 ctx 传入真实 trace_id（替代硬编码 `""`）

## Capabilities

### New Capabilities
- `harness-rsi-remove-governance`: tools_remove 路径的治理对称 + 安全校验 + 并发保护 + trace_id 透传。覆盖 apply_harness_mutation 的 remove 语义、SecureToolRegistry unregister 安全检查、ToolRegistry mutation 并发约束

### Modified Capabilities
- (none — 新 capability 独立，无既有 spec 行为变更)

## Impact

- `src/evolution/harness_rsi.cpp` — Gate 2 扩展 tools_remove + trace_id 发射
- `include/agenticdsl/evolution/harness_rsi.h` — `MutationGateContext` + trace_id 字段（**BREAKING**: 结构体增加 1 字段，构造点需同步）
- `src/common/tools/secure_tool_registry.cpp` — unregister 安全检查
- `src/common/tools/registry.h` + `.cpp` — mutex 或文档化约束
- `tests/test_harness_rsi_pilot.cpp` — 新增 remove-governance 测试 cases
- `tests/test_tool_registry*` — SecureToolRegistry unregister 安全测试
- 无外部依赖变更，无 PDK contract 变更（trace_id 是 MutationGateContext 内部字段）
