## Why

ADR-0070 明确界定 Command 与 Tool 的本质差异：Command 是 L4 用户输入层入口，不等同于 Tool，不能产生安全旁路。当前代码基线实证显示 `include/agenticdsl/pdk/` 下 `registerCommand` / `registerShortcut` 搜索为空；`examples/pdk_chat_demo/main.cpp` 输入循环仍硬编码 `exit` / `quit` 处理。三类命令性质分析（`/tree` 纯 UI、`/compact` 委托工具、`/fork` 混合）证明"命令即工具"是概念错位：纯 UI 命令不具备 ToolMetadata 语义，也不应走工具治理路径。

本 change 通过 `DECLARE_COMMAND` 宏、`ICommandRegistry` L3 契约、L1 `CommandRegistry` 实现及 pdk_chat_demo 输入循环改造，建立命令层统一分发模型，使 plugin 命令受控注册、内置 `/help` 自动生成、`/exit` 保留字不可覆盖，并确保委托工具类命令复用既有的 `ToolCoordinator::execute()` 治理路径（ToolCoordinator 内部包装 `IToolRegistry::call_tool`，因此 layer check / ApprovalHandler / ADR-0069 hooks 全程生效）。

## What Changes

- **新增** `include/agenticdsl/pdk/command_macros.h`：定义 `CommandSpec` / `CommandContext` / 错误包装 handler，并实现 `DECLARE_COMMAND` 宏（遵循 ADR-0021 P1-P6 模式，静态链接、Runtime 零感知）。
- **新增** `include/agenticdsl/contract/icommand_registry.h`：L3 契约接口，提供注册、解析、列举、冲突检测能力。
- **新增** L1 默认实现 `CommandRegistry`（位置由实现确定，建议 `src/common/pdk/command_registry.h` / `.cpp` 或 `src/common/tools/command_registry.cpp`）。
- **修改** `examples/pdk_chat_demo/main.cpp` 输入循环：消除硬编码 `exit` / `quit` 分支，将 `/` 前缀统一分发给 `ICommandRegistry`；非 `/` 开头输入保持原 `session.chat()` 行为。
- **新增** 内置 `/help` 命令：由 `CommandRegistry` 自动生成，输出全部已注册命令（内置 + plugin）的名称、描述、用法，无特权显示差异。
- **保留** `/exit` 作为保留字，禁止 plugin 注册同名命令。
- **新增** 1 个真实 plugin 命令 `/compact`（经 `IToolCoordinator::execute()` 调用 `session/compact` 工具），验证委托治理路径。
- **新增** pdk_chat_demo ToolCoordinator 接线：当前 demo 未实例化 `ToolCoordinator`（opt-in），必须通过 `DSLEngine::set_tool_coordinator()` 注入，命令层方可触发 layer check / ApprovalHandler / ADR-0069 hooks 治理路径。
- **修改** `CommandContext` 暴露 `IToolCoordinator*`（而非 `IToolRegistry&`），强制命令 handler 经治理路径执行工具调用，杜绝绕过。
- **不修改** `DECLARE_SHORTCUT` 实际触发逻辑（依赖终端 raw mode，defer 至 L4-2 异步 I/O 改造后，仅契约先行）。
- **不修改** `/tree` TUI 本体（属于 L4-6，依赖 L0-1 SessionManager）。
- **不修改** `command.invoked` 等事件发射（需先入 ADR-0068 Registry 再实施）。
- **不修改** CLI flag 重写（属于 L4-4 cxxopts）。
- **不修改** 将命令实现为 `ToolRegistry` 工具的概念错位设计。

## Capabilities

### New Capabilities
- `command-registry-contract`: `DECLARE_COMMAND` 宏 + `ICommandRegistry` L3 契约 + L1 `CommandRegistry` 实现，覆盖命令注册、解析、列举、冲突检测、保留字保护、委托工具治理路径。

### Modified Capabilities
- `pdk-chat-demo-input-loop`: `examples/pdk_chat_demo/main.cpp` 输入循环从硬编码 `exit` / `quit` 改为 `/` 前缀统一分发，内置 `/help` 自动生成，`/exit` 保留字保护，并接入 `/compact` 真实 plugin 命令验证治理路径；同步注入 `ToolCoordinator` 实例以启用 `ToolCoordinator::execute()` 治理路径。

## Impact

- **生产代码**:
  - `include/agenticdsl/pdk/command_macros.h`（新建）
  - `include/agenticdsl/contract/icommand_registry.h`（新建）
  - L1 `CommandRegistry` 实现文件（新建）
  - `examples/pdk_chat_demo/main.cpp`（输入循环改造 + ToolCoordinator 注入）
- **测试代码**:
  - 命令注册冲突测试（同名命令第二次注册返回 false 并含双方 `plugin_origin`）
  - `/help` 列举测试（内置 + plugin 命令完整输出）
  - 委托治理路径测试（`/compact` 经 `IToolCoordinator::execute("session/compact", ...)`，ToolCoordinator 层 / ApprovalHandler / ADR-0069 hooks 全程生效）
  - 治理路径覆盖测试（确认直接调用 `IToolRegistry::call_tool` 被命令层禁止）
- **API 兼容性**:
  - ✅ 非 `/` 开头输入行为不变，原样进入 `session.chat()`
  - ✅ `CommandContext` 新增 `IToolCoordinator*` 字段；旧版本如有暴露 `IToolRegistry&` 的设计需同步迁移
  - ✅ 纯 UI 命令不触碰工具层
- **依赖**:
  - ✅ 无新外部依赖（复用现有 `IToolCoordinator` / `IToolRegistry` / `ApprovalHandler` 治理路径）
- **文档**:
  - `ADR-0070` 状态可转 🟡 Partial（转 Approved 条件见其 §决策 7）
- **风险**:
  - 中 - 主要新增头文件 + 实现 + 示例改造 + ToolCoordinator 接线，无既有 API 签名破坏
  - 最大风险在于 plugin 命令命名冲突、治理路径正确性、与 `ToolCoordinator` opt-in 接线的完整性，由 3 类测试覆盖