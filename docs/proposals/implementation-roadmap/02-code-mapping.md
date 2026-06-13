# IP-002: 代码库扩展点映射

**ID**: IP-002
**日期**: 2026-05-20
**状态**: 草案
**关联**: ADR-002~005, IP-001

---

## 说明

本文件将 ADR-002~005 的决策映射到当前代码库的具体文件和扩展点。
按模块组织，标注每个改动在代码中的精确位置。

---

## 1. src/core/types/node.h

**当前状态**：10 种 NodeType，6 种节点结构体

```
NodeType: START, END, ASSIGN, DSL_CALL, TOOL_CALL, RESOURCE, FORK, JOIN, GENERATE_SUBGRAPH, ASSERT
Node结构体: StartNode, EndNode, AssignNode, DSLNode, ToolCallNode, ResourceNode, ForkNode, JoinNode, GenerateSubgraphNode, AssertNode
```

### 需要新增

```cpp
// 1. NodeType 枚举扩展（第 20 行附近）
enum class NodeType : uint8_t {
    // ... 现有 10 种 ...
    YIELD,            // ← 新增（Step 3）
    CONTINUE_STREAM,  // ← 新增（Step 3）
    STOP_STREAM,      // ← 新增（Step 3）
};

// 2. 模块状态声明（第 64 行 ParsedGraph 附近）
struct ModuleStateDef {     // ← 新增（Step 1）
    std::string name;
    std::string type;
    std::string scope;      // "session" | "module_call"
    nlohmann::json init_value;
    ForkBehavior fork_behavior;
};

struct ImportField {        // ← 新增（Step 1）
    std::string name;
    std::string access;     // "readonly" | "readwrite"
};

struct ImportDef {          // ← 新增（Step 1）
    std::string module_path;
    std::vector<ImportField> fields;
};

enum class ForkBehavior : uint8_t {  // ← 新增（Step 1/4）
    DEEP_COPY,
    COW,
    INHERIT,
    SHARE_READONLY
};

// 3. ParsedGraph 扩展（第 64 行）
struct ParsedGraph {
    // ... 现有字段 ...
    std::optional<std::vector<ModuleStateDef>> module_state_schema;  // ← 新增
    std::optional<std::vector<ImportDef>> imports;                    // ← 新增
};

// 4. ForkNode 扩展（第 169 行）
struct ForkNode : public Node {
    std::vector<NodePath> branches;
    std::map<std::string, ForkBehavior> fork_behaviors;  // ← 新增（Step 4）
    // ...
};

// 5. YieldNode 新增（第 210 行附近）
struct YieldNode : public Node {    // ← 新增（Step 3）
    std::string yield_value;
    // ...
};
```

---

## 2. src/modules/parser/markdown_parser.cpp

**当前状态**：从 YAML 映射到节点结构体

### 需要新增/修改

```cpp
// 1. create_node_from_json() 中解析 YAML
// 在 YAML 解析循环中新增：

// module_state: 解析（Step 1）
if (node_yaml.contains("module_state")) {
    for (const auto& [name, def] : node_yaml["module_state"].items()) {
        ModuleStateDef msd;
        msd.name = name;
        msd.type = def.value("type", "json");
        msd.scope = def.value("scope", "session");
        msd.init_value = def.value("init", nlohmann::json{});
        // fork_behavior 解析
        current_graph.module_state_schema->push_back(msd);
    }
}

// 2. imports_module_state: 解析（Step 1）
if (node_yaml.contains("imports_module_state")) {
    // 解析 ImportDef 列表
}

// 3. fork_behaviors: 解析（Step 4）
if (node_yaml.contains("fork_behaviors")) {
    // 读取 fork_behaviors map 并附加到 ForkNode
}

// 4. yield 节点解析（Step 3）
if (type_str == "yield") {
    auto node = std::make_unique<YieldNode>(path, yield_value, next);
}
```

---

## 3. src/modules/scheduler/topo_scheduler.cpp

**当前状态**：Fork 模拟通过 `start_fork_simulation` / `execute_fork_branches` / `complete_fork` 处理

### 需要新增/修改

```cpp
// 1. start_fork_simulation() 中处理 fork_behaviors（Step 4）
void TopoScheduler::start_fork_simulation(Context& ctx, const ForkNode* node) {
    // ... 现有 fork 逻辑 ...
    
    // 新增：处理 per-field fork behavior
    for (auto& [field, behavior] : node->fork_behaviors) {
        auto& module_state = session_.get_module_state(field);
        switch (behavior) {
            case ForkBehavior::DEEP_COPY:
                // 创建完整副本
                current_fork_branch_contexts_.push_back(
                    deep_copy_state(module_state));
                break;
            case ForkBehavior::COW:
                current_fork_branch_contexts_.push_back(
                    CowState(module_state));
                break;
            case ForkBehavior::INHERIT:
                current_fork_branch_contexts_.push_back(
                    module_state); // 复制值
                break;
            case ForkBehavior::SHARE_READONLY:
                // 共享引用
                break;
        }
    }
}

// 2. yield 支持（Step 3）
// execute() 循环中检测 yield context
auto result = session_.execute_node(current_node, ctx);
if (result.new_context.contains("__yield__")) {
    // 暂停执行，保存 resume_at
    paused_nodes_[session_id_] = {
        .module = current_module,
        .resume_at = result.new_context["__yield__"]["resume_at"],
        .state = result.new_context
    };
    break;  // 跳出执行循环
}
```

---

## 4. src/modules/scheduler/execution_session.h/cpp

**当前状态**：单次执行封装，含 context_engine_、budget_controller_、node_executor_

### 需要新增/修改

```h
// execution_session.h
class ExecutionSession {
public:
    // ... 现有接口 ...
    
    // 新增（Step 2）
    void set_session_vars(const nlohmann::json& vars);
    nlohmann::json& get_session_vars();
    
    // 新增（Step 1）
    nlohmann::json& get_module_state(const std::string& path);
    void init_module_state(const std::string& path, 
                            const std::vector<ModuleStateDef>& schema);
    
    // 新增（Step 3）
    bool is_yielded() const;
    std::optional<NodePath> get_resume_at() const;
    void resume();
    
private:
    // 新增成员
    std::string session_id_;
    nlohmann::json session_vars_;
    std::map<std::string, nlohmann::json> module_states_;
    std::optional<NodePath> yield_resume_at_;
};
```

```cpp
// execution_session.cpp — execute_node 中新增（Step 1）
ExecutionResult ExecutionSession::execute_node(Node* node, const Context& ctx) {
    // 在执行前：确保模块状态已初始化
    if (auto* dsl_node = dynamic_cast<DSLNode*>(node)) {
        // 检查子图是否有 module_state 声明
        auto& subgraph = find_subgraph(dsl_node->subgraph_name);
        if (subgraph.module_state_schema) {
            init_module_state(dsl_node->subgraph_name, 
                              *subgraph.module_state_schema);
        }
    }
    
    // ... 现有执行逻辑 ...
}
```

---

## 5. src/modules/executor/node_executor.h/cpp

**当前状态**：`execute_node()` dispatch → execute_xxx() 每个类型一个方法

### 需要新增

```h
// node_executor.h
class NodeExecutor {
    // ... 现有 11 个 execute_* 方法 ...
    
    // 新增（Step 3）
    Context execute_yield(const YieldNode* node, const Context& ctx);
};
```

```cpp
// node_executor.cpp — dispatch 中新增
Context NodeExecutor::execute_node(Node* node, const Context& ctx) {
    switch (node->type) {
        // ... 现有 case 分支 ...
        case NodeType::YIELD:
            return execute_yield(static_cast<const YieldNode*>(node), ctx);
        // ...
    }
}
```

---

## 6. src/modules/scheduler/（新文件）session_registry.h/cpp

**全新文件**（Step 2）：

```h
class SessionRegistry {
public:
    std::string create_session(const SessionConfig& config);
    void destroy_session(const std::string& id);
    SessionState& get_session(const std::string& id);
    
    nlohmann::json checkpoint(const std::string& id);
    void restore(const std::string& id, const nlohmann::json& checkpoint);
    
private:
    std::unordered_map<std::string, SessionState> sessions_;
    std::mutex mutex_;
};
```

---

## 7. 工具注册（src/common/tools/）

**需注册的新工具**：

| 工具名 | Step | 对应 C++ 函数 |
|--------|------|-------------|
| `session.create` | 2 | SessionRegistry::create_session |
| `session.destroy` | 2 | SessionRegistry::destroy_session |
| `session.set_var` | 2 | ExecutionSession::set_session_vars |
| `session.get_var` | 2 | ExecutionSession::get_session_vars |
| `session.prewarm_modules` | 1 | SessionRegistry::prewarm |
| `session.checkpoint` | 5 | SessionRegistry::checkpoint |
| `session.restore` | 5 | SessionRegistry::restore |
| `stream.resume` | 3 | TopoScheduler::resume_yielded |

---

## 改动统计

| 文件 | 位置 | 改动类型 | Step |
|------|------|---------|------|
| `src/core/types/node.h` | ~20行, ~64行, ~169行, ~210行 | 新增结构体/枚举 | 1,3,4 |
| `src/modules/parser/markdown_parser.cpp` | create_node_from_json | 解析新 YAML 字段 | 1,3,4 |
| `src/modules/scheduler/topo_scheduler.h` | fork 方法区 | 扩展 fork 行为 | 4 |
| `src/modules/scheduler/topo_scheduler.cpp` | execute / fork 方法 | yield 暂停 + fork_behavior | 3,4 |
| `src/modules/scheduler/execution_session.h` | 类定义 | 新增 session_vars/module_states | 1,2 |
| `src/modules/scheduler/execution_session.cpp` | execute_node | module_state init | 1,2 |
| `src/modules/executor/node_executor.h` | 方法列表 | 新增 execute_yield | 3 |
| `src/modules/executor/node_executor.cpp` | dispatch switch | YIELD 分支 | 3 |
| `src/modules/scheduler/session_registry.h/.cpp` | 新文件 | SessionRegistry 类 | 2,5 |
| `src/common/tools/registry.cpp` (init) | register_default_tools | 注册新工具 | 2,3,5 |

---

## 关联文档

| 文档 | 关系 |
|------|------|
| [01-roadmap.md](01-roadmap.md) | 实施路线图 — 本文的 6 步代码映射的上层计划 |
| [ADR-002: Session 隔离模型](../session-state/01-isolation-model.md) | Step 1~2 的架构设计 — 代码映射的改造目标 |
| [ADR-003: 内部状态模型](../session-state/02-internal-state-model.md) | Step 3~4 的架构设计 — YIELD + Fork 代码实现依据 |
| [ADR-006: 推理标准库](../inference-stdlib/01-interface-design.md) | Step 1+2 完成后推理工具注册接口 |
| [docs/specs/dsl.md](../../specs/dsl.md) | 节点类型文档，新增 YIELD 时需与之对齐 |
