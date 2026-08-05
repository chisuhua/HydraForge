# chat-slash-commands-migration

## Why

- adr-0070 已 ship DECLARE_COMMAND 宏 + ICommandRegistry + CommandRegistry 实现 + 内置 `/help` `/exit` + `/compact` 委托治理路径示范 + main.cpp `/` 前缀分发。
- 当前 main.cpp 输入循环已支持 `/` 前缀命令，但 `/model` 等命令尚未注册为 DECLARE_COMMAND（仍可能走老分支或缺失）。
- `examples/pdk_chat_demo/main.cpp` 经 adr-0070 ship 后 grep 验证：`/tree` `/fork` `/clone` 仍未注册（依赖 Wave 1 `session-tree-commands`），`/model` 需本提案迁移。
- pi-agent 借鉴路径 §三 P0.2：命令层统一化是流式渲染的前置（先有命令入口再有事件订阅消费）。

## What Changes

**In Scope**:

- `/model` slash 命令迁移至 DECLARE_COMMAND（Wave 1 仅注册 + stub 实现：调用 `provider/switch` 工具，依赖 `provider-dynamic-discovery` ship 后才能完整生效；Wave 1 阶段 stub 返回 "待 provider-dynamic 落地后激活"）
- main.cpp hardcode 命令全量审计 + 消除（grep 验证零 hardcode slash 命令分支）
- 未注册 `/` 命令的统一处理（"unknown command + /help 提示"，参考 adr-0070 §决策 X）
- 输入循环回归测试覆盖（含 `/help` `/exit` `/compact` `/model` 四个命令 + 未注册命令路径）
- **Out of Scope**:
- EventHandler 流式渲染（→ chat-streaming-render，Wave 2）
- `--system-prompt` / `--append-system-prompt` CLI flag（→ cli-args-cxxopts + Wave 2 chat-streaming-render）
- `/tree` `/fork` `/clone` 命令（→ session-tree-commands，Wave 1）
- `/model` 实际切换功能（依赖 provider-dynamic-discovery，Wave 2 完整实现）

### 关键场景

- GIVEN 用户输入 `/model deepseek-v4-pro`，WHEN 解析，THEN CommandRegistry 路由至 `/model` handler，handler 调用 `provider/switch` 工具，stub 阶段返回 "Wave 2 provider-dynamic 落地后激活"。
- GIVEN 用户输入 `/help`，WHEN 解析，THEN CommandRegistry 列出已注册命令（含 `/help` `/exit` `/compact` `/model` + 任何 plugin 注册命令），无 hardcode 分支。
- GIVEN 用户输入 `/unknown_cmd`，WHEN 解析，THEN 输出 "unknown command: /unknown_cmd. Type /help for list of commands." 不进入 LLM 调用。
- GIVEN main.cpp 输入循环，WHEN grep `^\s*if\s*(.*\"/.*\".*\)`，THEN 零 hardcode slash 命令分支（除 `/` 前缀分发统一入口）。

**Out of Scope**:

- (TBD)

## Capabilities

- MUST slash 命令全部经 DECLARE_COMMAND/CommandRegistry 派发（adr-0070），main.cpp 零 hardcode 命令。
- MUST `/model` stub 实现经 ToolCoordinator 治理路径（layer check + ApprovalHandler）。
- MUST NOT 重复实现 adr-0070 已 ship 的 `/help` `/exit` `/compact`（复用，不重写）。
- SHOULD 测试覆盖含 mock + 真实 LLM 两种模式（即使流式渲染 defer，输入循环在两种模式下行为应一致）。
- MUST NOT 引入新外部依赖。

## Impact

- MUST slash 命令全部经 DECLARE_COMMAND/CommandRegistry 派发（adr-0070），main.cpp 零 hardcode 命令。
- MUST `/model` stub 实现经 ToolCoordinator 治理路径（layer check + ApprovalHandler）。
- MUST NOT 重复实现 adr-0070 已 ship 的 `/help` `/exit` `/compact`（复用，不重写）。
- SHOULD 测试覆盖含 mock + 真实 LLM 两种模式（即使流式渲染 defer，输入循环在两种模式下行为应一致）。
- MUST NOT 引入新外部依赖。

## Acceptance

- `/model` stub 命令注册 + DECLARE_COMMAND 展开测试通过。
- `grep -n '"/[^h/]' examples/pdk_chat_demo/main.cpp` 无 hardcode slash 命令分支（除统一分发入口）。
- `/help` `/exit` `/compact` `/model` + 5 个未注册命令路径 E2E 测试通过。
- ctest 全量零回归。
- 复用 adr-0070 已有测试（如 `test_command_registry`），不重复实现。

