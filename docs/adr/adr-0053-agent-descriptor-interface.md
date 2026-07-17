# ADR-0053: AgentDescriptor 与 `pdk_register_agent` 接口

## 状态

✅ Approved (2026-07-16, 架构评审确认)

## 领域

Agent-as-Plugin 架构 / PDK 扩展

## 关联

- [ADR-0021 — PDK Design](../adr-0021-pdk-design.md) — 基础 PDK 宏与注册模式
- [ADR-0022 — Plugin Loading](../adr-0022-plugin-loading.md) — Plugin 加载与导出符号
- [ADR-0041 — PluginLoader Lifecycle Extension](../adr-0041-pluginloader-lifecycle-extension.md) — `pdk_plugin_init`/`fini` 钩子
- [ADR-0051 — PDK Composition Spike](../adr-0051-phase6-pdk-composition-spike.md) — 组合可行性 Spike 前置工作

## 背景

### 问题

当前 PDK Plugin 只注册**工具**（`pdk_register_tools`），但不注册**Agent**。OS 无法知道一个 Plugin 是"定义了一个 Agent"还是"只是提供了一组工具"。

具体缺失：
- 无法声明 Agent 的形态（SKILL/DSL/C++/Wasm 四选一）
- 无法声明 Agent 的能力和入口工具
- 无法声明 Agent 的依赖（依赖哪些其他 Agent）
- 无法声明 Agent 是否需要隔离执行
- 无法声明 Agent 支持哪些接口版本

### 现有入口

```cpp
// 当前 Plugin 只注册工具
extern "C" void pdk_register_tools(hydraforge::IToolRegistry& registry);
```

### 目标

为 Agent Plugin 添加统一的 Agent 描述与注册接口，使 OS 能：
1. 发现 Plugin 定义了哪些 Agent
2. 了解 Agent 的形态、能力、入口
3. 了解 Agent 的安全要求（隔离 / 依赖）
4. 在不启动 Agent 的情况下获取其元数据

## 决策

### 决策 1 — AgentForm 枚举

```cpp
// include/agenticdsl/pdk/agent_descriptor.h
namespace hydraforge {

enum class AgentForm {
    Skill,      // SKILL.md 解释执行，隔离环境
    DSL,        // .agent.md 图编译，可审计，可预算控制
    Cpp,        // C++ 原生代码，PDK DEFINE_AGENT 宏实现
    Wasm,       // WebAssembly 固化产物，capability 受限
    // Hybrid 暂不纳入 v1（Phase 2 扩展）
};

} // namespace hydraforge
```

**理由**：
- `Hybrid` 形态（一个 Agent 同时包含 SKILL+DSL+C++）的注册规则尚未经过实践验证，推迟到 Phase 2。
- 四形态与 "SKILL→DSL→C++→Wasm" 进化路径一一对应，没有歧义。

### 决策 2 — AgentDescriptor 结构体

```cpp
// include/agenticdsl/pdk/agent_descriptor.h
namespace hydraforge {

struct AgentDescriptor {
    std::string id;                       // 唯一标识，如 "code.review"
    std::string display_name;             // 显示名，如 "Code Review Agent"
    std::string version;                  // SemVer 字符串，如 "0.1.0"
    std::vector<AgentForm> forms;         // Agent 支持的形态列表
    std::string entry_tool;               // 入口工具名，如 "code_review/run"
    std::vector<std::string> provided_tools;  // 提供的所有工具名
    std::vector<std::string> requires_agents; // 依赖的其他 Agent ID
    nlohmann::json default_config;        // 默认配置（JSON）
    bool requires_isolation;              // 是否需要隔离环境
    std::vector<std::string> interface_versions;  // 支持的接口版本
};

} // namespace hydraforge
```

**字段详解**：

| 字段 | 必填 | 说明 |
|------|:----:|------|
| `id` | ✅ | reverse-DNS 风格，与工具命名约定一致（ADR-0043） |
| `display_name` | ✅ | 人类可读，用于 UI/日志 |
| `version` | ✅ | SemVer，用于版本约束和兼容性检查 |
| `forms` | ✅ | 非空。Skill 形态时 `requires_isolation` 必须为 true |
| `entry_tool` | ✅ | 其他 Agent 通过 `call_tool(entry_tool, args)` 调用此 Agent |
| `provided_tools` | ✅ | 可用于 `CapabilityRegistry` 索引（ADR-0054） |
| `requires_agents` | ⚠️ | 声明依赖，OS 可检查/等待依赖可用 |
| `default_config` | ❌ | 覆盖或合并到 Plugin 级配置 |
| `requires_isolation` | ✅ | Skill 必须 = true；DSL/C++/Wasm 可选 |
| `interface_versions` | ❌ | 空 = 兼容 V1 基础契约 |

### 决策 3 — 注册接口

```cpp
// Plugin 导出的新 C 符号（可选）
extern "C" void pdk_register_agent(hydraforge::AgentDescriptor& desc);
```

**注册时序**（调用 `pdk_register_tools` 之后）：

```
PluginLoader::load_so("libCodeReviewAgent.so")
  → pdk_plugin_init(hooks)          // ADR-0041
  → pdk_register_tools(registry)    // 注册工具到 IToolRegistry
  → pdk_register_agent(desc)        // 注册 Agent 描述（新增）
```

### 决策 4 — 注册规则

| 规则 | 行为 |
|------|------|
| 一个 Plugin 可注册多个 Agent | `pdk_register_agent` 可调用多次 |
| Agent 可以没有 Agent 描述 | 向后兼容现有 PDK 插件（`llama_engine` 等） |
| `forms` 包含 Skill 时 `requires_isolation` 必须 = true | 编译时断言或运行时拒绝注册 |
| `entry_tool` 必须在 `provided_tools` 中 | OS 在注册时验证 |
| `id` 必须唯一 | 重复 id 时后注册覆盖先注册（OS 发出 warning） |
| `interface_versions` 为空时默认 V1 | 基础契约：`execute(input)` → `output` |
| OS 启动时默认选择 `forms[0]` | 第一个形态为默认实现 |

### 决策 5 — `forms` 向量设计

使用 `vector<AgentForm>` 而非单一值，支持同一 Agent 的多形态共存：

```yaml
agent: code.review
forms:
  - skill:      # 开发环境：SKILL.md 解释执行
    isolation: true
  - dsl:        # 生产环境：DSL .agent.md 编译执行
    path: agents/code_review.agent.md
  - cpp:        # 高性能环境：C++ 原生
  - wasm:       # 边缘部署：Wasm 二进制
    path: wasm/code_review.wasm
```

OS 根据部署环境自动选择最佳形态：
- 开发/测试 → `Skill`（热更新，快速迭代）
- 生产/CI → `DSL`（可审计，预算控制）
- 高频路径 → `Cpp`（微秒级延迟）
- 边缘/不可信 → `Wasm`（强隔离，跨平台）

## 替代方案

### 方案 A：单一形态值（非向量）

**否决理由**：
- 无法表达 Agent 的多形态进化能力（SKILL→DSL→C++→Wasm）
- 每次进化需要修改 Plugin 代码
- 违反"四形态可进化"的核心命题

### 方案 B：manifest 文件 + C 符号

**否决理由**：
- 两套 Agent 描述源（manifest JSON + C struct）增加维护成本
- 本 ADR 只解决 C 级注册，manifest 格式交由 ADR-0052
- Phase 2 可合并：`pdk_manifest()` 返回 JSON 包含 `AgentDescriptor`

### 方案 C：不新增 `pdk_register_agent`，工具即 Agent

**否决理由**：
- 一个 Plugin 可能注册 12 个工具（`llama_engine`）但不定义任何 Agent
- 一个 Agent 可能依赖多个其他 Agent 的工具
- OS 无法区分"工具集合"和"Agent 入口"

## 不变量

- Agent 注册不影响现有 `pdk_register_tools` 流程（100% 向后兼容）
- `requires_isolation = true` 的 Agent 必须由 OS 在隔离环境中执行
- `forms` 中声明的形态必须都可用（OS 启动时验证）
- 一个 Plugin 可以注册 0-N 个 Agent，N 个 Agent 可以共享同一 Plugin 的工具

## 权衡

| 决策 | 选择 | 理由 |
|------|------|------|
| `forms` 类型 | `vector<AgentForm>` | 支持多形态进化 |
| `Hybrid` 纳入 v1 | ❌ 不纳入 | 实践验证不足 |
| `requires_isolation` 约束 | Skill 强制 true | 安全不可商量 |
| 注册时序 | `register_tools` → `register_agent` | 工具先可用，再声明 Agent |
| 向后兼容 | 完全兼容 | `register_agent` 为可选导出 |

## 后续行动

- ADR-0052: `pdk_manifest.json` 格式（manifest 字段标准化）
- ADR-0057: Agent 生命周期管理（基于 `AgentDescriptor` 的 install/init/activate 流程）
- ADR-0054: `CapabilityRegistry` 按能力发现 Agent（使用 `provided_tools` 索引）
- Phase 2: `AgentDescriptor` 扩展 `forms` 支持路径配置（如 `path: agents/react.agent.md`）

## 参考

- [ADR-0021 — PDK Design](.//adr-0021-pdk-design.md)
- [ADR-0022 — Plugin Loading](./adr-0022-plugin-loading.md)
- [ADR-0043 — PDK Tool Naming Convention](./adr-0043-pdk-tool-naming-convention.md)
- [ADR-0051 — PDK Composition Spike](./adr-0051-phase6-pdk-composition-spike.md)
- [docs/architecture/agent-as-plugin-architecture-v1.1.md](../architecture/agent-as-plugin-architecture-v1.1.md)