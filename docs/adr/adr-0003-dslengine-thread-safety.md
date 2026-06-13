# ADR-0003: DSLEngine 线程安全与多实例架构

## 状态

**✅ Approved** (2026-05-12)

## 背景

HydraForge Phase 1 需要支持多 Agent 并发执行，每个 Agent 有独立的 DSLEngine 实例。Phase 2 计划支持沙箱隔离的多租户执行。现有的 `DSLEngine::run()` 存在以下问题：

1. **非线程安全**：`full_graphs_`、`tool_registry_`、`llama_adapter_` 等成员无锁保护
2. **竞态条件**：`library_loader.cpp` 中的 `static bool initialized` 在多线程下存在 race
3. **FORK/JOIN 假并发**：`execute_fork_branches()` 实际是顺序模拟，非真正并发

---

## 决策

### 1. 总体策略：多 DSLEngine 实例 + 内部真正并发

```
┌─────────────────────────────────────────────────────────┐
│  Multi-Agent Harness                                    │
│                                                          │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐     │
│  │ Planner     │  │ Executor    │  │ Critic       │     │
│  │ Agent       │  │ Agent       │  │ Agent        │     │
│  │             │  │             │  │              │     │
│  │ DSLEngine   │  │ DSLEngine   │  │ DSLEngine    │     │
│  │ (Instance A)│  │ (Instance B)│  │ (Instance C) │     │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘     │
│         │                │                │             │
│         └────────────────┼────────────────┘             │
│                          │                              │
│                   Inter-Agent IPC                        │
│                   (AgentMessage queue, Phase 2)          │
└─────────────────────────────────────────────────────────┘

每个 DSLEngine 内部：
- fork/join 通过 std::jthread 实现真正的轻量级并发
- 多 DSLEngine 实例间完全隔离，无共享可变状态
```

### 2. DSLEngine 成员重组

```cpp
// EngineResources: 不可变或线程安全的共享资源
struct EngineResources {
    std::shared_ptr<const std::vector<ParsedGraph>> graphs;  // 不可变图谱
    std::shared_ptr<ToolRegistry> tools;                      // 读写锁保护
    std::shared_ptr<LLMProviderFactory> llm_factory;         // 工厂实例
};

class DSLEngine {
    // Per-execution 状态（每次 run() 创建）
    struct ExecutionState {
        TopoScheduler scheduler;
        ExecutionSession session;
        std::vector<TraceRecord> traces;
    };

    // 共享资源（immutable 或内部同步）
    EngineResources resources_;

    // 历史数据（从 DSLEngine 移出）
    std::vector<ExecutionResult> execution_history_;  // 每个 run() 的结果

    // 线程安全工具注册
    std::shared_mutex tool_mutex_;
};
```

### 3. `full_graphs_` 不可变设计

```cpp
// 加载后不可变，append 创建新副本
using GraphSnapshot = std::shared_ptr<const std::vector<ParsedGraph>>;

class DSLEngine {
    // 初始加载
    void load_graphs(const std::vector<ParsedGraph>& graphs) {
        graphs_ = std::make_shared<const std::vector<ParsedGraph>>(graphs);
    }

    // append 时创建新副本（Copy-on-write）
    void append_graphs(std::vector<ParsedGraph> new_graphs) {
        auto old = graphs_;
        auto merged = std::make_shared<std::vector<ParsedGraph>>(*old);
        merged->insert(merged->end(),
                      std::make_move_iterator(new_graphs.begin()),
                      std::make_move_iterator(new_graphs.end()));
        graphs_ = std::merged;  // 原子替换
    }

private:
    std::atomic<GraphSnapshot> graphs_;  // 原子操作保证线程安全
};
```

**优点**：
- 读取时无锁（shared_ptr const）
- 写入时创建新副本，不阻塞正在执行的 run()

### 4. ToolRegistry 读写锁

```cpp
class ToolRegistry {
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, ToolFunc> tools_;

public:
    // 读操作：多个并发读
    std::optional<json> call_tool(const std::string& name,
                                   const unordered_map<string, string>& args) {
        std::shared_lock lock(mutex_);
        auto it = tools_.find(name);
        if (it == tools_.end()) return std::nullopt;
        return it->second(args);
    }

    // 写操作：独占锁
    void register_tool(const std::string& name, ToolFunc func) {
        std::unique_lock lock(mutex_);
        tools_.emplace(name, std::move(func));
    }
};
```

### 5. 竞态条件修复

```cpp
// library_loader.cpp - 修复前
static bool initialized = false;
if (!initialized) {
    initialized = true;  // RACE: 两个线程同时通过检查
    load_library();
}

// 修复后
static std::once_flag flag;
static std::atomic<bool> loaded = false;

std::call_once(flag, []() {
    load_library();
    loaded = true;
});
```

### 6. FORK/JOIN 真正并发实现

```cpp
// topo_scheduler.cpp - execute_fork_branches 重构
void TopoScheduler::execute_fork_branches(const ForkNode& node,
                                          ExecutionSession& parent_session) {
    std::vector<std::jthread> threads;
    std::vector<BranchResult> results;
    std::mutex results_mutex;

    for (const auto& branch : node.branches) {
        // 每个 branch 在独立线程执行
        threads.emplace_back([&, branch]() {
            // 复制 session 快照（隔离）
            auto session_copy = parent_session.snapshot();
            auto result = execute_single_branch(branch, session_copy);

            std::lock_guard lock(results_mutex);
            results.push_back(std::move(result));
        });
    }

    // 等待所有 branch 完成
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    // 合并结果到 parent_session
    merge_branch_results(parent_session, results);
}
```

**关键设计**：
- 每个 branch 操作独立的 `session_copy`（隔离边界）
- 使用 `std::jthread`（C++20，自动 join）
- 主线程等待所有 branch 完成后再合并

### 7. 多 Agent 通信（Phase 2 预留）

```cpp
// Agent 间消息（Phase 2 实现）
struct AgentMessage {
    std::string from_agent;
    std::string to_agent;
    std::string intent;          // "request", "response", "delegate"
    nlohmann::json payload;
    std::chrono::steady_clock::time_point timestamp;
};

// 每个 Agent 的 EventBus 是独立的
class Agent {
    DSLEngine engine_;
    std::shared_ptr<EventBus> event_bus_;  // 独属于此 Agent
    std::jthread executor_;
};
```

---

## 权衡

### 为什么不是单一 DSLEngine + 锁？

| 方案 | 优点 | 缺点 |
|------|------|------|
| **多 DSLEngine 实例** | 完全隔离，崩溃不传染，易推理 | 工具注册重复，内存占用高 |
| **单 DSLEngine + 内部锁** | 共享资源，内存高效 | 复杂，锁粒度难把握，一 Agent 崩溃影响全局 |

**选择多实例的理由**：
- Phase 2 多租户需要 OS 级隔离
- 多 Agent 是一等公民，不只是"一个引擎的多个执行"
- 工具注册是一次性的，内存成本可接受

### 为什么不用 Process-per-Agent？

| 方案 | 优点 | 缺点 |
|------|------|------|
| **多 DSLEngine 实例（进程内）** | IPC 低延迟，共享内存 | 崩溃在同一进程内 |
| **Process-per-Agent** | OS 级隔离 | IPC 复杂，延迟高 |

**Phase 1 选择进程内多实例**：延迟敏感场景（Agent 间高频通信），Process-per-Agent 留到 Phase 2 沙箱需求明确后再考虑。

---

## 实现要求

### Phase 1 必须完成

| # | 任务 | 验证方式 |
|---|------|---------|
| 1 | `library_loader.cpp` 竞态修复 | `std::call_once` + `std::once_flag` |
| 2 | `graphs_` 不可变改造 | 原子 `shared_ptr<const vector<ParsedGraph>>` |
| 3 | `ToolRegistry` 加 `shared_mutex` | 读多写少场景下 10+ 并发读 |
| 4 | FORK 并发实现 | `std::jthread` + session snapshot |
| 5 | 多实例隔离测试 | 两个线程同时调用 `run()`，无 data race |

### TSAN 验证

```bash
# 编译带 TSAN
cmake -B build -DCMAKE_CXX_FLAGS="-fsanitize=thread"
cmake --build build
ctest -C Debug --output-on-failure

# 或运行
TSAN_OPTIONS=halt_on_error=1 ./build/harness_cli test.agent.md
```

---

## 影响范围

| 组件 | 变更 |
|------|------|
| `src/core/engine.h/cpp` | 添加 `EngineResources`，重构成员 |
| `src/core/library_loader.cpp` | 竞态修复 |
| `src/modules/scheduler/topo_scheduler.cpp` | fork 并发实现 |
| `src/common/tools/registry.h` | 添加 `shared_mutex` |
| `src/harness/harness_engine.h/cpp` | 多 Agent 生命周期管理（Phase 2） |

---

## 替代方案

### 替代 1：全局锁串行化（被否决）

```cpp
// DSLEngine::run() 加全局锁
std::lock_guard<std::mutex> lock(run_mutex_);
execute(context);
```

**否决理由**：无法并发 fork/join，与多 Agent 目标矛盾。

### 替代 2：Actor 模型（被否决）

每个 Agent 是一 Actor，消息传递通信。

**否决理由**：过度工程，Phase 1 不需要。直接函数调用足够。

---

## 结论

采用**多 DSLEngine 实例**架构：

- 每个 Agent 有独立实例，完全隔离
- 内部 FORK/JOIN 通过 `std::jthread` 实现真正并发
- 共享资源（ToolRegistry、LLM Factory）通过 `shared_ptr` + 读写锁管理
- 修复 `library_loader.cpp` 竞态条件
- 预留 Agent 间通信接口（Phase 2）

此设计支持：
- **Phase 1**：单 Agent 内流式 TUI + fork 并发
- **Phase 2**：多 Agent 并发 + 沙箱隔离

---

*文档版本: v1.0*
*最后更新: 2026-05-12*