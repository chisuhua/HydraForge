# 架构能力-应用地图（2026-08 v2.2）

**生成日期**: 2026-08-28
**最后验证**: 2026-08-28（v2.2 — 28 项能力 / 9 项 open gap (G10-G15 全部 ✅ Closed) / 17 类应用 / 22 个工程任务 T1-T22 (**T17 ✅ SHIP** / **T15 ✅ SHIP** / **IEvaluator V2 ✅ SHIP** / **T19 GEPA MVP V1 ✅ SHIP** / **T21 ✅ SHIP** / T20/T22 待启动)，T15 TrajectoryIR V1 ship 同步 (9 cases / 55 assertions)，T19 GEPA Phase 2 commit 2026-08-27 完成，T21 Prompt Evidence Gate 2026-08-28 ship (test_prompt_evidence_gate 19 cases / 338 assertions)，验证命令见 §六）
**作者**: Architecture Working Group
**状态**: ✅ Active — 架构能力的**唯一事实源**（取代已归档的 `defect-truth-table-2026-08.md`）

**前置文档**（继任关系）:
- ⛛ Superseded: [`archive/architecture/defect-truth-table-2026-08.md`](../../archive/architecture/defect-truth-table-2026-08.md) — 14 项跟踪缺陷 + 3 个盲点 × 代码 × ADR 真相（已完成其历史使命，11/17 ship）

**关联文档**:
- `docs/architecture/adr-implementation-status-gap-analysis.md` — ADR 实施状态基线（ADR 状态引用必须以此为准）
- `docs/architecture/defect-fix-roadmap-2026-08.md` — 12 个 rdd-workflow 提案跟踪（已 ship 9/12，剩余 3 项进入本表 §二）
- `docs/specs/architecture.md` — AgenticOS 五层架构规范（L0~L4 + R1~R5）
- `docs/architecture/layer-based-missing-capabilities-analysis.md` — 五层缺失能力分析 + Wave 1-4 执行计划

**核心视角转换**（从 v1.0 defect-truth-table → 本表 v1.0 capability-application-map）:

| 旧视角（问题导向） | 新视角（能力导向） |
|---|---|
| "我们有 14 项缺陷" | "我们有 22 项已 ship 能力 + 9 项 open gap" |
| "ADR-0079 实施率 100%" | "工程任务 T1 启动即可解锁 6 个 A 类应用" |
| "缺陷 3.1 骨架 ship" | "IAgentRegistry 已可用，B 类应用 2 sprint 内可构建" |
| 计数聚焦（grep + ctest） | 应用聚焦（什么工程 → 解锁什么应用） |

---

## 一、架构能力清单（28 项已 ship 能力，L4 含 #23 T14 + #24 T17 + #25 T15 + #26 IEvaluator V2 v2.0 后置增补 + #27 GEPALoop + #28 Prompt Evidence Gate）

> **分层依据**: `docs/specs/architecture.md` L0~L4 + R1~R5 五层架构模型
> **验证方法**: `grep` 实证 + ctest PASS + ADR 头部状态三方交叉（详见 §六）
> **覆盖范围**: 截至 2026-08-27, 含 Sprint 24 (含 T14/T17/T15/IEvaluator-V2 后置增补) 全部 ship

### L0 运行时层（核心引擎，5 项）

| # | 能力 | 实现位置 | 验证证据 | 关联 ADR | 上线 |
|---|---|---|---|---|---|
| 1 | DSL Markdown 工作流加载 | `src/core/engine.h::from_markdown()` | 7 examples 全部 ship | ADR-0009 | Sprint 5 |
| 2 | DAG 拓扑调度（节点级 + Taskflow 并行）| `topo_scheduler.h/cpp` | 49/49 ctest PASS | ADR-0019 | Sprint 5 |
| 3 | 节点执行器 + ToolCoordinator + 错误恢复 | `node_executor.h/cpp` + `tool_coordinator.h/cpp` | 8 cases + hooks 8 cases PASS | ADR-0031 + 0069 | Sprint 13 |
| 4 | LayeredContext 5 层结构化上下文 | `include/agenticdsl/types/layered_context.h` | test_basic + test_engine 全部 PASS | ADR-0008 | Sprint 20 |
| 5 | Context 压缩 + 生命周期事件 | `src/core/context_compactor.cpp` + EventBuilder | 4 cases / 29 assertions PASS + ADR-0068 附录 A ✅ | ADR-0007 + 0068 | Sprint 22 (P10) |

### L1 编排层（智能体循环，5 项）

| # | 能力 | 实现位置 | 验证证据 | 关联 ADR | 上线 |
|---|---|---|---|---|---|
| 6 | ReAct 循环 (think→act→observe) | `include/agenticdsl/pdk/agent_loops/react_loop.h` | test_pdk_macros 5 cases PASS | ADR-0021 | Sprint 4 |
| 7 | Plan-Execute 循环 (plan→execute→verify, 3 重试) | `plan_execute_loop.h` 5 状态机 | test_pdk_plan_execute 5 cases PASS | ADR-0021 | Sprint 20 |
| 8 | Fork-Join 并行循环 (fork→execute→join, 4 状态机) | `fork_join_loop.h` + DomainWorkerPool | test_pdk_fork_join 5 cases PASS | ADR-0021 + 0020 | Sprint 20 |
| 9 | Cognitive Worker 隔离执行 (jthread + stop_token) | `cognitive_worker.h` + state machine | 9 cases / 33 assertions PASS | ADR-0020 | Sprint 2 |
| 10 | Domain Worker Pool 并发 (N jthread 共享 FIFO) | `domain_worker_pool.h` | 7 cases / 94 assertions PASS | ADR-0020 | Sprint 3 |

### L2 工具层（4 项）

| # | 能力 | 实现位置 | 验证证据 | 关联 ADR | 上线 |
|---|---|---|---|---|---|
| 11 | Tool Registry V2 (per-engine, 9 虚函数) | `include/agenticdsl/contract/itool_registry.h` | test_tool_registry_interface PASS | ADR-0022 + 0004 | Sprint 14 |
| 12 | Tool Metadata V2 (category × approval × allowed_layers) | `secure_tool_registry.cpp` | 9 处调用点迁移 + 0 回归 | ADR-0004 V2 | Sprint 15 |
| 13 | Tool Coordinator Hook 拦截 (pre/post + HookErrorPolicy) | `itool_hook_registry.h` + `tool_coordinator.cpp` | 5 类测试 PASS | ADR-0069 | Sprint 22 |
| 14 | DECLARE_TOOL + DECLARE_COMMAND 宏 (PDK) | `pdk/tool_macros.h` + `pdk/command_macros.h` | test_pdk_macros + test_command_registry PASS | ADR-0070 | Sprint 4 + Sprint 23 |

### L3 插件层（5 项）

| # | 能力 | 实现位置 | 验证证据 | 关联 ADR | 上线 |
|---|---|---|---|---|---|
| 15 | Plugin Loader 双 ABI (V1/V2 + RTLD_LOCAL) | `plugin_loader.cpp` | test_plugin_loader 7 cases PASS | ADR-0022 + 0041 | Sprint 5 + 16 |
| 16 | IAgentRegistry L3 契约 (骨架, per-engine 注册) | `iagent_registry.h` + InMemory 实现 | test_agent_registry 5 cases / 29 assertions PASS | ADR-0082 | Sprint 22 (P7) |
| 17 | IAgentHookRegistry L3 契约 (骨架, agent-scoped) | `iagent_hook_registry.h` + InMemory 实现 | test_agent_hook_registry_contract 4 cases / 18 assertions PASS | ADR-0081 | Sprint 22 (P7) |
| 18 | IAgentComposition (call/call_async/delegate + stream 占位) | `iagent_composition.h` + `agent_composition.cpp` | test_agent_composition 10 cases PASS | ADR-0060 | Sprint 22 (P8) |
| 19 | PDK 三种 Agent Loop + SafeExec 沙箱 + **SLM 路由 .so** (v1.2 +1) | `pdk/agent_macros.h` + `pdk/safe_exec.h` + `pdk/model_router/slm_strategy/` | test_pdk_macros 5 cases + test_safe_exec PASS + **test_model_router_slm ≥5 cases PASS (v1.2 ship)** | ADR-0021 + **0061-04 (SLM)** | Sprint 4 + 22 + **23 (SLM)** |

### L4 可观测 + 治理层（9 项，#23 T14 / #24 T17 / #25 T15 / #26 IEvaluator V2 v2.0 后置增补 / #27 GEPALoop / #28 Prompt Evidence Gate）

| # | 能力 | 实现位置 | 验证证据 | 关联 ADR | 上线 |
|---|---|---|---|---|---|
| 20 | EventLog (read + query + v:1 schema + perf 基准) | `src/core/event_log.cpp` 253 行 | 12 cases + perf 10k<100ms PASS | ADR-0080 | Sprint 22 (P4) |
| 21 | Session 4-scope + node_id 寻址 + branch cursor + extract fork | `session_manager.cpp` + `session_store.cpp` + `session_writer.cpp` | 8 + 4 + 6 + 6 + 7 cases PASS | ADR-0079 v1.2 | Sprint 22 (P5 + P6) |
| 22 | 17 ErrorCode + ExecutionResult<T> + is_retryable() | `tool_result.h` + `execution_result.h` | test_execution_result_error_taxonomy 8 cases PASS | ADR-0023 + 0033 + 0080 | Sprint 22 (P9) |
| **23** | **行为回归套件 (AgentAssay-style, 三值 Verdict + Hotelling T²)** | `include/agenticdsl/testing/behavioral_regression.h` + `src/modules/testing/behavioral_regression.cpp` | test_behavioral_regression **6 cases / 13 assertions PASS** | ADR-0061-02 | **Sprint 23 (T14, 2026-08-24 ship)** |
| **24** | **SkillCompiler V1 (纯函数式 SKILL.md 编译 + T14 自检 + IEvaluator 门 + G11 emit-only)** | `include/agenticdsl/contract/iskill_compiler.h` + `src/modules/cognitive/skill_compiler.cpp` | test_skill_compiler **16 cases / 63 assertions PASS** | ADR-0061-03 | **Sprint 24 (T17, 2026-08-27 ship)** |
| **25** | **Trajectory IR V1 (独立序列化视图, 3 级 IR + 单向 Converter + SFT/OTel backends + Pass 占位)** | `include/agenticdsl/ir/trajectory_ir.h` + `src/core/parsed_graph_to_trajectory_ir.cpp` + `src/modules/ir/` | test_trajectory_ir **9 cases / 55 assertions PASS**; ParsedGraph 零修改 | ADR-0061-06 v1.1 | **Sprint 24 (T15, 2026-08-27 ship)** |
| **26** | **IEvaluator V2 (BehavioralEquivalence + Composite 多评估器聚合)** | `include/agenticdsl/cognitive/{behavioral_equivalence_evaluator,composite_evaluator}.h` + `src/modules/cognitive/{behavioral_equivalence_evaluator,composite_evaluator}.cpp` | test_evaluator **8 cases / 18 assertions PASS (V2)**; IEvaluator 接口零修改 | ADR-0083 | **Sprint 24 (evaluator-v2-composite, 2026-08-27 ship)** |
| **27** | **GEPALoop 反思循环 MVP (失败轨迹 → LLM 反思 → 修订候选 → 编译 → 回归 → 评估 → MutationGovernor 授权提交)** | `include/agenticdsl/cognitive/gepa_loop.h` + `src/modules/cognitive/gepa_loop.cpp` | test_gepa_phase2 **14 cases PASS (含 3 E2E: 真实 V2 evaluator / 真实 governor / 回归拒绝中止)**; 既有 7 契约零修改; 6 个 `gepa.*` 事件注册 (ADR-0068 v1.3) | ADR-0071 + ADR-0061-09 (GEPA) | **Sprint 24 (T19, 2026-08-27 Phase 2 commit ship)** |
| **28** | **Prompt Evidence Gate (Go/No-Go 阈值 + parse-valid + 两阶段注入 ≤8k + JSONL 导出 + llm.dsl.* 事件)** | `include/agenticdsl/prompt/evidence_gate.h` + `src/modules/prompt/evidence_gate.cpp` + `src/modules/prompt/prompt_assembler.cpp` + `tools/baseline/measure_prompt_baseline.py` + `tools/prompt/export_training_data.py` | test_prompt_evidence_gate **19 cases / 338 assertions PASS**; 30+ few-shot `lib/prompt/few_shots/` + 50+ golden `lib/prompt/golden/`; 3 主题注册 (ADR-0068 v1.4); 既有契约零修改 | ADR-0074 + ADR-0083 (IEvaluator V2) | **Sprint 25 (T21, 2026-08-28 ship)** |

**总覆盖**: **28 项已 ship 能力** = L0(5) + L1(5) + L2(4) + L3(5) + L4(9)
**v1.1.2 新增**: L4 +1（行为回归套件 — Oracle 评审 "本周最高杠杆" T14 完成）
**v1.3 新增**: L4 +1（SkillCompiler V1 — T17 ship, B7 自进化"变异对象生成器"落地, 闭环 2 第 4 环接通）
**v1.9 新增**: L4 +1（Trajectory IR V1 — T15 ship, B6 蒸馏数据标准化落地, G14 闭环; ParsedGraph 独立视图, 单向 Converter 桥接）
**v2.0 新增**: L4 +1（IEvaluator V2 — evaluator-v2-composite ship, BehavioralEquivalence (T14 集成) + Composite 多评估器聚合, G10 评估信号 V2 层落地）
**v2.1 新增**: L4 +1（GEPALoop — T19 Phase 2 commit ship, B7 自进化基础应用解锁, 失败→反思→修订→回归→评估→授权提交全闭环落地）
**v2.2 新增**: L4 +1（Prompt Evidence Gate — T21 ship, prompt 质量门控层落地, Wave 2 → Wave 3 Go/No-Go 客观标准就绪, B7 自进化 prompt 门控前置）

---

## 二、已知开放 Gap（15 项追踪：G1-G9 继任 + G10-G15 Oracle 新增；含 5 项已 ✅ Closed）

> **继任关系**: 本节接管 `archive/architecture/defect-truth-table-2026-08.md` §一/§二 中 9 项仍开放的追踪对象（G1-G9） + v1.1 Oracle 评审新增 6 项（G10-G15，蒸馏 + 自进化相关）
> **状态词汇**: 🔓 Open（无前置）/ 🔒 Blocked（前置未 ship）/ 🟡 Partial（分层部分解决）/ 🔴 架构契约缺失（需新 ADR）/ ✅ Closed（评审通过，已 ship 不需再实施，如 G10/G12/G13/G14/G15）
> **性质标记**: [架构] = 需新 ADR 或 ADR amendment；[工程] = 仅需实现
> **v1.1.2 状态**: G10 (ADR-0083) / G12 (ADR-0080 v1.2 amendment) / G15 (ADR-0061-13) 已起草 🔍 Proposed, 待架构组评审

### Gap 索引（速查表）

| # | Gap | 来源 | 性质 | 状态 | 影响范围 | 解锁条件 |
|---|---|---|---|---|---|---|
| G1 | compact 破坏性重写（有意例外）| 缺陷 1.4 | [架构] | 🔒 待 ADR-0079 v1.2 决策 | 会话可回放性 | 架构组决议 |
| G2 | EventLog query API 自动化校验 | 缺陷 2.2 子项 | [架构] | 🔓 Open | 客户端 SDK 演进保障 | ADR-0080 D11 + EventBuilder schema 校验 |
| G3 | AgentWorker 完整实现 + spawn_agent + YAML | 缺陷 3.1 延展 | [工程] | 🔓 Open（前置 IAgentRegistry ✅） | B2 跨进程多 agent 协作 | T3 + T4 |
| G4 | Agent↔Agent stream 模式 | 缺陷 3.3 stream 占位 | [架构] | 🔓 Open | B4 streaming agent | T6 |
| G5 | Plugin per-agent 隔离（marketplace 多租户）| 缺陷 4.1 agent 级 | [工程] | 🟡 Partial（per-engine ✅） | B1 marketplace 部署 | T5 |
| G6 | Agent hook loop 集成 | 缺陷 4.2 延展 | [工程] | 🔓 Open（前置 IAgentHookRegistry ✅） | 全部 B/C 类应用的可观测性 | Sprint 24+ 集成 |
| G7 | Structured concurrency (scope tree) | 缺陷 5.1 scope tree | [架构] | 🟡 Partial（协作取消 ✅） | 形式化父子级联保证 | T8 |
| G8 | OTel 真实 OTLP 客户端（替换 NoopSink）| 盲点 7.3 延展 | [工程] | 🔓 Open | B3 真实分布式追踪 | T2 |
| G9 | ADR-0076 DSL Engine as MCP Server | ADR-0076 🔍 Proposed | [架构] | 🔓 Open | B5 DSL-as-MCP-tool | T7 |
| **G10** | **评估/奖励信号契约缺失** | v1.1 Oracle 评审 [架构]| **🔴 架构层** | **✅ Closed (ADR-0083 Approved + IEvaluator 代码 ship, 2026-08-26)** | **所有进化路径（GEPA/AFlow/fine-tune/行为克隆）** | **~~新立 ADR: IEvaluator/RewardSignal 契约 + OpenSpec task ship IEvaluator 类 + 3 内置评估器~~ (已 ship: 12 cases / 31 assertions, V2 评估器留 follow-up)** |
| **G11** | **变异治理/授权契约缺失** | v1.1 Oracle 评审 [架构]| **🔴 架构层** | **✅ Closed (ADR-0084 Approved + V1 gate-and-audit 代码 ship, 2026-08-26, commit `a2b2d52`)** | **Agent 自修改攻击面** | **~~新立 ADR: adr-0084-mutation-governance-contract~~ (已 ship: 13 cases / 139 assertions, ctest 187/187 PASS, issue #14 已关闭)** |
| **G12** | **D10 蒸馏数据采集被死锁** | ADR-0080 D10.3 [架构]| **🔴 架构层** | **✅ Closed (评审通过 2026-08-25)** | **蒸馏数据面（采集→训练→评估）** | **ADR-0080 v1.2 amendment（解耦 scrub hook 与 capture）** |
| **G13** | **ADR-0071 父方向 ADR 仍 Proposed** | ADR-0071 🔍 Proposed [架构]| **🔴 架构层** | **✅ Closed (评审通过 2026-08-25)** | **4 个 TD 项派生（SkillCompiler/GEPA/Prompt Gate）** | **架构组评审 ADR-0071 → Approved** |
| **G14** | **Trajectory IR 标题耦合风险** | ADR-0061-06 [架构+工程]| **🔴 架构层** | **✅ Closed (评审通过 2026-08-25)** | **TD1 实现方式（应独立序列化视图而非改 ParsedGraph）** | **架构评审 0061-06 标题修订** |
| **G15** | **行为克隆器（蒸馏输出格式）无 ADR** | v1.1 Oracle 评审 [架构]| **🔴 架构层** | **✅ Closed (评审通过 2026-08-25)** | **蒸馏输出层** | **新立 ADR-0061-13 蒸馏输出格式** |

> **Oracle 评审关键发现**:
> - G10 评估契约 + G11 变异治理是**未识别的架构层缺口**——GEPA/AFlow/fine-tune/行为克隆全部依赖它，没有它整个自进化方向无法启动
> - **G10 ✅ Closed (ADR-0083 Approved + IEvaluator 代码 ship 2026-08-26) + G11 ✅ Closed (ADR-0084 Approved + V1 gate-and-audit 代码 ship 2026-08-26, commit `a2b2d52`, issue #14 已关闭)**
> - **2026-08-26 自审修正 (Oracle session `ses_fc3090b49ffe7yJwXhx1MoNz5N`)**：原文档将 G10 标 "✅ Closed" 与代码实际状态不符（ADR-0083 头部 + §状态字段自相矛盾，`include/agenticdsl/contract/ievaluator.h` 代码不存在）；现统一为「ADR 起草完成 + 代码 ship 待办」语义。G11 同步标 ADR-0084 文件创建 + 待评审转 Approved
> - G12 D10 死锁是**已 Approved 契约被 Proposed 链锁住**（ADR-0081 → 0082）——需 v1.2 amendment 解耦
> - G13 ADR-0071 父未批 → 4 个子项全部悬空——**本周架构评审是最高杠杆动作**

---

## 三、应用场景矩阵（A/B/C 三类共 17 个应用类型）

> **分类逻辑**:
> - 🟢 **A 类**: 现成可构建（零工程工作，直接基于 23 项已 ship 能力）
> - 🟡 **B 类**: 1-3 sprint 工程后可构建（解锁条件见 §四 T1-T8 + §八 T14-T17）
> - 🟠 **C 类**: 1-3 月工程后可构建（Phase 6 Candidate B 服务化路径 + §八 T18-T22 研究轨）

### 🟢 A 类：现成可构建（6 个应用类型）

> **依赖能力**: L0(1,2,3,4,5) + L1(6,7,8,9,10) + L2(11,12,13,14) + L3(15,16,17,18,19) + L4(20,21,22)
> **零工程**: 不需要任何新的代码工作，直接组合 23 项已 ship 能力

| 应用类型 | 组合能力 | 参考/示例 | 关键依赖 |
|---|---|---|---|
| **A1** 单 agent 工具调用 ReAct | L0(1,3,4) + L1(6,9) + L2(11,12) | `examples/agent_basic/` 已 ship | LLM provider + 工具注册 |
| **A2** 多轮对话 + 会话分支 + `--fork` | L4(21) + CLI `--fork` | `examples/pdk_chat_demo --fork` 已 ship | SessionManager + SessionStore.extract |
| **A3** 成本/合规/速率受限的 LLM 应用 | L4(22) + LLMProvider 装饰器链 | CostTrackingDecorator + ComplianceDecorator + RateLimitDecorator 已 ship | IBudgetController + Tool Coordinator |
| **A4** 单进程并行任务聚合 (Fork-Join) | L1(8,10) + L4(20) | `test_pdk_fork_join.cpp` 已 ship | DomainWorkerPool + IInteractionBus |
| **A5** 多 LoRA / 多模型路由 | L3(15) + `pdk/model_router/` | ADR-0034 ship, **4 路由策略 .so (v1.2 +SLM)** | CostModelRouterPolicy / QualityModelRouterPolicy / LatencyModelRouterPolicy / **SLMModelRouterPolicy (v1.2 ship)** |
| **A6** 工具调用审核 (审批流) | L2(13) + IApprovalHandler + `safe_exec` | `test_tool_coordinator` ship | ToolCoordinator + IApprovalHandler |

### 🟡 B 类：1-3 sprint 工程后可构建（7 个应用类型）

> **共同前置**: A 类全部能力 + 至少 1 个 §四 工程任务（T1-T8）+ §八 T14-T17 完成

| 应用类型 | 解锁条件（§四/§八任务）| 估时 | 关键技术挑战 |
|---|---|---|---|
| **B1** Marketplace Agent 部署 | T5 (per-agent ToolRegistry) | 2 sprint | Agent 版本隔离 + 数字签名 + 安全审计 |
| **B2** 跨进程多 agent 协作 | T3 + T4 (YAML + 完整 AgentWorker) | 2-3 sprint | 进程间通信 + 状态序列化 + 重启恢复 |
| **B3** 真实分布式追踪 | T2 (OTel OTLP 客户端) | 1 sprint | ISpanSink → OTLP gRPC client + 批量处理 |
| **B4** Streaming Agent (partial result 流) | T6 (ADR-0060 stream 模式) | 2-3 sprint | IGenerationStream 流式契约 + 背压控制 |
| **B5** MCP Server 形态 (DSL-as-tool) | T7 (ADR-0076 ship) | 2 sprint | stdio/HTTP/SSE transport + capability 暴露 |
| **B6** Agent 蒸馏环境（教师→学生能力迁移）| T14 (行为回归 ✅ 门禁) + T15 (Trajectory IR) + ADR-0071 + G10 评估契约 | | 2-3 sprint | 教师规划轨迹采集 + 学生行为克隆 + 等价性评估（依赖 IEvaluator 契约）|
| **B7** Agent 自进化基础（GEPA 反思循环 MVP）| T14 + T15 + T19 (GEPA spike Phase 2 committed 2026-08-27) + G10 ✅/G11 ✅ Closed (V1 code ship, 2026-08-26) 契约 | | 2-3 sprint | 失败→反思→修订 prompt + 回归门禁 + 变异授权（T19 Phase 2 committed 2026-08-27, B7 → ✅ Completed）|

### 🟠 C 类：1-3 月工程后可构建（4 个应用类型）

> **共同前置**: B 类全部能力 + Phase 6 Candidate B 服务化路径启动 + §八 T18-T22 研究轨 spike

| 应用类型 | 解锁条件（§四/§八任务）| 估时 | 关键技术挑战 |
|---|---|---|---|
| **C1** 跨主机多 agent 联邦 | T9 (ADR-0077 gRPC data plane) | 1-3 月 | 64KB mTLS 高吞吐 + 路由决策 + 命名服务 |
| **C2** 自进化 agent (fine-tune + evolution loop) | T22 (ADR-0078 **事件驱动** 非排期) + B6/B7 基础 | 事件驱动（AgenticMind 回流触发）| 数据回流 + LoRA 增量训练 + 行为回归测试 |
| **C3** WASM 沙箱多语言 agent | T12 + T13 (ADR-0056 + 0065) | 2-3 月 | wasi-sdk 集成 + Python PDK + 字节码编译 |
| **C4** Cloud-native agent 服务 | T10 (Service-ification) | 1-3 月 | 容器化 + K8s operator + 自动扩缩容 |

---

## 四、工程任务 → 功能/应用解锁映射（T1-T13 共 13 项）

> **核心问题回答**: "后续只需哪些工程工作 → 支持哪些功能/应用"
> **任务来源**: §二 9 项 open gap + 现有 rdd-workflow 提案 + Phase 6 Candidate B 路径
> **优先级**: 立即（≤1 sprint）/ 短期（1-3 sprint）/ 中期（1-3 月）

### 立即可启动（无前置，估时 ≤ 1 sprint）

| 任务 | 估时 | 解锁功能 | 解锁应用 | 来源 |
|---|---|---|---|---|
| **T1**: ADR-0079 v1.2 compact 模式决策（Pi-style vs 绝对 append-only）| 1 sprint | 完整会话可回放性 + 可审计压缩 | A 类全部 + B5 | G1（架构组决策）|
| **T2**: 实施 OTel 真实 OTLP 客户端（替换 NoopSink → otlp-http exporter）| 1 sprint | 真实分布式追踪 + Jaeger/Tempo 集成 | B3 | G8 |
| **T3**: 扩展 IAgentRegistry 支持 YAML 配置加载 + 命名版本 | 1 sprint | 动态 agent 配置 + 版本管理 | B2 起步 | G3 |

### 短期（前置依赖已 ship，1-3 sprint）

| 任务 | 前置 | 估时 | 解锁功能 | 解锁应用 | 来源 |
|---|---|---|---|---|---|
| **T4**: 实施 ADR-0082 完整 AgentWorker + spawn_agent | IAgentRegistry 骨架 ✅ | 2 sprint | per-engine → 全局 agent 注册 + 生命周期 | B2 | G3 |
| **T5**: 实施 per-agent ToolRegistry 隔离（marketplace 隔离）| ADR-0082 完整 ship | 2 sprint | 多租户 agent 版本隔离 | B1 | G5 |
| **T6**: 实施 ADR-0060 stream 模式（partial result 流）| ToolCoordinator + agent_composition ✅ | 2-3 sprint | 流式 agent + 增量响应 | B4 | G4 |
| **T7**: 实施 ADR-0076 DSL Engine as MCP Server | EnvBackend ✅ + DSL Engine ✅ | 2 sprint | DSL-as-MCP-tool + stdio/HTTP/SSE transport | B5 | G9 |
| **T8**: 实施 structured concurrency (scope tree / nursery) | 无（C++23 兼容路径）| 2 sprint | 形式化父子级联取消保证 | 提升所有 B/C 类可靠性 | G7 |

### 中期（Phase 6 Candidate B 路径，1-3 月）

| 任务 | 估时 | 解锁功能 | 解锁应用 | 来源 |
|---|---|---|---|---|
| **T9**: ADR-0077 gRPC data plane + 64KB mTLS 高吞吐 | 2-3 月 | 跨进程/跨主机 agent 通信 | C1 | Phase 6 路径 |
| **T10**: Service-ification (DSL Engine as Cloud Service) | 1-2 月 | Cloud-native 部署 + 自动扩缩容 | C4 | Phase 6 路径 |
| **T11**: ADR-0078 fine-tune + 进化 loop | 2-3 月 | self-improvement agent + 行为回归 | C2 | Phase 6 路径 |
| **T12**: ADR-0056 WASM runtime | 2-3 月 | 浏览器/边缘 agent + 沙箱 | C3 | Phase 6 路径 |
| **T13**: ADR-0065 Python PDK | 1-2 月 | 多语言开发生态 | C3 | Phase 6 路径 |

### 总估时

- **立即启动 3 项（T1-T3）**: 约 0.5-1 个 Sprint
- **短期 5 项（T4-T8）**: 约 2-3 个 Sprint
- **中期 5 项（T9-T13）**: 约 6-12 个月（Phase 6 全路径）

---

## 五、与现有文档的关系

```
docs/specs/architecture.md  (L0-L4 + R1-R5 五层模型)
       ↓ 能力落地
docs/architecture/capability-application-map-2026-08.md  ← 本表 (v1.1)
       ↓ 拆分映射
   ├── §一 22 项 ship 能力（按 L0-L4 工程分层）
   ├── §二 15 项 open gap（G1-G9 继任自 defect-truth-table + G10-G15 v1.1 Oracle 评审新增）
   ├── §三 A/B/C 应用场景矩阵（15 → 17 类,v1.1 新增 B6/B7 蒸馏+自进化）
   ├── §四 T1-T13 工程任务解锁映射（基础设施轨）
   └── §八 Agent 蒸馏与自进化专题（v1.1 新增:契约vs实现分类 + 双最小闭环 + T14-T22 + 新增 ADR 需求）

↔ docs/architecture/defect-fix-roadmap-2026-08.md (12 提案跟踪)
   - 9/12 提案已 ship，本表 §一/§二 接管后续追踪
   - 剩余 3 项活跃提案进入 §四 T1-T8

↔ v1.1 关键评审输入（Oracle session ses_fcba5e477ffeG9wEBHVhU64J0o）
    - 架构 vs 工程分层错误（L0-L4 是工程维度）
    - 评估契约 + 变异治理是未识别的架构层缺口
    - D10 蒸馏数据采集被 ADR-0081→0082 死锁

↔ G11 战略评估（Oracle session ses_fc640ea84ffe0f4dyYTa4aFjiL, 2026-08-26）
    - G11 变异治理契约 6 维度骨架（决策 1-6）
    - G11 起草前置：T17 ship + ADR-0083 ✅ + ADR-0079 v1.1 ✅
    - T19 时序冲突解决：Phase 1 只读反思约束（不执行 commit(PromptEdit)）

↔ G11 Self-Review 预审（Oracle session ses_fc41537bbffeC35NKqgvzn4m1c, 2026-08-26）
    - 12 项通用 + 4 项专用 Self-Review Checklist 逐项审查
    - 9 ✅ / 6 ⚠ / 1 ❌ → 6 项修订后全部转 ✅
    - T19 时序决议 + L1 措辞 + V1 终止锚点 + 6 项事实修订
    - 引用：GitHub issue #14 Self-Review Pre-Review comment

↔ G11 Approved 后续执行深度审查（Oracle session ses_fc3e070c0ffeIVgAhsgx2pNXFa, 2026-08-26）
    - 6 步后续执行清单完整性验证
    - cap-map 9 section 修订清单 + commit 策略 B' (3 atomic commits)
    - 6 项风险点识别（R1-R6）+ 验证检查清单补充
```

↔ docs/architecture/adr-implementation-status-gap-analysis.md
   - ADR 状态基线，本表 §一/§二 ADR 引用以此为准

⛛ Superseded: docs/archive/architecture/defect-truth-table-2026-08.md
   - 历史审计，14 项缺陷 × 代码 × ADR 真相（三方实证方法论保留）
```

**职责边界**:
- `specs/architecture.md` — 架构契约层（"应该是什么"）
- `capability-application-map-2026-08.md`（本表） — 能力 + 应用 + 任务层（"能做什么 + 做什么能解锁什么"）
- `defect-fix-roadmap-2026-08.md` — 提案跟踪层（"12 个 rdd-workflow 提案进度"）
- `active-status.md` — 看板层（"当前活跃变更状态"）

**本表不维护**: ADR 状态副本（引用 `adr-implementation-status-gap-analysis.md`）、实施率计数（引用 ctest 输出）、具体 ship 进度（引用 `active-status.md`）。

---

## 六、验证命令附录

> 符合 `docs/architecture/README.md` §二 "Last-Verified 规则"——所有**计数类数据**必须可用命令复现

### 6.1 已 ship 能力计数（验证 §一 = 22 项）

```bash
# L0 (5 项): 引擎核心
grep -l "DSLEngine::from_markdown" src/core/engine.cpp
grep -l "build_dag\|schedule" src/modules/scheduler/topo_scheduler.cpp
grep -l "execute_node" src/modules/executor/node_executor.cpp
grep -l "LayeredContext" include/agenticdsl/types/layered_context.h
grep -l "ContextCompactor\|context.compact" src/core/context_compactor.cpp

# L1 (5 项): 智能体循环
ls include/agenticdsl/pdk/agent_loops/{react_loop,plan_execute_loop,fork_join_loop}.h
ls include/agenticdsl/cognitive/{cognitive_worker,domain_worker_pool}.h

# L2 (4 项): 工具层
ls include/agenticdsl/contract/itool_registry.h
ls src/common/tools/secure_tool_registry.cpp
ls include/agenticdsl/contract/itool_hook_registry.h
ls include/agenticdsl/pdk/{tool_macros,command_macros}.h 2>/dev/null || ls pdk/

# L3 (5 项): 插件层
ls src/modules/plugin/plugin_loader.cpp
ls include/agenticdsl/contract/{iagent_registry,iagent_hook_registry,iagent_composition}.h
ls include/agenticdsl/pdk/{agent_macros,safe_exec}.h

# L4 (4 项): 可观测 + 治理 (v1.1.2 +1 行为回归)
ls src/core/event_log.cpp
ls src/core/{session_manager,session_writer}.cpp pdk/session_agent/src/session_store.cpp
ls src/core/types/{tool_result,execution_result}.h
ls include/agenticdsl/testing/behavioral_regression.h
```

### 6.1.1 行为回归套件 (T14, v1.1.2 新增)

```bash
# 验证 T14 ship gate
ls include/agenticdsl/testing/behavioral_regression.h
ls src/modules/testing/behavioral_regression.cpp
ls tests/test_behavioral_regression.cpp
ls src/modules/testing/CMakeLists.txt

# ctest 验证 (6 cases / 13 assertions 预期 PASS)
cd build && ctest --output-on-failure -R test_behavioral_regression

# OpenSpec validate --strict
openspec validate 2026-08-24-adr-0061-02-behavioral-regression --strict
# 预期: "Change '2026-08-24-adr-0061-02-behavioral-regression' is valid"
```

### 6.1.2 SLM 路由 (T16, v1.2 新增 ship gate)

```bash
# 验证 T16 ship gate
ls pdk/model_router/slm_strategy/{slm_router.h,slm_router.cpp,CMakeLists.txt}
ls tests/test_model_router_slm.cpp
grep -n "add_subdirectory(slm_strategy)" pdk/model_router/CMakeLists.txt

# ctest 验证 (≥5 cases 预期 PASS)
cd build && ctest --output-on-failure -R test_model_router_slm

# OpenSpec validate --strict
openspec validate 2026-08-24-adr-0061-04-slm-routing --strict
# 预期: "Change '2026-08-24-adr-0061-04-slm-routing' is valid"

# OpenSpec spec 落地验证
ls openspec/specs/slm-routing-strategy/spec.md
# 预期: 3 requirements (SLM bucket + fallback + NoViableModel)

# 全量 ctest 零回归
cd build && ctest --output-on-failure
# 预期: 185/185 PASS
```

### 6.2 Open Gap 计数（验证 §二 = 15 项）

```bash
# G1-G9 (v1.0 继任自 defect-truth-table)
# G1: compact 仍是 in-place (破坏性)
grep -n "::rename\|O_TRUNC" src/core/session_manager.cpp

# G2: EventLog schema 校验
grep -n "schema_validate\|verify_payload" src/core/event_log.cpp

# G3-G4: Agent 完整实现 + stream 模式
grep -n "spawn_agent\|YAML.*agent\|stream.*throw" src/core/agent_registry.cpp src/modules/cognitive/agent_composition.cpp

# G5: per-agent 隔离
grep -n "per.*agent.*scope\|AgentScopedToolRegistry" src/modules/plugin/plugin_loader.cpp

# G6: Agent hook loop 集成
grep -n "IAgentHookRegistry.*apply" src/modules/cognitive/cognitive_worker.cpp src/modules/executor/node_executor.cpp

# G7: structured concurrency
grep -rn "scope_tree\|nursery" include/ src/

# G8: OTel 真客户端
grep -n "OTLP\|otlp::OtlpHttp\|opentelemetry::" src/common/observability/otel_exporter.cpp

# G9: MCP Server
grep -l "MCP\|mcp_server\|stdio.*server" src/ include/ 2>/dev/null

# G10-G15 (v1.1 Oracle 评审新增 6 项架构层缺口)
# G10: IEvaluator/RewardSignal 契约
ls docs/adr/adr-0083-evaluator-reward-contract.md
grep -l "class IEvaluator\|struct RewardSignal" include/agenticdsl/ src/

# G11: 变异治理/授权契约
grep -rn "mutation.*authority\|mutation.*governance\|self.modification.*auth" docs/adr/ include/

# G12: D10 蒸馏数据采集被死锁
ls docs/adr/adr-0080-v1-2-amendment-d10-decouple.md
grep -n "CaptureMode.*Training\|capture_prompt_bytes.*true" src/core/

# G13: ADR-0071 父方向 ADR 仍 Proposed
head -3 docs/adr/adr-0071-llm-native-agenticdsl-architecture.md | grep "🔍"

# G14: Trajectory IR 标题耦合风险 (0061-06 标题修订)
grep -A3 "### 决策" docs/adr/skill/adr-0061-06-trajectory-ir.md | grep -i "ParsedGraph\|升级"

# G15: 行为克隆器蒸馏输出格式
ls docs/adr/skill/adr-0061-13-distillation-output-format.md
grep -l "class IDistillationWriter\|struct DistillationRecord" include/agenticdsl/
```

### 6.3 ctest 全量验证（验证 §一 ctest PASS + T14 新增）

```bash
cd /workspace/project/HydraForge/build
ctest --output-on-failure -R \
  "test_session_writer|test_session_node_id_addressing|test_session_branch_cursor|test_session_extract_fork|test_event_log_query|test_event_log_capture|test_agent_registry|test_agent_hook_registry_contract|test_agent_lifecycle_emit|test_agent_composition|test_mock_bus_canonical|test_execution_result_error_taxonomy|test_context_compact_events_payload|test_otel_exporter|test_pdk_macros|test_command_registry|test_tool_coordinator|test_pdk_fork_join|test_pdk_plan_execute|test_plugin_loader|test_domain_worker_pool|test_cognitive_worker|test_behavioral_regression"
# 预期: 全 PASS (2026-08-25 验证: 185/185 tests, 0 failures; v1.1.2 当时 183/183,T14 ship 后 +1, 后续新增 +1)
# 新增 T14: test_behavioral_regression (6 cases, 13 assertions) PASS
```

### 6.4 ADR 状态基线交叉验证（验证 §一 + §二 ADR 引用）

```bash
# ADR Approved 状态基线 (§一 引用)
for adr in adr-0009 adr-0019 adr-0008 adr-0007 adr-0021 adr-0020 adr-0022 adr-0004 adr-0069 adr-0070 adr-0082 adr-0081 adr-0060 adr-0080 adr-0079 adr-0023; do
  status=$(grep -m1 "## 状态" docs/adr/${adr}*.md | grep -oE "✅|🔍|🟡|⛔|🔓" | head -1)
  echo "${adr}: ${status}"
done

# v1.1 §二 新增 ADR 状态基线 (G10/G12/G15)
for adr in adr-0083-evaluator adr-0080-v1-2-amendment; do
  status=$(grep -m1 "## 状态" docs/adr/${adr}*.md | grep -oE "✅|🔍|🟡|⛔|🔓" | head -1)
  echo "${adr}: ${status}"
done
# ADR-0061-13 在 docs/adr/skill/
grep -m1 "## 状态" docs/adr/skill/adr-0061-13-distillation-output-format.md
```

### 6.5 Oracle 评审引用验证（验证 §八 来源完整）

```bash
# Oracle session 引用 (§八 章节均应引用)
grep -n "ses_fcba5e477ffeG9wEBHVhU64J0o" docs/architecture/capability-application-map-2026-08.md
# 预期: §五 + §八 + §七 变更记录均出现

# 新增 ADR 文件存在
ls docs/adr/adr-0083-evaluator-reward-contract.md
ls docs/adr/adr-0080-v1-2-amendment-d10-decouple.md
ls docs/adr/skill/adr-0061-13-distillation-output-format.md
# 预期: 3 个文件均存在

# T14 OpenSpec change + 实现文件
ls openspec/changes/archive/2026-08-25-2026-08-24-adr-0061-02-behavioral-regression/{proposal,design,tasks}.md
ls tests/test_behavioral_regression.cpp
ls src/modules/testing/behavioral_regression.cpp
ls include/agenticdsl/testing/behavioral_regression.h
# 预期: 5 个文件均存在

# ctest T14 单独验证
cd build && ctest --output-on-failure -R test_behavioral_regression
# 预期: 1/1 PASS
```

### 6.6 ADR-0071/0074 评审通过验证 (v1.3 新增)

```bash
# 验证 6 个 ADR 状态字段已更新（兼容 ## 状态 标题 + **状态**: 内联两种格式）
grep -m1 -A 1 "状态" docs/adr/adr-0083-evaluator-reward-contract.md
# 预期: "✅ Approved (评审通过 YYYY-MM-DD)" 或 "**状态**: ✅ Approved"

grep -m1 -A 1 "状态" docs/adr/adr-0080-v1-2-amendment-d10-decouple.md
grep -m1 -A 1 "状态" docs/adr/skill/adr-0061-13-distillation-output-format.md
grep -m1 -A 1 "状态" docs/adr/skill/adr-0061-06-v1-1-amendment-trajectory-ir-decouple.md
grep -m1 -A 1 "状态" docs/adr/adr-0071-llm-native-agenticdsl-architecture.md
grep -m1 -A 1 "状态" docs/adr/adr-0074-prompt-evidence-gate.md

# 验证 G10/G12/G13/G14/G15 状态已更新
grep "G10.*Closed\|G12.*Closed\|G13.*Closed\|G14.*Closed\|G15.*Closed" docs/architecture/capability-application-map-2026-08.md

# 验证 §八 任务排期已更新
grep "✅ APPROVED Sprint" docs/architecture/capability-application-map-2026-08.md
```

---

## 七、变更记录

| 日期 | 版本 | 变更 | 操作 |
|---|---|---|---|
| 2026-08-24 | v1.0 | 新建初版 | 22 项 ship 能力 + 9 项 open gap + 3 类应用矩阵 + 13 个工程任务映射 |
| 2026-08-24 | v1.0 | 同步归档 `defect-truth-table-2026-08.md` | `git mv` 至 `archive/architecture/` + ⛛ Superseded 横幅 |
| 2026-08-24 | v1.0 | 更新 `docs/architecture/README.md` 索引 | 替换索引条目 + 追加变更记录 |
| 2026-08-24 | **v1.1** | **Oracle 评审输入（ses_fcba5e477ffeG9wEBHVhU64J0o）→ 蒸馏+自进化专题** | 6 项重大修订: (1) §二 9→15 项 gap（含 G10-G15 架构层缺口）；(2) §三 15→17 类应用（新增 B6 蒸馏 + B7 自进化）；(3) §四 C2 措辞修订（Fine-tune 改事件驱动）；(4) 新增 §八 专题章节（契约 vs 实现 + 双最小闭环 + T14-T22 + 新增 ADR 需求）；(5) §五 关系图更新（含 Oracle session 引用）；(6) 附录 Oracle 关键发现 |
| 2026-08-24 | **v1.1.2** | **任务推进: Phase A 完成** | (1) **A1 T14 行为回归套件 ship**（6 cases / 13 assertions PASS, 183/183 全量 ctest 0 回归, OpenSpec validate --strict PASS）→ §一 +1 (L4=23 项)，§六 6.3 验证命令更新；(2) **A2 ADR-0083 IEvaluator/RewardSignal 契约**（草案 🔍 Proposed, 解决 G10 缺口）；(3) **A3 ADR-0080 v1.2 amendment (D10 解耦)**（草案 🔍 Proposed, CaptureMode 三态+fail-open, 解决 G12 死锁）；(4) **A4 ADR-0061-13 蒸馏输出格式**（草案 🔍 Proposed, IDistillationWriter + DistillationRecord, 解决 G15 缺口）；(5) §六 6.5 新增 Oracle 评审引用验证；(6) §二 G10-G15 状态标记"已起草 🔍 Proposed, 待架构组评审" |
| 2026-08-24 | **v1.1.3** | **任务推进: Phase B 完成** | (1) **B3 pdk_chat_demo JSONL 调研**（`docs/architecture/pdk-chat-demo-distill-source-survey-2026-08.md` 创建，推荐 SessionWriter JSONL 而非 pdk JSON）；(2) **B4 §六 验证命令扩展**（6.1.1 T14 ship gate + 6.5 Oracle 评审引用验证）；(3) **C1+C2 状态保持 pending**（待 T14 集成验证 + ADR-0071 获批）|
| 2026-08-25 | **v1.2** | **任务推进: Phase C 完成（OpenSpec archive + ADR-0071/0074 评审会议筹备）** | (1) **C1 T14 OpenSpec change archive**（`behavioral-regression-suite` spec 创建 4 requirements，`openspec/changes/archive/2026-08-25-2026-08-24-adr-0061-02-behavioral-regression/`）；(2) **C2 T16 SLM 路由 OpenSpec change archive**（`slm-routing-strategy` spec 创建 3 requirements，`openspec/changes/archive/2026-08-25-2026-08-24-adr-0061-04-slm-routing/`）；(3) **C3 L3 plugin 计数扩展**：IModelRouter 实现路由策略由 3 → 4（+ SLM，v1.1.3 §一未变 → 现已 ship）；(4) **C4 ADR-0071/0074 评审会议纪要就绪**（`docs/architecture/adr-review-minutes/adr-0071-0074-distillation-review-2026-08-24.md` 251 行 + 8 议程 + 决策点 + 风险评估 + 检查清单）；(5) **C5 §六 6.1.2 新增 T16 SLM ship gate 验证**；(6) **C6 §一 L3 第 4 个路由策略 SLM 标注 ship 状态**；总计：本会话 ship 能力 L4 +1 (行为回归)、L3 plugin 数量 +1 (SLM 路由 .so)，§一总能力 23 项保持（SLM 归属 L3 现有 IModelRouter 契约实现） |

| 2026-08-25 | **v1.3** | **评审会议通过: 6 个 ADR 决议落地** | (1) ADR-0083 ✅ Approved → G10 Closed; (2) ADR-0080 v1.2 amendment ✅ Approved → G12 Closed; (3) ADR-0061-13 ✅ Approved → G15 Closed; (4) ADR-0071 ✅ Approved (Promotion) → G13 Closed; (5) ADR-0074 ✅ Approved (Promotion); (6) ADR-0061-06 v1.1 amendment ✅ Approved → G14 Closed; (7) §八 T17/T15/T19/T20/T21 启动 Sprint 排期确定 (Sprint 24-26); (8) §八.5 优先级排序从 Oracle 预审更新为评审通过后 Sprint 排期表; (9) §二 G10/G12/G13/G14/G15 状态 🔴 → ✅; 决议依据: `docs/architecture/adr-review-minutes/resolution-draft-2026-08-25.md` §八 会议决议记录 |
| 2026-08-25 | **v1.3.1** | **Sprint 24 Step 3 文档结构 drift 收口 (OpenSpec `2026-08-25-cap-map-v1-3-drift-fix`)** | **12 项声明 drift 全修复** (drift-1 §七/§八 顺序互换、drift-2 §六.6 位置 + 路径、drift-3 §二 词汇表 ✅ Closed、drift-4 §二 标题去 "未 ship"、drift-5 §八 闭环 1 G15、drift-6 §八.5 重复排期、drift-7 ADR-0071/0074 footer、drift-8 ctest 184→185、drift-9 §一覆盖 Sprint 23、drift-10 §三 22→23、drift-11 性质标记、drift-12 L4 表头加注) **+ 4 项扩展修复** (闭环 1 G10/G12 同行 stale、闭环 2 G10 同行 stale、§一 标题 22→23)；章节顺序恢复 标准 §一→§二→§三→§四→§五→§六→§七→§八；ADR-0071/0074 footer 同步 "Promotion 评审通过 2026-08-25"；ctest 185/185 PASS 零回归；openspec validate --strict PASS；ADR lint + docs_drift_audit 零增加；本会话 ship 后保持 Sprint 24 Pre-Launch 治理范式 |
| 2026-08-26 | **v1.4** | **G11 Approved 落地: ADR-0084 起草启动 + T19 Phase 1 只读化 + 5 文件同步** | (1) §一版本 v1.3 → v1.4 + 最后验证 2026-08-26；(2) §二 G11 状态 🔒 Blocked → 🔍 Proposed（ADR-0084 起草中, Sprint 25 W1, issue #14 ✅ Approved 2026-08-26）；(3) §二 Oracle 评审关键发现段补充 G10/G11 Closed/Proposed 状态；(4) §三 B7 G11 契约状态同步；(5) §五新增 3 个 Oracle session 引用（`ses_fc640ea84ffe0f4dyYTa4aFjiL` 战略评估 + `ses_fc41537bbffeC35NKqgvzn4m1c` Self-Review 预审 + `ses_fc3e070c0ffeIVgAhsgx2pNXFa` 深度审查）；(6) §七 v1.4 changelog（commit `docs(cap-map): v1.4 - G11 Proposed + ADR-0084 起草启动 + T19 Phase 1 只读化`）；(7) §八.3 T19 row 加 Phase 1 只读反思约束；(8) §八.4 新增 ADR 需求表标注 G11 方向已 Approved → adr-0084-mutation-governance-contract.md；(9) §八.5 排期新增 ADR-0084 起草行（Sprint 25 W1 启动周 4a, Sprint 26 末评审 11, Sprint 26 末 G11 ✅ Closed 12）；(10) §八.6 风险段更新"变异治理缺位"为"已解决"+ ADR-0071 风险同步更新；(11) Oracle Deep Review（session `ses_fc3e070c0ffeIVgAhsgx2pNXFa`）6 项风险点 + 验证检查清单。决议依据：GitHub issue #14 ✅ Approved + Oracle 三轮评审 session 引用 |
| 2026-08-26 | **v1.5** | **新增自进化架构边界并同步能力地图** | 新增 [`self-evolution-architecture-2026-08.md`](self-evolution-architecture-2026-08.md)，定义受治理的单编排器自进化闭环、轨迹/聚合指标边界、信用分配与预测误差缺口、混合更新器、在线教师蒸馏训练期约束、稳定性与协同进化 promotion criteria；同步 §八 引用、闭环状态和维护职责 |
| 2026-08-26 | **v1.6.1** | **ADR-0083 IEvaluator/RewardSignal 代码 ship 同步** | (1) §二 G10 "🔍 Proposed 代码 ship 待办" → "✅ Closed (ADR-0083 Approved + 代码 ship)"; (2) §八.2 闭环 1/2 "评估信号环" 契约状态 → ✅ Approved + 实现状态 → ✅ IEvaluator 已 ship (12 cases / 31 assertions); (3) §八.2 闭环 1 "闭环状态" 由 "5 环断裂" → "4 环断裂"; (4) 头部版本 v1.6 → v1.6.1 |
| 2026-08-26 | **v1.7** | **G11 ✅ Closed: ADR-0084 Approved + MutationGovernor V1 gate-and-audit 代码 ship** | (1) §二 G11 "🔍 Proposed (ADR-0084 文件已创建)" → "✅ Closed (ADR-0084 Approved + V1 ship 2026-08-26, commit `a2b2d52`)"; (2) §三 B7 行 "G11 🔍 (ADR-0084 起草中)" → "G11 ✅ Closed (V1 code ship, 2026-08-26)" + T19 Phase 2 commit 解锁; (3) §八.2 闭环 5 "变异治理/授权" 契约状态 → ✅ Approved + 实现状态 → ✅ MutationGovernor 已 ship (13 cases / 139 assertions), "闭环状态" 由 "4 环断裂" → "3 环断裂"; (4) §八.5 排期 12 项追加 "✅ 已完成 (2026-08-26 ship)" 标记; (5) §八.3 T19 行 / §八.4 ADR 需求表 / §八.6 风险段 G11 状态同步 Closed; (6) 头部版本 v1.6.1 → v1.7。决议依据: OpenSpec change `g11-closed-adr-0084-approved` + issue #14 关闭 (2026-08-26) |
| 2026-08-27 | **v1.8** | **T17 ✅ SHIP: SkillCompiler V1 (ADR-0061-03) 代码落地** | (1) §一 L4 +1 (#24 SkillCompiler V1), 总能力 23 → 24; (2) §八.2 闭环 2 第 4 环 "变异对象" 实现状态 ⚙ 无代码 → ✅ T17 V1 ship, "闭环状态" 由 "3 环断裂" → "2 环断裂" (剩轨迹抽取 + 发布/热加载); (3) §八.3 E 轨 T17 行 APPROVED → ✅ SHIP; (4) 头部版本 v1.7 → v1.8。决议依据: OpenSpec change `2026-08-24-adr-0061-03-skill-compiler` (15 cases / 61 assertions PASS, ADR-0068 附录 A v1.2.2 注册 3 个 skill.compilation.* 主题) |
| 2026-08-27 | **v1.9** | **T15 ✅ SHIP: Trajectory IR V1 (ADR-0061-06 v1.1) 代码落地** | (1) §一 L4 +1 (#25 Trajectory IR V1), 总能力 24 → 25; (2) §八.2 闭环 2 第 2 环 "轨迹抽取" 实现状态 ⚙ 无代码 → ✅ T15 V1 ship, "闭环状态" 由 "2 环断裂" → "1 环断裂" (仅剩发布/热加载); (3) §八.3 E 轨 T15 行 APPROVED → ✅ Completed/SHIP; (4) 头部版本 v1.8 → v1.9; (5) ADR-0061-06 v1 头部追加 ship 证据段 + v1.1 amendment 状态 → ✅ Approved + Shipped。决议依据: OpenSpec change `t15-trajectory-ir` (9 cases / 55 assertions PASS, commits `3ba9f2c`/`53a0f17`/`1fd5c4b`/`7b24973`, ParsedGraph 零修改, SkillCompiler TrajectoryPlaceholder → TrajectoryIR::hash 升级) |
| 2026-08-27 | **v2.0** | **IEvaluator V2 ship (OpenSpec `evaluator-v2-composite`)** | (1) §一 25→26 项能力, L4 +1 (#26 IEvaluator V2: BehavioralEquivalence + Composite); (2) §二 G10 评估信号 V2 层落地 (BehavioralEquivalence 复用 T14 fingerprint + Hotelling T², Composite 多评估器加权聚合); (3) §八 闭环 1 评估信号行更新 V2 ship; (4) 头部 v1.9 → v2.0 + 生成/最后验证 2026-08-27; (5) 测试: test_evaluator V2 8 cases / 18 assertions PASS, IEvaluator 接口零修改, V1 12 cases 零回归, ctest 动态基线 0 回归 |
| 2026-08-27 | **v2.1** | **T19 GEPA Phase 2 commit ship (OpenSpec `t19-gepa-phase2-commit`)** | (1) §一 26→27 项能力, L4 +1 (#27 GEPALoop 反思循环 MVP); (2) §三 B7 行 "T19 Phase 2 commit 已解锁" → "T19 Phase 2 committed 2026-08-27, B7 → ✅ Completed"; (3) §八 T19 行 Phase 1 只读反思约束解除 + Phase 2 committed; (4) §八.2 闭环 2 变异提交环接通 (GEPALoop 经 MutationGovernor 授权 commit); (5) 头部 v2.0 → v2.1 + 最后验证 2026-08-27; (6) 测试: test_gepa_phase2 14 cases PASS (含 3 E2E), 既有 7 契约零修改, ADR-0068 附录 A v1.3 注册 6 个 gepa.* 主题, ctest 动态基线 0 回归 |
| 2026-08-28 | **v2.2** | **T21 Prompt Evidence Gate ship (OpenSpec `t21-prompt-evidence-gate`)** | (1) §一 27→28 项能力, L4 +1 (#28 Prompt Evidence Gate: Go/No-Go 阈值 + parse-valid + 两阶段注入 + JSONL + llm.dsl.* 事件); (2) §八 T21 行 APPROVED → ✅ SHIP; (3) 头部 v2.1 → v2.2 + 生成/最后验证 2026-08-28; (4) 测试: test_prompt_evidence_gate 19 cases / 338 assertions PASS, 30+ few-shot + 50+ golden 实际生成 (非占位), 既有 7 契约零修改, ADR-0068 附录 A v1.4 注册 3 个主题 (2 llm.dsl.* + 1 prompt.*), ctest 动态基线 0 回归 |
**后续追踪**:
- **下一修订触发**: (1) 任意 §二 open gap ship；(2) 任意 §四/§八工程任务完成；(3) 新应用类型立项；(4) Phase 6 Candidate B 启动；(5) T14-T22 任一 ship/promotion；(6) 5 个新增 ADR 任一获批；(7) **ADR-0071/0074 评审会议召开 + 决议记录**
- **定期审计**: 每 Sprint 收官同步（`scripts/sprint-closeout.sh` Step 8 加本表交叉检查）
- **预防漂移**: `scripts/docs-drift-detect.sh`（B.2 计划）自动校验 §一/§二/§三/§四/§八 计数与代码一致
- **本周关键决策**: ADR-0071/0074 架构评审会议召集 + T17 SkillCompiler 等 ADR-0071 获批 + T15 Trajectory IR 启动

---

**审批与维护**:
- v1.0 提议：2026-08-24（本会话）
- v1.1 修订：2026-08-24（Oracle 评审输入 ses_fcba5e477ffeG9wEBHVhU64J0o）
- 维护者：架构组
- 审查频率：每 Sprint 收官
- 与 `docs/architecture/adr-implementation-status-gap-analysis.md` 交叉验证
- 继任关系：`defect-truth-table-2026-08.md` → 本表（视角从缺陷 → 能力-应用）

## 八、Agent 蒸馏与自进化专题（v1.2 新增）

> **评审来源**: Oracle session `ses_fcba5e477ffeG9wEBHVhU64J0o` (2026-08-24)
> **架构边界**: 详见 [`self-evolution-architecture-2026-08.md`](self-evolution-architecture-2026-08.md)。本节维护能力、Gap 和任务映射；不重复定义自进化运行时契约。
> **章节定位**: 蒸馏 + 自进化是 Phase 6 核心方向，且涉及未识别的架构层缺口，独立成章
> **任务编号**: 续接 §四 T1-T13，本章任务命名为 T14-T22（避免双命名空间冲突）

### 8.1 契约层 vs 实现层 分界

> **Oracle 关键修正**: §一 的 L0-L4 分层是**工程维度**（部署/运行时分层），**不是**架构契约分层。架构契约层 = `include/agenticdsl/contract/*.h` + `types/` + `policy/` + ADR 治理的 schema/主题注册表。

| 维度 | 内容 | 改动规则 |
|---|---|---|
| **架构契约层**（ADR 治理）| IInteractionBus / ILLMProvider / IToolRegistry / IAgentRegistry / IAgentHookRegistry / IAgentComposition / IEnvBackend / IApprovalHandler / LayeredContext 5 层 / ToolResult + 17 ErrorCode / Session 4-scope / EventLog schema + EventBuilder 主题注册表（ADR-0068）/ **D10 Distillation Capture**（ADR-0080 v1.1：prompt_text/response_text 字段 + capture_prompt_bytes + fail-closed agent_id）| **改它需要新 ADR 或 ADR amendment** |
| **工程实现层**（可替换）| InMemoryBus / MockLLMProvider / LlamaAdapter / SecureToolRegistry / 3 个 model_router 策略 .so / SkillInterpreter posix_spawn 后端（ADR-0066 允许换隔离机制）/ LocalEnv/DockerBackend / pdk_chat_demo 全部接线 / MockBus fixture | **实现可换，不需要 ADR** |

### 8.2 两个最小闭环 readiness（替代 60%/30% 粗评估）

> **Oracle 评估方法学**: 能力计数加权失真。改用"最小闭环断链计数"——列出端到端闭环所需的每一环，标注每环状态。

#### 闭环 1：Agent 蒸馏（教师→学生能力迁移）

| 环节 | 所需能力/契约 | 契约状态 | 实现状态 | 阻塞链 |
|---|---|---|---|---|
| 1. 轨迹采集 | ADR-0080 D10 Distillation Capture | ✅ Approved | ⚙ 未启用 | G12: ✅ Closed (ADR-0080 v1.2 解耦) |
| 2. 数据集结构 | ADR-0074 D6 JSONL | ✅ Approved (Promotion) | ⚙ 无代码 | G13: ✅ Closed (ADR-0071 Promotion) |
| 3. 评估信号 | IEvaluator / RewardSignal (ADR-0083) | ✅ Approved (代码 ship 2026-08-26) | ✅ IEvaluator V1+V2 已 ship (test_evaluator.cpp V1 12 cases / 31 assertions + V2 8 cases / 18 assertions, 2026-08-27) | G10: ✅ Closed |
| 4. 训练管线 | ADR-0078 LoRA | 🔍 Proposed | ❌ 无 | 🔒 外部阻塞：AgenticMind 项目 4-6 周 |
| 5. 模型服务 | ADR-0034 model_router + llama_engine | ✅ Approved | ✅ 已 ship | 无 |
| 6. 等价性回归门 | ADR-0061-02 行为回归 | ✅ Approved | ✅ T14 ship (v1.2, 6 cases / 13 assertions) | 工程任务 T14 |
| 7. 蒸馏输出格式 | ADR-0061-13 | ✅ Approved (2026-08-25) | ❌ IDistillationWriter 类代码不存在 (grep 0 命中, 2026-08-26 自审识别) | G15 状态需补 OpenSpec task ship 输出格式类 |

**闭环状态**: **7 环中 4 环仍断裂**（采集未启用 + 训练管线外部阻塞 + 蒸馏输出代码未 ship + 运行时接线未完成）；评估信号环已 ship（2026-08-26），契约多数已 Approved 但其余代码 ship 不充分，自进化方向"60% 评估"**仍高估**。

**Oracle 路径建议**: 先用 pdk_chat_demo SessionManager JSONL 作**临时数据源**（不依赖 D10 启用），同时启动 T14（行为回归门禁）+ ADR-0071/0074 评审会。

#### 闭环 2：Agent 自进化（经验→改进）

| 环节 | 所需能力/契约 | 契约状态 | 实现状态 | 阻塞链 |
|---|---|---|---|---|
| 1. 观测/事件 | EventLog (能力 20) | ✅ | ✅ | 无 |
| 2. 轨迹抽取 | Trajectory IR（ADR-0061-06 v1.1） | ✅ Approved + Shipped (2026-08-27) | ✅ T15 V1 ship (9 cases / 55 assertions; ParsedGraph 零修改) | T15 ✅ |
| 3. 评估信号 | IEvaluator / RewardSignal (ADR-0083) | ✅ Approved (代码 ship 2026-08-26) | ✅ IEvaluator 已 ship | G10: ✅ Closed |
| 4. 变异对象（prompt/skill）| SkillCompiler（ADR-0061-03） | ✅ Approved | ✅ T17 V1 ship (2026-08-27, 15 cases / 61 assertions) | T17 ✅ |
| 5. 变异治理/授权 | 变异授权契约 (ADR-0084) | ✅ Approved (代码 ship 2026-08-26) | ✅ MutationGovernor 已 ship (13 cases / 139 assertions) | G11: ✅ Closed (2026-08-26) |
| 6. 等价性回归门 | ADR-0061-02 行为回归 | ✅ Approved | ✅ T14 ship (v1.2) | T14 |
| 7. 发布/热加载 | ADR-0076 MCP hot-reload + ADR-0038 dynamic config | 🔍 Proposed | ❌ 无 | 外部依赖 |

**闭环状态**: **7 环中 1 环仍断裂**（发布/热加载）；轨迹抽取已 ship（T15 TrajectoryIR V1, 2026-08-27），评估契约、变异治理、回归门与变异对象生成器已经具备 (T17 SkillCompiler V1 ship 2026-08-27)，不能据此宣称自进化端到端可用。信用分配和稳定性机制还属于更高阶协同进化前置能力，详见 [`self-evolution-architecture-2026-08.md`](self-evolution-architecture-2026-08.md)。

### 8.3 T14-T22 任务映射（E 轨工程 + R 轨研究）

> **Oracle 关键建议**: 拆双轨复用项目 ADR-0051 spike 模式——E 轨走正常 OpenSpec ship gate，R 轨 spike 立项设 promotion criteria 不进 Sprint 承诺。TD9 Fine-tune 改"事件驱动"而非排期。

#### E 轨（工程，立即可启动）

| 任务 | 估时 | 解锁能力 | 解锁应用 | 前置 | 来源 |
|---|---|---|---|---|---|
| **T14**: ~~实施 ADR-0061-02 行为回归套件（AgentAssay 风格，Hotelling T²）~~ → **✅ SHIP (v1.2, 2026-08-25)** | 1 sprint ✅ | 等价性评估门（所有变异循环的安全前提）| B6 蒸馏 + B7 自进化 + 全部 A/B/C 类应用的回归资产 | 无（ADR Approved P0）| G6 衍生 → OpenSpec archived `2026-08-25-2026-08-24-adr-0061-02-behavioral-regression` |
| **T15**: ~~✅ **APPROVED** Sprint 25 启动~~ → **✅ Completed / SHIP (v1.9, 2026-08-27)** — Trajectory IR (ADR-0061-06 v1.1)| 2 sprint → 实际 1 sprint ✅ | 标准化轨迹数据格式 | B6 蒸馏数据标准化 | T14 + G14 标题修订评审 | G14 + G15 → OpenSpec archived `t15-trajectory-ir` |
| **T16**: ~~SLM 路由（基于已 shipped IModelRouter 契约，新策略 .so）~~ → **✅ SHIP (v1.2, 2026-08-25)** | 1 sprint ✅ | 自动决定何时用小模型 | 蒸馏部署 + 成本优化 | 无（model_router ✅）| T3 v1.1 改名 → OpenSpec archived `2026-08-25-2026-08-24-adr-0061-04-slm-routing` |
| **T17**: ~~✅ APPROVED Sprint 24 启动~~ → **✅ SHIP (v1.3, 2026-08-27)** — SkillCompiler V1 (ADR-0061-03)| 1 sprint ✅ (原估 2) | 自动生成改进 prompt | B7 自进化的变异对象 | T14 + ADR-0071 获批 | T4 v1.1 改名 → OpenSpec archived `2026-08-24-adr-0061-03-skill-compiler` |

#### R 轨（研究，以 spike 模式立项，设 promotion criteria）

| 任务 | 估时 | 解锁能力 | 解锁应用 | 前置（promotion criteria）| 来源 |
|---|---|---|---|---|---|
| **T18**: PASTE 推测执行（ADR-0061-07）spike | 1-2 sprint | 并行假设验证 | C5 (高级) 自进化加速 | spike: 不破坏调度器确定性语义 | T5 v1.1 改名 |
| **T19**: ✅ **SHIP** Sprint 24 末 (R 轨 spike) (ADR-0083 ✅ + ADR-0071 ✅) — GEPA MVP| 2-3 sprint (R 轨 spike) | 失败→反思→修订 prompt | B7 自进化基础 | T14 + IEvaluator 契约（G10）+ 变异治理（G11 ✅ Closed ADR-0084 Approved 2026-08-26）+ ADR-0074 prompt 资产 **+ ~~Phase 1 只读反思约束~~ Phase 2 committed 2026-08-27** | T6 v1.1 改名 |
| **T20**: ✅ **APPROVED** Sprint 26 末 (R 轨 spike) (ADR-0083 ✅ + T15 ✅) — AFlow MCTS| 1-2 月 (R 轨 spike) | 工作流自动优化 | C2 自进化高级 | T15 + IEvaluator 契约（G10）+ spike: 评估信号有可比性 | T7 v1.1 改名 |
| **T21**: ✅ **SHIP** 2026-08-28 (ADR-0074 ✅ + ADR-0083 ✅) — Prompt Evidence Gate| 1 月 | prompt 质量门控 | T19/T20 前置 | ADR-0071 父获批 + D1 30+ few-shot | T8 v1.1 改名 |
| **T22**: Fine-tune 训练管线（ADR-0078）| **事件驱动**（AgenticMind 回流触发）| self-improvement agent | C2 自进化 | 外部阻塞解除 + T14 行为回归 | T9 v1.1 改事件驱动 |

### 8.4 新增 ADR 需求清单

| 拟新立 ADR | 性质 | 解决 Gap | 估时 | 优先级 |
|---|---|---|---|---|
| **ADR-0061-13** 蒸馏输出格式（行为克隆器契约） | 架构 | G15 | 1 sprint 草案 | P0 |
| **IEvaluator / RewardSignal 契约** | 架构 | G10 | 1 sprint 草案 | P0 |
| **变异治理/授权契约** | 架构 | G11 | 2 sprint 草案（含安全审计）| P1 | ✅ **Approved + V1 ship (2026-08-26, commit `a2b2d52`) → adr-0084-mutation-governance-contract.md, G11 ✅ Closed** |
| **ADR-0080 v1.2 amendment**（解耦 D10 capture 与 ADR-0081 scrub）| 架构 | G12 | 0.5 sprint 草案 | P0 |
| **ADR-0061-06 标题修订**（Trajectory IR 独立序列化视图 vs 升级 ParsedGraph）| 架构 | G14 | 0.5 sprint 草案 | P1 |

### 8.5 评审通过后优先级排序 (2026-08-25 评审 + 2026-08-26 G11 Approved 补充)

```
Sprint 24 启动周:
  1. ADR-0071 v1.1 amendment 起草 (0.5 sprint)
  2. ADR-0080 v1.2 amendment ship (0.5 sprint)
  3. T17 SkillCompiler 骨架 (1 sprint)

Sprint 24 末:
  4. T19 GEPA R 轨 spike 启动 (Phase 1 只读反思约束, 不执行 commit(PromptEdit))

Sprint 25 启动周:
  4a. ADR-0084 mutation-governance-contract 起草 (2 sprint, P1, issue #14 Approved 2026-08-26)
  5. ADR-0083 IEvaluator ship (1 sprint)
  6. ADR-0061-13 蒸馏输出 ship (1 sprint 并行)
  7. T15 Trajectory IR 启动 (G14 ✅)
  8. T21 Prompt Evidence Gate 启动

Sprint 26:
  9. T15 + T21 完整 ship
  10. T20 AFlow R 轨 spike 准备
  11. ADR-0084 mutation-governance-contract ship (2 sprint 完成)

Sprint 26 末:
  12. ADR-0084 mutation-governance 评审通过 → G11 ✅ Closed + T19 Phase 2 commit 启动 ✅ **已完成 (2026-08-26 ship, commit `a2b2d52`)**
```

### 8.6 Oracle 风险提示

| 风险 | 影响 | 缓解 |
|---|---|---|
| Trajectory IR 按 0061-06 标题"升级 ParsedGraph"会把训练数据格式耦合进运行时图结构 | 训练/运行时双向耦合 | 改独立序列化视图（详见 G14）|
| D10 死锁不解决，蒸馏在数据面永远停在第一步 | 蒸馏方向无法启动 | pdk_chat_demo SessionManager JSONL 作临时数据源 + ADR-0080 v1.2 amendment |
| R 轨若混入 Sprint 承诺，会重演 TD9 式虚假排期 | 排期失信 | R 轨 spike 模式 + promotion criteria，明确"事件驱动"非排期 |
| 变异治理缺位，Agent 自修改无审计无授权 | 安全攻击面 | ✅ **已解决**（2026-08-26）：G11 ✅ Closed（ADR-0084 Approved + V1 gate-and-audit 代码 ship, commit `a2b2d52`, issue #14 已关闭）+ T19 GEPA spike Phase 2 commit 已解锁 |
| ⛛ Superseded 历史风险（2026-08-26 G11 已启动起草后保留作 audit trail） | — | 原文："G11 契约强制（所有 R 轨 task 启动前置）" — Oracle session `ses_fc640ea84ffe0f4dyYTa4aFjiL` + `ses_fc41537bbffeC35NKqgvzn4m1c` + `ses_fc3e070c0ffeIVgAhsgx2pNXFa` 三轮评审落定 G11 起草路径 |
| ADR-0071 父未批，4 个子项全冻结 | 4 个 TD 项依赖 | ✅ **已解决**（2026-08-25 评审通过）：ADR-0071 ✅ Approved + T17/T15/T19/T20/T21 排期已 ship |

---
