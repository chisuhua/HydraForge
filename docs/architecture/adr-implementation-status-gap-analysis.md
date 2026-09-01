# ADR 实施状态差距分析

> ⚠️ **本文档为滚动快照**。ADR 状态以 `docs/adr/*.md` 各自状态行为准；本表滞后风险自负。
> 数据修订记录见 `git log docs/architecture/adr-implementation-status-gap-analysis.md`。本表文档定位降级为"**ADR 状态权威参照**"（最终以 `docs/adr/*.md` 状态字段为准）。

**生成日期**: 2026-08-03
**最后更新**: 2026-09-01 — 与 OpenSpec change `analysis-status-snapshot-sync` 同步：状态清扫（ADR-0068 ✅/0069 🟡/0070 🟡/0074 ✅/0075 ✅）+ Header 快照横幅 + 措辞降级（详见 §一 总览 + §2.3）
**分析范围**: 102 个 ADR（67 主 + 1 plugin + 15 skill 子项 + 19 archive）vs 代码库实施状态；可复现命令 `python3 tools/adr_lint.py`
**数据源**: `docs/adr/` 目录、`tools/adr_lint.py` 输出、AGENTS.md Recent Changes、Oracle session `ses_037e12115ffeLkeR1QTIko0BHb`、代码库架构扫描、`docs/active-status.md §四`、code-review-graph

---

## 一、总体状态概览

| 状态 | 计数 | 占比 | 备注 |
|------|:---:|:----:|------|
| ✅ Approved — 已批准且实施完成 | 52 | 51.0% | (Phase 6 架构评审 0052-0065 14 个 + ADR-0084 2026-08-26 + ADR-0061-06 v1.1 T15 ship + ADR-0061-08 T20 V1 ship + ADR-0068/0071/0074/0075/0085 评审通过 + ADR-0080 v1.1/v1.2 + 0061-13 等 — 详见 ADR 文件 `## 状态` 字段) |
| 🟡 Partial — 已批准但实施不完整 | 10 | 9.8% | 含 **ADR-0068 v1.1-v2.0 持续修订**、ADR-0069/0070/0073 翻牌、ADR-0066 SkillInterpreter V1、ADR-0067/0068/0069/0070 等 |
| 🔍 Proposed — 提议阶段，未批准 | 12 | 11.8% | 含 **4 个新起草 LLM-native** (0072/0076/0077/0078) + ADR-0042/0045/0046/0038/0039/0085(已 Approved, 见备注) — 最终权威见 ADR 文件 |
| ❌ Not Implemented — 未实施（含已归档） | 18 | 17.6% | 12 归档 (0010-0018 + 0030V1 + 0036×2) + 6 永久 (0002/0030V1/0036×2/0073V1待翻) |
| ⛔ Superseded — 被替代 | 2 | 2.0% | ADR-0006 → ADR-0020, ADR-0036 → ADR-0045 |
| 📦 Archived (已实施后归档) | 1 | 1.0% | ADR-0032 (CostCollector) |
| 📋 Reserved — 编号预留 | 2 | 2.0% | (占位文件 0024/0028; 编号 0024-0028 预留) |
| **总计** | **102** | **100%** | （含 ADR-0084 ✅ Approved + ADR-0085 ✅ Approved 已计入 ✅ Approved 段；可复现命令 `python3 tools/adr_lint.py`）|

> ① `docs/archive/adr/` 19 个归档 ADR（0010-0018 + 0030 V1 + 0036×2 + 其他）计入 `❌ Not Implemented`。② ADR-0032 已实施后归档, 单列 `📦 Archived`。③ ADR-0002 未实施但文件仍在主目录, 计入 `❌`。④ ADR-0037 于 2026-07-27 从 🔍 提升为 🟡 Partial (CausalClock ship)。⑤ **LLM-native 8 ADR (0071-0078) 当前状态**：0071 ✅ Approved (2026-08-25), 0072 🔍 Proposed, 0073 🟡 Partial (翻牌), 0074 ✅ Approved (2026-08-25), 0075 ✅ Approved (2026-08-18), 0076-0078 🔍 Proposed — 最终权威见 `docs/adr/*.md` 各自状态行。⑥ **ADR-0073 三重状态已统一**：内部 🔍 → 翻牌 🟡 Partial（实施率约 30%，见 §2.1）。

**📌 ADR 状态权威源声明**（重要）：本文档为滚动快照，**不**再以"唯一事实源"自我定位。ADR 状态唯一事实源 = 各 `docs/adr/*.md` 文件 `## 状态` 字段；本表为视图层（按维度组织的状态汇总），与 ADR 文件冲突时**以 ADR 文件为准**。`docs/README.md` ADR 表、`tools/adr_lint.py` 输出、`tools/adr_relationships.py` 生成的 `docs/adr-management/relationships.md` 同样为视图层。

---

## 二、实施差距详细分类分析

### 2.1 🟡 Partial — 契约达成但实施不完整（8 个 ADR）

#### ADR-0007: 上下文压缩机制
- **现状**: 快照机制已实现，但无 LLM 驱动的内容压缩
- **缺失**: LLM 摘要/压缩管道
- **影响**: 长对话场景下上下文窗口可能溢出
- **建议**: Phase 6 规划 LLM 压缩器，优先级 P2

#### ADR-0019: IInteractionBus 接口与 MVP
- **已 ship**: IInteractionBus + InMemoryBus MPMC (2026-06-24) + BusEvent 公开契约 + subscribe_glob 通配符订阅 (2026-07-26~27) + CausalClock 集成
- **缺失**: 正式 `subscribe_topic(topic_pattern, callback)` 未实施 — 但 subscribe_glob 已部分覆盖
- **建议**: 关闭 subscribe_topic gap, subscribe_glob 视为已实施版本

#### ADR-0030 V2: Phase 2 异步运行时
- **已 ship**: DomainWorkerPool ✅ / InMemoryBus MPMC ✅ / stream_to_bus bridge ✅ / Context fork/merge ✅ / Taskflow 集成 ✅
- **缺失**: FleetOrchestrator（16 路并行 LLM 调度）— Oracle 决议 DEFER（0 examples 使用, ~1 周节省）
- **建议**: 维持 DEFER, 真实需求出现时重新评估

#### ADR-0031: IExecutionPolicy 执行策略与审批
- **已 ship**: C3 P1-P2 + C4 P3-P4 全部 ship (5-method / 3 策略 / ToolCoordinator / LayerProfile / Audit Log)
- **缺失**: 4 项 defer 至 C6 — 超时机制、std::async 异步、成本预算集成、审批历史持久化
- **影响**: 中等 — 工具调用无超时保护
- **建议**: Phase 6 重新评估 C6 优先级

#### ADR-0037: 跨 Worker 事件因果序
- **已 ship**: CausalClock 类 + emit auto-tick + BusEvent.causal_time attach (2026-07-27)
- **缺失**: 完整因果序 — 向量时钟、跨 worker 版本向量合并、分布式因果关系检测
- **建议**: 维持单进程 CausalClock, 分布式留待跨进程需求出现

#### ADR-0066: SkillInterpreter 模块架构
- **已 ship**: V1 PIMPL (posix_spawn + seccomp + pipe IPC, 2026-07-22) + 4 host function
- **缺失**: V2 (多 skill 协同 / hot-reload / 性能基准)
- **建议**: Phase 6 规划 V2 增强

#### ADR-0073: Tool JSON Schema 契约 ⚠️ **状态待对齐**
- **ADR 状态**: 🔍 Proposed (ADR 内部) vs ✅ Approved (docs/README.md) — **不一致**
- **实际实施**: Phase 5 Sprint 21 已部分 ship (ToolMetadata V3 字段 per AGENTS.md)
- **建议**: **翻牌 🟡 Partial** (Sprint 21 已 ship 部分决策 D2/D5, 缺 D1/D3/D4/D6 实施)
- **关联 OpenSpec change**: ADR-0073 Wave 2 Phase 2.1 (估时 1-2 周)

#### ADR-0008: 结构化 Context ⚠️ **本会话参考**
- **已 ship**: LayeredContext (L1-L5) 实现完成 (2026-06-12) ✅
- **关联**: ADR-0074 D5 Prompt 注入 working / episodic / semantic 三层依赖此 ADR

---

### 2.2 🔍 Proposed — 未批准 + LLM-native 8 ADR 详细（13 个）

#### LLM-native 架构蓝图 (本会话起草, 6 个新 ADR)

##### ADR-0071: LLM-native AgenticDSL 架构 (顶层, Wave 2 锚定)
- **状态**: 🔍 Proposed (2026-08-02, 顶层方向 ADR, 锚定 Phase 6+ 演化)
- **9 项决策 D1-D9** 全部有派生 ADR:
  - D1 顶层架构 (自身) / D2 规范升级 (D3+D5) / D3 → ADR-0072 / D4 → ADR-0073 / D5 → ADR-0074 / D6 → ADR-0075 / D7 → ADR-0076 / D8 → ADR-0077 / D9 → ADR-0078
- **D7 战略协调**: 已 Oracle 修复 — "⚠️ INTEGRATES WITH Phase 6 Candidate B (gated by active-status.md §四)" — 不再隐含假设服务化路径默认开启
- **代码对齐**: 0/9 实施

##### ADR-0072: DSL 节点扩展 (Wave 2.4, **GATED**)
- **状态**: 🔍 Proposed (2026-08-03)
- **6 项决策**: D1 stream:true 强制 + D2 $var 条件 + D3 declarative style 条件 + D4 backend: 强制 + D5 双语法共存期 + D6 try/catch OFF
- **Oracle 修复 #3 已应用**: D3 触发条件 `parse-valid < 90%` → `85% ≤ parse-valid < 90%` 临界带
- **代码对齐**: 0/6 实施, Wave 2.4 GATED 等 Evidence Gate

##### ADR-0073: Tool JSON Schema 契约 (Wave 2.1, **status 翻转待 apply**)
- **状态**: 🔍 Proposed → 待翻牌 🟡 Partial (per §2.1)
- **6 项决策**: JSON Schema 2020-12 + V3 字段 + 运行时校验 + DECLARE_TOOL 自动生成 + 向后兼容 + Schema 版本
- **代码对齐**: 部分 ship (Phase 5 Sprint 21, ToolMetadata V3 字段)

##### ADR-0074: Prompt Engineering + Evidence Gate (Wave 2.2)
- **状态**: 🔍 Proposed (2026-08-03)
- **7 项决策**: D1 few-shot 30+ + D2 golden 50+ + D3 baseline 3 模型 × 2 指标 + D4 Evidence Gate 阈值 + D5 两阶段注入 ≤8k + D6 JSONL + D7 失败事件
- **Oracle 修复 #2 已应用**: D7 中 2 个候选主题 `llm.dsl.{parse_failed,schema_validation_failed}` 标注 "⚠️ pending + ADR-0068 §附录 A amendment PR"
- **代码对齐**: 0/7 实施, Wave 2.2 估时 2-3 周 (≈ Phase 6a 37h 容量, 接近上限)

##### ADR-0075: EnvBackend Local + Docker (Wave 3 Phase 1+2)
- **状态**: 🔍 Proposed (2026-08-03)
- **5 项决策**: D1 IEnvBackend 接口 + D2 LocalBackend + D3 DockerBackend + D4 backend: 字段 + D5 EnvValidationHook
- **K8s/SSH 推迟** (替代方案 #4 拒绝 "4 backend 一次性实施", 估时 4-6 周超容量)
- **Oracle 修复 #2 + #5 已应用**:
  - 2 个候选事件 `env.backend.exec.start/end` + `env.backend.unavailable` 标注 "⚠️ pending + ADR-0068 §附录 A amendment"
  - §不变量 3 "必填" → "推荐必填, 缺省 local (向后兼容 V3.10)"
- **代码对齐**: 0/5 实施, Wave 3 Phase 1+2 估时 2-3 周 (≈ Phase 6b 44h 容量)

##### ADR-0076: DSL Engine as MCP Server (Wave 3 末, **GATED**)
- **状态**: 🔍 Proposed (2026-08-03, gated by active-status.md §四)
- **7 项决策**: D1 双 transport + D2 静态 token + D3 tools/* + D4 prompts/* + D5 resources/* + D6 inputSchema 零转换 + D7 外部 MCP client
- **Oracle 修复 #1 + #2 已应用**:
  - 4 处 "IS Phase 6 Candidate B" → "**INTEGRATES WITH (gated by active-status.md §四)**" + 启动条件 (AgentForge ≥ Sprint 25 + Solo Dev ≥2 人)
  - 6 个候选主题 `mcp.server.{connected,disconnected,request,response}` + `mcp.client.{request,response}` 标注 "⚠️ pending + ADR-0068 §附录 A amendment"
- **代码对齐**: 0/7 实施, Wave 3 末估时 2-3 周 + **启动条件未满足结构性暂缓**

##### ADR-0077: gRPC Data Plane (Wave 4, **descoped docs-only**)
- **状态**: 🔍 Proposed (2026-08-03, docs-only)
- **7 项决策**: D1 4 service + D2 路由 64KB + D3 mTLS + D4 proto 集成 + D5 4 grpc.* 事件 + D6 GRPCBackend + D7 路由决策
- **Oracle 修复 #2 已应用**: D5 中 4 个候选事件 `grpc.stream.{start,chunk,end}` + `grpc.connection.{up,down}` 标注 "⚠️ pending + ADR-0068 §附录 A amendment"
- **重新激活条件** (4 项): 团队 ≥2 人 / AgenticMind ship / K8s 分布式 / MCP 阈值实测校准
- **代码对齐**: 0/7 实施, Phase 7+ 评估

##### ADR-0078: Fine-tune 基模 (Wave 5+, **descoped docs-only**)
- **状态**: 🔍 Proposed (2026-08-03, docs-only)
- **7 项决策**: D1 4 维度评分 + D2 触发条件 + D3 训练数据 + D4 LoRA + D5 评估 + D6 AgenticMind 回流 + D7 serving
- **重新激活条件** (4 项): AgenticMind ship / Evidence Gate FAIL / 用户 ≥10 / Fine-tune 价格 ≤$1
- **代码对齐**: 0/7 实施, Phase 5+ 评估

#### 其他 🔍 Proposed ADR (5 个 + 6 个 P2 子项)

##### ADR-0042: ILLMProvider 演进路径
- **状态**: 🔍 Proposed (主文档硬性 banner)
- **已实施**: C16 增量决议已记录 — Decorator 链 / Dual Consumer / available_models pure virtual / PluginLoader V2 / LlamaAdapter deprecated
- **建议**: 批准 ADR-0042 承认现有事实或将增量决议升级为正式 ADR

##### ADR-0045: 编排 PDK Plugin 规范
- **实施率**: ~20% — `provider_agent` (390 行, 4 工具) / `g1_coding_assistant` (242 行) / `g3_knowledge_base` (345 行) 有部分逻辑
- **缺失**: 编排 DSL schema、插件间协调协议、执行审计

##### ADR-0046: PDK 插件间通信协议
- **实施率**: ~35% — InMemoryBus + BusEvent + subscribe_glob 部分覆盖

##### ADR-0038 / 0039 / 0061-07~12 (P2 子项)
- **ADR-0038**: DELAY (被 ADR-0041 覆盖)
- **ADR-0039**: DELAY (JSON 工具未实现)
- **ADR-0061-07~12**: v2 candidate 实验性方向

### 2.3 ✅ Approved — 但代码对齐度需确认

以下 ADR 标记为 Approved, 但存在代码-契约差距。Phase 6 系列（0052-0064）的"Approved"为架构评审批准, 非实施完成批准, 属正常的前期定义阶段。

| ADR | 差距 |
|-----|------|
| **ADR-0052~0064** (Phase 6 系列) | 架构评审 Approved 但多数**无代码实现** — 属正常的前期定义 |
| **ADR-0056** (Wasm 运行时) | 零代码 — 待 wamr 集成 |
| **ADR-0057** (Agent 生命周期) | 设计已确认但生命周期管理器未实现 |
| **ADR-0058** (Schema 校验) | 零代码 — ToolMetadata V2 已有关联, schema 校验独立 (衔接 ADR-0073) |
| **ADR-0059** (跨进程协议) | 零代码 — 与 ADR-0060 6 种协作模式对齐但未实施 |
| **ADR-0062** (Marketplace) | 设计已确认但包格式/分发未实现 |
| **ADR-0063** (OpenTelemetry) | 零代码 — 分布式追踪未集成 |
| **ADR-0064** (Conformance Suite) | 零代码 — PDK 兼容性测试套件未建立 |
| **ADR-0065** (Python PDK) | 零代码 — Wasm 多语言支持未启动 |
| **ADR-0068** (Event Emission Contract) | ✅ Approved (2026-08-03, 经 v1.1-v2.0 持续修订); Wave 1 §1-§5 ship + 7 幻影主题已真实发射（OpenSpec change `2026-08-03-adr-0068-event-emission-contract` + `2026-08-03-promote-event-builder-fulltoolresult-support` 已 archived）。附录 A 当前版本 v2.0（2026-08-31）含 60+ 注册主题（详见 `docs/adr/adr-0068-event-emission-contract.md`）。剩余候选主题（0074/0075/0076/0077 14 个）已通过 ADR-0068 附录 A amendment 持续注册 |
| **ADR-0069** (ToolCoordinator Hooks) | 🟡 Partial (2026-08-04); middleware 改造 + budget_agent pre-hook + 5 类测试 ship，待 HookErrorPolicy amendment（衔接 ADR-0075 D5 EnvValidationHook） |
| **ADR-0070** (DECLARE_COMMAND) | 🟡 Partial (2026-08-04); D4 立项 + 实施排期 Wave 1，与 LLM-native 正交 |

### 2.4 已归档/未实施 ADR

#### ADR-0002: EventBus 有界队列
- **状态**: ❌ Not Implemented (文件在 `docs/adr/` 主目录, 未归档)
- **取代**: Phase 1 由 IInteractionBus + InMemoryBus (ADR-0019) 承担事件通信职责
- **影响**: 无 — 功能已被 ADR-0019 覆盖

#### 已归档 ADR (编号 0010-0018 + 0030 V1 + 0036×2)
12 个 Phase 0 认知/记忆 ADR 于 2026-06-09 整批归档, 全部未实施。ADR-0030 V1 被 V2 替代。ADR-0036 (三层服务协议 / 混合内核) 2 文件未实施。

---

## 三、LLM-native 架构深度分析 (8 ADR 一体化)

### 3.1 8 ADR 实施差距汇总

| ADR | Wave | 决策数 | 强制决策 | 条件触发 | 实施差距 |
|-----|------|:---:|:---:|------|---------|
| 0071 顶层 | 2 锚定 | 9 | 0 | 9 (派生) | 0/9 (蓝图) |
| 0072 DSL 节点 | 2.4 | 6 | D1+D4 | D2+D3+D5+D6 | 0/6 (Gated) |
| 0073 Schema | 2.1 | 6 | D1+D6 | D2-D5 | 🟡 2/6 (Sprint 21) |
| 0074 Prompt | 2.2 | 7 | D1-D4+D6+D7 | 0 | 0/7 (估 2-3 周) |
| 0075 EnvBackend | 3 P1+2 | 5 | D1-D5 | 0 | 0/5 (估 2-3 周) |
| 0076 MCP | 3 末 | 7 | D1+D2+D3+D5+D6+D7 | D4 (baseline) | 0/7 (Gated) |
| 0077 gRPC | 4 | 7 | 0 | 7 (全条件) | 0/7 (Wave 4 descoped) |
| 0078 Fine-tune | 5+ | 7 | 0 | 7 (全条件) | 0/7 (Wave 5+ descoped) |
| **合计** | | **54** | **16** | **38** | **🟡 2/54 (~3.7%)** |

### 3.2 跨 ADR 依赖图 + Wave 推进路径

```
                    ┌─────────────────────────────────┐
                    │  Wave 2 锚定 (Phase 6b 优先)    │
                    └─────────────────────────────────┘
                                       │
            ┌──────────────────────────┼──────────────────────────┐
            │                          │                          │
       ┌────▼─────┐               ┌─────▼─────┐              ┌─────▼─────┐
       │ ADR-0073 │               │ ADR-0074  │              │ ADR-0072  │
       │  Schema  │───────────────▶│  Prompt   │─────────────▶│  D1+D4    │
       │ (ship)   │  schema       │  Baseline │  Evidence    │  强制     │
       └──────────┘  snapshot     │  + Gate   │  Gate        └───────────┘
                                  └───────────┘
                                       │
                                       │ (Gate PASS)
                                       ▼
                              ┌─────────────┐
                              │ ADR-0072    │
                              │ D2/D3/D5/D6 │
                              │ 条件性触发   │
                              └─────────────┘

                    ┌─────────────────────────────────┐
                    │  Wave 3 (Phase 7+ 启动评估)     │
                    └─────────────────────────────────┘
                                       │
                       ┌───────────────┴───────────────┐
                       │                               │
                  ┌────▼─────┐                  ┌───────▼───────┐
                  │ ADR-0075 │─────────────────▶│ ADR-0076      │
                  │ EnvBackend│ stdio 模式      │ MCP Server    │
                  │ Local+Docker│ 复用          │ (gated by     │
                  │ Phase 1+2 │                 │  active-status│
                  └──────────┘                  │  Candidate B) │
                                                └───────────────┘

                    ┌─────────────────────────────────┐
                    │  Wave 4+ (Phase 7+ 评估)        │
                    └─────────────────────────────────┘
                                       │
                       ┌───────────────┴───────────────┐
                       │                               │
                  ┌────▼─────┐                  ┌───────▼───────┐
                  │ ADR-0077 │                  │ ADR-0078      │
                  │ gRPC     │                  │ Fine-tune     │
                  │ DataPlane│                  │ (AgenticMind  │
                  │ (docs-   │                  │  回流前置)    │
                  │  only)   │                  │ (docs-only)   │
                  └──────────┘                  └───────────────┘
```

### 3.3 Solo Dev 容量 vs 估时 (3.5-5x 超额分析 — Oracle 关键发现)

| 维度 | 估时 | 容量 | 超额倍数 | 状态 |
|------|------|------|:---:|------|
| **Wave 2 强制决策** (0073 翻牌 + 0074 + 0072 D1+D4) | 24-32h | Phase 6b 44h | ✅ 0.55-0.73x | 可行 |
| **Wave 3 全 scope** (0075 + 0076) | 160-240h | Phase 6b 44h | ❌ 3.6-5.5x | **不可行** |
| **总 LLM-native** (0071 + 7 派生) | 280-400h | Phase 6a+6b = 81h | ❌ 3.5-5x | **不可行** |

**Oracle 建议**: Solo Dev 无法承担 Wave 3 全 scope; 必须 descope 至 Wave 2 only (Schema + Prompt + 基础 Evidence Gate) 再审 Wave 3.

**实际推进策略** (建议):

```
Sprint 24 (Phase 6a, 2026-07-24 ~ 2026-08-05):
  ├─ pdk_chat_demo v1 (active, P0)
  ├─ pkm_temporal_demo PDK 骨架 (active, P0)
  └─ (Wave 2 决策不抢容量)

Sprint 25 (Phase 6b, 2026-08-05 ~ 2026-08-19):
  ├─ Wave 2 Phase 2.1-2.3 (ADR-0073 翻牌 + ADR-0074 baseline + ADR-0072 D1+D4 强制)
  │  估时: 24-32h / 容量 44h ✅ 可行
  └─ 跳 Wave 3 至 Sprint 26+ (待容量评估)

Sprint 26+:
  └─ Wave 3 评估 (0075 + 0076)
     ├─ 启动条件: AgentForge ≥ Sprint 25 + Solo Dev ≥2 人 (per active-status.md §四)
     └─ 当前: 结构性暂缓 (per Oracle 修复 #1)
```

---

## 四、ADR-0068 幻影主题注册缺口 (14 个待注册)

### 4.1 背景

ADR-0068 §决策 2 明确: **新增/修改主题必须 PR 修订 §附录 A**. 当前 §附录 A (v1.2.1) 含 31 个注册主题 (22 + 1 evaluation.result + 4 mutation.* + 4 既有扩展). 5 个 ADR (0074/0075/0076/0077) 设计 **14 个候选主题**, **未注册** (ADR-0084 4 个 mutation.* 主题已于 2026-08-26 随 V1 代码 ship 完成注册):

### 4.2 14 个待注册主题清单 (ADR-0084 mutation.* 4 主题已注册 ✅)

| ADR | 主题名 | 触发 | Payload schema |
|-----|--------|------|---------------|
| **0074** | `llm.dsl.parse_failed` ⚠️ pending | MarkdownParser 无法解析 LLM 输出 | `{task_id, raw_output, error_offset, hint}` |
| **0074** | `llm.dsl.schema_validation_failed` ⚠️ pending | ToolSchemaValidator 拒绝 | `{task_id, tool_name, errors[], hint}` |
| **0075** | `env.backend.exec.start` ⚠️ pending | EnvBackend exec 开始 | `{backend, cmd, args, request_id}` |
| **0075** | `env.backend.exec.end` ⚠️ pending | EnvBackend exec 结束 | `{backend, exit_code, duration_ms, error_code?}` |
| **0075** | `env.backend.unavailable` ⚠️ pending | Docker daemon 不可用 | `{backend_spec, reason}` |
| **0076** | `mcp.server.connected` ⚠️ pending | MCP server 连接建立 | `{peer_addr, tls?, auth_method}` |
| **0076** | `mcp.server.disconnected` ⚠️ pending | MCP server 连接断开 | `{peer_addr, duration_ms, reason}` |
| **0076** | `mcp.server.request` ⚠️ pending | MCP server 收到 request | `{method, request_id, payload_size}` |
| **0076** | `mcp.server.response` ⚠️ pending | MCP server 发出 response | `{method, request_id, status, duration_ms}` |
| **0076** | `mcp.client.request` ⚠️ pending | MCP client 发送 request (外部 server) | `{server_name, method, request_id}` |
| **0076** | `mcp.client.response` ⚠️ pending | MCP client 收到 response | `{server_name, method, request_id, status}` |
| **0077** | `grpc.stream.start` ⚠️ pending | gRPC stream 建立 | `{service, method, peer_addr, request_id}` |
| **0077** | `grpc.stream.chunk` ⚠️ pending | stream chunk 发送/接收 | `{service, method, chunk_index, bytes}` |
| **0077** | `grpc.stream.end` ⚠️ pending | stream 终止 | `{service, method, request_id, duration_ms, error_code?}` |
| **0077** | `grpc.connection.{up,down}` ⚠️ pending | TCP 连接建立/断开 | `{peer_addr, tls?, error?}` |
| **0084** | `mutation.proposed` ✅ registered (2026-08-26 V1 ship) | R 轨任务 propose 变异通过全部门禁链 | `{mutation_id, source_id, subject_ref, mutation_kind, proposed_change, parent_ref, evaluation_refs}` |
| **0084** | `mutation.committed` ✅ registered (2026-08-26 V1 ship) | commit (evaluation_refs 非空校验通过) | `{mutation_id, version_id, mutation_kind, evaluation_refs}` |
| **0084** | `mutation.reverted` ✅ registered (2026-08-26 V1 ship) | revert() 纯审计记录 (audit-only, 不恢复状态) | `{mutation_id, target_version, rollback_reason}` |
| **0084** | `mutation.denied` ✅ registered (2026-08-26 V1 ship) | 任一门禁步骤失败 (终态事件) | `{mutation_id, denial_reason, failed_step, subject_ref}` |

> **2026-08-26 ADR-0084 V1 ship 完成 4 个 mutation.* 主题注册**（G11 ADR-0084, issue #14 ✅ Approved + Oracle Self-Review session `ses_fc41537bbffeC35NKqgvzn4m1c`）:
> - ADR-0068 §附录 A v1.2.1 amendment: 4 主题 payload schema 对齐 design D4, `mutation.approved` 修正为 `mutation.reverted`
> - 剩余待注册: 14 个 LLM-native 主题 (0074/0075/0076/0077)
> - 实施代码: `src/common/governance/mutation_governor.cpp` + `tests/test_mutation_governance.cpp`

### 4.3 注册流程 (推荐)

1. **提交 ADR-0068 §附录 A amendment PR** (单一 PR 注册 14 个主题)
2. **PR 评审检查项**:
   - 命名规则: `<domain>.<entity>.<verb>` (per ADR-0068 §决策 2)
   - 命名一致性: `llm.*` / `env.backend.*` / `mcp.*` / `grpc.*` 各自 domain 唯一
   - Payload schema 标准化: 与 ADR-0068 EventBuilder L1 契约兼容
3. **PR ship 后**: 4 个 LLM-native ADR "⚠️ pending" 标记移除, 主题正式可发射
4. **建议 owner**: ADR-0068 author (Wave 1 在审 owner)

---

## 五、Phase 6 Candidate B 启动条件 vs 现状

### 5.1 ADR-0076 与 active-status.md §四 协调 (Oracle 修复 #1 已应用)

**active-status.md §四 状态** (per Oracle 审查):

> ⏸ Phase 6 服务化 (Candidate B) | **结构性暂缓** — ADR-0050 §启动条件 #4 Solo Dev 容量 + #5 AgentForge 非真正"外部" 双重不满足

**ADR-0076 (修复后) 状态**: ⚠️ INTEGRATES WITH Phase 6 Candidate B — ship **gated** by active-status.md §四 "Candidate B 结构性暂缓" 启动条件

### 5.2 启动条件清单

| 条件 | 现状 | 启动阈值 | 差距 |
|------|------|---------|------|
| Solo Dev 容量 | 1 人, 37h/44h | ≥2 人 OR ≥80h/双周 | ❌ 1 人 (当前) |
| AgentForge 里程碑 | 第 2 agent 未 ship | ≥ Sprint 25 milestone | ❌ 当前 Sprint 24 |
| AgentForge "外部" 性质 | 同人项目复用 pdk_chat_demo | 真正"外部"消费者 | ❌ 当前内部 |
| 服务化路径集成 | 结构性暂缓 | ADR-0076 ship 启动 | ❌ 当前 |

### 5.3 Wave 3 启动决策树

```
Wave 3 启动评估 (Sprint 26+):
  ├─ AgentForge ≥ Sprint 25? ──── ❌ 当前 Sprint 24 — 推迟
  ├─ Solo Dev ≥2 人? ──────────── ❌ 当前 1 人 — 推迟
  ├─ ADR-0068 amendment PR ship? ─ ❌ 当前 14 pending — 需先 ship
  ├─ ADR-0073 翻牌 🟡 Partial? ── ❌ 当前 🔍 Proposed — 需先翻牌
  └─ 全部满足? ──────────────────── → Wave 3 (0075 + 0076) 启动
                                      ├─ 0075 估时 2-3 周 (Phase 1+2)
                                      └─ 0076 估时 2-3 周 (Wave 3 末)
                                      合计 4-6 周 / 当前 44h 容量 ❌ 仍超额
                                       → 进一步 descope (仅 0075 LocalBackend)
```

---

## 六、Oracle MUST-FIX 5 项应用状态 (本会话 2026-08-03)

| # | 修复内容 | 涉及 ADR | 应用位置 | 验证 |
|---|---------|---------|---------|------|
| **#1a** | "IS Candidate B" → "INTEGRATES WITH (gated)" | 0076 §状态 L5 | ✅ 应用 | 文本替换确认 |
| **#1b** | 同上 + 交叉引用 active-status.md | 0076 §关联 L14 | ✅ 应用 | 文本替换确认 |
| **#1c** | 同上 | 0076 文档尾 L686 | ✅ 应用 | 文本替换确认 |
| **#1d** | 措辞修正 + 注明仍 gated | 0076 §替代方案 L563 | ✅ 应用 | 文本替换确认 |
| **#1e** | "✅ D7 IS" → "⚠️ D7 INTEGRATES WITH" | 0071 §战略协调 L58 | ✅ 应用 | 文本替换确认 |
| **#2a** | 2 llm.dsl.* 主题 "⚠️ pending + ADR-0068 amendment" | 0074 D7 (L22, L375-376, L532, L529, L558) | ✅ 应用 | 5 处文本确认 |
| **#2b** | 2 env.backend.* 主题同 | 0075 §风险 (L20, L165, L368) | ✅ 应用 | 3 处文本确认 |
| **#2c** | 6 mcp.* 主题同 | 0076 D7 (L23, L486, L624-625) | ✅ 应用 | 4 处文本确认 |
| **#2d** | 4 grpc.* 主题同 | 0077 D5 (L392-395, L567) | ✅ 应用 | 6 处文本确认 |
| **#3** | `parse-valid < 90%` → `85% ≤ x < 90%` 临界带 | 0072 D3 (L54, L173-179) | ✅ 应用 | 文本替换 + 新增"为何用临界带"说明 |
| **#4** | 拆分 `env:` → `backend:` + `ExecOptions.env` → `env_vars:` | 0071 §3.A L212 | ✅ 应用 | 拆分为 2 个独立声明 |
| **#5** | "`backend:` 字段必填" → "推荐必填, 缺省 local" | 0075 §不变量 3 L331 | ✅ 应用 | 文本替换确认 |

**总编辑数**: ~18 处, 跨 6 个 ADR (0071/0072/0074/0075/0076/0077)

**验证**: `tools/adr_lint.py` 72/72 PASS (零回归)

**Oracle session**: `ses_037e12115ffeLkeR1QTIko0BHb` (可续接细化审查)

---

## 七、量化数据 + 测试覆盖

### 7.1 核心模块达成率

| 模块 | ADR 对应 | 实施率 | 备注 |
|------|---------|:---:|------|
| DSLEngine 核心 | 0019, 0033, 0008 | **100%** | PIMPL-lite 解耦完成, 仅 1 types 例外 |
| 抽象接口层 | 0019 §1.4 | **100%** | 7 接口全部实现 |
| 认知编排 | 0020 | **95%** | FleetOrchestrator DEFER |
| PDK 系统 | 0021, 0034 | **100%** | 3 Agent 循环 + ToolRegistry V2 |
| Skill 隔离 | 0055, 0066 | **80%** | V1 done, V2 deferred |
| LLM Providers | 0001, 0005, 0042 | **100%** | 3 Decorators + CloudLLMAdapter |
| 异步运行时 | 0030 V2 | **85%** | Fleet DEFER |
| 策略/审批 | 0031 | **85%** | 4 项 defer 至 C6 |
| 插件加载 | 0022, 0041 | **100%** | PluginLoader V2 shipped |
| 调度/执行 | 0033, 0019 | **100%** | TopoScheduler + NodeExecutor |
| EventBus 基础设施 | 0019, 0002, 0037, 0046 | **90%** | BusEvent + subscribe_glob + CausalClock ship |
| Temporal Agent | — | **100%** | pkgm-temporal-agent Phase 1+2 完整 ship (41/41 tasks, 10/10 ctest) |
| Tool Metadata V3 | **0073** | **🟡 30%** | Schema 字段已 ship, DECLARE_TOOL 自动生成 + 校验层 待实施 |
| **LLM-native 架构** | **0071-0078** | **🟡 3.7%** | 顶层 + Schema 部分 ship; Prompt/Backend/MCP/gRPC/Fine-tune 未启动 |
| **MCP/gRPC 协议** | **0076/0077** | **0%** | docs-only, 启动条件未满足 |

### 7.2 测试覆盖

| 指标 | 数值 |
|------|:---:|
| 测试文件数 | 100+ (`.cpp` 测试文件) |
| 最新 ctest | 97/98 PASS (2026-07-30 main 分支 + pre-existing `test_cost_tracking_decorator` 失败) |
| main 分支状态 | 97/98 零回归, 3 EventBus 变更 + pkgm-temporal-agent + LLM-native ADR 起草全部 ship |

### 7.3 ADR 起草统计 (本会话产出)

| 指标 | 数据 |
|------|:---:|
| 起草 ADR 总数 | 6 (0072/0074/0075/0076/0077/0078) |
| 总行数 | 3,548 行 |
| 跨 Wave 覆盖 | Wave 2.2 + 2.4 + 3 + 4 + 5+ 全部完整 |
| Oracle MUST-FIX 应用 | 5 项, 18 处编辑 |
| 新架构分析 | `llm-native-blueprint-vs-code-gap-analysis.md` (367 行) |

---

## 八、关键发现与建议

### 🔴 高优先级

1. **Wave 3 启动条件**: 4 项结构性暂缓 (AgentForge ≥ Sprint 25, Solo Dev ≥2 人, ADR-0068 amendment ship, ADR-0073 翻牌)
2. **ADR-0068 §附录 A 主题注册**: 14 个候选主题待 PR, 阻塞 4 个 LLM-native ADR 实施
3. **ADR-0042 状态不匹配**: 5 项 C16 增量决议已落地为代码, ADR 仍 🔍 Proposed

### 🟡 中优先级

4. **Solo Dev 容量超额**: Wave 3 4-6 周 vs Phase 6b 44h (3.5-5x), 必须 descope
5. **ADR-0031 4 项 defer**: 工具调用无超时保护, Phase 6 应重新评估
6. **ADR-0007 LLM 压缩**: 长对话场景瓶颈
7. **ADR-0073 翻牌**: 🔍 → 🟡 Partial (Sprint 21 部分 ship)
8. **LLM-native Phase 6b 推进**: 24-32h 强制决策可 ship, 但 Wave 3 descope 路径需明确

### 🟢 低优先级

9. **Phase 6 ADR (0052-0064)**: 13 个 ADR Approved 但多数无代码, 按 Phase 6 节奏推进
10. **已归档 ADR (0010-0018)**: 认知增强功能推迟, Phase 6 重新评估非当前焦点
11. **ADR-0037 CausalClock**: 基础 ship 完成, 分布式向量时钟留待跨进程需求
12. **ADR-0077/0078 descoped 重新激活条件**: 4 项各自明确, Phase 7+ 评估

---

## 九、演进路径建议

```
2026-08-03 (当前) ──→ Phase 6b (Sprint 25, ~08-19)
  ✅ 6 个 LLM-native ADR 起草           🔴 Wave 3 启动条件评估
  ✅ Oracle MUST-FIX 5 项应用            🟡 ADR-0073 翻牌 🟡 Partial
  ✅ 14 候选主题标注 pending             🟡 ADR-0068 amendment PR ship
  ⚠️ Wave 3 结构性暂缓                  🟡 Wave 2 强制决策 ship (24-32h)
  ✅ Phase 6a Sprint 24 推进              🟢 ADR-0042 状态对齐
                                        🟢 ADR-0031 C6 收尾评估
                                        🟢 Phase 6 ADR 代码实施

Phase 6b 末 (Sprint 25) ──→ Phase 7 (Sprint 26+)
  ⚠️ Wave 3 启动决策                   🔴 AgentForge ≥ Sprint 25 milestone
  ⚠️ 容量评估 (Wave 3 descope)          🔴 Solo Dev ≥2 人 OR descope
  ⚠️ 14 主题注册推进                     🟡 Wave 3 Phase 1 (LocalBackend only)
                                        ⏸ gRPC/Fine-tune descoped

Phase 7+ ──→ Phase 8+
  ✅ Wave 3 ship (Local+Docker)         ⏸ gRPC Wave 4 (条件触发)
  ✅ MCP server ship (服务化基础)        ⏸ Fine-tune Wave 5+ (AgenticMind 回流)
  ✅ Evidence Gate 运转
```

---

## 附录 A: 完整 ADR 状态清单

### ✅ Approved（32 个）

| ADR | 标题 | 批准日期 | 备注 |
|-----|------|---------|------|
| 0001 | ILLMProvider 流式接口 | 2026-05-28 | C16 增补 Decorator/Dual Consumer |
| 0003 | DSLEngine 线程安全 | 2026-05-12 | 代码已落地 |
| 0004 | ToolRegistry 安全模型 V2 | 2026-07-02 | C6 ship |
| 0005 | LLM 后端配置工厂 | 2026-05-12 | C16 增补修订 |
| 0008 | 结构化 Context | 2026-06-12 | Sprint 20 桥接期 ship |
| 0009 | DSL 标准库规划 | 2026-05-12 | |
| 0020 | 多智能体线程模型 | 2026-06-24 | Sprint 5 ship |
| 0021 | PDK 设计 | 2026-06-24 | Sprint 20 增量 |
| 0022 | 插件加载机制 | 2026-06-24 | Sprint 5 ship |
| 0023 | ToolResult 标准化 | 2026-06-24 | Sprint 5 ship |
| 0033 | Session Hierarchy | 2026-07-02 | C5 ship |
| 0034 | IModelRouter 模型路由 | 2026-07-02 | C7 ship |
| 0035 | 推理引擎 Plugin 规范 | 2026-07-10 | C14 ship |
| 0040 | 推理引擎构建策略 | 2026-07-10 | C14 ship |
| 0041 | PluginLoader 生命周期 | 2026-07-10 | C16 ship |
| 0043 | PDK 工具命名约定 | 2026-07-10 | C13/C14 D3 |
| 0044 | 推理引擎安全模型 | 2026-07-10 | C14 ship |
| 0050 | Phase 6 战略评估 | 2026-07-23 | 转向 PDK 生产化 |
| 0051 | Phase 6 Composition Spike | 2026-07-15 | W3 ship gate |
| 0052 | Agent Plugin Manifest | 2026-07-16 | 架构评审确认 |
| 0053 | AgentDescriptor 接口 | 2026-07-16 | 架构评审确认 |
| 0054 | Capability Discovery | 2026-07-16 | 架构评审确认 |
| 0055 | SKILL.md 执行隔离 | 2026-07-22 | 实施完成 |
| 0056 | Wasm Agent 运行时 | 2026-07-16 | 无代码 |
| 0057 | Agent 生命周期 | 2026-07-16 | 无代码 |
| 0058 | Schema 强制校验 | 2026-07-16 | 无代码 (衔接 0073) |
| 0059 | 跨进程协议 | 2026-07-16 | 无代码 |
| 0060 | Agent 组合协议 | 2026-07-16 | 6 种协作模式 |
| 0061 | Agent 进化与固化 | 2026-07-16 | 父 ADR |
| 0062 | Agent Marketplace | 2026-07-16 | 无代码 |
| 0063 | OpenTelemetry 追踪 | 2026-07-16 | 无代码 |
| 0064 | Conformance Test Suite | 2026-07-16 | 无代码 |
| 0067 | 分层插件架构拆分 | 2026-07-23 | 追溯性正式化 |
| 0084 | Mutation Governance 契约 | 2026-08-26 | V1 gate-and-audit ship (commit `a2b2d52`), G11 ✅ Closed |

> **注**: ADR-0032 (CostCollector, 2026-06-30) 已实施后归档至 `docs/archive/adr/`, 不在此 Approved 列表中.

### ADR-0061 子项 (12 个)

| 子项 | 名称 | 状态 | 备注 |
|------|------|------|------|
| 0061-01 | SKILL.md 标准对齐 | ✅ Approved | P0 |
| 0061-02 | 行为回归套件 | ✅ Approved | P0, 无代码 |
| 0061-03 | SkillCompiler 实施 | ✅ Approved | P0, 无代码 |
| 0061-04 | SLM 路由优先 | ✅ Approved | P1 |
| 0061-05 | wasi-sdk 集成 | ✅ Approved | P1, 无代码 |
| 0061-06 | Trajectory IR 独立序列化视图 (v1.1) | ✅ Approved + Shipped | P1, T15 V1 ship 2026-08-27 (9 cases / 55 assertions, ParsedGraph 零修改) |
| 0061-07 | PASTE 推测执行 | 🔍 Proposed | P2 |
| 0061-08 | MCTS 工作流搜索 | ✅ Approved | P2, T20 V1 ship 2026-08-28 (17 cases / 65 assertions, 既有 5 契约零修改) |
| 0061-09 | GEPA 反思循环 | 🔍 Proposed | P2 |
| 0061-10 | Config 结构检查 | 🔍 Proposed | P2 |
| 0061-11 | DSL→Wasm 编译器 | 🔍 Proposed | P2 |
| 0061-12 | WebLLM 集成 | 🔍 Proposed | P2 |

### 🟡 Partial (8 个, 含 ADR-0073 待翻牌)

| ADR | 标题 | 缺失项 |
|-----|------|--------|
| 0007 | 上下文压缩 | 无 LLM 压缩 |
| 0019 | IInteractionBus | subscribe_glob 已 ship, subscribe_topic 关闭 |
| 0030 V2 | 异步运行时 | FleetOrchestrator DEFER |
| 0031 | 执行策略 | 4 项 defer 至 C6 |
| 0037 | 因果序 | CausalClock 基础 ship, 分布式向量时钟 defer |
| 0066 | SkillInterpreter 架构 | V1 done, V2 deferred |
| **0073** | **Tool JSON Schema 契约** | **待翻牌 (Phase 5 Sprint 21 部分 ship)** |

### 🔍 Proposed (13 个, 含 6 个新 LLM-native)

| ADR | 标题 | 备注 |
|-----|------|------|
| 0038 | 动态配置接口 | BatchingQueue 延迟 |
| 0039 | 性能元数据契约 | JSON 工具未实现 |
| 0042 | ILLMProvider 演进路径 | C16 决议已记录, 状态不匹配 |
| 0045 | 编排 Plugin 规范 | 实施率 ~20% |
| 0046 | 插件间通信协议 | 实施率 ~35% |
| 0061-07~12 | P2 子项 (6 个) | v2 candidate |
| **0071** | **LLM-native AgenticDSL** | **本会话起草, 顶层** |
| **0072** | **DSL 节点扩展** | **本会话起草, Wave 2.4 GATED** |
| **0074** | **Prompt + Evidence Gate** | **本会话起草, Wave 2.2** |
| **0075** | **EnvBackend Local+Docker** | **本会话起草, Wave 3** |
| **0076** | **DSL Engine as MCP Server** | **本会话起草, Wave 3 末 GATED** |
| **0077** | **gRPC Data Plane** | **本会话起草, Wave 4 descoped docs-only** |
| **0078** | **Fine-tune 基模** | **本会话起草, Wave 5+ descoped docs-only** |

### ⛔ Superseded (1 个)

| ADR | 标题 | 被替代为 |
|-----|------|---------|
| 0006 | HarnessEngine 线程模型 | ADR-0020 |

### ❌ Not Implemented (18 个)

| ADR | 标题 | 位置 | 备注 |
|-----|------|------|------|
| 0002 | EventBus 有界队列 | `docs/adr/` | V2 设计锁定, 功能由 ADR-0019 覆盖 |
| 0010-0018 | Phase 0 认知/记忆 (9 个) | `docs/archive/adr/` | 2026-06-09 整批归档 |
| 0030 V1 | 异步运行时 V1 | `docs/archive/adr/` | 被 V2 替代 |
| 0036 | 三层服务协议 / 混合内核 (2 文件) | `docs/archive/adr/` | 未实施 |

### 📦 Archived (1 个)

| ADR | 标题 | 归档日期 | 备注 |
|-----|------|---------|------|
| 0032 | CostCollector | 2026-06-30 | 已实施后归档 |

---

## 附录 B: 验证命令

```bash
# ADR 总数 + 状态分布
python3 tools/adr_lint.py

# ADR 实施状态基线 (本文档唯一事实源)
cat docs/architecture/adr-implementation-status-gap-analysis.md

# LLM-native 蓝图深度分析
cat docs/architecture/llm-native-blueprint-vs-code-gap-analysis.md

# Oracle 审查 session 续接
# session_id: ses_037e12115ffeLkeR1QTIko0BHb

# Phase 6 Candidate B 启动条件
cat docs/active-status.md | grep -A 5 "Candidate B"

# ADR-0068 §附录 A 主题清单
grep -E "^### |^#### " docs/adr/adr-0068-event-emission-contract.md | head -50

# 本会话起草的 6 个新 ADR
ls docs/adr/adr-007{2,4,5,6,7,8}*.md | xargs -I {} wc -l {}
```

---

*文档版本: v1.1 (本会话整合重写版)*
*创建日期: 2026-08-03*
*作者: Sisyphus (guide-arch Phase 3 重写)*
*上次整合: 2026-08-03 — 集成 `llm-native-blueprint-vs-code-gap-analysis` (367 行) + Oracle 审查 (5 MUST-FIX, 18 处编辑) + ADR-0073 翻牌待 apply*
*下一次更新*: Sprint 25 末 (Wave 2 强制决策 ship 后) 或 ADR-0073 翻牌时

**变更摘要** (vs 2026-07-30 v1.0):
- §一 总体状态: 59 → 72 ADR (+6 新起草 LLM-native, +1 ADR-0073 翻牌)
- §二.2.1: 7 Partial → 8 Partial (+ADR-0073 待翻牌)
- §二.2.2: 5 + 6 P2 → 7 + 6 LLM-native (8 个 LLM-native ADR 详细)
- §三 LLM-native 深度: 新增章节 (跨 ADR 依赖图 + Solo Dev 容量分析)
- §四 ADR-0068 主题注册: 新增 (14 候选主题清单 + 注册流程)
- §五 Phase 6 Candidate B 启动条件: 新增 (4 项条件 + Wave 3 启动决策树)
- §六 Oracle MUST-FIX 应用: 新增 (5 项 + 18 处编辑追踪)
- §七.1 量化: 新增 LLM-native 架构 (3.7%) + Tool Metadata V3 (30%) + MCP/gRPC 协议 (0%) 行
- §九 演进路径: 更新至 Phase 6b/7+ 双轨推进路径