# ADR 关联性分析

> 本文档记录 ADR-0019 ~ ADR-0023 之间的依赖关系、接口交叉点、已识别的缺口和实施顺序建议。
>
> **最后更新**: 2026-05-28 | **依据**: ADR-0030 异步架构审查

---

## 一、ADR 全景

### 状态总览

| ADR | 主题 | 状态 | 替代关系 |
|-----|------|------|---------|
| ADR-0001 ~ 0018 | 基础设施/推理/记忆/DSL | ✅ 已批准 | — |
| ADR-0006 | HarnessEngine 后台线程模型 | 🔄 已替代 | → ADR-0020 |
| **ADR-0019** | IInteractionBus + TUI Chat MVP | ✅ 已批准 (C1 A-stage) | — |
| **ADR-0020** | 多智能体线程模型与隔离策略 | 🔄 部分实施 (C1 B-stage) | ← 替代 ADR-0006 |
| **ADR-0021** | Plugin Development Kit (PDK) | 🔍 提议中 | — |
| **ADR-0022** | 插件加载机制 (已规划) | 📋 待创建 | — |
| **ADR-0023** | ToolResult 标准化 | 🔄 P1 已实施, P2-P4 待 | 简化为 {ok, data, meta} 信封 |
| **ADR-0030** | AsyncRuntime 双层异步架构 | ✅ 已批准 | → 替代 ADR-0020 Phase 2/4 协程计划 |
| **ADR-0032** | CostCollector 成本收集 | 🔍 提议中 | — |
| **ADR-0031** | IExecutionPolicy 执行策略 | 🔍 提议中 | — |
| **ADR-0033** | Session 层级模型 | 🔍 提议中 | — |
| **ADR-0034** | IModelRouter 模型路由 | 🔍 提议中 | — |
| **ADR-0036** | 混合内核三层架构 | 🔍 提议中 | ← 取代 `multi-domain-agent-architecture.md` 服务调用部分 |

> **ADR-0035（DomainAgentDescriptor 领域智能体注册契约）**：经 2026-05-28 评审，决定**延迟创建**。先实施 ADR-0022（插件加载）和 ADR-0034 P1（ModelRegistry），再以 C++ 抽象类 `IDomainAgent` 形式实现（静态链接 MVP，避免 C-API 和 DLL 复杂度）。

### 生命周期阶段

```
阶段1: 基础设施 (ADR-0001~0009)
阶段2: 记忆系统 (ADR-0010~0014)
阶段3: 推理能力 (ADR-0015~0018)
阶段4: 智能体层 (ADR-0019~0023) ← 当前
阶段5: 异步架构 (ADR-0030) ← 新增
阶段5: 安全与执行策略 (ADR-0031 + ADR-0004 V2)
阶段6: Session 层级 (ADR-0033)
阶段7: 模型路由 (ADR-0034) ← 当前
阶段8: 混合内核架构 (ADR-0036)
阶段9: 领域智能体注册契约 (ADR-0035 — 延迟，待 ADR-0022 + ADR-0034 P1 就绪)
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

ADR-0030 (AsyncRuntime) ─── 双层异步架构（新增）
    │
    │  替代 ADR-0020 Phase 2/4 的协程计划
    │  为 ADR-0019 提供 run_async() 协程实现
    │  为 ADR-0015 IPER 循环提供 Lazy<T> 状态机
    │  为 ADR-0031 审批等待提供协程挂起能力
    │
    ├── Taskflow (计算层) ─── DAG 并行、Fork/Join
    └── async_simple (控制层) ─── 协程、流式、审批等待

ADR-0032 (CostCollector) ─── 成本收集（新增）
    │
    │  订阅 ADR-0002 EventBus 的 LLMCallFinished 事件
    │  在 Taskflow 计算池中执行成本计算
    │  发布 CostUpdated 事件供 TUI 订阅
    │
    └── 依赖 ADR-0002 P1 (EventBus) + ADR-0030 P2 (AsyncRuntime)

ADR-0031 (IExecutionPolicy) ─── 执行策略（新增）
    │
    │  定义 Plan/Agent/YOLO 三模式的行为策略
    │  为 ADR-0004 ToolRegistry 提供审批决策
    │  通过 ADR-0002 EventBus 发送审批请求
    │  通过 ADR-0030 async_simple 协程等待用户响应
    │
    ├── PlanModePolicy ─── 所有写入操作需审批
    ├── AgentModePolicy ─── 遵循工具自身策略
    └── YoloModePolicy ─── 仅危险操作需审批

ADR-0033 (Session 层级模型) ─── Session 管理（新增）
    │
    │  三层会话模型：UserSession → TaskSession → SubtaskSession
    │  管理 Session 生命周期、失败重试、自动分裂
    │  通过 EventBus 发布 ModeChanged / SessionSplit 事件
    │
    ├── 依赖 ADR-0031 (IExecutionPolicy) — TaskSession 持有 current_policy
    ├── 依赖 ADR-0032 (CostCollector) — TaskSession 预算集成成本追踪
    ├── 依赖 ADR-0002 (EventBus) — 会话事件传输
    ├── 依赖 ADR-0030 (AsyncRuntime) — async_simple LazyLocals 会话传播
    └── 依赖 ADR-0023 (ToolResult) — UserSession.messages 追加写保护

ADR-0034 (IModelRouter 模型路由) ─── 模型路由（新增）
    │
    │  基座层接口：IModelRouter + ModelRegistry
    │  同步路由：根据任务类型/预算/标签选择模型
    │  非破坏性扩展：ILLMProvider 新增默认实现方法
    │
    ├── 依赖 ADR-0001 (ILLMProvider) — 扩展现有接口
    ├── 依赖 ADR-0030 (AsyncRuntime) — Phase 2/3 异步支持
    ├── 依赖 ADR-0002 (EventBus) — Phase 3 事件发布
    └── 依赖 ADR-0033 (SessionHierarchy) — RoutingContext 使用 session 信息

ADR-0036 (混合内核三层架构) ─── 系统架构总纲（新增）
    │
    │  定义三层角色：基座(kernel) / 认知(shell) / 领域(程序)
    │  定义调用方向约束、LLM 调用权归属、状态访问矩阵
    │  以 Unix 进程模型为类比心智模型
    │
    ├── 依赖 ADR-0033 (SessionHierarchy) — SubtaskSession 隔离执行
    ├── 依赖 ADR-0031 (IExecutionPolicy) — 审批作为 sudo 机制
    ├── 依赖 ADR-0034 (ModelRouter) — 模型选择
    ├── 依赖 ADR-0030 (AsyncRuntime) — 认知层异步 IPER
    └── 依赖 ADR-0019 (IInteractionBus) — 用户 IO 通道

ADR-0004 V2 (ToolRegistry 安全模型) ─── 安全模型更新
    │
    │  新增 ToolCategory（工具安全分类）
    │  新增 ApprovalPolicy（三模式审批策略）
    │  新增 LayerProfile（调用层级限制）
    │  与 ADR-0031 IExecutionPolicy 对齐
    │
    └── 依赖 ADR-0031 (IExecutionPolicy) + ADR-0002 (EventBus)
```

### 依赖方向规则

| 规则 | 内容 |
|------|------|
| **R1** | ADR-0021 依赖 ADR-0020 和 ADR-0019，方向不可逆 |
| **R2** | ADR-0019 和 ADR-0020 互相独立（可并行实施） |
| **R3** | ADR-0022 依赖 ADR-0021（必须等 PDK 定义了 .so 格式才能定义加载） |
| **R4** | ADR-0023 依赖 ADR-0019/0020/0021（必须等全线对齐后才能标准化格式） |
| **R5** | ADR-0030 独立实施，但 ADR-0020 Phase 2/4 依赖 ADR-0030 完成后才能启动 |
| **R6** | ADR-0019 的 `run_async()` 在 ADR-0030 完成后需对齐协程签名 |

---

## 三、接口交叉点

### 3.1 接口一致性矩阵

| 接口点 | ADR-0019 | ADR-0020 | ADR-0021 | ADR-0030 | 状态 |
|--------|----------|----------|----------|----------|------|
| **ToolSchema** | 未定义（引用 ADR-0004） | `std::function(json)→json` | `DECLARE_TOOL` 生成 `ToolSpec` | 未涉及 | ⚠️ 不一致 → 由 ADR-0023 统一 |
| **Event/Token** | `EventType` + `Token` 在 `events.h` | 未定义（引用 ADR-0019） | 通过 `RETURN_SUCCESS` 隐式产生 | `Generator<Token>` 流式推送 | ✅ 一致 |
| **Session** | `session_id` 统一标识 | `CognitiveTask.session_id` | 未定义 | `SessionContext` 协程管理 | ✅ 一致 |
| **Tool 注册** | 未涉及 | `ToolRegistry::call_tool(name, args)→json` | `DECLARE_TOOL` → 注册函数 | 未涉及 | ✅ 一致 |
| **DSLEngine** | `atomic<shared_ptr<IInteractionBus>>` | `unique_ptr<DSLEngine>` per Worker | 不直接引用 | `AsyncRuntime` 外部注入 | ✅ 一致 |
| **Sandbox 返回** | 未定义 | 未定义（Phase 2） | `SafeExec::run()` 模板返回 | 未涉及 | ⚠️ 缺失 → ADR-0023 |
| **Plugin Lifecycle** | 未涉及 | 未涉及 | `init/load/unload` | 未涉及 | ❌ 缺失 → ADR-0022 |
| **协程模型** | `run_async()` 回调式 | ~~自定义 `LLMTokenStream`~~ → 废弃 | 未涉及 | `Lazy<T>` + `Generator<T>` (async_simple) | ✅ ADR-0030 统一 |

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

Phase 5 (安全与执行策略)
├── ADR-0031 P1 — IExecutionPolicy 接口 + 三种模式实现
├── ADR-0031 P2 — 审批流程集成（EventBus + async_simple 协程挂起）
├── ADR-0004 P1 — ToolCategory + ApprovalPolicy + LayerProfile 元数据
├── ADR-0004 P2 — ToolRegistry 元数据注册（保留旧 API 兼容）
└── ADR-0004 P3 — 预览生成器（diff / 命令预览）
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
| `archive/adr-0030-async-runtime-dual-layer.md` | AsyncRuntime 双层异步架构 | ✅ 已批准 |
| `adr-0031-execution-policy.md` | IExecutionPolicy 执行策略 | 🔍 提议中 |
| `archive/adr-0032-cost-collector.md` | CostCollector 成本收集 | 🔍 提议中 |
| `adr-0033-session-hierarchy.md` | Session 层级模型 | 🔍 提议中 |
| `archive/adr-0034-model-router.md` | IModelRouter 模型路由 | 🔍 提议中 |
| `archive/adr-0036-hybrid-kernel-architecture.md` | 混合内核三层架构 | 🔍 提议中 |
