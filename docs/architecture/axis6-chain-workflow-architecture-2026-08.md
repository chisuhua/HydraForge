# Axis6 Chain 完整工作流程架构 (Axis6 Cognitive Domain Chain Architecture)

**生成日期**: 2026-08-31
**最后验证**: 2026-08-31（v1.1 — Oracle 评审修正 (session `ses_facbd3ffbffeUjlJgZsgMWFiM4`): 4 项设计修正 + 5 项新缺口 N1-N5 + G6 自相矛盾修复 + P0 降级 + G2 合并）
**作者**: Architecture Working Group（Sisyphus 综述，整合用户提议 + Metis/Oracle 评审 + 代码审计）
**状态**: 🔍 Proposed（v1.1，指导性文档——Axis6 chain 的端到端工作流程定义与缺口清单，非 ADR 本身）

> **文档定位**: 回答"Axis6 cognitive_domain chain 从搜索到执行到进化的完整工作流程是什么"。
> 它是 `agent-orchestration-architecture-2026-08.md` v1.5 §十四 M5 + §18.10.1（触发条件）+ `adr-0061-08-v1-1-amendment-axis6.md`（搜索维度）+ `adr-0086-credit-assignment-contract.md`（归因判定）的**端到端整合视图**。
> **不重复**: 各组件实现细节（引用为准）、ADR 决策正文（引用为准）。

---

## 一、核心发现: 两个平行的图宇宙 (The Two Graph Universes)

**这是理解 Axis6 chain 与 GenerateSubGraph 关系的第一性原理** — 代码库中存在**两种互不相通的图表示**:

```
┌─────────────────────────────────────────────────────────────────────┐
│  图宇宙 A: WorkflowGraph (MCTS 搜索世界)                              │
│  ─────────────────────────────────────                              │
│  来源: MCTSWorkflowSearch (cognitive/mcts_workflow_search.h)         │
│  结构: vector<WorkflowNode> + vector<WorkflowEdge>                   │
│  节点属性: 5 轴模板 (Axis1-5) + axis6 (v1.1 新增)                    │
│  本质: 结构模板 (structural template) — "这个 workflow 长什么样"      │
│  执行: ❌ 不可执行 — synthesize_results() 是 mock (mcts_workflow_search.cpp:124)
│  消费者: ❌ 零 — grep 全库无 MCTSWorkflowSearch 输出消费方             │
│  评分: IEvaluator::evaluate(WorkflowGraph) — 模拟层评分               │
├─────────────────────────────────────────────────────────────────────┤
│  图宇宙 B: ParsedGraph (DSL 执行世界)                                 │
│  ─────────────────────────────────────                              │
│  来源: MarkdownParser::parse() / GenerateSubgraphNode                │
│  结构: 真实节点树 (Start/End/Assign/ToolCall/GenerateSubgraph/...)    │
│  节点属性: type/prompt_template/output_keys/signature/next/...       │
│  本质: 可执行图 (executable graph) — "这个 workflow 怎么跑"           │
│  执行: ✅ TopoScheduler::execute() — 真实执行                          │
│  序列化: TrajectoryIR::from_parsed_graph() (T15, 单向快照)            │
│  生成: GenerateSubgraphNode (LLM 生成 DSL 文本 → parse)              │
├─────────────────────────────────────────────────────────────────────┤
│  🔴 桥: 不存在                                                        │
│  grep -rn "to_parsed_graph|to_dsl|materialize|WorkflowGraph.*parse"   │
│  include/ src/ → 0 命中                                              │
└─────────────────────────────────────────────────────────────────────┘
```

**含义**: Axis6 chain 搜索出的"最优 cognitive_domain chain"当前**只能存在于图宇宙 A，无法进入图宇宙 B 被执行**。GenerateSubGraph 在图宇宙 B 中通过 LLM 自由生成子图，与图宇宙 A 的结构化搜索**不产生同一种图**。这是 G1 缺口（§五）。

---

## 二、Axis6 Chain 完整工作流程 (目标态)

```
┌──────────────────────────────────────────────────────────────────────────┐
│  阶段 0: 触发判断 — "当前 cognitive agent 可以进化吗?"                    │
│  ──────────────────────────────────────────────────                      │
│  EvolutionReadinessGate (待设计, 当前零基础设施, G3)                       │
│    ├─ 输入: evaluation.result 事件流 (ADR-0083)                          │
│    ├─ 输入: attribution.recorded 事件流 (ADR-0086)                       │
│    ├─ 输入: mutation.* 事件流 (ADR-0084)                                 │
│    ├─ 输入: ExecutionBudget 状态 (ADR-0019 §6)                           │
│    └─ 输出: verdict ∈ {Ready, NotReady(reason), Blocked(gate)}          │
│                                                                          │
│  判定矩阵 (§三详细):                                                      │
│    Ready    = 归因 Attributed + 回归门 PASS + 预算充足 + 无未控制混杂      │
│    NotReady = 任一条件不满足 (含具体 reason)                              │
│    Blocked  = 硬门禁触发 (CaptureMode≠Training / MutationGovernor deny)   │
└─────────────────────────────────┬────────────────────────────────────────┘
                                  ▼ Ready
┌──────────────────────────────────────────────────────────────────────────┐
│  阶段 1: 搜索 — MCTSWorkflowSearch + Axis6 (图宇宙 A)                     │
│  ──────────────────────────────────────────────────                      │
│  MCTSWorkflowSearch(config, chain_config).search(TaskSpec)               │
│    ├─ UCB1 树选择 (per-child q + C·sqrt(ln N/n), v1.0 实装)              │
│    ├─ 子节点扩展: 5 轴模板采样 + axis6 采样 (available_specialists)       │
│    ├─ chain 深度截断: 连续 axis6≠None 节点 ≤ max_chain_depth (3)         │
│    ├─ 嵌套预算: axis6=Search 嵌套迭代 ≤ max_nested_search_iterations (30)│
│    ├─ 改进停机: best_reward 绝对改进 < 0.05 即停                         │
│    ├─ commit/revert: governor authorize → axis6.commit / axis6.revert   │
│    └─ 输出: SearchResult{best_workflow: WorkflowGraph, best_reward}      │
│                                                                          │
│  ⚠️ 此时 best_workflow 仍是图宇宙 A 的结构模板, 不可执行                  │
└─────────────────────────────────┬────────────────────────────────────────┘
                                  ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  阶段 2: 具体化 — WorkflowGraph → ParsedGraph (跨宇宙桥, G1)              │
│  ──────────────────────────────────────────────────                      │
│  WorkflowMaterializer (待设计, 当前不存在, G1)                            │
│    ├─ 输入: WorkflowGraph (图宇宙 A, 含 axis6 chain)                     │
│    ├─ 转换规则 (§四详细):                                                │
│    │   axis6=Reflect  → DSL 节点调用 GEPALoop.reflect_and_commit         │
│    │   axis6=Search   → DSL 节点调用 MCTSWorkflowSearch (嵌套)           │
│    │   axis6=Compile  → DSL 节点调用 SkillCompiler                        │
│    │   axis6=Reason   → DSL 节点调用 IPER (Phase 1 预留)                 │
│    │   axis1/axis4     → DAG 结构 (Linear/Branching/Loop/Parallel)       │
│    │   axis2           → LLM 参数 (temperature/max_tokens/top_p)         │
│    │   axis3           → ToolCall 节点 (Calculator/Search/Custom)        │
│    │   axis5           → 错误处理节点 (Retry/Fallback/Abort)             │
│    ├─ 输出: ParsedGraph (图宇宙 B, 可执行) + signature                   │
│    └─ 验证: signature_validation (strict/warn/ignore, GenerateSubGraph 同机制)│
└─────────────────────────────────┬────────────────────────────────────────┘
                                  ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  阶段 3: 注册 — ParsedGraph → TopoScheduler (图宇宙 B 执行入口)           │
│  ──────────────────────────────────────────────────                      │
│  路径 A (静态注册): DSLEngine::append_graphs({parsed_graph})             │
│    → full_graphs_.push_back → 下次 execute() 可被调度                    │
│                                                                          │
│  路径 B (动态注册, GenerateSubGraph 断链修复后):                          │
│    TopoScheduler::append_dynamic_graphs({parsed_graph})                  │
│    → dynamic_graphs_ → 主循环 rebuild_dynamic_graph() → 立即可调度       │
│    ⚠️ 当前断链: node_executor.cpp:327 append_graphs 注释掉 (P0, §十一)    │
│                                                                          │
│  路径 C (chain 级注册, 待设计, G2):                                      │
│    CognitiveChainRegistry (不存在)                                        │
│    → 注册 chain 为命名实体 (e.g. "/chains/reflect_search_compile@v1")     │
│    → CognitiveWorker 可通过 IAgentComposition.delegate 调用               │
└─────────────────────────────────┬────────────────────────────────────────┘
                                  ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  阶段 4: 执行 — TopoScheduler 调度 ParsedGraph (图宇宙 B)                 │
│  ──────────────────────────────────────────────────                      │
│  TopoScheduler::execute(context)                                         │
│    ├─ build_dag() → DAG 构建                                            │
│    ├─ 主循环: 按依赖顺序调度节点                                          │
│    ├─ chain 内节点执行:                                                  │
│    │   Reflect 节点 → 调用 GEPALoop.reflect_and_commit(failed_trace)     │
│    │   Search 节点   → 调用 MCTSWorkflowSearch (嵌套, 预算 ≤30)          │
│    │   Compile 节点  → 调用 SkillCompiler                                │
│    │   GenerateSubgraph 节点 → LLM 生成新子图 (若 chain 含此类节点)        │
│    ├─ 横切拦截: L1 ToolHook + L2 AgentHook + L4 Approval 全程生效         │
│    ├─ 事件发射: cognitive.task.* / domain.task.* / evaluation.result      │
│    └─ 输出: ExecutionResult + ExecutionTrace                             │
└─────────────────────────────────┬────────────────────────────────────────┘
                                  ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  阶段 5: 归因 — 执行结果归因判定 (ADR-0086)                               │
│  ──────────────────────────────────────────────────                      │
│  attribute_version_pair(child_version, parent_version, confounders)      │
│    ├─ child  = 新 chain 执行结果 (eval score + evidence)                 │
│    ├─ parent = 旧 chain (或旧版本) 基线 (Session 4-Scope 固定)           │
│    ├─ confounders: TaskDifficulty / Environment / EvaluatorDrift / ...   │
│    ├─ 判定:                                                              │
│    │   Attributed   → eval_delta > 2×parent_stddev 且混杂全控制          │
│    │   Confounded   → 存在未控制混杂 → 只能相关性观察                     │
│    │   Insufficient → 证据不足/噪声带内 → 只能相关性观察                  │
│    │   NotAttempted → 默认 fail-closed                                   │
│    └─ emit: attribution.recorded / attribution.confounded                │
└─────────────────────────────────┬────────────────────────────────────────┘
                                  ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  阶段 6: 提交/回滚 — MutationGovernor 治理 (ADR-0084)                     │
│  ──────────────────────────────────────────────────                      │
│  IF attribution.verdict == Attributed:                                   │
│    governor.propose(new_chain) → 白名单 + L4 + 模式×等级矩阵              │
│      → IEvaluator 评估 → 行为回归门 (T14) → emit mutation.proposed        │
│    governor.commit() → evaluation_refs 非空 + attribution==Attributed     │
│      → emit mutation.committed → 新 chain 生效                           │
│  ELSE:                                                                   │
│    emit mutation.denied (denial_reason = attribution verdict)            │
│    → 新 chain 不生效, 旧 chain 继续服务                                   │
│                                                                          │
│  回滚路径: governor.revert() → emit mutation.reverted → 恢复 parent chain │
└─────────────────────────────────┬────────────────────────────────────────┘
                                  ▼
                    回到 阶段 0 (下一轮进化判断)
```

---

## 三、进化触发判断: "当前 cognitive agent 可以进化吗?"

**当前状态**: `grep -rn "should_evolve|evolution_trigger|ready_to_evolve" src/ include/` = **0 命中**。没有任何组件回答这个问题。GEPA 是失败驱动（被动触发），MCTS 是显式调用（手动触发）。**这是 G3 缺口**。

### 3.1 EvolutionReadinessGate (提议设计)

一个**纯函数式判定器**（无状态，无 LLM，确定性），消费 4 个已 ship 事件流：

```cpp
namespace agenticdsl {

enum class EvolutionReadiness {
  Ready,                    // 可以进化
  NotReady_Attribution,     // 归因不成立 (Confounded/Insufficient/NotAttempted, 含混杂控制)
  NotReady_Regression,      // 行为回归门未通过
  NotReady_Budget,          // 进化预算不足 (N1 evolution-budget-cap)
};

struct EvolutionReadinessReport {
  EvolutionReadiness verdict;
  std::string reason;                    // 人类可读原因
  AttributionVerdict attribution;        // ADR-0086 归因判定 (内含混杂控制)
  bool regression_passed;                // T14 回归门
  bool evolution_budget_ok;              // N1 进化周期预算检查
};

// 纯函数: 消费事件流快照, 输出判定 (无 LLM, 无 specialist 执行)
EvolutionReadinessReport check_evolution_readiness(
    const std::string& agent_id,
    const EventLogSnapshot& events,       // ADR-0080 事件日志快照
    const ExecutionBudget& budget);       // ADR-0019 §6

} // namespace agenticdsl
```

### 3.2 判定矩阵 (Oracle 修正: 7→3 条件)

> **Oracle 修正 (session `ses_facbd3ffbffeUjlJgZsgMWFiM4`)**: 原 7 条件过度设计。精简理由:
> - **C4 (混杂控制) ⊂ C1 (归因判定)**: ADR-0086 的 `Confounded` 判定已内含混杂控制检查, gate 重复检查是冗余
> - **C6 (governor 预检) 是双重门禁**: governor.propose() 是 commit 时的本职, gate 里预检与其重复 — 门禁应在治理层 (commit) 而非判定层 (gate)
> - **C5 (CaptureMode) 属蒸馏正交面**: 蒸馏场景才有 CaptureMode 要求, 不应混入通用进化门
> - **C7 (冷却期) 是配置 guard 非判定条件**: 冷却逻辑可由调用方节流, 非 gate 语义
>
> **V1 精简判定矩阵 (3 条件 AND)**:

| # | 条件 | 数据来源 (已 ship) | Ready 判据 |
|---|------|-------------------|-----------|
| C1 | 归因判定 (含混杂控制) | ADR-0086 `attribute_version_pair()` | `verdict == Attributed` |
| C2 | 行为回归门 | T14 BehavioralRegressionGate | `gate.pass(candidate, baseline) == true` |
| C3 | 预算预留 | ExecutionBudget + **N1 进化周期预算** (evolution-budget-cap) | `!evolution_budget_exceeded()` 且预留 ≥ 1 次进化迭代成本 |

**3 条件全部满足 → Ready; 任一不满足 → NotReady(具体 reason)**

> **V2 扩展** (不在 V1): 冷却期节流 (配置 guard) / CaptureMode 蒸馏场景检查 / 连续监控触发。

### 3.3 进化触发的 3 种模式 (与现有机制对齐)

| 模式 | 触发源 | 当前状态 | 与 EvolutionReadinessGate 关系 |
|------|--------|---------|-------------------------------|
| **失败驱动** | GEPALoop.reflect_and_commit (失败轨迹) | ✅ ship (T19) | 失败触发 → 先过 ReadinessGate → 再 reflect |
| **搜索驱动** | MCTSWorkflowSearch.search() 显式调用 | ✅ ship (T20) | 调用前 → 先过 ReadinessGate → 再搜索 |
| **连续监控** | (待设计) 周期性检查 evaluation.result 事件流 | ❌ 零基础设施 (G3) | ReadinessGate 周期性运行, Ready → 触发搜索 |

---

## 四、WorkflowGraph → ParsedGraph 具体化 (跨宇宙桥)

**这是 Axis6 chain 从"搜索结果"变成"可执行实体"的唯一路径，当前完全不存在（G1 缺口）**。

> **Oracle 修正 (session `ses_facbd3ffbffeUjlJgZsgMWFiM4`) — Materializer 输出 DSL 文本而非直接构造 ParsedGraph**:
> - `DSLEngine::continue_with_generated_dsl()` (engine.cpp:390) **已存在且在用** (plan_execute_loop.h:241 / agent_loop 示例 / test_engine.cpp:44 — 4 处调用)
> - `MarkdownParser::parse_from_string()` 是 DSL→ParsedGraph 的**单一事实源** (signature/权限/节点解析全在此)
> - 直接构造 ParsedGraph 会制造**第二套构建逻辑**, 与 parser 分裂
> - **正确方案**: Materializer 输出 **DSL 文本** (Markdown AgenticDSL 格式) → `continue_with_generated_dsl(dsl_text)` → 复用现有 parser+append 路径

### 4.1 转换规则 (axis → DSL 节点映射)

| Axis 值 | → DSL 节点类型 | 说明 |
|---------|---------------|------|
| **axis6=Reflect** | `type: tool_call, tool: "cognitive::gepa_reflect"` | 调用 GEPALoop — **V1 走 tool 注册路线** (Oracle T2 修正: 比 SKILL.md 化更简单, 天然走 ADR-0004 审批矩阵; 见 §六 G4) |
| **axis6=Search** | `type: tool_call, tool: "cognitive::mcts_search"` | 嵌套 MCTS, 预算 `max_nested_search_iterations=30` |
| **axis6=Compile** | `type: tool_call, tool: "cognitive::skill_compile"` | 调用 SkillCompiler (T17 已 ship) |
| **axis6=Reason** | Phase 1 预留 | IPER (未实装) — V1 返回 nullopt + "axis6_not_implemented_in_v1" |
| **axis1=Linear** | 顺序边 `next: ["/next_node"]` | DAG 线性结构 |
| **axis1=Branching** | `type: fork, branches: [...]` | 分支结构 |
| **axis1=Loop** | 循环边 `next: ["/loop_start"]` | 循环结构 (受 max_iterations 限制) |
| **axis1=Parallel** | `type: fork, branches: [...] → type: join` | 并行结构 (ForkJoinLoop) |
| **axis2=Temperature** | `llm_params: {temperature: X}` | LLM 采样参数 |
| **axis2=MaxTokens** | `llm_params: {max_tokens: N}` | LLM 输出上限 |
| **axis2=TopP** | `llm_params: {top_p: X}` | LLM nucleus 采样 |
| **axis3=Calculator** | `type: tool_call, tool: "math::calculate"` | 计算工具 |
| **axis3=Search** | `type: tool_call, tool: "search::query"` | 搜索工具 |
| **axis3=Custom** | `type: tool_call, tool: "{{custom_tool}}"` | 自定义工具 (TaskSpec 注入) |
| **axis4=Sequential** | 顺序调度 (默认) | 控制流 |
| **axis4=Parallel** | `type: fork` | 并行控制流 |
| **axis4=Loop** | 循环控制 | 循环控制流 |
| **axis5=Retry** | `on_failure: "/retry_node"` + `retry: {max: 3}` | 错误重试 |
| **axis5=Fallback** | `on_failure: "/fallback_node"` | 降级路径 |
| **axis5=Abort** | `on_failure: "/end"` + `termination_mode: hard` | 失败终止 |

### 4.2 转换器接口 (Oracle 修正: 输出 DSL 文本)

```cpp
namespace agenticdsl {

class WorkflowMaterializer {
 public:
  // WorkflowGraph (图宇宙 A) → DSL 文本 (Markdown AgenticDSL 格式)
  // 返回的 DSL 文本经 DSLEngine::continue_with_generated_dsl() 进入图宇宙 B
  // 失败时返回 nullopt + failure_reason (fail-safe, 不抛异常)
  static std::optional<std::string> materialize_to_dsl(
      const WorkflowGraph& wf_graph,           // MCTS 输出
      const TaskSpec& spec,                    // 原始任务描述
      const MaterializeConfig& config,
      std::string* failure_reason = nullptr);

  struct MaterializeConfig {
    std::string output_path_prefix = "/dynamic/mcts/";  // GenerateSubGraph 同命名空间
    std::string signature_validation = "strict";         // 与 GenerateSubGraph 同机制
    bool emit_lineage_event = true;                      // emit workflow.materialized 事件
  };
};

} // namespace agenticdsl
```

**调用路径**: `materialize_to_dsl()` → `DSLEngine::continue_with_generated_dsl(dsl_text)` → `MarkdownParser::parse_from_string()` → `append_graphs()` → 可调度。

### 4.3 signature 生成 (与 GenerateSubGraph 对齐)

materialize 后的 DSL 文本必须携带 signature（Markdown yaml 块内 `signature:` 字段），由 MarkdownParser 解析时校验（与 GenerateSubGraph 的 signature_validation strict/warn/ignore 同机制），确保：
- 输入/输出 schema 明确
- `strict` 模式下 signature 违规 → 解析失败 → 不注册
- 与 ADR-0073 Tool JSON Schema 契约兼容

---

## 五、GenerateSubGraph 与 Axis6 的关系

**这是用户最关心的问题，也是最容易混淆的地方**。两者是**互补而非竞争**的关系，但当前**完全没有集成**。

### 5.1 本质差异

| 维度 | GenerateSubGraph | Axis6 / MCTS |
|------|-----------------|--------------|
| **图宇宙** | B (ParsedGraph, 可执行) | A (WorkflowGraph, 结构模板) |
| **生成方式** | LLM 自由生成 DSL 文本 | UCB1 树搜索 (模板采样) |
| **搜索空间** | **无界** (LLM 可生成任何合法 DSL) | **有界** (5+1 轴模板组合, 有限空间) |
| **约束机制** | signature_validation (strict/warn/ignore) + max_depth 权限 | max_chain_depth + max_nested_search_iterations + governor authorize |
| **评估** | ❌ 无内置评估 (生成即注册) | ✅ IEvaluator V2 评分 + UCB1 探索/利用 |
| **归因** | ❌ 无 (生成后无法归因到 LLM 的哪个决策) | ✅ ADR-0086 归因 (chain 级版本对差分) |
| **治理** | ⚠️ 弱 (仅 signature 校验 + 断链导致实际无法注册) | ✅ 强 (MutationGovernor L1 authorize + 回归门 + attribution) |
| **当前状态** | 🔴 断链 (node_executor.cpp:327 注释掉, P0) | ✅ ship (T20, 但零消费者) |
| **适用场景** | 探索性/开放式子图生成 (LLM 创造力) | 优化性/结构化 chain 发现 (搜索最优组合) |

### 5.2 交互模式 (3 种集成方式)

#### 模式 1: Axis6 chain 包含 GenerateSubGraph 节点 (chain → subgraph)

```
MCTS 搜索出的 WorkflowGraph:
  [Start] → [axis6=Reflect] → [axis6=Compile] → [GenerateSubGraph] → [End]

materialize 后:
  chain 执行到 GenerateSubGraph 节点时,
  LLM 在 chain 的约束上下文中生成子图
  (GenerateSubGraph 的 prompt 由 chain 上游节点输出填充)
```

**关键设计点**: GenerateSubGraph 作为 chain 的**末端开放节点** — chain 提供结构化骨架，GenerateSubGraph 在骨架内提供 LLM 创造力。**深度限制叠加**: `max_chain_depth(3) × generate_subgraph.max_depth(2)` = 理论最深 6 层，需定义**联合深度上限**（§六 G6）。

#### 模式 2: GenerateSubGraph 生成的子图包含 Axis6 节点 (subgraph → chain)

```
LLM 通过 GenerateSubGraph 生成的 DSL 子图:
  ### AgenticDSL `/dynamic/plan_v1`
  nodes:
    - type: dsl_call, subgraph: "/lib/cognitive/gepa_reflect@v1"   # 等效 axis6=Reflect
    - type: dsl_call, subgraph: "/lib/cognitive/mcts_search@v1"    # 等效 axis6=Search
```

**关键设计点**: LLM 生成的子图**可以引用 cognitive_domain specialists**（通过 §十四 M1 inject_subgraphs_into_prompt 注入的可用 skill 列表），等效于 LLM 在自由空间中"手动"构建 chain。这条路径**绕过 MCTS 搜索**，但也**绕过 Axis6 的评估/归因/治理** — 需要决定：是否要求 GenerateSubGraph 生成的含 cognitive_domain 节点的子图**也走 MutationGovernor 治理**？（§六 G5）

#### ~~模式 3: 双轨并行 + 仲裁~~ (Oracle 修正: V1 删除)

> **Oracle 修正 (session `ses_facbd3ffbffeUjlJgZsgMWFiM4`)**: 双轨仲裁是**早产优化**。
> V1 默认结构化路径 (Axis6/MCTS, 可归因), GenerateSubGraph 仅在 chain 显式包含时启用 (模式 1)。
> 仲裁器引入额外的 eval 比较 + margin 阈值 + 双路径并行成本, V1 无此需求。V2 再评估。

**V1 规则**: 默认路径 A (结构化可归因); GenerateSubGraph 仅作为 chain 的末端开放节点 (模式 1) 或独立使用 (与 Axis6 无交互, 走自身治理路径)。

### 5.3 当前集成缺口 (关键!)

**GenerateSubGraph 断链与 Axis6 的隐藏依赖**: `node_executor.cpp:327` 的 `g_current_engine->append_graphs()` 被注释掉，意味着：
- GenerateSubGraph 生成的子图**无法注册到调度器**（动态 in-loop 注册路径断裂）
- 如果 Axis6 chain 中包含 GenerateSubGraph 节点，该节点在运行时无法注册其生成的子图

**Oracle 优先级修正 (session `ses_facbd3ffbffeUjlJgZsgMWFiM4`)**: 断链**降级** — 静态路径 A (`DSLEngine::append_graphs()`) 可用，断链只阻塞 chain 内含 GenerateSubGraph 节点的动态 in-loop 场景。**V1 chain 可以不包含 GenerateSubGraph 节点，因此断链不是 Axis6 端到端的硬前置**。断链修复 (§十一 11.6) 仅在 chain 需要动态子图生成时才升级回 P0。

---

## 六、未考虑/未想到的 7 项缺口 (G1-G7)

> 按严重程度排序。G1-G3 为 🔴 阻塞级，G4-G5 为 🟠 重要级，G6-G7 为 🟡 设计级。

### G1 🔴 Materialize 缺口 — WorkflowGraph → ParsedGraph 转换器不存在

**发现**: `grep -rn "to_parsed_graph|to_dsl|materialize|WorkflowGraph.*parse" include/ src/` = **0 命中**

**问题**: MCTS 搜索出的最优 chain 是结构模板，**无法被执行**。Axis6 的完整工作流程在"阶段 2 具体化"断裂 — 搜出来了，但用不了。

**影响**: Axis6 Phase 0 即使 ship（enum + ctor + chain 语义），也只是一个"能产生不可执行结构模板"的搜索引擎。端到端价值为零。

**建议**: 新增 `WorkflowMaterializer`（§四），作为独立 OpenSpec change（~1 sprint），与 Axis6 Phase 0 并行或紧随其后。包含 axis→DSL 节点映射表 + signature 生成 + strict/warn/ignore 校验 + `workflow.materialized` 事件。

### G2 🔴 MCTS 输出零消费者 — 搜索结果无处可去

**发现**: `grep -rn "MCTSWorkflowSearch" src/ examples/ tests/ | grep -v test_mcts | grep -v "mcts_workflow_search\."` = **0 命中**

**问题**: MCTSWorkflowSearch ship 后（2026-08-28），**没有任何生产代码调用它**。它是一个"有输入、有处理、无输出消费者"的孤儿组件。Axis6 chain 搜索出的 `best_workflow` 即使 materialize 成功，也没有调用方触发这个流程。

**影响**: 整个 Axis6 工作流程的"阶段 1 搜索"没有启动者。

**建议**: 定义 **ChainEvolutionDriver** 作为 MCTS 的消费者 — 接收 goal → 调用 MCTSWorkflowSearch → materialize → 注册 → 执行 → 归因 → 提交。

> **Oracle 修正 (session `ses_facbd3ffbffeUjlJgZsgMWFiM4`)**:
> 1. **G2 与 "缺 ChainExecutor" 是同一缺口的两次陈述** — G2 的修复建议本身就是消费者, 应与 G3 合并为单一 "进化驱动器" 组件
> 2. **不复用 CognitiveWorker 队列** — MCTS 搜索是分钟级长任务, 放进 ADR-0020 单线程单队列会饿死其他 cognitive 任务。**V1 = 同步 driver 函数** (纯 C++ 调用, 由进化触发点同步调用), 不进队列

### G3 🔴 进化触发信号零基础设施 — "何时进化"无人回答

**发现**: `grep -rn "should_evolve|evolution_trigger|ready_to_evolve" src/ include/` = **0 命中**

**问题**: 当前进化触发全靠**被动失败**（GEPA）或**手动调用**（MCTS）。没有组件持续监控"当前 cognitive agent 是否达到了可以进化的状态"。§三 EvolutionReadinessGate 只是设计提议，零实装。

**影响**: 自进化闭环的"触发"环节缺失 — 系统无法自主决定"现在是进化的好时机"。

**建议**: 新增 `EvolutionReadinessGate`（§三），纯函数式判定器，消费 4 个已 ship 事件流，输出 Ready/NotReady/Blocked。作为 ChainExecutor 的前置门禁。

### G4 🟠 Cognitive Domain Specialists 未 SKILL.md 化 — chain 无法通过 DSL 调用

**发现**: GEPALoop / MCTSWorkflowSearch / SkillCompiler 都是 C++ class，不是 SKILL.md。`lib/reasoning/` 目录为空（IPER 仅被 dsl.md:1053 引用，零实装）。`lib/loop/*.agent.md` 存在但未被标记为 cognitive domain。

**问题**: 即使 materialize 成功，chain 中 axis6=Reflect 节点要调用的 `/lib/cognitive/gepa_reflect@v1` **不存在**。DSL 无法 dsl_call 一个纯 C++ class。

**影响**: 阶段 4 执行时，chain 中的 cognitive_domain 节点无法通过 DSL 调度到实际 specialist。

**建议 (Oracle T2 修正)**: **V1 走 tool 注册路线** — 将 GEPALoop/MCTSWorkflowSearch/SkillCompiler 注册为 tool (`cognitive::gepa_reflect` / `cognitive::mcts_search` / `cognitive::skill_compile`, ToolMetadata V2, approval=plan)。C++ class 经 ToolRegistry 注册即可被 `tool_call` 节点调用, **比写 3 个 .agent.md 更简单, 且天然走 ADR-0004 审批矩阵**（~0.5 sprint）。SKILL.md 化（`/lib/cognitive/*.agent.md`）是 V2 可组合性需求, 非 V1 阻塞。

### G5 🟠 GenerateSubGraph 绕过治理 — LLM 自由生成的 chain 无评估/归因/授权

**发现**: GenerateSubGraph 的治理仅 signature_validation (strict/warn/ignore)，**无 eval / 无归因 / 无 MutationGovernor authorize**。而 Axis6 chain 有完整治理链（IEvaluator + governor authorize + attribution + 回归门）。

> **Oracle 严重度上调 (session `ses_facbd3ffbffeUjlJgZsgMWFiM4`)**: 文档假设 signature_validation 是有效防线 — 实际 `node_executor.cpp:309` 是 `bool is_valid = true; // Placeholder`，**校验根本没实现，strict 模式恒通过**。GenerateSubGraph 当前治理 ≈ 零。"攻击面"措辞对 solo-dev 本地系统偏重，实质是**治理一致性**问题。

**问题**: 如果 LLM 通过 GenerateSubGraph 生成一个等效于 axis6 chain 的子图（模式 2），它**完全绕过** Axis6 的治理体系。这是治理不对称 — 同一结果（cognitive_domain chain），一条路径（Axis6/MCTS）受严格治理，另一条路径（GenerateSubGraph）治理为零（signature 校验是占位符）。

**影响**: 治理一致性缺陷 — 恶意或低质量 LLM 输出可通过 GenerateSubGraph 注入未受治理的 chain，而 Axis6 的治理设计（决策 8, ADR-0085 兼容）形同虚设。

**建议 (Oracle 修正, 分两步)**:
1. **先修复 signature_validation 占位符** (`node_executor.cpp:309` 替换为真 schema 校验, 复用 ADR-0073 nlohmann validator) — 独立 change `signature-validation-real-impl` (~0.5 sprint)
2. **再加 cognitive_domain 检测** — GenerateSubGraph 生成的子图若含 `/lib/cognitive/` 引用或 `cognitive::*` tool, 升级为 strict + governor authorize

### G6 🟡 深度限制三重奏未对齐 — 3 个独立深度限制的联合上限未定义

**发现**: 3 个独立深度限制同时存在：
- GenerateSubGraph: `generate_subgraph: { max_depth: 2 }` (DSL 权限， dsl.md:771)
- Axis6 chain: `max_chain_depth = 3` (chain 内连续 specialist 节点数)
- Axis6 嵌套: `max_nested_search_iterations = 30` (axis6=Search 节点嵌套 MCTS 迭代)

> **Oracle 修正 (session `ses_facbd3ffbffeUjlJgZsgMWFiM4`) — 原文档自相矛盾**: §五.2 模式 1 算的是 3×2=**6 层**，本节却算 3×30×2=**180 层** — `max_nested_search_iterations` 是**迭代次数**不是嵌套深度。180 层场景不现实。**真正的风险是预算**（每次嵌套搜索都烧 LLM token），应按预算而非深度设防 — 见 N1 (进化预算上限)。

**问题 (修正后)**: 多个独立深度/迭代限制的组合**总成本**无预算上限。chain_depth × subgraph_depth 的真实最大层数 ≈ 3 × 2 = 6 层（可接受），但每层内的 nested MCTS 迭代 (30) × IEvaluator 调用是 LLM 成本的主要来源。

**影响 (修正后)**: 预算失控 — 不是"180 层深度"，而是"嵌套搜索 × 评估 × 反思的 LLM 调用次数"无上限。

**建议 (Oracle 修正)**: **按预算设防而非按深度** — 新增 `evolution_cycle_budget` 预留字段 (ExecutionBudget, N1 修复 change `evolution-budget-cap`)，超限 fail-closed。深度限制保留为辅助防线 (chain_depth=3 + subgraph_depth=2 已足够)。

### G7 🟡 Chain 内归因（Blame Assignment）— ADR-0086 无法回答"chain 中哪个 specialist 贡献了提升"

**发现**: ADR-0086 `attribute_version_pair` 是**版本对差分**（chain_A vs chain_B），但 chain 是序列。+0.06 提升无法归因到 chain 中的具体 specialist（是 Reflect 贡献了还是 Search 贡献了？）。

**问题**: 多主体归因的"信用分配"在 chain 层面仍然缺失 — 我们只能归因"新 chain 比旧 chain 好"，但不知道"新 chain 中哪个环节是改进的关键"。这阻碍了 chain 的**局部优化**（只能整体替换，不能局部调整）。

**影响**: 自进化只能做"整个 chain 换血"，不能做"chain 中某个 specialist 升级"。粒度过粗。

**建议**: V2 阶段定义 **ChainAttributionRecord**（扩展 ADR-0086），支持 chain 内节点级归因 — 通过**消融实验**（ablation：逐个移除 chain 中的 specialist，观察 eval 变化）或**Shapley 值近似**（对 chain 中每个 specialist 计算边际贡献）。这是反事实归因（V2 deferred）的具体形式，当前不阻塞 V1，但需在 ADR-0086 V2 范围中显式声明。

---

### Oracle 评审新增缺口 N1-N5 (session `ses_facbd3ffbffeUjlJgZsgMWFiM4`)

> 以下 5 项缺口由 Oracle 独立评审发现, 原文档未列出。N1/N2 为 🔴 Blocker 级。

### N1 🔴 进化预算失控 (Evolution loop burn) — 进化循环无成本上限

**发现**: MCTS 搜索 (100 迭代 × IEvaluator 调用) × GEPA 反思 (LLM) × 嵌套 MCTS 全是 LLM 调用, 且进化是**正反馈循环** (越进化越触发评估)。ExecutionBudget 管单次执行, 无"每进化周期成本上限"。

**问题**: EvolutionReadinessGate 的预算检查只查**余额**, 不**预留** — 进化循环可烧光预算后才被 exceeded() 拦截。**这才是 G6 的真身** (按预算设防而非按深度)。

**影响**: 第一次自进化就可能烧光预算, fail-closed 原则要求先有钱闸。

**建议**: ExecutionBudget 增 `max_evolution_llm_calls` + `evolution_llm_calls_used` + `try_consume_evolution_llm_call()` (CAS 原子, 与现有 try_consume_* 同构) + `reset_evolution_cycle()`。默认 -1 无限制保证零回归。修复 change: `openspec/changes/2026-08-31-evolution-budget-cap/` (~0.25 sprint)。

### N2 🔴 变异提交时 in-flight 一致性 — 旧 chain 执行中的 session/task 怎么办

**发现**: governor.commit() 切换 chain 时, 正在执行旧 chain 的 session (ADR-0079 4-Scope) 和 DomainWorkerPool 排队/在跑的 DomainTask 的一致性未定义。

**问题**: (a) 执行入口未绑定 chain 版本快照 — in-flight 执行用旧 chain 还是新 chain? (b) 回滚后基于旧 chain 的 trajectory 归因到哪个版本? (c) commit 是否等待 in-flight 完成 (drain) 还是立即切换?

**影响**: 归因错误 (旧 chain 的 trajectory 被归到新 chain) + 执行结果不一致。

**建议**: 执行入口固定 chain 版本快照 (ExecutionSession 构造时记录 chain_version); commit 语义定义 (V1: 新版本仅影响新启动的 execution, in-flight 继续旧版本); 回滚时 trajectory 携带 chain_version 供归因。

### N3 🟠 并发进化互斥 — 并发 MCTS 搜索的 propose→commit 竞争

**发现**: CognitiveWorker 单线程 (ADR-0020), 但触发源可多个 (GEPA 失败 / 手动 / 未来监控)。两个并发 MCTS 搜索同时走 propose→commit 无互斥保护; governor 状态非线程安全。

**问题**: 并发进化的提交竞争条件未定义 — 两个 chain 同时通过评估, 谁的 commit 生效?

**影响**: 状态不一致 + 归因混乱。

**建议**: V1 单触发源串行规避 (GEPA 失败驱动同步调用); V2 加 governor 互斥锁 (propose→commit 原子化)。

### N4 🟠 LLM-in-the-loop chain 测试策略 — mock 分层未定义

**发现**: MockLLMProvider 在 provider 层, 但 MCTS 的 IEvaluator / GEPA 反思 / GenerateSubGraph 生成各需独立 mock 点。T14 回归套件是行为级的, chain 级 golden 测试缺失。

**问题**: 3 层 chain 的 E2E 测试如何构造确定性结果? mock 在哪个层级?

**影响**: chain 级功能无法被确定性测试覆盖。

**建议**: 定义 chain 级 mock 分层 (provider mock + evaluator mock + governor mock 三独立注入点); 参考 test_mcts_workflow_search.cpp 的 Mock evaluator 模式扩展到 chain 级。

### N5 🟠 GenerateSubGraph 断链修复的回归风险 — 迭代中图突变

**发现**: `append_graphs` 在 node_executor.cpp:327 被注释**必有原因** — 执行中 DAG 突变 → `rebuild_dynamic_graph()` 与主循环迭代器交互。

**问题**: 直接恢复 append_graphs 调用可能在主循环迭代中修改图, 导致迭代器失效或节点重复执行。

**影响**: 断链修复不是"取消注释"那么简单 — 需专项并发/迭代安全验证。

**建议**: 断链修复 change (`generatesubgraph-append-restore`) 必须含: rebuild_dynamic_graph 迭代安全测试 + 图突变时的 ready_queue 一致性验证 + in-flight 节点完成保证。

---

## 七、缺口依赖图与修复顺序 (Oracle 修正版)

> **Oracle 评审修正 (session `ses_facbd3ffbffeUjlJgZsgMWFiM4`)**: 缺口重排 + P0 断链降级 + G2 合并 + 新增 N1-N5。

```
🔴 Blocker 级 (不修则 Axis6 端到端零价值):
  G1 Materialize (WorkflowGraph→DSL 文本)         — Oracle 修正: 输出 DSL 文本
  N1 进化预算上限 (evolution-budget-cap)          — Oracle 新增, G6 真身
  G4 Cognitive Specialists tool 注册              — Oracle 修正: tool 路线非 SKILL.md

🟠 Important 级 (端到端可跑但有缺陷):
  G5 GenerateSubGraph 治理 (先修 signature 占位符)
  N2 In-flight 版本绑定 (commit 语义定义)
  P0 GenerateSubGraph 断链修复                     — Oracle 降级: 仅 chain 含 GenerateSubGraph 节点时需
  G3 EvolutionReadinessGate 精简版 (3 条件)        — Oracle 修正: 7→3 条件
  N3 并发进化互斥                                  — V1 单触发源串行规避
  N4 chain 级测试策略                              — mock 分层定义
  N5 断链修复回归风险                              — 迭代安全验证

🟡 V2 级:
  G6 联合深度上限 (并入 N1 预算方案)
  G7 Chain 内归因 (ADR-0086 V2)
  连续监控触发
```

### 修复任务表 (Oracle 评审 + Solo Dev 容量 ~27h/周)

| # | Change 名 | 估时 | 依赖 | 修复缺口 | 并行 |
|---|-----------|------|------|---------|------|
| **T1** | `workflow-materializer-v1` | 1 sprint | 无 | G1 | 起点 |
| **T2** | `cognitive-specialists-as-tools` | 0.5 sprint | 无 | G4 (tool 路线) | ∥T1 |
| **T3** | `evolution-budget-cap` | 0.25 sprint | 无 | N1 | ∥T1/T2 |
| **T4** | `signature-validation-real-impl` | 0.5 sprint | 无 | G5 (占位符修复) | ∥ |
| **T5** | `evolution-readiness-gate-v1` | 0.5 sprint | **ADR-0086 ship** (关键路径) | G3 (3 条件) | T1 后 |
| **T6** | `chain-evolution-driver-v1` | 0.5 sprint | T1+T2+T5 | G2 (合并) — 同步 driver, 非 CognitiveWorker 队列 | 收官 |
| T7 | `generatesubgraph-append-restore` | 0.5 sprint | T4 | P0 断链 + N5 | 可推迟 |
| T8 | chain 内归因 (G7) | 2 sprint | T6 + 数据积累 | G7 | V2 |

**关键路径**: ADR-0086 ship (docs-only, self-review) → T5 → T6

**额外硬前置**:
- ADR-0061-08 v1.1 amendment 评审通过 (24h cooling-off + self-review)
- ADR-0086 信用分配契约 ship (已立项 🔍 Proposed, T5 前置)
- **Hygiene: 33 commits push 已完成** (2026-08-31)

**总估时**: T1-T6 ≈ 3.25 sprint 串行 (~2.25 sprint 并行压缩), T7+ 可推迟。

**最关键的 1 件事 (Oracle)**: **T1 Materializer (DSL 文本输出) + 手写 driver 脚本跑通 "MCTS 搜索→DSL 文本→append→执行" demo** — 成本最低、无依赖, 立刻把已 ship 的 MCTS 从"孤儿组件"变成"可演示价值", 实证检验整条管线假设。

---

## 八、与现有文档/ADR 的对接

| 文档/ADR | 关系 |
|---------|------|
| `agent-orchestration-architecture-2026-08.md` v1.5 | 本文档是其 §十四 M5 + §18.10.1 的**端到端展开** — 从"触发条件"扩展为"完整工作流程 + 缺口清单" |
| `adr-0061-08-v1-1-amendment-axis6.md` | 本文档 §二 阶段 1 是其搜索机制的**执行语义补充** — amendment 定义"怎么搜"，本文档定义"搜出来之后怎么办" |
| `adr-0086-credit-assignment-contract.md` | 本文档 §二 阶段 5 是其归因判定的**应用层** — ADR-0086 定义"怎么归因"，本文档定义"在哪里归因" |
| §十一 GenerateSubGraph 分析 | 本文档 §五 是其 P0 断链的**影响扩展** — 断链不仅影响 GenerateSubGraph 自身，还阻塞 Axis6 chain 端到端 |
| ADR-0085 §决策 5 | 本文档所有组件（EvolutionReadinessGate / WorkflowMaterializer / ChainExecutor）均为**无状态工具类/纯函数**，非 MetaAgent — 与 ADR-0085 完全兼容 |
| self-evolution-architecture-2026-08.md | 本文档 §三 是 S1-S2 阶段"受治理变异"的**触发机制细化** — 填补"何时允许变异"的判定空白 |

---

## 九、验证命令

```bash
# 1. 双图宇宙验证 (G1 缺口存在性)
grep -rn "to_parsed_graph\|to_dsl\|materialize\|WorkflowGraph.*parse" include/ src/ 2>/dev/null | wc -l
# 预期: 0 (G1 缺口确认)

# 2. MCTS 消费者验证 (G2 缺口存在性)
grep -rn "MCTSWorkflowSearch" src/ examples/ tests/ 2>/dev/null | grep -v test_mcts | grep -v "mcts_workflow_search\." | wc -l
# 预期: 0 (G2 缺口确认)

# 3. 进化触发验证 (G3 缺口存在性)
grep -rn "should_evolve\|evolution_trigger\|ready_to_evolve" src/ include/ 2>/dev/null | wc -l
# 预期: 0 (G3 缺口确认)

# 4. Cognitive SKILL.md 验证 (G4 缺口存在性)
ls lib/cognitive/ lib/reasoning/ 2>/dev/null | wc -l
# 预期: 0 (G4 缺口确认 — 目录不存在或为空)

# 5. GenerateSubGraph 断链验证 (P0 硬前置)
grep -c "Placeholder.*append_graphs\|append_graphs.*Placeholder" src/modules/executor/node_executor.cpp
# 预期: 1 (断链存在, 修复后应为 0)

# 6. 深度限制三重奏验证 (G6)
grep -rn "max_depth\|max_chain_depth\|max_nested_search_iterations" docs/specs/dsl.md docs/adr/skill/adr-0061-08-v1-1-amendment-axis6.md | wc -l
# 预期: ≥3 (3 个独立深度限制存在)

# 7. Axis6 chain 文档引用完整性
grep -c "G1\|G2\|G3\|G4\|G5\|G6\|G7" docs/architecture/axis6-chain-workflow-architecture-2026-08.md
# 预期: ≥14 (7 缺口 × 至少 2 处引用)

# 8. N1-N5 新缺口存在性 (v1.1)
grep -c "N1\|N2\|N3\|N4\|N5" docs/architecture/axis6-chain-workflow-architecture-2026-08.md
# 预期: ≥10 (5 缺口 × 至少 2 处引用)

# 9. Gate 精简验证 (v1.1: 7→3 条件)
grep -c "C4\|C5\|C6\|C7" docs/architecture/axis6-chain-workflow-architecture-2026-08.md
# 预期: ≥4 (C4-C7 仅在"Oracle 修正"说明中引用, 不在判定矩阵)

# 10. Materializer DSL 文本输出 (v1.1 Oracle 修正)
grep -c "materialize_to_dsl\|continue_with_generated_dsl" docs/architecture/axis6-chain-workflow-architecture-2026-08.md
# 预期: ≥4

# 11. 模式 3 已删除 (v1.1)
grep -c "模式 3: 双轨并行" docs/architecture/axis6-chain-workflow-architecture-2026-08.md
# 预期: 1 (仅删除标记, 仲裁器内容已删除)

# 12. Oracle 评审 session 引用
grep -c "ses_facbd3ffbffeUjlJgZsgMWFiM4" docs/architecture/axis6-chain-workflow-architecture-2026-08.md
# 预期: ≥5 (各修正点引用)
```

---

## 变更记录

| 日期 | 版本 | 变更 |
|------|------|------|
| 2026-08-31 | v1.0 | 初始化（双图宇宙第一性原理 + 6 阶段完整工作流程 + EvolutionReadinessGate 判定矩阵 + WorkflowMaterializer 转换规则 + GenerateSubGraph×Axis6 三种交互模式 + 7 项未识别缺口 G1-G7 + 缺口依赖图与修复顺序）|
| 2026-08-31 | v1.1 | **Oracle 评审修正** (session `ses_facbd3ffbffeUjlJgZsgMWFiM4`): (1) **§三 Gate 7→3 条件** — C4 混杂 ⊂ C1 归因 (ADR-0086 Confounded 内含), C6 governor 预检是双重门禁, C5 CaptureMode 属蒸馏正交面, C7 冷却是配置 guard; V1 = C1+C2+预算预留; (2) **§四 Materializer 输出 DSL 文本** (非直接构造 ParsedGraph) — 复用 `continue_with_generated_dsl()` 现有路径 (engine.cpp:390, 4 处调用) + MarkdownParser 单一事实源; axis6 节点走 **tool_call** 路线 (非 dsl_call, 无需先 SKILL.md 化); (3) **§五 删除模式 3 双轨仲裁** — 早产优化, V1 默认结构化路径; (4) **§六 G2 合并 + G4 tool 路线 + G5 严重度上调 (signature 占位符 `is_valid=true` 恒通过) + G6 自相矛盾修复 (180 层夸大 → 预算设防)**; (5) **新增 5 项缺口 N1-N5** — N1 进化预算失控 (Blocker, G6 真身) / N2 in-flight 一致性 / N3 并发互斥 / N4 chain 测试策略 / N5 断链修复回归风险; (6) **§七 缺口依赖图重排** — P0 断链降级 (仅 chain 含 GenerateSubGraph 节点时需) + 8 任务表 (T1-T8) + 关键路径 (ADR-0086 ship → T5 → T6) + 最关键 1 件事 (T1 + driver demo)。 |

---

**状态**: 🔍 Proposed（v1.1）— 指导性文档，需架构组评审后晋升为 Active
**维护者**: solo-dev（Sisyphus）
**下一修订**: T1-T6 缺口修复落地后 + ADR-0061-08 v1.1 amendment Approved 后 + ADR-0086 ship 后
