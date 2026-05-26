# ADR 关联性分析

> 本文档记录 ADR-0019 ~ ADR-0023 之间的依赖关系、接口交叉点、已识别的缺口和实施顺序建议。
>
> **最后更新**: 2026-05-25 | **依据**: 联合审查 (ADR-0019 × ADR-0020 × ADR-0021)

---

## 一、ADR 全景

### 状态总览

| ADR | 主题 | 状态 | 替代关系 |
|-----|------|------|---------|
| ADR-0001 ~ 0018 | 基础设施/推理/记忆/DSL | ✅ 已批准 | — |
| ADR-0006 | HarnessEngine 后台线程模型 | 🔄 已替代 | → ADR-0020 |
| **ADR-0019** | IInteractionBus + TUI Chat MVP | 🔍 提议中 | — |
| **ADR-0020** | 多智能体线程模型与隔离策略 | 🔍 提议中 | ← 替代 ADR-0006 |
| **ADR-0021** | Plugin Development Kit (PDK) | 🔍 提议中 | — |
| **ADR-0022** | 插件加载机制 (已规划) | 📋 待创建 | — |
| **ADR-0023** | ToolResult 标准化 (已规划) | 📋 待创建 | — |

### 生命周期阶段

```
阶段1: 基础设施 (ADR-0001~0009)
阶段2: 记忆系统 (ADR-0010~0014)
阶段3: 推理能力 (ADR-0015~0018)
阶段4: 智能体层 (ADR-0019~0023) ← 当前
```

---

## 二、依赖关系

### 依赖图

```
ADR-0019 (IInteractionBus)         ADR-0002 (EventBus, 底层传输)
    │                                    ↑
    │  依赖 Event 类型                     │ 未来 InMemoryBus 可重构为使用
    │                                    │ EventBus 的有界队列/优先级
    ▼                                    │
ADR-0020 (Thread Model) ────────────────┘
    │
    │  CognitiveWorker 持有 unique_ptr<DSLEngine>
    │  ToolRegistry 使用 ADR-0019 的 IInteractionBus
    │  DomainWorkerPool 是领域执行器的骨架
    │
    ▼
ADR-0021 (PDK)
    │
    │  DECLARE_TOOL → 使用 ToolRegistry 的签名 (→ ADR-0020)
    │  DEFINE_AGENT → 使用 CognitiveWorker 模式 (→ ADR-0020)
    │  SafeExec → 使用 ADR-0020 的锁模型
    │  Test Mocks → 使用 ADR-0019 的 Event/Token 类型
    │
    ▼
ADR-0022 (插件加载) ─── 定义 Runtime 如何加载 PDK 编译的 .so
    │
    ▼
ADR-0023 (ToolResult) ─── 统一 JSON 格式贯穿全线
```

### 依赖方向规则

| 规则 | 内容 |
|------|------|
| **R1** | ADR-0021 依赖 ADR-0020 和 ADR-0019，方向不可逆 |
| **R2** | ADR-0019 和 ADR-0020 互相独立（可并行实施） |
| **R3** | ADR-0022 依赖 ADR-0021（必须等 PDK 定义了 .so 格式才能定义加载） |
| **R4** | ADR-0023 依赖 ADR-0019/0020/0021（必须等全线对齐后才能标准化格式） |

---

## 三、接口交叉点

### 3.1 接口一致性矩阵

| 接口点 | ADR-0019 | ADR-0020 | ADR-0021 | 状态 |
|--------|----------|----------|----------|------|
| **ToolSchema** | 未定义（引用 ADR-0004） | `std::function(json)→json` | `DECLARE_TOOL` 生成 `ToolSpec` | ⚠️ 不一致 → 由 ADR-0023 统一 |
| **Event/Token** | `EventType` + `Token` 在 `events.h` | 未定义（引用 ADR-0019） | 通过 `RETURN_SUCCESS` 隐式产生 | ✅ 一致 |
| **Session** | `session_id` 统一标识 | `CognitiveTask.session_id` | 未定义 | ✅ 一致 |
| **Tool 注册** | 未涉及 | `ToolRegistry::call_tool(name, args)→json` | `DECLARE_TOOL` → 注册函数 | ✅ 一致 |
| **DSLEngine** | `atomic<shared_ptr<IInteractionBus>>` | `unique_ptr<DSLEngine>` per Worker | 不直接引用 | ✅ 一致 |
| **Sandbox 返回** | 未定义 | 未定义（Phase 2） | `SafeExec::run()` 模板返回 | ⚠️ 缺失 → ADR-0023 |
| **Plugin Lifecycle** | 未涉及 | 未涉及 | `init/load/unload` | ❌ 缺失 → ADR-0022 |

### 3.2 Tool 注册格式差异

当前三个 ADR 对"工具是什么"的定义不一致：

```json
// ADR-0020 (ToolRegistry 现有格式)
{
    "name": "edit_file",
    "func": std::function<nlohmann::json(const Args&)>  // 无 Schema
}

// ADR-0021 (PDK 的 ToolSpec)
{
    "name": "edit_file",
    "description": "Edit files in workspace",
    "parameters": [{"name": "path", "type": "string", "required": true}],
    "permissions": "Workflow",
    "impl": [](){...}  // 领域逻辑
}

// ADR-0023 (需要统一的格式)
{
    "schema": {"name", "description", "params"},
    "permissions": {"level", "paths"},
    "handler": std::function<nlohmann::json(const Args&)>
}
```

---

## 四、数据流全景

### 4.1 完整工具调用链路

```
┌── PDK 编译时 ──────────────────────────────────────────────────┐
│  DECLARE_TOOL(edit_file)                                        │
│      → 展开为 ToolSpec 结构体                                    │
│      → 编译到 edit_file_plugin.so                                │
└─────────────────────────────────────────────────────────────────┘
         │
         ▼
┌── ADR-0022 (插件加载) ──────────────────────────────────────────┐
│  Runtime 启动时:                                                 │
│  dlopen("edit_file_plugin.so")                                   │
│  dlsym(handle, "pdk_register_tools")                             │
│  → 调用注册函数                                                  │
└─────────────────────────────────────────────────────────────────┘
         │
         ▼
┌── ADR-0020 (运行时执行) ────────────────────────────────────────┐
│  ToolRegistry::call_tool("edit_file", args)                      │
│      ├── shared_lock 查找工具                                    │
│      ├── 锁外权限检查 (ADR-0004)                                  │
│      ├── 执行 lambda → ToolResult → nlohmann::json              │
│      └── NodeExecutor 写入 Context                               │
└─────────────────────────────────────────────────────────────────┘
         │
         ▼
┌── ADR-0019 (事件推送) ──────────────────────────────────────────┐
│  bus_->push_event(ToolResult{...})                                │
│  bus_->push_token(AssistantToken{...})                           │
│      → InMemoryBus 遍历回调                                       │
│      → TUI Chat 收到事件 → redraw()                              │
└─────────────────────────────────────────────────────────────────┘
```

### 4.2 完整 Agent 循环链路

```
┌── ADR-0021 (PDK) ──────────────────────────────────────────────┐
│  DEFINE_AGENT(coding_assistant, REACT_LOOP)                      │
│      → 创建 CognitiveWorker 实例                                  │
│      → 注册 ON_INTENT 回调                                       │
└─────────────────────────────────────────────────────────────────┘
         │
         ▼
┌── ADR-0020 (CognitiveWorker) ───────────────────────────────────┐
│  CognitiveWorker::run():                                         │
│      ├── 等待任务                                                │
│      ├── bus_->subscribe_tokens()                                │
│      ├── engine_->run_async()  // 独立 DSLEngine 实例             │
│      └── 循环                                                    │
└─────────────────────────────────────────────────────────────────┘
         │
         ▼
┌── ADR-0020 (DSLEngine) ────────────────────────────────────────┐
│  DSLEngine::run_async(session_id, message):                      │
│      ├── MarkdownParser 解析 DSL                                 │
│      ├── Topo Scheduler 拓扑排序                                 │
│      ├── NodeExecutor 逐节点执行                                 │
│      │   ├── DSLNode → LLM 调用 → 流式 Token                    │
│      │   ├── ToolCallNode → ToolRegistry → 工具执行              │
│      │   └── 通过 bus_ 推送 Token 和 Event                       │
│      └── 返回 ExecutionResult                                    │
└─────────────────────────────────────────────────────────────────┘
```

---

## 五、已识别的缺口

| 编号 | 缺口描述 | 涉及 ADR | 严重度 | 解决方案 |
|------|---------|---------|--------|---------|
| **G1** | **ToolResult 格式未标准化**：`RETURN_SUCCESS`/`ToolRegistry::call_tool`/`NodeExecutor` 三者之间 JSON 格式不一致 | 0019/0020/0021 | 🔴 高 | 创建 ADR-0023 |
| **G2** | **插件加载机制缺失**：PDK 编译的 .so 如何被 Runtime 发现、加载、注册符号无定义 | 0021 | 🔴 高 | 创建 ADR-0022 |
| **G3** | **ToolSchema 定义不统一**：DECLARE_TOOL 的 PARAM 语法与 ToolRegistry 的 string→string map 不匹配 | 0020/0021 | 🟡 中 | ADR-0022 + ADR-0023 |
| **G4** | **DEFINE_AGENT 与 CognitiveWorker 接口未定义**：Agent 模板需要哪些参数（DSL 路径？bus 引用？） | 0020/0021 | 🟡 中 | 在 ADR-0022 中定义 |
| **G5** | **SafeExec 返回类型未约束**：模板类型 vs `nlohmann::json` 期望 | 0020/0021 | 🟢 低 | ADR-0023 统一 |
| **G6** | **IInteractionBus 与 EventBus 未对接**：MVP 用 mutex，长期需对齐 ADR-0002 | 0019/0002 | 🟢 低 | Phase 2 处理 |

---

## 六、实施顺序建议

### 按 Phase 排列

```
Phase 1 (独立并行)
├── ADR-0019 Phase 1 — IInteractionBus + InMemoryBus
├── ADR-0020 Phase 1 — CognitiveWorker + DomainWorkerPool
└── ADR-0021 Phase 1 — DECLARE_TOOL 宏

Phase 2 (依赖 Phase 1)
├── ADR-0019 Phase 2 — DSLEngine bus 集成
├── ADR-0020 Phase 2 — StateStore + 锁优化
├── ADR-0021 Phase 2 — DEFINE_AGENT 模板
├── ADR-0022         — 插件加载机制 (依赖 PDK)
└── ADR-0023         — ToolResult 标准化 (依赖全线)

Phase 3 (集成)
├── ADR-0019 Phase 3 — TUI Chat 应用
├── ADR-0020 Phase 3 — 沙箱接口
├── ADR-0021 Phase 3 — PluginLifecycle + .so
└── 集成测试

Phase 4 (优化)
├── ADR-0020 Phase 4 — 协程化
└── 性能测试 + 调优
```

### 关键路径

```
最短路径: ADR-0019 P1 → ADR-0020 P1 → ADR-0021 P1 → demo 可运行
           ↓          ↓            ↓
         InMemoryBus  CognitiveWorker  DECLARE_TOOL
         + TUI Chat   + DomainPool     + 测试替身
```

---

## 七、ADR 文件索引

| 文件 | 主题 | 关联性 |
|------|------|--------|
| `adr-0002-eventbus-bounded-queue.md` | EventBus 有界队列 | IInteractionBus 底层传输（未来） |
| `adr-0003-dslengine-thread-safety.md` | DSLEngine 线程安全 | CognitiveWorker 依赖（per-instance） |
| `adr-0004-toolregistry-security.md` | ToolRegistry 安全模型 | 权限校验标准 |
| `adr-0006-harness-engine-thread-model.md` | HarnessEngine | 🔄 已替代 → ADR-0020 |
| `adr-0019-iinteraction-bus-mvp.md` | IInteractionBus + TUI Chat | 本分析 |
| `adr-0020-thread-model-isolation.md` | 线程模型与隔离 | 本分析 |
| `adr-0021-pdk-design.md` | Plugin Development Kit | 本分析 |
| `adr-0022-plugin-loading.md` | 插件加载机制 | 🔍 提议中 |
| `adr-0023-tool-result-standard.md` | ToolResult 标准化 | 🔍 提议中 |
