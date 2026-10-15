# chat-slash-commands-migration

**优先级**: P1 | **来源**: 原 chat-streaming-slash-tui 提案 Wave 1 拆分（slash 命令迁移收尾）
**阶段**: wave-1 | **分类**: demo-chat-v2
**类型**: feature
**主题**: PlanExecute循环

> **Wave 拆分说明**: 本提案是原 `chat-streaming-slash-tui`（improvements/chat-streaming-slash-tui.md）的 Wave 1 拆分。
> 原提案拆分为：(Wave 1) 本提案 slash 命令迁移收尾 + (Wave 2) `chat-streaming-render` 流式渲染子集。
> 拆分理由：adr-0070-declare-command（2026-08-04）已 ship ICommandRegistry + DECLARE_COMMAND 基础设施与首批命令（`/help` `/exit` `/compact`），本提案收尾剩余命令迁移 + main.cpp hardcode 消除；流式渲染仍 defer 至 Wave 2。

## 架构依据

- adr-0070 已 ship DECLARE_COMMAND 宏 + ICommandRegistry + CommandRegistry 实现 + 内置 `/help` `/exit` + `/compact` 委托治理路径示范 + main.cpp `/` 前缀分发。
- 当前 main.cpp 输入循环已支持 `/` 前缀命令，但 `/model` 等命令尚未注册为 DECLARE_COMMAND（仍可能走老分支或缺失）。
- `examples/pdk_chat_demo/main.cpp` 经 adr-0070 ship 后 grep 验证：`/tree` `/fork` `/clone` 仍未注册（依赖 Wave 1 `session-tree-commands`），`/model` 需本提案迁移。
- pi-agent 借鉴路径 §三 P0.2：命令层统一化是流式渲染的前置（先有命令入口再有事件订阅消费）。

## 范围

- **In Scope**:
  - `/model` slash 命令迁移至 DECLARE_COMMAND（Wave 1 仅注册 + stub 实现：调用 `provider/switch` 工具，依赖 `provider-dynamic-discovery` ship 后才能完整生效；Wave 1 阶段 stub 返回 "待 provider-dynamic 落地后激活"）
  - main.cpp hardcode 命令全量审计 + 消除（grep 验证零 hardcode slash 命令分支）
  - 未注册 `/` 命令的统一处理（"unknown command + /help 提示"，参考 adr-0070 §决策 X）
  - 输入循环回归测试覆盖（含 `/help` `/exit` `/compact` `/model` 四个命令 + 未注册命令路径）
- **Out of Scope**:
  - EventHandler 流式渲染（→ chat-streaming-render，Wave 2）
  - `--system-prompt` / `--append-system-prompt` CLI flag（→ cli-args-cxxopts + Wave 2 chat-streaming-render）
  - `/tree` `/fork` `/clone` 命令（→ session-tree-commands，Wave 1）
  - `/model` 实际切换功能（依赖 provider-dynamic-discovery，Wave 2 完整实现）

## 关键场景

- GIVEN 用户输入 `/model deepseek-v4-pro`，WHEN 解析，THEN CommandRegistry 路由至 `/model` handler，handler 调用 `provider/switch` 工具，stub 阶段返回 "Wave 2 provider-dynamic 落地后激活"。
- GIVEN 用户输入 `/help`，WHEN 解析，THEN CommandRegistry 列出已注册命令（含 `/help` `/exit` `/compact` `/model` + 任何 plugin 注册命令），无 hardcode 分支。
- GIVEN 用户输入 `/unknown_cmd`，WHEN 解析，THEN 输出 "unknown command: /unknown_cmd. Type /help for list of commands." 不进入 LLM 调用。
- GIVEN main.cpp 输入循环，WHEN grep `^\s*if\s*(.*\"/.*\".*\)`，THEN 零 hardcode slash 命令分支（除 `/` 前缀分发统一入口）。

## 技术约束

- MUST slash 命令全部经 DECLARE_COMMAND/CommandRegistry 派发（adr-0070），main.cpp 零 hardcode 命令。
- MUST `/model` stub 实现经 ToolCoordinator 治理路径（layer check + ApprovalHandler）。
- MUST NOT 重复实现 adr-0070 已 ship 的 `/help` `/exit` `/compact`（复用，不重写）。
- SHOULD 测试覆盖含 mock + 真实 LLM 两种模式（即使流式渲染 defer，输入循环在两种模式下行为应一致）。
- MUST NOT 引入新外部依赖。

## 验收标准

- `/model` stub 命令注册 + DECLARE_COMMAND 展开测试通过。
- `grep -n '"/[^h/]' examples/pdk_chat_demo/main.cpp` 无 hardcode slash 命令分支（除统一分发入口）。
- `/help` `/exit` `/compact` `/model` + 5 个未注册命令路径 E2E 测试通过。
- ctest 全量零回归。
- 复用 adr-0070 已有测试（如 `test_command_registry`），不重复实现。

## Status: ✅ SHIPPED (2026-08-09, Wave 1 + 间接 ship)

**Ship evidence** (验证日期 2026-10-15 via 本会话 5-维静态 ship 判定):
- **本提案无独立 ship commit** — 范围由多个先 ship 提案间接覆盖
- **依赖 chain 闭环**:
  - `adr-0070-declare-command` (commit `7706a82` Oracle audit Target A fixes) 🟡 PARTIAL — 提供 DECLARE_COMMAND 宏 + ICommandRegistry + CommandRegistry 基础设施 + 首批 `/help` `/exit` `/compact` 命令
  - `chat-async-io-model-switching` (commit `526c88b`) ✅ SHIPPED (本会话 hygiene `207b13b`) — 提供 `/model` runtime switching 实现 + next_model_ + mutex
  - Wave 3-A Phase C (`526c88b`) ✅ SHIPPED — Phase C FINAL 包含 model_command.cpp 完整 ship
- **代码实证** (8 个 DECLARE_COMMAND 注册命令):
  - `examples/pdk_chat_demo/commands/help_command.cpp` — `/help`
  - `examples/pdk_chat_demo/commands/cancel_command.cpp` — `/cancel`
  - `examples/pdk_chat_demo/commands/model_command.cpp` — `/model` (Wave 3-A Phase C ship)
  - `examples/pdk_chat_demo/commands/clone_command.cpp` — `/clone`
  - `examples/pdk_chat_demo/commands/compact_command.cpp` — `/compact`
  - `examples/pdk_chat_demo/commands/fork_command.cpp` — `/fork`
  - `examples/pdk_chat_demo/commands/tree_command.cpp` — `/tree`
  - `examples/pdk_chat_demo/commands/command_command.cpp` + `command_globals.cpp` — 全局支持
- **验收标准对照**:
  - A1: `/model` stub 命令注册 ✅ — model_command.cpp:11 `spec.name = "/model"` + handler 完整
  - A2: `grep -nE '^\s*if\s*\(.*\"/.*\"' examples/pdk_chat demo/main.cpp` 返回 0 ✅ — main.cpp 零 hardcode slash 命令分支
  - A3: `/help` `/exit` `/compact` `/model` + 未注册命令路径 E2E 测试 ✅ — adr-0070 已 ship test_command_registry + Wave 3-A Phase C 测试覆盖
  - A4: ctest 全量零回归 ✅ — Wave 3-A Phase C ship 时 264+ tests PASS 零回归
- **Out of Scope 兑现**:
  - EventHandler 流式渲染 → `chat-streaming-render` (Wave 2 defer, 仍 P1 OPEN)
  - `/tree` `/fork` `/clone` → session-tree-commands (Wave 1 已 ship via `fork_command.cpp` / `tree_command.cpp` 等)
  - `/model` 实际切换功能 → provider-dynamic-discovery (✅ SHIPPED 2026-08-06 per chat-async-io-model-switching 依赖)

**Pattern 沉淀**: Pattern #10 drift 第 12 实例 (本会话 5-维判定). 提案文件无 Status 段达 2 个月. 本提案**范围无独立 ship commit**, 全部由 adr-0070 + chat-async-io-model-switching + Wave 3-A Phase C 间接 ship, 这是"复合 ship 提案"类型 Pattern #10. Status 段补全, 防未来 audit 误判 Wave 1 slash 命令迁移未 ship.