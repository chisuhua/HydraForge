# Harness-RSI Remove Governance Spec

## ADDED Requirements

### Requirement: tools_remove 路径过治理检查

`apply_harness_mutation` 的 Gate 2 治理检查 MUST 同时覆盖 `tools_add` 与 `tools_remove` 两个变异路径。任一工具名出现在 `MutationGovernancePolicy.denied_tools` 中时，MUST 返回 `MutationError::GovernanceDenied` 且零状态变更（system_prompt / tools / registry 全部不动）。

#### Scenario: remove 拒绝 denied tool
- **WHEN** `mutations.tools_remove` 包含 policy.denied_tools 中的工具名
- **THEN** 返回 `MutationError::GovernanceDenied`
- **AND** system_prompt 未变
- **AND** registry 中该工具仍然存在（未被 unregister）

#### Scenario: remove 允许 trusted tool
- **WHEN** `mutations.tools_remove` 包含未被 policy 拒绝的工具名
- **AND** Gate 0/1/2.5 全部通过
- **THEN** 返回 success
- **AND** registry 中该工具被 unregister（blast radius 按 pilot 语义 = 全局）

#### Scenario: add + remove 混合双拒绝
- **WHEN** `mutations.tools_add` 含 denied_tool_A 且 `mutations.tools_remove` 含 denied_tool_B
- **THEN** 返回 `MutationError::GovernanceDenied`（add 或 remove 任一命中即拒绝）
- **AND** 零状态变更（两个路径都未应用）

### Requirement: SecureToolRegistry unregister 安全检查

`SecureToolRegistry::unregister_tool_function(name)` MUST 在委托到 wrapped registry 前检查该工具是否被 `disable_tool` 禁用。被禁用的工具 MUST 拒绝 unregister（静默忽略，不修改底层 registry，与 `call_direct` disabled 语义一致）。

#### Scenario: disabled tool 拒绝 unregister
- **WHEN** `secure.disable_tool("shell.exec")` 后调用 `secure.unregister_tool_function("shell.exec")`
- **THEN** wrapped registry 中该工具仍然存在（unregister 被拒绝）
- **AND** 无异常抛出（静默忽略语义）

#### Scenario: enabled tool 正常 unregister
- **WHEN** 工具未被禁用时调用 `secure.unregister_tool_function(name)`
- **THEN** wrapped registry 中该工具被移除

### Requirement: ToolRegistry mutation 并发保护

`ToolRegistry::register_tool_function` 与 `ToolRegistry::unregister_tool_function` MUST 受同一把 mutex 保护（写-写互斥）。读路径（has_tool / list_tools / call_tool）MUST NOT 获取该锁——文档化约束："读路径假设写路径单线程或外部同步"。

#### Scenario: 并发 register + unregister 无数据竞争
- **WHEN** 两个线程并发分别调用 register_tool_function 与 unregister_tool_function（不同工具名）
- **THEN** 无数据竞争（TSan 干净）
- **AND** 最终 registry 状态 = 一个工具已注册 + 另一个已移除

#### Scenario: 回调不得反向注册
- **WHEN** mutation_mutex_ 持有期间工具回调尝试再次调用 register/unregister
- **THEN** 文档化禁止（非递归 mutex，回调反向注册会死锁）
- **AND** 测试不覆盖此场景（约束为文档级）

### Requirement: trace_id 事件透传

`MutationGateContext` MUST 提供 `trace_id` 字段（默认空字符串）。`apply_harness_mutation` 发射 `evolution.readiness.denied` 事件时，MUST 使用 `ctx.trace_id` 作为事件 meta 的 `trace_id` 值（替代硬编码空串）。

#### Scenario: trace_id 透传
- **WHEN** ctx.trace_id = "trace-abc-123" 且 readiness 失败发射事件
- **THEN** 事件 meta 的 trace_id == "trace-abc-123"

#### Scenario: trace_id 默认空
- **WHEN** ctx.trace_id 未设置（默认空字符串）发射事件
- **THEN** 事件 meta 的 trace_id == ""（向后兼容，Wave 3 调用方必填）
