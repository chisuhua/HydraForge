# 多领域智能体架构与服务协作

> **文档目标**：解释 ADR-0019 至 ADR-0023 定义的多领域智能体架构中，智能体之间如何互相提供服务。

> **涉及 ADR**：ADR-0019 (IInteractionBus)、ADR-0020 (线程模型)、ADR-0021 (PDK)、ADR-0022 (插件加载)、ADR-0023 (ToolResult 标准)

---

## 一、智能体分层模型

根据 ADR-0020，智能体分为两类：

| 类型 | 角色 | 职责 |
|------|------|------|
| **认知智能体** (Cognitive Worker) | 编排者 | 意图理解、任务分解、DSL 生成、结果聚合 |
| **领域智能体** (Domain Workers) | 执行者 | 提供 `human::`、`code::`、`browser::` 等领域能力 |

```
HydraForge 进程
├── 主线程 (Main Thread)
│   ├── L0 DSL Engine — 编译/调度/执行
│   ├── L1 State Store — 内存状态访问
│   └── L2 Tool Registry — Schema 校验
│
├── 认知智能体工作线程 (Cognitive Worker)
│   ├── 意图理解
│   ├── 任务分解
│   ├── DSL 生成
│   └── 结果聚合
│
└── 领域智能体工作线程池 (Domain Workers)
    ├── human:: — 人类交互 (confirm/clarify)
    ├── code:: — 编程助手
    └── 其他领域 (browser::, fs:: 等)
```

---

## 二、服务提供方式

### 2.1 事件驱动总线模式 (ADR-0019)

智能体通过 **IInteractionBus** 进行通信：

```
┌──────────────────────────────────────────────────────┐
│  IInteractionBus (应用层协议)                          │
│  Session 管理 / Token 流 / 多轮对话                    │
│  session_id ↔ subscribe_tokens ↔ push_token          │
├──────────────────────────────────────────────────────┤
│  EventBus (传输层基础设施) — ADR-0002                  │
│  有界队列 / 优先级 / 节流合并 / Per-Agent 隔离         │
└──────────────────────────────────────────────────────┘
```

**事件类型**：

| 事件类型 | 说明 |
|---------|------|
| `UserMessage` | 用户输入 |
| `AssistantToken` | LLM 流式 Token |
| `AssistantComplete` | LLM 输出完成 |
| `ToolCall` | 工具调用开始 |
| `ToolResult` | 工具调用结果 |
| `Error` | 错误 |
| `Status` | 状态更新 |

### 2.2 工具服务注册模式 (ADR-0021, ADR-0022)

领域智能体通过 **PDK (Plugin Development Kit)** 提供标准化的工具服务：

```cpp
// 插件导出符号约定 (ADR-0022)
extern "C" const hydraforge::PluginInfo pdk_plugin_info;
extern "C" void pdk_register_tools(hydraforge::ToolRegistry& registry);
```

工具使用 `DECLARE_TOOL` 宏注册，返回标准化格式。

### 2.3 统一结果信封 (ADR-0023)

所有工具调用返回统一格式，智能体间传递的结构化数据：

```json
// 成功
{
    "ok": true,
    "data": { ... },
    "meta": {
        "duration_ms": 42,
        "tool_name": "code::edit_file",
        "trace_id": "sess_abc123"
    }
}

// 失败
{
    "ok": false,
    "error": {
        "code": "ERR_TOOL.NOT_FOUND",
        "message": "Tool 'foo' not registered"
    },
    "meta": { ... }
}
```

---

## 三、领域智能体间服务调用模式

领域智能体之间有两种主要协作方式，**不需要通过认知智能体中转**：

### 模式一：通过 DSL 工作流图（DAG）直接协作

这是**主要的协作方式**。DSL 定义了节点图，节点之间通过边传递数据：

```
┌─────────────────────────────────────────────────────────┐
│  DSL 工作流图 (由 CognitiveWorker 生成)                      │
│                                                          │
│  [code::analyze] ──output──→ [human::confirm]            │
│       │                          │                      │
│       │    边传递数据              │    边传递数据         │
│       ↓                          ↓                      │
│  {issues: [...]} ──────────→ {approved: true/false}     │
└─────────────────────────────────────────────────────────┘
```

**特点**：

- 节点间数据传递是**直接的**，通过 DSL 图的边
- CognitiveWorker 只负责**生成和监控**工作流，不参与数据传递
- 节点执行完结果自动流向下一个节点

### 模式二：通过 ToolRegistry 发现和调用工具

这是**工具级别的调用**，不同领域注册不同工具到共享的 ToolRegistry：

```cpp
// DomainWorkerPool 中的领域处理器注册
domain_worker_pool.register_domain_handler("code", [](const DomainTask& task) {
    // code:: 领域工具实现
});

domain_worker_pool.register_domain_handler("browser", [](const DomainTask& task) {
    // browser:: 领域工具实现
});

// 跨领域调用：ToolCallNode 执行时通过 ToolRegistry 调用
ToolRegistry::call_tool("code::analyze", args);
```

**调用链**：

```
ToolCallNode → ToolRegistry → 领域处理器 (DomainWorkerPool)
                    ↓
            不同领域的工具可互相调用
```

---

## 四、认知智能体的作用

认知智能体是**编排者而非中转者**：

| 职责 | 说明 |
|------|------|
| **意图理解** | 解析用户需求 |
| **DSL 生成** | 创建工作流图（定义节点和边） |
| **任务分配** | 将任务提交给 DomainWorkerPool |
| **结果聚合** | 监控执行、处理异常 |
| **不中转数据** | 节点间数据流由 DSL 图的边负责 |

---

## 五、完整执行流程

```
用户输入 → IInteractionBus → Cognitive Worker (编排)
                              ↓ 任务分解
                         Domain Worker Pool
                              ↓ 工具调用 (ToolRegistry)
                         ToolResult 标准化信封
                              ↓
                         Cognitive Worker (聚合结果)
                              ↓
                         IInteractionBus → UI
```

---

## 六、关键服务接口总结

| 服务 | 提供者 | 消费者 | 标准化方式 |
|------|--------|--------|-----------|
| **LLM 推理** | LlamaAdapter | Cognitive Worker | Token 流回调 |
| **工具执行** | Domain Workers | Cognitive Worker | ToolResult 信封 |
| **Session 管理** | IInteractionBus | 所有智能体 | Session ID 路由 |
| **插件加载** | PluginLoader (ADR-0022) | ToolRegistry | `pdk_plugin_info` + `pdk_register_tools` |

---

## 七、架构原则

1. **编排与执行分离**：认知智能体负责"思考"，领域智能体负责"执行"
2. **领域解耦**：不同领域通过标准化工具体接口交互，无直接依赖
3. **数据流直传**：节点间数据通过 DSL 图的边直接传递，无需中转
4. **事件驱动**：智能体间通过 IInteractionBus 异步通信

---

## 八、相关文档

| 文档 | 内容 |
|------|------|
| ADR-0019 | IInteractionBus 接口与 TUI Chat MVP |
| ADR-0020 | 多智能体线程模型与隔离策略 |
| ADR-0021 | Plugin Development Kit (PDK) 设计 |
| ADR-0022 | 插件加载机制 |
| ADR-0023 | ToolResult 标准化 |
| [relationships.md](../adr/relationships.md) | ADR 之间的关系与完整链路 |
