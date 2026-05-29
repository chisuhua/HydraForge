# Implementation Slices — 端到端验证切片

> 将 ADR 决策转化为可执行的验证切片，每个切片验证一组核心架构假设。
>
> **创建**: 2026-05-28 | **关联**: ADR-0036, ADR-0034, ADR-0031, ADR-0004, ADR-0030, ADR-0025

---

## 设计原则

1. **每切片一个假设**——失败时精确定位哪个 ADR 设计有问题
2. **依赖递进**——Slice N 只依赖 Slice < N 的成果
3. **可独立验证**——每个切片有明确的"通/不通"标准
4. **最小代码量**——只实现验证假设所需的最少功能，不做工程完备

## 切片依赖图

```
Slice 01 (三层调用链)          ← 无依赖，可立即开始
  ├──→ Slice 02 (多模型路由)   ← 依赖 01
  │       └──→ Slice 04 (舰队并行)  ← 依赖 02
  └──→ Slice 03 (审批流程)     ← 依赖 01，与 02 并行
```

---

## Slice 01：三层调用链（最小可行路径）

### 验证目标

基座层 → 认知层 → 领域层 的完整调用链路通路。验证 ADR-0036 混合内核架构的核心假设：三层能通。

### 涉及 ADR

| ADR | 验证条款 |
|-----|---------|
| ADR-0036 二.2.1 | `ICognitiveOrchestrator.process()` 调用链路 |
| ADR-0036 四 | 调用方向约束（基座→认知→领域） |
| ADR-0034 二 | `ModelRegistry` + `IModelRouter` 基座服务 |
| ADR-0004 | `ToolRegistry` 工具注册与 `call()` |

### 场景

```
用户输入 "读取文件 main.cpp 的内容"
→ orchestrator.process(session_id, callback)
  → LLM 解析意图 → 识别出 "code::read_file"
  → ToolRegistry.call("code::read_file", {path: "main.cpp"})
  → ToolResult{ok:true, data: 文件内容}
→ callback(result)
→ 终端输出文件内容
```

### 文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/common/llm/llm_types.h` | 修改 | 添加 `ModelCapability` 结构体 |
| `src/common/llm/llm_types.h` | 修改 | 添加 `ILLMProvider::available_models()` + `is_model_available()` 默认实现 |
| `src/common/llm/model_registry.h` | 新建 | `ModelRegistry` 类 |
| `src/common/llm/model_registry.cpp` | 新建 | `register_model()` / `get_provider()` |
| `src/common/llm/model_router.h` | 新建 | `IModelRouter` 接口 + `RoutingContext` |
| `src/common/llm/default_model_router.h` | 新建 | `DefaultModelRouter`（始终返回第一个模型） |
| `src/cognitive/simple_orchestrator.h` | 新建 | `SimpleCognitiveOrchestrator` MVP 实现 |
| `src/cognitive/simple_orchestrator.cpp` | 新建 | 硬编码 ReAct：LLM → 解析工具调用 → 执行 |
| `examples/slice_01_tool_call/main.cpp` | 新建 | 入口：构建服务 → 注册工具 → process → 输出 |
| `examples/slice_01_tool_call/CMakeLists.txt` | 新建 | 链接 `agenticdsl_core` |

### 关键接口

```cpp
// ModelRegistry（基座层）
class ModelRegistry {
    void register_model(std::string id, ModelCapability cap, ILLMProvider* provider);
    ILLMProvider* get_provider(const std::string& id);
    std::vector<ModelCapability> all_models();
};

// IModelRouter（基座层）
class IModelRouter {
    virtual std::string route(const RoutingContext& ctx) = 0;
    virtual std::string name() const = 0;
};

// ICognitiveOrchestrator（认知层）
class ICognitiveOrchestrator {
    virtual void process(const std::string& session_id,
                         std::function<void(ExecutionResult)> on_complete) = 0;
};

// SimpleCognitiveOrchestrator：单轮 ReAct
// process() 内部：
//   1. 调 LLM → 解析出 tool_call
//   2. registry.call(tool_call)
//   3. callback(result)
```

### 成功标准

```
终端执行：
  $ ./slice_01_tool_call

输出：
  [Input] 读取文件 main.cpp 的内容
  [LLM] intent: call code::read_file(path=main.cpp)
  [Tool] code::read_file → OK (320 bytes)
  [Output] #include <iostream> ...
```

**失败信号**：LLM 调用失败、ToolRegistry 找不到工具、结果不返回。

---

## Slice 02：多模型路由

### 验证目标

路由器根据任务类型选择不同模型。验证 ADR-0034 的 `DefaultModelRouter` 路由逻辑。

### 前置条件

Slice 01 完成（三层链路已通）。

### 涉及 ADR

| ADR | 验证条款 |
|-----|---------|
| ADR-0034 四 | `DefaultModelRouter` 任务类型匹配 |
| ADR-0034 五 | 使用模式：`route()` → `get_provider()` → `generate()` |
| ADR-0001 | `ILLMProvider` 扩展的 `available_models()` 查询 |

### 场景

```
输入两种任务：
  "这个函数有什么bug？" → task_type=analysis → 路由到 Pro
  "给这个函数加个注释" → task_type=format → 路由到 Flash

流程：
  ModelRegistry 注册 2 个 Provider（Pro + Flash）
  DefaultModelRouter 根据 task_type 选择模型
  每次调用打印路由决策和原因
```

### 文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/common/llm/default_model_router.h` | 修改 | 实现 `route()`：任务类型 → 模型映射 |
| `src/common/llm/default_model_router.cpp` | 新建 | 路由逻辑实现 |
| `examples/slice_02_routing/main.cpp` | 新建 | 注册 Pro + Flash → 两次调用 → 比较路由结果 |
| `examples/slice_02_routing/CMakeLists.txt` | 新建 | 链接 `agenticdsl_core` |

### 关键接口

```cpp
// DefaultModelRouter 路由逻辑（简化）
class DefaultModelRouter : public IModelRouter {
    std::string route(const RoutingContext& ctx) override {
        if (ctx.task_type == "analysis" || ctx.task_type == "architecture")
            return "deepseek-v4-pro";           // 复杂任务 → Pro
        if (ctx.task_type == "format" || ctx.task_type == "test")
            return "deepseek-v4-flash";          // 简单任务 → Flash
        return "deepseek-v4-flash";              // 默认 → Flash
    }
};
```

### 成功标准

```
终端执行：
  $ ./slice_02_routing

输出：
  [Task] 分析 bug → Router: 「deepseek-v4-pro (reason: complex analysis)」
  [LLM Pro] generating... (模拟 500ms)
  [Task] 加注释 → Router: 「deepseek-v4-flash (reason: simple format)」
  [LLM Flash] generating... (模拟 100ms)
```

**失败信号**：两种任务路由到同一模型、路由原因不明确。

---

## Slice 03：审批流程

### 验证目标

写入工具需要用户确认，只读工具直接执行。验证 ADR-0031 IExecutionPolicy 的审批拦截机制。

### 前置条件

Slice 01 完成（三层链路已通）。

### 涉及 ADR

| ADR | 验证条款 |
|-----|---------|
| ADR-0031 一 | `IExecutionPolicy.requires_approval()` |
| ADR-0031 二 | `AgentModePolicy` 写入需审批 |
| ADR-0004 | `ToolCategory` 分类 + `ToolMetadata` 元数据 |
| ADR-0036 二.2.2 | `call_tool_with_policy()` 审批链 |

### 场景

```
输入 "修改 main.cpp，把函数名改为 new_name"
→ LLM 解析意图 → call code::edit_file
→ ToolRegistry.call("code::edit_file")
  → 内部检查 category == WriteFile
  → AgentModePolicy.requires_approval() → true
  → 生成 diff 预览
  → 等待用户输入 [y/n]
  → 用户确认 → 执行
  → 返回结果
```

### 文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/common/tools/tool_metadata.h` | 新建 | `ToolMetadata`、`ToolCategory` enum、`ApprovalPolicy` |
| `src/common/tools/registry.h` | 修改 | `register_tool()` 支持 `ToolMetadata` 参数 |
| `src/common/tools/registry.cpp` | 修改 | 新增带 metadata 的重载，保留旧 API |
| `src/common/policy/execution_policy.h` | 新建 | `IExecutionPolicy` 接口 |
| `src/common/policy/execution_policy.cpp` | 新建 | `AgentModePolicy` 实现 |
| `src/common/tools/tool_coordinator.h` | 新建 | `call_tool_with_policy()` 中间件 |
| `examples/slice_03_approval/main.cpp` | 新建 | 注册写入工具 → 触发审批 → 交互确认 |
| `examples/slice_03_approval/CMakeLists.txt` | 新建 | 链接 `agenticdsl_core` |

### 关键接口

```cpp
// ToolCategory 分类
enum class ToolCategory {
    ReadOnly,      // grep, ls — 无副作用，直接执行
    WriteFile,     // edit_file, create_file — 需审批
    Execute,       // exec_shell — 需审批
    Network,       // web_search — 需审批
    StateModify    // set_config — 需审批
};

// IExecutionPolicy
class IExecutionPolicy {
    virtual bool requires_approval(const ToolMetadata& meta,
                                   const ToolCallContext& ctx) const = 0;
};

// AgentModePolicy：写入操作需审批
class AgentModePolicy : public IExecutionPolicy {
    bool requires_approval(const ToolMetadata& meta,
                           const ToolCallContext& ctx) const override {
        return meta.category != ToolCategory::ReadOnly;
    }
};

// ToolCoordinator（基座层中间件）
ToolResult call_tool_with_policy(
    ToolRegistry& registry,
    const std::string& name,
    const nlohmann::json& params,
    const IExecutionPolicy* policy);
// 内部：检查 category → 如需审批 → 打印预览 → stdin 等待确认 → 执行
```

### 成功标准

```
终端执行：
  $ ./slice_03_approval

输出：
  [LLM] intent: call code::edit_file(path=main.cpp, old=foo, new=bar)
  [Approval] 写入工具 code::edit_file 需要确认：
  [Preview] -  void foo() {
  [Preview] +  void bar() {
  Apply? [y/n]: y
  ✓ code::edit_file → OK

  同样的流程，如果是只读工具：
  [LLM] intent: call code::read_file(path=main.cpp)
  [Tool] 只读工具，直接执行
  ✓ code::read_file → OK (320 bytes)
```

**失败信号**：写入工具未经审批直接执行、只读工具也触发审批。

---

## Slice 04：舰队模式并行

### 验证目标

16 路并行 LLM 调用，Taskflow 执行，结果聚合。验证 ADR-0025 舰队模式的并行加速假设。

### 前置条件

Slice 02 完成（多模型路由已通）。Taskflow 外部库引入。

### 涉及 ADR

| ADR | 验证条款 |
|-----|---------|
| ADR-0025 | 舰队模式拆分子任务 + 并行执行 + 结果聚合 |
| ADR-0030 | Taskflow 异步并行执行 |
| ADR-0034 | Flash 模型路由（舰队模式强制 Flash） |

### 场景

```
输入：16 个文件路径列表
流程：
  FleetOrchestrator.execute(subtasks, Concatenate)
    → 每个 subtask 调用 Flash 模型："Review file X"
    → Taskflow executor.async() × 16
    → wait_for_all()
    → 聚合 16 个结果
    → 打印汇总（时间统计、成本统计）
```

### 文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `external/taskflow/` | 引入 | Taskflow v4.0 header-only |
| `CMakeLists.txt`（根目录） | 修改 | `add_subdirectory(external/taskflow)` |
| `src/common/parallel/parallel_executor.h` | 新建 | `ParallelExecutor` 封装 |
| `src/common/parallel/parallel_executor.cpp` | 新建 | Taskflow `executor.async()` 封装 |
| `src/common/parallel/fleet_orchestrator.h` | 新建 | 子任务拆分 + 结果聚合 |
| `examples/slice_04_fleet/main.cpp` | 新建 | 16 路并行 → 时间/成本统计 |
| `examples/slice_04_fleet/CMakeLists.txt` | 新建 | 链接 `agenticdsl_core` + `Taskflow` |

### 关键接口

```cpp
// ParallelExecutor（基座层）
class ParallelExecutor {
    // 提交一批异步任务，返回 futures
    std::vector<std::future<FleetResult>>
    execute_batch(std::vector<FleetTask> tasks);

    // 等待全部完成
    void wait_for_all();
};

// FleetOrchestrator（认知层）
class FleetOrchestrator {
    // 子任务结构
    struct FleetTask {
        std::string prompt;           // 每个子任务的输入
        std::string file_path;        // 关联文件（用于错误报告）
    };

    struct FleetResult {
        bool success;
        std::string text;
        int64_t duration_ms;
    };

    // 拆分 + 执行 + 聚合
    std::vector<FleetResult> execute(
        const std::vector<FleetTask>& tasks,
        ModelRegistry& models,
        ParallelExecutor& executor);

    // 聚合策略
    struct Concatenate;      // 简单拼接
};

// 舰队模式路由（强制 Flash）
class FleetRouter : public IModelRouter {
    std::string route(const RoutingContext& ctx) override {
        return "deepseek-v4-flash";  // 舰队模式强制低成本模型
    }
};
```

### 成功标准

```
终端执行：
  $ ./slice_04_fleet

输出：
  [Fleet] 16 tasks submitted (model: flash)
  [Fleet] Task  1/16 done (245ms)  — review main.cpp
  [Fleet] Task  2/16 done (312ms)  — review utils.cpp
  ...
  [Fleet] Task 16/16 done (198ms)  — review config.cpp
  [Fleet] All complete: 16/16 succeeded, 0 failed
  [Fleet] Total time: 890ms (vs sequential ~4500ms)
  [Fleet] Total cost: $0.0032
```

**失败信号**：并行比串行慢、部分子任务失败、结果聚合丢失。

---

## 实施时间表

| 切片 | 工时 | 前置 | 产出物 |
|------|------|------|--------|
| Slice 01 | 3-5天 | 无 | 三层调用链可跑 |
| Slice 02 | 2-3天 | Slice 01 | 路由选择可验证 |
| Slice 03 | 2-3天 | Slice 01 | 审批交互可体验 |
| Slice 04 | 3-5天 | Slice 02 + Taskflow | 并行加速可观测 |
| **总计** | **2-3周** | — | 4 个可执行示例 |

---

## 与 ADR 的关系

```
Slice 01 → ADR-0036 (三层协议)
            ADR-0034 P1 (ModelRegistry)
            ADR-0004 (ToolRegistry)

Slice 02 → ADR-0034 (ModelRouter)
            ADR-0001 (ILLMProvider 扩展)

Slice 03 → ADR-0031 (IExecutionPolicy)
            ADR-0004 (ToolMetadata + 审批)

Slice 04 → ADR-0025 (舰队模式)
            ADR-0030 (Taskflow 并行)
```

每个切片通不过，对应的 ADR 设计就需要重新审视。

---

*文档版本: v1.0*
*最后更新: 2026-05-28*
