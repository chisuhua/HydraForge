# IP-001: 增量实现路线图（6 步）

**ID**: IP-001
**日期**: 2026-05-20
**状态**: 已批准（调整版，2026-05-22）
**关联**: ADR-002~007, IP-002
**调研依据**: Session 1-5 全部讨论确认

---

## 概述

这是从当前 AgenticDSL 基线到完整自举系统的实施计划。

**重要调整**（基于 Session 1-5 讨论确认）：
- ModuleState 简化为 Lazy Init + json scope nesting（无 schema/imports/fork_behavior）
- Fork 语义扩展、Checkpoint、静态分析延后到阶段 2
- 推理标准库 7 个子图与核心步骤并行开发
- 实施分为 3 个阶段（而非原 6 个线性步骤）

---

## 三阶段实施计划

### 阶段 1：核心自举能力（目标：Agent 可通过 DSL 控制推理参数）

#### Step 0: Lazy ModuleState（json scope nesting）

**目标**：模块状态通过 json scope nesting 持久化

**设计确认**（Session 3 Oracle）：
- 无 schema 校验
- 无 imports 声明
- 无 fork_behavior 配置
- 首次 dsl_call 时自动创建空 json

**涉及文件**：

| 文件 | 改动 |
|------|------|
| `src/modules/scheduler/execution_session.h` | 新增 `module_states_: map<string, json>` |
| `src/modules/scheduler/execution_session.cpp` | Lazy init：首次 dsl_call 时创建空 json |
| `src/modules/executor/node_executor.cpp` | dsl_call 执行时传入 module_states_ 引用 |

**关键代码**：

```cpp
// execution_session.h
class ExecutionSession {
    // ... 现有成员 ...
    std::map<std::string, nlohmann::json> module_states_; // 新增
};

// execution_session.cpp - Lazy init
void ExecutionSession::ensure_module_state(const std::string& module_path) {
    if (module_states_.find(module_path) == module_states_.end()) {
        module_states_[module_path] = nlohmann::json::object(); // 空对象
    }
}
```

**验证方式**：
```yaml
# 测试模块
## /test/counter
module: "test::counter"

## /test/increment
  type: assign
  assign:
    count: "{{ (module_states.test_counter.count | default(0)) + 1 }}"
  next: ["/test/save"]

## /test/save
  type: assign
  assign:
    module_states.test_counter.count: "{{ count }}"
  next: ["/end_soft"]

# 第一次调用 → count = 1
# 第二次调用 → count = 2（验证状态持久化）
```

**工作量**：1-2 天

---

#### Step 1: Session Registry + Session Vars

**目标**：多 Session 隔离、Session 级变量

**设计确认**（Session 3 Oracle）：
- SessionRegistry 由 DSLEngine 持有（与 ToolRegistry 模式一致）
- SessionVars 在 ExecutionSession 中隔离

**涉及文件**：

| 文件 | 改动 |
|------|------|
| `src/modules/scheduler/session_registry.h/cpp`（新文件） | SessionRegistry 类 |
| `src/modules/scheduler/execution_session.h` | 新增 `session_id_`、`session_vars_` |
| `src/common/tools/` | 注册 `session.create`、`session.destroy` 工具 |

**关键代码**：

```cpp
// session_registry.h
class SessionRegistry {
public:
    std::string create_session(const SessionConfig& config);
    void destroy_session(const std::string& id);
    ExecutionSession& get_session(const std::string& id);
    
private:
    std::unordered_map<std::string, std::unique_ptr<ExecutionSession>> sessions_;
    std::mutex mutex_;
};

// engine.h
class DSLEngine {
    // ... 现有成员 ...
    SessionRegistry session_registry_; // 新增（与 tool_registry_ 并列）
};
```

**验证方式**：
- 创建两个 Session → 各自设置 session_vars → 验证互不影响
- 销毁 Session → 验证状态释放

**工作量**：2-3 天

---

#### Step 2: YIELD / STREAM（单节点 + mode 参数）

**目标**：token-by-token 生成器

**设计确认**（Session 3 Oracle）：
- 单 YIELD 节点类型（无 CONTINUE_STREAM/STOP_STREAM）
- mode 参数控制行为：next/continue/stop
- Session 持有 `std::optional<YieldState> pending_yield_`

**涉及文件**：

| 文件 | 改动 |
|------|------|
| `src/core/types/node.h` | NodeType 枚举新增 `YIELD` |
| `src/core/types/node.h` | 新增 `YieldNode` 结构（含 mode 字段） |
| `src/modules/executor/node_executor.h/cpp` | 新增 `execute_yield()` |
| `src/modules/scheduler/topo_scheduler.cpp` | yield 暂停逻辑 + resume 回调 |

**关键代码**：

```cpp
// node.h
enum class YieldMode : uint8_t {
    NEXT,      // 产生一个值，等待下次调用
    CONTINUE,  // 继续生成（流式）
    STOP       // 停止生成
};

struct YieldNode : public Node {
    std::string yield_value;  // 模板表达式
    YieldMode mode = YieldMode::NEXT;
    
    YieldNode(NodePath path, std::string value, YieldMode m, std::vector<NodePath> next = {});
};

// topo_scheduler.cpp
void TopoScheduler::handle_yield(const YieldNode* node, const Context& ctx) {
    auto value = render_template(node->yield_value, ctx);
    
    // 保存恢复状态
    session_.pending_yield_ = {
        .module_id = current_module_,
        .node_id = node->path,
        .resume_context = ctx
    };
    
    // 返回 yield 值给调用者
    result["__yield__"] = {
        {"value", value},
        {"mode", yield_mode_to_string(node->mode)}
    };
}
```

**验证方式**：
- 写一个 token_generator.agent.md → 调用多次 → 验证每次返回一个 token
- 验证 yield 之间的 module_state 保持

**工作量**：2-3 天

---

### 并行任务：推理标准库 7 个子图

与阶段 1 同步进行（Oracle 确认全部现在可构建）：

| 批次 | 子图 | 依赖 | 时间 |
|------|------|------|------|
| **2a（立即）** | engine.md | 纯 tool_call | 与 Step 0 同步 |
| **2a（立即）** | model.md | 纯 tool_call | 与 Step 0 同步 |
| **2a（立即）** | session.md | dsl_call 聚合 | 与 Step 1 同步 |
| **2b（1 周后）** | prefix_cache.md | json scope nesting | Step 0 完成后 |
| **2b（1 周后）** | kv_cache.md | json scope nesting | Step 0 完成后 |
| **2b（1 周后）** | decoding.md | json scope nesting | Step 0 完成后 |
| **2c（2 周后）** | batching.md | queue 管理 | Step 2 完成后 |

**已创建**：engine.md, model.md, session.md（3/7）

---

### 阶段 2：增强能力（目标：Agent 可编排复杂工作流）

#### Step 3: Fork 语义扩展（per-field behavior）

**目标**：支持 COW / INHERIT / SHARE_READONLY

**状态**：延后（当前 deep_copy 已够用）

**触发条件**：性能测试显示 deep_copy 成为瓶颈

**工作量**：2-3 天（未来）

---

#### Step 4: Checkpoint / Restore

**目标**：Session 状态可序列化

**状态**：延后（运维能力，非自举阻塞）

**触发条件**：需要 Session 迁移或容错

**工作量**：1-2 天（未来）

---

#### Step 5: 静态分析优化

**目标**：消除热路径首次调用延迟

**状态**：延后（性能优化）

**触发条件**：生产环境性能测试

**工作量**：1-2 天（未来）

---

### 阶段 3：自进化（目标：Agent 自主改进 DSL）

待规划。触发条件：阶段 1+2 完成且稳定运行。

---

### Step 2: Session Registry + Session Vars

**目标**：多 Session 隔离、Session 级变量

**涉及文件**：

| 文件 | 改动 |
|------|------|
| `src/modules/scheduler/` (新文件) | `session_registry.h/cpp` — 全局 Session 注册表 |
| `src/modules/scheduler/execution_session.h` | 新增 `session_id_`、`session_vars_` |
| `src/modules/scheduler/execution_session.cpp` | session_vars 读写逻辑 |
| `src/common/tools/` (注册新工具) | `session.create`、`session.destroy`、`session.prewarm_modules` |
| `src/modules/executor/node_executor.cpp` | 工具调用透传 session_id |

**关键代码**：

```cpp
// session_registry.h
class SessionRegistry {
public:
    std::string create_session(const SessionConfig& config);
    void destroy_session(const std::string& id);
    SessionState& get_session(const std::string& id);
    std::vector<std::string> list_active_sessions() const;

private:
    std::unordered_map<std::string, SessionState> sessions_;
    std::mutex mutex_;
};
```

**验证方式**：
- 创建两个 Session → 各自设置 session_vars → 验证互不影响
- 销毁 Session → 验证状态释放

---

### Step 3: YIELD / STREAM 节点类型

**目标**：token-by-token 生成器

**涉及文件**：

| 文件 | 改动 |
|------|------|
| `src/core/types/node.h` | NodeType 枚举新增 `YIELD`、`CONTINUE_STREAM`、`STOP_STREAM` |
| `src/core/types/node.h` | 新增 `YieldNode` 结构（yield_value 字段） |
| `src/modules/executor/node_executor.h` | 新增 `execute_yield()` |
| `src/modules/executor/node_executor.cpp` | dispatch 加入 YIELD 分支 |
| `src/modules/scheduler/topo_scheduler.cpp` | yield 暂停逻辑 + resume_on 回调 |
| `src/modules/scheduler/execution_session.cpp` | yield 时保存 module_state 快照 |

**关键代码**：

```cpp
// node.h
struct YieldNode : public Node {
    std::string yield_value;  // 模板表达式，如 "{{token}}"
    
    YieldNode(NodePath path, std::string value, std::vector<NodePath> next = {});
    Context execute(Context& ctx) override;
    std::unique_ptr<Node> clone() const override;
};

// 执行器
Context NodeExecutor::execute_yield(const YieldNode* node, const Context& ctx) {
    // 1. 渲染 yield_value 模板
    auto value = renderer_.render(node->yield_value, ctx);
    
    // 2. 创建 yield 上下文（含值 + resume_at）  
    Context result = ctx;
    result["__yield__"] = {
        {"value", value},
        {"resume_at", node->next.empty() ? "" : node->next[0]}
    };
    return result;
}
```

**验证方式**：
- 写一个 token_generator.agent.md → 调用 10 次 → 验证每次返回一个 token
- 验证 yield 之间的 module_state 保持

---

### Step 4: Fork 语义扩展

**目标**：per-field fork 行为

**涉及文件**：

| 文件 | 改动 |
|------|------|
| `src/core/types/node.h` | ForkNode 新增 `fork_behaviors: map<string, ForkBehavior>` |
| `src/modules/parser/markdown_parser.cpp` | 解析 `fork_behaviors:` 字段 |
| `src/modules/scheduler/topo_scheduler.cpp` | `start_fork_simulation()` 中处理 fork_behaviors |
| `src/core/types/context.h` | 新增 `CowState` 辅助类 |

**关键代码**：

```cpp
// topo_scheduler.cpp — fork 时
void TopoScheduler::start_fork_simulation(Context& ctx, const ForkNode* node) {
    auto& session = session_.get_session(session_id_);
    for (auto& [field_name, behavior] : node->fork_behaviors) {
        auto& module_state = session.module_states[field_name];
        switch (behavior) {
            case ForkBehavior::DEEP_COPY:
                // 创建完整副本
                break;
            case ForkBehavior::COW:
                // 包装为 CowState
                break;
            case ForkBehavior::INHERIT:
                // 复制当前值
                break;
            case ForkBehavior::SHARE_READONLY:
                // 共享引用
                break;
        }
    }
}
```

**验证方式**：
- Fork 两个分支 → 分支 A 修改 module_state → 分支 B 不受影响
- COW 行为：读零开销，写才复制

---

### Step 5: Checkpoint / Restore

**目标**：Session 状态可序列化，支持迁移和容错

**涉及文件**：

| 文件 | 改动 |
|------|------|
| `src/modules/scheduler/session_registry.h` | 新增 `checkpoint()`, `restore()` |
| `src/common/tools/` (注册新工具) | `session.checkpoint`, `session.restore` |

**关键代码**：

```cpp
json SessionRegistry::checkpoint(const std::string& id) {
    auto& session = get_session(id);
    json cp;
    cp["session_id"] = session.session_id;
    cp["session_vars"] = session.session_vars;
    cp["module_states"] = {};
    for (auto& [path, state] : session.module_states) {
        cp["module_states"][path] = state.state;
    }
    // 写入文件
    std::string path = "checkpoints/" + id + ".json";
    std::ofstream(path) << cp.dump(2);
    return {{"path", path}};
}
```

**验证方式**：
- checkpoint 一个运行中的 Session → 销毁 → restore → 验证状态恢复

---

### Step 6: 静态分析优化

**目标**：消除热路径的首次调用延迟

**涉及文件**：

| 文件 | 改动 |
|------|------|
| `src/modules/library/library_loader.h` | 新增 `build_reachability_graph()` |
| `src/modules/scheduler/execution_session.cpp` | 解析时预初始化可达模块 |

**验证方式**：
- 静态分析标记的模块 vs 实际运行时调用 → 覆盖率 > 90%
- 预热后的首次调用延迟 < 未预热时的 10%

---

## 时间估算

| 阶段 | 步骤 | 核心改动文件数 | 估算工作量 |
|------|------|--------------|-----------|
| **阶段 1** | Step 0: Lazy ModuleState | 3 文件 | 1-2 天 |
| **阶段 1** | Step 1: Session Registry | 3 文件 | 2-3 天 |
| **阶段 1** | Step 2: YIELD / STREAM | 4 文件 | 2-3 天 |
| **阶段 1** | 并行：推理标准库 7 个子图 | 7 文件 | 与阶段 1 同步 |
| **阶段 2** | Step 3: Fork 扩展 | 3 文件 | 2-3 天（延后） |
| **阶段 2** | Step 4: Checkpoint | 2 文件 | 1-2 天（延后） |
| **阶段 2** | Step 5: 静态分析 | 2 文件 | 1-2 天（延后） |

**阶段 1 总计**：5-8 天（原 11-19 天）
**阶段 2 总计**：4-7 天（未来按需启动）

---

## 推理优化专项计划（已更新）

基于 [BOOT-001: 自举实施路径方案](../implementation/self-bootstrapping-path.md) 和 Oracle 架构建议，更新推理优化专项实施计划：

### 阶段 0：云端集成 + 质量保障（1-2 周）

**目标**：建立云端 LLM 集成，确保推理质量，为自举提供可靠基础

**原因**：
- 本地 llama.cpp 质量不足，无法支撑自举
- 需要云端 LLM 作为老师模型
- 自举的前提是可靠的推理输出

**实施步骤**：
1. 实现 `CloudLLMAdapter`：支持 OpenAI/Anthropic API
2. 实现 `LLMRouter`：云端/本地路由决策
3. 注册云端推理工具：`inference.cloud_generate`, `inference.local_generate`, `inference.route`
4. 验证功能正常

**工作量**：3-5 天
**优先级**：P0（阻塞所有优化）

### 阶段 1：质量评估闭环 + 服务分层（2-3 周）

**目标**：建立质量评估机制，实现云端/本地服务分层

**实施**：
- 质量评估节点：`quality_eval.md`（快速规则 + 深度评估）
- 服务分层路由：`router.md`（自动选择云端/本地）
- 回退机制：本地质量不达标时自动回退云端

**工作量**：5-7 天
**优先级**：P1

### 阶段 2：反馈闭环 + 自适应优化（3-4 周）

**目标**：建立质量反馈闭环，实现自适应优化

**实施**：
- `QualityFeedbackController`：记录和分析质量数据
- 自适应优化循环：根据质量反馈调整路由策略
- 老师模型数据收集：收集 (输入, 本地输出, 云端纠正) 三元组

**工作量**：7-10 天
**优先级**：P2

### 阶段 3：服务化 + 完全自举（4-6 周）

**目标**：提供推理 API 服务，Agent 自主发现优化策略

**实施**：
- `InferenceServer`：提供 MCP + OpenAI 兼容接口
- 元学习优化器：基于历史数据推荐策略
- 完全自举：Agent 自主发现新的优化策略组合

**工作量**：10-15 天
**优先级**：P3

**详细计划**：见 [BOOT-001: 自举实施路径方案](../implementation/self-bootstrapping-path.md)

### 关键组件

| 组件 | 文档 | 状态 |
|------|------|------|
| 推理路由器 | [ROUTER-001](../architecture/inference-router.md) | 已设计 |
| 质量评估器 | [QUALITY-001](../architecture/quality-evaluator.md) | 已设计 |
| 云端适配器 | BOOT-001 任务 0.1 | 待实现 |
| 反馈控制器 | BOOT-001 任务 2.1 | 待实现 |
| 推理服务器 | BOOT-001 任务 3.1 | 待实现 |

---

## 关联文档

| 文档 | 关系 |
|------|------|
| [02-code-mapping.md](02-code-mapping.md) | 每步的精确代码改动位置 — 本文路径的代码级展开 |
| [VN-001: 自举愿景](../vision/01-self-bootstrapping-vision.md) | 实施路线图的顶层目标 — 6 步完成后进入自举阶段 |
| [VN-002: 语言演进路线图](../vision/02-language-evolution-roadmap.md) | 3阶段演进目标 — 实施路线图与阶段 A/B/C 的映射 |
| [ADR-002: Session 隔离模型](../session-state/01-isolation-model.md) | Step 1~2 的架构设计 |
| [ADR-003: 内部状态模型](../session-state/02-internal-state-model.md) | Step 3~4 的架构设计（YIELD + Fork） |
| [ADR-006: 推理标准库](../inference-stdlib/01-interface-design.md) | Step 1+2 完成后的推理标准库工具接口 |
| [RES-001: 推理引擎调研报告](../research/inference-engine-research.md) | vLLM/SGLang/llama.cpp 深度调研，指导推理优化实施 |
| [ARCH-001: 总体推理架构](../architecture/inference-architecture.md) | 推理子图的上层架构设计 |
| [ROUTER-001: 推理路由器](../architecture/inference-router.md) | 云端/本地路由决策设计 |
| [QUALITY-001: 质量评估器](../architecture/quality-evaluator.md) | 质量评估闭环设计 |
| [OPT-001: 优化方向方案](../optimization/inference-optimization-strategies.md) | 6 个优化维度的具体策略 |
| [BOOT-001: 自举实施路径方案](../implementation/self-bootstrapping-path.md) | 自举实施路径（已更新） |
| [docs/adr/](../../adr/) | 现有 18 个 ADR 中，ADR-0003（线程安全）和 ADR-0008（结构化 Context）是实施前提 |
