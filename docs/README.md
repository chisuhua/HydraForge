# HydraForge 文档索引

## 目录结构

```
docs/
├── adr/              # Architecture Decision Records (架构决策, 阶段化分类)
│   ├── plugin/       # Plugin 化实施候选 ADR (plugin-candidate, 2026-06-16+)
│   └── skill/        # ADR-0061 技能演化子项 (12 个)
├── adr-management/   # ADR 元数据: 状态词汇表 + 关联性分析 (自 2026-06-16 移出 adr/)
├── architecture/     # 架构工作组文档 (五层模型/缺失能力分析/进化管线, ADR 的证据输入)
├── specs/            # 规范文档 (当前有效版本)
├── guides/           # 用户和开发者指南
├── design/           # 设计文档
├── research/         # 调研报告 (pi-agent 对比, SOTA 调研)
├── audits/           # 审计报告 (drift gate, sanitizer 复验, ship gate 验证)
├── adversarial-reviews/ # 对抗性评审决议记录
├── service-composition/ # Phase 6 服务组合 spike 产出
├── skills/           # 文档化技能说明
├── archive/          # 归档 (过期版本)
├── proposals/        # AgenticDSL 语言演进提案 (18 docs: 14 话题子目录 + 4 根文件)
├── GOVERNANCE.md     # 文档治理方案 (分层权限 + 任务驱动流水线)
└── active-status.md  # [统一看板] 当前活跃变更状态追踪 (替代 roadmap-status.md + implementation-roadmap.md)
```

---

## adr/ - Architecture Decision Records

架构决策记录，记录重要的架构决策及其背景、权衡。

| 文件 | 议题 | 状态 |
|------|------|------|
| `adr-0001-illm-provider-streaming-interface.md` | ILLMProvider 流式接口 | ✅ Approved |
| `adr-0002-eventbus-bounded-queue.md` | EventBus 有界队列 | ❌ Not Implemented (V1 归档, Phase 1 改用 ADR-0019 IInteractionBus MVP 承担事件通信) |
| `adr-0003-dslengine-thread-safety.md` | DSLEngine 线程安全 | ✅ Approved |
| `adr-0004-toolregistry-security.md` | ToolRegistry 安全模型 | ✅ Approved |
| `adr-0005-llm-backend-config-factory.md` | LLM 后端配置与工厂 | ✅ Approved |
| `adr-0006-harness-engine-thread-model.md` | HarnessEngine 后台线程 | ⛔ Superseded (被 ADR-0020 替代) |
| `adr-0007-context-compression.md` | 上下文压缩机制 | ✅ Approved (Sprint 22 context-compactor ship 2026-08-13) |
| `adr-0008-structured-context.md` | 结构化 Context | ✅ Approved (2026-06-12 LayeredContext 实现完成) |
| `adr-0009-dsl-standard-library.md` | DSL 标准库规划 | ✅ Approved |
| `adr-0019-iinteraction-bus-mvp.md` | IInteractionBus 接口与 TUI Chat MVP | 🟡 Partial (MVP ship 2026-06-24; 事件发射契约缺位, 见 layer-based-missing-capabilities-analysis.md X1) |
| `adr-0020-thread-model-isolation.md` | 多智能体线程模型与隔离策略 | ✅ Approved (2026-06-24, Sprint 5 ship) |
| `adr-0021-pdk-design.md` | Plugin Development Kit (PDK) 设计 | ✅ Approved (2026-06-24, Sprint 5 ship) |
| `adr-0022-plugin-loading.md` | 插件加载机制 | ✅ Approved (2026-06-24, Sprint 5 ship) |
| `adr-0023-tool-result-standard.md` | ToolResult 标准化 | ✅ Approved (2026-06-24, Sprint 5 ship) |
| `adr-0031-execution-policy.md` | 执行策略 | 🟡 Partial (C3 P1-P2 ✅ Approved 2026-07-31; C4 P3-P4 🟡 active; §决策 8 4 项 defer 至 C6) |
| `adr-0033-session-hierarchy.md` | 会话层次结构 | ✅ Approved (Sprint 15 C5, 2026-07-02) |
| `adr-0035-inference-engine-plugin-spec.md` | 推理引擎 PDK Plugin 规范 | ✅ Approved (2026-07-10 — C14 `phase5-llama-engine-plugin` ship) |
| `adr-0040-inference-plugin-build-strategy.md` | 推理引擎 Plugin 构建与交付策略 | ✅ Approved (2026-07-10 — C14 `phase5-llama-engine-plugin` ship) |
| `adr-0041-pluginloader-lifecycle-extension.md` | PluginLoader 生命周期扩展 (pdk_plugin_init / fini 钩子) | ✅ Approved (2026-07-10 — C16 `phase5-illmprovider-call-chain-v2` ship) |
| `adr-0043-pdk-tool-naming-convention.md` | PDK 工具命名约定规范 | ✅ Approved (2026-07-10 — C13/C14 D3 决策已应用) |
| `adr-0044-inference-plugin-security-model.md` | 推理引擎 Plugin 安全模型 | ✅ Approved (2026-07-10 — C14 `phase5-llama-engine-plugin` ship, 三层安全模型应用) |
| `adr-0030-async-runtime-v2.md` | 异步运行时 V2 | 🟡 Partial (2026-07-23 提升, C2 ship) |
| `adr-0037-causal-ordering.md` | 因果排序 (CausalClock) | 🟡 Partial (2026-07-27 提升, CausalClock + emit auto-tick ship; 分布式向量时钟 defer) |
| `adr-0038-dynamic-config-interface.md` | 动态配置接口 | 🔍 Proposed |
| `adr-0039-performance-metadata-contract.md` | 性能元数据契约 | 🔍 Proposed |
| `adr-0042-illmprovider-evolution-path.md` | ILLMProvider 演进路径 | 🔍 Proposed (C16 部分决策已 ship) |
| `adr-0045-orchestration-plugin-spec.md` | 编排 Plugin 规范 | 🔍 Proposed |
| `adr-0046-plugin-communication-protocol.md` | 插件间通信协议 | 🔍 Proposed (~35% 实施率) |
| `adr-0050-phase6-strategic-evaluation.md` | Phase 6 战略评估 | ✅ Approved (2026-07-23 — Solo Dev 重估, Candidate B) |
| `adr-0051-phase6-pdk-composition-spike.md` | Phase 6 PDK 组合 Spike | ✅ Approved (experimental, 2026-07-15 — C19 ship) |
| `adr-0052` ~ `adr-0065` (14 个 Phase 6 ADR) | Agent Manifest / Descriptor / Capability Discovery / Skill 隔离 / Wasm 运行时 / 生命周期 / Schema 校验 / 跨进程协议 / 组合协议 / 进化固化 / Marketplace / OTel / Conformance / Python PDK | ✅ Approved (2026-07-15 架构评审; 0055/0060 已实施, 余 12 个零代码) |
| `adr-0066-skill-interpreter-arch.md` | SkillInterpreter 架构 | 🟡 Partial (V1 ship 2026-07-22, V2 deferred) |
| `adr-0002-impl-scope-audit.md` | ADR-0002 实施范围审计 (OpenSpec change `docs-code-drift-audit-2026-
| `adr-0004-impl-scope-audit.md` | ADR-0004 实施范围审计 (同上) | 📋 审计补充 |
| `adr-0001-illm-provider-streaming-interface-impl-scope.md` | ADR-0001 实施范围审计 (C9) | 📋 审计补充 |
| `adr-0003-dslengine-thread-safety-impl-scope.md` | ADR-0003 实施范围审计 (C9) | 📋 审计补充 |
| `adr-0004-toolregistry-security-impl-scope.md` | ADR-0004 实施范围审计 (C9) | 📋 审计补充 |
| `adr-0005-llm-backend-config-factory-impl-scope.md` | ADR-0005 实施范围审计 (C9) | 📋 审计补充 |
| `adr-0007-context-compression-impl-scope.md` | ADR-0007 实施范围审计 (C9) | 📋 审计补充 |
| `adr-0008-structured-context-impl-scope.md` | ADR-0008 实施范围审计 (C9) | 📋 审计补充 |
| `adr-0019-iinteraction-bus-mvp-impl-scope.md` | ADR-0019 实施范围审计 (C9) | 📋 审计补充 |
| `adr-0020-thread-model-isolation-impl-scope.md` | ADR-0020 实施范围审计 (C9) | 📋 审计补充 |
| `adr-0022-plugin-loading-impl-scope.md` | ADR-0022 实施范围审计 (C9) | 📋 审计补充 |
| `adr-0023-tool-result-standard-impl-scope.md` | ADR-0023 实施范围审计 (C9) | 📋 审计补充 |
| `adr-0033-session-hierarchy-impl-scope.md` | ADR-0033 实施范围审计 (C9) | 📋 审计补充 |
| `adr-0067-layered-plugin-architecture-split.md` | L2/L3/L4 分层插件架构拆分 | ✅ Approved (追溯性正式化, 2026-07-23, 源自 `docs/specs/architecture.md` §2.3) |
| `adr-0068-event-emission-contract.md` | 事件发射契约 (Canonical Topic Registry + 7 幻影主题强制发射点 + EventBuilder) | ✅ Approved (2026-08-03 — Wave 1 §1-§5 ship + Appendix A v1.2.2, 2026-08-27) |
| `adr-0069-tool-coordinator-hooks.md` | ToolCoordinator Hook 注入点 (pre/post 双列表 + IToolHookRegistry + HookErrorPolicy) | 🟡 Partial (2026-08-04 — middleware 改造 + budget_agent pre-hook + 5 类测试已 ship; 待 HookErrorPolicy amendment) |
| `adr-0070-declare-command.md` | PDK Plugin 命令/快捷键注册 (Command≠Tool + DECLARE_COMMAND + ICommandRegistry) | 🔍 Proposed (2026-07-31, D4 立项, 实施排期 Wave 1) |
| `adr-0071-llm-native-agenticdsl-architecture.md` | LLM-native AgenticDSL 架构 (LLM 作为 DSL 作者, 3 平面 Operator/DSL/Backend, 派生 6 个子 ADR/Change) | ✅ Approved (评审通过 2026-08-25, Promotion, 顶层方向 ADR, 锚定 Phase 6+ 演化) |
| `adr-0073-tool-json-schema-contract.md` | Tool JSON Schema 契约 (JSON Schema 2020-12, input_schema/output_schema 字段 + nlohmann validator + DECLARE_TOOL 自动生成) | 🟡 Partial (Phase 6a manifest 边界部分采纳, 详见 adr-0073-impl-scope-audit.md; D2/D3/D4 属 Phase 6c C8/C9) |
| `adr-0074-prompt-evidence-gate.md` | Prompt Engineering + Evidence Gate (D1 30+ few-shot + D2 50+ golden + D3 3 模型 baseline + D4 Evidence Gate + D5 两阶段注入 ≤8k + D6 JSONL + D7 失败事件) | ✅ Approved (评审通过 2026-08-25, Promotion, Wave 2 Phase 2.2, 派生自 ADR-0071 §D5) |
| `adr-0075-env-backend-local-docker.md` | EnvBackend 多环境执行 (D1 IEnvBackend 接口 + D2 LocalBackend + D3 DockerBackend + D4 backend: 字段 + D5 EnvValidationHook) | ✅ Approved (2026-08-18 — Wave 3-A `from-roadmap-phase-6c-execution-envbackend` ship: D1+D2+D3+D5 全 ship) |
| `adr-0076-dsl-engine-mcp-server.md` | DSL Engine as MCP Server 控制面 (D1 stdio+HTTP+SSE + D2 静态 token + D3-D7 capability 暴露 + D8 Stateless) | 🔍 Proposed (2026-08-03, Wave 3 末, **gated by active-status.md §四**, INTEGRATES WITH Phase 6 Candidate B) |
| `adr-0077-grpc-data-plane.md` | gRPC Data Plane 高吞吐通道 (D1 4 service + D2 64KB 路由 + D3 mTLS + D4 proto + D5 grpc.* 事件 + D6 GRPCBackend + D7 路由决策 + D8 启动条件) | 🔍 Proposed (2026-08-03, Wave 4 descoped docs-only, 派生自 ADR-0071 §D8) |
| `adr-0078-finetune-base-model.md` | Fine-tune 基模与训练管线 (D1 4 维度评分 + D2 触发条件 + D3 数据 + D4 LoRA + D5 评估 + D6 AgenticMind 回流 + D7 serving) | 🔍 Proposed (2026-08-03, Wave 5+ descoped docs-only, 派生自 ADR-0071 §D9) |
| `adr-0079-unified-session-4scope.md` | 统一会话模型与 4-Scope 存储 (Conversation/Attempt/Step/Execution + ConvergenceEntry) | ✅ Approved (v1.1 amendment 2026-08-12; 原始 v1 文本 2026-01-19, 本会话讨论产出) |
| `adr-0080-append-only-event-log.md` | AppendOnlyEventLog 作为核心审计日志 (Step 0: BusEvent 信封扩展 + GenerationRequest.purpose; v1.1 D10 Distillation Capture + D2 causal_time + D6 opt-in) | ✅ Approved (v1.1 amendment 2026-08-12; 原始 v1 文本 2026-01-19, 本会话讨论产出) |
| `adr-0082-agent-first-class-registry.md` | Agent as First-Class Registry (AgentWorker + YAML + spawn_agent, IAgentRegistry + IAgent 骨架) | ✅ Approved (评审通过 2026-08-21, Sprint 22 — adr-0082-promote-to-approved, V1 骨架 ship; 完整 AgentWorker 推迟 Sprint 24+) |
| `adr-0081-pre-step-hook-contract.md` | Pre-Step Hook Contract (Agent 级拦截点, IAgentHookRegistry Agent-scoped) | ✅ Approved (评审通过 2026-08-21, Sprint 22 — adr-0081-promote-to-approved, V1 骨架 ship; Agent loop 集成推迟 Sprint 24+) |
| `adr-0080-v1-2-amendment-d10-decouple.md` | D10 Capture 与 Scrub Hook 解耦 (CaptureMode 三态 + Training fail-open 三重保护) | ✅ Approved (评审通过 2026-08-25, Oracle G12 解锁 ADR-0081/0082 死锁) |
| `adr-0083-evaluator-reward-contract.md` | IEvaluator/RewardSignal 评估契约 (双层契约 + 3 内置评估器 + V1 简化避免 ADR-0057 零实施) | ✅ Approved (V1 ship 2026-08-26 — IEvaluator + TaskSuccessEvaluator, 12 cases / 31 assertions PASS, G10 Closed; V2 ship 2026-08-27 — BehavioralEquivalence + Composite, 8 cases / 18 assertions PASS, `evaluator-v2-composite` archived) |
| `adr-0084-mutation-governance-contract.md` | Mutation Governance 契约 (6 维度: 变异对象 L1-L4 分级 / 授权绑定复用 ADR-0004 + ADR-0031 / 治理流程 propose→evaluator→回归门→commit / 审计复用 ADR-0080 + 4 个 mutation.* 主题 / 失败回滚 / 攻击面 fail-closed) | ✅ Approved (2026-08-26 — V1 gate-and-audit 代码 ship, G11 Closed, 13 cases / 139 assertions PASS, OpenSpec change `2026-08-26-adr-0084-mutation-governance-contract`) |
| `adr-0085-cross-cutting-pattern-pdk.md` | Cross-Cutting Pattern PDK (4 范式 PDK Pattern + `CrossCuttingOrchestrator` 无状态 dispatcher + `ICrossCuttingPattern` 统一抽象 + 横切功能 DSL `*.cc.md`；类比 PDK Loop Agent 模式；V1 不实施 Meta-Agent 自管理) | ✅ Approved (2026-08-28 — Oracle 3 轮复审全部通过, issue #15 Self-Review 决议; 实施载体 OpenSpec change `pdk-cross-cutting-patterns` 后续创建, ~2.2 sprint) |
| `adr-0086-credit-assignment-contract.md` | 信用分配契约 (评估层 IEvaluator vs 归因层 AttributionRecord 划界; VersionPairDiff V1 归因方法 + ConfounderRecord 混杂分层记录 + 4 态判定 Attributed/Confounded/Insufficient/NotAttempted; 默认 fail-closed; S4 协同进化前置) | 🔍 Proposed (2026-08-31 — self-evolution §七 #6 立项, 取代过期 `adr-0085-` 文件名; spike + ADR 形式, V1 不强制实施; Axis6 Phase 1 blocker) |

### adr/plugin/ - Plugin 化候选清单

> 自 2026-06-16 起，本目录存放**计划通过 Plugin 实现**的活跃 ADR（与根目录共用同一编号空间与工具链扫描范围）。详见 [adr/plugin/README.md](adr/plugin/README.md)。

| 文件 | 议题 | 状态 |
|------|------|------|
| `adr/plugin/adr-0034-model-router.md` | IModelRouter 模型路由接口（plugin-candidate, C7 完整 ship） | ✅ Approved (2026-07-02) |

### adr/skill/ — ADR-0061 技能演化子项

> 12 个子 ADR 记录 Agent 进化管线（阶段 1→4）的详细设计决策。全部以头部行格式书写（`**状态**: ...`），与 `adr/skill/` 目录约定一致。

| 文件 | 议题 | 优先级 | 状态 |
|------|------|:---:|:----:|
| `adr-0061-01-skill-std.md` | SKILL.md 标准对齐（Anthropic / Cline Skills） | P0 | ✅ Approved |
| `adr-0061-02-behavioral-regression.md` | AgentAssay-style 行为回归套件 | P0 | ✅ Approved (✅ T14 ship 2026-08-25, 6 cases / 13 assertions PASS) |
| `adr-0061-03-skill-compiler.md` | SkillCompiler 实施 | P0 | ✅ Approved (✅ V1 ship 2026-08-27, T17, 15 cases / 61 assertions PASS) |
| `adr-0061-04-slm-routing.md` | SLM 路由优先（NVIDIA 2025） | P1 | ✅ Approved |
| `adr-0061-05-cpp-wasm-toolchain.md` | wasi-sdk 集成 + C++→Wasm CI | P1 | ✅ Approved (⚠ 无代码) |
| `adr-0061-06-trajectory-ir.md` | AgentIR-style Trajectory IR 升级 ParsedGraph | P1 | ⛔ Superseded (v1.1 amendment 2026-08-25, 见下行; v1.1 ✅ Shipped 2026-08-27) |
| `adr-0061-07-paste-speculation.md` | PASTE-style 推测执行 | P2 | 🔍 Proposed |
| `adr-0061-08-aflow-search.md` | AFlow-style MCTS 工作流搜索 | P2 | ✅ Approved + V1 Shipped 2026-08-28 (T20 MCTSWorkflowSearch ship) |
| `adr-0061-09-gepa-loop.md` | GEPA-style 反思循环 | P2 | 🔍 Proposed |
| `adr-0061-10-formal-lint.md` | λ_A-style Config 结构完整性检查 | P2 | 🔍 Proposed |
| `adr-0061-11-dsl-wasm.md` | DSL→Wasm Bytecode 编译器 | P2 | 🔍 Proposed |
| `adr-0061-12-webllm.md` | 浏览器端 WebLLM 集成 | P2 | 🔍 Proposed |
| `adr-0061-13-distillation-output-format.md` | 蒸馏输出格式 (IDistillationWriter + DistillationRecord + trajectory/policy/meta 三文件分离) | P0 | ✅ Approved (评审通过 2026-08-25, Oracle G15, 7 环闭环最后 1 环) ⏳ 代码 ship: Pending |
| `adr-0061-06-v1-1-amendment-trajectory-ir-decouple.md` | Trajectory IR 独立序列化视图 (不改 ParsedGraph, v1 标题耦合修正) | P1 | ✅ Approved + Shipped (T15 ship 2026-08-27, 9 cases / 55 assertions, ParsedGraph 零修改) |
| `adr-0061-08-v1-1-amendment-axis6.md` | MCTS Axis6 cognitive_domain composition chain (第 6 轴 + CognitiveDomainChainConfig + 单主体 commit 路径 + ADR-0068 v1.8 归口 + W4 双发射语义分离) | P2 | ✅ Approved 2026-08-31 (B1-B3/W4 修复 commits bc157fb + 283591f + 06ddd13; 实施载体 `openspec/changes/2026-08-31-mcts-axis6-cognitive-domain/` v2.1) |

### adr-management/ - ADR 元数据

> 自 2026-06-16 起，状态词汇表与关联性分析从 `adr/` 移至本目录。

| 文件 | 议题 | 说明 |
|------|------|------|
| `STATUS-GLOSSARY.md` | ADR 状态词汇表 | 6 个标准标签定义 + 维护规则 |
| `relationships.md` | ADR 关联性分析 | 由 `tools/adr_relationships.py` 自动生成 |

> ADR 编号 0024-0028 为未来 Phase-4 / Phase-6 规划保留；占位文件 0029/0035 已删除（2026-06-12）；13 个已废弃 ADR 已归档到 [docs/archive/adr/](archive/adr/README.md)（ADR-0034 已迁出归档至 plugin 候选区）。

---

## architecture/ - 架构工作组文档

> 架构分析、分层模型与缺失能力评估。定位：**ADR 的证据输入**（工作文档，非决策本身；数据须可用 `tools/doc_metrics.py` 等命令复现）。

| 文件 | 议题 | 状态 |
|------|------|------|
| ~~`agent-as-plugin-architecture-v1.2.md`~~ → [`specs/architecture.md`](specs/architecture.md) | Agent-as-Plugin 五层架构规范 (L0~L4 + R1~R5) | ✅ Approved — 已晋升 specs 契约层 (2026-07-31 D1) |
| `agent-evolution-pipeline.md` | Agent 四阶段进化管线 (SKILL→DSL→C++→Wasm) | 🔍 Proposed (管线已由 ADR-0061 承接) |
| `application-layer-sota-positioning-v2.md` | 应用层 SOTA 定位分析 v2 | 🟡 Proposed (v1 已归档至 `archive/architecture/`) |
| `adr-implementation-status-gap-analysis.md` | ADR 实施状态基线 (**ADR 状态唯一事实源**) | 🔄 滚动更新 (2026-07-30) |
| `layer-based-missing-capabilities-analysis.md` | 五层缺失能力分析 + Wave 1-4 执行计划 | ✅ v1.2.1 (2026-07-31 数据修正版) |

> 归档版本（v1.0/v1.1 架构、v1 SOTA 定位）见 [archive/architecture/](archive/architecture/)。

---

## specs/ - 规范文档 (当前有效版本)

> ⚠️ **DEPRECATED** ⚠️ [`../SPECS-ALIGNMENT.md`](SPECS-ALIGNMENT.md) (2026-07-06) — 该规范对齐计划文件自标半准确,不再维护。当前维护规范以下方表格为准,新增 spec 请直接添加到此目录并在 ADR 中交叉引用。

核心规范文档，定义系统行为。

| 文件 | 议题 | 说明 |
|------|------|------|
| `architecture.md` | AgenticOS 架构规范 | **五层架构定义 (L0~L4 + R1~R5)** — 原 `agent-as-plugin-architecture-v1.2` 晋升 (2026-07-31 D1)；前代八层规范 v2.2 已归档至 `archive/specs/architecture-v2.2.md` |
| `layer0.md` | L0 运行时规范 | DSL 引擎核心行为 |
| `layer0-refactor.md` | L0 重构计划 | Layer0 重构计划 |
| `dsl.md` | DSL 规范 v3.10 | 最新 DSL 语言规范 |
| `stdlib-v3.10.md` | DSL 标准库 v3.10 | 合并自 dsl-lib + stdlib (Stage 2 / Task 8) |
| `memory-v3.10.md` | DSL 内存记忆规范 v3.10 | 合并自 memory.md (MEP-001 v3.2 Draft) + dsl.md §10.3 (Stage 2 / Task 9) |

---

## guides/ - 用户和开发者指南

面向用户的指南和参考文档。

| 文件 | 议题 | 说明 |
|------|------|------|
| `app-dev-guide.md` | 应用开发指南 | 使用 AgenticDSL 开发应用 |
| `app-dev-guide-cpp.md` | C++ 开发指南 | C++ API 使用 |
| `developer-guide.md` | 开发者指南 | 开发规范和最佳实践 |
| `rt-guide.md` | 运行时指南 | 运行时配置和部署 |
| `reference.md` | DSL 参考 | DSL 语法快速参考 |
| `contract-template.md` | 契约模板 | Agent 间交互模板 |
| `example.md` | 示例 | DSL 示例说明 |
| `training-guide.md` | 培训指南 | 新手入门 |

---

## design/ - 设计文档

设计文档和提案。

| 文件 | 议题 | 说明 |
|------|------|------|
| `design-v0.md` | 设计 v0 | 初始设计 |
| `design-v1.md` | 设计 v1 | 设计迭代 |
| `design-v3.1.md` | 设计 v3.1 | v3.1 版本设计 |

---

## 其他目录速览

| 目录/文件 | 内容 |
|------|------|
| `GOVERNANCE.md` | 文档治理方案：分层权限 + 任务驱动流水线（每份文档要么驱动任务、要么被任务更新、要么归档） |
| `research/` | 调研报告：`pi-agent-vs-pdk-chat-demo-analyze.md`（pi-agent 借鉴路径）、SOTA 调研、Agent 架构综合等 |
| `audits/` | 审计报告：drift gate、sanitizer 复验、ship gate 验证、LSP false positive 等 |
| `adversarial-reviews/` | 对抗性评审决议记录（decisions-*.md） |
| `service-composition/` | Phase 6 服务组合 spike 产出：`spike-onboarding.md`（C20 团队入口）、`observations/` Layer 3 dual memos |
| `skills/` | 文档化技能说明 |

---

## archive/ - 归档 (过期版本)

过期的文档，不再维护，仅供历史参考。

| 目录 | 内容 |
|------|------|
| `v3.8/` | DSL v3.8 规范 (过期) |
| `v3.7/` | DSL v3.7 规范 (过期) |
| `v3.6/` | DSL v3.6 规范 (过期) |
| `v3.5/` | DSL v3.5 规范 (过期) |
| `v3.4/` | DSL v3.4 规范 (过期) |
| `v3.3/` | DSL v3.3 规范 (过期) |
| `v3.2/` | DSL v3.2 规范 (过期) |
| `v3.1/` | DSL v3.1 规范 (过期) |
| `v3.0/` | DSL v3.0 规范 (过期) |
| `v2.3/` | DSL v2.3 规范 (过期) |
| `adr/` | 归档 ADR (13 个, 2026-06-12) — 见 [archive/adr/README.md](archive/adr/README.md) |
| `specs/` | 归档 Spec (Phase 2 标准库 v1.0, 2026-06-12; **AgenticOS 八层架构规范 v2.2, 2026-07-31 D1 归档**) — 见 [archive/specs/](archive/specs/) |
| `compiler/` | SKILL Compiler 预研设计 (2026-05-24,设计已决但未实施;2026-07-06 归档) — 见 [archive/compiler/README.md](archive/compiler/README.md) |

**看板归档 (2026-07-07)**：
- `archive/roadmap-status.md` — Phase 0-4 Sprint 日志看板 (已过期，被 `active-status.md` 替代)
- `archive/implementation-roadmap.md` — 2026-06-03 旧实施路线图 (已过期，被 master plan + active-status.md 替代)

**其他归档文件**：
- `AgenticDSL_LibSpec_v1.1.md` - 旧库规范 (过期)
- `AgenticDSL_SystemPrompt_v3.6.md` - 旧 System Prompt (过期)
- `Roadmap.md` - 旧路线图 (过期)
- `brain-thinking-spec.md` - 旧思考规范 (过期)
- ~~`AgenticDSL whitepaper.md`~~ (2026-07-06 删除,内容近似空)
- ~~`Application_guide.md`~~ (2026-07-06 删除,内容近似空)

---

## proposals/ - AgenticDSL 语言演进提案

> **与 docs/adr/ 和 docs/specs/ 的关系**：`docs/adr/` 记录引擎实现决策，`docs/specs/` 描定当前引擎行为（v3.10）；
> `docs/proposals/` 记录**语言演进提案**，讨论 AgenticDSL 应该往哪个方向演化及其实现路径。
>
> 注:此目录原位于 `docs/adr/agenticdsl/`(2026-06-12 升级为顶层目录,语义边界更清晰)。

文档组织按**话题领域**（而非文档类型），共 14 个子目录：

| 目录 | 话题 | 关联现有文档 |
|------|------|------------|
| `vision/` | 自举愿景与演进路线图 | [specs/dsl.md](specs/dsl.md), [specs/architecture.md](specs/architecture.md) |
| `skill-system/` | 技能分类体系、invoke/compose 语法、当前 6 技能映射（规划 39） | [examples/skill_porting/skills/](../../examples/skill_porting/skills/), [adr/adr-0009](../adr/adr-0009-dsl-standard-library.md) |
| `session-state/` | 四层隔离模型、ModuleState/Yield/Fork 语义、Oracle 问答 | [adr/adr-0014](../adr/adr-0014-conversation-context.md), [adr/adr-0008](../adr/adr-0008-structured-context.md) |
| `inference-stdlib/` | 推理标准库接口设计与子图规格 | [adr/adr-0001](../adr/adr-0001-illm-provider-streaming-interface.md), [specs/stdlib-v3.10.md](specs/stdlib-v3.10.md) |
| `language-extensions/` | 类型系统、模块命名空间、标准库扩展 | [specs/dsl.md](specs/dsl.md), [specs/stdlib-v3.10.md](specs/stdlib-v3.10.md) |
| `implementation-roadmap/` | 6 步实施计划与代码映射 | [src/](../../src/) |
| `research/` | 推理引擎调研报告（vLLM/SGLang/llama.cpp） | [adr/adr-0001](../adr/adr-0001-illm-provider-streaming-interface.md) |
| `architecture/` | 推理架构、路由器、质量评估器设计 | [adr/adr-0001](../adr/adr-0001-illm-provider-streaming-interface.md), [adr/adr-0008](../adr/adr-0008-structured-context.md) |
| `optimization/` | 推理优化方向方案（6 维度） | — |
| `implementation/` | 自举实施路径、阶段 0 实施方案 | [adr/adr-0001](../adr/adr-0001-illm-provider-streaming-interface.md), [adr/adr-0005](../adr/adr-0005-llm-backend-config-factory.md) |
| `testing/` | 测试策略（金字塔、Mock 策略、CI） | — |
| `api/` | CloudLLMAdapter API 设计 | [adr/adr-0001](../adr/adr-0001-illm-provider-streaming-interface.md), [adr/adr-0005](../adr/adr-0005-llm-backend-config-factory.md) |
| `operations/` | 安全规范、性能基准 | [adr/adr-0004](../adr/adr-0004-toolregistry-security.md) |

详细索引见 [proposals/README.md](proposals/README.md)。

---

## superpowers/ - superpowers plans 目录

> **2026-06-24 更新**：`docs/superpowers/plans/2026-06-22-sprint7-scheduler-pipeline-tightened.md`
> (668 行) 已通过 `git mv` 移至 `docs/archive/superpowers/plans/`。该 plan 是 Sprint 7 启动
> 时的执行计划,已 ship + 延展至 Sprint 8 + Sprint 9 step 1,本计划已不再 active。
>
> **2026-06-25 更新**: 目录中保留 2 个 active 跟踪 plan (均 ship + 归档闭环相关):
> - `2026-06-24-tech-debt-full-closure.md` (1769 行) — 13 步全路径 plan (阶段 A+B 100% + 阶段 C handoff 至 `2026-06-24-engine-include-final-decoupling`),已 ship
> - `2026-06-24-engine-include-final-decoupling.md` (895 行) — 6.3.x 收官 plan (engine.cpp includes 10→3 + 3 engine_factory tests),已 ship + change 已 archive
>
> **2026-07-06 更新** (OpenSpec change `docs-cleanup-phase-2`):5 个已 ship plan 同步归档至 `archive/superpowers/plans/`:
> - `2026-06-24-engine-include-final-decoupling.md` (Sprint 6 已 ship + change 已 archive)
> - `2026-06-24-tech-debt-full-closure.md` (Sprint 6 已 ship)
> - `2026-06-25-sprint-10-pre-existing-sanitizer-cleanup.md` (Sprint 10 已 ship + change 已 archive)
> - `2026-07-02-c7-model-router-mvp.md` (Sprint 17 Phase 1 MVP 已 ship,被 Phase 2 超集覆盖)
> - `2026-07-02-c7-phase2-model-router-plugin.md` (Sprint 17 Phase 2 已完整 ship,ADR-0034 ✅ Approved + 61/61 ctest)
>
> 后续 superpowers plans 由各 Sprint 启动时按需创建。
>
> **历史归档(2026-06-03)**:`docs/superpowers/` 原 3 个文件已归档至 `docs/archive/superpowers/`:
> - `specs/2026-05-12-dsl-standard-library-design.md` → 已被 `docs/adr/adr-0009-dsl-standard-library.md` 取代
> - `specs/2026-05-13-memory-state-interface-design.md` → 已被 `docs/adr/adr-0010-memory-system.md` 取代
> - `plans/2026-06-02-test-fixes-for-prephase.md` → 7 任务已全部执行,12 个测试 100% 通过

---

## 文档更新记录

| 日期 | 更新内容 |
|------|---------|
| 2026-05-20 | 新增 agenticdsl/ 目录（16 篇语言演进文档），按话题组织 |
| 2026-06-12 | 提升 agenticdsl/ 为 docs/proposals/，明确与 docs/adr/ 语义边界 |
| 2026-05-23 | 扩展至 30+ 篇文档，新增 research/architecture/optimization/implementation/testing/api/operations 7 个目录 |
| 2026-06-08 | C1 迁移：ADR-0019/0020/0023 状态更新；ADR-0030~0036 补录；`dsl.md`/`dsl-lib.md` 版本升至 v3.10 |
| 2026-06-12 | Stage 2 / Task 7：归档 13 个已废弃 ADR 到 `docs/archive/adr/`；移除 `phase-2-memory/`, `phase-3-reasoning/`, `phase-5-async/`, `phase-5-policy/`, `phase-7-router/`, `phase-8-kernel/` 6 个空目录 |
| 2026-06-12 | Stage 2 / Task 8：合并 `dsl-lib.md` + `stdlib.md` 为 `stdlib-v3.10.md`；归档 `phase2-standard-library.md` 到 `docs/archive/specs/` |
| 2026-06-12 | Stage 2 / Task 9：合并 `memory.md` (MEP-001 v3.2 Draft) + `dsl.md` §10.3 为 `memory-v3.10.md`；`memory.md` 已删除 |
| 2026-07-31 | 文档治理修正：新增 architecture/ 章节 + 其他目录速览；ADR 表补齐 0030/0037-0039/0042/0045/0046/0050/0051/0052-0066 行；目录结构块补全 8 个缺失目录；`application-layer-sota-positioning.md` v1 归档至 `archive/architecture/` |