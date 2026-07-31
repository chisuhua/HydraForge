# ADR-0070: PDK Plugin 命令/快捷键注册 (DECLARE_COMMAND)

## 状态

🔍 Proposed (2026-07-31 — 架构缺失能力审计 D4 决议立项, 待架构组评审; 实施排期 Wave 1 第 3 项; 快捷键仅定义契约, 实现 defer)

## 领域

L3 PDK Contract / L4 用户输入层命令抽象

## 关联

- [ADR-0021 — PDK 设计](./adr-0021-pdk-design.md) — 宏家族哲学 (P1-P6), DECLARE_COMMAND 遵循同一模式 (宏展开 = spec 结构体 + 错误包装 handler, P3 静态链接)
- [ADR-0043 — PDK 工具命名约定](./adr-0043-pdk-tool-naming-convention.md) — 工具 slash 命名与命令 `/name` 用户语法为两个命名空间, 见 §决策 6
- [ADR-0069 — ToolCoordinator Hook](./adr-0069-tool-coordinator-hooks.md) — 命令委托工具调用时, pre/post hooks 全程生效 (组合关系, 见 §决策 1)
- [ADR-0068 — 事件发射契约](./adr-0068-event-emission-contract.md) — 命令执行事件通知遵循该契约
- [`docs/architecture/layer-based-missing-capabilities-analysis.md`](../architecture/layer-based-missing-capabilities-analysis.md) §三 X3 / §七 L3-3 — 决策源头
- [`docs/research/pi-agent-vs-pdk-chat-demo-analyze.md`](../research/pi-agent-vs-pdk-chat-demo-analyze.md) — pi-agent `registerCommand`/`registerShortcut` 对照

## 背景

### 问题

PDK 宏家族 (DECLARE_TOOL / DEFINE_AGENT) 已成熟, 但**用户输入层零抽象**: 源码核查 `grep -rn "registerCommand|registerShortcut" include/agenticdsl/pdk/` 返回空 (2026-07-31 复核)。`examples/pdk_chat_demo/main.cpp:388` 输入循环仅硬编码 `exit`/`quit`, 其余输入原样进入 `session.chat()`。`/tree`、`/compact`、`/fork`、`/clone` 等斜杠命令若实现, 唯一途径是 main.cpp if-else 链——hardcode 持续膨胀, 违背 Agent-as-Plugin "契约唯一" 原则。

**核心概念问题 — 命令不是工具。** 三类目标命令的性质分析 (D4 决议):

| 命令 | 性质 | 与工具的关系 |
|------|------|------------|
| `/tree` (树导航 TUI) | 纯 UI 交互 (渲染/光标/分支显示) | 不对应任何工具; 不应套 ToolMetadata/审批/layer 检查 |
| `/compact` | UI 入口 → 能力调用 | 委托 `session/compact` 工具 |
| `/fork` | 混合 | 委托 `session/branch` + UI 反馈 |

"命令即工具"方案 (`command/tree` 工具 + 通用分发器) 使纯 UI 命令被迫携带 ToolMetadata V2 全套安全元数据与审批矩阵——概念错位, 且丢失 usage 文本、参数解析、`/help` 列举等 UI 专属元数据的安放处。

### 目标

1. PDK plugin 可注册用户输入层命令 (`/name args...`), slash 命令全部可插拔化, main.cpp 零 hardcode。
2. 安全模型零变化: 命令只是 UI 糖, 委托能力时走正常受治理工具调用路径。
3. 宏模式与 DECLARE_TOOL 一致, 开发者学习成本零增量。
4. 快捷键契约先定义, 实现不绑架本 ADR (raw mode 输入基础设施 defer)。

## 决策

### 1. 概念界定: Command = L4 用户输入层入口, ≠ Tool

Command 是用户输入 `/name args...` 的分发入口, handler 接收应用上下文自由发挥。**命令不产生任何安全旁路**: handler 需要能力时必须经 `IToolRegistry::call_tool` 正常路径——ToolCoordinator layer check / ApprovalHandler / ADR-0069 pre/post hooks 全程生效。纯 UI 命令 (如 `/tree`) 不触碰工具层, 自然无需 ToolMetadata。

### 2. DECLARE_COMMAND 宏

沿用 DECLARE_TOOL 已验证模式 (ADR-0021 §3.1):

```cpp
// include/agenticdsl/pdk/command_macros.h (新建)
struct CommandSpec {
    std::string name;           // "tree" (不含 '/' 前缀)
    std::string description;    // /help 列举用
    std::string usage;          // "/tree [session_id]"
    std::string plugin_origin;  // 注册来源 plugin (冲突诊断)
};

struct CommandContext {
    IToolRegistry& registry;                    // 委托工具调用 (受治理)
    std::shared_ptr<IInteractionBus> bus;       // 事件通知 (ADR-0068)
    void* app_session;                          // 应用层会话句柄 (L4 自定类型, 插件以 void* 接收)
};

using CommandHandler = std::function<nlohmann::json(const CommandContext&, std::string_view args)>;

// DECLARE_COMMAND(name, description, usage, body)
// 展开为: inline CommandSpec command_spec_##name + inline json command_handler_##name(...)
// body 内异常自动捕获返回 {"error": ...} (与 DECLARE_TOOL 同模式)
```

### 3. ICommandRegistry (L3 契约)

```cpp
// include/agenticdsl/contract/icommand_registry.h (新建)
class ICommandRegistry {
public:
    virtual ~ICommandRegistry() = default;
    virtual bool register_command(CommandSpec spec, CommandHandler handler) = 0;  // 冲突返回 false
    virtual const CommandSpec* find(const std::string& name) const = 0;           // 支持 "plugin/cmd" 与 "cmd" 解析
    virtual std::vector<const CommandSpec*> list() const = 0;                     // /help 数据源
};
```

- 命名: 建议 plugin 使用 `plugin/command` 全名注册, `/name` 为快捷解析 (无歧义时); 冲突时注册失败 (与 ToolRegistry 同名策略一致)。
- L1 提供默认实现 `CommandRegistry`, 镜像 IToolRegistry/InMemoryBus 的既有模式。

### 4. DECLARE_SHORTCUT — 契约定义, 实现 defer

`DECLARE_SHORTCUT(key, command_name)` 仅声明绑定 (如 `"ctrl+l"` → `"model"`), 存入 CommandSpec 附属表。**实际触发依赖终端 raw mode 逐键输入基础设施, pdk_chat_demo 当前为 `getline` 行输入, 无此能力**——实现 defer 至 L4-2 异步 I/O 改造后, 不在本 ADR 实施范围。契约先行避免后续 ABI 破坏。

### 5. 应用壳内置命令

- `/help`: 从 ICommandRegistry::list() 自动生成 (名称 + description + usage)。
- `/exit` (alias `/quit`): 应用壳保留字, plugin 不可注册同名 (注册返回 false)。
- 内置命令与 plugin 命令同表列举, 无特权显示差异。

### 6. 边界条款

- 宏哲学遵循 ADR-0021 P1-P6 (P3 静态链接, Runtime 零感知)。
- 工具命名 (`inference/engine/generate`, slash, ADR-0043) 与命令用户语法 (`/tree`, slash 前缀) 是**两个独立命名空间**: 前者是 ToolRegistry 键, 后者是用户输入令牌, 不存在冲突域。
- 命令执行前后的事件通知 (如 `command.invoked`) 若发射, 主题与 payload 遵循 ADR-0068 Registry (新增主题需先入 Registry)。
- `void* app_session` 是有意的 L4 逃逸舱: PDK 契约不感知具体应用会话类型 (与 R2 "L3 不依赖 L4" 兼容——void* 不形成类型依赖)。

### 7. 转 Approved 条件

1. `command_macros.h` + `icommand_registry.h` + L1 默认实现落地;
2. pdk_chat_demo 接入: 输入循环 `/` 前缀分发, `/help` 可用, main.cpp 硬编码命令清零;
3. 至少 1 个真实 plugin 命令 (建议 `/compact` 委托 `session/compact`, 验证治理路径生效);
4. 注册冲突 / `/help` 列举 / 委托工具调用经 ToolCoordinator, 3 类测试通过;
5. ctest 零回归 + `adr_lint` 0 错误。

**估时**: 0.5 Sprint (宏 + registry + 注册流程 + 测试), 与 Wave 1 分析文档一致。
