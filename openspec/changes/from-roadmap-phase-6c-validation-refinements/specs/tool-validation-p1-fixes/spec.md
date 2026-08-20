# tool-validation-p1-fixes Specification

## Purpose

修复 ADR-0073 D3 ToolCoordinator 4 步 sanitization pipeline 中 Oracle review 发现的 3 个 P1 语义缺口：Warn-mode coercion 值对工具不可见（P1#1）、不可转换输入在 Warn 模式下静默放行（P1#2）、enum 检查在 coercion 之前执行导致误杀合法 Warn 输入（P1#3）。同时修复 `emit_audit_denied` 硬编码空字符串导致 session_id/trace_id 丢失的问题，并将 `DECLARE_TOOL_V3` 默认值从 Strict 改为 Warn 与 ToolCoordinator 行为匹配。

## ADDED Requirements

### Requirement: Warn-Mode Coercion Visibility

Warn 模式下 coercion 转换后的值 SHALL be visible to downstream tools. The coerced value MUST be written back to the string-map correctly, preserving the JSON-transformed value rather than re-serializing the original string.

#### Scenario: Warn + integer enum value accepted after coerce

- GIVEN a tool schema with `type: integer` and enum `[1, 2, 3]`
- AND input is the string `"1"`
- AND validation mode is `Warn`
- WHEN ToolCoordinator processes the input through the 4-step pipeline
- THEN the enum check passes using the coerced integer value `1`
- AND the tool receives `1` (not the string `"1"`) in its input map

#### Scenario: Strict mode preserves boundary - coercion not applied

- GIVEN a tool schema with `type: integer`
- AND input is the string `"8080"`
- AND validation mode is `Strict`
- WHEN ToolCoordinator processes the input
- THEN the input is rejected before coercion is attempted
- AND no coercion warning is emitted

### Requirement: Uncoercible Input Warning Emission

Warn 模式下不可转换的输入 SHALL emit a stderr warning and emit a `tool.audit.denied` event with reason "coercion_failed". The input MUST NOT be silently passed through without warning.

#### Scenario: Warn + uncoercible string emits warning and audit event

- GIVEN a tool schema with `type: integer`
- AND input is the string `"abc"`
- AND validation mode is `Warn`
- WHEN ToolCoordinator processes the input through the coercion step
- THEN a warning is emitted to stderr describing the coercion failure
- AND a `tool.audit.denied` event is emitted with `reason: "coercion_failed"`
- AND the input is not silently passed through

#### Scenario: Strict mode rejects uncoercible input without warning

- GIVEN a tool schema with `type: integer`
- AND input is the string `"abc"`
- AND validation mode is `Strict`
- WHEN ToolCoordinator processes the input
- THEN the input is rejected
- AND no coercion warning is emitted (rejection takes precedence)

### Requirement: Enum Check Post-Coercion Retry

Enum 校验 MUST be performed after coercion completes. After a successful coercion, the enum validation SHALL be retried with the `retry_after_coerce` flag set.

#### Scenario: Enum check retries after successful coercion

- GIVEN a tool schema with `type: integer` and enum `[80, 443, 8080]`
- AND input is the string `"8080"`
- AND validation mode is `Warn`
- WHEN ToolCoordinator processes the input
- THEN coercion converts `"8080"` to integer `8080`
- AND enum check is retried after coercion
- AND the input passes enum validation

#### Scenario: Enum check fails after coercion when value not in enum

- GIVEN a tool schema with `type: integer` and enum `[80, 443]`
- AND input is the string `"8080"`
- AND validation mode is `Warn`
- WHEN ToolCoordinator processes the input
- THEN coercion converts `"8080"` to integer `8080`
- AND enum check fails because `8080` is not in `[80, 443]`
- AND `tool.audit.denied` is emitted with appropriate reason

### Requirement: Audit Event Session/Trace ID Inheritance

`emit_audit_denied` SHALL inherit the real `session_id` and `trace_id` from ctx. The function MUST NOT hardcode empty strings or "validation" as these values.

#### Scenario: Audit denied event carries real session_id and trace_id

- GIVEN a tool call is denied during validation
- AND the execution context has `session_id: "sess_abc123"` and `trace_id: "trace_xyz789"`
- WHEN `emit_audit_denied` is called
- THEN the emitted event contains `session_id: "sess_abc123"` and `trace_id: "trace_xyz789"`
- AND NOT hardcoded empty strings or "validation"

#### Scenario: Audit denied event handles missing context gracefully

- GIVEN a tool call is denied during validation
- AND the execution context has empty session_id and trace_id
- WHEN `emit_audit_denied` is called
- THEN the emitted event contains empty strings for session_id and trace_id
- AND no crash or undefined behavior occurs

### Requirement: DECLARE_TOOL_V3 Default Mode

`DECLARE_TOOL_V3` macro SHALL default to `ValidationMode::Warn`. This default SHALL match the default behavior of ToolCoordinator.

#### Scenario: DECLARE_TOOL_V3 defaults to Warn mode

- GIVEN a tool is declared using `DECLARE_TOOL_V3` without specifying validation mode
- WHEN the tool is registered
- THEN the default ValidationMode is `Warn`
- AND the tool behaves identically to ToolCoordinator with default Warn mode

#### Scenario: DECLARE_TOOL_V3 explicit Strict mode overrides default

- GIVEN a tool is declared using `DECLARE_TOOL_V3` with explicit `ValidationMode::Strict`
- WHEN the tool is registered
- THEN the tool uses `Strict` mode
- AND coercion warnings are not emitted (strict rejection only)
