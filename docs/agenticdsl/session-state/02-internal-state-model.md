# ADR-003: Session 内部状态模型（ModuleState + Yield + Fork）

**ID**: ADR-003
**日期**: 2026-05-20
**状态**: 已批准（经 Oracle 确认，决策已定）
**关联**: ADR-002, IP-001
**调研依据**: Oracle 架构审查（bg_159ae7c8, bg_c7c77504）

---

## 上下文

本 ADR 合并三个紧密相关的子议题，因为它们依赖同一个核心概念——**Module State**：

1. **Module State 系统** — 模块如何声明和访问持久化状态
2. **Yield/Stream 模型** — 如何实现 token-by-token 生成器
3. **Fork 语义扩展** — 并行分支时状态如何复制

---

## 议题 3A：Module State 系统

### 声明语法

```yaml
### AgenticDSL `/lib/inference/kv_cache`
# --- BEGIN AgenticDSL ---
graph_type: subgraph
signature: "(action: string, token_id?: int) -> (result: json)"

module_state:
  page_table:
    type: array<int>
    scope: session
    init: "[]"
    fork_behavior: deep_copy
  current_pos:
    type: int
    scope: session
    init: 0
  cache_misses:
    type: int
    scope: session
    init: 0
    fork_behavior: share_readonly
# --- END AgenticDSL ---
```

### 跨模块访问（imports_module_state）

```yaml
### batching.md
# --- BEGIN AgenticDSL ---
imports_module_state:
  - module: "/lib/inference/kv_cache"
    fields:
      - name: page_table
        access: readonly
      - name: current_pos
        access: readonly

module_state:
  queue:
    type: array
    scope: session
    init: "[]"
# --- END AgenticDSL ---
```

**规则**：
- 默认：module_state 对模块外部**不可见**
- 通过 `imports_module_state` 显式声明导入
- 外部访问默认 **readonly**，readwrite 必须显式声明
- 所有跨模块访问在 parse 时验证，不通过则报错

### 初始化时机

```
决策（已批准）：Lazy Init（首次调用时创建）
            Declared Prewarm 为未来优化，MVP 不实现

默认行为（MVP）：
  模块首次被 dsl_call 调用 → ExecutionSession.module_states_ 检查
  → 不存在 → emplace 默认值（json scope nesting）→ 执行

实现代码（MVP）：
  // ExecutionSession::get_or_init_module_state(path)
  if (!module_states_.contains(path)) {
      module_states_[path] = nlohmann::json::object();
  }
  return module_states_[path];

未来优化（Declared Prewarm）：
  当模块数量 > 50 或首次调用含重量级初始化时启用。
  通过 build_reachability_graph() 静态分析可达模块，
  Session 创建时预分配，消除热路径首次调用延迟。
  当前: 不实现。

为什么 MVP 不做 Prewarm（Oracle 确认）：
  - 推理标准库 ~8 个模块，Lazy Init 首次调用延迟在微秒级
  - 当前代码库无静态分析基础设施，投入产出比低
  - Prewarm 的收益只有当模块 > 50 时才显著
```

---

## 议题 3B：Yield/Stream 模型

### 决策

选择 **状态机风格（Option B）**，而非原生 C++20 协程。

| 因素 | 协程风格 | 状态机风格（选） |
|------|---------|----------------|
| C++20 实现复杂度 | 高（promise_type, allocator, symmetric transfer） | 低（纯 DAG 调度） |
| Agent 生成正确性 | 低（隐式控制流易错） | 高（显式 resume_at） |
| 性能 | 对高频 yield 优（零开销暂停） | 中（重入路径判断） |
| 序列化/迁移 | 难（调用栈快照） | 易（module_state 完整） |
| 调试 | 难（隐式恢复点） | 易（显式路径） |

### 语法

```yaml
# token_generator.md
graph_type: generator
signature: "(prompt: string, max_tokens: int) -> stream<token: string>"

module_state:
  position: { type: int, scope: session, init: 0 }

nodes:
  - id: check_complete
    type: assert
    condition: "{{module_state.position}} < {{inputs.max_tokens}}"
    next: ["/token_generator/forward"]

  - id: forward
    type: tool_call
    tool: inference.forward_token
    arguments:
      session: "{{session_id}}"
      position: "{{module_state.position}}"
    output_keys: ["token"]
    next: ["/token_generator/yield_token"]

  - id: yield_token
    type: yield
    value: "{{token}}"
    # resume_at 隐式 = 下一个节点（/token_generator/update_state）
    next: ["/token_generator/update_state"]

  - id: update_state
    type: assign
    assign:
      module_state.position: "{{module_state.position + 1}}"
    next: ["/token_generator/check_complete"]
```

### 运行时行为（Oracle 确认）

```
YieldState 结构：
  struct YieldState {
      std::string module_id;       // 哪个模块 yield 了
      std::string node_id;         // 从哪里 yield（resume_at）
      nlohmann::json snapshot;     // 完整快照（仅 fork/checkpoint 时使用）
  };

所有权：Session（ExecutionSession 持有 std::optional<YieldState> pending_yield_）
理由：Scheduler 只管调度，Executor 只管执行，Session 是状态的唯一所有者。

恢复触发器：同模块下次 dsl_call 自动恢复
  推理标准库场景：Agent 写 dsl_call { subgraph: "/lib/inference/engine" }
  → NodeExecutor 检查该模块是否有 pending_yield_
  → 有：恢复到保存的 node_id，不执行常规入口节点
  → 无：常规执行入口节点

轻量级恢复（MVP，Oracle 确认）：
  每次 yield 只保存 (module_id, node_id) 恢复位置（~64 字节）。
  模块状态通过 ModuleState 机制已持久化在 Session 中，不需要再快照。
  完整快照仅在 fork/checkpoint 时需要。
```

### 消费方式

```yaml
## /workflow/generate_stream
# MVP：消费者通过 dsl_call 调用生成器，自动检测 pending_yield
type: dsl_call
subgraph: "/lib/inference/token_generator"
session: "{{session_id}}"
input:
  prompt: "Hello, world"
  max_tokens: 100
# 结果包含 stream 标记 + 每个 token
# 消费者循环调用直到 generator 返回 end 信号

## /workflow/on_token
type: assign
assign:
  response: "{{response}} + {{token}}"
next: ["/workflow/maybe_stop"]
```

### 与 tool_call 的关系

yield 不同于 tool_call：
- tool_call：调用外部工具，等待结果，继续
- yield：**暂停当前模块执行**，返回控制权给调用者，等待 resume

---

## 议题 3C：Fork 语义扩展

### 当前 Fork 行为

```cpp
// 当前 ForkNode
struct ForkNode : public Node {
    std::vector<NodePath> branches;  // 并行分支
    // 只有 context_isolation: deep_copy
};
```

### 扩展后

```yaml
## /fork/beam_search
type: fork
branches:
  - /decode/branch_1
  - /decode/branch_2
fork_behaviors:
  kv_cache: deep_copy       # 每个分支独立 KV-cache（beam search 必需）
  prefix_hash: share_readonly  # 共享 prefix cache（只读）
  generation_config: inherit    # 复制父值，之后独立
```

### 三种行为

| behavior | 语义 | 使用场景 |
|----------|------|---------|
| `deep_copy` | 每个分支获得完整独立副本 | KV-cache（beam search） |
| `share_readonly` | 所有分支共享同一个，直到写触发复制 | prefix cache |
| `inherit` | 复制父值的当前快照，之后独立 | generation config |

### 实现（Copy-on-Write）

```cpp
class CowState {
    std::shared_ptr<json> data_;  // 共享底层数据
    std::string owner_;            // 当前拥有者（session_id + branch_id）
    
    json& write() {
        if (owner_ != current_branch_) {
            data_ = std::make_shared<json>(*data_);  // 写时复制
            owner_ = current_branch_;
        }
        return *data_;
    }
    
    const json& read() const {
        return *data_;  // 读零开销
    }
};
```

### 分支与 module_state 交互

```
Session A
  ├── module_states: { kv_cache, batching, ... }
  │
  └── fork ──→ Session A.B1
  │                ├── kv_cache: deep_copy (独立)
  │                ├── prefix_hash: share_readonly (共享)
  │                └── generation_config: inherit (独立)
  │
  └── fork ──→ Session A.B2
                   ├── kv_cache: deep_copy (独立)
                   ├── prefix_hash: share_readonly (共享)
                   └── generation_config: inherit (独立)
```

---

## Oracle 最终确认（2026-05-21）

两轮架构审查后的最终决策汇总：

| 议题 | 决策 | MVP 实现 |
|------|------|---------|
| **ModuleState Init** | Lazy Init 优先。Prewarm 为未来优化。 | `if (!module_states_.contains(path)) module_states_[path] = json::object()` |
| **Yield 所有权** | Session 持有 `std::optional<YieldState> pending_yield_` | 在 ExecutionSession 中加一个字段 |
| **Yield 恢复** | 同模块下次 dsl_call 自动检测并恢复 | NodeExecutor 检查 pending_yield_ |
| **轻量级恢复** | 仅保存 (module_id, node_id) ~64 字节 | 不保存完整快照 |
| **Stream 原语** | 单 YIELD 节点 + mode 参数 (next/continue/stop) | 一个 NodeType 枚举值，一个 dispatch 分支 |
| **Fork MVP** | 仅 deep_copy，per-field 声明和 COW 为未来优化 | 复用现有 execute_fork_branches 调度，改为创建轻量 ExecutionContext |
| **Fork 复用** | 复用调度逻辑（execute_fork_branches + complete_fork） | 改 Context 创建，不改调度核心 |

### 对现有 Fork 设计的影响

当前文档中 fork_behavior per-field 声明和 COW 实现（第 239-266 行）保留作为架构参考，但 **不在 MVP 中实现**。MVP 只做：

```cpp
// MVP: 每个分支获得一个 ExecutionContext（轻量）
// 复用 TopoScheduler::execute_fork_branches 的调度逻辑
struct ExecutionContext {
    nlohmann::json inputs;
    nlohmann::json outputs;
    NodePath current_node;
};
// 不再创建完整 ExecutionSession，不拷贝 session_vars / module_states
```

---

## 三者的关系

```
ModuleState 是基础（提供持久化能力）
    ↓
Yield 依赖 ModuleState（保存/恢复状态）
    ↓
Fork 也依赖 ModuleState（决定每个字段如何复制）

没有 ModuleState → Yield 无法实现（因为不知道保存什么）
没有 ModuleState → Fork 只能全量 deep_copy（太浪费/太粗暴）
```

## 实现优先级（Oracle 确认后更新）

```
MVP（当前实施）：
P0: Lazy ModuleState（json scope nesting，无需 schema，无需 imports 声明）
P1: ExecutionSession 加 pending_yield_ 字段
P2: YIELD 节点类型（单 node type + mode 参数，executor dispatch）
P3: Fork MVP（复用 execute_fork_branches，创建 ExecutionContext 而非 ExecutionSession）

未来优化（不在 MVP 中）：
P4: ModuleState schema 定义 + 解析（parser）
P5: imports_module_state 跨模块访问（parser + runtime）
P6: fork_behavior per field + COW 实现（scheduler）
P7: Declared Prewarm 静态分析（library_loader）
```

---

## 关联文档

| 文档 | 关系 |
|------|------|
| [01-isolation-model.md](01-isolation-model.md) | 四层隔离模型 — 本文 ModuleState、Yield、Fork 的架构容器 |
| [03-oracle-qa.md](03-oracle-qa.md) | Oracle 对 Q2（yield）、Q3（init）、Q4（imports）、Q5（fork）的原始建议 |
| [IP-002: 扩展点映射](../implementation-roadmap/02-code-mapping.md) | 本文设计在 NodeExecutor、TopoScheduler 中的精确代码改动位置 |
| [docs/adr/adr-0008-structured-context.md](../../adr/adr-0008-structured-context.md) | 结构化 Context — LayeredContext 的分层经验被 ModuleState 复用 |
| [docs/specs/dsl.md](../../specs/dsl.md) | 当前节点类型列表（10 种），YIELD 是新类型需扩展 |
