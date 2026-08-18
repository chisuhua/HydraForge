# adr-0070-declare-command

**优先级**: P0 | **来源**: ADR-0070（D4 立项, 2026-07-31）+ layer-based-missing-capabilities-analysis.md §三 X3 / §七 L3-3 + active-status Wave 1 #3
**阶段**: wave-1 | **分类**: pdk-contract
**类型**: feature
**主题**: PlanExecute循环；ForkJoin循环

## 架构依据
- ADR-0070 全文：概念界定（Command = L4 用户输入层入口 ≠ Tool，不产生安全旁路）、`DECLARE_COMMAND` 宏（CommandSpec + CommandContext + 错误包装 handler，DECLARE_TOOL 同模式）、`ICommandRegistry` L3 契约（注册/解析/列举/冲突）、`DECLARE_SHORTCUT` 契约（定义 + raw mode defer）、内置 `/help` `/exit`、边界条款（ADR-0021 P1-P6 / ADR-0043 双命名空间 / ADR-0068 事件）。
- 实证基线：`grep -rn "registerCommand|registerShortcut" include/agenticdsl/pdk/` 返回空；`main.cpp:388` 输入循环仅硬编码 `exit`/`quit`；三类命令性质分析（`/tree` 纯 UI、`/compact` 委托工具、`/fork` 混合）证明"命令即工具"为概念错位。

## 范围
- **In Scope**: `include/agenticdsl/pdk/command_macros.h`（CommandSpec/CommandContext/DECLARE_COMMAND）；`include/agenticdsl/contract/icommand_registry.h`（L3 契约）；L1 默认实现 `CommandRegistry`；pdk_chat_demo 输入循环 `/` 前缀分发改造（main.cpp 硬编码命令清零）；内置 `/help`（registry 自动生成）+ `/exit` 保留字；1 个真实 plugin 命令（建议 `/compact` 委托 `session/compact`，验证治理路径）；3 类测试（注册冲突 / `/help` 列举 / 委托工具调用经 ToolCoordinator）。
- **Out Scope**: DECLARE_SHORTCUT 实际触发（依赖终端 raw mode，defer 至 L4-2 异步 I/O 改造后，契约先行）；`/tree` TUI 本体（L4-6，依赖 L0-1 SessionManager）；`command.invoked` 等事件发射（需先入 ADR-0068 Registry 再实施）；CLI flag 重写（L4-4 cxxopts）。

## 关键场景
- GIVEN plugin 注册 `/compact` 命令，WHEN 用户输入 `/compact`，THEN handler 经 `IToolRegistry::call_tool("session/compact", ...)` 委托，ToolCoordinator layer check / ApprovalHandler / ADR-0069 hooks 全程生效。
- GIVEN 两个 plugin 注册同名命令，WHEN 第二次注册，THEN 返回 false 且诊断信息含双方 plugin_origin。
- GIVEN 用户输入 `/help`，THEN 输出全部已注册命令（内置 + plugin，名称 + description + usage），无特权显示差异。
- GIVEN 用户输入未注册的 `/foo`，THEN 返回友好错误并提示 `/help`；WHEN 输入不以 `/` 开头，THEN 原样进入 `session.chat()`（行为不变）。

## 技术约束
- MUST 命令不产生安全旁路：能力调用一律经 `IToolRegistry::call_tool` 受治理路径；纯 UI 命令不触碰工具层。
- MUST 宏模式遵循 ADR-0021 P1-P6（P3 静态链接，Runtime 零感知）；MUST `/exit` 为保留字不可被 plugin 注册。
- MUST NOT 将命令实现为 ToolRegistry 工具（概念错位：纯 UI 命令无 ToolMetadata 语义）；MUST NOT 在本期实现 shortcut 触发（raw mode defer）。
- SHOULD 契约头文件 / L1 实现 / demo 接入分 commit；SHOULD `/compact` 作为首个真实 plugin 命令。

## 验收标准
- 3 类测试全部通过（注册冲突 / `/help` 列举 / 委托治理路径）。
- pdk_chat_demo main.cpp 输入循环硬编码命令清零，`/` 前缀统一分发。
- 1 个真实 plugin 命令落地（`/compact`）。
- ctest 全量零回归；`python3 tools/adr_lint.py` 0 错误；`python3 tools/docs_drift_audit.py` 0 DRIFT。
- ADR-0070 状态可转 🟡 Partial（转 Approved 条件见其 §决策 7）。
