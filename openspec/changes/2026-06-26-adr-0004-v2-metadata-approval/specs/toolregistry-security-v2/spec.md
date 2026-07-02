# toolregistry-security-v2 Specification

> **Purpose**: 追踪 ADR-0004 V2 (ToolRegistry Security Metadata + Approval) 实施 (Sprint 16 主体)
> **STATUS: ACTIVE** 🟢 — 6 ADDED Requirements 全部完成

## ADDED Requirements

### Requirement: toolregistry-v2-register-with-meta

`ToolRegistry` 的注册 API `register_tool_function()` MUST 接受 `ToolMetadata` 参数作为注册必填项。

#### Scenarios

- **WHEN** 调用 `register_tool_function("my_tool", ToolMetadata{...}, handler_fn)`
- **THEN** 注册成功，ToolRegistry 内部存储 `name → {meta, fn}` 映射

- **WHEN** 调用旧签名 `register_tool_function("my_tool", handler_fn)`（无 meta）
- **THEN** 编译错误（BREAKING，设计意图）

- **WHEN** `register_tool<Func>(name, meta, func)` 模板调用
- **THEN** 内部委托到 `register_tool_function(name, meta, erased_fn)`

### Requirement: toolregistry-v2-validate-conflict

注册时 MUST 执行元数据 validation，检测 `category × approval_policy` 冲突和 `min_layer × allowed_layers` 不一致。

#### Scenarios

- **WHEN** 注册 `category=Execute, approval={plan=false, agent=false, yolo=true}`（YOLO 默认不审批 + Execute 不审批 = 危险操作无审批）
- **THEN** `std::invalid_argument` throw，提示"Execute category requires at least plan or agent approval"

- **WHEN** 注册 `min_layer=Workflow, allowed_layers=[Cognitive]`（min_layer 不在 allowed_layers 中）
- **THEN** `std::invalid_argument` throw，提示"min_layer Workflow not in allowed_layers"

- **WHEN** 注册 `category=Network, allowed_layers=[Cognitive]`（Cognitive 层不允许 Network）
- **THEN** `std::invalid_argument` throw，提示"Cognitive layer does not permit Network tools"

- **WHEN** 注册合法元数据（e.g. ReadOnly + agent_approval + [Cognitive, Thinking, Workflow]）
- **THEN** 注册成功

- **WHEN** `allowed_layers` 为空（默认 = 全允许）
- **THEN** 跳过 `min_layer × allowed_layers` 检查，注册成功

### Requirement: pdk-declare-tool-v2-mandatory

`DECLARE_TOOL` 宏 MUST 强制要求 `category` 和 `approval_policy` 参数（位置参数第 3、4 位）。

#### Scenarios

- **WHEN** `DECLARE_TOOL(echo, "echo tool", ReadOnly, "agent", return args;)` 调用
- **THEN** 宏展开为完整的 `tool_spec_echo`（含 `ToolMetadata{name="echo", category=ReadOnly, ...}`） + handler 函数

- **WHEN** `DECLARE_TOOL(echo, "echo tool", ...)` 调用（遗漏第 3、4 参数）
- **THEN** 编译错误 — 宏参数计数不匹配

- **WHEN** `approval_policy` 传入非法字符串（非 `"plan"` / `"agent"` / `"yolo"` / `"always"`）
- **THEN** 展开后的 `ToolSpec` 使用默认 `ApprovalPolicy{}`（宽松行为，留运行时 catch）

### Requirement: pdk-declare-tool-v2-compile-check

缺失必要宏参数 MUST 触发编译错误（通过宏参数计数差异）。

#### Scenarios

- **WHEN** 宏定义 `#define DECLARE_TOOL(name, description, category, policy, ...)` 且调用只传 3 个参数
- **THEN** GCC/Clang 报 `error: too few arguments to function-like macro invocation`

- **WHEN** 宏调用传足 5 个参数（name, desc, category, policy, body）
- **THEN** 正常展开，无编译错误

### Requirement: approval-workflow-toolcoordinator-link

`ApprovalHandler` 收到的 `ToolPreview` MUST 包含完整 `ToolMetadata` JSON，以便 TUI 渲染。

#### Scenarios

- **WHEN** ToolCoordinator 调用 `approval_handler_->process_request(meta, ctx, preview)`
- **THEN** `preview.metadata_json` 字段包含 `meta` 的完整 JSON 序列化（name, category, min_layer, allowed_layers, cost_estimate, timeout_ms, approval policy 等）

- **WHEN** TUI `/apply` 回调收到 `ToolPreview`
- **THEN** 可读取 `preview.metadata_json` 并在显示审批对话框时渲染

### Requirement: layer-matrix-registration-check

Layer × ToolCategory 权限矩阵 MUST 在注册时静态验证 `allowed_layers` 合法性。

#### Scenarios

- **WHEN** 注册工具的 `allowed_layers = [Workflow, Thinking]` 且 `category = Execute`
- **THEN** 注册成功（Execute 在 Workflow 层允许，在 Thinking 层... 矩阵规定 Thinking ❌ Execute → 但 allowed_layers 是 "允许的层" 而非 "允许的操作"，由运行时 check_layer_permission 决定）
- **注**: allowed_layers 语义修正 — 它声明"这个工具可以在哪些层被调用"，运行时 `check_layer_permission` 检查调用方 layer 是否在 allowed_layers 中。矩阵检查在注册时验证 allowed_layers 不含完全非法的层

- **WHEN** 注册工具的 `allowed_layers = [Cognitive]` 且 `category = Network`
- **THEN** `std::invalid_argument` throw — Cognitive 层完全不运行 Network 工具

- **WHEN** 未设置 `allowed_layers`（空向量 = 全允许）
- **THEN** 注册成功，跳过矩阵检查

- **WHEN** 运行时 `check_layer_permission(caller_layer)` 被调用
- **THEN** 行为不变（C4 已实现），不修改

### Summary Table

| # | Requirement ID | Status | Priority | Test File |
|:-:|:--------------|:------:|:--------:|:----------|
| 1 | `toolregistry-v2-register-with-meta` | [ ] | P0 | `tests/test_tool_registry_v2.cpp` |
| 2 | `toolregistry-v2-validate-conflict` | [ ] | P0 | `tests/test_tool_registry_v2.cpp` |
| 3 | `pdk-declare-tool-v2-mandatory` | [ ] | P0 | `tests/test_pdk_macros_v2.cpp` |
| 4 | `pdk-declare-tool-v2-compile-check` | [ ] | P1 | `tests/test_pdk_macros_v2.cpp` |
| 5 | `approval-workflow-toolcoordinator-link` | [ ] | P1 | `tests/test_tool_coordinator.cpp` (增强) |
| 6 | `layer-matrix-registration-check` | [ ] | P0 | `tests/test_layer_profile_matrix.cpp` |