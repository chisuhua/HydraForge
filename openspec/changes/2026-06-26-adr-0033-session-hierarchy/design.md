# Design: ADR-0033 Session Hierarchy — 实施设计

> **前置**: proposal.md
> **Oracle 审查**: ses_0e28703caffeBUB7tgDqsClZiw (2026-07-01)
> **Metis 审查**: ses_0e26217edffeHrMpGUPLuhTecP (2026-07-01)
> **修订**: 2026-07-01 — D9 内存安全、D10 failure_count 语义、D11 message 语义、D12 LayeredContext 重载

## 决策 1：三层会话模型 + 稳定地址容器

**Decision**: UserSession/TaskSession 为 class（方法封装），SubtaskSession 为 struct（POD-like 值类型）。

为确保引用/指针的地址稳定性，容器采用 **`std::deque`** 而非 `std::vector`：
- `UserSession::task_sessions_`: `std::deque<TaskSession>` — deque 的 push_back 不使已有元素引用失效
- `TaskSession::subtask_sessions_`: `std::deque<SubtaskSession>` — 同上
- `UserSession::current_task_session_`: `TaskSession*` — 指向 deque 内部，地址稳定

```cpp
class UserSession {
    std::deque<TaskSession> task_sessions_;        // ✅ deque，地址稳定
    TaskSession* current_task_session_ = nullptr;  // ✅ deque 中元素地址不因 push 失效
    ...
};
class TaskSession {
    UserSession& user_session_;                     // ✅ 引用绑定 UserSession 本身
    std::deque<SubtaskSession> subtask_sessions_;   // ✅ deque，地址稳定
    ...
};
```

**Rationale**:
- 避免 `vector` 扩容导致所有引用/指针失效（Metis F1/F2）
- `deque::push_back` 不 invalidate 已有元素的引用或指针
- `deque` 随机访问 O(1)，遍历缓存局部性略差于 `vector`，但 session 数量小（<100），可忽略
- 与现有 BudgetController (值类型) / Context (值类型) 一致

---

## 决策 2：不重命名 ExecutionSession

**Decision**: 保留 `ExecutionSession` 名称，不做 `→ DagExecutionContext` 重命名。

**Rationale**:
- Sprint 19 刚完成 PIMPL-lite 重构 (commit 3a4852f)，改名引入 30+ 文件变更
- ADR-0033 文档中已使用 ExecutionSession，统一即可
- 零功能收益，纯命名变更

**影响**: 删除 ADR-0033 §6 的 rename 计划；在实现说明中标注 ExecutionSession 即 DagExecutionContext 概念

---

## 决策 3：TaskSession 持有 shared_ptr<IExecutionPolicy>

**Decision**: TaskSession 与 DSLEngine 共享 IExecutionPolicy 所有权。

```cpp
class TaskSession {
    std::shared_ptr<IExecutionPolicy> current_policy_;
    // DSLEngine 同样持有 shared_ptr<IExecutionPolicy> policy_
};
```

**Rationale**:
- DSLEngine 已用 `shared_ptr<IExecutionPolicy> policy_` 持有策略 (engine.h:148)
- unique_ptr 导致所有权分裂，TaskSession 无法安全引用
- shared_ptr 开销可忽略（策略通常单例）

**不采用 ADR-0033 原设计的 unique_ptr**

**共享策略的行为说明**: 若执行中途调用 `DSLEngine::set_execution_policy()` 切换策略，已存在的 TaskSession 通过 shared_ptr 观察到新策略。这是**预期行为**（策略作用于引擎级别），非 bug。

---

## 决策 4：不在 BudgetController 新增 cost_limit API

**Decision**: BudgetController 不新增 `set_cost_limit()`/`try_consume_cost()`。

现有接口 `record_llm_call(tokens, model)` + `get_total_cost_usd()` + `CostTracker` 已满足 ADR-0032 需求。
成本限制在 C6 (ADR-0004 V2) 阶段通过更高层（ToolCoordinator 或 DSLEngine）实施。

```cpp
// 已有（足够）：
BudgetController::record_llm_call(int tokens, const string& model)
BudgetController::get_total_cost_usd() const
CostTracker { total_cost_usd, tokens_consumed, last_call_cost_usd }

// 不新增（C6 范围）：
// BudgetController::set_cost_limit(double)     ← 否决
// BudgetController::try_consume_cost(double)   ← 否决
```

**Per-TaskSession 成本视图**: 当前 `DSLEngine::get_session_cost()` 是 engine 级别（全局累计）。若 per-task 成本需要，C6 阶段在 `ToolCoordinator` 层按 `task_session_id` 聚合，不在本 change 范围。

---

## 决策 5：SubtaskSession 在 DSLEngine 层包装（TopoScheduler 不改签名）

**Decision**: 
- **不改** `TopoScheduler::execute_single_branch()` 签名 — 保持返回 `Context`
- **不改** `TopoScheduler::execute()` 签名 — 不加 `TaskSession*` 参数
- SubtaskSession 的创建/归档全在 `DSLEngine::run_impl()` 中完成

```cpp
// TopoScheduler 保持（签名不变）：
Context TopoScheduler::execute(const Context&);
Context TopoScheduler::execute_single_branch(const NodePath&, const Context&);

// DSLEngine::run_impl 中包装：
SubtaskSession sub = task_sess.create_subtask(branch_path, ctx);
Context result = scheduler_->execute_single_branch(path, ctx);
sub.final_context = result;
sub.status = "completed";
task_sess.archive_subtask_result(std::move(sub));
```

**Rationale**:
- TopoScheduler 是纯调度层，不应耦合会话模型（单一职责）
- 避免 BranchExecutionResult 结构体引入大量级联修改
- 未来需调度层感知 Session 时可无损添加

---

## 决策 6：UserSession.messages 用 vector<ToolResult>

**Decision**: `messages` 直接用 `std::vector<ToolResult>`，不自定义容器。

```cpp
class UserSession {
    std::vector<ToolResult> messages_;  // 追加写（ADR-0023）
public:
    void append_message(ToolResult msg);  // push_back
    const std::vector<ToolResult>& messages() const;  // const 引用，只读
};
```

**Rationale**:
- 追加写保护通过 `const&` 返回 + 无非 const mutator 实现
- vector 性能足够（messages 典型 < 100 条）
- 不引入自定义容器复杂度

---

## 决策 7：DSLEngine::run(UserSession&, const string&) 接受 Context + 提供 LayeredContext 重载

**Decision**: 提供两个新重载，内部都委托到同一实现：

```cpp
// 主入口（Context 版本）：
ExecutionResult run(UserSession& user_sess, const std::string& message,
                    const Context& initial_ctx = Context{});

// 桥接重载（LayeredContext 版本，兼容 Sprint 20 API 风格）：
ExecutionResult run(UserSession& user_sess, const std::string& message,
                    const LayeredContext& initial_lctx);
```

```cpp
// 内部实现（统一在 run_impl）：
ExecutionResult DSLEngine::run(UserSession& user_sess, const std::string& message,
                                const Context& initial_ctx) {
    // 1. message 写入 ctx["user_input"]
    Context ctx = initial_ctx;
    ctx["user_input"] = message;

    // 2. 创建/复用 TaskSession
    auto* task_sess = resolve_task_session(user_sess);

    // 3. 设置或替换 context
    task_sess.set_context(std::move(ctx));

    // 4. 执行（委托到现有 TopoScheduler::execute）
    auto result = scheduler_->execute(task_sess.context());

    // 5. 追加到 UserSession.messages
    user_sess.append_message(to_tool_result(result));

    return result;
}

// LayeredContext 重载：桥接到 Context 版本
ExecutionResult run(UserSession& user_sess, const std::string& message,
                    const LayeredContext& initial_lctx) {
    return run(user_sess, message, to_context(initial_lctx));
}
```

**语义说明**:
- `message` 参数写入 `ctx["user_input"]`，作为用户输入注入 context
- 复用 TaskSession 时，新的 `initial_ctx` **替换**（而非合并）`task_sess.context_` 的顶层字段；子字段 `"user_input"` 始终覆盖

---

## 决策 8：UserSession 线程安全通过调用方保证

**Decision**: UserSession 内部不加锁。

**Rationale**:
- CognitiveWorker 当前单线程驱动 DSLEngine
- Fork/Join 的 SubtaskSession 是 TaskSession 内部隔离，不跨线程共享
- 如果未来多线程共享 UserSession，调用方负责外部同步
- 与现有 BudgetController / ExecutionSession 模式一致（单线程使用场景）

---

## 决策 9：failure_count 递增语义

**Decision**: `TaskSession::failure_count_` 仅在以下条件**全部满足**时递增：

1. `ExecutionResult.success == false`
2. `error_code` 属于**可重试类别**（Retryable / Network / Timeout / RateLimited）

```cpp
void TaskSession::record_failure(const ExecutionResult& result) {
    if (!result.success && is_retryable_error(result.error_code)) {
        ++failure_count_;
    }
}
```

非可重试错误（如 PermissionDenied、InvalidInput）不递增失败计数，也不触发 session 分裂。

---

## 决策 10：用户需预先回答的关键问题（实施前必须确认）

| 问题 | 当前采纳建议 | 说明 |
|------|-------------|------|
| `message` 参数写入 Context 哪个字段？ | `ctx["user_input"]` | 与现有 DSL 模板引用 `{{user_input}}` 一致 |
| 复用 TaskSession 时 `initial_ctx` 替换还是合并？ | **替换**顶层字段，`"user_input"` 始终覆盖 | 简单明确；合并语义在 C6/C8 阶段引入 |
| `set_execution_policy()` 影响已有 TaskSession？ | **是**，shared_ptr 共享策略实例 | 引擎级别策略，不 per-task 快照 |
| 是否需要 `LayeredContext` 重载？ | **是**，作为桥接提供，内部 `to_context()` 委托 | 与 Sprint 20 API 风格一致 |