# Design — Cognitive Specialists as Tools

## Context

axis6-chain-workflow §六 G4 缺口: GEPALoop/MCTSWorkflowSearch/SkillCompiler 是纯 C++ class, DSL 无法 dsl_call。Oracle 评审 (session `ses_facbd3ffbffeUjlJgZsgMWFiM4`) 修正: **tool 注册路线优于 SKILL.md 化** — C++ class 经 ToolRegistry 注册即可被 tool_call 节点调用, 更简单 + 天然走 ADR-0004 审批矩阵。

本 change 将 3 个 cognitive specialist 包装为 `cognitive::*` tool, 与 Materializer (T1) 的 axis6 映射 (axis6=Reflect → `tool_call: cognitive::gepa_reflect`) 一一对应。

## 决策

### 决策 1 — 3 个 tool 的命名与包装

| tool 名 | 包装目标 | 实装位置 |
|---------|---------|---------|
| `cognitive::gepa_reflect` | `GEPALoop::reflect_and_commit(const ExecutionTrace&)` | `include/agenticdsl/cognitive/gepa_loop.h:48` |
| `cognitive::mcts_search` | `MCTSWorkflowSearch::search(const TaskSpec&)` | `include/agenticdsl/cognitive/mcts_workflow_search.h:161` |
| `cognitive::skill_compile` | SkillCompiler 编译入口 | `include/agenticdsl/cognitive/skill_compiler.h` |

**命名约定**: `cognitive::*` 命名空间 (与 ADR-0043 PDK 工具命名约定 `module.verb` 对齐: `cognitive` = module, `gepa_reflect` = verb)。

### 决策 2 — ToolMetadata V2 全套 (ADR-0004 安全矩阵)

每个 tool 的 ToolMetadata V2:

```cpp
// cognitive::gepa_reflect
ToolMetadata{
  .name = "cognitive::gepa_reflect",
  .description = "GEPA 反思: 失败轨迹 → prompt/skill 候选修订",
  .domain = "cognitive",
  .category = ToolCategory::Execute,          // 认知任务执行
  .min_layer = LayerProfile::Thinking,        // 最低 Thinking 层
  .approval = ApprovalPolicy{
    .requires_approval_in_plan = true,        // Plan 模式需审批
    .requires_approval_in_agent = true,       // Agent 模式需审批
    .requires_approval_in_yolo = false,       // YOLO 放行
    .force_approval_always = false,
  },
  .allowed_layers = {LayerProfile::Workflow, LayerProfile::Thinking},
  .cost_estimate = 0.05,                      // USD (GEPA 反思 LLM 调用估计)
  .timeout_ms = 30000,                        // 30s
  .input_schema = {/* ExecutionTrace JSON Schema */},
  .output_schema = {/* ReflectionResult JSON Schema */},
};

// cognitive::mcts_search — cost_estimate=0.10, timeout_ms=60000 (嵌套搜索更贵)
// cognitive::skill_compile — cost_estimate=0.02, timeout_ms=10000 (编译较快)
```

**审批策略理由**: cognitive 任务是**高成本高影响**操作 (修改 prompt/skill/workflow), plan+agent 模式强制审批; yolo 模式放行 (开发者自我负责)。与 ADR-0004 §8 Layer×Category 矩阵对齐 (Execute × Thinking/Workflow 允许)。

### 决策 3 — 注册函数 (IToolRegistry 接口)

```cpp
// src/modules/cognitive/cognitive_tools.h
namespace agenticdsl {

// 注册 3 个 cognitive specialists 为 tool
// 调用方负责: 构造 GEPALoop/MCTSWorkflowSearch/SkillCompiler 实例并传入
void register_cognitive_tools(
    IToolRegistry& registry,
    std::shared_ptr<GEPALoop> gepa_loop,              // 可为 nullptr (未配置时注册为不可用 tool)
    std::shared_ptr<MCTSWorkflowSearch> mcts_search,  // 可为 nullptr
    std::shared_ptr<SkillCompiler> skill_compiler,    // 可为 nullptr
    std::shared_ptr<IInteractionBus> bus = nullptr);  // 可选, 事件发射

} // namespace agenticdsl
```

**nullptr 处理**: specialist 为 nullptr 时, tool 仍注册但 handler 返回 `ToolResult::error(ErrorCode::Unavailable, "specialist not configured")` (fail-closed, 不变量 3)。

### 决策 4 — Handler 实现 (lambda 包装)

```cpp
// cognitive::gepa_reflect handler
registry.register_tool_function("cognitive::gepa_reflect",
  [gepa_loop, bus](const nlohmann::json& args) -> nlohmann::json {
    if (!gepa_loop) return tool_error("specialist not configured");
    // 1. 从 args 构造 ExecutionTrace (字段: trace_id, final_result, ...)
    ExecutionTrace trace = parse_execution_trace(args);
    // 2. 调用 GEPALoop
    auto result = gepa_loop->reflect_and_commit(trace);
    // 3. 返回 ToolResult JSON (ok + data + meta)
    return tool_success({{"success", result.success},
                         {"failure_mode", result.failure_mode},
                         {"candidate_skills", result.candidate_skills}});
  });
```

### 决策 5 — 与 Materializer (T1) 的对齐 (不变量 5)

| axis6 值 | Materializer 生成的 DSL 节点 | 本 change 注册的 tool |
|----------|----------------------------|---------------------|
| Reflect | `type: tool_call, tool: "cognitive::gepa_reflect"` | `cognitive::gepa_reflect` ✅ |
| Search | `type: tool_call, tool: "cognitive::mcts_search"` | `cognitive::mcts_search` ✅ |
| Compile | `type: tool_call, tool: "cognitive::skill_compile"` | `cognitive::skill_compile` ✅ |

名称一一对应, 无漂移。

### 决策 6 — 事件发射 (可选, bus 非空时)

- `cognitive.specialist.invoked` — tool 调用开始 (payload: tool_name, trace_id)
- `cognitive.specialist.completed` — tool 调用完成 (payload: tool_name, ok, duration_ms)

注册 ADR-0068 Appendix A (v1.8+, 与 axis6/workflow.materialized 同 amendment 或紧随)。

## 接口

### 新增文件

- `src/modules/cognitive/cognitive_tools.h` (新建)
- `src/modules/cognitive/cognitive_tools.cpp` (新建)
- `tests/test_cognitive_specialists_as_tools.cpp` (新建, ≥6 cases)

### 零修改

- `include/agenticdsl/contract/` (不变量 4)
- `include/agenticdsl/cognitive/gepa_loop.h` / `mcts_workflow_search.h` / `skill_compiler.h` (不变量 1)

## 反例 (明确不做)

| 反例 | 拒绝理由 |
|------|----------|
| 写 SKILL.md (`/lib/cognitive/*.agent.md`) | Oracle: tool 路线更简单, SKILL.md 化是 V2 可组合性需求 |
| 修改 GEPALoop/MCTSWorkflowSearch/SkillCompiler 实装 | 不变量 1: 仅 lambda 包装 |
| 新增 `ICognitiveSpecialist` contract 接口 | 不变量 4 + 与 §18.8 不变量 "不新增 contract 类" 一致 |
| specialist 为 nullptr 时不注册 tool | 不变量 3: fail-closed, 注册但返回 error (可发现性) |
| 审批策略 yolo 也需审批 | 与 ADR-0004 矩阵对齐, yolo 是开发者自我负责模式 |

## 跨 change 依赖

### 前置 (已 ship)
- ✅ GEPALoop (T19)
- ✅ MCTSWorkflowSearch (T20)
- ✅ SkillCompiler (T17)
- ✅ IToolRegistry + ToolMetadata V2 (ADR-0004 V2)
- ✅ ToolCoordinator (ADR-0031 §决策 5)

### 后续
- T1 `workflow-materializer-v1` — Materializer 产出的 axis6 节点引用本 change 的 `cognitive::*` tool
- T6 `chain-evolution-driver-v1` — 依赖 T1+T2+T5

### 并行
- T3 `evolution-budget-cap` (独立)
- T4 `signature-validation-real-impl` (独立)

## ADR 兼容性

| ADR | 兼容性 |
|-----|--------|
| ADR-0004 V2 (ToolMetadata) | ✅ 全套 V2 字段 (category/approval/allowed_layers/cost/timeout) |
| ADR-0031 §决策 5 (ToolCoordinator) | ✅ tool 经 ToolCoordinator 治理路径 |
| ADR-0043 (命名约定) | ✅ `cognitive.verb` 命名 |
| ADR-0023 (ToolResult) | ✅ 标准化信封 |
| ADR-0020 (线程隔离) | ✅ tool 注册在调用方线程, 不进 CognitiveWorker 队列 |
| ADR-0085 §决策 5 | ✅ 无 MetaAgent, 纯 tool 包装 |
| ADR-0068 | 🟡 需注册 cognitive.specialist.* 主题 |
