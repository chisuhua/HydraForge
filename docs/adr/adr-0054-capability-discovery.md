# ADR-0054: Capability-Based Agent Discovery

## 状态

✅ Approved (2026-07-16, 架构评审确认)

## 领域

Agent-as-Plugin 架构 / Agent 发现与路由

## 关联

- [ADR-0052 — Agent Plugin Manifest](./adr-0052-agent-plugin-manifest.md) — manifest 中的 `capabilities` 字段
- [ADR-0053 — AgentDescriptor 与 pdk_register_agent](./adr-0053-agent-descriptor-interface.md) — Agent 注册
- [ADR-0057 — Agent 生命周期管理](./adr-0057-agent-lifecycle.md) — lifecycle 与 registry 集成
- [ADR-0022 — Plugin Loading](../adr-0022-plugin-loading.md) — Plugin 加载
- FIPA 97 — AMS/DF (白页/黄页) 规范

## 背景

### 问题

当前 Agent 之间通过**硬编码工具名**调用（`call_tool("loop/run", ...)`），没有能力发现机制。如果：

- `loop/run` 被替换为 `agent_loop/execute`
- 新增一个更快的 `loop/run` 实现
- 需要根据运行时上下文选择不同的 Agent

调用方都必须修改代码。OS 也无法在启动时**自动匹配**请求到合适的 Agent。

### 目标

定义一个 CapabilityRegistry，使 OS 能：
1. 按能力标签 + 输入 schema 发现 Agent
2. 根据 ranking 选择最佳实现
3. 与 lifecycle 自动集成（激活注册、停用注销）

## 决策

### 决策 1 — CapabilityRegistry 组件

```cpp
// include/agenticdsl/discovery/capability_registry.h
namespace hydraforge {

/// Agent 能力快照
struct AgentCapability {
    std::string agent_id;
    std::string entry_tool;
    std::vector<std::string> capabilities;
    nlohmann::json input_schema;     // JSON Schema 2020-12
    nlohmann::json output_schema;
    std::string version;
    AgentForm form;
    uint32_t ranking;                // 越大越优先
    AgentLifecycleStatus status;     // active / inactive / loaded
};

class CapabilityRegistry {
public:
    // 注册 Agent 能力（由 lifecycle activate 时触发）
    void register_agent(const AgentDescriptor& desc);
    
    // 注销（由 lifecycle deactivate 时触发）
    void unregister_agent(const std::string& agent_id);
    
    // 按入口工具查找（精确匹配，向后兼容 call_tool）
    std::optional<AgentCapability> resolve_by_tool(const std::string& tool_name);
    
    // 按能力标签 + schema 查找（新增能力发现）
    std::vector<AgentCapability> query(
        const std::vector<std::string>& required_capabilities,
        const nlohmann::json& input_schema_pattern,
        const QueryOptions& options = {}
    );
    
    // 检查是否有依赖
    bool has_dependents(const std::string& agent_id);
};

struct QueryOptions {
    std::vector<AgentForm> preferred_forms;  // 形态偏好
    std::string require_version = ">=0.0.0"; // 最低版本
    uint32_t max_results = 3;                // 最多返回数量
    std::string sort_by = "ranking";         // ranking / version / trust
};

} // namespace hydraforge
```

### 决策 2 — `query` 查找规则

```
query 执行流程：
  1. 过滤：只考虑 status == active 的 Agent
  2. 匹配：Agent.capabilities ⊇ required_capabilities（所有必需标签都要有）
  3. Schema 兼容：Agent.input_schema 兼容 input_schema_pattern
  4. 排序：按 QueryOptions.sort_by 排序
  5. 截断：取前 max_results 个
```

**Schema 兼容性检查**：
- `input_schema_pattern` 的每个字段在 Agent 的 `input_schema` 中都能找到对应
- 字段类型兼容（string 可匹配 string，不允许 string 匹配 number）
- optional 字段匹配 optional，required 字段匹配 required

**查询示例**：

```cpp
// Chat Agent 需要代码审查能力
auto results = registry.query(
    {"code_review", "static_analysis"},
    {{"code", "string"}, {"language", "string"}},
    {.preferred_forms = {AgentForm::Cpp, AgentForm::DSL}, .max_results = 3}
);
// → [code.review (cpp, ranking=10), code.ai (skill, ranking=5)]
// Chat Agent 选择 code.review
auto best = results.front();
auto result = call_tool(best.entry_tool, args);
```

### 决策 3 — Ranking 策略

**ranking 来源**（按优先级）：

| 來源 | 示例 | 说明 |
|------|------|------|
| manifest 声明 | `"ranking": 10` | 开发者手动指定 |
| trust_level 自动计算 | `"high" → 10, "medium" → 5, "low" → 1, "untrusted" → 0` | 信任等级映射为 ranking |
| 版本自动叠加 | v3.0.0 比 v1.0.0 多 +2 | 版本新有加分 |

**默认 ranking 计算公式**：

```
ranking = manifest.ranking (若声明)
       OR trust_level_ranking + version_bonus

trust_level_ranking = {high: 10, medium: 5, low: 1, untrusted: 0}
version_bonus = min(major_version * 2, 10)   // 最多加 10
```

**选择策略**：
- 默认返回 ranking 最高的一个
- ranking 相同时版本更新的优先
- 版本相同时先注册的优先
- 调用方可在 `QueryOptions` 中覆盖

### 决策 4 — Lifecycle 集成

```
Agent lifecycle 与 CapabilityRegistry 的绑定：

activate(agent_id):
  → 调用 pdk_register_tools(registry)    // 注册工具
  → 调用 pdk_register_agent(desc)        // 注册 Agent
  → OS 自动调用 registry.register_agent(desc)
  
deactivate(agent_id):
  → OS 检查 registry.has_dependents(agent_id)
    → true: 拒绝 deactivate，返回 ERR_HAS_DEPENDENTS
    → false: 继续
  → OS 调用 registry.unregister_agent(agent_id)
  → 调用 pdk_plugin_fini()
```

**事件通知**：
```cpp
// 自动发出事件，其他 Agent 可订阅
bus->emit("discovery.agent.registered", {agent_id, capabilities});
bus->emit("discovery.agent.unregistered", {agent_id});
bus->emit("discovery.agent.changed", {agent_id, old_caps, new_caps});
```

### 决策 5 — 与 `call_tool` 的关系

`call_tool` 保持**向后兼容**，只增加不修改：

```cpp
// 原有方式（精确匹配工具名）——不变
auto result = registry.call_tool("loop/run", args);

// 新方式（按能力发现 Agent 再调用）
auto agents = capability_registry.query({"plan_execute"}, ...);
auto best = agents.front();
auto result = registry.call_tool(best.entry_tool, args);
```

**resolve_by_tool** 提供了从工具名到 Agent 的反查：

```cpp
// 已知工具名，查找是哪个 Agent 提供的
auto agent = registry.resolve_by_tool("loop/run");
if (agent) {
    // agent->agent_id → "chat.loop"
    // agent->form → AgentForm::DSL
}
```

## 替代方案

### 方案 A：不引入 CapabilityRegistry，保持硬编码

**否决理由**：
- 违背 SOTA 共识（FIPA DF 黄页、OSGi Service Registry）
- Agent 无法运行时动态替换
- 调用方必须修改代码才能切换 Agent

### 方案 B：用 LLM 做能力发现

**否决理由**：
- LLM 的非确定性导致不可靠
- 每次 query 调用 LLM 成本高
- 简单的 schema 匹配不需要 LLM

### 方案 C：CapabilityRegistry 独立于 lifecycle

**否决理由**：
- 会产生"僵尸 Agent"（已卸载但仍在 registry 中）
- 必须强绑定：activate = register，deactivate = unregister

## 不变量

- 只有 `active` 状态的 Agent 才出现在 query 结果中
- `call_tool` 不经过 CapabilityRegistry（性能关键路径无额外开销）
- `register_agent` 在 `register_tools` 之后调用（工具必须先可用）
- `has_dependents` 是乐观检查（运行时依赖可能故障）

## 权衡

| 决策 | 选择 | 理由 |
|------|------|------|
| query 匹配 | capabilities ⊇ + schema 兼容 | 平衡灵活性和准确性 |
| ranking 策略 | trust → version | 信任优先，版本辅助 |
| max_results 默认 | 3 | 够用，不 overload |
| 生命周期集成 | 强绑定 | 防僵尸 Agent |
| 事件通知 | 实现 | 支持松耦合 Agent 协作 |

## 后续行动

- ADR-0052: manifest 中的 `capabilities` 字段（已定义）
- ADR-0058: schema 强制校验（query 的 schema 兼容性检查的基础）
- Phase 2: 动态 ranking（根据执行效果自动调整 ranking）
- Phase 2: 跨网络 CapabilityRegistry（代理远程 Agent）

## 参考

- [ADR-0052 — Agent Plugin Manifest](./adr-0052-agent-plugin-manifest.md)
- [ADR-0053 — AgentDescriptor 与 pdk_register_agent](./adr-0053-agent-descriptor-interface.md)
- [ADR-0057 — Agent 生命周期管理](./adr-0057-agent-lifecycle.md)
- FIPA 97 Agent Management Spec: AMS (白页) / DF (黄页) 模型
- OSGi Core R8 §5: Service Registry model