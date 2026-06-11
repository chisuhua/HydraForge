# ADR-002: Session & State 四层隔离模型

**ID**: ADR-002
**日期**: 2026-05-20
**状态**: 已批准（经 Oracle 确认，决策已定）
**关联**: ADR-003, ADR-005, IP-001

---

## 上下文

AgenticDSL 当前的状态管理只有一层：

```
Context (nlohmann::json) — 所有节点共享，无隔离，无作用域
```

当 AgenticDSL 要管理多个并发推理 Session（每个 Session 有自己的 KV-cache、batching 队列、解码状态）时，必须引入状态隔离。

## 可选方案

### 方案 A：单层扁平 JSON（当前）
- 优点：简单
- 缺点：多 session 状态污染、无法并行、无模块封装

### 方案 B：两层（Global + Session）
- 优点：比当前好
- 缺点：模块级状态无法独立管理，协程无法实现

### 方案 C：三层（Global + Session + Module）
- 优点：支持模块封装和协程
- 缺点：缺少调用栈帧，递归/重入无法处理

### 方案 D：四层（Global + Session + Module + Execution）（所选方案）
- 优点：完整覆盖所有场景
- 缺点：复杂度最高

## 决策

选择 **方案 D：四层隔离模型**

```
┌────────────────────────────────────────────────────┐
│  Global State                                      │
│  ├── model_weights (readonly shared)               │
│  ├── prefix_cache (readonly shared)                │
│  ├── tool_registry                                 │
│  └── session_registry                              │
├────────────────────────────────────────────────────┤
│  Session State (1:1 with session_id)               │
│  ├── session_vars: json (TLS analog)               │
│  │   ├── user_id, trace_id, current_model          │
│  │   └── 生命周期 = session 生命周期                │
│  │                                                  │
│  └── module_states: map<ModulePath, ModuleState>   │
│      ├── kv_cache (fork_behavior: deep_copy)        │
│      ├── batching (fork_behavior: inherit)          │
│      └── 生命周期 = 模块首次调用 → session 结束      │
├────────────────────────────────────────────────────┤
│  Execution Context (每 run() 新建)                  │
│  ├── stack_frames: vector<Frame> (LIFO)             │
│  │   ├── frame 0: root graph { inputs, outputs }    │
│  │   ├── frame 1: subgraph { inputs, outputs }      │
│  │   └── ...                                        │
│  └── current_node: NodePath                         │
└────────────────────────────────────────────────────┘
```

## 四层详细定义

### Global State

```
Scope:    进程级
Lifetime: 进程生命周期
Visibility: 所有 Session 只读共享
Mutation:  Write-once（初始化时写入）
```

### Session State

```
Scope:    Session 级
Lifetime: Session 创建 → 销毁
Visibility: 当前 Session 内所有模块可见

sub-structure:
  ├── session_vars:    TLS 类比，全局可见，读写自由
  └── module_states:   按模块隔离，跨调用持久化
```

### Module State

```
Scope:    模块级
Lifetime: 模块首次被调用 → Session 销毁
Visibility: 本模块私有（通过 imports 声明暴露）
Fork:     per-field 声明 (deep_copy / cow / inherit)
```

### Execution Context

```
Scope:    调用级
Lifetime: 单次 run() 调用
Visibility: 当前调用栈帧

sub-structure:
  ├── stack_frames: vector<Frame>
  │   ├── Frame: { module, inputs, outputs, temp_vars }
  │   └── 每次进入子图 Push，返回 Pop
  └── current_node: 当前执行到哪个节点
```

## Session 创建与生命周期

```yaml
## /workflow/create_session
type: tool_call
tool: session.create
arguments:
  isolation: process        # process | thread | coroutine
  max_memory_mb: 2048
output_keys: ["session_id"]

## /workflow/destroy_session
type: tool_call
tool: session.destroy
arguments:
  session_id: "{{session_id}}"
```

## 为什么没有 Session.shared

Oracle 明确建议不要 Session.shared 层，原因是：
- 跨模块共享应该通过显式的 `imports_module_state` 声明（ADR-003）
- 隐式共享池导致不可审计的偶发耦合
- 所有跨模块数据流应该可以通过静态声明追踪

## 影响

- 正面：多 session 完全隔离，互不干扰
- 正面：module_state 为实现协程提供了基础（状态可持久化可恢复）
- 正面：stack_frames 支持递归和重入
- 负面：四层状态增加了运行时复杂度
- 负面：工具调用需要透传 session_id

## 关键实现约束

```cpp
struct SessionState {
    std::string session_id;
    json session_vars;
    std::map<std::string, ModuleState> module_states;
    std::chrono::steady_clock::time_point created_at;
};

struct ModuleState {
    std::string module_path;
    json state;                    // 当前状态
    std::map<std::string, ForkBehavior> fork_behaviors;
    bool is_initialized = false;   // Lazy init
};

struct ExecutionFrame {
    std::string module_path;
    json inputs;
    json outputs;
    json temp_vars;
    NodePath current_node;
};
```

---

## Oracle 评估确认（2026-05-20）

经过两轮 Oracle 架构审查（bg_159ae7c8 + bg_c7c77504），以下设计决策已确认：

| 决策点 | Oracle 结论 | 影响 |
|-------|------------|------|
| **4 层的必要性** | ✅ 4 层合适。Session+Execution **不应合并**：Session=持久身份，Execution=瞬态调用。混淆会导致"一个 session 只能跑一个 DAG"的瓶颈。 | 保持 4 层设计 |
| **Global 层保留** | ✅ Global 必要。模型权重、prefix-cache 进程级共享。但应设计为"启动时初始化，运行时不修改"。 | 保持 Global 层 |
| **MVP 简化路径** | ✅ 建议先用 json scope nesting 做 ModuleState 的 MVP：`session.module_states["/lib/inference/kv_cache"] = {...}`，不加 schema 校验，不加 imports 声明。先把隔离跑起来，再谈类型安全。 | 新增 MVP 路径 |
| **迁移安全性** | ✅ 所有新功能都是 ADDITIVE（新增语法/节点类型），不影响现有图。现有流程图完全不受影响。 | 确认向后兼容 |
| **Session 所有权** | ✅ DSLEngine 持有 SessionRegistry 成员（与现有 ToolRegistry 模式一致）。`engine.create_session()` → `engine.run(session_id, graph)`。 | 保持当前架构风格 |
| **ModuleState 初始化** | ✅ MVP 用 Lazy Init（首次 dsl_call 时 emplace）。Prewarm 等模块 >50 或初始化含重量级操作时再加。 | 实现简单，无需静态分析 pass |
| **Fork 集成** | ✅ 复用 execute_fork_branches + complete_fork 调度逻辑。改为只为分支创建轻量 ExecutionContext（而非完整 ExecutionSession）。 | 只需改 Context 创建，不动调度核心 |

**第一轮 Oracle 问答见** [03-oracle-qa.md](03-oracle-qa.md)

---

## 关联文档

| 文档 | 关系 |
|------|------|
| [02-internal-state-model.md](02-internal-state-model.md) | 本文的细化设计 — ModuleState schema + Yield + Fork 三合一 |
| [03-oracle-qa.md](03-oracle-qa.md) | Oracle 对 6 个关键问题的完整回答，本文架构的决策依据 |
| [docs/adr/adr-0014-conversation-context.md](../../adr/adr-0014-conversation-context.md) | 现有对话上下文隔离设计，本文的四层模型是其泛化 |
| [docs/adr/adr-0008-structured-context.md](../../adr/adr-0008-structured-context.md) | 结构化的 LayeredContext，本文的 Execution 层的前身 |
| [docs/adr/adr-0003-dslengine-thread-safety.md](../../adr/adr-0003-dslengine-thread-safety.md) | 当前引擎线程安全模型，本文 Session 隔离设计的前提条件 |
| [docs/specs/dsl.md](../../specs/dsl.md) | 当前 LayeredContext（L1-L5）定义，本文 Execution 层的实现参考 |
