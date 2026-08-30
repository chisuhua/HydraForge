# Axis6 Chain 完整工作流程架构 (Axis6 Cognitive Domain Chain Architecture)

**生成日期**: 2026-08-31
**最后验证**: 2026-08-31（v1.0 — 基于代码审计 + Oracle 评审 + ADR-0061-08 v1.1 + ADR-0086 草案, 含 7 项未识别缺口 G1-G7）
**作者**: Architecture Working Group（Sisyphus 综述，整合用户提议 + Metis/Oracle 评审 + 代码审计）
**状态**: 🔍 Proposed（v1.0，指导性文档——Axis6 chain 的端到端工作流程定义与缺口清单，非 ADR 本身）

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
  NotReady_Attribution,     // 归因不成立 (Confounded/Insufficient/NotAttempted)
  NotReady_Regression,      // 行为回归门未通过
  NotReady_Budget,          // 预算不足
  NotReady_Confounder,      // 存在未控制混杂
  NotReady_CaptureMode,     // CaptureMode != Training (蒸馏场景)
  Blocked_Governor,         // MutationGovernor 硬门禁
};

struct EvolutionReadinessReport {
  EvolutionReadiness verdict;
  std::string reason;                    // 人类可读原因
  AttributionVerdict attribution;        // ADR-0086 归因判定
  bool regression_passed;                // T14 回归门
  bool budget_ok;                        // 预算检查
  std::vector<ConfounderRecord> open_confounders;  // 未控制混杂
};

// 纯函数: 消费事件流快照, 输出判定 (无 LLM, 无 specialist 执行)
EvolutionReadinessReport check_evolution_readiness(
    const std::string& agent_id,
    const EventLogSnapshot& events,       // ADR-0080 事件日志快照
    const ExecutionBudget& budget);       // ADR-0019 §6

} // namespace agenticdsl
```

### 3.2 判定矩阵 (全部条件 AND)

| # | 条件 | 数据来源 (已 ship) | Ready 判据 |
|---|------|-------------------|-----------|
| C1 | 归因判定 | ADR-0086 `attribute_version_pair()` | `verdict == Attributed` |
| C2 | 行为回归门 | T14 BehavioralRegressionGate | `gate.pass(candidate, baseline) == true` |
| C3 | 预算充足 | ExecutionBudget | `!budget.exceeded()` 且剩余 ≥ 1 次进化迭代成本 |
| C4 | 无未控制混杂 | ADR-0086 ConfounderRecord | 所有 `confounder.controlled == true` |
| C5 | CaptureMode | ADR-0080 v1.2 CaptureMode | 蒸馏场景: `mode == Training`; 非蒸馏: 不要求 |
| C6 | MutationGovernor 预检 | ADR-0084 | `governor.propose()` 白名单 + L4 + 矩阵通过 |
| C7 | 冷却期 | (新增, 防进化振荡) | 距上次 commit ≥ `min_evolution_interval` (默认 1h) |

**7 条件全部满足 → Ready; 任一不满足 → NotReady(具体 reason); C6 失败 → Blocked**

### 3.3 进化触发的 3 种模式 (与现有机制对齐)

| 模式 | 触发源 | 当前状态 | 与 EvolutionReadinessGate 关系 |
|------|--------|---------|-------------------------------|
| **失败驱动** | GEPALoop.reflect_and_commit (失败轨迹) | ✅ ship (T19) | 失败触发 → 先过 ReadinessGate → 再 reflect |
| **搜索驱动** | MCTSWorkflowSearch.search() 显式调用 | ✅ ship (T20) | 调用前 → 先过 ReadinessGate → 再搜索 |
| **连续监控** | (待设计) 周期性检查 evaluation.result 事件流 | ❌ 零基础设施 (G3) | ReadinessGate 周期性运行, Ready → 触发搜索 |

---

## 四、WorkflowGraph → ParsedGraph 具体化 (跨宇宙桥)

**这是 Axis6 chain 从"搜索结果"变成"可执行实体"的唯一路径，当前完全不存在（G1 缺口）**。

### 4.1 转换规则 (axis → DSL 节点映射)

| Axis 值 | → DSL 节点类型 | 说明 |
|---------|---------------|------|
| **axis6=Reflect** | `type: dsl_call, subgraph: "/lib/cognitive/gepa_reflect@v1"` | 调用 GEPALoop (需先 SKILL.md 化, 见 §六 G4) |
| **axis6=Search** | `type: dsl_call, subgraph: "/lib/cognitive/mcts_search@v1"` | 嵌套 MCTS, 预算 `max_nested_search_iterations=30` |
| **axis6=Compile** | `type: dsl_call, subgraph: "/lib/cognitive/skill_compile@v1"` | 调用 SkillCompiler (T17 已 ship) |
| **axis6=Reason** | `type: dsl_call, subgraph: "/lib/reasoning/iper_loop@v1"` | IPER (未实装, Phase 1 预留) |
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

### 4.2 转换器接口 (提议)

```cpp
namespace agenticdsl {

class WorkflowMaterializer {
 public:
  // WorkflowGraph (图宇宙 A) → ParsedGraph (图宇宙 B)
  // 失败时返回 nullopt + failure_reason
  static std::optional<ParsedGraph> materialize(
      const WorkflowGraph& wf_graph,           // MCTS 输出
      const TaskSpec& spec,                    // 原始任务描述
      const MaterializeConfig& config);

  struct MaterializeConfig {
    std::string output_path_prefix = "/dynamic/mcts/";  // GenerateSubGraph 同命名空间
    std::string signature_validation = "strict";         // 与 GenerateSubGraph 同机制
    bool emit_lineage_event = true;                      // emit workflow.materialized 事件
  };
};

} // namespace agenticdsl
```

### 4.3 signature 生成 (与 GenerateSubGraph 对齐)

materialize 后的 ParsedGraph 必须携带 signature（与 GenerateSubGraph 的 signature_validation strict/warn/ignore 同机制），确保：
- 输入/输出 schema 明确
- `strict` 模式下 signature 违规 → 拒绝注册
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

#### 模式 3: 双轨并行 + 仲裁 (chain vs subgraph 竞争)

```
同一任务, 两条路径并行:
  路径 A: MCTSWorkflowSearch + Axis6 → 结构化 chain
  路径 B: GenerateSubGraph → LLM 自由生成子图

仲裁器 (待设计, G7):
  IF 路径 A eval > 路径 B eval + margin:
    采用路径 A (结构化, 可归因)
  ELSE IF 路径 A 搜索空间不覆盖目标 (available_specialists 不足):
    采用路径 B (开放式, 但需额外治理)
  ELSE:
    默认路径 A (fail-safe: 优先可归因方案)
```

### 5.3 当前集成缺口 (关键!)

**GenerateSubGraph 断链与 Axis6 的隐藏依赖**: `node_executor.cpp:327` 的 `g_current_engine->append_graphs()` 被注释掉，意味着：
- GenerateSubGraph 生成的子图**无法注册到调度器**
- 即使 Axis6 chain materialize 成功，如果 chain 中包含 GenerateSubGraph 节点，该节点在运行时也无法注册其生成的子图
- **Axis6 chain 的完整工作流程在阶段 3（注册）和阶段 4（执行）都被这个 P0 断链阻塞**

**修复优先级**: GenerateSubGraph 断链修复（§十一 11.6，恢复 `append_graphs_callback_` 调用链）是 Axis6 chain 端到端工作的**硬前置**。

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

**建议**: 定义 **ChainExecutor**（或复用 CognitiveWorker）作为 MCTS 的消费者 — 接收 goal → 调用 MCTSWorkflowSearch → materialize → 注册 → 执行 → 归因 → 提交。这是 G3 EvolutionReadinessGate 的下游执行体。

### G3 🔴 进化触发信号零基础设施 — "何时进化"无人回答

**发现**: `grep -rn "should_evolve|evolution_trigger|ready_to_evolve" src/ include/` = **0 命中**

**问题**: 当前进化触发全靠**被动失败**（GEPA）或**手动调用**（MCTS）。没有组件持续监控"当前 cognitive agent 是否达到了可以进化的状态"。§三 EvolutionReadinessGate 只是设计提议，零实装。

**影响**: 自进化闭环的"触发"环节缺失 — 系统无法自主决定"现在是进化的好时机"。

**建议**: 新增 `EvolutionReadinessGate`（§三），纯函数式判定器，消费 4 个已 ship 事件流，输出 Ready/NotReady/Blocked。作为 ChainExecutor 的前置门禁。

### G4 🟠 Cognitive Domain Specialists 未 SKILL.md 化 — chain 无法通过 DSL 调用

**发现**: GEPALoop / MCTSWorkflowSearch / SkillCompiler 都是 C++ class，不是 SKILL.md。`lib/reasoning/` 目录为空（IPER 仅被 dsl.md:1053 引用，零实装）。`lib/loop/*.agent.md` 存在但未被标记为 cognitive domain。

**问题**: 即使 materialize 成功，chain 中 axis6=Reflect 节点要调用的 `/lib/cognitive/gepa_reflect@v1` **不存在**。DSL 无法 dsl_call 一个纯 C++ class。

**影响**: 阶段 4 执行时，chain 中的 cognitive_domain 节点无法通过 DSL 调度到实际 specialist。

**建议**: 新增 3 个 SKILL.md（`/lib/cognitive/{gepa_reflect,mcts_search,skill_compile}.agent.md`），与现有 `lib/loop/*.agent.md` 同构。这是 §十四 M5 cognitive domain 显式注册的具体落地形式（~0.5 sprint）。

### G5 🟠 GenerateSubGraph 绕过治理 — LLM 自由生成的 chain 无评估/归因/授权

**发现**: GenerateSubGraph 的治理仅 signature_validation (strict/warn/ignore)，**无 eval / 无归因 / 无 MutationGovernor authorize**。而 Axis6 chain 有完整治理链（IEvaluator + governor authorize + attribution + 回归门）。

**问题**: 如果 LLM 通过 GenerateSubGraph 生成一个等效于 axis6 chain 的子图（模式 2），它**完全绕过** Axis6 的治理体系。这是治理不对称 — 同一结果（cognitive_domain chain），一条路径（Axis6/MCTS）受严格治理，另一条路径（GenerateSubGraph）几乎无治理。

**影响**: 攻击面 — 恶意或低质量 LLM 输出可通过 GenerateSubGraph 注入未受治理的 chain，而 Axis6 的治理设计（决策 8, ADR-0085 兼容）形同虚设。

**建议**: 明确规则 — **GenerateSubGraph 生成的子图中若包含 cognitive_domain specialist 节点（gepa/mcts/skill_compile/iper），必须走与 Axis6 相同的 MutationGovernor 治理流程**。可在 GenerateSubGraph 的 signature_validation 阶段增加 cognitive_domain 检测（若子图含 `/lib/cognitive/` 引用，升级为 strict + governor authorize）。

### G6 🟡 深度限制三重奏未对齐 — 3 个独立深度限制的联合上限未定义

**发现**: 3 个独立深度限制同时存在：
- GenerateSubGraph: `generate_subgraph: { max_depth: 2 }` (DSL 权限， dsl.md:771)
- Axis6 chain: `max_chain_depth = 3` (chain 内连续 specialist 节点数)
- Axis6 嵌套: `max_nested_search_iterations = 30` (axis6=Search 节点嵌套 MCTS 迭代)

**问题**: 当一个 axis6=Search 节点内的嵌套 MCTS 搜索出的 chain 中又包含 GenerateSubGraph 节点，且该节点继续 generate_subgraph 时，**理论最大深度 = 3 × 30 × 2 = 180 层**（chain_depth × nested_iterations × subgraph_depth）。没有任何机制限制这个组合爆炸。

**影响**: 预算失控 — LLM 互相调用在最坏情况下可达 180 层，每次调用都消耗 token。

**建议**: 定义**联合深度上限** `max_total_cognitive_depth = 6`（chain_depth × max_subgraph_depth，不含 nested_iterations 因为它是迭代数不是层数），并在 ExecutionBudget 中增加 `cognitive_depth_used` 计数器。超过硬上限 → fail-closed。

### G7 🟡 Chain 内归因（Blame Assignment）— ADR-0086 无法回答"chain 中哪个 specialist 贡献了提升"

**发现**: ADR-0086 `attribute_version_pair` 是**版本对差分**（chain_A vs chain_B），但 chain 是序列。+0.06 提升无法归因到 chain 中的具体 specialist（是 Reflect 贡献了还是 Search 贡献了？）。

**问题**: 多主体归因的"信用分配"在 chain 层面仍然缺失 — 我们只能归因"新 chain 比旧 chain 好"，但不知道"新 chain 中哪个环节是改进的关键"。这阻碍了 chain 的**局部优化**（只能整体替换，不能局部调整）。

**影响**: 自进化只能做"整个 chain 换血"，不能做"chain 中某个 specialist 升级"。粒度过粗。

**建议**: V2 阶段定义 **ChainAttributionRecord**（扩展 ADR-0086），支持 chain 内节点级归因 — 通过**消融实验**（ablation：逐个移除 chain 中的 specialist，观察 eval 变化）或**Shapley 值近似**（对 chain 中每个 specialist 计算边际贡献）。这是反事实归因（V2 deferred）的具体形式，当前不阻塞 V1，但需在 ADR-0086 V2 范围中显式声明。

---

## 七、缺口依赖图与修复顺序

```
G1 Materialize (WorkflowGraph→ParsedGraph)     🔴 阻塞级
  ↓ 依赖
G4 Cognitive Specialists SKILL.md 化            🟠 重要级 (G1 的 materialize 需要可调用的 subgraph)
  ↓ 依赖
G3 EvolutionReadinessGate (进化触发)            🔴 阻塞级 (G1 完成前可并行设计)
  ↓ 依赖
G2 ChainExecutor (MCTS 消费者)                 🔴 阻塞级 (G1+G3 的下游执行体)
  ↓ 依赖
G5 GenerateSubGraph 治理对称                    🟠 重要级 (G1 完成后的治理补齐)
  ↓ 依赖
G6 联合深度上限                                 🟡 设计级 (G1+G4 完成后对齐)
  ↓ 依赖
G7 Chain 内归因 (Blame Assignment)              🟡 设计级 (V2 阶段, ADR-0086 V2 范围)

额外硬前置 (已识别, 非本次新发现):
  P0 GenerateSubGraph 断链修复 (§十一 11.6) — Axis6 chain 端到端工作的硬前置
  ADR-0061-08 v1.1 amendment 评审通过 (24h cooling-off + self-review)
  ADR-0086 信用分配契约 ship (Axis6 Phase 1 前置, 已立项 🔍 Proposed)
```

**推荐修复顺序**:
1. **P0 GenerateSubGraph 断链修复**（已在 §七 P0 清单，~0.5 sprint）— 一切动态注册的前置
2. **G4 Cognitive Specialists SKILL.md 化**（~0.5 sprint）— 与 P0 并行
3. **G1 WorkflowMaterializer**（~1 sprint）— 核心跨宇宙桥
4. **G3 EvolutionReadinessGate**（~0.5 sprint）— 与 G1 并行设计
5. **G2 ChainExecutor**（~0.5 sprint）— 串联 G1+G3
6. **G5 GenerateSubGraph 治理对称**（~0.5 sprint）— 治理补齐
7. **G6 联合深度上限**（~0.5 sprint）— 预算防护
8. **G7 Chain 内归因**（V2，~2 sprint）— ADR-0086 V2 范围

**总估时**: G1-G6 ≈ 3.5-4 sprint（可并行压缩到 ~2.5 sprint），G7 属 V2。

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
```

---

## 变更记录

| 日期 | 版本 | 变更 |
|------|------|------|
| 2026-08-31 | v1.0 | 初始化（双图宇宙第一性原理 + 6 阶段完整工作流程 + EvolutionReadinessGate 判定矩阵 + WorkflowMaterializer 转换规则 + GenerateSubGraph×Axis6 三种交互模式 + 7 项未识别缺口 G1-G7 + 缺口依赖图与修复顺序）|

---

**状态**: 🔍 Proposed（v1.0）— 指导性文档，需架构组评审后晋升为 Active
**维护者**: solo-dev（Sisyphus）
**下一修订**: G1-G7 缺口任一修复落地后 + ADR-0061-08 v1.1 amendment Approved 后 + GenerateSubGraph 断链修复后
