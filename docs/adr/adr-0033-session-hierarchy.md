# ADR-0033: Session Hierarchy 执行会话层级体系

## 状态

**✅ Approved** (2026-07-02, Sprint 15 C5 ship — 三层会话模型 + DSLEngine 会话感知接口完整实现)

> **C5 实施注记 (2026-07-02)**：OpenSpec change `2026-06-26-adr-0033-session-hierarchy` ship。
> 关键 ship: (1) 新建 `src/core/types/session.h` + `.cpp` — UserSession/TaskSession/SubtaskSession 完整类型定义
> 与实现; (2) 容器采用 `std::deque` (非 vector) 确保地址稳定性 (Metis F1/F2); (3) TaskSession
> 持有 `shared_ptr<IExecutionPolicy>` (与 DSLEngine 共享, Oracle R3); (4) `DSLEngine::run(UserSession&, ...)`
> 两个重载 (Context + LayeredContext 桥接); (5) `failure_count_` 仅可重试错误递增 (Oracle R6),
> <3→KeepSession, ≥3→NewSession; (6) TopoScheduler 签名完全不变 (Oracle R4), SubtaskSession 创建/归档
> 在 DSLEngine 层; (7) 54/54 ctest 零回归 (52 baseline + 2 新测试: test_session 5 unit + test_dslengine_session 2 integration);
> (8) 不重命名 ExecutionSession (Oracle R1), 不新增 BudgetController cost_limit API (Oracle R2)。
> 变更依据: `openspec/changes/2026-06-26-adr-0033-session-hierarchy/`。
> 
> **C1 迁移注记 (2026-06-08, commit 3f28020)**：`DagExecutionContext` 构造参数与成员变量中 `LlamaAdapter* llm_adapter` 已替换为 `ILLMProvider* llm_provider`（抽象流式接口，详见 ADR-0001）。原 `LlamaAdapter` 仍可用但需通过 `LlamaAdapterProvider` 包装后注入。

## 领域

基座 / 会话管理 / 状态编排

## 关联

- ADR-0031（IExecutionPolicy）— TaskSession 持有当前执行策略
- ADR-0032（CostCollector）— TaskSession 预算集成成本追踪
- ADR-0023（ToolResult）— UserSession.messages 追加写保护
- ADR-0030（AsyncRuntime）— async_simple LazyLocals 用于会话传播
- ADR-0004 V2（ToolRegistry 安全）— 权限检查使用会话上下文

---

## 背景

### 当前代码库状态

| 组件 | 现状 | 评估 |
|------|------|------|
| **ExecutionSession** | ✅ 已实现——封装单次执行状态 | 需重命名为 DagExecutionContext |
| **Context** | ✅ `using Context = nlohmann::json;` | JSON 类型直接暴露 |
| **TopoScheduler** | ✅ fork/join 通过 `execute_single_branch()` 隔离分支状态 | 需返回 SubtaskSession |
| **BudgetController** | ✅ 计数限制（节点数、LLM 调用、时长） | 需扩展 USD 成本维度 |
| **DSLEngine** | ✅  stateless `run(Context)` | 需新增带会话的重载 |
| **Session 层级** | ❌ **零实现**——无 UserSession/TaskSession/SubtaskSession | 需要新增 |

### 问题

1. **无会话层级**：多次用户交互（multi-turn）之间无法共享状态，每次调用 `run()` 相互独立
2. **ExecutionSession 命名冲突**：现有 `ExecutionSession` 与 TaskSession 概念重叠，需澄清
3. **Context 生命周期不清**：JSON context 在多次执行间如何保持、传递、清理未定义
4. **分叉状态丢失**：fork/join 的分支执行结果仅存为临时 `Context`，无法追溯分支历史
5. **失败重试粒度不清**：IPER retry 时是否复用同一会话？失败计数挂在哪个层级？

---

## 决策

### 1. 三层会话模型

```
┌─────────────────────────────────────────────────────────────┐
│  UserSession                                                │
│  ├── messages: Vector<ToolResult>  (追加写，ADR-0023)     │
│  ├── task_sessions: Vector<TaskSession>  (历史)            │
│  ├── current_task_session: Option<TaskSession>             │
│  ├── user_id: String                                       │
│  └── created_at: Timestamp                                 │
├─────────────────────────────────────────────────────────────┤
│  TaskSession                                                │
│  ├── user_session: UserSession&  (反向引用)                │
│  ├── subtask_sessions: Vector<SubtaskSession>  (分支历史)   │
│  ├── current_policy: IExecutionPolicy*                     │
│  ├── budget: ExtendedBudgetController  (ADR-0032)          │
│  ├── failure_count: u32                                    │
│  ├── status: active | completed | failed                    │
│  └── created_at: Timestamp                                 │
├─────────────────────────────────────────────────────────────┤
│  SubtaskSession                                             │
│  ├── task_session: TaskSession&  (反向引用)                │
│  ├── branch_path: NodePath  (分叉路径标识)                  │
│  ├── initial_context: Context                              │
│  ├── final_context: Context                                 │
│  ├── execution_trace: Vector<TraceRecord>                  │
│  ├── status: pending | running | completed | failed          │
│  ├── started_at: Timestamp                                  │
│  └── completed_at: Option<Timestamp>                        │
└─────────────────────────────────────────────────────────────┘
```

### 2. 核心类型定义

```cpp
// ===== src/core/types/session.h =====

#ifndef AGENTICDSL_CORE_TYPES_SESSION_H
#define AGENTICDSL_CORE_TYPES_SESSION_H

#include "core/types/context.h"
#include "core/types/budget.h"
#include "core/types/tool_result.h"
#include "agenticdsl/types/trace_record.h" // 2026-06-17 OpenSpec change `2026-06-15-residual-engine-h-decoupling` 上移
#include <chrono>
#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace agenticdsl {

// 前向声明
class IExecutionPolicy;
class BudgetController;

// SubtaskSession：最小执行单元，对应 fork 分支
struct SubtaskSession {
    std::string branch_path;                          // 分叉路径标识
    Context initial_context;                          // 入口 Context 快照
    Context final_context;                           // 出口 Context（执行后填充）
    std::vector<TraceRecord> execution_trace;        // 执行轨迹
    std::string status;                              // pending | running | completed | failed
    std::chrono::steady_clock::time_point started_at;
    std::optional<std::chrono::steady_clock::time_point> completed_at;

    // 便捷构造
    static SubtaskSession make(const std::string& path, Context initial) {
        SubtaskSession s;
        s.branch_path = path;
        s.initial_context = std::move(initial);
        s.status = "pending";
        s.started_at = std::chrono::steady_clock::now();
        return s;
    }
};

// TaskSession：对应一次完整的任务执行（可包含多次 subtask）
class TaskSession {
public:
    explicit TaskSession(UserSession& user_sess);

    // 失败重试策略
    enum class FailureMode { KeepSession, NewSession };

    // 获取 UserSession 反向引用
    UserSession& user_session() { return user_session_; }
    const UserSession& user_session() const { return user_session_; }

    // SubtaskSession 管理
    SubtaskSession& create_subtask(const std::string& branch_path, Context initial_context);
    void archive_subtask_result(SubtaskSession subtask);  // 成功+失败均归档
    const std::vector<SubtaskSession>& subtask_sessions() const { return subtask_sessions_; }

    // 执行策略
    void set_policy(std::unique_ptr<IExecutionPolicy> policy);
    IExecutionPolicy* current_policy() const { return current_policy_.get(); }

    // 预算管理
    BudgetController& budget() { return budget_; }
    const BudgetController& budget() const { return budget_; }

    // 失败计数
    u32 failure_count() const { return failure_count_; }
    void increment_failure();
    FailureMode determine_failure_mode() const;  // IPER retry → KeepSession；3 次失败 → NewSession

    // 状态
    std::string status() const { return status_; }
    void set_status(const std::string& s) { status_ = s; }

    // Context 封装（TaskSession 包装 JSON Context，不替换底层类型）
    const Context& context() const { return context_; }
    Context& context() { return context_; }
    void set_context(Context ctx) { context_ = std::move(ctx); }

private:
    UserSession& user_session_;
    std::vector<SubtaskSession> subtask_sessions_;
    std::unique_ptr<IExecutionPolicy> current_policy_;
    BudgetController budget_;
    u32 failure_count_ = 0;
    std::string status_ = "active";
    Context context_;  // 当前执行上下文（TaskSession 持有）
};

// UserSession：顶层会话，对应一次用户交互周期
class UserSession {
public:
    explicit UserSession(std::string user_id);

    // messages 追加写保护（ADR-0023）
    void append_message(ToolResult msg);
    const std::vector<ToolResult>& messages() const { return messages_; }

    // TaskSession 管理
    TaskSession& create_task_session();
    TaskSession* current_task_session() const { return current_task_session_; }
    void set_current_task_session(TaskSession* ts) { current_task_session_ = ts; }
    const std::vector<TaskSession>& task_sessions() const { return task_sessions_; }

    // 元数据
    const std::string& user_id() const { return user_id_; }
    auto created_at() const { return created_at_; }

private:
    std::string user_id_;
    std::chrono::steady_clock::time_point created_at_;
    std::vector<ToolResult> messages_;  // 追加写（ADR-0023）
    std::vector<TaskSession> task_sessions_;  // 历史 TaskSession
    TaskSession* current_task_session_ = nullptr;  // 当前活跃 TaskSession
};

} // namespace agenticdsl

#endif // AGENTICDSL_CORE_TYPES_SESSION_H
```

### 3. Context 关系澄清

**关键决策**：TaskSession 包装 JSON Context，不替换底层类型。

```cpp
// ===== Context 与 Session 的关系 =====

// 现有代码（ADR-0032 延续）：
// using Context = nlohmann::json;  // 直接暴露 JSON 类型

// 新增会话层包装：
TaskSession ts(user_sess);
ts.set_context({{"key", "value"}});  // Context 被 TaskSession 持有

// Context 的来源与走向：
// 1. UserSession.messages 追加 ToolResult（ADR-0023 信封）
// 2. TaskSession.context() 持有当前任务上下文
// 3. SubtaskSession.initial_context / final_context 保存分叉快照
// 4. DSLEngine.run() 接收 Context&，不改签名为「会话感知」

// 跨会话读取（TaskSession → UserSession）：
const auto& msgs = ts.user_session().messages();  // 只读访问 messages
for (const auto& msg : msgs) {
    // 可从历史消息中提取上下文
}
```

### 4. DSLEngine 会话集成

```cpp
// ===== src/core/engine.h =====

class DSLEngine {
public:
    // 现有接口（保持兼容）
    ExecutionResult run(const Context& context = Context{});

    // 新增：会话感知接口
    ExecutionResult run(UserSession& user_sess, const std::string& message);

private:
    // 内部：委托到会话感知版本
    ExecutionResult run_impl(TaskSession& task_sess, const std::string& message);
};
```

```cpp
// ===== src/core/engine.cpp =====

ExecutionResult DSLEngine::run(UserSession& user_sess, const std::string& message) {
    // 1. 创建或复用 TaskSession
    TaskSession* task_sess_ptr = user_sess.current_task_session();
    if (!task_sess_ptr || task_sess_ptr->status() == "completed") {
        task_sess_ptr = &user_sess.create_task_session();
    }

    // 2. 检查失败模式
    if (task_sess_ptr->determine_failure_mode() == TaskSession::FailureMode::NewSession) {
        task_sess_ptr = &user_sess.create_task_session();
    }

    // 3. 执行
    auto result = run_impl(*task_sess_ptr, message);

    // 4. 追加到 UserSession.messages（ADR-0023）
    user_sess.append_message(tool_result_from_result(result));

    return result;
}
```

### 5. TopoScheduler 分支返回 SubtaskSession

```cpp
// ===== src/modules/scheduler/topo_scheduler.h =====

// 新增返回类型
struct BranchExecutionResult {
    SubtaskSession subtask;
    bool success;
    std::string error_message;
};

class TopoScheduler {
public:
    // 现有接口（保持兼容）
    ExecutionResult execute(const Context& initial_context = Context{});

    // 新增：分支执行返回 SubtaskSession
    BranchExecutionResult execute_single_branch(
        const NodePath& branch_path,
        const Context& initial_context,
        TaskSession& task_sess  // 用于创建 SubtaskSession
    );
};
```

```cpp
// ===== src/modules/scheduler/topo_scheduler.cpp =====

BranchExecutionResult TopoScheduler::execute_single_branch(
    const NodePath& branch_path,
    const Context& initial_context,
    TaskSession& task_sess
) {
    // 1. 创建 SubtaskSession
    auto subtask = task_sess.create_subtask(branch_path, initial_context);
    subtask.status = "running";

    Context ctx = initial_context;
    try {
        // 2. 执行（复用原有逻辑）
        ctx = execute_single_branch_impl(branch_path, initial_context);
        subtask.final_context = ctx;
        subtask.status = "completed";
        subtask.completed_at = std::chrono::steady_clock::now();

        // 3. 归档结果（成功+失败均归档）
        task_sess.archive_subtask_result(std::move(subtask));

        return {std::move(subtask), true, ""};
    } catch (const std::exception& e) {
        subtask.final_context = ctx;
        subtask.status = "failed";
        subtask.completed_at = std::chrono::steady_clock::now();
        task_sess.archive_subtask_result(std::move(subtask));

        return {std::move(subtask), false, e.what()};
    }
}
```

### 6. ExecutionSession → DagExecutionContext 重命名

```cpp
// ===== src/modules/scheduler/execution_session.h =====

namespace agenticdsl {

// 重命名：避免与 TaskSession 概念冲突
// ExecutionSession（旧） → DagExecutionContext（新）
class DagExecutionContext {
public:
    using AppendGraphsCallback = std::function<void(std::vector<ParsedGraph>)>;

    DagExecutionContext(
        std::optional<ExecutionBudget> initial_budget,
        ToolRegistry& tool_registry,
        ILLMProvider* llm_provider, // C1 后 (commit 3f28020) 通过 ILLMProvider 注入 LLM
        // 原 LlamaAdapter 仍可用但需通过 LlamaAdapterProvider 包装。
        ResourceManager& resource_manager,
        const std::vector<ParsedGraph>* full_graphs,
        AppendGraphsCallback append_graphs_callback = nullptr
    );

    struct ExecutionResult {
        Context new_context;
        bool success;
        std::string message;
        std::optional<NodePath> snapshot_key;
        std::optional<NodePath> paused_at;
    };

    ExecutionResult execute_node(Node* node, const Context& initial_context);
    bool is_budget_exceeded() const;
    const TraceExporter& get_trace_exporter() const;
    const BudgetController& get_budget_controller() const;
    const ContextEngine& get_context_engine() const;

private:
    ResourceManager& resource_manager_;
    ContextEngine context_engine_;
    BudgetController budget_controller_;
    TraceExporter trace_exporter_;
    ToolRegistry& tool_registry_;
    ILLMProvider* llm_provider_;
    const std::vector<ParsedGraph>* full_graphs_;
    AppendGraphsCallback append_graphs_callback_;
    std::unordered_map<NodePath, std::vector<NodePath>> pending_dynamic_deps_;
    std::unordered_map<NodePath, nlohmann::json> dynamic_wait_for_expressions_;
};

// 别名：兼容旧代码（可删除，取决于迁移范围）
using ExecutionSession = DagExecutionContext;

} // namespace agenticdsl
```

### 7. BudgetController 扩展 USD 成本

```cpp
// ===== src/modules/budget/budget_controller.h =====

// 在现有 BudgetController 基础上扩展（ADR-0032 集成）

class BudgetController {
public:
    explicit BudgetController(std::optional<ExecutionBudget> initial_budget = std::nullopt);

    // 现有计数接口
    bool try_consume_node();
    bool try_consume_llm_call();
    bool try_consume_subgraph_depth();
    bool exceeded() const;

    // ADR-0032 新增：USD 成本接口
    void set_cost_limit(double usd_limit);
    double current_cost() const { return current_cost_; }
    bool try_consume_cost(double usd_amount);

private:
    std::optional<ExecutionBudget> budget_opt_;
    NodePath termination_target_ = "/__system__/budget_exceeded";
    double current_cost_ = 0.0;          // ADR-0032 新增
    std::optional<double> cost_limit_;    // ADR-0032 新增
};
```

### 8. Session 分裂策略

```cpp
// ===== Session 分裂逻辑 =====

// 触发条件：自动分裂，无用户确认
// 1. IPER retry → 保持同一 TaskSession（FailureMode::KeepSession）
// 2. 累计 3 次失败 → 新建 TaskSession（FailureMode::NewSession）
// 3. TUI 通知（异步事件推送，不阻塞执行）

TaskSession::FailureMode TaskSession::determine_failure_mode() const {
    if (failure_count_ < 3) {
        return FailureMode::KeepSession;  // IPER retry：复用同一会话
    }
    return FailureMode::NewSession;  // 超过次数：新建会话
}

// TUI 通知（通过 EventBus 或直接回调）
void on_session_split(TaskSession& old_sess, TaskSession& new_sess) {
    // TUI 展示分裂原因（IPER retry 或 3 次失败）
    // old_sess 的 subtask_sessions 历史完整保留
}
```

---

## 实现计划

### Phase 1：类型与重命名（1 天）

**目标**：创建会话类型，rename ExecutionSession → DagExecutionContext，无行为变更。

| 文件 | 操作 |
|------|------|
| `src/core/types/session.h` | 新建：UserSession、TaskSession、SubtaskSession 定义 |
| `src/core/types/session.cpp` | 新建：Session 方法实现 |
| `src/modules/scheduler/execution_session.h` | 重命名类为 `DagExecutionContext`，添加 `using ExecutionSession = DagExecutionContext;` 别名 |
| `src/modules/scheduler/execution_session.cpp` | 同上文件重命名 |
| `src/modules/scheduler/topo_scheduler.h` | 新增 `execute_single_branch()` 返回 `BranchExecutionResult` |
| `CMakeLists.txt` | 新增 `session.cpp` 编译单元 |

**验证**：
```bash
# 编译通过，无行为变更
cmake .. -DAGENTICDSL_BUILD_TESTS=ON && make -j$(nproc)
ctest --output-on-failure
```

### Phase 2：会话集成（2 天）

**目标**：DSLEngine 新增会话感知接口，TopoScheduler 分支返回 SubtaskSession。

| 文件 | 操作 |
|------|------|
| `src/core/engine.h` | 新增 `run(UserSession&, const std::string&)` 重载 |
| `src/core/engine.cpp` | 实现会话感知版本 |
| `src/modules/scheduler/topo_scheduler.cpp` | `execute_single_branch()` 返回 `SubtaskSession`，自动归档 |
| `src/modules/budget/budget_controller.h` | 扩展 USD 成本接口（ADR-0032 集成） |
| `src/modules/budget/budget_controller.cpp` | 实现 `try_consume_cost()` |
| `tests/test_session.cpp` | 新建：Session 层级单元测试 |

**验证**：
```bash
# Session 创建、失败重试、分裂测试通过
ctest -R "session" --output-on-failure
```

### Phase 3：持久化层（可选，延期）

**目标**：Session 持久化到磁盘，支持进程重启恢复。

| 文件 | 操作 |
|------|------|
| `src/core/persistence/session_persistence.h` | 新建：抽象持久化接口 |
| `src/core/persistence/json_session_store.h` | 新建：JSON 文件存储实现 |
| `UserSession::save() / load()` | 新增序列化方法 |

**验证**：
```bash
# 序列化/反序列化测试通过
ctest -R "persistence" --output-on-failure
```

---

## 验收标准

### 功能验收

| 验收项 | 验证方法 |
|--------|---------|
| UserSession.messages 追加写保护 | `const std::vector<ToolResult>& messages() const` 无 mutator |
| TaskSession 失败计数正确 | 模拟 3 次失败，确认第 4 次创建新会话 |
| IPER retry 复用 TaskSession | 失败计数 < 3 时 `determine_failure_mode() == KeepSession` |
| SubtaskSession 结果归档 | fork/join 执行后 `task_sess.subtask_sessions()` 包含所有分支结果 |
| DSLEngine 会话重载 | `run(user_sess, msg)` 正确创建/复用 TaskSession |
| DagExecutionContext 别名兼容 | `using ExecutionSession = DagExecutionContext;` 编译通过 |

### 集成验收

| 验收项 | 验证方法 |
|--------|---------|
| BudgetController USD 接口 | `try_consume_cost()` 正确累计 `current_cost_` |
| TopoScheduler 分支返回 | `execute_single_branch()` 返回包含 `SubtaskSession` 的 `BranchExecutionResult` |
| 跨会话只读 | TaskSession 可读取 UserSession.messages，不可写入 |
| Context 类型不变 | `using Context = nlohmann::json;` 未被替换 |

---

## 风险与缓解

| 风险 | 等级 | 缓解措施 |
|------|------|---------|
| Phase 1 别名导致链接冲突 | 中 | 若链接失败，移除 `using ExecutionSession = DagExecutionContext;` 别名，全量替换 |
| Session 内存泄漏 | 中 | Phase 3 持久化前，UserSession 限制 `task_sessions` 最大历史数量（默认 100） |
| Context JSON 类型直接暴露 | 低 | Session 层仅在边界做 JSON 转换，内部使用 `Context&` 引用 |
| 线程安全问题 | 中 | `UserSession.messages` 仅追加写（ADR-0023），TaskSession/SubtaskSession 按执行线程隔离 |
| BudgetController USD 精度 | 低 | 使用 `double`，设置合理精度阈值（$0.001） |

---

## 与 ADR-0036 的集成补充

### 1. SubtaskSession 创建者

根据 ADR-0036 的混合内核架构，SubtaskSession 的创建和所有权分离：

| 操作 | 负责方 | 说明 |
|:---|:---|:---|
| **创建**（舰队模式） | 认知层 `FleetOrchestrator` | 认知层识别并行机会后调用 `task_session.create_subtask()` |
| **创建**（未来完全隔离） | 基座层 `ToolCoordinator` | 每次 `call_tool()` 自动创建 SubtaskSession |
| **存储** | 基座层 `SessionManager` | 持有 UserSession → TaskSession → SubtaskSession 的完整层次 |
| **归档** | 基座层 `SessionManager` | `SubtaskSession` 执行完成后自动归档到 `TaskSession.subtask_sessions` |

**决策**：MVP 阶段创建权归认知层（认知层知道何时需要隔离），存储和归档归基座层（基座层管理生命周期）。未来如需完全隔离，创建权可迁移到基座层 `ToolCoordinator`，接口不变。

### 2. Session 切割 Phase 1 策略（LastNMessages）

与 ADR-0036 一致性对齐后确认：

**Phase 1（MVP）**：`LastNMessages` 策略，零 LLM 依赖

```
Context.token_count > limit 时：
  1. 保留最后 N 条 Message（N = 上下文窗口 / 平均消息大小 × 安全系数 0.7）
  2. TaskSession.state 保留所有未过期的 domain_state
  3. 切割完成后通过 EventBus 或回调通知 TUI
  4. 不阻塞认知层工作线程
```

**Phase 2**：Flash 模型摘要（后续 Phase，按需引入）

```
Context.token_count > limit 时：
  1. 用 Flash 模型生成摘要（预估 < 1s）
  2. 摘要作为第一条 Message 注入新 session
  3. 通知 TUI 展示摘要信息
```

**Phase 3（可选）**：Pro 摘要 + 质量验证

---

## 参考
- [ADR-0031 执行策略](./adr-0031-execution-policy.md)
- [ADR-0032 成本收集](../archive/adr/adr-0032-cost-collector.md)
- [ADR-0023 ToolResult 标准化](./adr-0023-tool-result-standard.md)
- [ADR-0030 AsyncRuntime](../archive/adr/adr-0030-async-runtime-dual-layer.md)
- [ADR-0004 ToolRegistry 安全](./adr-0004-toolregistry-security.md)
- `src/core/types/context.h` — Context 类型定义
- `src/modules/scheduler/execution_session.h` — 现有 ExecutionSession
- `src/modules/scheduler/topo_scheduler.cpp` — `execute_single_branch()` 实现
- `src/modules/budget/budget_controller.h` — BudgetController 接口