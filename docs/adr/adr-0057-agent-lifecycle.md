# ADR-0057: Agent 生命周期管理

## 状态

✅ Approved (2026-07-16, 架构评审确认)

## 领域

Agent-as-Plugin 架构 / Plugin 运行时管理

## 关联

- [ADR-0022 — Plugin Loading](../adr-0022-plugin-loading.md) — 加载/卸载基础
- [ADR-0041 — PluginLoader Lifecycle Extension](../adr-0041-pluginloader-lifecycle-extension.md) — `pdk_plugin_init`/`fini` 钩子
- [ADR-0052 — Agent Plugin Manifest](./adr-0052-agent-plugin-manifest.md) — manifest 中的 `activation_events`
- [ADR-0053 — AgentDescriptor 与 pdk_register_agent](./adr-0053-agent-descriptor-interface.md) — Agent 注册
- [ADR-0051 — PDK Composition Spike](../adr-0051-phase6-pdk-composition-spike.md) — 组合 Spike 的 RAII guard

## 背景

### 问题

当前 Agent Plugin 只有注册入口（`pdk_register_tools` + `pdk_register_agent`），没有定义生命周期状态机。OS 无法管理 Agent 的：

- 初始化时机（是否懒加载）
- 激活/停用（资源释放）
- 热更新（运行时替换）
- 依赖检查（卸载前验证）

### 目标

定义 Agent Plugin 的生命周期状态机，支持懒加载、冷切换、依赖检查。

## 决策

### 决策 1 — v1 简化状态机

```
LOADED → initialized → registered → active → inactive → unloaded
```

| 阶段 | OS 操作 | Plugin 响应 | 说明 |
|------|---------|------------|------|
| **LOADED** | `PluginLoader::load_so()` | — | 仅加载动态库符号 |
| **initialized** | 调用 `pdk_plugin_init(hooks)` | 分配内部状态 | 不注册工具，不启动线程 |
| **registered** | 调用 `pdk_register_tools` + `pdk_register_agent` | 注册到 OS | 工具可被发现但未激活 |
| **active** | 首次 `call_tool` 或条件触发 | 启动线程/连接 | 可处理请求 |
| **inactive** | OS 调用 `pdk_plugin_fini()` | 释放资源 | 资源回收，不卸载 |
| **unloaded** | `PluginLoader::unload_so()` | — | 彻底卸载 |

**状态转换**：

```
LOADED ──init──→ initialized ──register──→ registered ──activate──→ active
                                                                    │
                                                     deactivate     │
                                                    ┌───────────────┘
                                                    ▼
                                                inactive ←── unload → unloaded
```

**规则**：
- `registered` → `active` 由首次 `call_tool` 或 `activation_events` 触发
- `active` → `inactive` 由 OS 主动调用（资源回收、版本升级、用户卸载）
- `inactive` → `active` 再次激活（re-init）
- `inactive` → `unloaded` 彻底卸载

### 决策 2 — Lazy Loading

**声明方式**（manifest 中）：

```json
{
  "activation_events": [
    "onTool:provider/resolve",
    "onAgent:chat.orchestrator",
    "onLanguage:python",
    "onCommand:formatCode"
  ]
}
```

**支持的事件类型**：

| 事件 | 触发条件 | 示例 |
|------|---------|------|
| `onTool:{tool_name}` | 某工具被调用 | `onTool:provider/resolve` |
| `onAgent:{agent_id}` | 某 Agent 被注册 | `onAgent:chat.orchestrator` |
| `onLanguage:{lang}` | 用户打开某语言文件（IDE 场景） | `onLanguage:python` |
| `onCommand:{cmd}` | 用户执行某命令 | `onCommand:codeReview` |
| `onStartup` | OS 启动时立即激活 | — |

**行为**：
- 未触发 `activation_events` 的 Plugin 仅停留在 `LOADED` 状态
- 触发时 OS 自动调用 `init → register → activate` 序列
- `activation_events` 为空或缺失时，Plugin 在 `load_so` 后立即 `init`

### 决策 3 — 热更新（Phase 2，v1 不支持）

v1 只支持**冷切换**（deactivate → unload → load → activate），不支持 in-place 热更新。

**Phase 2 预留接口**：

```cpp
// 未来可能扩展
extern "C" ErrorCode pdk_plugin_hot_reload();  // 保存状态
extern "C" ErrorCode pdk_plugin_hot_loaded();  // 恢复状态
```

**理由**：
- 热更新的状态迁移复杂（线程迁移、连接保持、原子切换）
- 当前无硬性需求
- 冷切换（`deactivate → activate` 新版本）已可满足大多数场景

### 决策 4 — 依赖检查

```cpp
// 卸載前检查
if (registry.has_dependents(agent_id)) {
    // ERR_HAS_DEPENDENTS → 拒绝卸载
    return ErrorCode::ERR_HAS_DEPENDENTS;
}
```

**依赖声明**（`AgentDescriptor` 中）：

```cpp
struct AgentDescriptor {
    // ...
    std::vector<std::string> requires_agents;  // 依赖的其他 Agent ID
};
```

**规则**：
| 操作 | 检查条件 | 阻断条件 |
|------|---------|---------|
| deactivate Agent A | Agent B 是否 requires A？ | 拒绝，返回 ERR_HAS_DEPENDENTS |
| unload Agent A | 同上 | 拒绝 |
| activate Agent B | Agent A 是否已 active？ | warn 但仍可激活 |
| update Agent A | Agent B 是否与新版本兼容？ | warn 但仍可更新 |

**依赖链示例**：
```
Chat Agent → Loop Agent → Provider Agent, Session Agent, FS Agent, Code Agent

卸载 Provider Agent 时：
  → 检查谁 requires 我 → Loop Agent 依赖！
  → 拒绝卸载，返回 ERR_HAS_DEPENDENTS
  → 先卸载 Loop Agent（或其变更依赖）后再重试
```

### 决策 5 — 与 ADR-0051 ToolCoordinator RAII Guard 的关系

ADR-0051 定义了 ToolCoordinator 的嵌套深度和环检测 RAII guard。
本 ADR 的生命周期管理是**更外围**的 Plugin 级管理，两者正交：

| 管理层次 | 范围 | 机制 |
|---------|------|------|
| ADR-0051 ToolCoordinator RAII | 单次工具调用 | nesting depth + cycle detection |
| ADR-0057 Lifecycle | Plugin 级别 | load/init/register/activate/deactivate/unload |
| ADR-0033 Session | Agent 执行 | UserSession/TaskSession/SubtaskSession |

## 替代方案

### 方案 A：完整 ROS 2 Managed Node 状态机（6 状态 7 转换）

**否决理由**：
- v1 不需要 `Unconfigured → Inactive → Active → Finalized` 这么精细的状态管理
- 增加复杂度和调试成本

### 方案 B：不做生命周期，始终全量加载

**否决理由**：
- 违背 SOTA 共识（OSGi, VS Code, MCP 都支持 lazy-load）
- 内存浪费：未用 Plugin 占用资源
- 无法支持 Agent Marketplace

### 方案 C：热更新直接做

**否决理由**：
- 状态迁移复杂度高
- 无实际使用案例驱动
- 冷切换可满足当前所有场景

## 不变量

- Plugin 处于 `LOADED` 状态时 OS 不承诺其工具可用
- `active` 状态的 Plugin 必须能在 100ms 内响应 `call_tool`
- 依赖检查是**乐观检查**：运行时依赖可能因网络/故障不可用
- 冷切换不保证无缝（约 50-200ms 不可用窗口）

## 权衡

| 决策 | 选择 | 理由 |
|------|------|------|
| 状态数 | 6（v1 简化） | 够用，不增加复杂度 |
| Lazy loading | 必须实现 | SOTA 标准 |
| 热更新 | Phase 2 | 复杂度高，无硬性需求 |
| 依赖检查 | 实现 | 阻止常见问题 |
| 与 RAII Guard | 正交 | 不同管理层次 |

## 后续行动

- ADR-0054: `CapabilityRegistry` 与 lifecycle 集成（激活时自动注册，停用时自动注销）
- Phase 2: 热更新 —— `pdk_plugin_hot_reload`/`hot_loaded` 接口
- Phase 2: 在线升级——升级过程中保持服务可用

## 参考

- [ADR-0041 — PluginLoader Lifecycle Extension](../adr-0041-pluginloader-lifecycle-extension.md)
- [ADR-0052 — Agent Plugin Manifest](./adr-0052-agent-plugin-manifest.md)
- [ADR-0053 — AgentDescriptor 与 pdk_register_agent](./adr-0053-agent-descriptor-interface.md)
- [ADR-0051 — PDK Composition Spike](../adr-0051-phase6-pdk-composition-spike.md)
- ROS 2 Managed Node: `design.ros2.org/articles/node_lifecycle.html`
- VS Code Activation Events: `code.visualstudio.com/api/references/activation-events`