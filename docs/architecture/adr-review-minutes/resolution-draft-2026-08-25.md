# ADR-0071/0074 评审会议决议草案 (Oracle 预审版)

> **文件位置**: `docs/architecture/adr-review-minutes/resolution-draft-2026-08-25.md`
> **用途**: 会议召集前预审材料 — 模拟"如果会议通过"决议，供会议快速确认或修订
> **关联**: `meeting-notification-template.md` + `adr-0071-0074-distillation-review-2026-08-24.md`
> **创建日期**: 2026-08-25
> **草案基础**: Oracle 评审 ses_fcba5e477ffeG9wEBHVhU64J0o §八 关键发现

---

## ⚠️ 使用说明

本文件是 **决议草案** 而非正式决议。架构组会议可:
- ✅ **全部采纳** — 直接将本文件内容更新到 5 个 ADR 状态字段 + capability-application-map
- ✏️ **部分修订** — 在"会议修订"列记录每项修订内容
- ❌ **拒绝某项** — 在"拒绝理由"列填写依据，触发备选方案

会议主席填写 §八"会议决议记录"后，本草案转为正式纪要附件。

---

## 一、5 个 ADR 决议草案

### 决议 1: ADR-0083 IEvaluator/RewardSignal 契约

| 字段 | 草案值 | 依据 |
|---|---|---|
| **会议决议** | ✅ Approved | Oracle 评审 G10 关键缺口；22 项已 ship 能力无评估信号 |
| **状态变更** | 🔍 Proposed → ✅ Approved | 通过 |
| **版本** | v1.0 | 草案即终版 |
| **下游解锁** | T15/T19/T21/T22 (解除 IEvaluator 依赖) | 4 个 TD 项解除阻塞 |
| **Gap 闭合** | G10 🔴 → ✅ Closed | Oracle 评审标记最高优先级 |
| **实施启动** | Sprint 24 (1 sprint) | `IEvaluator` 接口 + 3 行 TaskSuccess 实现 |
| **会议修订** | (待会议填写) | |

**若被拒绝备选**:
- 选项 A: 推迟到 Sprint 26 (与 ADR-0023 扩展合并评审)
- 选项 B: 拆分 — 仅 `TaskSuccessEvaluator` 部分先批
- 选项 C: 合并到 ADR-0071 §D5 重写

---

### 决议 2: ADR-0080 v1.2 amendment (D10 解耦)

| 字段 | 草案值 | 依据 |
|---|---|---|
| **会议决议** | ✅ Approved | 解锁 G12 死锁；CaptureMode 三态解决训练数据面阻塞 |
| **状态变更** | (v1.1 父 ADR-0080 仍 Approved) → +v1.2 amendment | 通过 |
| **关键设计** | CaptureMode = Off / Online / Training | 草案 §决策 1 |
| **三重保护** | CLI `--allow-training-capture` + 路径前缀 + WARNING | 草案 §决策 2 |
| **下游解锁** | B6 蒸馏数据面 (采集→训练→评估→输出) | Oracle §8.2 闭环 1 第 1 环 |
| **Gap 闭合** | G12 🔴 → ✅ Closed | 已 Approved 契约被 Proposed 链锁住 → 解锁 |
| **实施启动** | 立即 (P0, 0.5 sprint) | 草案已 ship ready |
| **会议修订** | (待会议填写) | |

**若被拒绝备选**:
- 选项 A: 用 pdk_chat_demo SessionManager JSONL 临时绕过 (调研报告 B3 已验证可行)
- 选项 B: 等 ADR-0081 + 0082 (估计延期 2-4 月)

---

### 决议 3: ADR-0061-13 蒸馏输出格式

| 字段 | 草案值 | 依据 |
|---|---|---|
| **会议决议** | ✅ Approved | 解锁 G15；7 环闭环最后 1 环 |
| **状态变更** | 🔍 Proposed → ✅ Approved | 通过 |
| **关键设计** | `IDistillationWriter` 接口 + `DistillationRecord` (含 reward 必填) | 草案 §决策 |
| **三文件分离** | trajectory.jsonl / policy.jsonl / meta.json | 训练管线消费 policy，调试消费 trajectory |
| **下游解锁** | B6 蒸馏完整闭环 (7 环全闭合) | 闭环 1 状态: 4 环断裂 → 1 环断裂 (pdk_chat_demo JSONL 临时源) |
| **Gap 闭合** | G15 🔴 → ✅ Closed | 行为克隆器蒸馏输出格式 |
| **实施启动** | Sprint 25 (1 sprint 与 ADR-0083 并行) | 与 ADR-0083 实施并行 |
| **会议修订** | (待会议填写) | |

**若被拒绝备选**:
- 选项 A: 合并到 ADR-0078 (Fine-tune) — 但 ADR-0078 外部阻塞中
- 选项 B: 推迟到 ADR-0078 解除阻塞后 — 延迟 ≥4 周

---

### 决议 4: ADR-0071 Promotion → Approved

| 字段 | 草案值 | 依据 |
|---|---|---|
| **会议决议** | ✅ Approved | Oracle 评审 G13 关键缺口；4 个 TD 项派生冻结中 |
| **状态变更** | 🔍 Proposed (2026-08-02) → ✅ Approved | 通过 |
| **关键设计** | 顶层方向 LLM-native AgenticDSL；3 平面 Operator/DSL/Backend | ADR-0071 §决策 |
| **子项状态** | D1-D8 采纳；D9 拆为 ADR-0078 (已存在) | 草案 §决策 4.2 |
| **下游解锁** | T17 SkillCompiler + 全部派生 TD 项 | 4 个 TD 项命运确定 |
| **Gap 闭合** | G13 🔴 → ✅ Closed | 父未批冻结解除 |
| **是否需要 amendment** | 需要 v1.1 amendment (整合 6 子项状态 + Oracle §八) | 草案 §决策 4.3 |
| **实施启动** | v1.1 amendment 0.5 sprint + 6 子项各自 ship 排期 | 评审后立即启动 |
| **会议修订** | (待会议填写) | |

**若被拒绝备选**:
- 选项 A: 拆分 — D1-D8 各自独立评审 (估时 6 sprint)
- 选项 B: 维持 Proposed 状态 + 4 个 TD 项独立评审 (延迟 ≥4 sprint)

---

### 决议 5: ADR-0074 Prompt Evidence Gate

| 字段 | 草案值 | 依据 |
|---|---|---|
| **会议决议** | ✅ Approved | T21 前置；与 ADR-0071 协同形成完整证据门 |
| **状态变更** | 🔍 Proposed (2026-08-03) → ✅ Approved | 通过 |
| **关键设计** | D1-D6 采纳；D7 失败事件拆走 ADR-0068 附录 A | 草案 §决策 5.1 |
| **与 IEvaluator 边界** | ADR-0083 主管评估；ADR-0074 主管证据 | 职责分明 |
| **下游解锁** | T21 Prompt Evidence Gate 启动 (前置之一解除) | Sprint 25-26 |
| **Gap 闭合** | (非 Oracle 直接识别 Gap；T21 解锁间接) | — |
| **实施启动** | Sprint 25 (1 月估时) | 与 ADR-0083/0061-13 并行 |
| **会议修订** | (待会议填写) | |

**若被拒绝备选**:
- 选项 A: 仅 D1-D4 批，D5/D7 等更多数据积累
- 选项 B: 推迟到 ADR-0083 实施完成并验证后

---

## 二、4 个 TD 项命运表

| TD 项 | 前置 (本会议通过后) | 估时 | 优先级 | 启动 Sprint | 备注 |
|---|---|---|---|---|---|
| **T17** SkillCompiler (ADR-0061-03) | ADR-0071 ✅ | 2 sprint | P1 | **Sprint 24 启动周** | 等 ADR-0071 获批即可启动 |
| **T19** GEPA 反思循环 MVP (ADR-0061-09) | ADR-0083 ✅ + ADR-0071 ✅ | 2-3 sprint (R 轨 spike) | P1 | **Sprint 24 末** | R 轨 spike + promotion criteria |
| **T20** AFlow MCTS (ADR-0061-08) | ADR-0083 ✅ + T15 ✅ | 1-2 月 (R 轨 spike) | P2 | **Sprint 26 末** | R 轨 spike，不进 Sprint 承诺 |
| **T21** Prompt Evidence Gate (ADR-0074) | ADR-0074 ✅ + ADR-0083 ✅ | 1 月 | P1 | **Sprint 25 启动周** | 与 ADR-0083/0061-13 并行 |

---

## 三、3 项协同决议

### 协同决议 1: D10 Training → DistillationRecord.input 桥接

- **决议**: 由 ADR-0080 v1.2 + ADR-0061-13 联合实现
- **理由**: 避免中间格式；trajectory.jsonl 字段对齐 DistillationRecord.input
- **负责人**: T15 (Trajectory IR) 实施时同步推进
- **完成 Sprint**: 25

### 协同决议 2: IEvaluator 调用时机

- **决议**:
  - `TaskSuccess` 在 CognitiveWorker 任务完成时调用
  - `BehavioralEquivalence` 在 T15 Trajectory IR 集成时调用
- **理由**: 职责分明，不重复评估
- **负责人**: ADR-0083 实施者
- **完成 Sprint**: 25

### 协同决议 3: 蒸馏数据质控

- **决议**: 三层保护
  1. Training 模式 CLI 标志 `--allow-training-capture`
  2. meta.json.capture_mode 字段标记
  3. V2 集成 ADR-0081 scrub hook (待 ADR-0081 解锁)
- **理由**: PII scrub + 凭据脱敏 + 训练模式可观测
- **负责人**: ADR-0080 v1.2 实施者
- **完成 Sprint**: 24

---

## 四、Sprint 23-26 排期表

| Sprint | 周次 | 启动任务 | ship 目标 |
|---|---|---|---|
| **Sprint 23** (当前) | 2026-08-18 → 2026-08-31 | T14 行为回归 ✅ ship + T16 SLM ✅ ship + 3 ADR 草案 + B1 评审会议筹备 | (已完成) |
| **Sprint 24** | 2026-09-01 → 2026-09-14 | **B1 评审会议 (本周中) + ADR-0071 v1.1 amendment** + **T17 SkillCompiler 启动** + ADR-0080 v1.2 ship + ADR-0083 实施启动 | ADR-0071 ✅ Approved + ADR-0080 v1.2 ✅ + T17 骨架 |
| **Sprint 25** | 2026-09-15 → 2026-09-28 | **T17 SkillCompiler 核心编译逻辑** + **ADR-0083 IEvaluator ship** + **ADR-0061-13 蒸馏输出 ship** + **T15 Trajectory IR 启动** + ADR-0074 实施启动 | T17 完整 ship + T15 骨架 + 3 个新 ADR ship |
| **Sprint 26** | 2026-09-29 → 2026-10-12 | **T15 Trajectory IR 完整** + **T21 Prompt Evidence Gate** + T19 GEPA R 轨 spike 启动 + T20 AFlow spike 准备 | 自进化方向基础设施完整 (G10-G15 全闭合) |

---

## 五、决议落地检查表 (会议后 24 小时内执行)

会议主席/秘书:
- [ ] 更新 ADR-0083 状态字段 `🔍 Proposed` → `✅ Approved` + 添加"评审通过 (2026-08-XX)"
- [ ] 更新 ADR-0080 v1.2 amendment 状态 → `✅ Approved`
- [ ] 更新 ADR-0061-13 状态 → `✅ Approved`
- [ ] 更新 ADR-0071 状态 → `✅ Approved` + 起草 v1.1 amendment
- [ ] 更新 ADR-0074 状态 → `✅ Approved`
- [ ] 更新 `capability-application-map-2026-08.md` §二 G10/G12/G13/G15 状态 🔴 → ✅
- [ ] 更新 §八 T17/T19/T20/T21 启动 Sprint 标注
- [ ] 更新 §四/§八 任务表"待评审会议"→"已批准, Sprint XX 启动"
- [ ] 更新 `docs/architecture/README.md` §四 待决策清单 → 全部移除或转为 ✅
- [ ] 更新 `docs/adr/adr-0061-04-slm-routing.md` 状态 (已 archive)
- [ ] 创建 Sprint 24 启动会议纪要
- [ ] 更新 `active-status.md` 看板 (Phase 6 自进化方向进展)
- [ ] 通知 SLM/蒸馏团队新决议
- [ ] 归档本决议草案到 `docs/architecture/adr-review-minutes/`

---

## 六、若会议拒绝某 ADR 的应急路径

| 拒绝 ADR | 应急路径 | 估时影响 |
|---|---|---|
| ADR-0083 | 等 ADR-0023 扩展合并评审 | T15/T19/T21/T22 延迟 1 sprint |
| ADR-0080 v1.2 | 用 pdk_chat_demo SessionManager JSONL 临时绕过 | B6 蒸馏延迟 2 sprint (但仍可启动) |
| ADR-0061-13 | 推迟到 ADR-0078 解除阻塞后 | B6 蒸馏输出层延迟 ≥4 sprint |
| ADR-0071 | 6 个子项各自独立评审 | T17/T19/T20/T21 延迟 ≥4 sprint |
| ADR-0074 | 仅 D1-D4 批，D5/D7 等数据积累 | T21 延迟 1-2 sprint |

---

## 七、若会议延期 (超过 2 周)

- **触发条件**: 会议未在 2026-09-07 前召开
- **应急**:
  - Sprint 24 启动时使用本草案作为"暂定决议"，T17 启动延后到 Sprint 24 末
  - ADR-0083/0061-13 标记为"待评审默认通过" (基于 Oracle 评审结论)
  - ADR-0071 v1.1 amendment 推迟到 Sprint 25

---

## 八、会议决议记录 (会议主席填写)

> **填写人**: <架构组主席姓名>
> **会议日期**: <YYYY-MM-DD>
> **会议地点**: <会议室>
> **与会者**: <签名列表>

### 8.1 ADR 决议记录

| ADR | 草案决议 | 会议决议 | 修订内容 (如有) | 拒绝理由 (如拒绝) |
|---|---|---|---|---|
| ADR-0083 | ✅ Approved | (填写) | (填写) | (填写) |
| ADR-0080 v1.2 | ✅ Approved | (填写) | (填写) | (填写) |
| ADR-0061-13 | ✅ Approved | (填写) | (填写) | (填写) |
| ADR-0071 | ✅ Approved | (填写) | (填写) | (填写) |
| ADR-0074 | ✅ Approved | (填写) | (填写) | (填写) |

### 8.2 TD 项命运决议

| TD 项 | 草案命运 | 会议决议 | 修订内容 |
|---|---|---|---|
| T17 | Sprint 24 启动周 | (填写) | (填写) |
| T19 | Sprint 24 末 (R 轨) | (填写) | (填写) |
| T20 | Sprint 26 末 (R 轨) | (填写) | (填写) |
| T21 | Sprint 25 启动周 | (填写) | (填写) |

### 8.3 协同决议记录

| 协同项 | 草案决议 | 会议决议 | 修订内容 |
|---|---|---|---|
| 1. D10 → DistillationRecord 桥接 | ADR-0080 v1.2 + 0061-13 联合 | (填写) | (填写) |
| 2. IEvaluator 调用时机 | TaskSuccess/BehavioralEquivalence 分工 | (填写) | (填写) |
| 3. 蒸馏数据质控 | 三层保护 (CLI + meta + V2 scrub) | (填写) | (填写) |

### 8.4 Sprint 23-26 排期表决议

(会议如调整排期，记录在此)

### 8.5 行动项 (Action Items)

| # | 行动 | 负责人 | 截止 Sprint | 状态 |
|---|---|---|---|---|
| 1 | (会议填写) | | | |
| 2 | (会议填写) | | | |
| 3 | (会议填写) | | | |

---

## 九、关联文档

- **会议纪要**: `adr-0071-0074-distillation-review-2026-08-24.md` (251 行议程 + 决策点 + 风险评估)
- **召集模板**: `meeting-notification-template.md`
- **Oracle 评审**: `capability-application-map-2026-08.md` §八 (Oracle session ses_fcba5e477ffeG9wEBHVhU64J0o)
- **调研报告**: `pdk-chat-demo-distill-source-survey-2026-08.md` (B3)
- **Capability Map**: `capability-application-map-2026-08.md` (v1.2)
- **5 个 ADR 草案**:
  - `docs/adr/adr-0083-evaluator-reward-contract.md`
  - `docs/adr/adr-0080-v1-2-amendment-d10-decouple.md`
  - `docs/adr/skill/adr-0061-13-distillation-output-format.md`
  - `docs/adr/adr-0071-llm-native-agenticdsl-architecture.md`
  - `docs/adr/adr-0074-prompt-evidence-gate.md`

---

**审批与维护**:
- 草案创建: 2026-08-25 (基于 Oracle 评审 + 议程决策点)
- 维护者: Architecture Working Group
- 关联: 评审会议召集前预审材料，会议通过后转为正式附件
- 评审后归档: `docs/architecture/adr-review-minutes/`