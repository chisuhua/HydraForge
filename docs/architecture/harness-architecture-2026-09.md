# Harness 架构定义（2026-09）

**生成日期**: 2026-09-23（v1.0 — 由用户决定作为 Project Source of Truth 三方架构之一新建）
**最后验证**: 2026-09-23（v1.0，**C2 genome-registry ✅ ship** + **C4 harness-rsi-pilot ✅ ship + GO** + **Pre-Wave3 G1 remove 治理 ✅ ship** + **Pre-Wave3 G4 genome-wiring ✅ ship** + **ADR-0078 Wave 3 Phase 1 ✅ Approved**, 验证命令见 §八）
**作者**: Architecture Working Group
**状态**: ✅ **Approved (v1.0, 2026-09-23)** — **Harness 维度 Source of Truth**（与 [`./self-evolution-architecture-2026-08.md`](./self-evolution-architecture-2026-08.md) v1.5 + [`./rsi-architecture-2026-09.md`](./rsi-architecture-2026-09.md) v1.0 共同构成 Self-Evolution / Harness / RSI 三方架构一致基线）

> **三方配套关系**:
> - **自进化架构** ([`./self-evolution-architecture-2026-08.md`](./self-evolution-architecture-2026-08.md)) — 整体 9 段闭环 + 4 支撑平面 + 3 阶段路线
> - **Harness 架构** (本文档) — Harness 作为自进化闭环的核心变异对象, 数据模型 + 装配 + 变更能力 + 守门 + 持久化的纵深内容
> - **RSI 架构** ([`./rsi-architecture-2026-09.md`](./rsi-architecture-2026-09.md)) — Harness/Data/Model 三算子叠加策略 + 4 阶段 ship 实施路径
>
> 三方无冲突, 互补. 任何变更任一方必须同步另两方的边界段.
>
> **核心边界 (v1.0)**: 本文档只描述 **Harness**（Agent 的 prompt + tools + workflow 配置 + 索引/版本化/守门）。**不包含**: 训练权重 (Model-RSI 范畴, 见 RSI 架构文档) / 训练数据采集 (Data-RSI 范畴) / Agent-Agent 协同进化 (S4 阶段) / 端到端 serving 启用 (`harness-rsi-pilot V2` deferred, 见 §十.1).

---

## 一、Harness 在自进化体系中的位置

### 1.1 从外部观察

> **Harness** = HydraForge 中 Agent 的**完整配置**，包括 prompt（system prompt + 模板）、tool 注册表（可用工具集合 + 权限）、workflow 配置（DAG/ParsedGraph）。

`ChatSession` 启动时，Harness 决定这个 Agent:
- 看到什么系统 prompt
- 能调用哪些工具
- 如何编排 workflow
- (将来) 装载哪些 skill / plugin

### 1.2 三方架构边界

```
┌─ 自进化架构 (9 段闭环) ─────────────────────────────────────────────┐
│                                                                        │
│  ┌── Harness 架构 (本文档) ──────┐  ┌── RSI 架构 ───────┐           │
│  │                                │  │                   │           │
│  │  Harness 是什么                │  │  Harness-RSI      │           │
│  │  数据模型 + 装配               │  │  ─ Harness-RSI 部分 ─│          │
│  │  变更能力 (apply_harness_      │  │  (工具/边界/         │           │
│  │  mutation) + 持久化            │  │   评测基线)         │           │
│  │  守门 (Gate 0/1/2/2.5/3)      │  │                   │           │
│  │                                │  │  Data-RSI          │           │
│  │                                │  │  (Trajectory IR +   │           │
│  │                                │  │   Distillation)    │           │
│  │                                │  │                   │           │
│  │                                │  │  Model-RSI         │           │
│  │                                │  │  (Wave 3 + Phase 2)│           │
│  └────────────────────────────────┘  └───────────────────┘           │
│                                                                        │
└────────────────────────────────────────────────────────────────────────┘
```

### 1.3 已经实施的 5 阶段 ship 实证

Harness 架构不是在真空中设计的, 它由以下 5 阶段 ship 实证支撑:

| 阶段 | Change | 与 Harness 架构的关联 |
|------|--------|---------------------|
| **Wave 1** (C0/C1/P0/F1) | 修 chat demo 端到端 | 建立 ChatConfig + ChatSession 装载 Harness 的初始通道 |
| **Wave 2** (C2/C3/D8) | genome-registry + h-d-m-guard + Appendix A v2.2 | 定义 Harness 的可版本化形态 (Genome) + Harness 守门 (H→D 强制) |
| **Wave 2.5** (C4) | harness-rsi-pilot + GO | Harness-RSI 首个端到端闭环 (mock + 1 turn) |
| **Pre-Wave3** (G1/G2/G3/G4) | remove 治理 + eval_quality + sync-pdk + genome-wiring | Harness 变更的对称治理 (tools_add = tools_remove) + 基因接线 (apply + GEPA → Registry) |
| **Wave 3 Phase 1** (W3.P1) | finetune-base-model pilot + ADR-0078 翻牌 | Harness 与 Model-RSI 的衔接接口 (serving provider 注册) |

---

## 二、Harness 是什么

### 2.1 数据模型

#### 2.1.1 Genome CRD 中 `spec.harness` 字段（唯一事实源）

```yaml
apiVersion: agenticdsl.dev/v1
kind: Genome
metadata:
  name: <string>                  # Genome 名称 (e.g., "default-chat")
  version: <int>                 # 版本号 (单调递增)
  parent: <name>@<version>        # 父版本 (必须, lineage 强制)
  created_by: <string>           # 创建者 (fork 时继承 parent)
  capture_mode: <None|Training>   # 训练数据采集模式
spec:
  harness: <string>              # Harness 完整内容 (string, 是 .agent.md 等的渲染结果)
  tools: <tool_ref[]>            # 工具引用列表
  budget: <budget_ref>           # 预算引用 (IBudgetController)
  model_routing: <router_ref>    # 模型路由配置
  prompt_cache_prefix: <string>  # Prompt cache 稳定前缀 (实现未来)
```

> **说明**: v1.0 仅明确 `harness` 字段为 `string` 而非早先讨论中的 `object`。原因是 (a) Harness 渲染后的产物是 Markdown/YAML/DSL 字符串, (b) object schema 会过早承诺结构, (c) C2 ship 的 filesystem 后端直接保存 `genome.yaml` 内容, 文本字段最简单. 后续如需细粒度 patch, 在 Phase 2+ 在 `.harness_field` 子段增加 typed patch 接口, **不破坏 v1.0 string backward compat**.
>
> 详细 CRD 定义见 [`../../research/agent-distillation-sota-2026-08.md`](../../research/agent-distillation-sota-2026-08.md) + [`../../research/rsi-three-operators-hydraforge-mapping-2026-09.md` 旧路径](../../research/rsi-three-operators-hydraforge-mapping-2026-09.md) (即将迁移到 `../architecture/rsi-architecture-2026-09.md`).

#### 2.1.2 GenomeVersion 类型 (单一所有权避免 ODR)

```cpp
// 单一定义点: include/agenticdsl/genome/genome_version.h
namespace agenticdsl::genome {
struct GenomeVersion {
  std::string name;
  std::uint64_t version;  // 单调递增
};
}  // namespace agenticdsl::genome
```

> **设计约束**: 该 struct 在 `attribution_record.h` 中曾有空 stub, 已被 G1 ship 删除, 改为单一所有权. 严格规则: **任何 namespace 不允许有同 name / 不同字段 / 不同 size 的 `GenomeVersion` 定义** (避免 ODR 冲突).

### 2.2 不在 Harness 范围内

以下内容虽然常与 Harness 混淆, 但不属于本文档范围:

| 概念 | 在哪里定义 |
|------|-----------|
| 训练数据采集 (Capture → JSONL → Training Pipeline) | RSI 架构文档 Data-RSI 章节 |
| 模型权重调整 (LoRA/QLoRA 训练) | RSI 架构文档 Model-RSI 章节 |
| Agent-Agent 协同进化 | 自进化架构 §四.3 + §六 S4 |
| Prompt 模板库 (`lib/prompt/*.md`) | DSL 标准库文档 [`../specs/stdlib-v3.10.md`](../specs/stdlib-v3.10.md) |

---

## 三、Harness 装配 (从 Genome 到运行时 Agent)

### 3.1 装配入口

```cpp
// 入口 1: ChatSession 构造
ChatSession(
  DSLEngine* engine,
  AgentConfig cfg,                       // 包含 ChatConfig (Harness + 预算)
  std::unique_ptr<ILLMProvider> provider,
  ITimerService* timer = nullptr
);

// 入口 2: ChatConfig 显式 override_* 方法 (Sprint 19+ ship, 5 个 API)
ChatConfig& override_system_prompt(std::string);
ChatConfig& override_tools(std::vector<std::string>);
ChatConfig& override_budget(ExecutionBudget);
ChatConfig& override_model_routing(RouterConfig);
ChatConfig& override_prompt_prefix(std::string);
```

### 3.2 当前已 ship 的装配路径 (Wave 1 + Wave 2.5)

| 步骤 | 路径 | 状态 |
|------|------|------|
| 1 | 用户发命令 `ChatConfig::override_*` 直接修改 Harness | ✅ Sprint 19 ship |
| 2 | C4 harness-rsi-pilot apply_harness_mutation(prompt_delta, tools_add/remove, workflow_patch) | ✅ C4 ship + GO |
| 3 | `apply_harness_mutation` 走 MutationGovernance.authorized 双门禁 | ✅ C4 ship |
| 4 | 通过 → 修改 ChatConfig 内存 (但未持久化) | ✅ C4 ship (无持久化) |
| 5 | 通过 → fork → commit(genome) → IGenomeRegistry.persist (持久化) | ✅ **G4 wiring ship** |

### 3.3 已 ship vs 端到端缺口

| 装配阶段 | 是否已 ship | 证据 |
|----------|------------|------|
| prompt_delta 装载到 ChatSession | ✅ | ChatConfig override + ChatSession.cpp L500-503 |
| tools_add 到 ChatSession.tool_registry | ✅ | IToolRegistry::register_tool_function (Phase 4.0 DB1 fix `f7f0fe3`, 25 文件 override) |
| tools_remove 从 ChatSession.tool_registry | ✅ | **G1 ship** (Pre-Wave3, `9709317`) — IToolRegistry::unregister_tool_function 引入 + SecureToolRegistry gating + mutex commit + trace_id 透传 |
| workflow_patch 装载为新 ParsedGraph | ⚠ partial | C4 Case 4 Deferred per Oracle bg_1f291bc4 DEAL-BREAKER (workflow_patch VariantScope 风险), V1 不支持 |
| **load(genome@N) → 重建 ChatSession → 1 turn** | ❌ **V2 defer** | G4 out-of-scope 显式 deferred; 路径 = `IGenomeRegistry::load` → `Genome::to_chat_config()` → `ChatSession(engine, config, ...)` → `DSLEngine::run` |

> **V2 缺口** 是 Harness 架构的**最大遗留缺口**: Genome 持久化已 ship (C2 + G4), 但**装载回路**未 ship (commit message 显式 out-of-scope). 这是 Wave 4 立项的**最小可信前提** (per Decision Record `docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md`).

---

## 四、Harness 变更能力 (Mutation Governance)

### 4.1 apply_harness_mutation 5 参数签名 (C4 ship)

```cpp
// include/agenticdsl/evolution/harness_rsi.h
struct HarnessMutations {
  std::optional<PromptDelta> prompt_delta;                  // L1 — prompt 增量
  std::optional<ToolsAdd> tools_add;                          // L2 — 添加工具 (新增 G1 ship)
  std::optional<ToolsRemove> tools_remove;                    // L2 — 移除工具 (G1 ship, 治理对称)
  std::optional<WorkflowPatch> workflow_patch;                // L3 — workflow 重组 (V1 defer)
};

struct MutationGovernancePolicy {
  std::set<std::string> semantic_locked_tools;                // 语义锁 (G1 ship, D2 默认 empty)
  std::set<std::string> denied_tools;                         // 黑名单 (per case veto)
};

// 5 参数签名 (per Oracle C2 消除 core→PDK 反向依赖)
Result<AppliedMutation, MutationError> apply_harness_mutation(
  HarnessMutations mutations,           // 1
  EvolutionState current_state,         // 2 — H→D→M 状态
  const AttributionRecord* attribution, // 3 — Phase 6c 信用
  IEvaluator* evaluator,                // 4 — 评估信号
  IBudgetController* budget,            // 5 — 预算门禁
  MutationGovernancePolicy policy,      // 6 — 治理策略 (最终签名 = 6 字段)
);
```

> **L1/L2/L3/L4 分级** (per ADR-0084): L1 prompt / L2 tools / L3 workflow / L4 weights. **L4 显式禁止** in V1 (per ADR-0084 MutationGovernance 设计边界).

### 4.2 已 ship 治理功能（per Pre-Wave3 G1）

| 治理能力 | Ship 状态 | 证据 |
|----------|----------|------|
| semantic_locked_tools 语义锁 | ✅ G1 ship 2026-09-21 | D2 design 决策: `apply_harness_mutation` 检查 `policy.semantic_locked_tools` 不在 mutation.tools_add 范围内 |
| denied_tools 黑名单 | ✅ C4 ship | Case 3a veto 重写 (内部 `is_tool_allowed(meta, policy)` policy check) |
| tools_remove 对称治理 | ✅ G1 ship 2026-09-21 | 移除工具与添加工具走相同路径 + mutex commit (`mutation.reverted` 事件) |
| trace_id 透传 | ✅ G1 ship 2026-09-21 | mutation 事件 payload 加 `meta.trace_id`, 闭环节道 |
| ApprovalPolicy 拦截 | ✅ C4 ship | Case 2 4-field 事件载荷断言 (failed_conditions + attribution_verdict + eval_quality + budget_state) |

### 4.3 V1 boundary / V2 候选

**v1.0 显式 out-of-scope**:
- L3 workflow_patch (V1 不支持, Oracle bg_1f291bc4 DEAL-BREAKER)
- L4 weights (ADR-0084 显式禁止, 留 Model-RSI 范畴)
- tools_replace atomic (split into remove + add sequence, atomicity 由 G1 mutex commit 担保)
- prompt_template deep-edit (per ADR-0074 Prompt Evidence Gate 中间实现)

**v2.0 候选** (Wave 4 立项窗口):
- L3 workflow_patch 受治理支持 (per design 28 days)
- Harness ↔ Model-RSI Provider 双向绑定 (per Wave 3 Phase 2 D6/D7)
- load(genome@N) 端到端 E2E (per §三.3 缺口)

---

## 五、Harness 守门 (5-tier gate model)

Harness-RSI 守门是 C4 + G1 + G4 5 阶段 ship 后的**多层防御**, 不依赖单一检查点:

```text
                  ┌─ mutation request ─┐
                  │                     ▼
        ┌────────────────────────────┐
   G0   │  MutationRequest 语法校验   │  ← workflow_patch 校验上移 (G4 ship, 设计修订 #3)
        │  (named field 必填, types)   │
        └────────────────────────────┘
                  │  pass
                  ▼
        ┌────────────────────────────┐
   G1   │ MutationGovernance policy   │  ← is_tool_allowed(meta, policy)
        │ + attribution_verdict       │     semantic_locked_tools check
        │ + eval_quality              │     denied_tools check
        │ + budget_state              │
        └────────────────────────────┘
                  │  pass
                  ▼
        ┌────────────────────────────┐
   G2   │ IGenomeRegistry.load       │  ← load genome@N to validate
        │ + judge_data_freshness     │     (C3 follow-up, ADR-0086 v1.1)
        └────────────────────────────┘
                  │  pass
                  ▼
        ┌────────────────────────────┐
        │  G2.5 partial-apply         │  ← collect AppliedMutation 准备
        │  收集快照 (prompt+tools)    │     4 字段 (commit_→, prompt_snap, tools_snap, applied_)
        │  准备实现 (无副作用)        │
        └────────────────────────────┘
                  │  pass
                  ▼
        ┌────────────────────────────┐
   G3   │ fork(name, parent_version) │  ← persist BEFORE apply (G4 ship)
        │ → commit(genome)               RegistryRejected → 零状态变更
        │ → apply chat_config       │
        │ → append genomed commit    │
        │ committed 事件              │
        └────────────────────────────┘
                  │  success
                  ▼
                emit `genome.committed` event
                return Result::success(AppliedMutation)
```

| Gate | 失败行为 | 已 ship? |
|------|---------|----------|
| **G0** 语法校验 | 立即拒绝 (runtime_error InvalidMutation) | ✅ C4 (DB1 fix) + G4 (workflow_patch 上移) |
| **G1** policy + 3 信号 | 拒绝 + 发 `mutation.denied` 事件 (ADR-0068 L1) | ✅ C4 (4-field payload) + G1 (semantic_locked_tools) |
| **G2** Registry load + freshness | 拒绝 + RegistryRejected | ✅ C2 (Registry) + G4 (judge_data_freshness 实装) |
| **G2.5** partial-apply 零状态变更 | 失败 → 回滚到快照 | ✅ C4 (4th review bg_afa84d4d Critical-1) |
| **G3** persist-before-apply | commit 失败 → 零状态变更, emit `genome.persist_failed` | ✅ G4 ship (Gate 3 插入点与 workflow_patch 早退冲突已修) |
| **H→D→M 守门** (额外层) | can_transition(Harness→Model) 返回 false → InvalidPhase | ✅ C3 (ADR-0088) + C3 follow-up |

> **H→D 守门不是 mutation 守门的一部分, 是阶段守门**: `TransitionGuard::can_transition(Harness, Data, ...)` + `evaluate_readiness()` 4 条件 (Attributed + 回归门 PASS + 预算 + 无未控制混杂) 必须在 H→D 阶段转换时被满足. 已 ship (C3 + walk-ancestors follow-up).

---

## 六、Harness 变更事件 (ADR-0068 Appendix A v2.3 registration)

Harness-RSI 通过以下事件 (v2.3 append) 完成审计闭环:

| 事件名 | Payload | 发射方 |
|--------|---------|--------|
| `mutation.proposed` | `subject_kind` + `mutation_kind` + `subject_version` | apply_harness_mutation 入口 |
| `mutation.committed` | `subject_version` + `parent_version` + `applied_tools_add` + `applied_tools_remove` + `commit_at` | G3 success |
| `mutation.denied` | `failed_conditions` + `eval_quality` + `attribution_verdict` + `budget_state` + `denial_reason` | G1 fail |
| `mutation.reverted` | `subject_version` + `rollback_to` + `revert_reason` | G5 (rollback) — V1 defer |
| `genome.committed` | `genome_version` + `name` + `parent_version` + `capture_mode` + `trace_id` | G3 success |
| `genome.persist_failed` | `reason` + `attempted_version` + `registry_error_code` | G3 fail (G4 ship) |
| `evolution.transition.denied` | `from` + `to` + `reason` + `current_genome` | TransitionGuard can_transition false (C3 ship) |
| `evolution.readiness.denied` | `failed_conditions` + `attribution_verdict` + `eval_quality` + `budget_state` | evaluate_readiness false (C3 ship) |

> 主题表完整注册见 ADR-0068 Appendix A v2.3 (2026-09-22 ship via `2026-09-22-adr-0068-appendix-a-evolution-themes`).

---

## 七、Harness 在 RSI 中的角色 (与 RSI 架构的边界)

### 7.1 Harness-RSI = "改 harness + 评估 + 守门" 闭环

Per [`./rsi-architecture-2026-09.md`](./rsi-architecture-2026-09.md) 第七节, Harness-RSI 是 RSI 三算子之一:

> Harness-RSI 是 RSI 中**复杂度最低、可观测性最高、风险最容易控制**的算子.
> 它是 Model-RSI 的**前置训练阶段**: Harness 改善后, 同一 LLM 下性能提升 ⇒ 归因清晰 (ADR-0086 v1.1 HarnessChange confounder)
> ⇒ 决定是否进入 Model-RSI 训练 (Wave 3 Phase 2+).

### 7.2 边界守则

| 决策 | Harness 架构 (本文档) | RSI 架构 |
|------|----------------------|----------|
| "Can we mutate prompt.tools?" | ✅ 本文 §四 + §五 设计 | referenced in §七 |
| "Should we apply Harness-RSI?" | N/A | §七 "Harness-RSI 启动条件" |
| "Should we do Model-RSI?" | N/A | §七 "Model-RSI 启动条件" |
| "load(genome@N) → 1 turn" | ✅ 本文 §三.3 (deferred) | cross-ref |

---

## 八、Decision Record (为什么是这样)

### 8.1 关键决策 (5 个)

| # | 决策 | 实施载体 | 理由 |
|---|------|----------|------|
| D1 | **Harness 字段类型 = string (不是 object)** | C2 ship + Self-evolution §十.3 | (a) render 后是字符串 (b) schema-overcommit 风险 (c) Phase 2+ 可加 typed patch 不破坏 v1.0 |
| D2 | **apply_harness_mutation 6 字段签名 (非裸 MutationContext)** | C4 ship (Oracle bg_3ef7280a) | 核心↔PDK 反向依赖消除; MutationContext 8 字段太重, break ABI compat |
| D3 | **Gate 0 / Gate 1 / Gate 2 / Gate 2.5 / Gate 3 / H→D 5-tier gate** | C4 + G4 + C3 5 阶段 ship | 任一独立 gate fail-bypass ⇒ 已 ship C4 5 Oracle reviews 累计教训 |
| D4 | **persist-before-apply (G3 fork before apply)** | G4 ship (Oracle bg_e4eec567) | 失败零状态变更; GEPALoop::reflect_and_commit 不能仅发审计事件 |
| D5 | **tools_remove 必须对称治理 (G1 ship, 不允许单边)** | G1 ship (Oracle bg_c706862b) | mutations 等价; 防 undo 语义丢失 |

### 8.2 不做的决策 (vs 期望)

| 主题 | 期望实现 | v1.0 不做的原因 |
|------|----------|----------------|
| workflow_patch L3 mutation | Phase 4 设计包含 | Oracle bg_1f291bc4 DEAL-BREAKER (VariantScope 风险); V2 路线 |
| online prompt template deep-edit | Wave 1 C1 L1 prompt 涉及 | ADR-0074 Prompt Evidence Gate 中间实现; L2 patch 待 V2 |
| prompt auto-tuning with no human approval | 期望自治性 | 默认 read_only + 显式 ApprovalPolicy (per §四 当前允许的最小闭环) |
| load(genome@N) → 重建 ChatSession E2E | 应该有的 hook | C4 + G4 out-of-scope 显式 defer; V2 立项必备 |

---

## 九、验证命令

```bash
# 文档存在且关键引用有效
ls docs/architecture/harness-architecture-2026-09.md
ls docs/architecture/self-evolution-architecture-2026-08.md
ls docs/architecture/rsi-architecture-2026-09.md

# apply_harness_mutation 签名 (per ADR-0088 + C4 ship)
grep -E "struct HarnessMutations|apply_harness_mutation\(" \
  include/agenticdsl/evolution/harness_rsi.h

# GenomeVersion 单一所有权 (per ADR-0086 v1.1 + G1 ship)
grep -rE "struct GenomeVersion|class GenomeVersion" \
  include/agenticdsl/genome/ include/agenticdsl/evolution/ 2>/dev/null
# 预期: 单一命中 (genome/genome_version.h)

# Gate 0/1/2/2.5/3 + H→D 6 层守门存在
grep -E "Gate 0|Gate 1|Gate 2|Gate 3" \
  src/evolution/harness_rsi.cpp 2>/dev/null

# Pre-Wave3 4-Gate 状态 (G1+G2+G3+G4 全 ✅)
for g in harness-rsi-remove-governance evolution-verdict-reward-quality \
         sync-pdk-contract-header genome-wiring-harness-rsi-gepa; do
  test -d "openspec/changes/archive/${g}-2026-09-22" \
    && echo "$g ✅ archived" || echo "$g ❌ missing"
done

# Wave 3 Phase 1 ship (2026-09-23)
git log --oneline | grep -E "f0a5c4b|wave-3-finetune-base-model"

# ADR-0068 Appendix A v2.3 (evolution.* 主题注册)
grep -E "evolution\.(transition|readiness)\.denied|genome\.(committed|persist_failed)" \
  docs/adr/adr-0068-event-emission-contract.md
```

---

## 十、当前船状态 (shipped matrix)

### 10.1 已 ship ✅

| 阶段 | 与 Harness 相关 ship 物 |
|------|----------------------|
| **Wave 1** (2026-09-17-18) | ChatConfig override_* 方法 (Sprint 19 ship, 5 API); F1 react-decide-empty-response 修复 |
| **Wave 2** (2026-09-19-20) | C2 IGenomeRegistry 5 methods; C3 TransitionGuard H→D 守门 5×5 矩阵; C3 follow-up walk_ancestors + judge_data_freshness; D8 主题注册 evolution.transition/readiness.denied |
| **Wave 2.5** (2026-09-20-21) | C4 apply_harness_mutation 6 字段签名; 9 cases / 43 assertions; Decision Record GO; IToolRegistry::unregister_tool_function 25 文件 override |
| **Pre-Wave3** (2026-09-21-22) | G1 remove 治理 + semantic_locked_tools + trace_id; G2 eval_quality 字段复用; G3 sync-pdk-contract-header; G4 genome-wiring-harness-rsi-gepa (Gate 3 persist-before-apply + GEPA fork→commit + 2 事件) |
| **Wave 3 Phase 1** (2026-09-23) | ADR-0078 ✅ Approved; D1+D3+D7 最小版 ship; finetune-base-model pilot |

### 10.2 不在 v1.0 Source of Truth 范围 (out-of-scope)

| 项 | 原因 | 候选立项 |
|------|------|---------|
| `load(genome@N) → 重建 ChatSession → 1 turn` 端到端 | C4 + G4 out-of-scope 显式 deferred | harness-rsi-pilot V2 (per Decision Record §3 post-hoc closure gate) |
| `workflow_patch` L3 mutation | Oracle bg_1f291bc4 DEAL-BREAKER | V2 (Harness 架构 V2.0 候选) |
| `prompt_template deep-edit` | ADR-0074 中间实现 | L2 patch 设计窗口 |
| `online teacher distillation` | S3 阶段 + Data-RSI 边界 | Wave 3 Phase 2 D6 |
| `Agent-Agent 协同进化` | S4 阶段 + promotion criteria 未满足 | 未来独立 spike |
| `Multi-Action Conversational Agents` | L4 weights 显式禁止 | Model-RSI Wave 3 Phase 2 |

### 10.3 文档维护规则

**v1.0 维护触发**:
- 任何 C4 / G1 / G4 后续 ship 修订 → 更新本文 §三 + §四 + §五
- 任何 new Gate tier (V2+) → 更新本文 §五
- 任何 new harness 事件 (v2.4+) → 更新本文 §六
- 任何 self-evolution-architecture / rsi-architecture 不一致 → 同步 §一.2

**保留旧版数据**: C2 / C3 / C4 / G1 / G2 / G3 / G4 commits 保留 git 历史 (AGENTS.md Day 5 lesson — `git mv` not `rm`).

### 10.4 不与 ADR 冲突保证

**v1.0 与现行 ADR 一致, 无冲突**:
- ✅ ADR-0083 IEvaluator (无直接冲突)
- ✅ ADR-0084 MutationGovernance (L1-L3 分级 + 6 字段 MutationContext)
- ✅ ADR-0086 v1.0+v1.1 Credit Assignment (HarnessChange confounder 强相关)
- ✅ ADR-0088 H→D→M Transition Guard (H→D 守门)
- ✅ ADR-0078 Fine-tune (Wave 3 Phase 1 ship + Phase 2 待立项)
- ✅ ADR-0068 Appendix A v2.3 (genome.committed + evolution.* 主题注册)

任何未来 ADR 与本文 v1.0 冲突, 必须先升档 / amend 本文档 (经 Oracle dual-agent review + 24h cooling-off) 后才能 ship.

---

**维护规则 (v1.0)**: 三方架构中任意一方变更需复核另外两方 (cross-doc consistency check, 1-on-1 reasoning). Self-Review Checklist 强制 (per docs/architecture/adr-self-review-checklist.md). 当 ADR-0078 Phase 2 立项时, 本文档 v2.0 须重新 review (focus L3+L4 边界).

---

## 十一、承载例: pdk_chat_demo Reference Traceback (2026-09-23)

> **本节定位**: 把 Harness 架构的**每段纵深内容** (数据模型 + 装配 + 变更能力 + 5-tier 守门 + 持久化) 标记到 `examples/pdk_chat_demo/` 内的具体 file/line/test case, 让 abstract 的 Harness 概念变得可触达.

### 11.1 数据模型 → pdk_chat_demo 落地

| 数据模型元素 (§二.1) | pdk_chat_demo 落地 | 验证 |
|--------------------|-------------------|------|
| **Genome CRD `spec.harness` string** | `examples/pdk_chat_demo/AgentConfig.h` + `cmd_line_parser.h` 默认 harness 由 config.json 装载 (`config.json` `system_prompt` field, 加载为 .md 字符串) | `test_chat_session.cpp` Case "default config load" |
| **GenomeVersion 单一所有权** | `include/agenticdsl/genome/genome_version.h` + pdk_chat_demo 通过 `LLMProviderFactory` 间接消费 (不直接构造 GenomeVersion, 经 `LLMParams.model` 与 GenomeVersion 都有 `name` 字段, 命名空间分开 `agenticdsl::genome::GenomeVersion` vs `agenticdsl::LLMParams`) | `tests/test_genome_registry.cpp` Case 13 + `tests/test_provider_factory.cpp` 6 处 ctor 自注册 fix |

### 11.2 装配 (§三) → pdk_chat_demo 落地

| 装配路径 (§三.2) | pdk_chat_demo 落地 | 验证 |
|----------------|-------------------|------|
| **入口 1: ChatSession 构造** | `examples/pdk_chat_demo/main.cpp` 启动 `ChatSession` (Sprint 32 ship) — `ChatSession(dsl_engine, AgentConfig{ChatConfig{}}, llm_provider, &timer_service)` | `test_chat_session.cpp` 5 cases / `--mock` e2e PASS |
| **入口 2: ChatConfig override_*** | `examples/pdk_chat_demo/commands/` 16 commands 中 `--model` (model_command.cpp) + `--name` 等覆盖 `override_system_prompt` + `override_tools` | `test_chat_session_consumer.cpp` + `test_chat_session_loop_result_ok.cpp` (Loop OK 路径下 ChatConfig 装载) |
| **已 ship 步骤 1-5** | 步骤 1+2 已 ship (ChatConfig override + ChatSession ctor); **步骤 3-5 (mutation → governance → registry → fork → commit)** V1 C4 pilot **未启用到 pdk_chat_demo 实例** (mock + 1 turn V2 缺口) | C4 9/43 mock 测过; pdk_chat_demo 真实 wire 需 L2 (`pdk_chat_demo_evolution/`) |

### 11.3 变更能力 (§四) → pdk_chat_demo 落地

| 变更 (§四.1) | pdk_chat_demo 落地 | 验证 |
|------------|-------------------|------|
| **`prompt_delta` (L1)** | `model_command.cpp::model_command()` → `ChatConfig::override_system_prompt(suffix)` (Sprint 19 ship) | `test_chat_session_consumer.cpp` Case "model switch emits prompt delta" |
| **`tools_add` (L2)** | pdk_chat_demo 启动时 `ToolRegistry::register_tool_function` 装载 (Phase 4.0 DB1 fix, `f7f0fe3` 25 文件 override) | `test_chat_session.cpp` Case "工具注入 → 调用成功" |
| **`tools_remove` (L2)** | `commands/cancel_command.cpp::cancel_command()` 不通过工具移除, 但 `secure_tool_registry` + `tool_macros_unregister_tool_function` 已 ship (G1 ship 2026-09-21) | G1 test_harness_rsi_pilot 22/22 (含 Case 5d "remove 不存在的工具") |
| **`workflow_patch` (L3)** | ⚠️ **V1 defer** — pdk_chat_demo 不支持 workflow_patch (Oracle bg_1f291bc4 DEAL-BREAKER) | — |
| **`MutationGovernancePolicy.semantic_locked_tools`** | G1 ship → pdk_chat_demo 可在 V2 启用, 默认空 | G1 test_harness_rsi_pilot 22/22 |
| **trace_id 透传** | G1 ship → mutation 事件经 `IInteractionBus::emit_async` 经 `trace_id` 字段 + `event_log.cpp` JSONL 化 | `tests/test_chat_session_events.cpp` Case "mutation event 含 trace_id" |

### 11.4 5-tier 守门 (§五) → pdk_chat_demo 落地

| Gate tier | pdk_chat_demo 可应用入口 | 当前状态 |
|-----------|------------------------|---------|
| **G0 (语法)** | `model_command.cpp` 入参校验 (Sprint 19) | ✅ 已 ship (在 `model_command.cpp:103-119` 单测内) |
| **G1 (policy + 3 信号)** | **未 wire** — V2 `examples/pdk_chat_demo_evolution/` 计划接入 | ⏳ L2 立项 (L1 本节仅 traceback) |
| **G2 (load + freshness)** | **未 wire** | ⏳ L2 |
| **G2.5 (partial-apply)** | **未 wire** | ⏳ L2 |
| **G3 (persist-before-apply)** | `examples/pdk_chat_demo_evolution/Genome.commit_demo_session()` (规划中) | ⏳ L2 |

### 11.5 事件 (§六) → pdk_chat_demo 已观测到的事件 (selected)

| 事件 | pdk_chat_demo 观测点 | 测试 |
|------|------------------|------|
| `mutation.proposed` | **未触发** (V2 缺) | — |
| `mutation.committed` | **未触发** | — |
| `mutation.denied` | **未触发** | — |
| `genome.committed` (G3) | **未触发** (V2 缺) | — |
| `evolution.transition.denied` (C3) | **未触发** (V2 缺) | — |
| (其他已 ship 事件) `chat.turn.start/end` (Sprint 32) | `event_handler.cpp::handle_chat_turn_end` | `test_chat_session_events.cpp` 11/13 cases |

### 11.6 边界守则 (§七) 在 pdk_chat_demo 的体现

| 决策边界 | pdk_chat_demo 体现 |
|---------|-----------------|
| "可以 mutate prompt.tools?" | ✅ ChatConfig.override_* 5 method 已 wire (Sprint 19); apply_harness_mutation 6 字段**未 wire** (V2 立项) |
| "Should we do Harness-RSI?" | 决策点 = `commands/evolution_command.cpp` (L2 计划新增, 默认不入 pdk_chat_demo 主路径) |
| "load(genome@N) → 1 turn" | ⏳ V2 `examples/pdk_chat_demo_evolution/Genome.load_chat_session(genome@N)` (L2 计划) |

### 11.7 Reference Example (L2) 关系

- **L1 (本文)** — traceback 只标注"每段 Harness 概念**已经**在 pdk_chat_demo 哪里落地" (✅ 已 ship) **或** "将在哪里落地" (⏳ Phase 2)
- **L2 (`pdk_chat_demo_evolution/`)** — 独立 example sub-project 跑通**真实 Harness-RSI + Data-RSI + Model-RSI reference example** (用 chat_session.cpp + LoopAgent 包装, 不修改 pdk_chat_demo 主 demo), 通过 evolution tracer 收集 evidence → mutation → commit 端到端. 详见 OpenSpec change `pdk-chat-demo-evolution-reference-example` (L2.1-L2.3 待立项)

> **L1 + L2 组合价值**: L1 让人 grep 到落地路径 (30 分钟), L2 让人 clone 后能 `cd examples/pdk_chat_demo_evolution && ./run_evolution_demo` 看 Harness-RSI 实际效果 (2-3 天). 两者互不冲突, L2 必须先 follow L1 的导航.

---

## 十二、Verification Matrix (H1-H6 红线 + 5-tier gate 反向校验, 2026-09-23 升级)

> **来源**: per Cross-Doc Review 2026-09-23 + 用户提交 16 模块 H1-H6 + L2 spec R8/R9.
> **核心命题**: 任何 Harness 变更 (ChatConfig override, apply_harness_mutation, Genome 切换) 必须通过红线验收 (DoD / 不可逆闸门 / 凭证隔离 / trace 重放), 同时输出**正向 + 反向**指标 (per §12.1).

### 12.1 H1-H6 红线反向校验矩阵

| H 维度 | 项目当前 ship 实证 | 红线反向校验命令 |
|--------|--------------------|------------------|
| **H1 目标与契约** | ChatConfig override_* (Sprint 19) + tools_remove (G1) | `ctest -R test_chat_session Loop OK 路径` exit 0 |
| **H2 知识与上下文** | LayeredContext + Session 4-scope | `ctest -R test_session` + LayeredContext diff ≥ 0 lines |
| **H3 控制与编排** | 5-tier gate + TopoScheduler + 3 Loop | 5 tier 反向 unit test (12.2) |
| **H4 工具与协议** | IToolRegistry (Phase 4.0 DB1) + DECLARE_TOOL | drop_ratio ≤ 5% (per §12.3) |
| **H5 证据与结论** | EventLog + IDistillationWriter | trace 重放率 ≥ 99% (per §12.4) |
| **H6 横切保障** | SecureToolRegistry + fail-closed | ability regression test (per §12.1) |

### 12.2 5-tier gate 反向校验 (反向 unit test 套件)

| Gate | 正向 (pass case) | 反向 (fail case, 期望 fail) |
|------|------------------|------------------------------|
| **G0 语法** | valid mutation → pass | invalid mutation → exit non-zero + 错误码 |
| **G1 policy** | allowed tool → pass | denied_tools 命中 → fail + emit `mutation.denied` + drop_ratio ≤ 5% |
| **G1 eval_quality** | quality=Acceptable → pass | quality=Poor → fail |
| **G2 load** | valid genome → pass | invalid genome → fail + `GenomeError::NotFound` |
| **G2 freshness** | same lineage → pass | harness changed → `Confounded` verdict |
| **G3 persist** | valid genome → pass + emit `genome.committed` | disk full → fail + emit `genome.persist_failed` + 零状态变更 |

#### 验证命令

```bash
# 5-tier gate 反向 unit tests (per C4 + G1 + G4 ship 基线)
ctest -R test_harness_rsi_pilot --output-on-failure
# 预期: 22 cases PASS (含 reverse cases)
# 包含: Case 5d Spec R1 scenario 3 "remove 不存在工具" + Case 5e R3 scenario 1 并发 fuzz

# 基因失败反向 unit test (per C3 ship)
ctest -R test_transition_guard --output-on-failure
# 预期: 13/13 cases PASS (含 H→M 阻断)

# Walk-ancestors 失败 → Insufficient 反向 unit test (per C3 follow-up)
ctest -R test_genome_walk_ancestors --output-on-failure
# 预期: 10/10 cases PASS (Critical C2 walk-failure → Insufficient fail-closed)
```

### 12.3 经济性 + Reliability 双维度 (H6 横切, per R8.1)

| 维度 | 期望 | 验证命令 |
|------|------|----------|
| **Reliability** (同输入, 同结果一致性) | ≥ 99% (mock mode 100%) | `ctest -R test_evolution_session` 反复跑 10 次, 成功率 ≥ 99% |
| **Verifiability** (结果可验证率) | ≥ 95% (5-tier gate 通过率) | `ctest --test-dir build` 报告 PASS/TOTAL ≥ 0.95 |
| **单位任务成本曲线** (经济性 R6) | baseline 后每次 mutation +X% cost 但 ≥ -Y% eval_quality → 拒绝 | 待 L2 `--release-metrics` flag 实施 |

### 12.4 H5 trace 100% 可重放 (Red-line)

| 验证项 | 命令 |
|--------|------|
| `event_log.jsonl` 重建确定性 | `run_evolution_demo.sh --mock --trace-events` 跑 10 次, baseline_response 100% byte-identical |
| Session JSONL replay 一致性 | `diff <(run_demo_1.jsonl) <(run_demo_2.jsonl) \| head` 应 ≤ 0 lines (除 timestamp) |
| `genome.committed` 事件幂等 | `ctest -R test_harness_rsi_pilot Case 3c` 反复跑 5 次, genome_version 一致 |

### 12.5 H1 不可逆闸门无绕过路径 (Red-line)

**任何** 不可逆操作的闸门不允许 bypass:

| 不可逆操作 | 闸门 | 绕过尝试检测 |
|-----------|------|--------------|
| genome commit (写入文件系统) | G3 persist-before-apply | bypass 路径 grep → emit `gate.bypass_attempt` 事件 + exit non-zero |
| IToolRegistry::unregister_tool_function | G1 + 静默检查 + mutex | mutex 锁定 → 同线程重入检测 (per AGENTS.md pattern) |
| ChatConfig override_* (运行时 mutation) | G1 + rollback snapshot | rollback failure → fail (per apply_harness_mutation) |

### 12.6 反作弊三模式 (per L2 spec R9)

| R9.1 搜现成答案 | R9.2 修改评判指标 | R9.3 串谋外部平台 |
|------------------|------------------|---------------------|
| Poolside: input 含 baseline hint, Agent 不得速通 | 复旦: IEvaluator 无 write 接口 + mutation gate 拦截 schema 改写 | ExploitGym: sandbox network_mode=none + zero-day RCE 检测 |
| L2 test_anti_cheat_search_solution (1 case) | L2 test_anti_cheat_metric_tampering (1 case) | L2 test_anti_cheat_sandbox_escape (1 case) |
| eval_quality diff > -10% | mutation 拒绝 + emit `evaluation.tampering_attempt` | outbound 拦截 + emit `sandbox.escape_attempt` |

详见 [L2 spec §R9](../../openspec/changes/pdk-chat-demo-evolution-reference-example/specs/pdk-chat-demo-evolution/spec.md).

### 12.7 跨文档一致性

- **L2 链接**: [L2 spec R8 + R9](../../openspec/changes/pdk-chat-demo-evolution-reference-example/specs/pdk-chat-demo-evolution/spec.md) (主 source of reverse indicator + anti-cheat)
- **SoT §十二 同源**: [`./self-evolution-architecture-2026-08.md` §十二](./self-evolution-architecture-2026-08.md) (9 段闭环) + [`./rsi-architecture-2026-09.md` §十二](./rsi-architecture-2026-09.md) (真 RSI 三判据)
- **AGENTS.md Reverse Indicator Rule**: per `AGENTS.md` §REVERSE INDICATOR RULE (已 ship 2026-09-23, commit `4af2092`)

### 12.8 维保规则

**v1.0 → v1.1 升级触发**:
- L2 ship 后, 本 §十二 同步更新 `H1-H6` ship 实证 + 添加 "L2 ship 后实证" 行
- 任何 G1-G5 reverse case ship 后同步
- 任何 Phase 2 (Wave 3 Phase 2) ship 后同步

**禁止**:
- 本 §十二 永远不允许 drop 红线项目 (DoD 100% / 不可逆闸门无绕过 / 凭证零泄漏 / trace 100% 重放)
- 任何反向指标 gate 失败必须 block commit (R8.1 红线)

### 12.9 上下文驱动约束 (Context-Driven Constraint, 2026-09-23 升级)

> **来源**: 用户原话 + L2 spec §R13
> **核心命题**: 任何 H1-H6 红线验收 **必须** 由 ContextRequest 触发, 不是 L2 demo 自身自动跑.

#### 12.9.1 H1-H6 红线 + ContextRequest 字段映射

| H 维度 | 红线验证 | ContextRequest 字段 |
|--------|---------|---------------------|
| **H1 目标与契约** | DoD + 4 类终止条件 | `turn_input` (用户任务) + `expected_eval_quality` (用户期望) |
| **H2 知识与上下文** | 4 层记忆 + 凭证隔离 | `metadata.sensitivity` (public/internal/confidential) — internal/confidential 时 L2 不写盘 + redact trace |
| **H3 控制与编排** | 5-tier gate | `turn_input` (变更请求) + `context_id` (trace ID) |
| **H4 工具与协议** | 工具注册 + Schema 版本 | `task_class` (路由) + `metadata.domain` |
| **H5 证据与结论** | 全量轨迹 + 可重放 | `context_id` (必进 JSONL trace, 跨 ContextRequest 关联) |
| **H6 横切保障** | 安全 / 确定 / 经济 / 可观测 | `metadata.sensitivity` + `invocation_mode` + 跨 ≥ 3 类 ContextRequest 对比 |

#### 12.9.2 5-tier gate + ContextRequest 触发矩阵

| Gate | 触发字段 | 拒绝条件 |
|------|---------|----------|
| **G0** (语法) | `turn_input` + `context_id` 必填 | 任一缺 → exit non-zero (per L2 spec S29) |
| **G1** (policy + 3 信号) | `turn_input` + `task_class` | `metadata.sensitivity == "confidential"` + write → 拒绝 (per H2) |
| **G2** (load + freshness) | `context_id` (历史轨迹) | 跨 ContextRequest 类对比 → 期望 ≥ baseline - 5% (R8.1 红线) |
| **G2.5** (partial-apply) | `context_id` 进 AppliedMutation snapshot | 失败 → 零状态变更 |
| **G3** (persist-before-apply) | `context_id` 进 `genome.committed` payload | commit 失败 → 零状态变更 + `genome.persist_failed` |

#### 12.9.3 trace JSONL 必含 ContextRequest 字段

trace schema 顶层 8 字段不变 (per L2 spec R4) + `meta` 内 4 个**强制** 新字段 (M2 修复, per L2 spec R13 S31):

```diff
{
  "phase": "baseline | mutation | reload | compare",
  "timestamp_iso8601": "...",
  "session_id": "...",
  "turn_input": "...",
  "response": "...",
  "tokens": 0,
  "cost_usd": 0.0,
  "meta": {
+   "context_id": "<uuid>",                    // 来源于 ContextRequest, 必填 (R13 S31)
+   "task_class": "<enum>",                    // 来源于 ContextRequest
+   "expected_eval_quality": "<...>",          // 来源于 ContextRequest (R8.2 失败可追溯)
+   "is_hidden": <bool>,                       // 来源于 ContextRequest (E2 公开/隐藏集)
+   "sensitivity": "<...>",                    // 来源于 ContextRequest (H2 凭证隔离)
    "trace_id": "<uuid>",                     // (原有)
    "capture_mode": "None | Training"         // (原有)
  }
}
```

任何缺失 `meta.context_id` 的 trace 段视为 0% 失配 (per L2 spec S31) → exit non-zero.

#### 12.9.4 与 R8 (反向指标) 集成

**Harness 反向指标必须跨 ContextRequest 类 (per R8.1 + R8.3)**:

| 反向指标 | 计算公式 |
|----------|----------|
| 同任务不同 Harness | `eval_quality(turn_input_i, harness_new) - eval_quality(turn_input_i, harness_baseline)` |
| 同 Harness 不同任务 | `eval_quality(turn_input_j, harness_X) - eval_quality(turn_input_k, harness_X)` |
| 失败样本 drop ratio | `1 - (failure_intercepted_in_new / failure_intercepted_in_baseline)` (跨 ≥ 3 类 ContextRequest) |

`drop_ratio > 5%` → block commit (per R8.1 红线).

#### 12.9.5 跨文档一致性

- [L2 spec §R13](../../openspec/changes/pdk-chat-demo-evolution-reference-example/specs/pdk-chat-demo-evolution/spec.md) (ContextRequest schema 主契约)
- [`./self-evolution-architecture-2026-08.md` §12.9](./self-evolution-architecture-2026-08.md) (9 段闭环 + ContextRequest)
- [`./rsi-architecture-2026-09.md` §12.9 + §11.8.8](./rsi-architecture-2026-09.md) (R3 元指标 + ≥ 3 类 ContextRequest 实证)
- [AGENTS.md "Reverse Indicator Rule"](../../AGENTS.md) (commit `[Reverse Indicator]` 段含 `context_ids`)

#### 12.9.6 维保规则

**v1.0 → v1.1 升级触发**:
- L2 ship 后, 本 §12.9 + §十二 §12.2 5-tier gate 反向校验 双向同步
- ContextRequest schema 变更必须升档 L2 spec R13
- 任何 new ContextRequest 必填字段加入时同步 §12.9.3

**禁止**:
- L2 demo 启动时无 `--context-file` 不得 exit 0 (per L2 spec S28)
- L2 demo 不得自动生成 default ContextRequest (零 hardcode 红线, per L2 spec R13.2)
- 任何 H1-H6 红线验收缺 `context_id` 不得 ship
