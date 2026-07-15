<!--
  文件: docs/superpowers/plans/2026-07-14-phase6-agent-os-reference.md
  功能: Agent OS 愿景与 Phase 6 Service-ification 完整参考(策略 + v1 实施 + 方法论 + 决策树)
  作者: Sisyphus (HydraForge Team)
  日期: 2026-07-14
  状态: 🟡 Partial (Phase 1 + Phase 2 reconciliation 完成 — 2026-07-14)
  关联: ADR-0050 (Phase 6 战略) + ADR-0051 (Phase 6 PDK Composition Spike, 权威实施层)
        + Oracle 三轮 sessions: ses_0a2102c3effeVNVGOf5HxMgizu (Round 1)
        + ses_0a206a23cffe1IEirU5iNaxFxC (Round 2)
        + ses_0a09fe379ffeCQiEwcDo4y9XQd (Round 3 reconciliation)
  用途: 战略愿景与方法论参考(战术实施以 ADR-0051 为准)

  ⚠️ ============================================================
  📌 RECONCILIATION HEADER — 2026-07-14 Phase 1 + Phase 2 应用
  ⚠️ ============================================================

  本文档创建于 2026-07-14,**在 ADR-0051 commit c33b132 之前**。
  创建后立即发现 ADR-0051 (Phase 6 PDK Composition Spike) 已存在,
  且与本文档战术部分(§六/§七/§八/§九/§十/附录 C-D)存在 16 处分歧。

  Oracle Round 3 (ses_0a09fe379ffeCQiEwcDo4y9XQd) 给出 reconciliation 决议:
  - ✅ §二/§三/§四/§十三 (战略愿景 + 5 Agent 概览 + 未来演进) 仍有效
  - 🟡 §五/§六/§七/§八/§九/§十/§十二/附录 B/附录 D (Phase 2 校准后战术部分) 已对齐 ADR-0051
  - ⛔ 不再独立声称"v1 实施",仅作"Spike 战略愿景 + 方法论"参考
  - 📌 单一真理源层级: ADR-0051 (权威) > OpenSpec tasks.md (实施) > 本文档 (愿景)

  首次读者请先读: docs/adr/adr-0051-phase6-pdk-composition-spike.md

  Phase 1 已应用的修正(2026-07-14 早些时候):
  - 8 处 `knowledge_base.query` → `knowledge_base/query` (slash, per ADR-0043)
  - 3 处 "JSON in/JSON out" → `unordered_map<string,string>`-in / `nlohmann::json`-out
  - DECLARE_TOOL 引用加 deprecated 注 (实际用 IToolRegistry::register_tool_function)
  - 2 处 `call_log` 引用 → 复用现有 `tool.audit.*` 事件
  - 2 处 ADR-0051 路径错误 → 修正为 spike 路径
  - §六 W1→W2 过渡加 BLOCKED gate 标注
  - §一.3 关系图重绘(4 层真理层级 + Oracle 3 sessions)
  - §六.2 D1/D2 改为"复核"而非"创建"(OpenSpec 已存在)

  Phase 2 校准已应用的修正(2026-07-14 晚些时候):
  - §五.3 缺陷 #3 重写:Transport 泄漏从 JSON-in/JSON-out 上下文重写为 Spike 实际合约
  - §十二 全面重写:Onboarding 唯一入口指向 `docs/service-composition/spike-onboarding.md`(W2 D5 产出)
  - 附录 B 追加:Spike → Candidate B 5 项提升标准(per ADR-0051 §5)
  - 附录 D 决策树根节点更新:`IToolRegistry::register_tool_function()` 而非 DECLARE_TOOL
  - 所有"v1"措辞调整为"Spike",反映 ADR-0051 framing

  Phase 3 结构重组待 Spike ship 后执行(§六-§十 附录 C-D 归档 + 战略部分提取)
  ⚠️ ============================================================
-->

# HydraForge Agent OS 愿景 + Phase 6 Service-ification 完整参考

> **TL;DR** — Phase 6 的本质不是"暴露 MCP/OpenAI API 给外部",而是"为 HydraForge 上的 5 个领域 Agent 提供互相提供服务的标准机制"。v1 (Spike) 策略:**不引入任何新抽象,复用现有 IToolRegistry 接口完成 G1+G3 演示,让 awkward 模式自然涌现后再形式化 DECLARE_SERVICE**(推迟到 Phase 6 v2+;需 Spike→Candidate B 5 项提升标准全部满足)。本文档捕获完整推理链 + 可执行方法论 + 决策树。⚠️ **战术实施以 [ADR-0051](../adr/adr-0051-phase6-pdk-composition-spike.md) 为权威,本文档仅作战略愿景参考。**

---

## 目录

- [一、文档目的与读者](#一文档目的与读者)
- [二、核心洞察:HydraForge 是 Agent OS](#二核心洞察hydraforge-是-agent-os)
- [三、Phase 6 服务化的真实含义](#三phase-6-服务化的真实含义)
- [四、5 个领域 Agent(G1-G5)概览](#四5-个领域-agentg1-g5概览)
- [五、6 项架构缺陷防范清单](#五6-项架构缺陷防范清单)
- [六、Phase 6 v1 实施计划(W1-W3)](#六phase-6-v1-实施计划w1-w3)
- [七、G1+G3 最小可行定义](#七g1g3-最小可行定义mvp-scope)
- [八、Awkward 模式检测方法学](#八awkward-模式检测方法学)
- [九、ADR 影响与命名约定](#九adr-影响与命名约定)
- [十、Kill Criterion(终止判据)](#十kill-criterion终止判据)
- [十一、决策点状态(D0/D1/D2/D3)](#十一决策点状态d0d1d2d3)
- [十二、Onboarding 与团队扩展](#十二onboarding-与团队扩展)
- [十三、未来演进路径](#十三未来演进路径)
- [附录 A:5 项启动条件核验清单](#附录-a5-项启动条件核验清单)
- [附录 B:Awkward 模式触发阈值](#附录-bawkward-模式触发阈值)
- [附录 C:Escalation Triggers 矩阵](#附录-cescalation-triggers-矩阵)
- [附录 D:决策树(你的 Agent 适配 v1 吗)](#附录-d决策树你的-agent-适配-v1-吗)
- [参考文档](#参考文档)

---

## 一、文档目的与读者

### 1.1 文档目的

本文档是 **Agent OS 开发的唯一长期参考**,承担 3 个角色:

1. **战略记录**:捕获 2026-07-14 这一时点对 Phase 6 Service-ification 的全部决策推理,包括:
   - 为什么从 ADR-0050 的"暴露外部 API"重塑为"内部 Agent 互相提供服务"
   - 为什么不立即形式化 DECLARE_SERVICE
   - 为什么 in-process α 而非 cross-process β
2. **v1 实施手册**:G1+G3 demo 团队(及未来的 G2/G4/G5 团队)可直接按本文档 §六-W1-W3 排期与 §七 MVP scope 启动工作,无需重做决策。
3. **方法论模板**:Awkward 模式检测 §八、Kill Criterion §十、决策树附录 D,可作为所有后续 Phase 6+ 工作的标准操作流程(SOP)。

### 1.2 目标读者

| 读者 | 使用本文档的方式 |
|------|-----------------|
| **Phase 6 v1 实施者**(1.5 工程师) | §六 W1-W3 + §七 MVP scope + §八 检测 + §十 kill criterion |
| **G2/G4/G5 团队** | §四 5 Agent 概览 + §十二 onboarding 决策树 + 附录 D |
| **5 个 Agent 团队负责人** | 启动阶段读 §十一 决策点状态,理解 v1 合约会变 |
| **未来架构师** | §二 核心洞察 + §五 6 项缺陷 + §九 ADR 影响 |
| **Onboarding 新人** | 通读,理解 HydraForge 整体愿景 |

### 1.3 与其他文档的关系(2026-07-14 reconciliation 后)

```
═══════════════════════════════════════════════════════════════════════
  真理层级(Truth Hierarchy) — 严格从高到低
  ═══════════════════════════════════════════════════════════════════════

   ┌───────────────────────────────────────────────────────────────┐
   │ 1️⃣  ADR-0051 (Phase 6 PDK Composition Spike)  ←  权威入口     │
   │     docs/adr/adr-0051-phase6-pdk-composition-spike.md         │
   │     状态: 🔍 Proposed → W3 ship 后 ✅ Approved (experimental) │
   │     包含: 6 个 Oracle 决策 + 5 项 Spike→Candidate B 标准      │
   └────────────────────────────┬──────────────────────────────────┘
                                │
                                ▼
   ┌───────────────────────────────────────────────────────────────┐
   │ 2️⃣  OpenSpec `phase6-service-ification-v1`  ←  实施跟踪     │
   │     openspec/changes/phase6-service-ification-v1/             │
   │     包含: proposal.md + design.md + tasks.md + 3 spec files   │
   │     当前: W1 fix list 11/11 完成, W2-W3 BLOCKED                │
   └────────────────────────────┬──────────────────────────────────┘
                                │
                                ▼
   ┌───────────────────────────────────────────────────────────────┐
   │ 3️⃣  ADR-0050 (Phase 6 战略评估)  ←  战略层参考                 │
   │     docs/adr/adr-0050-phase6-strategic-evaluation.md          │
   │     状态: 🔍 Proposed(Phase 6 正式 v1 启动前不变)             │
   └────────────────────────────┬──────────────────────────────────┘
                                │
                                ▼
   ┌───────────────────────────────────────────────────────────────┐
   │ 4️⃣  本文档 (Agent OS Reference)  ←  战略愿景 + 方法论参考     │
   │     docs/superpowers/plans/2026-07-14-phase6-agent-os-reference.md
   │     状态: 🟡 Partial (§二-§四 §十三 ✅ Valid,                  │
   │                    §六-§十 附录 C-D ⛔ Superseded by ADR-0051) │
   └───────────────────────────────────────────────────────────────┘

  Oracle Sessions (可追溯决策推理):
    Round 1: ses_0a2102c3effeVNVGOf5HxMgizu (Phase 6 架构缺陷分析)
    Round 2: ses_0a206a23cffe1IEirU5iNaxFxC (实施细节 + 风险路径)
    Round 3: ses_0a09fe379ffeCQiEwcDo4y9XQd (Reconciliation + 防架构债)
═══════════════════════════════════════════════════════════════════════
```

**关系说明**:
- **本文档不取代 ADR-0051**:0051 是 Phase 6 战术层权威(2026-07-14 commit c33b132)。本文档的 §六/§七/§八/§九/§十/附录 C-D 已被 0051 取代(战术细节冲突),仅 §二-§四 §十三(战略愿景 + 5 Agent 概览 + 未来演进)仍有效
- **本文档不取代 OpenSpec**:正式 ship 仍走 `phase6-service-ification-v1` 流程;本文档作为 ADR-0051 的概念背景
- **本文档不取代 Oracle 答复**:Oracle 三轮的具体决策可追溯到 session IDs,但推理已合并到本文档 + ADR-0051
- **唯一真理源声明**:未来读者**先读 ADR-0051**(5 分钟),**再回读本文档 §二-§四 §十三**(理解战略愿景)

---

## 二、核心洞察:HydraForge 是 Agent OS

### 2.1 重塑时刻

ADR-0050(2026-07-10 sprint 22)将 Phase 6 推荐为 Candidate B"服务化",其原始框架是:
> "暴露 HydraForge 能力为外部可消费服务 — InferenceServer MCP + OpenAI-compatible API"

这是**outward-facing 服务化**(给外部 LLM 客户端用)。

2026-07-14 用户输入彻底重塑这个理解:
> "HydraForge 是内容团队的 Agent 平台基础设施,内部团队会通过 PDK 插件形式开始不同领域的 Agent,这个项目就是提供了公共的基础设施,让团队开 Agent 专注于不同领域的业务。"

这是**inward-facing 服务化**(Agent OS 内部组件互调)。

### 2.2 重塑后愿景:HydraForge = Agent OS

```
═══════════════════════════════════════════════════════════════════════
  THE VISION: HydraForge as Agent Operating System
  ═══════════════════════════════════════════════════════════════════


                          USER APPLICATIONS (PDK)
                          ═══════════════════
                          ┌──────────────────────────┐
                          │  所有"软件" = PDK plugin │
                          │  ┌────────────────────┐  │
                          │  │ Coding Assistant   │  │  ← G1 (Phase 6 v1)
                          │  ├────────────────────┤  │
                          │  │ Agentic Browser    │  │  ← G5 (Phase 7+)
                          │  ├────────────────────┤  │
                          │  │ Knowledge Base     │  │  ← G3 (Phase 6 v1)
                          │  ├────────────────────┤  │
                          │  │ Agentic Memory     │  │  ← G4 (Phase 7+)
                          │  ├────────────────────┤  │
                          │  │ Agentic Inf Engine │  │  ← G2 (Phase 7+)
                          │  ├────────────────────┤  │
                          │  │ ... N 个未来 PDK   │  │  ← Phase 7+
                          │  └────────────────────┘  │
                          └────────────┬─────────────┘
                                       │
                          ════════════╪═════════════
                           PDK SERVICE COMPOSITION   ← Phase 6 核心交付物
                          ════════════╪═════════════
                                       │
                          ┌────────────┴─────────────┐
                          │       KERNEL              │
                          │  • DSLEngine              │
                          │  • TopoScheduler (DAG)    │
                          │  • Session Hierarchy      │  ← ADR-0033
                          │  • ToolRegistry           │
                          │  • PDK Runtime            │  ← ADR-0021
                          │  • PluginLoader           │  ← ADR-0022
                          └──────────────────────────┘


  ════════════════════════════════════════════════════════════════════
  Unix 类比:
  ════════════════════════════════════════════════════════════════════

  HydraForge Phase 0-5  = 微内核 (microkernel + drivers)
  HydraForge Phase 6    = 系统调用层 + IPC (POSIX syscall + IPC)
  HydraForge Phase 7+   = 用户态服务 (userland daemons, systemd, etc.)

  ───────────────────────────────────────────────────────────────────
  ⚠️ 不是比喻 — 是 ADR-0050 的"路线依赖"推理:

  • 没有 PDK Service Composition,5 个 Agent 不能互相调用
  • 5 个 Agent 不能互相调用 → 永远只能跑单一 Agent
  • 永远只能跑单一 Agent → "所有软件 PDK 化"愿景无法落地
  • 愿景无法落地 → Phase 7+ "应用生态" 无附着点

  所以 Phase 6 = 不可跳过的"中间层"。
  ════════════════════════════════════════════════════════════════════
```

### 2.3 关键洞察清单(未来参考)

| # | 洞察 | 影响 |
|---|------|------|
| I1 | HydraForge 是 **Agent OS 平台**而非 API 产品 | Phase 6 战略层需重述,文档/vision 都受影响 |
| I2 | 5 团队通过 **PDK 插件**开发领域 Agent | PDK 是产品,不是 internal tool |
| I3 | 服务化 = **Agent 互相提供服务**(Unix 微内核哲学) | 第 3 概念("service")出现时机需谨慎 |
| I4 | "外部消费者"在 Phase 6 v1 **不重要**(团队还没开始) | W5 E2E demo 是内部 G1+G3 |
| I5 | ADR-0020 + ADR-0033 + TopoScheduler 已支持 in-process DAG | Phase 6 大量基础设施已 ship,无需重建 |
| I6 | Sprint 20 `LoopResult` BREAKING 是抽象承压的**历史信号** | 不能再"加抽象",应"复用现有" |

---

## 三、Phase 6 服务化的真实含义

### 3.1 服务化在 Agent OS 语义下的定义

**Agent 之间互相提供服务 = 5 个 PDK plugin(G1-G5)能像函数库一样互相调用。**

#### 同义于 Unix pipe 的哲学:

```
  Unix pipe:                HydraForge Service:
  $ cat file.txt |          G1 → call_tool("knowledge.query")
      grep "X" |                │
      sort | uniq               ▼
                          G3(L3 session 检索 + LLM)
                                 │
                                 ▼
                             返回 ToolResult
```

### 3.2 服务化的 3 个真实价值

| 价值 | 体现 |
|------|------|
| **协作复用** | G1 编程助手可调用 G3 知识库,无需各自实现 RAG |
| **能力即服务** | G2 推理引擎作为公共服务,被 5 个其他 Agent 调用 |
| **基础设施收敛** | session、approval、audit、cost tracking 自动应用到 Agent 互调 |

### 3.3 服务化 NOT(避免误读)

| 误读 | 实际 |
|------|------|
| ❌ 对外暴露 API 给 LLM 客户端 | ✅ **暂不做**(Phase 6 v1 不引入) |
| ❌ 引入 MCP 协议给 Claude Desktop | ✅ **不做**(内部场景无价值) |
| ❌ 立刻形式化 DECLARE_SERVICE 宏 | ✅ **推迟**(让 awkward 模式自然涌现) |
| ❌ 跨进程 IPC 服务化 | ✅ **v1 in-process**,β 推迟到触发条件 |
| ❌ 每个 Agent 是独立微服务 | ✅ v1 单进程,通过 TopoScheduler 调度 |

### 3.4 Phase 6 v1 范围边界

**In-Scope(W1-W3 ship target)**:
- G1 Coding Assistant(PDK plugin)
- G3 Knowledge Base(PDK plugin)
- G1 通过 `call_tool("knowledge.query", ...)` 调用 G3
- Causal trace ID 通过 IInteractionBus 传播
- Session 继承规则(UserSession 继承,TaskSession 新建)
- 审批继承(ToolCoordinator 顶层一次审批)
- "Awkward patterns 观察文档"(为 v2 DECLARE_SERVICE 设计输入)

**Out-of-Scope(Phase 7+ 或推到候选列表)**:
- G2/G4/G5 任何一项的 ship
- 跨进程 IPC
- MCP / OpenAI 兼容 bridge(留作 Option,不在 v1 默认)
- 物理隔离(进程级)
- 多租户 / 24-7 后台 Agent
- 流式 streaming 接口(为 G5 浏览器预留)

---

## 四、5 个领域 Agent(G1-G5)概览

### 4.1 G1-G5 调用语义对比(关键决策表)

| Agent | 调用语义 | 状态 | v1 适配 |
|-------|---------|------|--------|
| **G1** Coding Assistant | **多轮对话 + 调用其他 Agent** | 🔵 已确立 v1 demo 目标 | ✅ 在 v1 中 |
| **G2** Agentic Inference Engine | **单次请求/响应**(可能 streaming) | 🟡 候选 v1 demo 后扩展 | 留 v2 |
| **G3** Knowledge Base | **sync query/response** + session(多轮检索) | 🔵 已确立 v1 demo 目标 | ✅ 在 v1 中 |
| **G4** Agentic Memory | **KV-store transactional** | 🟡 候选 | 留 v2 |
| **G5** Agentic Browser | **stateful stream + side effects** | 🟡 候选 | 留 v2(可能推动 DECLARE_SERVICE) |

### 4.2 G1-G5 MVP 实施顺序(Phase 6 v1+ 演进路径)

```
                   Phase 6 v1 (1.5-3 周,1.5 eng)
                              │
                              ▼
                       G1 ←→ G3 demo
                   (强制暴露 multi-turn + query
                    两种语义,最少张力)
                              │
                  ┌───────────┴───────────┐
                  ▼                       ▼
              + G4(KV)               + G5(streaming)
              (暴露 transactional)    (暴露 sync-async impedance)
                  │                       │
                  └───────────┬───────────┘
                              ▼
                          + G2(single-shot)
                          (最简单,验证退化情形)
```

**为什么 G1+G3 先**:
- G1 = 多轮对话(暴露 session 继承 awkward #3)
- G3 = query + session(暴露 stateful tool awkward #1)
- 两者组合恰好暴露 **2 种调用语义**,不一次性压 5 种
- 若 G1+G3 都不暴露 awkward → v1 合约就够用 → DECLARE_SERVICE 永久推迟

### 4.3 G1-G5 协调状态机(后续团队如何参与)

```
  ┌────────────────────────────────────────────────────┐
  │  当前状态 (2026-07-14)                              │
  │  • G1/G3: v1 demo 在 W1-W3 ship                    │
  │  • G2/G4/G5: ⏸️ 未启动,等 v1 ship + onboarding     │
  └────────────────────────────────────────────────────┘
                          │
                          ▼
  ┌────────────────────────────────────────────────────┐
  │  v1 ship 后(预估 W3 末,W3 = 2026-07-28)            │
  │  • D3 决策:5 团队 kickoff(1 次性,validated v1)     │
  │  • 团队按 §十二 onboarding + 附录 D 决策树         │
  │  • 团队自评适配 v1 vs 需推 DECLARE_SERVICE         │
  └────────────────────────────────────────────────────┘
                          │
                          ▼
  ┌────────────────────────────────────────────────────┐
  │  G4 加入(估时 +1 周)                                │
  │  • 暴露 transactional semantics                     │
  │  • 强制 Service Composition 处理幂等性               │
  │  • 触发条件:2+ awkward 模式不同类别                 │
  └────────────────────────────────────────────────────┘
                          │
                          ▼
  ┌────────────────────────────────────────────────────┐
  │  G5 加入(估时 +2 周)                                │
  │  • 暴露 streaming + side effects                    │
  │  • 强制 Service Composition 处理 sync-async         │
  │  • ⭐ 最可能触发 DECLARE_SERVICE 形式化              │
  └────────────────────────────────────────────────────┘
                          │
                          ▼
  ┌────────────────────────────────────────────────────┐
  │  G2 加入(估时 +0.5 周)                              │
  │  • 验证 single-shot 退化                            │
  │  • 最简单,无新抽象需求                              │
  └────────────────────────────────────────────────────┘
                          │
                          ▼
  ┌────────────────────────────────────────────────────┐
  │  Phase 6 v2: 形式化 DECLARE_SERVICE(估时 4-6 周)    │
  │  • 当 ≥2 团队报告同 awkward pattern →               │
  │    用 §八 methodology 量化证据 →                   │
  │    形式化新 ADR-0051+                               │
  │  • β 跨进程迁移:若届时 ≥30GB 单进程 OR 24-7 需求  │
  └────────────────────────────────────────────────────┘
```

---

## 五、6 项架构缺陷防范清单

> **必读**:每项缺陷的"触发信号"是**自动化检查**的提示,不应出现时立即阻断。

### 缺陷 #1:早熟 Service Interface 标准化

| 字段 | 内容 |
|------|------|
| **等级** | 🔴 最严重(架构) |
| **来源** | Oracle Round 1,缺陷 #1 |
| **描述** | 在 G1-G5 任意原型存在之前定义 DECLARE_SERVICE 形状,会让抽象在零证据下固化。Sprint 20 的 `LoopResult` BREAKING 已证明这种压力。 |
| **触发信号** | 任何 PR 在 G1-G5 任一 Agent 出现前引入 `DECLARE_SERVICE` 宏 → 阻断 |
| **预防措施** | v1 不引入任何新宏。ADR-0051 §导言显式写"Service 概念推迟形式化" |
| **失效后果** | 抽象在错误形状上锁死,所有后续 Agent 必须适配 |

### 缺陷 #2:逻辑隔离误当物理隔离(ADR-0020 leak)

| 字段 | 内容 |
|------|------|
| **等级** | 🔴 最严重(运行时安全) |
| **来源** | Oracle Round 1,缺陷 #2 |
| **描述** | ADR-0020"per-agent 线程隔离"在 single-process 内**只是线程级隔离**,对 cross-agent 失败传播(段错误、内存破坏、未捕获异常终止进程)**毫无防护**。 |
| **触发信号** | 任何 Agent 依赖"调用方 Agent 崩溃不影响我"假设 → 错误前提 |
| **预防措施** | ADR-0051 §不变量显式声明 "v1 隔离是逻辑隔离,非物理隔离。物理隔离推迟到 β 路径触发条件满足时" |
| **失效后果** | 5 团队基于错误假设设计容错,真实部署时第一波失败即暴露 |

### 缺陷 #3:Service 合约 Transport 泄漏(Phase 2 校准:适配 Spike 实际合约)

| 字段 | 内容 |
|------|------|
| **等级** | 🔴 最严重(API 演化) |
| **来源** | Oracle Round 1,缺陷 #3(经 Phase 2 校准) |
| **描述** | 若 v1 (Spike) Service 合约暴露任何 in-process 便利(`shared_ptr<Context>`、`IInteractionBus&`、裸引用、复杂嵌套类型),β 跨进程迁移时合约必须变更 → API 破坏性 → 5 个 Agent 全部重写。**Phase 2 校准后特别提醒**:Spike 合约 `std::unordered_map<std::string, std::string> args` 看似"简单",但开发者可能塞入序列化字符串(如 `"{\"key\": \"value\"}"`)试图表达嵌套结构——这在 in-process OK,跨进程序列化时因 string encoding 不一致而 break。 |
| **触发信号** | (i) 任何 Service endpoint 签名包含 `&` 或 `shared_ptr<>` 参数 → 阻断<br/>(ii) `args` 值是 JSON 序列化的字符串而非简单 key-value → 阻断 |
| **预防措施** | (a) 沿用现有 `IToolRegistry::call_tool()` 签名(`std::unordered_map<std::string, std::string> args` → `nlohmann::json result`,per ADR-0051 Q5);(b) args 值**仅**用简单 string 表示,禁止 JSON-in-string;(c) `call_tool_json()` overload 推迟到 Phase 6 正式实施(满足 ADR-0050 §启动条件 #2 之后);(d) 跨进程迁移时仅需替换 IToolRegistry 实现,合约签名不变。 |
| **失效后果** | β 迁移成为 API 破坏性变更,所有后续团队重写 |

### 缺陷 #4:战略门跳跃(Strategic Gate Jumping)

| 字段 | 内容 |
|------|------|
| **等级** | 🔴 最严重(过程) |
| **来源** | Oracle Round 2,缺陷 #4(NEW) |
| **描述** | 开 ADR-0051(实施)前未核验 ADR-0050 §启动条件 5 项硬前置。AGENTS.md 记 Stage Gate 推迟至 2026-07-18 且 3 PARTIAL/PENDING + 1 RISKY。 |
| **触发信号** | 任何 OpenSpec change `phase6-service-ification-v1` 创建前未完成附录 A 核验 |
| **预防措施** | **D0 30 分钟核验**:打开 ADR-0050 §启动条件,逐项打勾。3+ 满足则推进,<3 则 W1 改为关前置 |
| **失效后果** | 跳过战略门 = 过程缺陷,0051 在不稳定基础上推进 |

### 缺陷 #5:嵌套 ToolCoordinator 递归

| 字段 | 内容 |
|------|------|
| **等级** | 🟠 高(运行时) |
| **来源** | Oracle Round 2,缺陷 #5(NEW) |
| **描述** | G1 调 G3 经 ToolCoordinator(ADR-0031 §决策 5);若 G3 内部 Agent loop 再调需审批 tool → 嵌套审批/审计,潜在**无界递归**。 |
| **触发信号** | G3 MVP 代码中 `call_tool(...)` 调用且该 tool 需审批 → 阻断 |
| **预防措施** | G3 MVP scope **硬性禁止** G3 内部调需审批 tool;G3 内部只能调 MockLLMProvider(无 tool) |
| **失效后果** | ToolCoordinator 嵌套无限增长,栈溢出或无限循环 |

### 缺陷 #6:Error Flattening 静默缺陷

| 字段 | 内容 |
|------|------|
| **等级** | 🟡 中(demo 看不出的隐性缺陷) |
| **来源** | Oracle Round 2,缺陷 #6(NEW) |
| **描述** | tool-call wrapping(无论 DECLARE_TOOL 还是 `IToolRegistry::call_tool`)把内层 Agent-loop 错误藏在 tool-success payload 后:demo"工作"但错误语义错——G1 见"成功"call 带 error payload→不重试瞬态失败。 |
| **触发信号** | G3 tool handler 返 `{success: true, error: "..."}`(同一 return 中 success 但有 error) → 阻断 |
| **预防措施** | v1 合约**强制** error schema `{success: bool, error: string?}`(互斥);instrumentation 记 error-as-success 比例 |
| **失效后果** | v1→v2 必修项,v2 生产环境 break;但**非** v1 ship blocker |

### 5.7 缺陷触发 → 阻断自动化清单(待 v1 实施时落地)

| 缺陷 | 自动化检查位置 | 触发即阻断 |
|------|--------------|----------|
| #1 早熟标准化 | git diff 检查 DECLARE_SERVICE 宏引入 | ✅ |
| #2 ADR-0020 leak | ADR-0051 §不变量检查含 "逻辑隔离"字样 | ⚠️ 文档层 |
| #3 Transport 泄漏 | 所有 Service endpoint 签名检查 `&`/`shared_ptr` | ✅ |
| #4 战略门跳跃 | OpenSpec 创建 PR 检查附录 A 完成 | ✅ |
| #5 ToolCoordinator 嵌套 | static analysis,call_tool 调用图静态分析 | 🟡 需工具支持 |
| #6 Error schema 不规范 | tool handler 返回 schema lint | ✅ |

---

## 六、Phase 6 v1 实施计划(W1-W3)

### 6.1 时间线总览

```
  W1 (D1-5)                    W2 (D6-10)                  W3 (D11-15)
  ════════════                  ════════════                ════════════
  D1-2 [CP] OpenSpec change     D6 [CP] G1 脚手架           D11 [CP] ADR-0051 定稿
  D3 [CP] G3 脚手架             D7 [CP] G1 E2E 整合
  D4 [CP] G3 单轮路径            D8 [可压] Layer 2 监测
  D5 [CP] G3 session 隔离测试    D9 [可滑] 3 场景测试
                                D10 [可滑] Layer 3 memo
                                                          D12 [可压] tasks/specs
                                                          D13 [可滑] ctest 新增
                                                          D14 [CP] Ship gate
                                                          D15 [CP] ADR-0051 ✅

  [CP] = Critical Path(不容许 slip)
  [可压] = 可与 [可滑] 并行
  [可滑] = 可推迟到 W3 buffer

  ══════════════════════════════════════════════════════════════════════
  ⚠️  W1→W2 过渡有 BLOCKED GATE (per ADR-0051 §启动条件 + actual state)
  ══════════════════════════════════════════════════════════════════════

  W1 fix list 11/11 已 ✅ 完成 (commits c33b132, 0b993c6, 1f1d587, 8e90508)
  W2 启动条件 (per OpenSpec tasks.md §2 BLOCKED + ADR-0051 §启动条件):
    1. Stage Gate 2026-07-18 通过 (Risk V1-R2 解除)
    2. Sprint 23 启动 commitment 确认 (1.5 eng × 2 周)
    3. Oracle Q6 follow-up confirmation (Spike → Candidate B 提升标准评估)

  满足任一未达 → W2 不启动,W1 停留在 fix list 状态。
  本文档 §六 描述的"连续 3 周冲刺"为说明性场景,实际启动时以
  OpenSpec `phase6-service-ification-v1/tasks.md` 状态为准。
  ══════════════════════════════════════════════════════════════════════
```

### 6.2 W1 详细(D1-5)

| 日 | 任务 | 输出 | 关键路径? |
|---|---|---|:---:|
| **D1** | (a) 核验 ADR-0050 §启动条件 5 项(见附录 A) (b) 复核 OpenSpec change `phase6-service-ification-v1/`(已于 2026-07-14 创建 + 4 commits + W1 fix list 11/11 完成) (c) 起草 `proposal.md`,其中 §决策 含 ADR-0051 草稿 | OpenSpec 仓库齐全 | ✅ **D0+D1 不能 slip** |
| **D2** | 复核 `design.md`(G1+G3 合约草图)+ `tasks.md`(本时间线) + `specs/agent-composition-v1/spec.md`(G1/G3/pdk-service-composition 3 spec files) | OpenSpec artifacts 完成 | ✅ |
| **D3** | G3 Knowledge Base 脚手架:`pdk/g3_knowledge_base/` 目录 + `IToolRegistry::register_tool_function()` 注册(4-param,Sprint 15 C6 V2 签名;工具名 `knowledge_base/query` slash 命名 per ADR-0043)+ 内部 session store(`unordered_map<string, SessionState>`) + MockLLMProvider 接入 | `g3_knowledge_base.cpp` 编译通过 | ✅ |
| **D4** | G3 单轮路径通:call(input) → 硬编码 doc 检索(3-5 条) → MockLLMProvider → answer。返回 schema:`{success: bool, answer: string, error: string?}`(互斥:success + answer vs !success + error) | test_g3_singleshot 4-5 个 assertion PASS | ✅ |
| **D5** | G3 session 隔离测试:同 session_id 二次调用 + 不同 session_id 隔离 + 错误传播(显式 error schema) | test_g3_session 6-8 个 assertion PASS | ✅ |

### 6.3 W2 详细(D6-10)

| 日 | 任务 | 输出 | 关键路径? |
|---|---|---|:---:|
| **D6** | G1 Coding Assistant 脚手架:`pdk/g1_coding_assistant/` + DEFINE_AGENT(React,Sprint 20)+ 注册 1 个 tool(`knowledge_base/query`) | `g1_coding_assistant.cpp` 编译通过 | ✅ |
| **D7** | G1 ReAct → call `knowledge_base/query` → 收到 G3 返回 → 综合 final review comment。E2E 跑通(用 mock code input) | test_g1_e2e 5-7 个 assertion PASS | ✅ **最关键** |
| **D8** | Awkward 模式监测(Layer 2):**复用现有 `tool.audit.{invoked,completed,denied}` 事件**(per ADR-0051 Q4);G3 内部 metrics 通过 audit event self-report,不修改 ToolRegistry::call_tool() | audit event payload 字段定义 + 测试验证 | 可压 |
| **D9** | 跑 3 个场景测试:(i) 单轮 G3 (ii) 多轮 G3 + G1 调用 (iii) G1 调用 G3 + G3 错误传播 | 3 个 catch2 TEST_CASE,各 3-5 assertion | 可滑 |
| **D10** | 工程师写 Layer 3 memo("什么感觉不对")+ ADR-0051 §观察 更新(记录实际看到的 awkward patterns) | `docs/phase6/awkward-patterns-found.md` + ADR-0051 §观察 | 可滑 |

### 6.4 W3 详细(D11-15)

| 日 | 任务 | 输出 | 关键路径? |
|---|---|---|:---:|
| **D11** | ADR-0051 定稿:§决策 + §观察 + §触发条件 + §不变量 + §未来 ADR 候选 (causal trace / cancellation / cycle / session inheritance / approval inheritance 等) | `docs/adr/adr-0051-phase6-pdk-composition-spike.md`(本计划下 ADR-0051 已是该路径的 1.0 版本,W3 D11 翻 ✅ Approved) | ✅ |
| **D12** | OpenSpec `tasks.md` 全勾 + `specs/agent-composition-v1/spec.md` §验收 全部满足 | OpenSpec artifacts 完成 | 可压 |
| **D13** | `tests/test_service_v1.cpp`(集成测试:G1-calls-G3 多次 / session 隔离 / 错误传播 flatten 测试 / instrumental verification) | ctest 新增测试 PASS | 可滑 |
| **D14** | Ship gate:ctest 全部 PASS + openspec validate exit 0 + sprint-closeout.sh 绿 | ships status 文档 | ✅ |
| **D15** | ADR-0051 状态 🔍 Proposed → ✅ Approved(若 ship gate 通过)+ OpenSpec change archive | Phase 6 v1 complete | ✅ |

### 6.5 关键路径总结

**不容许 slip**:D1-2 → D3-5 → D6-7 → D11 → D14-15
**可压/可滑**:D8-10(并行)+ D12-13(W3 buffer)
**2 周可达 min ship**:若 D8-10 全压 + W3 不介入 buffer → 2 周足够

### 6.6 人员配置(bus factor 缓解)

| 角色 | 工作量 | 主要负责 |
|------|:---:|---------|
| **Primary**(1 人 FT) | 100% | G3 + integration + OpenSpec authoring + ADR-0051 起草 + Layer 3 memo |
| **Reviewer**(1 人 50%) | 50% | G1 + review G3 + co-author ADR-0051 §决策 + 独立 Layer 3 memo |

**为什么 1.5 而不是 1 或 2**:
- 1 不可接受:v1 合约 5 团队依赖,单点失败
- 2 FT 风险 over-engineering(两人各自发明抽象)
- 1.5 = primary 进度 + reviewer 保险

**Reviewer 必须不同人**:为 code review 多样性 + 后续 G2/G4/G5 onboard 时 2 人答疑

### 6.7 工具与依赖

| 依赖 | 状态 | 备注 |
|------|------|------|
| `DECLARE_TOOL` 宏(Sprint 4, C6 V2 4-param) | ✅ 已 ship 但 ⚠️ **deprecated for PDK use** | ADR-0051 Q1:宏 `##name` token-pasting 不支持 `.` / `/` 等非 C++ 标识符;**所有现有 PDK 插件 100% 绕过 DECLARE_TOOL**,改用 `IToolRegistry::register_tool_function()` |
| `DEFINE_AGENT` + `ReactLoop`(Sprint 20) | ✅ 已 ship | G1 直接用 |
| `MockLLMProvider`(Sprint 19) | ✅ 已 ship | G1/G3 都用,无需真实 LLM |
| `IInteractionBus`(ADR-0019) | ✅ 已 ship | causal trace 传播 |
| `IToolRegistry`(Sprint 18) | ✅ 已 ship | tool 调用路径 |
| `IApprovalHandler`(Sprint 18 P1.T2) | ✅ 已 ship | 审批链继承 |
| `TopoScheduler` + DAG | ✅ 已 ship | in-process 调度已就绪 |
| `LayeredContext`(ADR-0008,Sprint 20) | ✅ 已 ship | session context 传递 |
| **新增**:任何 Service Composition 抽象 | 🔴 不引入 | Round 1 锁定:v1 不新增任何宏 |

---

## 七、G1+G3 最小可行定义(MVP Scope)

> **必读**:此节是 G1+G3 demo 的**唯一真理**。任何偏离须更新本节并在 ADR-0051 §观察 文档化。

### 7.1 G3 Knowledge Base

| 维度 | 定义 |
|------|------|
| **Plugin 名称** | `g3_knowledge_base` |
| **路径** | `pdk/g3_knowledge_base/` |
| **tool 暴露** | `knowledge_base/query` (via `IToolRegistry::register_tool_function()`,slash 命名 per ADR-0043;⚠️ 不使用 DECLARE_TOOL 宏 — Oracle Q1 决议因 `##name` token-pasting 不支持 `.` / `/`) |
| **输入 schema** | `{question: string, session_id: string}` |
| **返回 schema** | `{success: bool, answer: string, error: string?}`(互斥:success + answer vs !success + error) |
| **行为 - 新 session_id** | (1) 硬编码检索 3-5 条 doc 片段(写在 `pdk/g3_knowledge_base/docs/corpus_seed.json`)<br/>(2) MockLLMProvider 调用(question + context)<br/>(3) 返回 answer + 内部存 `{last_question, last_answer}` |
| **行为 - 同 session_id** | (1) 查 session store → 拿到 last_q/a<br/>(2) MockLLMProvider 调用(question + context + last_q/a)<br/>(3) 返回 follow-up + 更新 store |
| **行为 - 不同 session_id** | (1) session 隔离 → 独立 store → 独立上下文 |
| **scope 边界** | ❌ 无真实检索<br/>❌ 无向量库<br/>❌ 无多文档索引<br/>❌ 无并发写 store 优化 |
| **tool handler 代码预算** | ≤30 行(session lookup + MockLLM call + answer 组装) |
| **Mock LLM 行为** | MockLLMProvider 已 ship(Sprint 19)。Mock response:基于 question 长度 + 上文长度生成"hash 化"确定性回答,不需外部模型 |
| **测试** | `tests/test_g3_knowledge_base.cpp` ≥3 TEST_CASE,5+ assertion |

### 7.2 G1 Coding Assistant

| 维度 | 定义 |
|------|------|
| **Plugin 名称** | `g1_coding_assistant` |
| **路径** | `pdk/g1_coding_assistant/` |
| **agent loop** | `DEFINE_AGENT(React)`(Sprint 20) |
| **注册 tool** | 1 个(`knowledge_base/query` → G3) |
| **输入 schema** | `{request: string, code: string}` |
| **返回 schema** | `ToolResult{success, review_comment, raw_context?}` (DEFINE_AGENT 适配) |
| **行为** | 2 步 ReAct:<br/>Step 1: 调 `knowledge_base/query`,question="风格指南对 X 怎么说?"<br/>Step 2: 用 G3 返回 + code → 综合 final review comment |
| **scope 边界** | ❌ 无真实代码解析(把 code 当字符串)<br/>❌ 无真实 review 逻辑(synthesis trivial)<br/>❌ 无 LLM-based 代码理解(Mock 即可) |
| **ReAct 控制** | 用 Sprint 4 ReactLoop(threshold N=2 由 hardcode),不允许超过 2 步 |
| **测试** | `tests/test_g1_coding_assistant.cpp` ≥2 TEST_CASE,3+ assertion |

### 7.3 G1 ⇄ G3 集成测试矩阵

| 场景 | 输入 | 期望 | 测谁 |
|------|------|------|------|
| 单轮 G3 | `{question: "什么是 X?", session_id: "S1"}` | G3 返 answer(success=true) | test_g3_singleshot |
| 多轮 G3 同 session | 第 1 次 + 第 2 次(同 session_id) | 第 2 次 answer 含上轮上下文 | test_g3_session |
| session 隔离 | 2 个 session_id 独立调用 | 各自 store 不交叉 | test_g3_session |
| G1 调 G3 | `{request: "review", code: "x = 1"}` | G1 ReAct → G3 → 综合 review | test_g1_e2e |
| G3 错误传播 | G3 内部 LLM 失败 | G3 返 `success=false, error="..."`,G1 ReAct 捕获后返 ToolResult.error | test_g1_error |
| causal trace | 任一调用链 | IInteractionBus event 含 parent_agent_id + parent_session_id | test_causal_trace |

### 7.4 必须 NOT 实现的特性(防 scope creep)

- ❌ 真实 LLM 接入(v1 用 MockLLMProvider 足够)
- ❌ 真实文档检索 / 向量库(v1 硬编码)
- ❌ 真实代码分析(G1 把 code 当字符串)
- ❌ 流式输出(G5 才需要,v1 不实现)
- ❌ 并发访问优化(v1 单线程足够)
- ❌ 持久化 session(G3 session 在内存,v1 进程重启即失)
- ❌ 多用户会话隔离(UserSession 由 ADR-0033 提供,v1 不再额外实现)

---

## 八、Awkward 模式检测方法学

### 8.1 方法论总览

```
  3 层检测:
  Layer 1: 静态代码 review checklist  (实施时立即)
  Layer 2: 运行时 instrumentation       (跑 demo 时自动)
  Layer 3: demo 后工程师反思 memo         (D10 写)
```

### 8.2 Layer 1 — 静态 checklist(5 项)

每完成一个 tool handler / `IToolRegistry::register_tool_function()` 调用点,工程师过清单:

| # | 检查项 | Awkward 模式类别 |
|---|--------|-----------------|
| 1 | tool handler 跨调用维护状态? | **#1 stateful tool** |
| 2 | tool handler 内部跑 Agent 循环? | **#2 nested agent behind tool** |
| 3 | caller 需在 args 传 session/context 而 tool 接口天然不携带? | **#3 context threading via args** |
| 4 | 内层 Agent 错误到外层 caller 信息丢失? | **#4 error flattening** |
| 5 | tool 需流式 token 但接口是 sync? | **#5 sync-async impedance** |

### 8.3 Layer 2 — 运行时 instrumentation(5 字段)

每次 `call_tool` 记结构化日志:

```cpp
// ⚠️ Per ADR-0051 Q4 决议:不复用新增 call_log,改复用现有 tool.audit.* 事件
// ADR-0051 Q4: "Layer 2 instrumentation: 复用现有 tool.audit.{invoked,completed,denied}
// 事件(C4 ship);Plugin-internal metrics: G3 内部 metrics (session store 大小、error-as-success
// 比例) 通过 audit event self-report,不走核心代码"
//
// 推荐实现: 在 G3 tool handler 中:
//   1. emit("tool.audit.invoked", {caller_session_id, callee_tool_name, args_keys, ...});
//   2. 处理后 emit("tool.audit.completed", {latency_ms, callee_invoked_llm, ...});
//   3. 通过 audit event listener(独立 module)聚合,作为 Layer 2 数据源
//
// C4 ship 的 audit event payload 字段定义见 ADR-0031 §决策 5
```

**post-demo 分析**:
- 多少 call 在 args 传 session_id?(→ 模式 #3)
- 多少 tool 内部调 LLM?(→ 模式 #2)
- 多少 error-as-success 比例?(→ 缺陷 #6)
- 单 tool handler 有多少行"session lookup + Agent loop"?(→ 模式 #1)

**输出**:客观数字,而非"感觉别扭"。

### 8.4 Layer 3 — 工程师反思 memo(D10)

**格式**:1 页 markdown,模板见下。

```markdown
# Phase 6 v1 Awkward Patterns Memo

**作者**:
**日期**:
**范围**: G1+G3 demo 观察

## 1. 写的样板代码(本该是框架)
- [ ] G3 tool handler 有 多少行 boilerplate?具体在哪?
- [ ] G1 调用 G3 时手写了什么 wrapper?

## 2. 信息丢失
- [ ] G3 内部 LLM 错误到 G1 经过了几层转换?丢失了什么?

## 3. 反复手写的模式
- [ ] 2+ 个不同调用点做了相同的事?

## 4. 决策树被打断
- [ ] 哪些本该自动的(继承/审批/审计)在 demo 中要手写?

## 5. 结论
- [ ] 演示暴露的 awkward 模式清单(标签 #1-#5)
- [ ] 我的主观判断:这是 missing abstraction 还是 acceptable boilerplate?
```

### 8.5 触发阈值(精炼版)

| 触发类型 | 条件 | 行动 |
|---------|------|------|
| **强制触发** DECLARE_SERVICE 设计探索(非实现) | #1/#2/#3 任一 + instrumentation 证据 | 启动新 ADR-0051+ |
| **强触发** 形式化(必须) | 2+ **不同类别**(如 #1+#5) | DECLARE_SERVICE 形式化 justified |
| **弱信号** 仅记录 | #4 或 #5 单独,无其他 | 记入 ADR-0051 §未来 |
| **不触发** | 零模式涌现 | 推迟 DECLARE_SERVICE 无限期 |

**同类别不叠加**:2 次同模式 = 1 个 missing abstraction,非 2 个。

### 8.6 Awkward 模式标签词典

| 标签 | 名称 | 典型症状 |
|------|------|---------|
| **#1** | stateful tool | tool handler 跨调用维护 `unordered_map<session_id, ...>` |
| **#2** | nested agent behind tool | tool handler 内 `while (true) { llm.Generate(); }` |
| **#3** | context threading via args | caller `call_tool("x", {session_id: "...", trace_id: "...", ...args})` |
| **#4** | error flattening | 内部 exception → 捕获 → 返 `{success: true, error: "..."}` |
| **#5** | sync-async impedance | tool 返回 `string` 但实际是 `stream<string>`,syntactic mismatch |

---

## 九、ADR 影响与命名约定

### 9.1 ADR 影响力矩阵

| 操作 | 数量 | 详情 |
|------|:---:|------|
| 新建 ADR | **1** | ADR-0051 Service Composition v1 |
| 修改现有 ADR | **0** | v1 不修订任何已 Approved ADR |
| 推迟现有 ADR | **0** | - |
| **预期后续** ADR-0052+ | 不定 | 涌现真实失败后追加,**非预先** |

### 9.2 ADR-0051 草稿大纲(待 W3 D11 定稿)

```
  ADR-0051: Phase 6 Service Composition v1
  ────────────────────────────────────────

  ## 状态
  🔍 Proposed (2026-07-XX) → ship 后 ✅ Approved

  ## 领域
  / Phase 6 实施 / 服务化契约层

  ## 关联
  - ADR-0019 (IInteractionBus)  // 约束
  - ADR-0020 (Thread Isolation)  // 约束,**显式声明逻辑隔离**
  - ADR-0021 (PDK Design)       // 约束
  - ADR-0022 (Plugin Loading)    // 约束
  - ADR-0023 (ToolResult)        // 约束
  - ADR-0031 (Execution Policy)  // 约束
  - ADR-0033 (Session)           // 约束
  - ADR-0034 (IModelRouter)      // 参考 Service 模板
  - ADR-0050 (Phase 6 战略)      // 战略层引用

  ## §背景
  (Phase 6 重塑时刻描述)

  ## §决策
  ### 范围
  - in-process (single-process α path)
  - `unordered_map<string,string>` args → `nlohmann::json` result(per ADR-0051 Q5;不声称 JSON-in/JSON-out)
  - DECLARE_TOOL-based(无新宏)
  - G1+G3 in-process composition

  ### 不变量
  - Transport-agnostic 合约(详缺陷 #3)
  - 逻辑隔离非物理(详缺陷 #2)
  - G3 内部不调审批 tool(详缺陷 #5)
  - Error schema 强制 `{success, answer/error}`(详缺陷 #6)
  - Session 继承:UserSession 继承,TaskSession 新建

  ## §观察
  (由 §八 Layer 1-3 检测到的 awkward patterns 列表)

  ## §触发条件
  - DECLARE_SERVICE 形式化触发(Q2 §8.5)
  - β 跨进程触发(见附录 C)

  ## §未来 ADR 候选
  (涌现真实失败后追加,**非预先**)
  - ADR-0052+ Cross-Agent Causal Trace Propagation
  - ADR-0052+ Cancellation Token Plumbing
  - ADR-0052+ Runtime Cycle Detection
  - ADR-0052+ Session Inheritance Clarification(v1 暴露后)
  - ADR-0052+ Approval Inheritance Rule

  ## §估时
  (W1-W3 详细,见本文档 §六)
```

### 9.3 命名约定

| 决策 | 选择 | 否决 |
|------|------|------|
| ADR 标题 | **"Service Composition v1"** | ❌ Interface(C++ 抽象类名碰撞)<br/>❌ Orchestration(已有 SimpleCognitiveOrchestrator) |
| v1 (Spike) namespace | **无新 namespace**(复用 `IToolRegistry`) | ❌ 过早命名 |
| v2 预留 namespace | `agenticdsl::service` | (DECLARE_SERVICE 落地时引入) |
| Tool 命名 | **slash-only** per ADR-0043,`domain/subdomain/action`(`knowledge_base/query`) | ❌ 点号 / 单层 snake_case(Oracle Q1 决议因 DECLARE_TOOL 宏 `##name` 不支持非标识符字符) |
| Plugin 目录 | `pdk/<snake_case>/`(`pdk/g3_knowledge_base/`) | ❌ camelCase / kebab-case |

### 9.4 ADR 与 OpenSpec / Plan 文档的关系

```
  ┌────────────────┐   ┌─────────────────┐   ┌─────────────────┐
  │  ADR-0050      │   │  ADR-0051       │   │  OpenSpec       │
  │  Phase 6 战略   │   │  Service Comp   │   │  phase6-        │
  │  (战略层)       │   │  v1 (实施层)    │   │  service-       │
  │                │   │                 │   │  ification-v1   │
  │ ✅ Approved    │   │ 🔍 Proposed    │   │  (4 artifacts)  │
  │    (待完成)     │   │   (本计划补完)  │   │  proposal/      │
  │                │   │                 │   │  design/tasks/  │
  │                │   │                 │   │  specs          │
  └────────────────┘   └─────────────────┘   └─────────────────┘
                                                     │
                                                     ▼
                                            ┌─────────────────┐
                                            │  本文            │
                                            │  docs/          │
                                            │  superpowers/   │
                                            │  plans/         │
                                            │  2026-07-14-    │
                                            │  phase6-        │
                                            │  agent-os-      │
                                            │  reference.md   │
                                            └─────────────────┘
                                                     │
                                                     ▼
                                            Phase 6 v1 archive
```

---

## 十、Kill Criterion(终止判据)

### 10.1 4 类 kill 准则

| 类型 | 触发条件 | 应对动作 |
|------|---------|---------|
| **HARD KILL** | 任一触发:<br/>(a) crash 传播(G3 崩 → G1 崩)<br/>(b) ToolCoordinator 无界嵌套<br/>(c) cycle 检测(G1→G3→G1)<br/>(d) W2 D10 末零 E2E call(premise 破) | **abort v1 + escalate ADR-0050**(战略发现,Candidate B 可能错) |
| **SOFT KILL** | 2+ 不同类别 awkward 模式 + 无 DECLARE_SERVICE 方向 | pause 2 天 + Oracle round 3;无解→HARD |
| **DRIFT KILL** | W3 末无收敛 | 写 "what we learned" doc,**不延 W4**(防沉没成本) |
| **NOT KILL** | error flattening(记 defer v2) / session store 增长(cleanup 修) / F2 state leak 1 天可修(= bug) | 继续推进 |

### 10.2 回退计划(若 v1 失败)

| 回滚对象 | 操作 | 影响 |
|---------|------|------|
| 代码 | `git rm -r pdk/g1_coding_assistant/ pdk/g3_knowledge_base/` + 从 CMakeLists 移除 | 零影响其余 codebase(目录隔离) |
| 测试 | `git rm tests/test_service_v1.cpp tests/test_g1_*.cpp tests/test_g3_*.cpp` | 零影响其余测试 |
| 文档 | ADR-0051 保持 🔍 Proposed(不翻 Approved) | OpenSpec change 标 abandoned + findings |
| 战略 | findings 反馈 ADR-0050——Candidate B v1 失败 = 战略层重新评估 | **战略升级**(非工程失败) |

### 10.3 触发信号检测方式

| 信号 | 检测方式 |
|------|---------|
| crash 传播 | ctest 跑通但 OS-level signal handler 触发(SIGSEGV/SIGABRT) |
| ToolCoordinator 嵌套 | static analysis:`call_tool` 调用图深度 > 2 |
| cycle G1↔G3 | instrumentation:trace_event 包含已访问 node_id |
| zero E2E by W2 D10 | 自动化检查:`tests/test_g1_e2e.cpp` 未 PASS → W2 D10 日报 |

---

## 十一、决策点状态(D0/D1/D2/D3)

### 11.1 决策汇总表

| ID | 决策 | 推荐 | 状态 |
|----|------|------|------|
| **D0** | 核验 ADR-0050 §启动条件 5 项 | **必须** | 🔴 未核验 |
| **D1** | ADR-0051 起草时机 | (b) OpenSpec change 先建,ADR-0051 在 §决策 | 🔵 已推荐 |
| **D2** | G1+G3 领域边界 | (a)+(b) Oracle 推荐张力最大化 MVP,用户校验 | 🔵 已推荐 |
| **D3** | 5 团队 kickoff 时机 | (c) v1 ship 后再 kickoff,0051 §后续含 onboarding 种子 | 🔵 已推荐 |

### 11.2 决策点详细

#### D0:启动条件核验(🔴 BLOCKER)

```
  详见 附录 A 核验清单
  决策: 3+ met → 推进 / <3 → W1 改为关前置
```

#### D1:ADR-0051 起草时机

| 选项 | 推荐度 | 理由 |
|------|:---:|------|
| (a) 立即起草 ADR-0051 单独文件 | 🟡 | 脱离实施 tracking,触发 docs_drift_audit |
| **(b)** OpenSpec change 先,ADR-0051 在 §决策 | ✅ | 保持 🔍 活态,项目 rhythm,C0-C16 全是这种 |
| (c) 代码先,文档后 | 🔴 | 失去决策纪律,post-hoc 合理化 |

#### D2:G1+G3 领域边界

| 选项 | 推荐度 | 理由 |
|------|:---:|------|
| (a) Oracle 推荐 | 🟡 | 不知业务上下文,可能脱节 |
| **(a)+(b) 混合** | ✅ | Oracle 推荐"张力最大化 MVP",用户校验 |
| (b) 用户定义 | 🟡 | "团队还没开始",卡住 |
| (c) 抽象占位 | 🔴 | stateless demo 证明不了 awkward,Round 1 defect #1 重现 |

#### D3:Kickoff 方式

| 选项 | 推荐度 | 理由 |
|------|:---:|------|
| (a) 用户自负责 + ADR 文档 only | 🟡 | ship 后 onboarding 临时 |
| (b) OpenSpec 模板自带 checklist | 🔴 | 过早正式化 onboarding,组织动能抗拒合约修改 |
| **(c)** 不 kickoff + ADR-0051 §后续 onboarding 种子 | ✅ | 防止 trust erosion,等 v1 validated 再教学 |

### 11.3 决策时间线

```
  2026-07-14 (今日)         →  本文完成
  2026-07-XX (D0,30 min)    →  用户核验 ADR-0050 §启动条件
  2026-07-XX (W1 D1-2)      →  OpenSpec change 创建 + ADR-0051 §决策草稿
  2026-07-XX (W3 D11-15)    →  ADR-0051 ✅ Approved + OpenSpec archive
  2026-07-18 (Stage Gate)   →  ADR-0050 §启动条件 #5 重新评估
  2026-07-XX (W3 末)        →  5 团队 kickoff(若 D3 = c)
  2026-08-XX (估时)         →  G2/G4/G5 启动
```

---

## 十二、Onboarding 与团队扩展(Phase 2 校准:指向 ADR-0051 + spike-onboarding.md)

### 12.1 Onboarding 时机

D3 决策 = **(c)** Spike ship 后再做 kickoff(经 Phase 2 校准后措辞更新)。因此:
- **不要在 W1-W3 做 5 团队 kickoff**——会过早承诺合约
- **W3 末 Spike ship 后做 1 次 kickoff**——合约 validated,再教学
- **G2/G4/G5 团队启动顺序**:Spike ship + Phase 6 v2 决议通过后(参 §十三 + ADR-0051 §5 提升标准)

### 12.2 Onboarding 文档唯一入口

**Phase 2 校准决议**:`docs/service-composition/spike-onboarding.md` 是**唯一入口文档**(Per Oracle Round 3 §单一真理源)。本文档 §十二 仅作**存在性提示**,不再内联完整 onboarding 内容(避免与 spike-onboarding.md drift)。

**创建时机**:OpenSpec tasks.md §8 输出(预计 W2 D5)创建 `docs/service-composition/spike-onboarding.md`。

**推荐骨架**(将由 OpenSpec W2 D5 实际产出):

```markdown
# PDK Composition Spike — Onboarding Guide

## 1. Spike v1 是什么
[in-process / `unordered_map<string,string>` args → `nlohmann::json` result /
 IToolRegistry::register_tool_function()-based / 无新宏 / slash 工具名 per ADR-0043]

## 2. Spike v1 不是什么
[非网络 / 非异步 / 非流式 / 非多租户 / 非兑现 ADR-0050 Candidate B 战略目标]

## 3. Spike 合约规范
- Tool 名格式: domain/subdomain/action (slash-only per ADR-0043)
- args schema: `std::unordered_map<std::string, std::string>` (per ADR-0051 Q5)
- return schema: `nlohmann::json` (parsed by caller)
- error schema: `{success: bool, answer/error: ...}` 互斥

## 4. 决策树:你的 Agent 适配 Spike 吗?
(见本参考文档 附录 D,Phase 2 校准后更新根节点为 register_tool_function)

## 5. 何时推 DECLARE_SERVICE 形式化
(见本参考文档 §八.5 触发阈值 + ADR-0051 §5 Spike→Candidate B 5 项提升标准)

## 6. 参考实现
- G1: pdk/g1_coding_assistant/
- G3: pdk/g3_knowledge_base/
- Llama Engine 模板: pdk/llama_engine/(C14 ship)
- Model Router 模板: pdk/model_router/(C7 ship)
```

### 12.3 团队扩展实施模板(Phase 2 校准:对齐 ADR-0051 路径)

每个新 Agent(从 G2 开始)走同样流程(Phase 2 校准:路径用 `phase6-<agent>-spike` 而非 `v1`):

```
  1. 读 docs/service-composition/spike-onboarding.md(W2 D5 产出) + 本文档 §四
  2. 决策树评估(附录 D)
  3. 若适配 Spike:
     - 创建 OpenSpec change `phase6-<agent>-spike/`
     - 写 proposal.md(含 §决策 引用 ADR-0051)
     - 在 `pdk/<agent_name>/` 实现
     - 加测试,跑 ctest
     - ship + archive
  4. 若不适配 Spike(需流式/多租户/24-7):
     - 启动 ADR-0052+ 评估
     - 推到 Phase 6 v2 或 Phase 7+
```

### 12.4 决策树参考(Phase 2 校准:根节点已更新)

完整决策树见 **附录 D**(Phase 2 校准后,根节点为 `IToolRegistry::register_tool_function()` 而非 DECLARE_TOOL)。

---

## 十三、未来演进路径

### 13.1 阶段化演进

```
  Phase 6 v1        Phase 6 v2          Phase 7+
  (1.5-3 周)        (4-6 周,估时)        (估时不定)
  ════════════      ═══════════════════  ═══════════════

  G1+G3 演示        DECLARE_SERVICE      多服务编排
  §八 methodology   形式化(若触发)        跨服务事务
  ADR-0051 ship     ADR-0052+ 涌现        服务发现 / 监控
                   β 跨进程(若触发)       OpenAPI Bridge
                                         MCP Bridge(?)
```

### 13.2 Phase 6 v2 触发条件(任一)

| 条件 | 行动 |
|------|------|
| 2+ 不同类别 awkward 模式 + DECLARE_SERVICE 设计方向清晰 | 启动形式化 |
| 单进程 >32GB(5 Agent 全 ship 后) | 启动 β 跨进程 |
| 出现 24-7 后台 Agent 需求 | 启动 β |
| 出现多主机部署需求 | 启动 β |
| 任意 Agent crash 致其他 Agent 不可用 | 启动 β |

### 13.3 Phase 7+ 候选(不在 Phase 6 范围)

- 多服务编排(workflow engine 之上)
- 服务发现(动态 service registry 而非 manifest)
- OpenAI-compatible bridge(若外部 LLM 客户端需求涌现)
- MCP bridge(若内部团队用 Claude Desktop)
- 跨服务事务(transaction coordinator)
- 服务监控 / SLO 定义
- 流量控制 / rate limiting(per-service)

### 13.4 长期愿景(Strategic North Star)

> "所有未来的'软件'都通过 PDK 插件的形式来实现。"
>
> 最终 HydraForge = 整个内容团队的 Agent OS。所有新软件 = 新 PDK plugin。所有 plugin = Agent。所有 Agent 间通过 Service Composition 互相调用。

这条路径一旦建立,HydraForge 成为内容团队的**基础设施**而非**工具**。

---

## 附录 A:5 项启动条件核验清单

> **D0 必做项**(约 30 分钟,详 oracle Defect #4)

```markdown
## Phase 6 启动条件核验 — D0 Checklist

日期: _____
检查人: _____

参考: docs/adr/adr-0050-phase6-strategic-evaluation.md §113

### 5 项硬前置逐项打勾

- [ ] **1. Phase 5 完全关闭**
  - [ ] C18 OpenSpec change 已 archive
  - [ ] active OpenSpec changes = 0
  - 验证: `openspec list --json | jq '.changes | length'` == 0
  - 状态: ✅ MET / 🔴 NOT MET / 🟡 PARTIAL

- [ ] **2. 服务化范围文档批准**
  - [ ] in-scope 明确:MCP server + /v1/chat/completions + /v1/models
  - [ ] out-of-scope 明确:cloud deployment → Candidate D follow-up
  - 验证: 文档是否存在于 docs/adr/adr-0050 §启动条件 #2 描述?
  - 状态: ✅ MET / 🔴 NOT MET / 🟡 PARTIAL(需要补)

- [ ] **3. C20 placeholder 决议**
  - [ ] C20 是 activation 还是 defer?
  - 验证: ADR-0050 §C19/C20 决策章节
  - 状态: ✅ MET(activated) / 🔴 NOT MET / 🟡 DEFER

- [ ] **4. 团队容量确认**
  - [ ] 1-2 工程师 4-6 周无中断可用?
  - 验证: 需要 owner 签署
  - 状态: ✅ MET / 🔴 NOT MET

- [ ] **5. ≥1 个具体集成目标**
  - [ ] 至少 1 个 agent/tool 会消费 MCP/OpenAI API
  - 验证: 当前已锁定 G1+G3
  - 状态: ✅ MET / 🔴 NOT MET

### 决策

| 满足数 | 行动 |
|:---:|------|
| ≥3 | 推进 OpenSpec change `phase6-service-ification-v1` |
| <3 | W1 改为关闭前置(不推进 v1) |
```

---

## 附录 B:Awkward 模式触发阈值

```
┌──────────────────────────────────────────────────────────────────┐
│  ⚙️ 触发决策树(基于 §八方法论)                                     │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Q1: G1+G3 demo 跑完后,awkward patterns 有哪些?                     │
│  Q2: 每类有 instrumentation 证据?                                   │
│  Q3: 类别数?                                                     │
│                                                                  │
│  ┌────────────────────────────────────────────────┐              │
│  │  0 类别    →  不触发 DECLARE_SERVICE           │              │
│  │  1 类别 + 证据 →  设计探索(非形式化)            │              │
│  │  1 类别 无证据 → 记录 + 跳过                   │              │
│  │  2+ 不同类别 + 证据 → ⭐ 形式化 justified     │              │
│  │  2+ 同类别 → 1 个 missing abstraction(不立即形式化)│           │
│  └────────────────────────────────────────────────┘              │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘

同类别不叠加的例:
  发现 2 次 #1 stateful tool = 1 个抽象缺失,记入 backlog
  发现 #1+#5 不同类别 = 抽象层真实不足,启动 DECLARE_SERVICE 设计

---

## 📌 Phase 2 校准:Spike → Candidate B 5 项提升标准(2026-07-14 追加)

> **本节是 Phase 2 校准的核心补充**。原 §八.5 触发阈值仅描述"何时形式化 DECLARE_SERVICE",**未包含** Spike 提升到 Phase 6 Candidate B v1 的完整门槛。Oracle Round 3 决议 + ADR-0051 §5 已确立 5 项硬提升标准,本节补全。

```
┌──────────────────────────────────────────────────────────────────────┐
│  🎯  ADR-0051 §5 Spike → Candidate B 提升标准                          │
│     (5 项全部满足 → 才可提议 ADR-0052 启动 Phase 6 Candidate B v1)   │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  1. ≥3 awkward patterns 从 ≥2 不同 Layer 1 类别观察到                 │
│     ↳ 不只是数量,必须跨类别(同类别不叠加,见本附录决策树)            │
│     ↳ 证据来源:Layer 1 静态 checklist + Layer 2 audit event 监测    │
│                                                                      │
│  2. Layer 1 reviewer agreement: ≥2 reviewers 独立识别                │
│     ↳ 不是单个工程师的判断,需 ≥2 位独立 reviewer 共识              │
│     ↳ 避免单点主观偏差                                                │
│                                                                      │
│  3. Layer 3 dual memos convergence: primary + reviewer 在 ≥1 major    │
│     awkward pattern 上达成共识                                       │
│     ↳ Layer 3 memo(1 页 "什么感觉不对")primary + reviewer 各 1 份   │
│     ↳ 至少在 1 个主要 awkward pattern 上收敛                          │
│                                                                      │
│  4. Oracle round 4 确认内部 Spike 证据支持"外部 agent/tool"需求     │
│     ↳ Spike 是内部组合,Phase 6 正式 v1 需外部目标(ADR-0050 §启动条件 #5)│
│     ↳ Oracle round 4 评估:内部 Spike 数据能否证明外部需求?         │
│                                                                      │
│  5. ADR-0050 §启动条件 #2/#4/#5 重新评估通过                          │
│     ↳ #2 服务化范围文档批准(若适用)                                   │
│     ↳ #4 团队容量确认                                                  │
│     ↳ #5 ≥1 个具体外部集成目标                                        │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

**与本附录原决策树的关系**:
- 原决策树(2+ 不同类别 → 形式化)→ **升级为** 5 项标准中的 #1 + 强证据要求(#2/#3)
- 原决策树无 Oracle round 验证 → **新增** #4
- 原决策树无 ADR-0050 条件 re-eval → **新增** #5
- 即原决策树是"形式化触发条件"(局部),新 5 项是"Spike 提升条件"(全局)

**推荐触发顺序**:
1. Spike ship + 2 周 production-like 监控(ADR-0051 §后续 7)
2. 收集 Layer 1-3 证据 + 写 dual memos
3. 自我评估 5 项标准,≥4 项满足 → 启动 Oracle round 4
4. Oracle round 4 通过 → 提议 ADR-0052
5. ADR-0052 ship → Phase 6 Candidate B v1 正式启动

**W2 D5 前用户确认事项**(per ADR-0051 §后续 3):Layer 3 memo 模板 5 个固定 section 需用户拍板。

---

## 附录 C:Escalation Triggers 矩阵

| Round 1 触发 | Round 2 调整 | v1 是否适用 |
|---|---|:---:|
| 2+ awkward 模式 | **精炼**: 2+ **不同类别** + instrumentation 证据(同类别 ×2 不计) | ✅ |
| crash 传播 | 保留 + **新增**: nested-ToolCoordinator 深度 > 2 | ✅ |
| >32GB mem | **澄清**: 指 G3 session store 跨 caller 无界;**新增**: >1K sessions | ✅ |
| 24-7 background | 保留(v2 关注) | 🔴 |
| cycle stack overflow | **明确**: G1→G3→G1 检出 = **立即 HARD KILL** | ✅ |
| >10K events/sec | **降级**: v1 in-process 无事件总线,移 v2 trigger | 🔴 |
| **新增:ToolCoordinator 嵌套 > 2**(defect #5) | 立即 HARD KILL 候选 | ✅ |
| **新增:error-as-success 比例 > 10%**(defect #6) | 记 v1 §观察,defer v2 | 🟡 |
| **新增:session store 增长率** | 触发 cleanup 监控 | ✅ |

---

## 附录 D:决策树(你的 Agent 适配 Spike 吗)— Phase 2 校准后

```
                  你的 Agent 适配 Phase 6 Spike 吗?
                              │
                              ▼
                  Q1: Agent 需跨调用有状态吗?
                              │
                  ┌───────────┴───────────┐
                  ▼                       ▼
                  否                      是
                  │                       │
                  ▼                       ▼
              ✅ 干净适配 Spike        Q2: 需流式输出吗?
              (通过 IToolRegistry::    ┌──────┴──────┐
               register_tool_function()   ▼             ▼
               注册 1 个 tool)         否            是
              干净 = atomic 无状态    │             │
              slash 命名 per ADR-0043  ▼             ▼
              (例: g4_memory/store)  Q3: 需调其他 Agent?  🔴 不适配 Spike
                                        │          → 推 Phase 6 v2 +
                  ┌────────────┬────────┴────────┐  stream-aware
                  ▼            ▼                 ▼  Service Interface
              否              是                 是
              │               │                 │
              ▼               ▼                 ▼
              ✅ Spike OK     🟡 awkward #2   🟡 awkward #1+#2
              (单 tool 调用)  nested agent    stateful + nested
                             (可做 Spike 但     (可做 Spike 但
                              flag for §八)      flag for §八)
                             
                             flag for DECLARE_SERVICE 形式化(参 ADR-0051 §5)
```


  ─────────────────────────────────────────────────────────────
  ⚠️ 任何"🟡 awkward"响应都触发 §八检测方法论:
     • Layer 1: review checklist 跑
     • Layer 2: instrumentation 加埋点
     • Layer 3: demo 后 memo 写

  当 2+ 不同类别 awkward 模式涌现 → 触发 DECLARE_SERVICE 形式化
  ─────────────────────────────────────────────────────────────
```

---

## 参考文档

### 关联 ADR

| ADR | 关系 | 状态 |
|-----|------|------|
| [ADR-0050](../adr/adr-0050-phase6-strategic-evaluation.md) | Phase 6 战略,本文档战术层引用 | 🔍 Proposed |
| [ADR-0051](../adr/adr-0051-phase6-pdk-composition-spike.md) | Phase 6 PDK Composition Spike(2026-07-14 commit c33b132,W1 fix list 11/11 已完成,2nd Metis 0 CRITICAL;W2-W3 BLOCKED on Stage Gate 2026-07-18 + Sprint 23 capacity) | 🔍 Proposed |
| [ADR-0019](../adr/adr-0019-iinteraction-bus-mvp.md) | IInteractionBus 约束 | ✅ Approved |
| [ADR-0020](../adr/adr-0020-thread-model-isolation.md) | Thread Isolation 约束(⚠️ 逻辑非物理) | ✅ Approved |
| [ADR-0021](../adr/adr-0021-pdk-design.md) | PDK Design 约束 | ✅ Approved |
| [ADR-0022](../adr/adr-0022-plugin-loading.md) | Plugin Loading 约束 | ✅ Approved |
| [ADR-0023](../adr/adr-0023-tool-result-standard.md) | ToolResult 约束 | ✅ Approved |
| [ADR-0031](../adr/adr-0031-execution-policy.md) | Execution Policy 约束(ToolCoordinator) | 🟡 Partial |
| [ADR-0033](../adr/adr-0033-session-hierarchy.md) | Session Hierarchy 约束 | ✅ Approved |
| [ADR-0034](../adr/plugin/adr-0034-model-router.md) | IModelRouter 参考 Service 模板 | ✅ Approved |

### OpenSpec / Master Plan

- [Master Plan: 2026-07-03-phase5-self-bootstrapping.md](../superpowers/plans/2026-07-03-phase5-self-bootstrapping.md)
- [Master Plan: 2026-07-10-phase5-remainder-adr-sync.md](../superpowers/plans/2026-07-10-phase5-remainder-adr-sync.md)

### Oracle Sessions

| Session | 主题 | 时长 |
|---------|------|:---:|
| `ses_0ae4b8107ffetONLmb2Sv2wTb5` | Phase 6 4 候选战略评估 | - |
| `ses_0a2102c3effeVNVGOf5HxMgizu` | **Round 1** Service-ification 架构缺陷分析 | 3m52s |
| `ses_0a206a23cffe1IEirU5iNaxFxC` | **Round 2** 实施细节 + 风险路径 | 5m51s |

### PDK Examples(参考实现)

- `examples/agent_basic/` — 基础单 Agent
- `examples/agent_simple/` — MockLLMProvider 单轮 ReAct(Sprint 19)
- `examples/agent_loop/` — MockLLMProvider 多轮(Sprint 19)
- `examples/slice_01_tool_call/` — Track 0.2 端到端(--mock)
- `pdk/llama_engine/` — 首个 PDK 推理 engine plugin(C14,Sprint 8)

---

## 文档元数据

| 字段 | 值 |
|------|-----|
| **文件** | `docs/superpowers/plans/2026-07-14-phase6-agent-os-reference.md` |
| **作者** | Sisyphus(HydraForge Team) |
| **创建日期** | 2026-07-14 |
| **最后更新** | 2026-07-14 |
| **状态** | 🔍 Proposed (待 D0 + 用户拍板) |
| **预计下次更新** | W3 D11 后(ADR-0051 ✅ + 实际数据回填 §八观察) |
| **后续归档位置** | Phase 6 v1 ship 后,本文档作为 Phase 6 v1 archive 一部分;后续 Phase 6 v2 / Phase 7+ 创建新文档 |
| **预计长期保留** | 是(战略愿景文档,与 ADR-0050 同级别) |

---

**最后说明**:本文档是 **Agent OS 愿景 + Phase 6 战术的双重文档**。它服务于两个时间尺度:
- **短期(W1-W3)**:作为 Phase 6 v1 实施的 checklist + method reference
- **长期(Phase 7+ 或更远)**:作为 Agent OS 愿景的"宪法性"参考,所有新 G2/G4/G5 及更多 PDK plugin 都应参考本文档 §四 §十二 + 附录 D 决策树

文档本身是🔍 Proposed 状态,随实施进展会更新。预计 W3 D11 ADR-0051 ✅ 后,本文档作为 ADR-0051 的**强证据文档**被引用。
