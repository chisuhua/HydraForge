# RF-002: Oracle 问答实录

**ID**: RF-002
**日期**: 2026-05-20
**状态**: 草案
**关联**: ADR-002, ADR-003

---

## 背景

在 Session/State 隔离模型设计过程中，提交了 6 个关键问题给 Oracle 咨询。
以下是 Oracle 的完整回答记录。

---

## Q1: 状态作用域粒度

**问题**：三层（Global / Session / Module）是否足够？

**Oracle 建议**：四层，去掉 Session.shared 层。

| 层 | 作用域 | 生命周期 | 用途 |
|----|--------|---------|------|
| Global | 进程 | 进程 | 模型权重、prefix cache（只读）、工具注册表 |
| Session | Session | Session | Session 变量（TLS 类比）、配置、用户身份 |
| Module | 模块 | Session | 跨调用持久状态（KV-cache page table、batch queue） |
| Execution | 调用 | 瞬态 | 每次 run() 的局部变量、栈帧、节点输出 |

**为什么去掉 Session.shared**：
跨模块共享应该通过声明的 imports（Q4）显式完成，而不是通过一个隐式的共享池。
这可以防止意外耦合，并使数据流可审计。

**Session 结构**：

```cpp
struct Session {
    json session_vars;                         // 任意模块读写
    std::map<std::string, ModuleState> modules; // 每个模块独立
};
```

---

## Q2: Yield/Stream 实现模型

**问题**：协程风格（Option A）还是状态机风格（Option B）？

**Oracle 建议**：Option B（状态机风格），DSL 层面看起来像协程语法糖。

**理由**：
- **Agent 友好**：LLM 生成显式状态机比推理 suspension 点更可靠
- **C++20 现实**：原生协程需要复杂的 promise 类型、分配器和对称转移——自进化系统的 bug 面太大
- **性能**：状态机避免每次 yield 的协程帧分配（对 token 频率级别的推理至关重要）
- **可观测性**：`resume_at: "/module/node_id"` 可以轻松记录和 checkpoint

**实现方式**：

```yaml
# DSL 中看起来像协程，实际是状态机
type: yield
value: "{{token}}"
# 运行时隐式记录：resume_at = 下一个节点
```

运行时在 yield 时记录 `(module_id, node_id, module_state_snapshot)`。
恢复时恢复状态并跳转到目标节点。

---

## Q3: 模块状态初始化时机

**问题**：Lazy init、Eager init 还是 Declared init？

**Oracle 建议**：混合——Declared init 作为默认，Lazy init 作为动态图的 fallback。

**方法**：
1. 静态分析 pass：加载 .md 图时，收集所有 DSL_CALL 目标的传递闭包
2. Schema 驱动分配：每个模块的 .md 声明其 state schema；运行时在 session 创建时预分配所有可达模块
3. Lazy fallback：对于通过条件分支或动态分发到达的模块，在首次调用时在 per-module mutex 下分配

**原因**：
LLM 推理图大部分是静态的（你知道会调用哪些模块）。
Declared init 消除了热路径的首次调用延迟，而 Lazy init 处理边界情况而不浪费。

---

## Q4: ModuleState 访问控制

**问题**：全封装、声明式导入、还是全局可见？

**Oracle 建议**：Option 2 — 声明式导入，默认 readonly。

```yaml
# batching.md
imports_module_state:
  - module: "/lib/inference/kv_cache"
    fields:
      - name: page_table
        access: readonly
      - name: current_pos
        access: readwrite   # 必须显式声明
```

**理由**：
- **封装**：Module state 默认私有；破坏面最小化
- **可审计**：静态声明 → 运行时可以记录每次跨模块访问
- **Agent 友好**：LLM 生成显式的 `imports_module_state` 块；没有隐式魔法
- **性能**：访问控制是声明式的；运行时在加载验证后使用直接指针（零开销）

---

## Q5: Session Fork 语义

**问题**：One-size-fits-all 还是 per-field 声明？

**Oracle 建议**：每个 state 字段显式声明 fork_behavior，底层实现 copy-on-write。

```yaml
module_state:
  kv_cache:
    type: buffer
    scope: session
    fork_behavior: deep_copy    # 每个分支独立（beam search）
  prefix_hash:
    type: map
    scope: session
    fork_behavior: share_readonly  # 所有分支共享，写时复制
  generation_config:
    type: object
    scope: session
    fork_behavior: inherit        # 复制父值，之后独立
```

**Copy-on-write 是对的，但显式声明才是正确的接口。** 不要向 DSL 作者隐藏语义——推理优化需要理解数据移动。

**是否可以更简单？** 不能。Beam search、speculative decoding 和 continuous batching 都需要不同的 fork 行为。一刀切会破坏用例。

---

## Q6: 决策矩阵

**Oracle 提供的完整决策矩阵**：

| 维度 | Execution Context | Session Variables | Module State | Global State |
|------|-----------------|-----------------|-------------|-------------|
| 生命周期 | Transient（per run） | Session | Session | Process |
| 作用域 | Node / call | Session-wide | Module-wide | All sessions |
| 可见性 | Private（node only） | Session | Module（通过 imports） | Readonly shared |
| 变更 | Read-write | Read-write | Read-write | Write-once（init） |
| 序列化 | No | Yes（checkpoint） | Yes（checkpoint） | No（reconstruct） |
| Fork 行为 | N/A | Inherit / copy | Per-field declared | N/A |

---

## 实施优先级（Oracle 建议）

1. **ModuleState schema + 声明式导入**（解锁 Q3, Q4, Q5）
2. **Session registry + session_vars 层**（解锁 Q1, Q6）
3. **状态机 yield（NodeExecutor）**（解锁 Q2）
4. **Fork 语义（TopoScheduler）**（解锁 Q5）
5. **Checkpoint/restore for session_vars + module_states**（解锁 Q6）
6. **静态分析 pass for declared init**（优化）

**工作量估算**：1-2 周核心，2-3 周全量 checkpoint/fork 优化。

---

## 第二轮 Oracle（bg_c7c77504）：5 个实施细节

### Q7: ModuleState 初始化策略

**问题**：MVP 用 Lazy Init 还是 Declared Prewarm？

**Oracle 建议**：**Lazy Init 足够**。

- MVP 只需在 `ExecutionSession::module_states_` 中做一个 `emplace`：`module_states_[path] = json::object()`
- 当前代码库无静态分析基础设施，Prewarm 需要 `build_reachability_graph()` 新 pass
- 推理标准库 ~8 个模块，Lazy Init 的首次调用延迟在微秒级，不值得 Prewarm 的工程投入
- **何时需要 Prewarm**：当模块数量 > 50 或初始化含重量级操作（如 GPU 内存分配）时

### Q8: Yield 恢复机制

**问题**：a) 所有权 b) 触发 c) 快照深度

**Oracle 建议**：

a) **Session 持有** `std::optional<YieldState> pending_yield_`。Scheduler 只管调度顺序，NodeExecutor 只管当前节点，Session 是状态的唯一自然所有者。

b) **Option A：同模块下次 dsl_call 自动恢复**。推理标准库场景中，Agent 写 `dsl_call { subgraph: "/lib/inference/engine" }`，NodeExecutor 自动检查该模块是否有 `pending_yield_`。有则恢复到保存的 node_id，无则常规执行入口节点。不新增节点类型。

c) **MVP 只保存 `(module_id, node_id)`（~64 字节）**。模块状态已通过 ModuleState 机制持久化在 Session 中，不需要再快照。完整快照仅在 fork/checkpoint 时需要。

**YieldState 代码结构**：

```cpp
struct YieldState {
    std::string module_id;
    std::string node_id;
    nlohmann::json module_state_snapshot;  // 空（MVP），fork 时使用
};
```

### Q9: Stream 原语（YIELD vs CONTINUE_STREAM vs STOP_STREAM）

**Oracle 建议**：**Option A：单 YIELD 节点 + mode 参数**。

```yaml
type: yield
value: "{{token}}"
mode: "next"    # next | continue | stop
```

不新增 CONTINUE_STREAM / STOP_STREAM 节点类型。理由：
- 只需要一个 NodeType 枚举值
- Executor dispatch switch 只加一个 case
- Agent 生成时只需记住 `type: yield` 一个名字
- `mode: stop` 信号足够让消费者判断流结束

### Q10: Session 所有权

**问题**：DSLEngine 持有、全局单例、还是外部传入？

**Oracle 建议**：**Option A：DSLEngine 持有 SessionRegistry 成员**。

- 当前架构中 DSLEngine 已持有 ToolRegistry 成员（非单例），保持一致
- 外部 API：`engine.create_session() → auto id = ... → engine.run(id, graph) → engine.destroy_session(id)`
- TopoScheduler::run() 改为接收 session_id，从 DSLEngine 获取 Session 引用
- 不与"多实例线程安全"冲突（每个 DSLEngine 实例有自己的 SessionRegistry）

### Q11: Fork 与现有 TopoScheduler 集成

**问题**：现有 fork 代码有多少可复用？

**Oracle 建议**：

**可复用**：
- `execute_fork_branches()` 的分支遍历和调度逻辑
- `complete_fork()` 的 join_mode（all/any）逻辑

**需修改**：
- 当前每个分支创建完整 ExecutionSession（太重）→ 改为只创建轻量 `ExecutionContext`
- 分支共享同一个 Session（访问 session_vars + module_states），不拷贝
- 模块写遵循主 Session 的 fork_behavior（MVP 全部 deep_copy）

**最小改动**：

```cpp
// MVP：分支只新建 ExecutionContext，不从 Session 拷贝
struct ExecutionContext {
    nlohmann::json inputs;
    nlohmann::json outputs;
    NodePath current_node;
};
// TopoScheduler::execute_fork_branches 复用，但创建 ExecutionContext 而非 ExecutionSession
```

---

## 关联文档

| 文档 | 关系 |
|------|------|
| [01-isolation-model.md](01-isolation-model.md) | Oracle Q1 的决策实现 — 四层隔离模型 |
| [02-internal-state-model.md](02-internal-state-model.md) | Oracle Q2-Q5 的决策实现 — ModuleState + Yield + Fork |
| [IP-001: 实施路线图](../implementation-roadmap/01-roadmap.md) | Oracle 实施优先级建议的具体化 — 6 步实施计划 |
| [docs/adr/adr-0003-dslengine-thread-safety.md](../../adr/adr-0003-dslengine-thread-safety.md) | 引擎并发模型，Oracle 的设计前提 |
