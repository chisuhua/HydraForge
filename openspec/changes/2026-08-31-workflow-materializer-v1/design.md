# Design — WorkflowMaterializer V1

## Context

MCTSWorkflowSearch (T20, 2026-08-28 ship) 产出 WorkflowGraph 结构模板, 但零消费者 + 不可执行。Oracle 评审 (session `ses_facbd3ffbffeUjlJgZsgMWFiM4`) 确认 G1 Materialize 缺口真实, 并修正方案: **输出 DSL 文本而非直接构造 ParsedGraph**, 复用 `MarkdownParser` 单一事实源 + `DSLEngine::continue_with_generated_dsl()` 现有路径 (engine.cpp:390, 已被 4 处调用)。

## 决策

### 决策 1 — DSL 文本输出 (非 ParsedGraph 直接构造)

**理由** (Oracle 评审):
- `MarkdownParser::parse_from_string()` 是 DSL→ParsedGraph 的**单一事实源** (signature/权限/节点解析/校验全在此)
- 直接构造 ParsedGraph 会制造第二套构建逻辑, 与 parser 分裂
- `continue_with_generated_dsl(dsl_text)` 已是成熟的 append 路径 (engine.cpp:390, plan_execute_loop.h:241 在用)

**接口**:
```cpp
namespace agenticdsl {

class WorkflowMaterializer {
 public:
  struct MaterializeConfig {
    std::string output_path_prefix = "/dynamic/mcts/";
    std::string signature_validation = "strict";   // 与 GenerateSubGraph 同机制
    bool emit_lineage_event = true;
  };

  // WorkflowGraph (图宇宙 A) → DSL 文本 (Markdown AgenticDSL 格式)
  // 返回 nullopt + failure_reason 于空图/无效 axis 组合
  static std::optional<std::string> materialize_to_dsl(
      const WorkflowGraph& wf_graph,
      const TaskSpec& spec,
      const MaterializeConfig& config,
      std::string* failure_reason = nullptr);
};

} // namespace agenticdsl
```

### 决策 2 — axis→DSL 节点映射表 (与 axis6-chain-workflow §4.1 单一事实源)

**结构轴 (axis1 Template + axis4 Control)** → DAG 结构:
| axis1 | axis4 | → DSL 结构 |
|-------|-------|-----------|
| Linear | Sequential | 顺序 `next: ["/next"]` |
| Branching | * | `type: fork, branches: [...]` |
| Loop | Loop | 循环边 + `max_iterations` |
| Parallel | Parallel | `type: fork ... → type: join` |

**参数轴 (axis2 Param)** → 节点级 `llm_params` (挂到最近 LLM 节点):
| axis2 | → DSL 字段 |
|-------|-----------|
| Temperature | `llm_params: {temperature: X}` |
| MaxTokens | `llm_params: {max_tokens: N}` |
| TopP | `llm_params: {top_p: X}` |

**工具轴 (axis3 Tool)** → `tool_call` 节点:
| axis3 | → DSL 节点 |
|-------|-----------|
| Calculator | `type: tool_call, tool: "math::calculate"` |
| Search | `type: tool_call, tool: "search::query"` |
| Custom | `type: tool_call, tool: "{{task_spec.custom_tool}}"` |
| None | 不生成工具节点 |

**错误轴 (axis5 Error)** → 错误处理边:
| axis5 | → DSL 结构 |
|-------|-----------|
| Retry | `on_failure: "/retry" + retry: {max: 3}` |
| Fallback | `on_failure: "/fallback"` |
| Abort | `on_failure: "/end" + termination_mode: hard` |

**认知域轴 (axis6 CognitiveDomain, v1.1)** → `dsl_call` 或 `tool_call` 节点:
| axis6 | → DSL 节点 (V1 走 tool_call 路线, Oracle T2) |
|-------|-----------|
| Reflect | `type: tool_call, tool: "cognitive::gepa_reflect"` |
| Search | `type: tool_call, tool: "cognitive::mcts_search"` (嵌套预算受 axis6 amendment 决策 5 约束) |
| Compile | `type: tool_call, tool: "cognitive::skill_compile"` |
| Reason | Phase 1 预留 (IPER 未实装) |
| Meta_Select | V2 预留 |
| None | 不生成 cognitive 节点 |

**V1 关键选择 (Oracle T2)**: axis6 specialist 节点走 **`tool_call`** 而非 `dsl_call` — specialist 以 tool 注册 (T2 change `cognitive-specialists-as-tools`), 无需先写 SKILL.md。

### 决策 3 — DSL 文本格式 (Markdown AgenticDSL)

输出为完整可解析的 `.md` 文本:
```markdown
### AgenticDSL `/dynamic/mcts/<task_id>`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
signature:
  inputs: [{name: task_id, type: string}]
  outputs: [{name: result, type: object}]
  version: "1.0"
  stability: experimental
nodes:
  - id: start
    type: start
    next: ["/dynamic/mcts/<task_id>/n1"]
  - id: n1
    type: tool_call
    tool: cognitive::gepa_reflect
    ...
  - id: end
    type: end
    termination_mode: hard
# --- END AgenticDSL ---
```
```

**生成后路径**: `DSLEngine::continue_with_generated_dsl(dsl_text)` → `MarkdownParser::parse_from_string` → `append_graphs` → 可调度。

### 决策 4 — 空图/无效 axis 兜底

| 输入 | 输出 |
|------|------|
| `wf_graph.nodes.empty()` | `nullopt` + "empty_workflow_graph" |
| axis6∉{None,Reflect,Search,Compile} (Reason/Meta_Select) | `nullopt` + "axis6_not_implemented_in_v1" |
| 未覆盖 axis 组合 | `nullopt` + "unsupported_axis_combination" |

不抛异常 (fail-safe, 与 ADR-0085 HookErrorPolicy 一致)。

### 决策 5 — lineage 事件

`workflow.materialized` 事件 (IInteractionBus, 注册 ADR-0068 Appendix A):
- payload: `{workflow_hash, output_path, node_count, edge_count, axis6_specialists: [...], spec_task_id}`
- 用于: chain 血缘追踪 + 审计 + 后续归因 (ADR-0086 evidence_refs)

### 决策 6 — 与 CognitiveWorker 的关系 (Oracle 修正)

Materializer **不放 CognitiveWorker 队列** (ADR-0020 单线程单队列, 长任务饿死风险)。Materializer 是纯函数式静态方法, 由调用方 (MCTS driver / evolution driver) 同步调用。

## 接口

### 新增文件

- `include/agenticdsl/cognitive/workflow_materializer.h` (新建)
- `src/modules/cognitive/workflow_materializer.cpp` (新建)
- `tests/test_workflow_materializer.cpp` (新建, ≥6 cases)
- `examples/mcts_materialize_demo/main.cpp` (新建, E2E demo)

### 零修改

- `include/agenticdsl/contract/` (不变量 4)
- `MarkdownParser` (复用, 不修改)
- `MCTSWorkflowSearch` (复用, 不修改)

## 反例 (明确不做)

| 反例 | 拒绝理由 |
|------|----------|
| 直接构造 ParsedGraph (不输出 DSL 文本) | Oracle: 第二套构建逻辑, 与 MarkdownParser 分裂 |
| Materializer 放进 CognitiveWorker 队列 | Oracle: ADR-0020 单线程队列, 长任务饿死 |
| LLM 调用 (用 LLM 把 WorkflowGraph 翻译成 DSL) | 不变量 2: 纯模板渲染, 确定性, 无需 LLM |
| 模式 3 双轨仲裁 (Axis6 vs GenerateSubGraph 竞争) | Oracle: 早产优化, V1 默认结构化路径 |
| 修改 MarkdownParser 以支持新节点类型 | 不变量 1: 复用单一事实源, 不修改 parser |

## 跨 change 依赖

### 前置 (已 ship)
- ✅ T20 MCTSWorkflowSearch (mcts_workflow_search.h)
- ✅ DSLEngine::continue_with_generated_dsl (engine.cpp:390)
- ✅ MarkdownParser::parse_from_string
- ✅ IInteractionBus (workflow.materialized 事件通道)

### 后续 (不在本 change)
- T2 `cognitive-specialists-as-tools` — axis6=Reflect/Search/Compile 节点的 tool 实装 (本 change 的 DSL 节点引用 `cognitive::*` tool, T2 提供实装)
- T5 `evolution-readiness-gate-v1` — 进化触发门禁
- T6 `chain-evolution-driver-v1` — 串联器 (依赖 T1+T2+T5)

### 并行 (本 change 可并行)
- T3 `evolution-budget-cap` — 独立
- T4 `signature-validation-real-impl` — 独立 (node_executor.cpp:309 占位符)

## ADR 兼容性

| ADR | 兼容性 |
|-----|--------|
| ADR-0061-08 v1.0/v1.1 | ✅ Materializer 消费 MCTS WorkflowGraph, 不修改 MCTS |
| ADR-0020 | ✅ 纯函数静态方法, 不进 CognitiveWorker 队列 |
| ADR-0085 §决策 5 | ✅ 无状态工具类, 非 MetaAgent |
| ADR-0068 | 🟡 需 Appendix A 注册 `workflow.materialized` (随 ship 同步) |
| ADR-0073 | 🟡 signature 生成参考 (Partial) |
| dsl.md v3.10 | ✅ DSL 文本格式对齐规范 |
