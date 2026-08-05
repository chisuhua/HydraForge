# chat-slash-commands-migration Specification

## Purpose

定义 `pdk_chat_demo` CLI 中 `/model` slash 命令的统一注册契约、main.cpp 零 hardcode 命令分支约束，以及未注册 `/` 命令的统一错误处理行为。本 spec 在 `adr-0070-declare-command` (shipped 2026-08-04) 已提供 `ICommandRegistry` + `DECLARE_COMMAND` 宏 + `/help` `/exit` `/compact` 示范的前提下，迁移 `/model` 主线命令、清理残留 hardcode 分支，禁止引入未 ship 的 provider 切换完整实现（依赖 Wave 2 `provider-dynamic-discovery`）。

## ADDED Requirements

### Requirement: model-command-registered-via-declare-command

`/model` slash 命令 MUST 通过 `DECLARE_COMMAND` 宏在 `examples/pdk_chat_demo/commands/` 下注册，命令体调用 `ToolCoordinator::call_tool("provider_switch_stub", args)`。Wave 1 阶段命令体 MUST 返回 stub 文案明确告知用户依赖 `provider-dynamic-discovery` 未 ship，禁止直接调用未注册的工具或 LLM。

#### Scenario: /model 无 provider 参数
- **WHEN** 用户输入 `/model`（无参数）
- **THEN** 输出 usage hint：`usage: /model <provider_name> (Wave 1 stub - provider switch pending provider-dynamic-discovery)`
- **AND** 不进入 LLM 调用

#### Scenario: /model 指定 provider
- **WHEN** 用户输入 `/model deepseek-v4-pro`
- **THEN** `ToolCoordinator::route("provider_switch_stub", {provider_name: "deepseek-v4-pro"})` 被调用
- **AND** 返回 `[Wave 1 stub] provider switch 将在 provider-dynamic-discovery 落地后激活 (TBD: provider/switch tool 注册 + 配置持久化)`
- **AND** ToolResult.ok=true + data.message 含上述文案

#### Scenario: /help 列出 /model
- **WHEN** 用户输入 `/help`
- **THEN** 输出 `* /model - Switch LLM provider (Wave 1 stub)` 一行
- **AND** 列表中含 adr-0070 ship 三个命令与 `/model`，共 4 个内置命令（不含 sister change `session-tree-commands` 注册的 `/tree /fork /clone`，这些在其各自 ship 后自动出现）

#### Scenario: Cognitive layer 拒绝 /model
- **GIVEN** 当前 layer = Cognitive
- **WHEN** `/model` 命令体调用 ToolCoordinator
- **THEN** ToolResult.ok=false + error_code=PermissionDenied
- **AND** 输出错误：`Layer Cognitive not permitted for provider_switch_stub; use --layer override at startup`
- **AND** SessionState 与 provider 配置零变化

### Requirement: main-cpp-zero-hardcode-slash-branches

`examples/pdk_chat_demo/main.cpp` 的输入循环 MUST 零 hardcode slash 命令分支。唯 MUST 例外为 `/` 前缀分发统一入口（如 `if (input.starts_with("/")) { return CommandRegistry::route(input); }`），其余条件分支或字符串比较 MUST 消除。

#### Scenario: grep 验证零 hardcode
- **WHEN** 运行 `grep -nE '"\/(help|exit|compact|model|tree|fork|clone)' examples/pdk_chat_demo/main.cpp`
- **THEN** 仅匹配统一分发入口本身（1 行），非入口位置 0 匹配

#### Scenario: 测试化 grep 验证
- **WHEN** 运行 `cmake --build build --target test_main_hardcode_audit && ctest -R test_main_hardcode_audit`
- **THEN** 退出码 0
- **AND** 测试内部 `std::system("grep -nE ...")` 返回 1（grep 无匹配）

#### Scenario: 残留 hardcode 检测回归
- **GIVEN** main.cpp 含有残留 `if (input.starts_with("/model")) { ... }`
- **WHEN** 编译并运行 test_main_hardcode_audit
- **THEN** 测试 catch2 REQUIRE 断言失败（要求 grep 零匹配）
- **AND** 测试退出码非 0
- **AND** CI 阻断合并

### Requirement: unknown-command-unified-handler

未注册的 `/` 命令 MUST 由 `CommandRegistry` 统一返回 `UnknownCommand` 错误，main.cpp 错误处理路径 MUST 输出 `unknown command: /<name>. Type /help for list of commands.` 并 **不** 调用 LLM（避免 LLM 兜底编造命令响应）。

#### Scenario: /unknown1 未注册命令处理
- **WHEN** 用户输入 `/unknown1`
- **THEN** CommandRegistry::route 返回 UnknownCommand
- **AND** main.cpp 输出 `unknown command: /unknown1. Type /help for list of commands.`
- **AND** LLM 调用次数 = 0（mock LLM 验证计数器）

#### Scenario: 5 个未注册命令路径
- **WHEN** 依次输入 `/unknown1` `/foo_bar` `/baz` `/qux` `/123abc`
- **THEN** 每条各打印 `unknown command: /<name>. Type /help for list of commands.`
- **AND** 输入循环继续，不退出

#### Scenario: 错误信息防御性
- **WHEN** 用户输入 `/evil; rm -rf /`
- **THEN** main.cpp 提取命令名为 `evil`（去除 `/` 后第一个 token）
- **AND** 错误输出 `/evil; rm -rf /` 的原始 prefix **不** 出现在任何 log / output 中
- **AND** 防御性日志：仅记录结构化 `unknown_command name=evil timestamp=...`

### Requirement: tool-stub-tool-coordinator-governance

`provider_switch_stub` 工具 MUST 在 `ToolCoordinator` 注册，`ToolMetadata.allowed_layers = {Workflow}`。Cognitive 与 Thinking layer 调用 MUST 被拒绝返回 `ToolResult.ok=false`。

#### Scenario: ToolRegistry 注册校验
- **WHEN** `register_provider_switch_stub_tool(tool_registry)` 执行
- **THEN** ToolMetadata 含 `allowed_layers: [Workflow]` + `category: Workflow` + `approval_policy: agent`
- **AND** 重复注册抛 `ToolAlreadyRegistered`

#### Scenario: layer check 拒绝 Cognitive
- **GIVEN** layer = Cognitive
- **WHEN** ToolCoordinator::call_tool("provider_switch_stub", args)
- **THEN** ToolResult.ok=false + error_code=PermissionDenied
- **AND** 不触发任何 provider 切换逻辑

#### Scenario: layer check 拒绝 Thinking
- **GIVEN** layer = Thinking
- **WHEN** ToolCoordinator::call_tool("provider_switch_stub", args)
- **THEN** ToolResult.ok=false + error_code=PermissionDenied（按 ADR-0004 §8 矩阵，Thinking layer 不允许 `provider/switch` 类工作流工具）

#### Scenario: Workflow layer 正常调用
- **GIVEN** layer = Workflow
- **WHEN** ToolCoordinator::call_tool("provider_switch_stub", args)
- **THEN** ToolResult.ok=true + data.message 含 stub 文案
- **AND** SessionManager / provider config 零变化（纯文本返回）

### Requirement: regression-coverage-input-loop

输入循环的回归测试 MUST 覆盖内置命令 `/help` `/exit` `/compact` `/model` 与 ≥ 5 个未注册命令路径，验证 mock LLM 调用次数仅在合法命令（`/help`, `/exit` + 用户正常消息）触发，`/compact`, `/model`, 未注册命令均零 LLM 调用。

#### Scenario: 顺序命令输入 + mock LLM
- **GIVEN** mock LLM + 真实 CommandRegistry
- **WHEN** 顺序输入 `/help` `/exit` `/compact` `/model test` `/u1` `/u2` `/u3` `/u4` `/u5`
- **THEN** mock LLM 调用次数 = 1（`/help` 间接通过，但实际 adr-0070 实现是纯命令派发零 LLM；最终断言 = 0 LLM 调用更严格）
- **AND** `/exit` 触发循环退出
- **AND** `/compact` 调用 session/compact tool 返回 ok
- **AND** `/model test` 输出 stub 文案
- **AND** 5 个未注册命令各打印 unknown 错误

#### Scenario: 真 LLM 模式 (--no-mock) 命令派发与 mock 一致
- **GIVEN** DeepSeek cloud provider + 真实 LLM network
- **WHEN** 运行 `pdk_chat_demo --no-mock` + 输入 `/help` `/model test` `/u1`
- **THEN** 命令派发行为与 mock 模式一致（命令处理逻辑无 mock 依赖）
- **AND** 真实 LLM 调用计数仅在用户实际非 slash 消息时递增
- **AND** `/u1` 错误处理与 mock 模式输出相同文案

### Requirement: no-new-external-dependency

本 change MUST NOT 引入新外部依赖（无新增 CMake `find_package`、无 FetchContent、无 vcpkg/Conan 包条目）。Wave 1 stub 的文案与日志输出 MUST 使用现有 `agenticdsl::log` 宏（已 ship 自 `adr-0068`）。

#### Scenario: CMakeLists.txt 零依赖新增
- **WHEN** 运行 `git diff main -- examples/pdk_chat_demo/CMakeLists.txt`
- **THEN** diff 仅含源文件追加（`commands/model_command.cpp`）与 ToolCoordinator 列表更新
- **AND** 0 行 `find_package` / `FetchContent` / `add_subdirectory` 新增

#### Scenario: build 零新警告
- **WHEN** 运行 `cmake --build build --target pdk_chat_demo 2>&1 | grep -i warning`
- **THEN** 输出仅含 pre-existing 项目级警告，无新增 `unused-variable` / `unused-parameter` / `missing-include` 警告
