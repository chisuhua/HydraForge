# command-registry-contract Specification

## Purpose

定义 AgenticDSL L4 命令层与 L2 工具层的边界契约，规范 `DECLARE_COMMAND` 宏、`ICommandRegistry` L3 接口及 L1 `CommandRegistry` 默认实现的行为。确保 Command 作为用户输入入口不替代 Tool，不产生安全旁路；委托工具类 Command 必须经 `IToolCoordinator::execute()` 受治理路径（而非直接 `IToolRegistry::call_tool`）；纯 UI 类 Command 不触碰工具层。

## ADDED Requirements

### Requirement: declare-command-macro-expands-command-spec

`DECLARE_COMMAND` MUST 在编译期展开为一个 `CommandSpec` 静态实例，并自动填充 `plugin_origin` 与错误包装 handler，遵循 ADR-0021 P1-P6（静态链接、Runtime 零感知）。

#### Scenario: plugin 使用 DECLARE_COMMAND 注册 /compact
- **GIVEN** plugin 源文件包含 `DECLARE_COMMAND("/compact", "compact session", handler)`
- **WHEN** 编译并加载该 plugin
- **THEN** `CommandRegistry` 中可见 `CommandSpec` 条目，字段 `name="/compact"`、`description="compact session"`、`plugin_origin` 非空、`handler` 可调用

### Requirement: command-context-exposes-only-tool-coordinator

`CommandContext` MUST 仅暴露 `IToolCoordinator*` 指针供委托工具命令使用，**禁止**暴露 `IToolRegistry&`（反向调用会绕过 `ToolCoordinator` 的 layer check、`ApprovalHandler` 审批与 ADR-0069 hooks 治理路径）。

#### Scenario: CommandContext 字段集
- **GIVEN** `CommandContext` 结构体定义
- **WHEN** 检查其公开字段
- **THEN** 含 `IToolCoordinator* tool_coordinator` 字段
- **AND** **不得**含 `IToolRegistry&` 或 `IToolRegistry*` 字段

#### Scenario: handler 通过 IToolCoordinator 调用工具
- **GIVEN** `/compact` handler 接收 `CommandContext ctx`
- **WHEN** handler 调用工具
- **THEN** 必须经 `ctx.tool_coordinator->execute("session/compact", call_context)`
- **AND** 直接调用 `IToolRegistry::call_tool`（即使绕过）须被测试或静态检查禁止

### Requirement: icommand-registry-register-returns-conflict-bool

`ICommandRegistry::register_command` MUST 在成功时返回 true；在命名冲突、保留字冲突或元数据非法时返回 false，并附带诊断信息。

#### Scenario: 两个 plugin 注册同名命令
- **GIVEN** plugin A 已注册 `/compact`
- **WHEN** plugin B 再次注册 `/compact`
- **THEN** 第二次 `register_command` 返回 false，诊断信息包含 plugin A 与 plugin B 的 `plugin_origin`

### Requirement: exit-reserved-word-cannot-be-registered

`/exit` MUST 为保留字，任何 plugin 尝试注册 `/exit` 均返回 false，确保会话生命线命令不可被覆盖或劫持。

#### Scenario: plugin 尝试注册 /exit
- **GIVEN** plugin 声明 `DECLARE_COMMAND("/exit", "exit", handler)`
- **WHEN** 加载该 plugin 并调用注册
- **THEN** `register_command` 返回 false，诊断提示 `/exit` 为系统保留字

### Requirement: help-command-auto-generated-from-registry

`CommandRegistry` MUST 自动根据当前已注册命令生成内置 `/help` 输出，包含所有命令（内置 + plugin）的 name、description、usage，且不区分特权显示。

#### Scenario: 用户输入 /help
- **GIVEN** 已注册内置 `/help` 与 `/exit`，以及 plugin 命令 `/compact`
- **WHEN** 用户输入 `/help`
- **THEN** 输出列表包含 `/help`、`/exit`、`/compact` 三项，每项含 name、description、usage，无"builtin"或"plugin"标签差异

### Requirement: delegated-command-uses-tool-coordinator-execute

委托工具类命令 MUST 通过 `IToolCoordinator::execute()` 调用工具（ToolCoordinator 内部包装 `IToolRegistry::call_tool`），使 layer check、`ApprovalHandler` 审批、ADR-0069 hooks 等治理路径全程生效；命令自身不得直接调用 `IToolRegistry::call_tool` 绕过治理。

#### Scenario: 用户输入 /compact 经 ToolCoordinator 调用 session/compact
- **GIVEN** plugin 已注册 `/compact`，其 handler 经 `ctx.tool_coordinator->execute("session/compact", ...)` 调用
- **WHEN** 用户输入 `/compact`
- **THEN** `IToolCoordinator::execute()` 被调用一次，参数正确
- **AND** 内部 `IToolRegistry::call_tool` 被 ToolCoordinator 包装触发
- **AND** `ToolCoordinator` layer check、`ApprovalHandler`、ADR-0069 hooks 全程生效

#### Scenario: 治理路径覆盖测试
- **GIVEN** 一个错误 handler 尝试绕过 `ToolCoordinator`，直接调用 `IToolRegistry::call_tool`
- **WHEN** 执行该 handler
- **THEN** 测试断言失败（编译期禁止或运行期检测拒绝）
- **AND** 治理链路（layer check / ApprovalHandler / hooks）必须被触发才视为合规

### Requirement: unknown-command-returns-friendly-error-and-hint

当用户输入未注册的 `/` 前缀命令时，系统 MUST 返回友好错误并提示 `/help`。

#### Scenario: 用户输入 /foo
- **GIVEN** `/foo` 未在 `CommandRegistry` 中注册
- **WHEN** 用户输入 `/foo`
- **THEN** 返回错误信息 `"Unknown command /foo. Type /help for available commands."` 或语义等价文本

### Requirement: non-slash-input-passthrough-to-chat

非 `/` 开头的输入 MUST 原样进入 `session.chat()`，行为不变，不被命令层拦截或修改。

#### Scenario: 用户输入普通聊天文本
- **GIVEN** 用户输入 `"hello world"`
- **WHEN** 命令分发层检查该输入
- **THEN** 不调用 `ICommandRegistry::resolve_command`，直接进入 `session.chat()` 处理

### Requirement: pdk-chat-demo-input-loop-uses-command-registry

`examples/pdk_chat_demo/main.cpp` 的输入循环 MUST 移除硬编码 `exit` / `quit` 分支，改为统一 `/` 前缀分发，并接入 `CommandRegistry`。

#### Scenario: demo 启动后加载 plugin 命令 /compact
- **GIVEN** pdk_chat_demo 启动并加载 `/compact` plugin
- **WHEN** 用户输入 `/compact`
- **THEN** 输入循环调用 `CommandRegistry::resolve_command("/compact")` 并执行其 handler，不再走任何硬编码分支

### Requirement: pdk-chat-demo-injects-tool-coordinator

`examples/pdk_chat_demo/main.cpp` MUST 实例化 `ToolCoordinator` 并通过 `DSLEngine::set_tool_coordinator()` 注入，确保命令治理路径可达（demo 默认未实例化 `ToolCoordinator`）。

#### Scenario: demo 启动时 ToolCoordinator 已注入
- **GIVEN** pdk_chat_demo 启动
- **WHEN** 创建 `DSLEngine` 后执行 `engine.set_tool_coordinator(make_unique<ToolCoordinator>())`
- **THEN** `engine.tool_coordinator() != nullptr`（若 DSLEngine 暴露该 getter）
- **AND** 任何委托工具命令经 `CommandContext::tool_coordinator` 调用时，治理路径可达

#### Scenario: 缺失 ToolCoordinator 导致治理路径空跑（编译期/启动期断言）
- **GIVEN** pdk_chat_demo 未注入 `ToolCoordinator`
- **WHEN** 启动 demo
- **THEN** 启动失败或测试断言失败（治理路径不可达）

### Requirement: command-listing-excludes-tool-registry-entries

`ICommandRegistry::list_commands` MUST 只返回命令层条目，不得将 `ToolRegistry` 中的工具混入命令列表，避免概念错位。

#### Scenario: 系统已注册 5 个工具和 2 个命令
- **GIVEN** `ToolRegistry` 中有 5 个工具，`CommandRegistry` 中有 2 个命令
- **WHEN** 调用 `list_commands`
- **THEN** 返回的列表大小为 2，仅含命令，不含任何工具名