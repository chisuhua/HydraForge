## Context

ADR-0070 立项（2026-07-31 D4）指出 AgenticDSL 的 L4 用户输入层缺少命令层抽象。当前基线显示：

1. `grep -rn "registerCommand\|registerShortcut" include/agenticdsl/pdk/` 返回空，说明命令注册机制完全不存在。
2. `examples/pdk_chat_demo/main.cpp` 输入循环仅在 `~389` 行硬编码处理 `exit` / `quit`，无统一分发，plugin 无法扩展命令。
3. 命令分三类性质：
   - 纯 UI 命令（如 `/tree`）：只影响展示，不触发工具层；
   - 委托工具命令（如 `/compact`）：将用户输入翻译为一次受治理的工具调用；
   - 混合命令（如 `/fork`）：兼具 UI 与工具调用。
   若把命令简单实现为 `ToolRegistry` 工具，纯 UI 命令会被迫携带无意义的 `ToolMetadata` 与审批语义，产生概念错位，并可能为安全旁路打开缺口。

本 change 在 L3 引入 `ICommandRegistry` 契约，在 L4 提供 `DECLARE_COMMAND` 宏，在 L1 提供默认 `CommandRegistry` 实现，并在 pdk_chat_demo 中替换硬编码输入循环，为后续 `DECLARE_SHORTCUT` 与 `/tree` TUI 奠定命令层基础。

**关键架构事实**：`ToolCoordinator::execute()` 内部包装 `IToolRegistry::call_tool()`（正向调用），反向不成立——命令 handler 若直接持有 `IToolRegistry&` 并调用 `call_tool`，将完全绕过 `ToolCoordinator` 的 layer check、`ApprovalHandler` 的审批与 ADR-0069 的 hooks。`CommandContext` 必须暴露 `IToolCoordinator*`（而非 `IToolRegistry&`），强制委托工具命令经治理路径执行。此外 `pdk_chat_demo` 当前未实例化 `ToolCoordinator`（opt-in 模式，通过 `DSLEngine::set_tool_coordinator()` 注入），必须在本 change 内同步接线，否则治理路径不可达。

## Goals / Non-Goals

**Goals:**
- 建立 `Command` 与 `Tool` 的清晰概念边界：Command 是 L4 入口，Tool 是 L2 能力单元。
- 实现 `DECLARE_COMMAND` 宏，提供 `CommandSpec` + `CommandContext` + 错误包装 handler，模式遵循 ADR-0021 P1-P6。
- 实现 `ICommandRegistry` L3 契约（注册、解析、列举、冲突检测）。
- 提供 L1 默认 `CommandRegistry` 实现，内置 `/help` 自动生成、`/exit` 保留字保护。
- 改造 `examples/pdk_chat_demo/main.cpp` 输入循环，将 `/` 前缀统一分发给 registry，非 `/` 开头保持 `session.chat()` 行为。
- 在 pdk_chat_demo 中实例化并注入 `ToolCoordinator`，启用 `IToolCoordinator*` 治理路径。
- 落地 1 个真实 plugin 命令 `/compact` 委托 `session/compact` 工具，验证 `IToolCoordinator::execute()` 治理路径全程生效。
- 通过 3 类测试：注册冲突、`/help` 列举、委托治理路径（经 ToolCoordinator）。
- 保持 ctest 全量零回归，`tools/adr_lint.py` 0 错误，`tools/docs_drift_audit.py` 0 DRIFT。

**Non-Goals:**
- 不实现 `DECLARE_SHORTCUT` 实际触发（依赖终端 raw mode，仅契约先行，defer 至 L4-2）。
- 不实现 `/tree` TUI 本体（属于 L4-6，依赖 L0-1 SessionManager）。
- 不发射 `command.invoked` 等事件（需先入 ADR-0068 Registry）。
- 不重构 CLI flag 解析（L4-4 cxxopts）。
- 不将命令实现为 `ToolRegistry` 工具。
- 不修改 `ToolRegistry` 审批矩阵或 ADR-0004 §8 的 Layer×Category 语义。
- 不替代现有 `pdk_chat_demo` 中 `exit` / `quit` 行为以外的硬编码逻辑（如版本号打印等）。

## Decisions

### Decision 1: Command 与 Tool 严格区分

**Rationale**:
- Command 是 L4 用户输入入口，Tool 是 L2 可执行能力单元；二者处于不同架构层。
- 纯 UI 命令（如 `/tree`）不触碰工具层，不应携带 `ToolMetadata` 与审批语义。
- 委托工具命令（如 `/compact`）必须经 `IToolCoordinator::execute()` 调用 `session/compact`，复用既有治理路径（ToolCoordinator 内部包装 `IToolRegistry::call_tool`，自然继承 layer check、`ApprovalHandler` 审批、ADR-0069 hooks），不产生安全旁路。
- 若将命令实现为工具，纯 UI 命令会产生"空审批"或"无意义审批"的歧义，并为未来安全旁路埋下隐患。

**Alternatives Considered**:
- 将命令实现为 `ToolRegistry` 工具：被 ADR-0070 拒绝，概念错位且安全旁路风险高。
- 命令完全独立执行体系：不重用 `IToolRegistry`，会导致治理路径分叉，审计与审批无法统一。

### Decision 2: `DECLARE_COMMAND` 宏遵循 ADR-0021 P1-P6 模式

**Rationale**:
- PDK 插件需静态链接注册命令，宏在编译期展开 `CommandSpec` 与 `plugin_origin` 元数据。
- 与 `DECLARE_TOOL` 模式对称，降低 PDK 作者学习成本。
- Runtime 零感知：注册发生在 static initialization 阶段，无需运行时 reflection。

**Alternatives Considered**:
- 运行时 JSON 注册：破坏静态链接与类型安全，增加运行时解析失败面。
- 手写 `CommandSpec` 工厂： boilerplate 高，容易遗漏 `plugin_origin` 或错误包装 handler。

### Decision 3: `/exit` 为保留字，禁止 plugin 注册

**Rationale**:
- `/exit` 是终端会话的生命线命令，必须确保任何 plugin 都无法覆盖或劫持。
- 在 `CommandRegistry::register_command` 中硬编码保留字检查，返回 false 并附带诊断信息。

**Alternatives Considered**:
- 仅在 pdk_chat_demo 中硬编码 `/exit`：缺乏 L1 层面的统一保护，其他入口（如 future TUI）可能重复出错。
- 保留字列表配置化：过度设计，当前仅 `/exit` 一个保留字；若未来增加，再扩展为集合。

### Decision 4: `DECLARE_SHORTCUT` 契约先行，实际触发 defer

**Rationale**:
- 终端 raw mode 捕获全局快捷键（如 Ctrl+R）需要 L4-2 异步 I/O 改造，超出本 change 范围。
- 先定义接口与宏，确保后续实现时 plugin 无需修改注册代码。
- 避免在本期引入尚未就绪的终端控制逻辑，导致范围膨胀。

**Alternatives Considered**:
- 本期直接实现 raw mode 触发：需要跨平台终端处理，估时 2-3 周，阻塞 Wave 1 P0 主路径。
- 完全不做 `DECLARE_SHORTCUT`：后续 shortcut 改造缺乏契约基础，plugin 作者需二次迁移。

### Decision 5: 委托工具命令经 `IToolCoordinator::execute()` 受治理路径

**Rationale**:
- **架构关键事实**：`ToolCoordinator::execute()` 内部包装 `IToolRegistry::call_tool()`（正向调用）；反向不成立。命令 handler 若持有 `IToolRegistry&` 直接调用 `call_tool`，将完全绕过 layer check、`ApprovalHandler` 审批、ADR-0069 hooks。
- 因此 `CommandContext` MUST 暴露 `IToolCoordinator*`（而非 `IToolRegistry&`），强制命令 handler 经 `ToolCoordinator::execute()` 调用工具。
- 自然继承 layer check、`ApprovalHandler` 审批、ADR-0069 hooks 全程生效，不产生安全旁路。
- `CommandContext` 暴露必要字段（`IToolCoordinator*`、当前 session、user_input、LayeredContext 片段），由 handler 组装为 `ToolCallContext` 后调用。

**Alternatives Considered**:
- 命令直接调用 `ToolRegistry` 内部函数：绕过 `ToolCoordinator` 与 `ApprovalHandler`，破坏 ADR-0031 与 ADR-0004 的治理层级——**架构错误**，本 change 强制禁止。
- `CommandContext` 暴露 `IToolRegistry&`：等同上一条，绕过治理路径，**不允许**。
- 命令层独立审批逻辑：重复实现，且与工具层审批不一致，导致 UX 碎片化。

### Decision 6: pdk_chat_demo 同步注入 `ToolCoordinator`

**Rationale**:
- 当前 `pdk_chat_demo` 未实例化 `ToolCoordinator`（opt-in 模式，通过 `DSLEngine::set_tool_coordinator()` 注入）；若不在本 change 内同步接线，命令治理路径不可达，所有测试断言失败。
- 注入位置：`main.cpp` 在创建 `DSLEngine` 后立即构造 `ToolCoordinator` 并调用 `engine.set_tool_coordinator(make_unique<ToolCoordinator>())`。
- 接线后 `/compact` 等委托命令可经 `ToolCoordinator::execute()` 触发 layer check 等治理路径。

**Alternatives Considered**:
- 假设 `ToolCoordinator` 已存在（实际不存在）：测试与 demo 行为不一致。
- 在 `CommandRegistry` 内部自动创建 `ToolCoordinator`：违反 opt-in 设计原则，强制所有命令 demo 携带治理路径开销。

## Risks / Trade-offs

### Risk 1: Plugin 命令命名冲突导致 UX 断裂

**Mitigation**:
- `ICommandRegistry::register_command` 强制返回 bool，第二次同名注册返回 false 并包含双方 `plugin_origin` 诊断。
- 测试覆盖冲突场景，确保不会静默覆盖。

### Risk 2: 输入循环改造引入非 `/` 前缀行为回归

**Mitigation**:
- 非 `/` 开头输入原样进入 `session.chat()`，不得修改；测试中显式覆盖该路径。
- `/` 前缀若未匹配命令，返回友好错误并提示 `/help`。

### Risk 3: 委托工具命令绕过治理路径

**Mitigation**:
- `CommandContext` MUST 仅暴露 `IToolCoordinator*`，编译期与运行期双重禁止直接 `IToolRegistry::call_tool`。
- 治理路径覆盖测试：验证 `/compact` handler 必须调用 `IToolCoordinator::execute()` 而非 `IToolRegistry::call_tool`；若误用后者，测试断言失败。
- 不引入新的审批语义，完全复用 ADR-0004 V2 的 `ToolMetadata` 与 `ApprovalHandler`。

### Risk 4: pdk_chat_demo 未注入 `ToolCoordinator` 导致治理路径空跑

**Mitigation**:
- 在 demo 启动序列中强制注入 `ToolCoordinator`（参见 Decision 6）。
- 启动断言：`engine.tool_coordinator() != nullptr`（假设 `DSLEngine` 暴露该 getter；若无，须扩展 DSLEngine 公开 accessor）。
- 集成测试：模拟 `/compact` 触发 layer check / ApprovalHandler 调用链。

### Risk 5: CommandRegistry 与现有 ToolRegistry 头文件循环 include

**Mitigation**:
- `ICommandRegistry` 仅依赖 `CommandSpec` / `CommandContext` 等值类型，不直接 include `ToolRegistry` 完整定义。
- 委托路径在 `.cpp` 中通过 `IToolCoordinator` 前向声明 + 构造函数注入接口指针解决。

### Trade-off 1: L1 CommandRegistry 默认实现 vs 强制每个 plugin 自带 registry

**Trade-off**: 提供默认实现降低 plugin 接入成本，但会引入一个中心注册点。
**Decision**: 接受，L1 默认实现遵循 AgenticDSL 的"可替换但默认可用"原则；L3 契约允许未来替换为事件总线或分布式 registry。

### Trade-off 2: 内置 `/help` 自动生成 vs 手工维护帮助文档

**Trade-off**: 自动生成确保 plugin 命令实时同步，但输出格式受限。
**Decision**: 接受，先实现纯文本列表；若未来需要格式化输出，在 `CommandSpec` 中扩展 `usage` / `examples` 字段。