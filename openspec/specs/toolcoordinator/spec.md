# toolcoordinator Specification

## Purpose
ADR-0031 C4 P3-P4 ToolCoordinator + Layer Profile (Oracle Option C, session `ses_0ed4408faffeLv8VfrC0s5PzW7`):P3 ToolCoordinator = standalone middleware 注入 NodeExecutor (非 IToolRegistry 装饰器,避免 9-method 中 7 个无意义 forwarder),内部委托 `ApprovalHandler::process_request()`,`tool_coordinator_ == nullptr` 时回退直接 `call_tool()` (C3 auto-approve 兼容),audit log via `IInteractionBus::emit("tool.audit.<phase>", ToolResult)` (invoked/completed/denied/timed_out, args 仅 key 不 value 防 secret 泄露);P4 LayerProfile 复用既有 enum (`src/common/policy/execution_policy.h:52` Workflow/Thinking/Cognitive),权限矩阵复刻 ADR-0004 §8,强制点 = execution time (返回 `ToolResult::error(ErrorCode::PermissionDenied)`)。
## Requirements
### Requirement: toolcoordinator-middleware (PLACEHOLDER)

`ToolCoordinator` MUST 包装所有 tool 调用 + 集成 IExecutionPolicy + ApprovalCoordinator

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: toolcoordinator-executor-integration (PLACEHOLDER)

`NodeExecutor` MUST 改用 `ToolCoordinator::call_tool_with_policy()` 替换直接 `call_tool`

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: toolcoordinator-audit-log (PLACEHOLDER)

工具调用 MUST 记录审计日志到 EventBus (tool.audit event)

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: layer-profile-three-tiers (PLACEHOLDER)

`Layer` MUST 含 Cognitive/Thinking/Workflow 三层

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: tool-metadata-v2-extensions (PLACEHOLDER)

`ToolMetadata` V2 MUST 扩展含 `allowed_layers` / `cost_estimate` / `timeout_ms` 字段

#### Scenario: TBD (待设计)

- **WHEN** TBD
- **THEN** TBD

### Requirement: toolcoordinator-activation-mode

`ToolCoordinator` MUST 默认为 opt-in middleware, 通过显式 API 激活.

#### Scenario: Default DSLEngine ctor does not auto-create ToolCoordinator

- **WHEN** 用户调用 `DSLEngine::from_markdown(...)` 或 `DSLEngine(initial_graphs)` 默认 ctor
- **THEN** `engine.get_tool_coordinator()` 返回 `nullptr`
- **AND** `NodeExecutor` 不经过 ToolCoordinator, 走原有 ApprovalHandler / direct call_tool 路径
- **AND** 不再产生 `LOG_WARN("both tool_coordinator_ and approval_handler_ are set, ...")`

#### Scenario: User explicitly activates ToolCoordinator via API

- **WHEN** 用户调用 `engine->set_tool_coordinator(std::make_unique<ToolCoordinator>(...))`
- **THEN** `engine.get_tool_coordinator()` 返回非 nullptr 指针
- **AND** `NodeExecutor` 经过 ToolCoordinator 路径 (audit + layer check + approval)

#### Scenario: Backward compatibility for tests using DSLEngine::register_tool

- **WHEN** 测试创建 DSLEngine + `engine->register_tool("name", lambda)` + 触发 `tool_call` 节点
- **THEN** 工具调用走原有 ApprovalHandler / direct 路径, 返回成功 (`result.success == true`)
- **AND** 适用于 `test_basic.cpp:123` 和 `test_no_llm.cpp:59` 等 pre-existing 用例

### Requirement: toolcoordinator-legacy-path-preserved

Sprint 15 opt-in 改造 MUST 不破坏 Sprint 13-14 之前的所有测试与功能.

#### Scenario: All 41 ctest pass without modification

- **WHEN** 运行 `cmake --build build && ctest --output-on-failure`
- **THEN** 41/41 PASS (100%)
- **AND** 0 测试需要修改源代码 (除新增 opt-in 路径本身的实现)

### Requirement: toolcoordinator-adapter-zero-impact

Sprint 14 C4 ship 的 `ToolCoordinator` 类本身 MUST 不修改 (impl + tests).

#### Scenario: ToolCoordinator impl + tests unchanged

- **WHEN** Sprint 15 改造 ship 后
- **THEN** `src/common/tools/tool_coordinator.{h,cpp}` 无任何文件变更
- **AND** `tests/test_tool_coordinator.cpp` 无任何测试用例变更
- **AND** `ToolCoordinator` 类的 public API 与 C4 ship 时完全一致

