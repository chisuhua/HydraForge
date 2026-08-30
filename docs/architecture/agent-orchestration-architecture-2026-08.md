# 多智能体编排架构总览（Agent Orchestration Architecture）

**生成日期**: 2026-08-29
**最后验证**: 2026-08-30（v1.4 — Oracle 综合评审后续 Step 1：新增 §十八 cognitive-cognitive 协调模式目录 5 模式 + §四 决策树分支 + §十七关联文档 + §九 验证命令）
**作者**: Architecture Working Group（Sisyphus 综述，整合 Oracle + Metis + Deep Agent 会话）
**状态**: 🔍 Proposed（v1.4，指导性文档——定位为**应用场景编排的决策地图**，非 ADR 本身）

> **文档定位**: 回答"一个应用场景应该用哪些 Agent / Pattern / 编排层组合来实现"。
> 它是 `cross-cutting-hooks-architecture-2026-08.md`（横切机制）+ `multi-domain-agent-architecture.md`（认知/领域协作，同目录,2026-08-30 从 `docs/guides/` 迁入）+ `pdk-cross-cutting-patterns`（PDK 家族）的**统一视图**。
> **不重复**: ADR 决策正文（引用为准）、各组件实现细节（引用为准）。

---

## 一、编排全景：5 层模型

```
┌──────────────────────────────────────────────────────────────────────────┐
│  编排层 (Orchestration Layer)                                             │
│  ┌──────────────┐  ┌───────────────┐  ┌───────────────────────────────┐ │
│  │ 场景智能体    │  │ LLM 编排回调   │  │ 行为矩阵统一编排 (IBehavior)   │ │
│  │ Scenario     │  │ llm_selector  │  │ BehaviorOrchestrator          │ │
│  │ Orchestrator │  │ (goal→config) │  │ (Loop + Pattern 统一入口)      │ │
│  │ (V2, 蓝图)   │  │ (V2, 蓝图)    │  │ (V3, 蓝图)                    │ │
│  └──────┬───────┘  └──────┬────────┘  └───────────────┬───────────────┘ │
│         └─────────────────┴──────┬────────────────────┘                 │
│                                  ▼                                       │
│  行为编排层 (Behavior Dispatch Layer)                                    │
│  ┌──────────────────────┐   ┌────────────────────────────────────────┐ │
│  │ CrossCutting         │   │ Loop Dispatcher (编译期)               │ │
│  │ Orchestrator         │   │ LoopDispatcher<LoopType>               │ │
│  │ dispatch(config)     │   │ React / PlanExecute / ForkJoin         │ │
│  └──────────┬───────────┘   └──────────────────┬─────────────────────┘ │
├─────────────┴───────────────────────────────────┴──────────────────────┤
│  认知执行层 (Cognitive Execution Layer)                                 │
│  ┌────────────────────────┐   ┌──────────────────────────────────────┐ │
│  │ CognitiveWorker        │   │ GenerateSubgraphNode (LLM 生成 DSL)  │ │
│  │ 意图→任务分解→DSL 生成  │──▶│ append_graphs() (⚠️ 断链待修复)        │ │
│  └────────────────────────┘   └──────────────────────────────────────┘ │
├────────────────────────────────────────────────────────────────────────┤
│  领域执行层 (Domain Execution Layer)                                    │
│  ┌────────────────────────┐   ┌──────────────────────────────────────┐ │
│  │ DomainWorkerPool       │   │ ToolRegistry → ToolCoordinator       │ │
│  │ code:: / browser:: /   │──▶│ (L1 hook + L4 approval 拦截)          │ │
│  │ human:: 领域能力        │   └──────────────────────────────────────┘ │
│  └────────────────────────┘                                              │
├────────────────────────────────────────────────────────────────────────┤
│  可观测层 (Observability Layer)  — 贯穿所有层                            │
│  IInteractionBus (27+ 主题) → EventLog / OTel / SIEM / 审计 / 蒸馏采集    │
└────────────────────────────────────────────────────────────────────────┘
```

### 各层职责与负责人

| 层 | 核心组件 | 职责 | 类比 |
|----|----------|------|------|
| **编排层** | 场景智能体 / LLM 编排回调 | 根据 goal 决定"用什么 Pattern + 什么 Loop + 什么横切策略" | 指挥家 |
| **行为编排层** | CrossCuttingOrchestrator + LoopDispatcher | 将编排决策翻译为 4 Pattern 装配 / 3 循环实例化 | 分谱员 |
| **认知执行层** | CognitiveWorker | 意图理解 → 任务分解 → 生成业务 DSL 图 | 编剧 |
| **领域执行层** | DomainWorkerPool | 执行领域工具（code/browser/human）| 演员 |
| **可观测层** | IInteractionBus | 全局事件流（27+ 主题）→ 审计/追踪/蒸馏 | 舞台监视器 |

---

## 二、编排单元分类：5 种"可编排体"

### 2.1 Loop Agent（业务循环，3 种）— ADR-0021 + Sprint 20

| 类型 | 状态机 | 关键方法 | 适用 |
|------|--------|----------|------|
| **ReactLoop** | Thinking→Acting→Observing | `run(prompt, ctx, std::stop_token token = {})` | 单轮工具调用 ReAct |
| **PlanExecuteLoop** | Planning→Executing→Verifying→Done/Retry | `run(goal, ctx, std::stop_token token = {})` + `plan_phase`/`execute_phase`/`verify_phase` | 规划-执行-验证 |
| **ForkJoinLoop** | Forking→Executing→Joining→Done | `run(branches, ctx, std::stop_token token = {})` + DomainWorkerPool | 并行分支聚合 |

**共性**: `LoopResult{success, message, final_context, retries_used, total_steps, failed_phase}` 统一返回。

### 2.2 Cross-Cutting Pattern（横切关注点，4 种）— ADR-0085

| Pattern | 注入通道 | 可编排行为 |
|---------|----------|-----------|
| **DecoratorPattern** | L0 set_llm_provider | 计费/合规/PII脱敏/限流/追踪 |
| **HookPattern** | L1 tool / L2 agent / L4 approval | 工具拦截/参数修改/拒绝/审批 |
| **CompositionPattern** | L3 IAgentRegistry | Agent 创建 + 生命周期 + hook 注入 |
| **BusPattern** | L5 IInteractionBus | 事件订阅/转发/SIEM/审计/OTel |

### 2.3 认知 Worker（编排者）— ADR-0020 Sprint 2

`CognitiveWorker(unique_ptr<DSLEngine>, shared_ptr<IInteractionBus>)` — 每 worker 独占 DSLEngine（per-agent 隔离），状态机 idle/running/stopped，构造时强制注入 bus（F7）。

### 2.4 领域 Worker（执行者）— ADR-0020 Sprint 3

`DomainWorkerPool` — N 个 jthread worker + 共享 FIFO 队列 + `register_domain_handler(domain, handler)` + `submit_task(DomainTask)`。DomainTask 含 domain/tool_name/arguments/output_key。

### 2.5 Agent 组合（跨 Agent 编排）— ADR-0060

`IAgentComposition` — 4 模式：`call`（同步）/ `call_async`（异步 + callback）/ `delegate`（委派 + TaskHandle）/ `stream`（流式，Phase 2 占位）。

---

## 三、行为矩阵：5 种编排体 × 编排层 的组合空间

### 3.1 行为矩阵（Loop × Pattern，核心组合）

| | ReactLoop | PlanExecuteLoop | ForkJoinLoop |
|------|-----------|-----------------|--------------|
| **DecoratorPattern** (L0) | 单轮 ReAct 计费/追踪 | Plan 阶段 LLM 计费 | 并行分支 LLM 限流 |
| **HookPattern** (L1/L2/L4) | 每 step 工具拦截/审批 | plan/verify 阶段 Agent hook | 分支工具 pre-hook |
| **CompositionPattern** (L3) | 单 agent 嵌入 | plan-execute 子 agent | fork-join 子 agent |
| **BusPattern** (L5) | 观察 React 轨迹 | 观察 plan 决策 | 观察并行事件 |

### 3.2 认知/领域 × 编排层 组合

| | CognitiveWorker | DomainWorkerPool | GenerateSubgraphNode |
|------|-----------------|------------------|----------------------|
| **CompositionPattern** | 注册认知 agent | 注册领域 agent | — |
| **HookPattern** (agent) | 拦截认知 step | 拦截领域 step | — |
| **HookPattern** (tool) | — | 拦截领域工具调用 | — |
| **BusPattern** | 观察 cognitive.task.* | 观察 domain.task.* | 观察 dsl.call.started/completed |

### 3.3 编排粒度总结

| 编排对象 | 粒度 | 层 |
|----------|------|-----|
| LLM 调用 | per-LLM-call | L0 |
| 工具调用 | per-tool | L1 |
| Agent step | per-agent-step | L2 |
| Agent 生命周期 | per-agent | L3 |
| 执行授权 | per-tool/skill call | L4 |
| 全局事件 | 27+ 主题 | L5 |
| 业务 DAG | per-subgraph | GenerateSubgraphNode |

---

## 四、编排决策树：如何为一个应用场景选型

```
用户描述应用场景 (goal)
  │
  ├─ 需要跨 Agent 协作？ ──是──▶ 选 IAgentComposition (call/call_async/delegate)
  │                                 + CompositionPattern 注册各 agent
  │
  ├─ 需要多轮规划-执行-验证？ ──是──▶ 选 PlanExecuteLoop
  │
  ├─ 需要并行分支聚合？ ──────是──▶ 选 ForkJoinLoop + DomainWorkerPool
  │
  ├─ 需要简单工具调用？ ──────是──▶ 选 ReactLoop
  │
  ├─ 需要 LLM 计费/限流/合规？ ──是──▶ 选 DecoratorPattern (L0)
  │
  ├─ 需要工具拦截/审批？ ──────是──▶ 选 HookPattern (L1/L4) + IApprovalHandler
  │
  ├─ 需要事件审计/追踪/蒸馏？ ──是──▶ 选 BusPattern (L5)
  │
  ├─ 需要认知 agent 互相协调？ ──是──▶ 选 §十八 cognitive-cognitive 协调模式（sync-delegate / fan-out / hierarchical-plan / debate-round 配方 / stream-pipeline V2 占位）
  │
  └─ 需要动态 Agent 编排？ ────是──▶ 选 CompositionPattern (L3) + IAgentRegistry
```

---

## 五、应用场景映射：A/B/C 17 类 → 编排组合

> 数据源: `capability-application-map-2026-08.md` §三（17 类应用，A/B/C 三档）

### 5.1 🟢 A 类（现成可构建，6 类）

| 应用 | 编排组合 | 关键依赖 |
|------|----------|----------|
| **A1** 单 agent 工具调用 ReAct | ReactLoop + DecoratorPattern(CostTracking) + BusPattern(tool.audit.*) | LLM provider + ToolRegistry |
| **A2** 多轮对话 + 会话分支 + --fork | SessionManager + CLI --fork | Session 4-scope |
| **A3** 成本/合规/速率受限 LLM 应用 | PlanExecuteLoop + DecoratorPattern(CostTracking+Compliance+RateLimit) + HookPattern(L3 审批) | IBudgetController + Decorator 链 |
| **A4** 单进程并行任务聚合 | ForkJoinLoop + DomainWorkerPool + BusPattern | DomainWorkerPool + IInteractionBus |
| **A5** 多 LoRA / 多模型路由 | model_router plugin + SLM 路由 .so | CostModelRouterPolicy 等 |
| **A6** 工具调用审核（审批流）| ReactLoop + HookPattern(L1 tool pre approve) + BusPattern(tool.audit.denied) | ToolCoordinator + IApprovalHandler |

### 5.2 🟡 B 类（1-3 sprint 后可构建，7 类）

| 应用 | 编排组合 | 关键依赖 |
|------|----------|----------|
| **B1** Marketplace Agent 部署 | CompositionPattern(per-agent) + HookPattern + 数字签名 | T5 per-agent ToolRegistry |
| **B2** 跨进程多 agent 协作 | IAgentComposition(call_async/delegate) + ADR-0059 | T4 完整 AgentWorker |
| **B3** 真实分布式追踪 | BusPattern(llm/tool/agent/dsl.*) → OTel | T2 OTLP 客户端 |
| **B4** Streaming Agent | IAgentComposition(stream) + IGenerationStream | T6 ADR-0060 stream |
| **B5** MCP Server 形态 | CompositionPattern(暴露 agent) + stdio/HTTP/SSE | T7 ADR-0076 |
| **B6** Agent 蒸馏环境 | PlanExecuteLoop + DecoratorPattern(CaptureMode Training) + BusPattern(轨迹采集) | T14 行为回归 + T15 TrajectoryIR + IEvaluator |
| **B7** Agent 自进化基础 | GEPA/MCTS loop + HookPattern(set_evaluator) + BusPattern(gepa.*/mcts.*) + MutationGovernor | T19/T20 + ADR-0084 |

### 5.3 🟠 C 类（1-3 月后，4 类）

| 应用 | 编排组合 | 关键依赖 |
|------|----------|----------|
| **C1** 跨主机多 agent 联邦 | IAgentComposition(跨进程) + ADR-0077 gRPC data plane（**ADR-0077 Wave 4 descoped docs-only**, 重启须 Phase 6c 重评）| T9 ADR-0077 (待 Phase 6c 评估重启) |
| **C2** 自进化 agent | 综合编排: GEPA/MCTS + fine-tune + 行为回归门 + 蒸馏回流 | T22 + B6/B7 |
| **C3** WASM 沙箱多语言 agent | CompositionPattern + WASM runtime + Python PDK | T12/T13 ADR-0056/0065 |
| **C4** Cloud-native agent 服务 | CompositionPattern + 服务化（**ADR-0050 Candidate B 服务化 🔒 冻结**, 全部重开条件 0/6 满足; Phase 6c 重新评估）| T10 Phase 6c 重新评估 |

---

## 六、LLM 编排蓝图（V2+，非当前实现）

> 以下为**演进蓝图**，非已 ship 能力。当前 `CrossCuttingOrchestrator::dispatch()` 是**静态 JSON 分发**（V1 已 ship）。

### 6.1 场景智能体（Scenario Orchestrator Agent）

```
用户: "帮我运营一个成本敏感的多租户 agent 平台，需要审计 + 限流"
  ↓ ScenarioOrchestratorAgent.self_configure(goal)
{"goal": "cost_sensitive_multi_tenant", "constraints": ["audit", "rate_limit"]}
  ↓ LLM 动态生成 config
{"patterns": [CostTracking, RateLimit, hook(L3 approval), bus(mutation.* → SIEM)]}
  ↓ CrossCuttingOrchestrator.dispatch_with_context(config, runtime_ctx)
```

### 6.2 LLM 编排回调（I4）

`CrossCuttingOrchestrator` 增加 `llm_selector(ctx)` — LLM 根据 `{goal, tool_trace, budget_remaining, latency_ms}` 动态返回最适合的横切组合。

### 6.3 行为矩阵统一抽象（I7，V3）

将 Loop Agent（3 种）+ Cross-Cutting Pattern（4 种）+ Cognitive/Domain Worker 统一为 `IBehavior` 接口 — `BehaviorOrchestrator` 单入口编排所有行为。

### 6.4 编排反馈闭环（I8）

LLM 生成 DSL 图执行后 → Trace 回传 → 下一轮 `dispatch` 使用 → 自进化编排。

---

## 七、当前断链与改进路径（P0/P1/P2）

| 优先级 | 断链 | 位置 | 影响 | 改进 |
|--------|------|------|------|------|
| 🔴 P0 | **GenerateSubgraphNode 断链** | `node_executor.cpp:327` `g_current_engine->append_graphs()` 注释掉 | 业务 DSL 图 LLM 生成后无法动态注册 → 多 Agent 多目的子图不可能 | 恢复 `AppendGraphsCallback` 回调链 |
| 🔴 P0 | **无 generating_agent + purpose 元数据** | `__rendered_prompt__` 仅含渲染 prompt | 无法区分"哪个 Agent 生成何种目的的子图" | 注入 `{agent_id, purpose}` 到 prompt |
| 🟡 P1 | **dispatch() 单向静态** | `src/common/governance/cross_cutting/cross_cutting_orchestrator.cpp:39` | 无运行时状态感知 + 无反馈 | `dispatch_with_context(runtime_ctx)` + `llm_selector` |
| 🟡 P1 | **Hot-Reload 无反向取消** | 仅 enable，无 disable | 运行时模式切换需重启 | `disable_pattern(name)` + Pattern.undo() |
| 🟡 P1 | **Meta-Agent V2 deferred** | ADR-0085 §决策 "V1 不实施 Meta-Agent 自管理"，代码库零设计稿落盘（`cross_cutting_meta_agent.h` 不存在）| 场景智能体无法落地 | 先写设计稿（含 `self_configure(goal)` 契约），再实现；与 ADR-0086 信用分配契约协同（详见 §17 关联文档, 取代 self-evolution §七 #6 过期文件名 `adr-0085-credit-assignment-contract.md`）|
| 🔴 P2 | **Loop 未感知横切状态** | Loop 编译期实例化 | 无法按 loop phase 条件化横切 | `when: {loop_phase, budget_remaining}` 条件字段 |

---

## 八、关键不变量（Oracle B3 类，全编排层适用）

1. **契约层零修改**: 既有 21 个 contract 头文件只增不改（`include/agenticdsl/contract/`）
2. **编排与执行分离**: CognitiveWorker 负责生成/监控，不中转数据（`multi-domain-agent-architecture.md` §四）
3. **横切不侵入业务**: Pattern 通过注入通道，业务 code 零感知
4. **fail-safe 默认**: HookErrorPolicy (FailClosed/FailOpen) 贯穿所有 hook
5. **Loop/Pattern 正交**: 5 种编排体互相独立，按需组合
6. **命名空间卫生**: 新代码全部 `agenticdsl::` / `hydraforge::pdk::`

---

## 九、验证命令

```bash
# 1. 编排层组件存在性
ls include/agenticdsl/pdk/agent_loops/react_loop.h
ls include/agenticdsl/pdk/agent_loops/plan_execute_loop.h
ls include/agenticdsl/pdk/agent_loops/fork_join_loop.h
ls include/agenticdsl/pdk/cross_cutting/*.h
ls include/agenticdsl/cognitive/cognitive_worker.h
ls include/agenticdsl/cognitive/domain_worker_pool.h

# 2. 6 层横切接口存在性
ls include/agenticdsl/contract/i_llm_provider_decorator.h
ls include/agenticdsl/contract/itool_hook_registry.h
ls include/agenticdsl/contract/iagent_hook_registry.h
ls include/agenticdsl/contract/iagent_registry.h
ls include/agenticdsl/contract/iagent_composition.h
ls include/agenticdsl/policy/iapproval_handler.h
ls include/agenticdsl/contract/iinteraction_bus.h

# 3. 断链检测（修复后应返回 0）
grep -c "Placeholder.*append_graphs\|append_graphs.*Placeholder" src/modules/executor/node_executor.cpp

# 4. 编排测试
ctest -R "test_cross_cutting|test_hook_pattern|test_pdk|test_cognitive|test_domain" --output-on-failure

# 5. 已知场景映射验证（cap-map 17 类应用，每类在映射表各出现 1 次）
grep -cE "^\| \*\*[ABC][0-9]+\*\* " docs/architecture/capability-application-map-2026-08.md

# 6. Contract 头文件计数（应返回 21）
ls include/agenticdsl/contract/*.h | wc -l

# 7. GenerateSubgraphNode 工厂注册（应返回 1 行）
grep -c "register_factory.*generate_subgraph" src/modules/parser/node_factory.cpp

# 8. 自进化关键组件 ship 验证（应全部存在 + 状态翻转）
ls include/agenticdsl/cognitive/gepa_loop.h
ls include/agenticdsl/cognitive/mcts_workflow_search.h
ls include/agenticdsl/types/capture_mode.h
ls include/agenticdsl/contract/idistillation_writer.h
ls include/agenticdsl/contract/ievaluator.h

# 9. MutationGovernor + IEvaluator 代码 ship（应各 ≥1 命中）
grep -r "class MutationGovernor\|struct MutationGovernor" include/agenticdsl/ src/ 2>/dev/null | head -3
grep -r "class IEvaluator\|struct IEvaluator" include/agenticdsl/contract/ src/ 2>/dev/null | head -3

# 10. 已 ship 能力计数（cap-map §一 应返回 31）
grep -cE "^\| \*\*[0-9]+\*\* " docs/architecture/capability-application-map-2026-08.md | head -1
grep -m1 "31 项" docs/architecture/capability-application-map-2026-08.md

# 11. 17 类应用 × 8 横切矩阵（§十二）每行应用验证：grep 17 个应用代号各 ≥1
for app in A1 A2 A3 A4 A5 A6 B1 B2 B3 B4 B5 B6 B7 C1 C2 C3 C4; do
  grep -c "\\*\\*${app}\\*\\*" docs/architecture/agent-orchestration-architecture-2026-08.md | xargs -I{} echo "$app: {} 行"
done

# 12. 多领域文档同目录验证
ls docs/architecture/multi-domain-agent-architecture.md

# 13. ADR-0061-13 蒸馏输出 ship 状态
grep -m1 "^✅" docs/adr/skill/adr-0061-13-distillation-output-format.md

# 14. ADR-0050 Candidate B 服务化冻结状态（v1.3 Oracle 修正 #1+#2 验证）
grep "🔒" docs/adr/adr-0050-phase6-strategic-evaluation.md | head -3
grep -c "暂缓\|冻结" docs/adr/adr-0050-phase6-strategic-evaluation.md

# 15. ADR-0077 Wave 4 descoped 状态（v1.3 修正 #1 验证）
grep "Wave 4 descoped\|docs-only" docs/adr/adr-0077-grpc-data-plane.md | head -1

# 16. ADR-0086 信用分配待立项（v1.3 修正 #3 验证）
#     期望: 文档中提及 ADR-0086 (而非过期的 adr-0085-credit-assignment-contract.md)
grep -c "adr-0086-credit-assignment-contract\|adr-0086-信用" docs/architecture/agent-orchestration-architecture-2026-08.md
#     反向验证: 文档不应再使用过期文件名 adr-0085-credit-assignment-contract.md
grep -c "adr-0085-credit-assignment-contract" docs/architecture/agent-orchestration-architecture-2026-08.md

# 17. §五 C1/C4 标注 descoped/freeze 状态（v1.3 修正 #1 验证）
grep -c "Wave 4 descoped\|🔒 冻结" docs/architecture/agent-orchestration-architecture-2026-08.md

# 18. §十八 5 模式名齐全（v1.4 验证）
for pattern in "sync-delegate" "fan-out" "hierarchical-plan" "debate-round" "stream-pipeline"; do
  grep -c "**${pattern}**" docs/architecture/agent-orchestration-architecture-2026-08.md | xargs -I{} echo "$pattern: {}"
done

# 19. §十八 强制标注 stream-pipeline 为 V2 占位（Oracle Path 1 条件 #1）
grep -A1 "stream-pipeline.*V2 占位\|stream-pipeline.*🔴" docs/architecture/agent-orchestration-architecture-2026-08.md | head -3

# 20. §十八 debate-round 标为组合配方（Oracle Path 1 条件 #2）
grep -A1 "debate-round.*组合配方\|debate-round.*🟡" docs/architecture/agent-orchestration-architecture-2026-08.md | head -3

# 21. §四 决策树引用 §十八（v1.4 决策树分支验证）
grep "§十八\|cognitive-cognitive 协调模式" docs/architecture/agent-orchestration-architecture-2026-08.md | head -2

# 22. §十七关联文档引用 §十八（v1.4 交叉引用验证）
grep "agent-orchestration.*§十八\|本目录 §十八" docs/architecture/agent-orchestration-architecture-2026-08.md
```

---

## 十、关联文档

| 文档 | 关系 |
|------|------|
| `cross-cutting-hooks-architecture-2026-08.md` | 横切机制细节（6 层 + 4 Pattern + Orchestrator）|
| `multi-domain-agent-architecture.md` | 认知/领域协作细节（分层 + 服务方式,2026-08-30 从 `docs/guides/` 迁入同目录）|
| `docs/adr/adr-0085-cross-cutting-pattern-pdk.md` | 横切 Pattern PDK ADR |
| `docs/adr/adr-0081-pre-step-hook-contract.md` | Agent Hook ADR |
| `docs/adr/adr-0082-agent-first-class-registry.md` | IAgentRegistry ADR |
| `docs/adr/adr-0060-agent-composition.md` | Agent 组合 ADR |
| `docs/adr/adr-0020-thread-model-isolation.md` | CognitiveWorker/DomainWorkerPool ADR |
| `capability-application-map-2026-08.md` | 17 类应用场景矩阵 |
| `docs/specs/architecture.md` | AgenticOS 五层架构规范 (L0-L4 + R1-R5) |

---

## 十一、GenerateSubGraph 使用场景与流程分析

### 11.1 节点定义与代码定位

| 维度 | 内容 |
|------|------|
| **DSL 类型字符串** | `generate_subgraph`（`docs/specs/dsl.md:412`）— 规范用名 `llm_generate_dsl` 已弃用 |
| **C++ 类** | `agenticdsl::GenerateSubgraphNode`（`src/core/types/node.h:198`）继承自 `Node` |
| **Node 子类实现** | `src/modules/executor/node.cpp:190-204`（默认 `return ctx;`，实际逻辑委托给 NodeExecutor）|
| **Parser 工厂** | `make_generate_subgraph()` (`src/modules/parser/node_factory.cpp:228`) + `register_factory("generate_subgraph", …)` (`node_factory.cpp:328`) |
| **NodeExecutor 分发** | `execute_node()` 在 `node_executor.cpp:67` 通过 `dynamic_cast` 路由到 `execute_generate_subgraph()` |
| **执行函数** | `NodeExecutor::execute_generate_subgraph()` (`src/modules/executor/node_executor.cpp:265-343`, 79 行) |
| **回调契约** | `using AppendGraphsCallback = std::function<void(std::vector<ParsedGraph>)>` (`src/modules/executor/node_executor.h:41`) |

### 11.2 节点配置（YAML → JSON → Node）

```yaml
- id: generate
  type: generate_subgraph
  prompt_template: "Generate DSL for: {{ task }}"   # 必填,含 Inja 模板
  output_keys: ["generated_paths"]                  # 必填,存生成的子图路径
  signature_validation: strict                       # 可选: strict | warn | ignore, 默认 strict
  on_signature_violation: "/some/jump/path"          # 可选,strict 模式违规跳转
  next: ["/main/end"]                                # 后续节点
```

**关键字段语义**:
- `prompt_template`: LLM 输入，必须在 context 中可被 Inja 渲染（由 `ExecutionSession::inject_subgraphs_into_prompt()` 注入可用子图列表）
- `output_keys[0]`: 单图存 string 路径；多图存 string 数组
- `signature_validation`: `strict` 失败抛异常并跳 `on_signature_violation`；`warn` 仅 LOG_WARN 继续；`ignore` 完全跳过
- `/dynamic/<name>` 路径前缀：强制要求（图必须挂在 `/dynamic/` 命名空间，参见 `dsl.md:1187`）

### 11.3 调用链流程图

```
┌──────────────────────────────────────────────────────────────────────────┐
│ 1. DSL 解析期                                                            │
│    NodeFactoryRegistry::register_factory("generate_subgraph", …)          │
│       ↓                                                                  │
│    MarkdownParser::create_node_from_json(json)                            │
│       ↓                                                                  │
│    make_generate_subgraph(path, json)  [node_factory.cpp:228]              │
│       ↓                                                                  │
│    GenerateSubgraphNode{path, prompt, output_keys, next_paths}            │
└─────────────────────────────────┬────────────────────────────────────────┘
                                  ↓
┌──────────────────────────────────────────────────────────────────────────┐
│ 2. 图构建期                                                              │
│    TopoScheduler::build_dag() → DAG 含 GENERATE_SUBGRAPH 节点              │
│    ExecutionSession ctor (scheduler/execution_session.cpp:29)              │
│       ├─ append_graphs_callback → TopoScheduler::append_dynamic_graphs   │
│       └─ node_executor_->set_append_graphs_callback(cb) [line 39]         │
└─────────────────────────────────┬────────────────────────────────────────┘
                                  ↓
┌──────────────────────────────────────────────────────────────────────────┐
│ 3. 节点执行期  (scheduler 主循环发现 GENERATE_SUBGRAPH)                   │
│    ExecutionSession::execute_node (execution_session.cpp:210)              │
│       ├─ inject_subgraphs_into_prompt(gsn->prompt_template, ctx)          │
│       ├─ ctx["__rendered_prompt__"] = rendered                            │
│       └─ node_executor_->execute_node(node, new_ctx, yield_checker)       │
│              ↓                                                            │
│    NodeExecutor::execute_node → execute_generate_subgraph [line 67]        │
│       ↓                                                                  │
│    ① 校验 ctx["__rendered_prompt__"] 存在 [line 268-270]                │
│       ↓                                                                  │
│    ② llm_provider_->generate(req, {}) → Result<String,LLMError> [line 283]│
│       ↓                                                                  │
│    ③ parser_->parse(generated_dsl) → ParsedGraph [line 310]              │
│       ↓                                                                  │
│    ④ 过滤 /dynamic/ 前缀的图 [line 314]                                  │
│       ↓                                                                  │
│    ⑤ 可选签名验证 (strict/warn/ignore) [line 316-329]                    │
│       ↓                                                                  │
│    🔴 ⑥ 【断链】g_current_engine->append_graphs(…) 注释掉 [line 327]       │
│       └─ 本应触发: append_graphs_callback_ → TopoScheduler::              │
│          append_dynamic_graphs → dynamic_graphs_.push_back                │
│       ↓                                                                  │
│    ⑦ ctx[output_keys[0]] = dynamic_paths  [line 331-338]                  │
│       └─ (snapshot 由 ExecutionSession::execute_node 触发, 不在此处)       │
└─────────────────────────────────┬────────────────────────────────────────┘
                                  ↓
┌──────────────────────────────────────────────────────────────────────────┐
│ 4. 修复后应触发的动态图重建（🔴 当前未触发）                              │
│    TopoScheduler::execute() 主循环 [topo_scheduler.cpp:229]               │
│       ├─ if (!dynamic_graphs_.empty()) rebuild_dynamic_graph(state)        │
│       │     ├─ state.dynamic_graphs.assign(…)                            │
│       │     ├─ 克隆 dynamic 图节点到 all_nodes_                           │
│       │     └─ 重建 node_map_ + reverse_edges_ + in_degree_               │
│       └─ 后续调度可发现并执行新生成的节点                                  │
└──────────────────────────────────────────────────────────────────────────┘
```

### 11.4 关键不变量（执行时校验）

| # | 不变量 | 实现位置 | 备注 |
|---|--------|----------|------|
| I1 | `__rendered_prompt__` 必须在 ctx 中 | `node_executor.cpp:269` | 缺失抛 `runtime_error` |
| I2 | `llm_provider_` 必须可用 | `node_executor.cpp:290` | 缺失抛 `runtime_error` |
| I3 | 生成的子图路径必须以 `/dynamic/` 开头 | `node_executor.cpp:314` | 否则静默丢弃（无 callback 调用）|
| I4 | signature 验证模式严格区分 | `node_executor.cpp:316-329` | strict→throw, warn→log, ignore→skip |
| I5 | 输出必须写入 `output_keys[0]` | `node_executor.cpp:333-338` | 单图存 string, 多图存 array |
| I6 | snapshot 触发点在 ExecutionSession 层 | `execution_session.cpp:226-237` | 节点本身不触发 |
| I7 | Fork 分支内不允许 generate_subgraph 注入 | `archive/compiler/adr-0019-dynamic-graph-execution-model.md:108` | 文档约束,无代码强制 |

### 11.5 使用场景矩阵

| 场景 | 角色定位 | 关键依赖 | 落地状态 |
|------|----------|----------|----------|
| **S1 动态 DAG 生成（ReWOO/Plan-Execute）** | 规划器按子任务动态产出子图 | LLMPrompt 模板 + `{{ available_subgraphs }}` 注入 | 🔴 受 P0 断链阻塞 |
| **S2 IPER 循环** | Plan→Execute→Reflect 每轮生成新计划 | `archive/adr/adr-0015-iper-loop.md` | 🔴 同上 |
| **S3 Skill JIT 编译（历史方案）** | SKILL.md → 编译产物 → 动态子图 | `archive/compiler/adr-0021-skill-compiler-architecture.md` | ⛔ Archived,被 SkillCompiler V1 取代（ADR-0061-03,2026-08-27）|
| **S4 Bootstrap Loader（历史方案）** | `native`/`source` 两种加载模式都依赖 generate_subgraph | `archive/compiler/spec-bootstrap-loader.md` | ⛔ Archived, 2026-08-24 |
| **S5 Agent 自进化（T19 GEPA Phase 2）** | 反思循环生成子 DSL 候选 | ADR-0084 MutationGovernance + IEvaluator | 🟡 部分（契约已 ship, generate_subgraph 集成待 §7 P0 修复）|
| **S6 LLM-native DSL Authoring（ADR-0071）** | LLM 作为 DSL 作者,多 Agent 多目的子图 | 需要 `generating_agent + purpose` 元数据 | 🔴 同 P0 #2 阻塞（§七 P0 行 230）|
| **S7 实测覆盖** | `test_executor_with_mock_provider.cpp:39-67` | MockLLMProvider 返回固定 DSL | 🟡 Mock 路径返回 success 但未注册到 scheduler |

> **零生产代码 ship 的真实用例**: 截至 2026-08-30,`examples/` 和 `lib/` 目录中 `grep -l "type: generate_subgraph" examples/ lib/` 仅命中 1 个文件（测试代码 + 文档示例片段），无业务 DSL 使用该节点类型。原因是 P0 断链未修复，动态子图注册路径不通。

### 11.6 修复路径建议（衔接 §七 P0）

**最小修复（恢复动态子图注册能力）**:

```cpp
// src/modules/executor/node_executor.cpp:327 — 替换注释行
if (append_graphs_callback_) {
    std::vector<ParsedGraph> to_register;
    to_register.push_back(std::move(graph));
    append_graphs_callback_(std::move(to_register));
} else {
    LOG_WARN("GenerateSubgraphNode: append_graphs_callback_ not set, graph "
             << graph.path << " dropped");
}
```

**配套修复（§七 P0 #2 元数据）**:
在 `ExecutionSession::execute_node`（line 210 附近）注入 `{generating_agent, purpose}` 到 `__rendered_prompt__`，或新增 `__generating_agent__` / `__purpose__` 字段。

**验收标准**（修复后 §九 #3 应返回 0）:
- `grep -c "Placeholder.*append_graphs\|append_graphs.*Placeholder" src/modules/executor/node_executor.cpp` → `0`
- 新增回归测试: mock LLM 返回 `/dynamic/test/foo` 子图 → 执行后调度器实际派发新节点

### 11.7 关联接口/ADR 交叉引用

| 引用 | 文件 | 关系 |
|------|------|------|
| ADR-0001 ILLMProvider | `docs/adr/adr-0001-illm-provider-streaming-interface.md` | 提供 `generate()` 调用接口 |
| ADR-0008 Structured Context | `docs/adr/adr-0008-structured-context.md` | `__rendered_prompt__` 等元数据字段约束 |
| ADR-0009 DSL Standard Library | `docs/adr/adr-0009-dsl-standard-library.md` | 节点类型定义源（line 160） |
| ADR-0071 LLM-native AgenticDSL | `docs/adr/adr-0071-llm-native-agenticdsl-architecture.md` | `generating_agent + purpose` 元数据需求 |
| ADR-0079 Session 4-Scope | `docs/adr/adr-0079-unified-session-4scope.md` | "嵌套语义:GenerateSubgraphNode 内部触发新 execution,带 parent_graph_execution_id" |
| ADR-0080 AppendOnlyEventLog v1.1 | `docs/adr/adr-0080-append-only-event-log.md` | `GenerationRequest.purpose = "generate_subgraph"` |
| ADR-0082 Agent First-Class Registry | `docs/adr/adr-0082-agent-first-class-registry.md` | "LLM 调用点共 8 处（含 GenerateSubgraphNode）"|
| DSL v3.10 规范 | `docs/specs/dsl.md:410-450` | 用户可见规范用名 + YAML 示例 + 权限 |
| STDLIB v3.10 | `docs/specs/stdlib-v3.10.md:769-773` | "动态子图生成原语" |
| 历史 ADR-0019 Dynamic Graph Model | `docs/archive/compiler/adr-0019-dynamic-graph-execution-model.md` | 5 种 dynamic graph 注入方案对比 (A/B/C/D/E), 选 E 增量修复 |

---

## 十二、横切功能矩阵 — 横切在应用场景中的赋能全景

> **本节核心观点**: 横切不是单一能力,而是**一组正交能力**的组合,通过 6 层抽象 + 4 PDK Pattern + Orchestrator 统一编排。在 17 类应用场景中,不同的应用会激活不同的横切子集。

### 12.1 8 类横切能力清单

| 横切能力 | 实现位置 | 主要拦截层 | 关键 ADR |
|----------|----------|------------|----------|
| **H1 计费 (Cost Tracking)** | `CostTrackingDecorator` (L0) | L0 | ADR-0080 D10 + CostTrackingDecorator ✅ Ship |
| **H2 合规 (Compliance)** | `ComplianceDecorator` (L0) hash-only | L0 | ADR-0080 D10 PII 约束 ✅ Ship |
| **H3 限流 (Rate Limit)** | `RateLimitDecorator` (L0) + `ToolHook` (L1) | L0 / L1 | ADR-0069 ✅ Ship |
| **H4 审计 (Audit)** | `BusPattern` 订阅 `mutation.*` + `tool.audit.*` | L5 | ADR-0068 ✅ Ship + ADR-0084 mutation.* ✅ |
| **H5 审批 (Approval)** | `IApprovalHandler` + `HookPattern target=approval` | L1 + L4 | ADR-0031 ✅ Ship + ADR-0069 M2 |
| **H6 PII 脱敏 (Scrub)** | `AgentHook` per-agent pre-hook + CaptureMode 联动 | L2 + L0 | ADR-0080 v1.2 D10 ✅ + ADR-0081 ✅ |
| **H7 可观测 (Observability)** | `BusPattern` 订阅 `cognitive.task.*` + `domain.task.*` + `dsl.call.*` | L5 | ADR-0068 (27+ 主题) ✅ Ship |
| **H8 重试 + 缓存 (Retry/Cache)** | 自定义 `Decorator` (L0) + `ToolHook post` | L0 + L1 | ADR-0085 ✅ Approved, 链深硬约束 ≤4 |

### 12.2 17 类应用场景 × 横切能力激活矩阵

| 应用 | H1 计费 | H2 合规 | H3 限流 | H4 审计 | H5 审批 | H6 PII | H7 可观测 | H8 重试 |
|------|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **A1** 单 agent 工具调用 ReAct | ✅ | ◻ | ◻ | ✅ tool.audit.* | ◻ | ◻ | ✅ | ✅ |
| **A2** 多轮对话 + 会话分支 | ◻ | ✅ | ◻ | ✅ | ◻ | ✅ session | ✅ | ◻ |
| **A3** 成本/合规/速率受限 LLM 应用 | ✅ CostTracking+Compliance+RateLimit | ✅ | ✅ | ✅ | ✅ L3 hook | ✅ | ✅ | ✅ |
| **A4** 单进程并行任务聚合 | ◻ | ◻ | ◻ | ◻ | ◻ | ◻ | ✅ domain.task.* | ◻ |
| **A5** 多 LoRA / 多模型路由 | ✅ per-token | ◻ | ✅ rate-limit | ◻ | ◻ | ◻ | ✅ model.* | ✅ |
| **A6** 工具调用审核（审批流） | ◻ | ✅ | ◻ | ✅ tool.audit.denied | ✅ L3 二次审批 | ◻ | ✅ | ◻ |
| **B1** Marketplace Agent 部署 | ✅ per-tenant | ✅ | ✅ | ✅ marketplace.audit | ✅ 数字签名 | ✅ | ✅ | ✅ |
| **B2** 跨进程多 agent 协作 | ✅ | ✅ | ✅ | ✅ cross-process.audit | ✅ | ✅ | ✅ | ✅ |
| **B3** 真实分布式追踪 (OTel) | ◻ | ◻ | ◻ | ✅ OTel sink | ◻ | ◻ | ✅ OTLP | ◻ |
| **B4** Streaming Agent | ✅ token | ◻ | ✅ | ◻ | ◻ | ◻ | ✅ stream.* | ◻ |
| **B5** MCP Server 形态 | ✅ | ✅ | ✅ | ✅ mcp.audit | ✅ L3 tool | ✅ | ✅ | ✅ |
| **B6** Agent 蒸馏环境 | ◻ | ✅ hash-only | ◻ | ✅ distill.audit | ✅ L1 mutation | ✅ CaptureMode | ✅ trajectory.* | ◻ |
| **B7** Agent 自进化基础 | ✅ reflect cost | ✅ | ◻ | ✅ mutation.* (4主题) | ✅ L3 mutation (ADR-0084) | ✅ D10 decouple | ✅ gepa.* / mcts.* | ◻ |
| **C1** 跨主机多 agent 联邦 | ✅ | ✅ | ✅ | ✅ grpc.audit | ✅ | ✅ | ✅ OTel | ✅ |
| **C2** 自进化 agent (fine-tune) | ✅ eval cost | ✅ | ◻ | ✅ | ✅ | ✅ | ✅ | ◻ |
| **C3** WASM 沙箱多语言 agent | ✅ wasm cost | ✅ | ✅ | ✅ wasm.audit | ✅ L3 | ✅ | ✅ | ✅ |
| **C4** Cloud-native agent 服务 | ✅ per-tenant | ✅ | ✅ K8s | ✅ k8s.audit | ✅ | ✅ | ✅ OTel | ✅ |

**图例**: ✅ 必须激活 ◻ 可选 ◼ 不适用

### 12.3 横切配置 DSL 实例（类比 Agent DSL）

每个应用场景通过 1 份 `examples/cross_cutting/dsl/<mode>.cc.md` + 1 次 `orch.dispatch(config.to_json())` 激活横切子集。例如 B6 蒸馏环境的横切配置:

```yaml
### AgenticDSL `/cross_cutting/distillation`
patterns:
  - type: decorator-v1
    config:
      decorators: ["Compliance"]      # H2 合规 (hash-only, ADR-0080 D10)
  - type: hook-v1
    config:
      hooks:
        - target: tool
          glob: "capture/*"
          type: pre
          priority: 100
          policy: FailOpen             # H6 PII scrub (fail-open 由 ADR-0080 v1.2 解锁)
        - target: approval
          glob: "L3_*"                 # H5 审批门
          type: pre
          priority: 1000
          policy: FailClosed
  - type: bus-v1
    config:
      subscriptions: ["mutation.*", "capture_mode.*", "trajectory.*"]  # H4 审计 + H7 可观测
      handler: external-siem-adapter-v1
```

> **设计哲学**: 应用开发者**只需选择场景标签**(A1-A6/B1-B7/C1-C4),预定义 goal 配置自动激活对应横切子集; 不需要从零构造 8 类横切配置。

---

## 十三、Agent = Loop — 编排哲学与 Loop 分类

### 13.1 核心哲学: Agent 是 Loop 的运行时实例

```
Agent = (Loop 实例) + (Worker 载体) + (Skill DSL) + (Policy/横切 装饰)
                       ↑              ↑              ↑
                       │              │              │
                  ReactLoop /     Cognitive /      ADR-0085
                  PlanExecute /   Domain            4 Pattern
                  ForkJoin
```

**不变量**: 在 HydraForge 中, 一个 **Agent 实例** = 一个 **Loop class 实例** (React/PlanExecute/ForkJoin) + 一个 **Worker 角色绑定** (Cognitive 或 Domain) + 一份 **可选 SKILL DSL** + 一组 **横切装饰**。这个映射关系是 ADR-0021 PDK 设计与 ADR-0082 Agent First-Class Registry 共同确立的"对等哲学":

| PDK Loop Agent（已 ship） | 角色绑定 (Worker 角色) | 横切装饰 |
|--------------------------|----------------------|----------|
| `class ReactLoop` (Sprint 4) | Cognitive (per-agent DSLEngine) or Domain (per-domain capability) | 4 Pattern 正交叠加 |
| `class PlanExecuteLoop` (Sprint 20) | Cognitive (规划+反思) or Domain (执行) | 同上 |
| `class ForkJoinLoop` (Sprint 20) | Cognitive (分支调度) or Domain (并行执行) | 同上 |

**关键洞察** (与 PDK Loop Agent 的对等映射):

```
3 个独立 Loop class (React/PlanExecute/ForkJoin)
  ↕ 编译期分发 (LoopDispatcher<LoopType> 模板特化)
  ↕ 运行时实例化 (CognitiveWorker / DomainWorkerPool)
  ↕ 运行时横切 (CrossCuttingOrchestrator 4 Pattern)
```

### 13.2 3 Loop × 5 编排体 × 横切层 全景组合

| Loop \ 编排体 | ReactLoop | PlanExecuteLoop | ForkJoinLoop |
|--------------|-----------|-----------------|--------------|
| **Cognitive Worker** | 单 agent 意图 ReAct (A1) | 反思循环规划-执行-验证 (B7 GEPA) | 反思候选并行生成 (T20 MCTS) |
| **Domain Worker** | 单领域工具调用 (A1) | 多阶段计划分阶段执行 (A3) | 多领域并行任务 (A4 Fork-Join) |
| **IAgentComposition** | ReactLoop → 委派给 ForkJoinLoop | PlanExecuteLoop → 委派给 ReactLoop | ForkJoinLoop → 各分支调用 ReactLoop |
| **横切 Loop** | H4/H5 (审计 + 审批) | H1/H2 (计费 + 合规) | H3 (限流) |
| **横切 Pattern** | L2 AgentHook pre-step | L0 Decorator (CostTracking) | L1 ToolHook pre-execute |

**Loop × Pattern 正交性**: 任何 Loop 可叠加任何 Pattern,无需修改 Loop 实现。这是 ADR-0085 设计哲学的核心:**横切不侵入业务**。

### 13.3 Loop 与 Skill DSL 的运行时关系

```
SKILL.md (静态资产)        Loop (运行时载体)
─────────────────         ──────────────────
signature:                run(prompt, ctx, token)
permissions:              → SimpleCognitiveOrchestrator 或 ReactLoop
implementation:           → loop body (state machine)
                          → 每 step 调用 tool_call / dsl_call
```

**SKILL.md 是"Loop 的 DSL 描述"**: Loop 是"运行时机制", SKILL.md 是"领域知识封装"。两者正交但通过 `prompt_template` + `output_keys` 关联。这与 cap-map §八 G10-G15 的"教师→学生"蒸馏路径一致: 学生可以是更简单的 Loop + 蒸馏出的 SKILL.md。

---

## 十四、Cognitive 指导 Domain 的功能矩阵生态

> **核心命题**: Cognitive Worker 不是数据中转者(per `multi-domain-agent-architecture.md` §四),但它是 **Domain Worker 的"知识喂入者"和"质量评判者"**。这一关系通过 4 个具体机制落地,形成完整的"指导生态"。

### 14.1 4 个指导机制 (机制级)

| 机制 | Cognitive 提供 | Domain 接收 | 代码锚点 |
|------|----------------|------------|----------|
| **M1 子图/可用 skill 注入** | `inject_subgraphs_into_prompt(prompt_template, ctx)` 将当前可用 subgraphs 列表拼入 prompt | Domain Worker 在执行 `tool_call` / `dsl_call` 时 LLM prompt 上下文含可用 skill | `execution_session.cpp:212-215` (GenerateSubgraphNode 路径) |
| **M2 评估信号注入** | `set_evaluator(shared_ptr<IEvaluator>)` 注入评估器 | Domain Worker 在 `set_evaluator` 后, 每个 task 完成时 emit `evaluation.result` 事件 | `cognitive_worker.h:137` + `domain_worker_pool.h:207` + `contract/ievaluator.h` |
| **M3 反思循环驱动** | Cognitive Worker 在 ReAct 失败时生成反思候选 | Domain Worker 提供失败 trajectory 给 Cognitive 反思 | T19 GEPALoop (`cognitive/gepa_loop.h` + `commit()` via `MutationGovernor`) |
| **M4 技能编译输出** | Cognitive Worker 通过 `SkillCompiler` 编译 SKILL.md | Domain Worker 的工具实现可被编译产物替换 (T17) | `cognitive/skill_compiler.h` ✅ Ship 2026-08-27 |

### 14.2 Cognitive × Domain × 横切能力 功能矩阵

下表展示 4 指导机制如何与 8 类横切能力叠加, 在 17 类应用中的功能矩阵生态:

| 应用 | M1 子图注入 | M2 评估注入 | M3 反思 | M4 编译 | + H1 计费 | + H5 审批 | + H6 PII |
|------|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| A1 ReAct | ◻ | ◻ | ◻ | ◻ | ✅ | ◻ | ◻ |
| A3 受限 LLM 应用 | ✅ | ✅ | ◻ | ◻ | ✅ | ✅ | ✅ |
| A4 Fork-Join 聚合 | ✅ | ✅ | ◻ | ◻ | ◻ | ◻ | ◻ |
| **B6 蒸馏环境** | ✅ | ✅ (核心!) | ◻ | ✅ | ◻ | ✅ mutation | ✅ |
| **B7 自进化基础** | ✅ | ✅ (IEvaluator V2 Composite) | ✅ (GEPA 反思) | ✅ (SkillCompiler) | ◻ | ✅ ADR-0084 | ✅ D10 |
| **C2 自进化 agent** | ✅ | ✅ | ✅ (GEPA + MCTS) | ✅ | ✅ | ✅ | ✅ |

### 14.3 蒸馏路径: 4 机制协同工作流

```
┌────────────────────────────────────────────────────────────────────┐
│ 阶段 0: 运行采集                                                    │
│  LLM call → EventLog (D10 CaptureMode=Training) → trajectory 事件     │
│  指导机制: M1 (子图注入使 LLM 知道可用 skill), H6 (PII scrub)         │
├────────────────────────────────────────────────────────────────────┤
│ 阶段 1: 评估信号                                                    │
│  trajectory → IEvaluator.evaluate(task_id, trace) → evaluation.result │
│  指导机制: M2 (Cognitive/Domain 共享同一 IEvaluator)                   │
├────────────────────────────────────────────────────────────────────┤
│ 阶段 2: 蒸馏输出                                                    │
│  IEvaluator verdict + trajectory → IDistillationWriter.write_record()│
│  → FileDistillationWriter → trajectory.jsonl / policy.jsonl / meta.json│
│  涉及: ADR-0061-13 ✅ Ship 2026-08-29 (cap-map §一 #31)               │
├────────────────────────────────────────────────────────────────────┤
│ 阶段 3: 学生训练 + 回归门                                            │
│  policy.jsonl → LoRA / SFT (外部 AgenticMind) → 学生模型              │
│  学生输出 → BehavioralRegressionGate (T14 ✅) → 通过/拒绝             │
│  指导机制: H5 (审批门禁, L3 mutation), H4 (mutation.audit)            │
└────────────────────────────────────────────────────────────────────┘
```

### 14.4 反思路径: M3 驱动的闭环 (B7 应用已 ship)

```
失败轨迹 → T19 GEPALoop.collect_traces()
  → GEPALoop.reflect() → PromptEdit 候选 (Pareto 评估)
    → MutationGovernor.authorize() (H5 审批 + ADR-0084)
      → BehavioralRegressionGate (T14) → 通过
        → commit() → mutation.committed 事件 (H4 审计)
          → 新 prompt 注入到 Loop (H1 重新计费) 
            → 下一轮训练采集
```

**关键不变量 (来自 self-evolution-architecture §二)**:
- 候选生成与候选提交分离
- 未通过授权、评估、回归门禁的候选不得改变运行时资产
- 默认最小权限 (外部输入不能直接触发自修改)
- 在线教师蒸馏属于训练期, 与 serving 路径隔离

---

## 十五、编排蒸馏学习 — Orchestration Distillation

> **蒸馏不是独立功能, 而是编排层 + 横切层 + 评估层 + 存储层协同的端到端闭环。** 本节给出 7 环蒸馏闭环的横切 + 编排视角。

### 15.1 蒸馏 7 环闭环(横切编排视角)

| 环 | 步骤 | 主要角色 | 横切能力 | 落地状态 |
|----|------|----------|----------|----------|
| **环 1** | LLM 调用产生轨迹 | Domain Worker (ReactLoop) | L0 TracingDecorator (H7) | ✅ ADR-0001 + TracingDecorator |
| **环 2** | 事件写入 EventLog | Domain Worker | D10 CaptureMode=Training (H6) | ✅ ADR-0080 v1.2 amendment |
| **环 3** | Trajectory IR 序列化 | (独立视图) | (无业务侵入) | ✅ ADR-0061-06 v1.1 + T15 ship 2026-08-27 |
| **环 4** | IEvaluator 评估 | Cognitive Worker (M2 注入) | L2 AgentHook (set_evaluator) | ✅ ADR-0083 + 12 cases |
| **环 5** | DistillationRecord 写入 | Cognitive Worker | H6 payload redact hash-only | ✅ ADR-0061-13 + 6 cases |
| **环 6** | 学生模型训练 (外部) | (外部 AgenticMind) | — | ⏳ ADR-0078 事件驱动未排期 |
| **环 7** | 行为回归门验证 | (T14) | L1 ToolHook | ✅ T14 6 cases |

**已 ship 闭环**: 环 1-5 + 环 7 (6/7); 环 6 属于训练基础设施, 由外部系统承担。

### 15.2 编排蒸馏的 3 种应用变体

#### 变体 1: 教师-学生异步蒸馏 (B6 完整 ship 后可用)
- 教师 = CognitiveWorker + 完整横切链 (H1/H2/H5/H6 全开)
- 学生 = 更简单 Loop (ReactLoop) + 蒸馏出的 SKILL.md
- 横切配置: 蒸馏模式 `.cc.md` (见 §12.3)
- 评估: IEvaluator V2 (BehavioralEquivalence + Composite)

#### 变体 2: 在线反思改进 (B7 已 ship)
- 不需要训练学生模型
- Cognitive 通过 GEPALoop 反思失败轨迹 → 修订 prompt
- 横切能力: H4 mutation.* + H5 审批 + 行为回归
- 见 self-evolution §六 S1-S2 阶段

#### 变体 3: 工作流搜索 (T20 AFlow 已 ship)
- CognitiveWorker 通过 MCTSWorkflowSearch 自动发现新工作流
- 5 轴模板实例化 + UCB1 选择 + IEvaluator 加权
- 横切能力: H4 mcts.* + H5 + H1 评估成本
- 与 B7 互补: B7 改 prompt, T20 改结构

### 15.3 蒸馏数据安全边界

来自 ADR-0061-13 §决策 5 + ADR-0080 v1.2 amendment:
- `input` 字段 ≤ 64KB (与 ADR-0080 D10.4 一致)
- `output` 字段 ≤ 1MB
- `DistillationRecord.input + output` ≤ 1.5MB
- 必须 `CaptureMode::Training` 模式
- payload redact: hash-only (D10.2 v1.2.6, T21 ship 2026-08-28)
- 调用方负责 PII scrub (V1 手动, V2 接入 ADR-0081)

---

## 十六、自进化全景 — 编排驱动的自进化

> **核心边界 (self-evolution §一)**: 当前定义的是"受治理的单编排器自进化闭环", 不是多智能体协同进化平台。本节聚焦已 ship 能力 + 编排驱动视角。

> **Phase 6 战略对齐 (2026-08-30 Oracle 评审修正)**: 本节描述的自进化路径 **不属于 Phase 6 服务化轨道**。Phase 6 战略已由 ADR-0050 Solo Dev 重估确认 Candidate B 服务化 **🔒 冻结**（全部重开条件 0/6 满足）, 实际主线 = PDK 生产化 + AgentForge MVP。本节落地的 B7 自进化应用 (`T19 GEPA Phase 2` + `T20 MCTSWorkflowSearch` + `T21 Prompt Evidence Gate`) 全部归属 PDK 生产化轨道, 不依赖服务化。S3/S4 阶段 (蒸馏训练期 + Agent-Agent 协同) 需待 `adr-0086-credit-assignment-contract.md` (待立项, 取代 self-evolution §七 #6 已过期的 `adr-0085-` 文件名) ship 后方可推进。

### 16.1 已 ship 的自进化基础 (cap-map §一 #27-#30)

| 能力 | 落地点 | ship 日期 | 在自进化闭环中的角色 |
|------|--------|-----------|---------------------|
| **#27 GEPALoop V1** (T19) | `include/agenticdsl/cognitive/gepa_loop.h` | 2026-08-27 | 反思循环 (S1 阶段) |
| **#28 Prompt Evidence Gate** (T21) | `include/agenticdsl/policy/prompt_evidence_gate.h` | 2026-08-28 | Prompt 质量门控 (Go/No-Go) |
| **#29 MCTSWorkflowSearch V1** (T20) | `include/agenticdsl/cognitive/mcts_workflow_search.h` | 2026-08-28 | 工作流结构搜索 (S2 阶段) |
| **#30 Cross-Cutting Pattern PDK V1** | `include/agenticdsl/pdk/cross_cutting/*` | 2026-08-28 | 横切能力作为变异对象 |
| **#31 Distillation Data Plane V1** | `include/agenticdsl/types/capture_mode.h` + `idistillation_writer.h` | 2026-08-29 | 蒸馏数据采集 + 输出 |

### 16.2 编排层在自进化中的角色分工

```
┌──────────────────────────────────────────────────────────────────────────┐
│  自进化闭环 (self-evolution §三)                                            │
│                                                                          │
│  ┌────────────────┐    ┌────────────────┐    ┌────────────────────────┐  │
│  │ 观测            │───▶│ 评估信号        │───▶│ 候选生成                │  │
│  │ 角色: Domain    │    │ 角色: IEvaluator │    │ 角色: Cognitive (M3)   │  │
│  │ 落地: EventLog  │    │ (M2 注入)        │    │ 落地: GEPALoop/MCTS    │  │
│  │ + CaptureMode   │    │ ADR-0083 ✅      │    │ T19/T20 ✅              │  │
│  └────────────────┘    └────────────────┘    └────────────────────────┘  │
│         │                                               │                │
│         │              ┌────────────────────────┐         │                │
│         └─────────────▶│ 行为回归 + 审批门禁     │◀────────┘                │
│                        │ 角色: 横切 H5 + T14    │                          │
│                        │ ADR-0084 ✅             │                          │
│                        │ ADR-0061-02 ✅          │                          │
│                        └────────────────────────┘                          │
│                                       │                                   │
│                                       ▼                                   │
│                        ┌────────────────────────┐                          │
│                        │ 提交 + 审计              │                          │
│                        │ 角色: 横切 H4 mutation.*│                          │
│                        │ ADR-0068 ✅             │                          │
│                        └────────────────────────┘                          │
└──────────────────────────────────────────────────────────────────────────┘
```

**关键洞察**: 自进化闭环是**编排层 (Cognitive/Domain 横切) + 评估层 (IEvaluator) + 横切层 (4 Pattern) + 治理层 (ADR-0084)** 的协同结果, 不是某个单一组件的功能。

### 16.3 4 阶段自进化路径 (self-evolution §六)

| 阶段 | 目标 | 编排层参与 | 允许的自动化 | 当前状态 |
|------|------|------------|--------------|----------|
| **S0 证据闭环** | EventLog → Trajectory IR → IEvaluator → 回归报告 | Domain 采集 + Cognitive 评估 | 只读抽取 | ✅ 大部分 ship |
| **S1 反思候选** | GEPA 反思生成候选 | Cognitive (M3) | 只读生成, 不提交变异 | ✅ T19 GEPALoop ship |
| **S2 受治理变异** | 显式授权提交 | Cognitive + 横切 H5 + ADR-0084 | Prompt/Skill/DSL 分级提交 | ✅ ADR-0084 + MutationGovernor ship |
| **S3 训练期蒸馏** | 教师-学生异步蒸馏 | Cognitive + Domain (B6) | 训练环境内门控吸收 | 🟡 部分 (环 1-5 + 7 ship, 环 6 待外部) |
| **S4 协同进化** | Agent-Agent/Environment | (待定) | 仅 spike/沙箱 | 🔍 研究阶段 |

### 16.4 当前允许的最小闭环 + 禁止的自动行为

**允许的最小闭环** (self-evolution §五):
1. 读取已授权的事件/会话证据 ✅
2. 由 IEvaluator 产生可解释评估 ✅
3. 生成 prompt/skill/DSL 候选 ✅
4. 在只读或显式审批模式下进行行为回归 ✅
5. 输出候选、评估和拒绝原因, 默认不自动提交 ✅

**当前禁止的自动行为**:
- ❌ 外部输入直接修改 prompt、skill、DSL 或权重
- ❌ 未通过独立回归和授权的候选热加载
- ❌ 默认 serving 路径常驻教师模型或在线改变权重
- ❌ 以单次成功、相对胜负或预测误差直接触发提交
- ❌ 在没有信用分配和对照基线时宣称多主体能力提升

### 16.5 编排层在自进化中的横切配置示例

```yaml
### AgenticDSL `/cross_cutting/self_evolution`
patterns:
  - type: decorator-v1
    config:
      decorators: ["Compliance", "CostTracking"]   # H2 + H1: 合规 + 计费
  - type: hook-v1
    config:
      hooks:
        - target: approval                          # H5: 变异必须经审批 (ADR-0084)
          glob: "L3_mutation_*"
          type: pre
          priority: 1000
          policy: FailClosed
        - target: agent                              # H6: PII scrub per-agent
          glob: "react-loop/*"
          type: pre
          priority: 500
          policy: FailClosed
          handler: privacy-policy-v1
  - type: bus-v1
    config:
      subscriptions: ["mutation.*", "gepa.*", "mcts.*", "evaluation.result"]  # H4 + H7
      handler: external-evolution-audit-v1
```

---

## 十七、关联文档

| 文档 | 关系 |
|------|------|
| `cross-cutting-hooks-architecture-2026-08.md` | 横切机制细节（6 层 + 4 Pattern + Orchestrator；ADR-0085 §决策 5 V1 不实施 MetaAgent）|
| `multi-domain-agent-architecture.md` | 认知/领域协作细节（分层 + 服务方式，2026-08-30 从 `docs/guides/` 迁入同目录）；cognitive-cognitive 协调见本目录 §十八 |
| `self-evolution-architecture-2026-08.md` | 自进化与协同进化架构（含受治理单编排器闭环边界 + 4 阶段路径）|
| `capability-application-map-2026-08.md` | 17 类应用场景矩阵 + 31 项已 ship 能力 + G10-G15 闭环状态 |
| `agent-evolution-pipeline.md` | Agent 四阶段进化管线 (SKILL→DSL→C++→Wasm, ADR-0061 设计附件) |
| `docs/adr/adr-0085-cross-cutting-pattern-pdk.md` | 横切 Pattern PDK ADR |
| `docs/adr/adr-0081-pre-step-hook-contract.md` | Agent Hook ADR |
| `docs/adr/adr-0082-agent-first-class-registry.md` | IAgentRegistry ADR |
| `docs/adr/adr-0060-agent-composition.md` | Agent 组合 ADR |
| `docs/adr/adr-0020-thread-model-isolation.md` | CognitiveWorker/DomainWorkerPool ADR |
| `docs/adr/adr-0083-evaluator-reward-contract.md` | IEvaluator 契约（自进化评估层）|
| `docs/adr/adr-0084-mutation-governance-contract.md` | MutationGovernance 契约（自进化变异治理）|
| `docs/adr/skill/adr-0061-13-distillation-output-format.md` | 蒸馏输出格式（IDistillationWriter + DistillationRecord）|
| `docs/adr/skill/adr-0061-09-gepa-loop.md` | GEPA 反思循环 ADR（注：文件状态滞后, 实际 T19 GEPALoop 已 ship 2026-08-27）|
| `docs/adr/skill/adr-0061-08-aflow-search.md` | AFlow MCTS 工作流搜索 ADR（T20 MCTSWorkflowSearch ✅ Ship）|
| `docs/adr/adr-0050-phase6-strategic-evaluation.md` | Phase 6 战略评估 ✅ Approved；**Candidate B 服务化 🔒 冻结** (重开条件 0/6), Phase 6 实际主线 = PDK 生产化 + AgentForge MVP |
| `docs/adr/adr-0077-grpc-data-plane.md` | gRPC Data Plane 🔍 Proposed + **Wave 4 descoped docs-only**; C1 跨主机联邦应用依赖此 ADR 重启 |
| `docs/adr/adr-0086-credit-assignment-contract.md` | **待立项**（取代 self-evolution §七 #6 已过期的 `adr-0085-` 文件名；0085 已被横切 Pattern PDK 占用）— 信用分配契约, S4 协同进化前置 |
| `docs/specs/architecture.md` | AgenticOS 五层架构规范 (L0-L4 + R1-R5) |

---

## 十八、Cognitive-Cognitive 协调模式目录（Pattern Catalog）

> **本节目的是把散落在 §2.5 / §13.2 / §十六.2 中的 cognitive-cognitive 协调原语**集中命名**为可识别模式**, 为决策树新增"认知 agent 互相协调?"分支（§四）提供目录。
>
> **与 ADR-0085 §决策 5 的关系**: ADR-0085 已决议 **V1 不实施 Meta-Agent 自管理**; 本节是该决议的**需求消化层**（命名 + 示例 + 模式目录），**不是组件前置设计**。任何 MetaAgent / CrossCuttingMetaAgent / CognitiveMetaAgent 实施须先经 ADR-0085 V2 amendment 流程。
>
> **三个强制标注**（Oracle Path 1 条件）:
> 1. `stream-pipeline` **🔴 V2 占位** — `IAgentComposition::stream()` 当前直接 `throw std::logic_error("Phase 2 - stream not yet implemented")`（`iagent_composition.h:67`），**不能**呈现为可用能力；
> 2. `debate-round` **🟡 组合配方** — 由 `call_async` + `IEvaluator` + `GEPALoop.reflect` 组合实现, **不是单一原语**;
> 3. `example` 必须 mock-mode 可跑或文档伪代码标"蓝图", 禁止无验证示例落盘。

### 18.1 5 模式目录表（与 §11.5/§12.2 矩阵风格一致）

| 模式 | 性质 | 原语锚点（含文件:行）| 应用代号 | 落地状态 |
|------|------|-----------------------|----------|----------|
| **sync-delegate** | ✅ 原语 | `IAgentComposition::delegate()` (`iagent_composition.h:59` → `TaskHandle`) | A3 / A6 / B1 / B5 | ✅ Shipped |
| **fan-out** | ✅ 原语 | `IAgentComposition::call_async()` (`iagent_composition.h:53` → `std::future<AgentResult<std::string>>`) + `ForkJoinLoop::run(branches, ctx, token)` (`fork_join_loop.h:138`) | A4 / B2 / C1（fan-out 形态）| ✅ Shipped |
| **hierarchical-plan** | ✅ 原语 | `PlanExecuteLoop::plan_phase()` + `delegate()` 嵌套 (`plan_execute_loop.h:107-256` + `iagent_composition.h:59`) | A3 / B2（plan-execute 子 agent）| ✅ Shipped |
| **debate-round** | 🟡 组合配方 | `call_async()` (N×并发) + `IEvaluator::evaluate()` (`ievaluator.h`) + `GEPALoop::reflect_and_commit()` (`gepa_loop.h:48`) 三者组合 | B7（GEPA 反思） / C2（MCTS 候选评估）| 🟡 配方 (3 组件各自 ship) |
| **stream-pipeline** | 🔴 V2 占位 | `IAgentComposition::stream()` 直接 `throw std::logic_error` (`iagent_composition.h:67`) | B4（streaming agent） / C1（跨进程 stream）| 🔴 V2 Phase 2 docs-only |

### 18.2 模式选择指南

| 用户问题 | 推荐模式 | 备选 |
|----------|----------|------|
| 子 agent 必须完成后才能继续？ | **sync-delegate** | hierarchical-plan（若含 plan 阶段）|
| 多个子 agent 可并行？ | **fan-out** + `ForkJoinLoop` | call_async 各自启动后手动 join |
| 父子 agent 协同（plan→delegate 子 agent→verify）？ | **hierarchical-plan** | sync-delegate（无 plan 阶段）|
| 多 cognitive agent 提案需评判/投票/合并？ | **debate-round** (配方) + `IEvaluator` | stream-pipeline（V2 后实时合并）|
| 需要流式 cognitive 协同（partial result 流回）？ | **stream-pipeline** (V2 占位, 暂用 `call_async` + polling) | hierarchical-plan 内部 stream |

### 18.3 sync-delegate 演示（✅ Shipped 原语, 文档示例)

```cpp
// A3 受限 LLM 应用: 主 cognitive 委派子 cognitive
auto main_agent = registry->create("plan-execute-v1", {.instance_id = "main"});
auto auditor    = registry->create("react-loop-v1",    {.instance_id = "auditor"});

IAgentComposition comp = make_agent_composition(registry);
auto audit_task = comp->delegate(auditor->id(), "audit_decision",
                                 /*priority=*/"high");
// 后续 audit_task.get() 同步等待结果; 中途可通过 CancellationRegistry 取消
```

### 18.4 fan-out 演示（✅ Shipped 原语 + ForkJoinLoop）

```cpp
// A4 Fork-Join: 3 cognitive agent 并行评估
std::vector<std::string> agents = {"react-loop-v1", "plan-execute-v1", "react-loop-v1"};
ForkJoinLoop loop(registry);
auto result = loop.run(agents, ctx, /*token=*/{});
```

### 18.5 hierarchical-plan 演示（✅ Shipped 原语）

```cpp
// A3 plan-execute 子 agent 委派
PlanExecuteLoop pe(registry, /*llm=*/provider);
auto result = pe.run(goal, ctx, /*token=*/{});
// pe 内部 plan_phase → LLM 规划 → 多步 execute_phase（每步可 call_async + delegate）→ verify_phase
```

### 18.6 debate-round 演示（🟡 组合配方, 3 组件组合）

```cpp
// B7 GEPA 反思: 多 cognitive agent 并发提案 → IEvaluator 评估 → GEPALoop 选最优
std::vector<std::future<AgentResult<std::string>>> proposals;
for (int i = 0; i < N; ++i) {
    proposals.push_back(comp->call_async(agents[i], problem));
}

// 等待并发结果 (顺序无关, 各自 trace_id 不同)
for (auto& fut : proposals) fut.wait();
for (auto& fut : proposals) {
    auto result = fut.get();
    evaluator->evaluate(/*task_id*/result.id, /*trace=*/result.trace);
    // GEPALoop 收集 evaluation.result 事件后 reflect_and_commit
}
```

### 18.7 stream-pipeline 演示（🔴 V2 占位, 文档伪代码）

```cpp
// B4 Streaming Agent: V2 占位, 当前抛 logic_error
try {
    auto stream = comp->stream(agent_id, args);
    while (auto token = stream.next()) { /* ... */ }
} catch (const std::logic_error& e) {
    // V2 落地前: 降级为 call_async + 长轮询或 ForkJoinLoop
    auto result = comp->call_async(agent_id, args).get();
}
```

### 18.8 不变量（适用于 5 模式全部）

- **不新增 contract 类**: 5 模式全部基于现有 `IAgentComposition` 4 模式 + 3 Loop class + `IEvaluator`/`GEPALoop`/`MCTSWorkflowSearch`, 不引入新 IAgent/CognitiveOrchestrator
- **per-worker 隔离不破坏**: 多 cognitive agent 协调经 `IAgentComposition` 调用, 不共享可变 `LayeredContext` (遵守 ADR-0020/ADR-0030 V2 隔离决议)
- **fail-safe 默认**: 任何模式调用失败 (异常/超时) 必须降级, 不静默返回成功
- **可观测性**: 5 模式全部经 IInteractionBus emit `agent.task.*` 事件 (ADR-0068 主题), 便于 BusPattern 订阅 + TraceExporter 收集

### 18.9 与已 ship 应用的映射（cap-map §三 17 类）

| cap-map 应用 | 推荐模式 | 备注 |
|--------------|----------|------|
| A1 单 agent ReAct | (单一 agent, 不需要 cognitive-cognitive) | — |
| A2 多轮对话 + fork | (SessionManager fork, 非 cognitive-cognitive) | — |
| A3 受限 LLM 应用 | hierarchical-plan | plan→delegate→verify |
| A4 Fork-Join 聚合 | fan-out | ForkJoinLoop |
| A5 多 LoRA 路由 | (model_router plugin, 非 cognitive-cognitive) | — |
| A6 审批流 | sync-delegate | delegate to approver agent |
| B1 Marketplace | sync-delegate + HookPattern L1 | per-tenant approval |
| B2 跨进程多 agent | fan-out + sync-delegate | IAgentComposition.call_async |
| B3 分布式追踪 | (observability, 叠加 BusPattern) | — |
| B4 Streaming Agent | stream-pipeline (V2 占位) | 暂用 fan-out 降级 |
| B5 MCP Server | sync-delegate + HookPattern | composition pattern |
| B6 蒸馏环境 | hierarchical-plan (teacher→students) | plan→delegate→verify |
| B7 自进化 | debate-round (配方) | GEPA + IEvaluator |
| C1 跨主机联邦 | fan-out (跨进程) + stream-pipeline | ADR-0077 重启后实现 |
| C2 自进化 agent | debate-round + stream-pipeline | GEPA + MCTS + 流式 |
| C3 WASM 沙箱 | sync-delegate (跨语言) | composition pattern |
| C4 Cloud-native | (服务化冻结, 待 ADR-0050 重启) | — |

### 18.10 升级触发条件

若以下任一情况发生, 启动 ADR-0085 V2 MetaAgent 评审 + 路径 3.2 MCTS Axis6 立项:

- `IAgentComposition::stream()` 由 Phase 2 实装 (需 ADR-0060 v2 amendment);
- ≥2 个真实 `IAgent` 实现类（当前 `SimpleAgent` 唯一 mock, 无生产实现者; 路径 2 Oracle 给出 No-Go 等待 AgentWorker 落地）;
- S4 promotion criteria 全部满足（self-evolution §六, 含信用分配 ADR ship + 防共谋/多样性指标定义）;
- 用户需求被识别为"运行时动态选择协调模式"而非静态选型（当前 §18.2 是静态选择, 不涉及运行时决策）。

---

## 变更记录

| 日期 | 版本 | 变更 |
|------|------|------|
| 2026-08-29 | v1.0 | 初始化（综合横切架构 + PDK Agent 家族 + 认知/领域 Worker + LLM 编排蓝图 + 应用场景映射 + 断链清单）|
| 2026-08-30 | v1.1 | (1) 修正 §一/§八/§十 中 `multi-domain-agent-architecture.md` 路径错误（3 处）；(2) §七 P1 Meta-Agent 修正为"ADR-0085 已显式 defer,无设计稿落盘"；(3) §2.1 3 个 Loop 签名补 `std::stop_token token = {}` 参数；(4) §八 invariant 1 "20" → "21"；(5) §七 dispatch 路径补全 + 行号 38→39；(6) §九 验证命令 #3 修正语义 + #5 改为精确计数 + #6/#7 新增；(7) 新增 §十一 GenerateSubGraph 使用场景与流程分析（11 子节, 含调用链流程图 + 7 不变量 + 7 场景 + 修复路径）|
| 2026-08-30 | v1.2 | (1) **文件迁移协调** 原 `docs/guides/multi-domain-agent-architecture.md` → `multi-domain-agent-architecture.md`（同目录迁入），全部引用同步更新（architecture/README, architecture/application-layer-sota-positioning-v2, research/agent-as-plugin-architecture-synthesis, specs/architecture 4 处 + developer-guide 顶部 banner 指向新位置）；(2) **§十二新增** 横切功能矩阵 (8 横切能力 × 17 应用场景激活矩阵 + 蒸馏环境配置 DSL 示例)；(3) **§十三新增** Agent = Loop 编排哲学（3 Loop × 5 编排体 × 横切层 + Loop × Skill 运行时关系）；(4) **§十四新增** Cognitive ↔ Domain 功能矩阵生态（4 指导机制 M1-M4 + 蒸馏路径 + 反思路径）；(5) **§十五新增** 编排蒸馏学习（7 环蒸馏闭环 + 3 变体 + 数据安全边界）；(6) **§十六新增** 自进化全景（已 ship 基础 #27-#31 + 编排层角色分工 + 4 阶段路径 + 横切配置示例）；(7) **§十七扩充** 关联文档表 (+ 4 个 ADR 1 个 evolution pipeline doc); (8) **§九扩充** 验证命令 #8-#13（自进化组件存在性 + IEvaluator/MutationGovernor 代码 + 31 项 cap-map 能力 + 17 应用行数 + 文档同目录 + ADR-0061-13 status） |
| 2026-08-30 | v1.3 | **Oracle 综合评审修正 4 处失实**（path 1/2/3/4 独立评审 + 综合, 4 Oracle 共识后修正）：(1) **§五 C1/C4** 引用 ADR-0077 + ADR-0050 Candidate B 服务化 — 标注 ADR-0077 Wave 4 descoped (docs-only) + Candidate B 🔒 冻结（重开条件 0/6, Phase 6 主线 = PDK 生产化 + AgentForge MVP）；(2) **§十六** 头部新增 Phase 6 战略对齐 note — 自进化路径不属服务化轨道, B7 落地应用归属 PDK 生产化, S3/S4 阶段待 `adr-0086-credit-assignment-contract.md` ship 后推进；(3) **§十七关联文档表** +3 行（ADR-0050/0077/0086-pending），其中 ADR-0086 取代 self-evolution §七 #6 已过期文件名 `adr-0085-credit-assignment-contract.md`（0085 已被 ADR-0085 横切 Pattern PDK 占用）；(4) **§七 P1 Meta-Agent** 修复路径同步指向 ADR-0086。Oracle 评审其他结论（路径 2.3 IAgent 直接扩展 No-Go / 路径 3.2 MCTS Axis6 优先 / 路径 4.3 完整平台 No-Go）以 §十六战略对齐 note 间接表达，避免与已 Approved ADR 冲突。 |
| 2026-08-30 | v1.4 | **Oracle 综合评审后续 Step 1 (Conditional-Go 路径 1)**: 新增 **§十八 Cognitive-Cognitive 协调模式目录**（5 模式 + 不变量 + 17 类应用映射 + 升级触发条件）；§四 决策树新增"需要认知 agent 互相协调？"分支；§十七关联文档+1 行（§十八自引用 + ADR-0085 §决策 5 V1 不实施 MetaAgent 链接）；§九验证命令 #18-#20 验证模式目录一致性 + stream 占位 + ADR-0085 §决策 5 引用。3 个强制条件已落实: (a) stream-pipeline 标 🔴 V2 占位; (b) debate-round 标 🟡 组合配方; (c) §18.7 stream 演示文档伪代码标"🔴 V2 占位"且不要求可执行验证。 |

---

**状态**: 🔍 Proposed（v1.2）— 指导性文档，需架构组评审后晋升为 Active
**维护者**: solo-dev（Sisyphus）
**下一修订**: 断链修复（P0 GenerateSubgraphNode 详见 §11.6）+ 编排层蓝图（P1 场景智能体）+ §11.5 S1/S2/S5/S6 场景落地后 + §十四蒸馏路径 B6 应用落地后
