# AgenticDSL 能力差距与增强路线图

> 基于 v3.10 代码深度分析 | 生成日期: 2026-05-19

---

## 目录

1. [现状概述](#1-现状概述)
2. [关键发现：代码层面的障碍](#2-关键发现代码层面的障碍)
3. [增强一：Fork/Join 基于 C++20 协程](#3-增强一forkjoin-基于-c20-协程)
4. [增强二：独立用户输入循环](#4-增强二独立用户输入循环)
5. [增强三：State 读写接口](#5-增强三state-读写接口)
6. [增强四：LLM 工具体系重构](#6-增强四llm-工具体系重构)
7. [增强五：工具能力矩阵](#7-增强五工具能力矩阵)
8. [增强六：标准库与子图组合](#8-增强六标准库与子图组合)
9. [能力对标：主流 Agent Harness](#9-能力对标主流-agent-harness)
10. [实施路线图](#10-实施路线图)

---

## 1. 现状概述

### 1.1 已拥有的能力

```
AgenticDSL v3.10 现状
═══════════════════════

✅ DAG 定义与执行          ✅ LLM 调用 (dsl_call)
✅ 工具调用 (tool_call)    ✅ 变量赋值 (assign)  
✅ 条件断言 (assert)       ✅ 动态图生成 (generate_subgraph)
✅ 预算控制 (budget)       ✅ 模板渲染 (Inja)
✅ 追踪记录 (trace)        ✅ 快照 (snapshot)
✅ 签名验证 (框架级)       ✅ 权限检查 (基础)
```

### 1.2 关键差距

| 维度 | 现状 | 目标 | 差距等级 |
|------|------|------|---------|
| **并行** | Fork/Join 抛 runtime_error | 真并行执行 | 🔴 致命 |
| **并发** | 单线程调度器 | 协程异步执行 | 🔴 致命 |
| **用户交互** | 无 user_input 节点 | 交互式工作流 | 🔴 致命 |
| **状态持久化** | 快照仅内存 | 磁盘序列化/恢复 | 🟡 重要 |
| **LLM 类型** | ~~2 个冲突的 LLMParams~~ | ✅ 已统一：`LLMParams` 现为 `LLMConfig` 的别名（`llm_types.h:60`） | ✅ 已解决 (C1) |
| **流式** | 无 streaming | SSE/WebSocket | 🟢 可增强 |
| **标准库** | 3/5 stub | 20+ 可组合子图 | 🟡 重要 |
| **资源管理** | 仅声明 | 连接/释放/池化 | 🟢 可增强 |

---

## 2. 关键发现：代码层面的障碍

### 2.1 调度器：单线程阻塞执行

当前 `TopoScheduler::execute()` 的实现模式：

```cpp
// 当前: 同步单线程执行
Context TopoScheduler::execute(const Context& initial_context) {
    Context current_ctx = initial_context;
    
    while (has_ready_nodes()) {
        auto* node = get_next_ready_node();
        
        // ⚠️ 阻塞调用
        Context node_result = node_executor_->execute_node(node, current_ctx);  
        
        if (has_jump(node_result)) {
            handle_jump(node_result);
        }
        
        current_ctx = std::move(node_result);
        mark_node_completed(node);
    }
    
    return current_ctx;
}
```

**问题：**
- 无异步/协程支持
- 每个节点执行阻塞当前线程
- Fork/Join 无法实现（需要并发执行）
- `execute_fork()` 和 `execute_join()` 直接抛异常

### 2.2 LLM 类型冲突（严重设计问题）

**发现：两套冲突的 LLM 参数类型**

```cpp
// src/common/llm/llm_tool.h
struct LLMParams {
    float temperature = 0.7;
    int max_tokens = 1024;
    float top_p = 0.9;
    int n_ctx = 2048;
    int n_threads = 4;
    std::string model;
};

// src/common/llm/llm_types.h  ← 冲突!
struct LLMParams {      // 同名不同结构!
    float temperature = 0.7;
    int max_tokens = 1024;
    float top_p = 0.9;
    std::string stop;   // 额外字段
    float frequency_penalty = 0.0;  // 额外字段
    float presence_penalty = 0.0;   // 额外字段
};
```

**影响：**
- 两套 `LLMParams` 在不同的 include 路径中
- `DSLNode` 引用 `llm_tool.h` 的版本
- 重构时必须统一

### 2.3 Context 设计：全局可变 JSON

```cpp
using Context = nlohmann::json; // 单树，全局共享
```

**问题：**
- 不可隔离：Fork 分支需要 context 副本
- 无类型安全：所有值都是 JSON
- 无变更日志：不知道谁修改了什么

### 2.4 快照系统：有接口无持久化

```cpp
// context_engine.h - 存在快照接口
class ContextEngine {
    void save_snapshot(const std::string& label, const Context& ctx);
    Context get_snapshot(const std::string& label);
};

// budget.h - 有快照预算
struct ExecutionBudget {
    int max_snapshots = -1;          // -1 = 无限
    int snapshot_max_size_kb = 512;
};
```

**发现：** 快照系统存在于接口层面，但：
- 无磁盘序列化
- 无恢复/回滚机制
- 仅内存存储

---

## 3. 增强一：Fork/Join 基于 C++20 协程

### 3.1 设计目标

```
┌──────────────────────────────────────────────────┐
│        协程调度的 Fork/Join 执行模型               │
├──────────────────────────────────────────────────┤
│                                                    │
│  ForkNode: 启动多个协程分支                        │
│    ├── co_spawn(branch_a) ──► 协程 A              │
│    ├── co_spawn(branch_b) ──► 协程 B              │
│    └── co_spawn(branch_c) ──► 协程 C              │
│                                                    │
│  JoinNode: 等待所有分支完成                        │
│    └── co_await all_tasks ──► 合并 Context         │
│                                                    │
└──────────────────────────────────────────────────┘
```

### 3.2 调度器协程改造

```cpp
// 新调度器接口
class CoroutineScheduler {
public:
    // 核心协程：执行一个 DAG 分支
    Awaitable<Context> co_execute(
        const ParsedGraph& graph,
        Context context,
        const ExecutionBudget& budget
    );
    
    // Fork: 启动多个独立协程
    std::vector<Task<Context>> co_fork(
        const std::vector<NodePath>& branches,
        Context shared_context
    );
    
    // Join: 等待所有协程并合并结果
    Awaitable<Context> co_join(
        std::vector<Task<Context>> tasks,
        MergeStrategy strategy
    );
};
```

### 3.3 ForkNode 协程实现

```cpp
// 协程版的 Fork 执行
Awaitable<Context> execute_fork_coro(
    const ForkNode* node, 
    Context context
) {
    std::vector<Task<Context>> branch_tasks;
    
    // 为每个分支创建独立 context 副本
    for (const auto& branch : node->branches) {
        Context branch_ctx = context;  // 深拷贝
        
        auto task = co_spawn([branch, branch_ctx]() -> Awaitable<Context> {
            return co_execute_subgraph(branch, branch_ctx);
        });
        
        branch_tasks.push_back(std::move(task));
    }
    
    // 所有分支同时运行（由协程调度器管理）
    // 当 co_join 被调用时，结果才被聚合
    co_return context;  // 暂存 task 到上下文
}
```

### 3.4 JoinNode 协程实现

```cpp
Awaitable<Context> execute_join_coro(
    const JoinNode* node,
    Context context,
    std::vector<Task<Context>> pending_tasks
) {
    std::vector<Context> branch_results;
    
    // 并发等待所有分支
    for (auto& task : pending_tasks) {
        Context result = co_await task;  // 协程挂起点
        branch_results.push_back(std::move(result));
    }
    
    // 按策略合并
    Context merged = merge_contexts(
        context, 
        branch_results, 
        node->merge_strategy
    );
    
    co_return merged;
}
```

### 3.5 合并策略

```cpp
enum class MergeStrategy {
    ERROR_ON_CONFLICT,     // 冲突抛异常
    LAST_WRITE_WINS,       // 最后写入覆盖
    FIRST_WRITE_WINS,      // 首次写入保留
    DEEP_MERGE,            // 递归合并 JSON
    CUSTOM                 // 用户定义回调
};

Context merge_contexts(
    const Context& parent,
    const std::vector<Context>& branch_results,
    MergeStrategy strategy
) {
    Context merged = parent;
    
    for (const auto& branch_ctx : branch_results) {
        switch (strategy) {
            case MergeStrategy::DEEP_MERGE:
                merged.merge_patch(branch_ctx);
                break;
            case MergeStrategy::ERROR_ON_CONFLICT:
                check_conflicts(merged, branch_ctx);
                merged.merge_patch(branch_ctx);
                break;
            // ...
        }
    }
    
    return merged;
}
```

### 3.6 DSL 语法

```yaml
## /main/parallel
type: fork
branches: ["/branch/a", "/branch/b"]
output_keys: ["branch_a_ctx", "branch_b_ctx"]
# 可选的 context 隔离策略
context_isolation: deep_copy  # deep_copy | reference | lazy_copy
next: ["/main/join"]

## /main/join
type: join
wait_for: ["/branch/a", "/branch/b"]
merge_strategy: error_on_conflict
# 可选的冲突处理
on_conflict: "/main/conflict_handler"
next: ["/main/continue"]
```

### 3.7 C++20 协程基础设施

```cpp
// 协程任务类型
template<typename T>
struct Task {
    struct promise_type {
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        void return_value(T value) {
            result_ = std::move(value);
        }
        
        void unhandled_exception() {
            exception_ = std::current_exception();
        }
        
        T result_;
        std::exception_ptr exception_;
    };
    
    bool await_ready() { return handle_.done(); }
    T await_resume() { /* 获取结果或抛异常 */ }
    void await_suspend(std::coroutine_handle<>) { /* 调度到线程池 */ }
    
    std::coroutine_handle<promise_type> handle_;
};

// 协程感知的调度器
class CoroutineExecutor {
    std::vector<std::jthread> thread_pool_;
    std::mutex ready_queue_mutex_;
    std::queue<Awaitable<Context>> ready_queue_;
    
public:
    template<typename F>
    Task<Context> co_spawn(F&& coro_fn) {
        // 将协程提交到线程池
        co_return co_await coro_fn();
    }
    
    Awaitable<void> run_loop() {
        // 事件循环：处理就绪节点
        while (has_pending_work()) {
            auto work = dequeue_ready();
            co_await work;
        }
    }
};
```

---

## 4. 增强二：独立用户输入循环

### 4.1 设计目标

创建一个 `user_input` 节点，让 DSL 可以在执行过程中暂停并等待用户输入，形成交互式循环：

```
┌─────────────────────────────────────────────────────────┐
│                Agent 主循环                               │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  while (true) {                                          │
│    ┌──────────┐    ┌──────────┐    ┌──────────┐         │
│    │ LLM 思考  │ → │ user_input│ → │ 执行动作  │         │
│    └──────────┘    └──────────┘    └──────────┘         │
│         ↑                                    │          │
│         └────────── 循环（直到退出）──────────┘          │
│  }                                                       │
│                                                          │
└─────────────────────────────────────────────────────────┘
```

### 4.2 Node 定义

```cpp
struct UserInputNode : public Node {
    std::string prompt_template;     // Inja 模板
    std::string input_variable;      // 存储用户输入的变量名
    std::optional<std::string> validation_pattern;  // 正则验证
    std::string input_type = "text"; // text | confirm | choice | multiline
    std::vector<std::string> options; // 选项列表 (choice 类型)
    int timeout_seconds = 0;         // 0 = 无限等待
    std::optional<NodePath> on_timeout;  // 超时跳转
    
    UserInputNode(/* ... */);
    Context execute(Context& context) override;
};
```

### 4.3 执行器实现

```cpp
Context NodeExecutor::execute_user_input(
    const UserInputNode* node, 
    const Context& ctx
) {
    Context new_ctx = ctx;
    
    // 1. 渲染 prompt
    std::string prompt = InjaTemplateRenderer::render(
        node->prompt_template, ctx
    );
    
    // 2. 通过回调获取用户输入
    // 使用回调而非阻塞，支持异步
    UserInputRequest request{
        .prompt = prompt,
        .type = node->input_type,
        .options = node->options,
        .timeout = node->timeout_seconds,
        .validation = node->validation_pattern
    };
    
    // 调度器暂停执行，直到用户回复
    auto user_response = co_await wait_for_user_input(request);
    
    // 3. 验证输入
    if (!validate_input(user_response, request)) {
        if (node->on_validation_failure) {
            throw JumpException(node->on_validation_failure.value());
        }
        throw std::runtime_error("Invalid user input");
    }
    
    // 4. 存储到 context
    new_ctx[node->input_variable] = user_response.value;
    
    return new_ctx;
}
```

### 4.4 Engine 层面的用户输入支持

```cpp
class DSLEngine {
public:
    // 异步执行：返回后可在需要用户输入时暂停
    Awaitable<ExecutionResult> run_async(const Context& initial_ctx);
    
    // 提供用户输入（由外部调用者触发）
    void provide_user_input(const std::string& input);
    
    // 检查是否需要用户输入
    bool is_waiting_for_input() const;
    
    // 获取当前待处理的输入请求
    std::optional<UserInputRequest> current_input_request() const;
    
    // 注册输入回调
    using InputCallback = std::function<Awaitable<std::string>(const UserInputRequest&)>;
    void set_input_callback(InputCallback cb);
    
private:
    std::optional<UserInputRequest> pending_input_;
    std::promise<std::string> input_promise_;
};
```

### 4.5 DSL 语法

```yaml
## /main/ask_user
type: user_input
prompt: |
  分析结果: {{analysis}}
  请确认是否继续？
input_variable: user_decision
input_type: confirm
next: ["/main/process"]

## /main/ask_choice
type: user_input
prompt: |
  请选择下一步操作:
input_variable: user_choice
input_type: choice
options: ["搜索", "分析", "退出"]
next: ["/main/handle_choice"]

## /main/free_text
type: user_input
prompt: |
  请输入您的问题:
input_variable: user_query
input_type: multiline
timeout_seconds: 120
on_timeout: "/main/timeout_handler"
next: ["/main/process_query"]
```

### 4.6 交互式循环 DSL 示例

```markdown
### AgenticDSL '/interactive_agent'

## /ia/meta
execution_budget:
  max_llm_calls: 100
  max_tool_calls: 200
  max_user_inputs: 50

## /ia/start
type: start
next: ["/ia/think"]

## /ia/think
type: dsl_call
llm_tool: llama-7b
output_keys: ["thought"]
prompt: |
  历史对话: {{conversation_history}}
  用户最新输入: {{last_user_input}}
  
  思考下一步行动。你需要:
  1. 分析用户需求
  2. 决定采取什么行动
  3. 输出回复和行动计划
next: ["/ia/act"]

## /ia/act
type: dsl_call
llm_tool: llama-7b
output_keys: ["action"]
prompt: |
  基于思考: {{thought}}
  
  决定是回复用户还是调用工具。
  
  如果需要回复: 输出 "action: respond"
  如果需要工具: 输出 "action: tool_call, tool: <name>, args: ..."
next: ["/ia/route"]

## /ia/route
type: generate_subgraph
prompt: |
  分析动作: {{action}}
  
  如果是 "respond":
  生成:
  ### AgenticDSL '/dynamic/respond'
  ## /dynamic/respond/reply
  type: user_input
  prompt: {{thought.reply}}
  input_variable: next_user_input
  input_type: text
  
  如果是 "tool_call":
  生成对应的工具调用节点。
output_keys: ["dynamic_path"]
signature_validation: ignore
next: ["/ia/execute"]

## /ia/execute
type: dsl_call
llm_tool: llama-7b
output_keys: ["execution_result"]
prompt: |
  执行结果: {{dynamic_path}}
  更新对话历史。
next: ["/ia/think"]  # ← 循环回到开始
```

---

## 5. 增强三：State 读写接口

### 5.1 设计目标

提供持久化状态管理，使 AgenticDSL 可以跨会话保存和恢复状态：

```
┌────────────────────────────────────────────────────────┐
│                   State System                           │
├────────────────────────────────────────────────────────┤
│                                                         │
│  StateStore ──────► Serializer ──────► Disk/DB          │
│     │                                                   │
│     ├── get(key) → Value                                │
│     ├── set(key, value)                                 │
│     ├── delete(key)                                     │
│     ├── snapshot(label) → checkpoint                    │
│     └── restore(checkpoint_id)                          │
│                                                         │
│  CheckpointManager                                      │
│     ├── save(graph_state, context, budget)              │
│     ├── list() → checkpoint[]                           │
│     └── load(id) → (graph_state, context, budget)       │
│                                                         │
└────────────────────────────────────────────────────────┘
```

### 5.2 StateStore 接口

```cpp
// 状态存储接口
class IStateStore {
public:
    virtual ~IStateStore() = default;
    
    // 读写
    virtual Value get(const std::string& key) = 0;
    virtual void set(const std::string& key, const Value& value) = 0;
    virtual bool exists(const std::string& key) = 0;
    virtual void remove(const std::string& key) = 0;
    
    // 批量操作
    virtual std::map<std::string, Value> get_all(const std::string& prefix) = 0;
    virtual void set_all(const std::map<std::string, Value>& entries) = 0;
    
    // 事务
    virtual void begin_transaction() = 0;
    virtual void commit() = 0;
    virtual void rollback() = 0;
    
    // 命名空间
    virtual void set_namespace(const std::string& ns) = 0;
};

// 内存实现
class InMemoryStateStore : public IStateStore {
    std::map<std::string, Value> store_;
    std::optional<std::map<std::string, Value>> transaction_buffer_;
    std::string current_namespace_;
    
public:
    Value get(const std::string& key) override {
        auto full_key = namespaced(key);
        auto it = store_.find(full_key);
        if (it == store_.end()) return nullptr;
        return it->second;
    }
    
    void set(const std::string& key, const Value& value) override {
        auto full_key = namespaced(key);
        if (transaction_buffer_) {
            (*transaction_buffer_)[full_key] = value;
        } else {
            store_[full_key] = value;
        }
    }
    
    // 持久化导出
    nlohmann::json export_snapshot() {
        return store_;
    }
    
    void import_snapshot(const nlohmann::json& data) {
        store_ = data.get<std::map<std::string, Value>>();
    }
};

// 文件持久化实现
class FileStateStore : public InMemoryStateStore {
    std::string state_file_;
    
public:
    FileStateStore(const std::string& path) : state_file_(path) {
        load_from_disk();
    }
    
    ~FileStateStore() {
        flush_to_disk();
    }
    
    void flush_to_disk() {
        std::ofstream file(state_file_);
        file << export_snapshot().dump(2);
    }
    
    void load_from_disk() {
        std::ifstream file(state_file_);
        if (file.good()) {
            nlohmann::json data;
            file >> data;
            import_snapshot(data);
        }
    }
    
    // 每次写入后自动持久化
    void set(const std::string& key, const Value& value) override {
        InMemoryStateStore::set(key, value);
        flush_to_disk();  // 或异步写入
    }
};
```

### 5.3 Checkpoint 系统

```cpp
struct Checkpoint {
    std::string id;
    std::string label;
    std::chrono::system_clock::time_point timestamp;
    nlohmann::json state_snapshot;    // 全量状态
    nlohmann::json context_snapshot;  // 执行上下文
    ExecutionBudget budget;           // 预算状态
    std::string graph_state;          // 图的执行位置
    size_t node_index;                // 当前节点索引
};

class CheckpointManager {
    IStateStore& store_;
    std::string checkpoint_dir_;
    
public:
    // 创建检查点
    Checkpoint create_checkpoint(
        const std::string& label,
        const Context& ctx,
        const ExecutionBudget& budget,
        const std::string& current_graph_state
    ) {
        Checkpoint cp;
        cp.id = generate_uuid();
        cp.label = label;
        cp.timestamp = std::chrono::system_clock::now();
        cp.context_snapshot = ctx;
        cp.budget = budget;
        cp.graph_state = current_graph_state;
        
        // 持久化到磁盘
        save_checkpoint_to_disk(cp);
        return cp;
    }
    
    // 恢复检查点
    std::tuple<Context, ExecutionBudget, std::string> restore(
        const std::string& checkpoint_id
    ) {
        auto cp = load_checkpoint_from_disk(checkpoint_id);
        return {cp.context_snapshot, cp.budget, cp.graph_state};
    }
    
    // 列出检查点
    std::vector<CheckpointInfo> list_checkpoints() {
        // 扫描 checkpoint 目录
    }
    
    // 自动检查点（定期）
    void enable_auto_checkpoint(int interval_seconds);
};
```

### 5.4 Node 类型：StateNode

```cpp
struct StateNode : public Node {
    enum class Operation {
        READ,
        WRITE,
        DELETE,
        LIST,
        TRANSACTION_BEGIN,
        TRANSACTION_COMMIT,
        TRANSACTION_ROLLBACK,
        CHECKPOINT,
        RESTORE
    };
    
    Operation operation;
    std::string state_key;        // 模板化 key
    std::string value_template;   // 仅 WRITE 时使用
    std::string output_key;       // 读取结果的输出变量
    std::optional<std::string> checkpoint_label;  // 仅 CHECKPOINT
    
    StateNode(/* ... */);
    Context execute(Context& context) override;
};
```

### 5.5 DSL 语法

```yaml
## /state/save
type: state
operation: write
state_key: "conversation.{{session_id}}.history"
value_template: "{{conversation_history}}"
next: ["/state/continue"]

## /state/load
type: state
operation: read
state_key: "conversation.{{session_id}}.history"
output_key: "restored_history"
next: ["/state/continue"]

## /state/checkpoint
type: state
operation: checkpoint
checkpoint_label: "before_critical_operation"
output_key: "checkpoint_id"
next: ["/state/critical_op"]

## /state/restore
type: state
operation: restore
state_key: "{{checkpoint_id}}"
# 恢复后跳转到指定节点
next: ["/state/retry"]
```

### 5.6 持久化 DSL 示例

```markdown
### AgenticDSL '/persistent_agent'

## /pa/state_init
type: state
operation: read
state_key: "agent.{{agent_id}}.session"
output_key: "saved_state"
next: ["/pa/check_state"]

## /pa/check_state
type: assert
condition: "{{saved_state}}"
on_failure: "/pa/new_session"
next: ["/pa/restore_session"]

## /pa/restore_session
type: assign
assign:
  conversation_history: "{{saved_state.history}}"
  task_queue: "{{saved_state.tasks}}"
  session_continue: "{{saved_state.progress}}"
next: ["/pa/main_loop"]

## /pa/new_session
type: assign
assign:
  conversation_history: "[]"
  task_queue: "{{initial_tasks}}"
  session_id: "{{generate_uuid}}"
next: ["/pa/main_loop"]

## /pa/auto_save
type: state
operation: write
state_key: "agent.{{agent_id}}.session"
value_template: |
  {
    "history": {{conversation_history}},
    "tasks": {{task_queue}},
    "progress": {{current_progress}},
    "last_saved": "{{timestamp}}"
  }
next: ["/pa/continue"]
```

---

## 6. 增强四：LLM 工具体系重构

### 6.1 关键问题：类型冲突

**当前代码库中发现的问题：**

```
src/common/llm/llm_tool.h     → struct LLMParams { temperature, max_tokens, top_p, n_ctx, n_threads, model }
src/common/llm/llm_types.h    → struct LLMParams { temperature, max_tokens, top_p, stop, frequency_penalty, presence_penalty }
src/common/llm/llm_adapter.h  → struct LLMConfig { ... }, struct LLMResult { ... }
```

**三套类型系统需要统一为一个。**

### 6.2 统一 LLM 接口

```cpp
// ========== 统一 LLM 类型 ==========

// 统一的 LLM 参数
struct LLMConfig {
    std::string model;                  // 模型名称
    float temperature = 0.7;            // 采样温度
    int max_tokens = 1024;              // 最大生成长度
    float top_p = 0.9;                  // 核采样
    int top_k = 40;                     // top-k 采样
    float frequency_penalty = 0.0;      // 频率惩罚
    float presence_penalty = 0.0;       // 存在惩罚
    std::vector<std::string> stop;      // 停止词
    int n_ctx = 2048;                   // 上下文窗口
    int n_threads = 4;                  // 推理线程数
    int seed = -1;                      // 随机种子 (-1 = 随机)
};

// 统一 LLM 结果
struct LLMResult {
    bool success = false;
    std::string text;
    std::string error;
    int tokens_generated = 0;
    int tokens_prompt = 0;
    float generation_time_ms = 0;
    std::optional<std::string> finish_reason;
};

// 统一 LLM Provider 接口
class ILLMProvider {
public:
    virtual ~ILLMProvider() = default;
    
    // 同步生成
    virtual LLMResult generate(
        const std::string& prompt,
        const LLMConfig& config
    ) = 0;
    
    // 流式生成
    virtual std::unique_ptr<IGenerationStream> generate_stream(
        const std::string& prompt,
        const LLMConfig& config
    ) = 0;
    
    // 聊天补全
    virtual LLMResult chat(
        const std::vector<ChatMessage>& messages,
        const LLMConfig& config
    ) = 0;
    
    // 模型信息
    virtual std::string model_name() const = 0;
    virtual bool is_available() const = 0;
};

// 流式接口
class IGenerationStream {
public:
    virtual ~IGenerationStream() = default;
    
    // 读取下一个 token（阻塞）
    virtual std::optional<std::string> next_token() = 0;
    
    // 异步读取
    virtual Awaitable<std::optional<std::string>> co_next_token() = 0;
    
    // 检查是否完成
    virtual bool is_complete() const = 0;
    
    // 取消生成
    virtual void cancel() = 0;
};
```

### 6.3 提供商注册

```cpp
class LLMProviderRegistry {
    std::unordered_map<std::string, std::unique_ptr<ILLMProvider>> providers_;
    
public:
    // 注册提供商
    void register_provider(
        const std::string& name,
        std::unique_ptr<ILLMProvider> provider
    );
    
    // 获取提供商
    ILLMProvider* get_provider(const std::string& name);
    
    // 列出可用
    std::vector<std::string> available_providers() const;
    
    // 默认提供商
    void set_default(const std::string& name);
    ILLMProvider* default_provider();
};

// 提供商实现
class LlamaCppProvider : public ILLMProvider { /* 封装 llama.cpp */ };
class OpenAIProvider : public ILLMProvider { /* OpenAI API */ };
class AnthropicProvider : public ILLMProvider { /* Claude API */ };
class MockProvider : public ILLMProvider { /* 测试用 */ };
```

### 6.4 DSL 语法

```yaml
## /main/think
type: dsl_call
llm_tool: llama-7b                     # 选择提供商
stream: true                            # 流式输出
output_keys: ["response"]
prompt: |
  请分析: {{data}}
params:
  temperature: 0.7
  max_tokens: 2000
  stop: ["\n\n", "用户:"]
  frequency_penalty: 0.3
next: ["/main/process"]

## /main/chat
type: dsl_call
llm_tool: gpt-4
mode: chat                              # 聊天模式
system_prompt: "你是一个专业助手"
messages: "{{conversation_history}}"
output_keys: ["reply"]
next: ["/main/continue"]
```

### 6.5 流式执行器

```cpp
Context NodeExecutor::execute_dsl_node_stream(
    const DSLNode* node,
    const Context& ctx
) {
    Context new_ctx = ctx;
    
    auto* provider = llm_registry_->get_provider(node->llm_tool_name);
    
    // 创建流式生成
    auto stream = provider->generate_stream(
        rendered_prompt,
        node->llm_params
    );
    
    // 逐个 token 处理
    std::string full_response;
    while (auto token = co_await stream->co_next_token()) {
        full_response += *token;
        
        // 通过回调发送中间结果
        if (on_token_callback_) {
            co_await on_token_callback_(*token);
        }
    }
    
    new_ctx[node->output_keys[0]] = full_response;
    return new_ctx;
}
```

---

## 7. 增强五：工具能力矩阵

### 7.1 当前工具系统

```cpp
// 当前 ToolRegistry 实现
class ToolRegistry {
    std::unordered_map<std::string, std::unique_ptr<ITool>> tools_;
    
public:
    void register_tool(const std::string& name, std::unique_ptr<ITool> tool);
    nlohmann::json call_tool(const std::string& name, const ArgsType& args);
    bool has_tool(const std::string& name) const;
};
```

**问题：**
- 无工具版本管理
- 无工具链
- 无异步工具
- 无工具发现

### 7.2 扩展工具接口

```cpp
// 增强的工具接口
class ITool {
public:
    struct ToolInfo {
        std::string name;
        std::string description;
        std::vector<ParameterInfo> parameters;  // JSON Schema
        std::vector<std::string> categories;     // 分类标签
        std::string version;
        std::vector<std::string> required_permissions;
        int timeout_seconds = 30;
        bool is_async = false;
        bool is_streaming = false;
    };
    
    virtual ~ITool() = default;
    virtual ToolInfo info() const = 0;
    virtual nlohmann::json execute(const nlohmann::json& args) = 0;
    
    // 异步工具
    virtual Awaitable<nlohmann::json> execute_async(const nlohmann::json& args) {
        throw std::runtime_error("Async not supported");
    }
    
    // 流式工具
    virtual std::unique_ptr<IStreamIterator> execute_stream(const nlohmann::json& args) {
        throw std::runtime_error("Streaming not supported");
    }
};

// 参数信息
struct ParameterInfo {
    std::string name;
    std::string description;
    std::string type;        // "string", "number", "boolean", "array", "object"
    bool required;
    std::optional<nlohmann::json> default_value;
    std::optional<std::vector<std::string>> enum_values;
    std::optional<std::string> pattern;  // 正则验证
};
```

### 7.3 AgenticDSL 需要预注册的工具

| 工具类别 | 工具名 | 用途 | 优先级 |
|---------|--------|------|--------|
| **文件系统** | `read_file` | 读取文件 | 🔴 高 |
| | `write_file` | 写入文件 | 🔴 高 |
| | `list_directory` | 列出目录 | 🔴 高 |
| | `glob_files` | 文件搜索 | 🔴 高 |
| **命令执行** | `bash` | 执行 shell 命令 | 🔴 高 |
| | `python` | 执行 Python 脚本 | 🟡 中 |
| | `make` | 构建项目 | 🟡 中 |
| **Git** | `git_status` | 查看仓库状态 | 🟡 中 |
| | `git_diff` | 查看变更 | 🟡 中 |
| | `git_commit` | 创建提交 | 🟡 中 |
| **Web** | `http_get` | HTTP GET 请求 | 🔴 高 |
| | `http_post` | HTTP POST 请求 | 🔴 高 |
| | `web_search` | 网络搜索 | 🟡 中 |
| | `web_fetch` | 抓取网页 | 🟡 中 |
| **开发** | `lsp_diagnostics` | 代码诊断 | 🔴 高 |
| | `ast_grep` | AST 搜索 | 🟡 中 |
| | `grep` | 文本搜索 | 🟡 中 |
| **状态** | `state_read` | 读取状态 | 🔴 高 |
| | `state_write` | 写入状态 | 🔴 高 |
| | `state_list` | 列出状态 | 🔴 高 |
| **用户** | `user_input` | 获取用户输入 | 🔴 高 |
| | `user_confirm` | 用户确认 | 🔴 高 |
| | `user_choice` | 用户选择 | 🔴 高 |
| **AI** | `llm_generate` | 调用 LLM | 🔴 高 |
| | `llm_chat` | LLM 对话 | 🔴 高 |
| | `embedding` | 文本嵌入 | 🟢 低 |
| **分析** | `tokenize` | 文本分词 | 🟢 低 |
| | `count_tokens` | 计数 token | 🟢 低 |
| | `template_render` | 渲染模板 | 🟢 低 |

### 7.4 工具注册示例

```yaml
## /tools/register_web
type: tool_call
tool_name: _register_tool
arguments:
  name: "web_search"
  description: "搜索网络信息"
  parameters: |
    {
      "query": {"type": "string", "description": "搜索关键词", "required": true},
      "limit": {"type": "number", "description": "返回结果数", "default": 5}
    }
  implementation: "python:tools/web_search.py"
output_keys: ["register_result"]
```

### 7.5 工具调用示例

```yaml
## /main/search_code
type: tool_call
tool_name: ast_grep
arguments:
  pattern: "{{search_pattern}}"
  paths: ["{{search_path}}"]
output_keys: ["matches"]

## /main/edit_file
type: tool_call
tool_name: edit_file
arguments:
  path: "{{target_file}}"
  old_string: "{{old_code}}"
  new_string: "{{new_code}}"
output_keys: ["edit_result"]

## /main/run_tests
type: tool_call
tool_name: bash
arguments:
  command: "make test && ctest --output-on-failure"
  timeout: "120"
output_keys: ["test_output"]
```

---

## 8. 增强六：标准库与子图组合

### 8.1 当前标准库

```
lib/
├── auth/verify_session.md         (0 bytes - 空)
├── human/clarify_input.md         (0 bytes - 空)
├── human/confirm_action.md        (0 bytes - 空)
├── inference/engine.md            (实现: 推理引擎子图)
├── inference/model.md             (实现: 模型子图)
├── inference/session.md           (实现: 会话子图)
├── math/add.md                    (实现: 两数相加)
└── utils/noop.md                  (实现: 空操作)
```

### 8.2 目标标准库

```
lib/
├── auth/
│   ├── verify_session.md       ✅
│   ├── login.md                ← 新增
│   └── check_permission.md     ← 新增
├── human/
│   ├── confirm_action.md       ✅
│   ├── clarify_input.md        ✅
│   └── multi_step_form.md      ← 新增
├── math/
│   ├── add.md                  ✅
│   ├── subtract.md             ← 新增
│   ├── multiply.md             ← 新增
│   ├── divide.md               ← 新增
│   └── evaluate.md             ← 新增
├── utils/
│   ├── noop.md                 ✅
│   ├── template.md             ← 新增
│   ├── delay.md                ← 新增
│   └── retry.md                ← 新增
├── flow/
│   ├── fork_join.md            ← 新增
│   ├── loop.md                 ← 新增
│   └── parallel_map.md         ← 新增
└── tools/
    ├── web_search.md           ← 新增
    ├── file_ops.md             ← 新增
    └── git_ops.md              ← 新增
```

### 8.3 标准库子图示例

```markdown
## lib/flow/fork_join.md

### AgenticDSL '/lib/flow/fork_join'
signature: "(branches: string[], context: object) -> {results: object[]}"

## /lib/flow/fork_join/start
type: start
next: ["/lib/flow/fork_join/fork"]

## /lib/flow/fork_join/fork
type: fork
branches: "{{branches}}"
context_isolation: deep_copy
next: ["/lib/flow/fork_join/join"]

## /lib/flow/fork_join/join
type: join
wait_for: "{{branches}}"
merge_strategy: deep_merge
output_keys: ["results"]
next: ["/lib/flow/fork_join/end"]

## /lib/flow/fork_join/end
type: end
```

```markdown
## lib/utils/retry.md

### AgenticDSL '/lib/utils/retry'
signature: "(action: graph, max_retries: number) -> {success: bool, result: any}"

## /lib/utils/retry/start
type: start
next: ["/lib/utils/retry/init"]

## /lib/utils/retry/init
type: assign
assign:
  attempts: "0"
  max_attempts: "{{max_retries|default:3|add:1}}"
next: ["/lib/utils/retry/attempt"]

## /lib/utils/retry/attempt
type: generate_subgraph
prompt: |
  {{action}}
  # 注入重试上下文
  attempt_number: {{attempts}}
output_keys: ["action_result"]
signature_validation: ignore
next: ["/lib/utils/retry/check"]

## /lib/utils/retry/check
type: assert
condition: "{{action_result.success}}"
on_failure: "/lib/utils/retry/handle_failure"
next: ["/lib/utils/retry/success"]

## /lib/utils/retry/handle_failure
type: assign
assign:
  attempts: "{{attempts|add:1}}"
next: ["/lib/utils/retry/check_max"]

## /lib/utils/retry/check_max
type: assert
condition: "{{attempts}} < {{max_attempts}}"
on_failure: "/lib/utils/retry/give_up"
next: ["/lib/utils/retry/wait"]

## /lib/utils/retry/wait
type: tool_call
tool_name: sleep
arguments:
  seconds: "{{attempts|mul:2}}"  # 指数退避
output_keys: ["waited"]
next: ["/lib/utils/retry/attempt"]

## /lib/utils/retry/success
type: assign
assign:
  success: "true"
  result: "{{action_result}}"
next: ["/lib/utils/retry/end"]

## /lib/utils/retry/give_up
type: assign
assign:
  success: "false"
  result: "{{action_result}}"
next: ["/lib/utils/retry/end"]

## /lib/utils/retry/end
type: end
```

### 8.4 子图组合系统

```cpp
class SubgraphComposer {
public:
    // 加载库子图
    ParsedGraph load_library(const std::string& lib_path);
    
    // 组合多个子图
    ParsedGraph compose(
        const std::vector<ParsedGraph>& subgraphs,
        const std::vector<Edge>& connections
    );
    
    // 参数注入
    ParsedGraph instantiate(
        const ParsedGraph& template_graph,
        const std::map<std::string, Value>& params
    );
    
    // 子图替换
    ParsedGraph substitute(
        ParsedGraph& parent,
        const NodePath& placeholder_path,
        const ParsedGraph& replacement
    );
};
```

---

## 9. 能力对标：主流 Agent Harness

### 9.1 能力矩阵

| 能力 | Claude Code | LangChain Agents | CrewAI | AutoGPT | **AgenticDSL+增强** |
|------|------------|-----------------|--------|---------|-------------------|
| DAG 工作流 | ⚠️ 手动 | ✅ LangGraph | ✅ 图 | ❌ | ✅ **原生** |
| LLM 调用 | ✅ 原生 | ✅ | ✅ | ✅ | ✅ `dsl_call` |
| 工具调用 | ✅ MCP | ✅ Tool | ✅ Tool | ✅ 插件 | ✅ `tool_call` |
| 并行执行 | ✅ subagent | ✅ 线程 | ✅ 代理 | ❌ | ✅ **Fork/Join 协程** |
| 用户交互 | ✅ 内建 | ❌ | ❌ | ✅ 等待 | ✅ **user_input** |
| 状态持久化 | ✅ 文件 | ✅ 内存 | ❌ | ✅ JSON | ✅ **StateStore** |
| 流式输出 | ✅ | ✅ | ⚠️ | ✅ | ✅ **LLM 流式** |
| 子图组合 | ⚠️ 手动 | ✅ Runnable | ✅ Pipeline | ❌ | ✅ **标准库** |
| 循环/递归 | ✅ 函数 | ✅ 循环 | ✅ While | ✅ 循环 | ✅ **generate_subgraph** |
| 预算控制 | ❌ | ❌ | ❌ | ⚠️ 有限 | ✅ **ExecutionBudget** |
| 动态图 | ✅ 代码 | ❌ | ❌ | ❌ | ✅ **generate_subgraph** |
| 快照/回滚 | ⚠️ git | ❌ | ❌ | ❌ | ⚠️ **部分实现** |
| 工具发现 | ✅ MCP | ❌ | ❌ | ✅ 插件 | ⚠️ **需扩展** |
| 多模型 | ✅ | ✅ | ✅ | ❌ | ⚠️ **类型冲突中** |

### 9.2 增强优先级

```
🔴 P0 - 核心缺失 (必须)
──────────────────────
1. Fork/Join C++20 协程实现     → 并行执行
2. user_input 节点               → 交互式工作流
3. StateStore + Checkpoint       → 状态持久化
4. LLM 类型统一                  → 多模型支持
5. 工具扩展 (文件/命令/Git)      → 实用能力

🟡 P1 - 重要增强 (应该)
──────────────────────
6. 流式 LLM 输出                 → 实时响应
7. 标准库填充 (20+ 子图)         → 可组合复用
8. Context 分支隔离              → 并行安全
9. 工具发现/自描述               → 动态注册
10. 子图组合系统                 → 模块化

🟢 P2 - 锦上添花 (可以)
──────────────────────
11. WebSocket 实时输出            → 浏览器集成
12. 嵌入式 DSL 编辑器             → 可视化编辑
13. 性能分析/追踪面板            → 调试工具
14. 插件系统                     → 第三方扩展
15. MCP 协议支持                 → 生态兼容
```

---

## 10. 实施路线图

### 阶段一：基础设施（1-2 周）

```
┌──────────────────────────────────────────────┐
│ Phase 1: 基础设施                             │
├──────────────────────────────────────────────┤
│                                              │
│ Day 1-2: LLM 类型统一                         │
│   ├── 合并 llm_tool.h / llm_types.h          │
│   ├── 统一的 ILLMProvider 接口                │
│   └── Provider 注册表                         │
│                                              │
│ Day 3-5: C++20 协程基础设施                   │
│   ├── Task<T> / Awaitable<T> 原语            │
│   ├── 协程调度器 (线程池)                     │
│   └── 协程感知的 Context 传递                 │
│                                              │
│ Day 6-7: Fork/Join 节点实现                   │
│   ├── ForkNode 协程执行                      │
│   ├── JoinNode 协程等待                      │
│   ├── Context 分支隔离/合并                   │
│   └── 合并策略实现                            │
│                                              │
└──────────────────────────────────────────────┘
```

### 阶段二：核心能力（2-3 周）

```
┌──────────────────────────────────────────────┐
│ Phase 2: 核心能力                             │
├──────────────────────────────────────────────┤
│                                              │
│ Week 1: 用户输入与状态                        │
│   ├── user_input 节点 + 回调系统             │
│   ├── StateStore 接口 + 内存实现             │
│   ├── FileStateStore 持久化                  │
│   └── CheckpointManager                      │
│                                              │
│ Week 2: 工具扩展                             │
│   ├── ITool 增强接口 (异步/流式)             │
│   ├── 文件系统工具集                         │
│   ├── 命令执行工具                           │
│   ├── Git 工具集                             │
│   └── Web 工具集                             │
│                                              │
│ Week 3: LLM 流式                             │
│   ├── IGenerationStream 接口                 │
│   ├── LlamaCpp 流式实现                      │
│   ├── OpenAI 流式实现                        │
│   └── DSL 流式支持                           │
│                                              │
└──────────────────────────────────────────────┘
```

### 阶段三：生态构建（3-4 周）

```
┌──────────────────────────────────────────────┐
│ Phase 3: 生态构建                             │
├──────────────────────────────────────────────┤
│                                              │
│ Week 1-2: 标准库                             │
│   ├── auth/ (3 subgraphs)                    │
│   ├── human/ (3 subgraphs)                   │
│   ├── math/ (5 subgraphs)                    │
│   ├── utils/ (4 subgraphs)                   │
│   ├── flow/ (3 subgraphs)                    │
│   └── 子图组合系统                           │
│                                              │
│ Week 3-4: 高级特性                           │
│   ├── MCP 协议适配器                         │
│   ├── 工具发现/注册 API                      │
│   ├── 性能追踪面板                           │
│   └── 可视化 DSL 编辑器 (可选)               │
│                                              │
└──────────────────────────────────────────────┘
```

### 10.1 实现后的 DSL 示例

```markdown
### AgenticDSL '/ultimate_agent'

## /ua/state_restore
type: state
operation: read
state_key: "agent.{{agent_id}}.session"
output_key: "session"
next: ["/ua/check_session"]

## /ua/check_session
type: assert
condition: "{{session}}"
on_failure: "/ua/init_session"
next: ["/ua/main_parallel"]

## /ua/init_session
type: assign
assign:
  session_id: "{{generate_uuid}}"
  history: "[]"
  task_queue: ["{{initial_task}}"]
next: ["/ua/main_parallel"]

## /ua/main_parallel
type: fork
branches: ["/ua/thinking_loop", "/ua/user_monitor"]
context_isolation: deep_copy
next: ["/ua/join_main"]

## /ua/thinking_loop
type: dsl_call
llm_tool: gpt-4
stream: true
params:
  temperature: 0.7
output_keys: ["thought"]
prompt: |
  任务: {{task_queue}}
  历史: {{history}}
  思考下一步。
next: ["/ua/act_on_thought"]

## /ua/act_on_thought
type: tool_call
tool_name: route_action
arguments:
  thought: "{{thought}}"
output_keys: ["action"]
next: ["/ua/execute_action"]

## /ua/execute_action
type: generate_subgraph
prompt: "{{action.dsl}}"
output_keys: ["action_result"]
signature_validation: warn
next: ["/ua/log_and_save"]

## /ua/log_and_save
type: state
operation: write
state_key: "agent.{{agent_id}}.session"
value_template: |
  {
    "history": {{history|append:action_result}},
    "tasks": {{task_queue}},
    "progress": "running"
  }
next: ["/ua/thinking_loop"]

## /ua/user_monitor
type: user_input
prompt: "输入指令 (或输入 /help):"
input_variable: user_command
input_type: text
timeout_seconds: 300
on_timeout: "/ua/no_input"
next: ["/ua/process_user"]

## /ua/no_input
type: assign
assign:
  user_command: "/continue"
next: ["/ua/process_user"]

## /ua/process_user
type: tool_call
tool_name: parse_command
arguments:
  command: "{{user_command}}"
output_keys: ["parsed_command"]
next: ["/ua/inject_command"]

## /ua/inject_command
type: assign
assign:
  task_queue: "{{task_queue|prepend:parsed_command}}"
next: ["/ua/user_monitor"]

## /ua/join_main
type: join
wait_for: ["/ua/thinking_loop", "/ua/user_monitor"]
merge_strategy: deep_merge
on_conflict: "/ua/resolve_conflict"
next: ["/ua/checkpoint"]

## /ua/checkpoint
type: state
operation: checkpoint
checkpoint_label: "main_loop_{{session_id}}"
output_key: "cp_id"
next: ["/ua/continue_or_exit"]

## /ua/continue_or_exit
type: user_input
prompt: "继续运行? (y/n):"
input_variable: should_continue
input_type: confirm
next: ["/ua/route_continue"]

## /ua/route_continue
type: assert
condition: "{{should_continue}}"
on_failure: "/ua/save_and_exit"
next: ["/ua/main_parallel"]

## /ua/save_and_exit
type: state
operation: write
state_key: "agent.{{agent_id}}.session"
value_template: |
  {
    "history": {{history}},
    "tasks": {{task_queue}},
    "progress": "completed",
    "final_checkpoint": "{{cp_id}}"
  }
next: ["/ua/end"]

## /ua/end
type: end
```

---

## 增强七：Oracle 监督协程 — 弥补智力差距

### 7.1 为什么需要 Oracle 监督

```
架构差异总结
═══════════════════════════════════════════════════

Superpowers Skills (LLM 驱动)         AgenticDSL (DSL 驱动)
────────────────────────────         ────────────────────
LLM 即运行时                         C++ 引擎即运行时
每步都由 LLM 决策                    预定义 DAG 确定性执行
灵活但昂贵、非确定                    高效但缺乏适应性
LLM 全程参与思考                     LLM 只在节点处介入

差距: AgenticDSL 在运行时无法 "思考" 和 "调整"
```

**核心问题**：AgenticDSL 的 DSL 图一旦生成，执行路径就确定了。当出现以下情况时，它无法自适应：
- 多次重试仍然失败（不知道换个策略）
- 上下文出现预期外的值（不知道调整）
- 循环检测到死循环（不知道跳出改道）
- 用户中途改变需求（不知道重新规划）
- 多个子目标冲突（不知道权衡优先级）

### 7.2 解决方案：Oracle 监督协程

引入一个**并行运行的监督协程**，在特定反射点介入，调用 LLM "先知" 分析执行状态并给出纠正指令：

```
┌─────────────────────────────────────────────────────────────────┐
│                    Oracle Supervisor Coroutine                   │
│                                                                   │
│  ┌──────────┐    ┌──────────────┐    ┌──────────────────┐        │
│  │  Trace    │ →  │ Reflection   │ →  │ Oracle LLM       │        │
│  │  Monitor  │    │  Point Check │    │ (分析/纠正)      │        │
│  └──────────┘    └──────────────┘    └──────────────────┘        │
│       ↑                                      │                    │
│       │ 执行路径                               │ 纠正指令           │
│       │                                      ↓                    │
│  ┌──────────────────────────────────────────────────┐             │
│  │              Scheduler (主执行循环)                │             │
│  │  Node A → Node B → [Reflection] → Node C → ...   │             │
│  └──────────────────────────────────────────────────┘             │
└─────────────────────────────────────────────────────────────────┘
```

**关键设计原则**：
1. **协程级并行**：监督协程与主调度器在独立协程中运行，不阻塞主执行
2. **反射点机制**：DSL 作者在图中标记需要 LLM 介入的位置
3. **增量纠正**：Oracle 只给出修正指令，不改写整个图
4. **预算控制**：每次 Oracle 调用消耗 `max_oracle_calls` 预算

### 7.3 Oracle 节点类型

```cpp
// ========== Oracle 监督节点 ==========

// 反射点：图中标记需要 LLM 分析的位置
struct ReflectNode : public Node {
    std::string reflection_prompt;    // 告诉 LLM 分析什么
    std::string analysis_variable;    // LLM 分析结果存哪里
    std::vector<std::string> metrics; // 需要监控的指标
    int max_analysis_calls = 3;       // 最多调用 LLM 次数
    std::optional<NodePath> on_critical_failure; // 严重失败跳转
    
    // 触发模式
    enum class Trigger {
        ALWAYS,          // 每次执行到都触发
        ON_FAILURE,      // 只有前置节点失败时触发
        ON_THRESHOLD,    // 指标超过阈值时触发
        PERIODIC         // 每 N 次触发一次
    };
    Trigger trigger_mode = Trigger::ALWAYS;
    float threshold_value = 0.0;       // ON_THRESHOLD 的阈值
    int periodic_interval = 1;         // PERIODIC 的间隔
};

// 纠正注入节点：应用 Oracle 的修正指令
struct CorrectNode : public Node {
    std::string correction_source;    // 从哪个变量读纠正指令
    std::vector<std::string> allowed_actions; // 允许的纠正动作
    bool require_user_confirm = false; // 重大纠正需要用户确认
};
```

### 7.4 监督协程架构

```cpp
class OracleSupervisor {
public:
    struct OracleConfig {
        int max_oracle_calls = 5;       // 最多 Oracle 调用
        int max_oracle_calls_per_minute = 2;
        float oracle_cost_weight = 1.0;  // 相对于普通 LLM 调用的成本
        std::string oracle_model = "gpt-4"; // Oracle 用更强的模型
        bool auto_correct = true;        // 自动应用修正
        bool require_user_confirm_critical = true;
    };

private:
    // 主调度器的弱引用（通过 EventBus 通信）
    std::weak_ptr<SchedulerEventBus> scheduler_bus_;
    LLMProviderRegistry& llm_registry_;
    TraceExporter& trace_store_;
    StateStore& state_store_;
    OracleConfig config_;
    
    // 协程句柄
    std::optional<Awaitable<void>> supervisor_coro_;

public:
    // 启动监督协程（与主调度器并行）
    Awaitable<void> start_supervision(
        std::weak_ptr<SchedulerEventBus> bus
    ) {
        scheduler_bus_ = bus;
        
        // 订阅调度器事件
        bus->subscribe(EventType::NODE_STARTED, 
            [this](const Event& e) { on_node_started(e); });
        bus->subscribe(EventType::NODE_FAILED,
            [this](const Event& e) { on_node_failed(e); });
        bus->subscribe(EventType::REFLECTION_POINT,
            [this](const Event& e) { co_await on_reflection(e); });
        bus->subscribe(EventType::LOOP_DETECTED,
            [this](const Event& e) { co_await on_loop_detected(e); });
    }
    
    // 反射点处理（核心协程）
    Awaitable<void> on_reflection(const Event& event) {
        if (oracle_call_count_ >= config_.max_oracle_calls) {
            co_return; // 超出预算，忽略
        }
        
        // 1. 收集执行上下文
        ExecutionContext ctx = collect_execution_context(event);
        
        // 2. 构建 Oracle prompt
        std::string oracle_prompt = build_oracle_prompt(ctx);
        
        // 3. 调用 Oracle LLM
        LLMResult result = co_await llm_registry_
            .get_provider(config_.oracle_model)
            ->co_generate(oracle_prompt, OracleLLMConfig());
        
        oracle_call_count_++;
        
        // 4. 解析 Oracle 输出
        OracleDecision decision = parse_oracle_decision(result.text);
        
        // 5. 应用纠正（如果需要）
        if (decision.needs_correction) {
            if (decision.severity == Severity::CRITICAL && 
                config_.require_user_confirm_critical) {
                // 等待用户确认
                bool confirmed = co_await wait_for_user_confirm(decision);
                if (!confirmed) co_return;
            }
            
            co_await apply_correction(decision);
        }
    }
};
```

### 7.5 Oracle 的 prompt 设计

Oracle 接收的是**执行路径摘要**而非完整上下文：

```cpp
std::string OracleSupervisor::build_oracle_prompt(
    const ExecutionContext& ctx
) {
    return fmt::format(R"(
你是一个 AgenticDSL 执行监督员。分析以下执行路径，判断是否需要纠正。

## 当前图结构
{graph_summary}

## 已执行路径
{executed_trace}
  ↓ (当前)
→ {current_node}

## 待执行节点
{pending_nodes}

## 上下文状态 (关键值)
{context_snapshot}

## 指标
- 重试次数: {retry_count}
- 失败率: {failure_rate}
- 已用预算: {budget_used}/{budget_total}

## 请分析
1. 当前执行是否正常？
2. 是否有偏离预期的迹象？
3. 是否需要调整执行路径？
4. 如果需要，输出纠正指令。

## 输出格式 (JSON)
{
  "status": "normal|warning|critical",
  "analysis": "分析说明",
  "needs_correction": true/false,
  "severity": "minor|major|critical",
  "correction": {
    "type": "redirect|modify_context|insert_nodes|restart",
    "target": "要操作的节点或变量",
    "value": "新值或新 DSL",
    "reason": "修正理由"
  }
}
)",
    fmt::arg("graph_summary", ctx.graph_summary),
    fmt::arg("executed_trace", ctx.executed_trace),
    fmt::arg("current_node", ctx.current_node),
    fmt::arg("pending_nodes", ctx.pending_nodes),
    fmt::arg("context_snapshot", ctx.context_snapshot),
    fmt::arg("retry_count", ctx.retry_count),
    fmt::arg("failure_rate", ctx.failure_rate),
    fmt::arg("budget_used", ctx.budget_used),
    fmt::arg("budget_total", ctx.budget_total)
    );
}
```

### 7.6 纠正注入机制

Oracle 的决策通过 `CorrectNode` 或调度器事件总线注入：

```cpp
Awaitable<void> OracleSupervisor::apply_correction(
    const OracleDecision& decision
) {
    switch (decision.correction.type) {
        
    case CorrectionType::REDIRECT:
        // 重定向执行流：修改调度器的下一个节点
        co_await scheduler_bus_->emit(CorrectionEvent{
            .type = CorrectionEvent::REDIRECT,
            .from_node = decision.correction.target,
            .to_node = decision.correction.value
        });
        break;
        
    case CorrectionType::MODIFY_CONTEXT:
        // 修改上下文值
        co_await scheduler_bus_->emit(CorrectionEvent{
            .type = CorrectionEvent::MODIFY_CONTEXT,
            .key = decision.correction.target,
            .value = decision.correction.value
        });
        break;
        
    case CorrectionType::INSERT_NODES:
        // 插入新的 DSL 子图到待执行队列
        auto new_graph = markdown_parser_.parse_from_string(
            decision.correction.value
        );
        co_await scheduler_bus_->emit(CorrectionEvent{
            .type = CorrectionEvent::INSERT_GRAPH,
            .graph = std::move(new_graph),
            .before_node = decision.correction.target
        });
        break;
        
    case CorrectionType::RESTART:
        // 回滚到检查点重新执行
        co_await scheduler_bus_->emit(CorrectionEvent{
            .type = CorrectionEvent::RESTART_FROM_CHECKPOINT,
            .checkpoint_id = decision.correction.value
        });
        break;
    }
}
```

### 7.7 反射点 DSL 语法

```yaml
## /main/reflect_after_retry
type: reflect
reflection_prompt: |
  以下操作已重试 3 次仍失败。分析原因并建议:
  1. 是否更换策略？
  2. 是否需要跳过？
  3. 是否需要回退？
analysis_variable: oracle_analysis
trigger_mode: on_failure
# 只在前置节点失败时触发
next: ["/main/apply_oracle"]

## /main/apply_oracle
type: correct
correction_source: oracle_analysis
allowed_actions: ["redirect", "modify_context", "insert_nodes"]
# 严重纠正需要确认
require_user_confirm: true
next: ["/main/continue"]

## /main/periodic_reflection
type: reflect
reflection_prompt: |
  定期健康检查。分析当前进度、质量和风险。
trigger_mode: periodic
periodic_interval: 5
# 每执行 5 个节点触发一次
analysis_variable: health_check
next: ["/main/continue"]

## /main/threshold_guard
type: reflect
reflection_prompt: |
  错误率超过阈值。判断是否继续。
trigger_mode: on_threshold
threshold_value: 0.3
# 失败率超过 30% 时触发
metrics: ["failure_rate", "retry_count"]
analysis_variable: guard_decision
on_critical_failure: "/main/fallback_plan"
next: ["/main/continue"]
```

### 7.8 完整示例：智能自适应工作流

```markdown
### AgenticDSL '/intelligent_workflow'

## /iw/meta
execution_budget:
  max_llm_calls: 50
  max_tool_calls: 100
  max_oracle_calls: 10       # Oracle 预算
    
oracle_config:
  model: gpt-4
  auto_correct: true
  require_user_confirm_critical: true

---

## /iw/start
type: start
next: ["/iw/phase1"]

## /iw/phase1
type: fork
branches: ["/iw/search_a", "/iw/search_b"]
context_isolation: deep_copy
next: ["/iw/join_phase1"]

## /iw/search_a
type: tool_call
tool_name: web_search
arguments:
  query: "{{query_a}}"
output_keys: ["result_a"]
next: ["/iw/end_search_a"]
## /iw/end_search_a
type: end

## /iw/search_b
type: tool_call
tool_name: web_search
arguments:
  query: "{{query_b}}"
output_keys: ["result_b"]
next: ["/iw/end_search_b"]
## /iw/end_search_b
type: end

## /iw/join_phase1
type: join
wait_for: ["/iw/end_search_a", "/iw/end_search_b"]
merge_strategy: deep_merge
next: ["/iw/synthesize"]

## /iw/synthesize
type: dsl_call
llm_tool: llama-7b
output_keys: ["synthesis"]
prompt: |
  综合搜索结果:
  A: {{result_a}}
  B: {{result_b}}
  输出综合分析。
next: ["/iw/reflect_quality"]  # ← 反射点

## /iw/reflect_quality — Oracle 检查合成质量
type: reflect
reflection_prompt: |
  分析了搜索结果并合成。请判断:
  1. 合成结果是否充分回答了用户需求？
  2. 是否需要进一步搜索？
  3. 当前方向是否正确？
  
  用户需求: {{user_requirement}}
  合成结果: {{synthesis}}
analysis_variable: quality_check
trigger_mode: always
next: ["/iw/apply_quality"]

## /iw/apply_quality
type: correct
correction_source: quality_check
allowed_actions: ["redirect", "insert_nodes"]
next: ["/iw/check_oracle"]

## /iw/check_oracle
type: assert
condition: "{{quality_check|find:'正常'}}"
on_failure: "/iw/handle_oracle_redirect"
next: ["/iw/report"]

## /iw/handle_oracle_redirect
type: generate_subgraph
prompt: |
  Oracle 建议重新规划:
  {{quality_check.correction}}
  
  生成新的执行 DSL。
output_keys: ["redirect_graph"]
signature_validation: ignore
next: ["/iw/report"]

## /iw/report
type: tool_call
tool_name: bash
arguments:
  command: |
    echo "=== 执行完成 ==="
    echo "Oracle 调用次数: {{oracle_call_count}}"
    echo "最终结果: {{synthesis}}"
output_keys: ["report"]
next: ["/iw/end"]

## /iw/end
type: end
```

### 7.9 Oracle 监督的三种模式

| 模式 | 适用场景 | LLM 调用频率 | 适应性 | 成本 |
|------|---------|-------------|--------|------|
| **无监督** | 完全确定的流程 | 无 | ❌ 无 | 最低 |
| **反射点监督** | 关键决策点需要 LLM 判断 | 仅在反射点 | ✅ 关键点 | 中 |
| **持续监督** | 高不确定性任务 | 每 N 节点或每失败 | ✅✅ 高 | 高 |

**反射点监督** 是推荐的默认模式——它在确定性的 DAG 执行和 LLM 的灵活性之间取得了平衡。

### 7.10 架构对比总结

```
                         Superpowers Skills
                         ═════════════════
                         LLM 全程决策
                         │
                         ├── 灵活性: ✅✅✅ 高
                         ├── 确定性: ❌ 低
                         ├── 成本:   高 (每步 LLM)
                         └── 可调试: ❌ 难 (非确定)
                         
                         AgenticDSL + Oracle 监督 (本方案)
                         ═══════════════════════════════
                         确定性 DAG + 关键点 LLM 介入
                         │
                         ├── 灵活性: ✅✅ 中高 (仅在反射点)
                         ├── 确定性: ✅✅ 高 (DAG 主干)
                         ├── 成本:   中 (仅反射点调 LLM)
                         └── 可调试: ✅✅ 易 (确定路径可复现)
                         
                         AgenticDSL 纯模式
                         ═════════════════
                         完全确定性的 DAG 执行
                         │
                         ├── 灵活性: ❌ 低 (图生成后固定)
                         ├── 确定性: ✅✅✅ 极高
                         ├── 成本:   低 (仅 dsl_call)
                         └── 可调试: ✅✅✅ 极易
```

### 7.11 实施要点

| 组件 | 复杂度 | 关键实现 |
|------|--------|---------|
| `ReflectNode` | 🟡 中 | 节点类型 + 事件触发 |
| `CorrectNode` | 🟡 中 | 纠正动作路由 |
| `OracleSupervisor` | 🔴 高 | 协程生命周期 + EventBus |
| `Oracle prompt 工程` | 🟡 中 | 执行路径摘要 + 上下文快照 |
| `纠正注入` | 🔴 高 | 与调度器的安全交互 |
| `预算控制` | 🟢 低 | ExtensionBudget 扩展 |

**优先级**：P1（重要）— 在 Fork/Join 和 State 之后实现。

| 文件 | 位置 | 当前行数 | 需要修改 |
|------|------|---------|---------|
| `node.h` | `src/core/types/` | 211 | 添加 UserInputNode, StateNode |
| `node_executor.cpp` | `src/modules/executor/` | ~350 | 实现 execute_fork/join |
| `topo_scheduler.h` | `src/modules/scheduler/` | 83 | 添加协程调度 |
| `execution_session.cpp` | `src/modules/scheduler/` | ~160 | 添加暂停/恢复 |
| `registry.cpp` | `src/common/tools/` | ~60 | 扩展工具接口 |
| `llama_adapter.h` | `src/common/llm/` | ~60 | 统一 LLM 接口 |
| `llm_tool.h` | `src/common/llm/` | ~40 | 合并到统一类型 |
| `llm_types.h` | `src/common/llm/` | ~60 | 合并到统一类型 |
| `context_engine.h` | `src/modules/context/` | ~60 | 添加 StateStore |
| `budget.h` | `src/core/types/` | 125 | 添加 State 预算 |
| `library_loader.cpp` | `src/modules/library/` | ~120 | 填充标准库 |

---

> **总结：AgenticDSL 的核心优势在于 DSL 定义 + 动态图生成 + 预算控制。**
> 通过以上 7 个增强（协程 Fork/Join、user_input、State、LLM 统一、工具扩展、标准库、**Oracle 监督**），
> AgenticDSL 可以在保持确定性 DAG 执行优势的同时，通过在关键反射点引入 LLM 监督协程来弥补智力差距。
> 这种混合架构在**高效确定性**与**灵活适应性**之间取得最佳平衡。
