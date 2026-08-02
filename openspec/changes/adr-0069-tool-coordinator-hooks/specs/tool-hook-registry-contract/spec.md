# tool-hook-registry-contract Specification

## Purpose

定义 `ToolCoordinator` 工具调用链的 pre/post hook L3 契约。本规范覆盖 `IToolHookRegistry` 接口、`PreHook` / `PostHook` 语义、`HookErrorPolicy` 失败策略、priority 排序、`tool_glob` 匹配、hook 对工具参数与结果的修改能力、hook Deny 路径的审计事件，以及无 hook 时的向后兼容性。本契约是 ADR-0069 的实施规格，也是 PDK plugin 扩展 `ToolCoordinator` 行为的唯一注入点。

## ADDED Requirements

### Requirement: itool-hook-registry-provides-registration-interface

`IToolHookRegistry` SHALL expose pure virtual methods `register_pre_hook` and `register_post_hook`, each accepting a `tool_glob` pattern, a hook function, an integer `priority`, and a `HookErrorPolicy`.

#### Scenario: plugin registers pre-hook and post-hook for shell tools
- **WHEN** a PDK plugin calls `register_pre_hook("shell/*", pre_hook, 10, HookErrorPolicy::FailClosed)` and `register_post_hook("shell/*", post_hook, 20, HookErrorPolicy::FailOpen)`
- **THEN** the registry stores both hooks without throwing
- **AND** the hooks are associated with the `shell/*` pattern

### Requirement: hook-execution-order-follows-ascending-priority

Hooks SHALL execute in ascending order of `priority`; hooks with equal priority SHALL execute in registration order (FIFO).

#### Scenario: multiple pre-hooks with mixed priorities
- **GIVEN** three pre-hooks registered with priorities `5`, `5`, and `10`
- **WHEN** a tool call triggers the pre-hooks
- **THEN** the two priority-5 hooks run first in registration order, followed by the priority-10 hook

### Requirement: tool-glob-matches-tool-names-per-adr-0043

Each hook SHALL only execute for tool names matching its registered `tool_glob` pattern. The pattern language SHALL follow ADR-0043 PDK tool naming conventions (`*` matches any sequence within a segment; `shell/*` matches all tools under `shell`).

#### Scenario: shell glob matches shell tools only
- **GIVEN** a pre-hook registered with `tool_glob = "shell/*"`
- **WHEN** `shell/exec` is called
- **THEN** the pre-hook executes
- **WHEN** `fs/read` is called
- **THEN** the pre-hook does not execute

#### Scenario: wildcard glob matches all tools
- **GIVEN** a pre-hook registered with `tool_glob = "*"`
- **WHEN** any tool is called
- **THEN** the pre-hook executes

### Requirement: pre-hook-may-modify-args

A pre-hook returning `PreHookResult::Action::ModifyArgs` SHALL cause `ToolCoordinator` to replace the tool arguments with `modified_args` for all subsequent steps, including layer check, ApprovalHandler, audit event, and the actual tool invocation.

#### Scenario: pre-hook rewrites command argument
- **GIVEN** a pre-hook registered for `shell/exec` that returns `ModifyArgs` with `{"command":"echo safe"}`
- **WHEN** the caller invokes `shell/exec` with `{"command":"rm -rf /"}`
- **THEN** layer check and ApprovalHandler see `{"command":"echo safe"}`
- **AND** `tool.execution.start` payload contains `{"command":"echo safe"}`
- **AND** the tool implementation receives `{"command":"echo safe"}`

### Requirement: pre-hook-may-deny-and-emit-audit-denied

A pre-hook returning `PreHookResult::Action::Deny` SHALL stop the tool call before layer check and ApprovalHandler, return a `ToolResult::error`, and emit `tool.audit.denied` with a reason containing the hook name and `deny_reason`.

#### Scenario: compliance pre-hook denies dangerous tool
- **GIVEN** a pre-hook registered for `shell/*` with `HookErrorPolicy::FailClosed` that returns `Deny` with `deny_reason = "policy violation"`
- **WHEN** `shell/exec` is called
- **THEN** layer check and ApprovalHandler are skipped
- **AND** `ToolCoordinator::execute` returns an error `ToolResult`
- **AND** `tool.audit.denied` is emitted with reason containing the hook name and `"policy violation"`

### Requirement: hook-error-policy-fail-closed-treats-exception-as-deny

A hook that throws an exception and was registered with `HookErrorPolicy::FailClosed` SHALL be treated as a Deny for pre-hooks and as an error result for post-hooks, with the failure recorded in the audit event.

#### Scenario: fail-closed pre-hook throws
- **GIVEN** a pre-hook registered with `HookErrorPolicy::FailClosed` that throws `std::runtime_error("budget service down")`
- **WHEN** the hooked tool is called
- **THEN** the call is denied
- **AND** `tool.audit.denied` is emitted with reason containing the hook name and `"budget service down"`

#### Scenario: fail-closed post-hook throws
- **GIVEN** a post-hook registered with `HookErrorPolicy::FailClosed` that throws `std::runtime_error("sanitizer failed")`
- **WHEN** the hooked tool succeeds
- **THEN** `ToolCoordinator::execute` returns an error `ToolResult`
- **AND** `tool.audit.denied` is emitted with reason containing the hook name and `"sanitizer failed"`

### Requirement: hook-error-policy-fail-open-tolerates-exception

A hook that throws an exception and was registered with `HookErrorPolicy::FailOpen` SHALL be skipped, execution SHALL continue with the next hook or the remaining pipeline, and a warning SHALL be recorded in the audit meta.

#### Scenario: fail-open observation hook throws
- **GIVEN** a pre-hook registered with `HookErrorPolicy::FailOpen` that throws `std::runtime_error("metrics unavailable")`
- **WHEN** the hooked tool is called
- **THEN** the call continues through layer check, ApprovalHandler, and tool invocation
- **AND** the audit event meta contains a warning entry with the hook name and `"metrics unavailable"`

### Requirement: post-hook-may-modify-result-and-audit-payload

A post-hook returning `PostHookResult::modify_result = true` SHALL cause `ToolCoordinator` to replace the tool result for all downstream consumers, including `tool.execution.end` and `tool.audit.completed`.

#### Scenario: post-hook masks sensitive output
- **GIVEN** a post-hook registered for `shell/exec` that returns `modify_result = true` with `{"output":"[REDACTED]"}`
- **WHEN** `shell/exec` returns `{"output":"secret"}`
- **THEN** `tool.execution.end` payload contains `{"output":"[REDACTED]"}`
- **AND** `tool.audit.completed` payload contains `{"output":"[REDACTED]"}`
- **AND** `ToolCoordinator::execute` returns the redacted result

### Requirement: no-hook-path-preserves-backward-compatibility

When no `IToolHookRegistry` is set on `ToolCoordinator`, `execute()` SHALL behave byte-for-byte identically to the pre-change 6-step pipeline, including all layer checks, ApprovalHandler calls, audit events, and returned `ToolResult` values.

#### Scenario: tool coordinator without hook registry
- **GIVEN** a `ToolCoordinator` constructed with a valid registry, policy, callback, and bus but without calling `set_hook_registry`
- **WHEN** any tool call is executed
- **THEN** the call follows the original 6-step flow
- **AND** no hook matching, traversal, or failure policy logic is executed

### Requirement: budget-agent-plugin-demonstrates-pre-hook

A real PDK plugin SHALL use the pre-hook mechanism to deny tool calls when a budget limit is exceeded, demonstrating production usage of `IToolHookRegistry`.

#### Scenario: budget agent denies over-budget calls
- **GIVEN** the `budget_agent` plugin registers a pre-hook for `tool_glob = "*"` with `HookErrorPolicy::FailClosed`
- **AND** the current task session reports `budget_remaining <= 0`
- **WHEN** any tool is called
- **THEN** the pre-hook returns `Deny` with `deny_reason = "budget exceeded"`
- **AND** `ToolCoordinator::execute` returns an error `ToolResult`
- **AND** `tool.audit.denied` is emitted

#### Scenario: budget agent allows within-budget calls
- **GIVEN** the `budget_agent` plugin registers a pre-hook for `tool_glob = "*"` with `HookErrorPolicy::FailClosed`
- **AND** the current task session reports `budget_remaining > 0`
- **WHEN** any tool is called
- **THEN** the pre-hook returns `Continue`
- **AND** the call proceeds through layer check, ApprovalHandler, and tool invocation
