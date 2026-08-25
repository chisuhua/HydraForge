# 架构评审会议纪要: ADR-0071 + ADR-0074 + 3 个新 ADR (Oracle 评审输入)

> **会议类型**: Phase 6 自进化方向架构评审会
> **召集人**: Architecture Working Group
> **日期**: 2026-08-24 (会前材料准备就绪, 待架构组会议召集)
> **优先级**: P0 (Oracle 评审 ses_fcba5e477ffeG9wEBHVhU64J0o 标记"本周最高杠杆")
> **关联文档**:
> - `docs/architecture/capability-application-map-2026-08.md` §八 (Oracle 评审)
> - 5 个待评审 ADR（详见 §二 评审材料清单）
> - `docs/architecture/pdk-chat-demo-distill-source-survey-2026-08.md` (B3 调研)

---

## 一、会议目标

1. **决策 4 项 ADR 的命运** (3 个新 ADR + 2 个已 Proposed 待批父 ADR)
2. **解锁 Oracle 评审识别的 6 项架构层缺口** (G10-G15)
3. **同步 ADR-0071/0074 → 4 个子项派生命运** (T17/T19/T20/T21)

---

## 二、评审材料清单 (6 个 ADR)

### 2.1 必评审 (4 个新 ADR, v1.1.3 已起草 🔍 Proposed)

| ADR | 状态 | 文件 | 解决 Gap | 估时 | 紧迫度 |
|---|---|---|---|---|---|
| **ADR-0083** IEvaluator/RewardSignal 契约 | 🔍 Proposed | `docs/adr/adr-0083-evaluator-reward-contract.md` | **G10** | 1 sprint 草案 | **P0** |
| **ADR-0080 v1.2 amendment** (D10 解耦) | 🔍 Proposed | `docs/adr/adr-0080-v1-2-amendment-d10-decouple.md` | **G12** (死锁解除) | 0.5 sprint 草案 | **P0** |
| **ADR-0061-13** 蒸馏输出格式 (IDistillationWriter) | 🔍 Proposed | `docs/adr/skill/adr-0061-13-distillation-output-format.md` | **G15** | 1 sprint 草案 | **P0** |
| **ADR-0061-06 v1.1 amendment** (Trajectory IR 标题修订) | 🔍 Proposed | `docs/adr/skill/adr-0061-06-v1-1-amendment-trajectory-ir-decouple.md` | **G14** (标题耦合风险) | 0.5 sprint 草案 | **P0** (Oracle S4.3 新增,T15 前置) |

### 2.2 待 Promotion (2 个已 Proposed 父 ADR)

| ADR | 状态 | 解决 Gap | 当前问题 |
|---|---|---|---|
| **ADR-0071** LLM-native AgenticDSL 架构 | 🔍 Proposed (2026-08-02) | **G13** | 4 个 TD 项派生, 父未批则子全冻结 |
| **ADR-0074** Prompt Evidence Gate | 🔍 Proposed (2026-08-03) | (T21 前置) | 2-3 sprint 实施, 需 ADR-0071 获批 |

---

## 三、决策议程 (9 项)

### 议程 1: ADR-0083 IEvaluator/RewardSignal 契约 — **决策: 通过/拒绝/延期**

**Oracle 评审关键论点**:
- 22 项已 ship 能力中无任何"评估信号"能力
- GEPA/AFlow/fine-tune/行为克隆 6+ ADR 全部依赖
- 自进化方向的"电源"

**关键决策点**:

| # | 决策点 | 推荐 | 理由 |
|---|---|---|---|
| 1.1 | V1 范围 (TaskSuccessEvaluator 3 行实现 vs 完整 Composite) | **TaskSuccess + BehavioralEquivalence** | V1 简化避免 ADR-0057 重蹈"零实施无需新设计"覆辙 |
| 1.2 | RewardSignal 三态 (Excellent/Acceptable/Poor) vs 二态 | **三态** | 保留"成功但低效"中间带 |
| 1.3 | 与 ToolResult.ok 关系 | **正交** | success=true 但 quality=Poor 可同时存在 |
| 1.4 | 接口位置 (`include/agenticdsl/contract/` vs `evaluation/`) | **contract/** | 与现有契约层一致 |

**预期产出**: ADR-0083 ✅ Approved → 解锁 G10 → T15/T19/T21/T22 解除依赖

### 议程 2: ADR-0080 v1.2 amendment (D10 解耦) — **决策: 通过/拒绝/分拆**

**Oracle 评审关键论点**:
- D10 v1.1 依赖 ADR-0081 scrub hook → ADR-0081 推迟到 ADR-0082 → ADR-0082 已搁置
- 已 Approved 的 D10 数据契约被 Proposed 链锁住

**关键决策点**:

| # | 决策点 | 推荐 | 理由 |
|---|---|---|---|
| 2.1 | CaptureMode 三态 (Off/Online/Training) | **三态** | 提供"过渡路径"+ 保留生产安全 |
| 2.2 | Training 模式 CLI 标志 `--allow-training-capture` | **强制** | 三重保护 (CLI + 路径前缀 + WARNING) |
| 2.3 | 模式降级 (Online → Training) 行为 | **WARN + 降级** | 静默拒绝破坏可用性 |
| 2.4 | 是否拆 V1/V2 (V1 仅 Training, V2 完整 Online) | **拆** | 立即解锁蒸馏, V2 等 ADR-0081 |

**预期产出**: ADR-0080 v1.2 ✅ Approved → 解锁 G12 → B6 蒸馏方向可启动

### 议程 3: ADR-0061-13 蒸馏输出格式 — **决策: 通过/拒绝/合并到 ADR-0078**

**Oracle 评审关键论点**:
- ADR-0061 家族无蒸馏输出层契约
- 7 环闭环中最后 1 环断裂 (采集/数据集/评估/训练/服务/回归门/输出格式)

**关键决策点**:

| # | 决策点 | 推荐 | 理由 |
|---|---|---|---|
| 3.1 | 三文件分离 (trajectory/policy/meta) vs 单文件 | **三文件分离** | 训练管线消费 policy, 调试消费 trajectory |
| 3.2 | DistillationRecord 字段 (input/output/steps/reward/...) | **采纳 (含 reward 必填)** | 与 ADR-0083 RewardSignal 对齐 |
| 3.3 | 是否合并到 ADR-0078 (Fine-tune) | **独立** | ADR-0078 是外部阻塞 (AgenticMind), 不能拖 |
| 3.4 | 与 Trajectory IR (ADR-0061-06) 边界 | **正交** | IR 序列化轨迹, 本 ADR 加 ML 字段 |

**预期产出**: ADR-0061-13 ✅ Approved → 解锁 G15 → B6 蒸馏完整闭环

### 议程 4: ADR-0071 Promotion → Approved — **决策: 通过/拒绝/分拆**

**关键论点**:
- Oracle 评审: "ADR-0071 父未批, 4 个子项全冻结" — 本周最高杠杆动作之一
- ADR-0071 是顶层方向, 派生 6 个子 ADR/Change

**关键决策点**:

| # | 决策点 | 推荐 | 理由 |
|---|---|---|---|
| 4.1 | ADR-0071 整体 Approved vs 拆 6 个子 ADR 各自评审 | **整体 Approved** | 顶层方向已成熟, 6 个子项已 Proposed/Approved |
| 4.2 | 决策 D1-D9 是否全采纳 | **D1-D8 采纳, D9 独立 ADR** | Fine-tune 拆出 ADR-0078 (已存在) |
| 4.3 | 是否需要 ADR-0071 v2 amendment | **v1.1 amendment** | 整合 6 个子项状态 + Oracle 评审 §八 引用 |
| 4.4 | 父未批导致的下游冻结影响 | **本次评审后解锁** | 4 个 TD 项立即可启动 |

**预期产出**: ADR-0071 ✅ Approved → 解锁 G13 → 4 个子项命运确定

### 议程 5: ADR-0074 Promotion → Approved — **决策: 通过/拒绝/分拆**

**关键论点**:
- ADR-0074 派生自 ADR-0071 §D5 (Prompt Engineering + Evidence Gate)
- 2-3 sprint 实施
- 衔接 ADR-0073 (Tool JSON Schema)

**关键决策点**:

| # | 决策点 | 推荐 | 理由 |
|---|---|---|---|
| 5.1 | D1-D7 是否全采纳 | **D1-D6 采纳, D7 拆分** | D7 失败事件可走 ADR-0068 附录 A |
| 5.2 | 训练数据格式 (D6 JSONL schema_snapshot_hash) | **采纳** | 与 ADR-0061-13 DistillationRecord 对齐 |
| 5.3 | Evidence Gate 与 IEvaluator (ADR-0083) 关系 | **ADR-0083 主管评估, ADR-0074 主管证据** | 职责分明 |

**预期产出**: ADR-0074 ✅ Approved → 解锁 T21 前置 → 自进化证据门基础

### 议程 6: ADR-0061-06 v1.1 amendment (Trajectory IR 标题修订) — **决策: 通过/拒绝/拆分**

**Oracle S4.3 关键论点**:
- G14 (Trajectory IR 标题耦合风险) 是 Oracle 评审识别的隐藏架构层缺口
- ADR-0061-06 v1 原标题 "升级 ParsedGraph" 会把训练数据格式耦合进运行时图结构
- T15 启动的前置闸门:Sprint 25 排期已假设 T15 开始

**关键决策点**:

| # | 决策点 | 推荐 | 理由 |
|---|---|---|---|
| 6.1 | v1.1 标题 (Trajectory IR 独立序列化视图 vs 升级 ParsedGraph) | **独立序列化视图** | 训练/运行时解耦 |
| 6.2 | Trajectory IR 与 ADR-0083 ExecutionTrace 边界 | **独立** ExecutionTrace 由 ADR-0083 实施者定义最小版本 | 解除 ADR-0083 → T15 循环依赖 |
| 6.3 | 是否拆 V1/V2 (V1 仅训练序列化, V2 集成运行时) | **拆 V1 立即解锁 T15** | V2 等 ADR-0081 决议 |
| 6.4 | 是否合并到 ADR-0061-06 v2 重写 | **独立 amendment** | 保持版本演进清晰 |

**预期产出**: ADR-0061-06 v1.1 ✅ Approved → 解锁 G14 → T15 闸门开

### 议程 7: 派生 4 个 TD 项命运

| TD 项 | 前置 (本会议后) | 估时 | 优先级 | 启动时机 |
|---|---|---|---|---|
| **T17** SkillCompiler (ADR-0061-03) | ADR-0071 ✅ | 2 sprint | P1 | 下个 Sprint |
| **T19** GEPA 反思循环 MVP (ADR-0061-09) | ADR-0083 ✅ + ADR-0071 ✅ | 2-3 sprint (R 轨 spike) | P1 | 评审后 1 sprint |
| **T20** AFlow MCTS (ADR-0061-08) | ADR-0083 ✅ + T15 ✅ | 1-2 月 (R 轨 spike) | P2 | 评审后 2 sprint |
| **T21** Prompt Evidence Gate (ADR-0074) | ADR-0074 ✅ + ADR-0083 ✅ | 1 月 | P1 | 评审后 1 sprint |

**预期产出**: 4 个 TD 项命运表 → 加入 master plan / active-status.md

### 议程 8: 数据面 / 评估契约 / 训练管线 协同

**关键决策点**:

| # | 决策点 | 推荐 | 理由 |
|---|---|---|---|
| 8.1 | D10 Training 模式输出 → DistillationRecord.input 桥接 | **ADR-0080 v1.2 + ADR-0061-13 联合实现** | 避免中间格式 |
| 8.2 | IEvaluator (ADR-0083) 调用时机 | **TaskSuccess 在 CognitiveWorker 任务完成时, BehavioralEquivalence 在 T15 Trajectory IR 集成时** | 职责分明 |
| 8.3 | 蒸馏数据质控 (PII scrub, 凭据脱敏) | **Training 模式 CLI 标志 + meta.json.capture_mode 标记 + V2 ADR-0081 集成** | 三层保护 |

**预期产出**: 3 项协同决议 → 写入 §五 集成架构图

### 议程 9: 时间表与责任分工

**预期产出**: Sprint 23-24 任务排期表

| Sprint | 启动任务 | ship 目标 |
|---|---|---|
| Sprint 23 (当前) | A1-A4 ✅ ship + B1 评审会筹备 | T14 + 3 ADR 草案 |
| Sprint 24 启动周 | B1 评审会通过后, 启动 T17 (SkillCompiler) | ADR-0061-03 实施启动 |
| Sprint 24-25 | ADR-0083 实施 (1 sprint) + ADR-0061-13 实施 (1 sprint 并行) | IEvaluator + IDistillationWriter ship |
| Sprint 25-26 | T15 (Trajectory IR) + ADR-0071 v1.1 amendment + ADR-0080 v1.2 ship | 自进化方向基础设施完整 |

---

## 四、会议输出物 (Deliverables)

1. ✅ 6 个 ADR 决策 (Approved/Rejected/Deferred + 决议理由;含 G14 0061-06 v1.1 amendment)
2. ✅ 4 个 TD 项启动时间表 (T17/T19/T20/T21)
3. ✅ 协同决议记录 (3 项: D10/IEvaluator/质控)
4. ✅ Sprint 23-26 排期表
5. ✅ 评审会议纪要 (本文档后续将更新)

---

## 五、集成架构图 (评审用)

```
┌─────────────────────────────────────────────────────────────┐
│         自进化方向数据流 (评审后定稿)                          │
└─────────────────────────────────────────────────────────────┘

EventLog (ADR-0080 D10.v1.2 Training 模式)
    ↓ prompt_text/response_text + causal_time
IDistillationWriter (ADR-0061-13)
    ↓ trajectory.jsonl + policy.jsonl + meta.json
IEvaluator (ADR-0083) ──── reward signal ────┐
    ↓ RewardSignal.quality                       │
DistillationRecord.reward (ADR-0061-13)        │
    ↓                                            │
Behavioral Regression (T14 ✅ ADR-0061-02) ────┤
    ↓ Hotelling T² 三值 Verdict                 │
    ↓                                            │
Student Model LoRA (ADR-0078 Fine-tune) ─────────┘
    ↓
imodel_router (ADR-0034 ✅)
    ↓
T17 SkillCompiler (生成新 skill)
    ↓
T19 GEPA 反思循环 / T20 AFlow MCTS
    ↓
T21 Prompt Evidence Gate (ADR-0074)
    ↓
CognitiveWorker (回放新配置)
    ↓
Behavioral Regression (再次验证 — 闭环)
```

---

## 六、风险与备选方案

| 风险 | 缓解 | 备选 |
|---|---|---|
| 评审会延期超过 2 周 | 提前 1 周发评审材料 | B1 筹备并行推进 T15/C1 等不依赖会议的任务 |
| 某 ADR 被拒绝 | 提供替代方案 (如 IEvaluator 可走 ADR-0023 扩展) | 推迟相关 TD 项 |
| ADR-0071 Promotion 失败 | v1.1 amendment 拆分决策 | 4 个子项独立评审 |
| Oracle session 引用的新发现 | 评审会议讨论 | 单独修订 |

---

## 七、会议前检查清单 (与会者)

- [ ] 通读本文档 + 6 个 ADR 草案 (约 70 分钟;含 0061-06 v1.1 amendment)
- [ ] 阅读 Oracle session 摘要: `ses_fcba5e477ffeG9wEBHVhU64J0o`
- [ ] 阅读 `capability-application-map-2026-08.md` §八
- [ ] 阅读 `pdk-chat-demo-distill-source-survey-2026-08.md`
- [ ] 准备 §三 决策点 (9 议程) 立场
- [ ] 准备 §六 风险评估

---

## 八、会议后动作

- [ ] 更新 6 个 ADR 状态 (Approved/Rejected/Deferred)
- [ ] 更新 `capability-application-map-2026-08.md` §二 (G10/G12/G15 → 🔓 或 ✅)
- [ ] 更新 §四/§八 任务表 (T17/T19/T20/T21 启动)
- [ ] 写入 `active-status.md` 看板上 (评审决议)
- [ ] 创建 Sprint 24 启动会议纪要
- [ ] 归档本文档到 `docs/architecture/adr-review-minutes/`

---

**会议纪要状态**: ✅ 材料就绪,待架构组会议召集
**下次更新**: 评审会议召开后 (添加 §决议记录 + §action items)
**关联文档**: 6 个 ADR (含 ADR-0061-06 v1.1 amendment) + `capability-application-map-2026-08.md` + Oracle session

**审批与维护**:
- 召集人: Architecture Working Group
- 评审对象: Architecture Group (架构组全员)
- 周期: 一次性 (决议后归档)
- 关联: 评审通过后 → Sprint 24 启动会议