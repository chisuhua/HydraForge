# ADR-0084: Mutation Governance 契约 (变异治理 / 授权契约)

**日期**: 2026-08-26
**父主题**: Phase 6 Agent 自进化方向 / cap-map §二 Gap G11

## 状态

🔍 Proposed — 起草启动，方案骨架已确定，待 6 维度契约细节完善 + 代码 ship + 评审转 Approved

> **状态说明**:
> 本 ADR 文件于 2026-08-26 创建 (Sprint 25 W1 起草触发器),承接 GitHub issue #14 中已
> 通过 Self-Review 的 6 维度契约骨架 (12 项通用 + 4 项专用清单全部 ✅)。
>
> **Approved 判定**: 待 6 维度契约 (变异对象分级 / 授权绑定 / 治理流程 / 审计轨迹 / 失败回滚 / 攻击面约束)
> 实现代码 ship + `tests/test_mutation_governance.cpp` ≥ 6 cases 通过 + 185/185 ctest 零回归
> + ADR-0068 amendment 注册 `mutation.{proposed,committed,reverted,denied}` 4 主题 +
> Sprint 26 末架构组评审通过后，从 🔍 Proposed 翻转为 ✅ Approved。

**前置文档**:
- `docs/architecture/capability-application-map-2026-08.md` §二 G11 + §八 R 轨任务
- GitHub issue [#14](https://github.com/chisuhua/HydraForge/issues/14) — G11 Self-Review (12 项通用 + 4 项专用清单全部 ✅)
- [`docs/architecture/self-evolution-architecture-2026-08.md`](../architecture/self-evolution-architecture-2026-08.md) — 自进化架构边界
- [`docs/research/agent-distillation-sota-2026-08.md`](../research/agent-distillation-sota-2026-08.md) §四 — G11 起草要点
- Oracle session `ses_fc640ea84ffe0f4dyYTa4aFjiL` (战略评估, 2026-08-26)
- Oracle session `ses_fc41537bbffeC35NKqgvzn4m1c` (Self-Review 预审, 2026-08-26)
- Oracle session `ses_fc3e070c0ffeIVgAhsgx2pNXFa` (深度审查, 2026-08-26)
- Oracle session `ses_fc3090b49ffe7yJwXhx1MoNz5N` (架构文档审计, 2026-08-26)
- ADR-0004 ApprovalPolicy ✅ / ADR-0031 ExecutionPolicy ✅ / ADR-0061-02 行为回归 ✅ (T14 ship) /
 ADR-0068 EventBuilder 🔍 / ADR-0074 Prompt Evidence Gate ✅ / ADR-0079 Session 4-scope ✅ /
 ADR-0080 EventLog ✅ + v1.2 amendment ✅ / ADR-0081 Pre-Step Hook ✅ (2026-08-21) /
 ADR-0083 IEvaluator 🔍 (起草完成, 代码 ship 待办)

**关联任务**: T17 SkillCompiler (前置) / T19 GEPA Phase 2 commit (硬前置) / T20 AFlow / T22 Fine-tune
**关联 Sprint**: Sprint 25 W1 (起草) + Sprint 26 末 (评审)

---

## 背景

`capability-application-map-2026-08.md` §二 Gap **G11（变异治理/授权契约缺失）** 是 2026-08-25 Oracle 评审识别的
关键架构层缺口：所有 R 轨任务（T18 PASTE / T19 GEPA / T20 AFlow / T22 Fine-tune）的**硬前置**，因为这些任务的核心动作
（`commit(PromptEdit)` / 工作流改写 / 权重微调）本质都是 Agent 自修改，没有变异授权契约就无门禁。

22 项已 ship 能力中**没有任何一项回答"这一变异是否被授权"**——这是 Oracle 评审 (`ses_fcba5e477ffeG9wEBHVhU64J0o`)
识别的未识别架构层缺口。

---

## 决策

### 决策 1 — 变异对象分级（4 级权限收紧）

| 等级 | 对象 | 关联 ADR | 权限收紧 | V1 状态 |
|---|---|---|---|---|
| **L1** | prompt 资产 | ADR-0074 Prompt Evidence Gate ✅ | **yolo 及以上** | ✅ V1 支持 |
| **L2** | DSL 图 | ADR-0061-06 Trajectory IR ✅ (v1.1 amendment) | 仅 plan + agent | ✅ V1 支持 |
| **L3** | SKILL.md | ADR-0061-03 SkillCompiler ✅ | 仅 plan + agent + 人类复核 | ✅ V1 支持 |
| **L4** | 权重 | ADR-0078 Fine-tune 🔍 | **V1 显式禁止自动变异** | ⛔ V1 边界外 |

**V1 边界 = ADR-0078 ship + 转 Approved 之前** (Oracle ⚠ 标注)；L4 变异在 ADR-0078 通过前不予支持。

### 决策 2 — 授权绑定（复用 ExecutionPolicy）

复用 ADR-0004 ApprovalPolicy ✅ + ADR-0031 ExecutionPolicy 🟡 (仅依赖 C3 已 ship 部分)：

| 模式 | L1 | L2 | L3 | L4 |
|---|---|---|---|---|
| **yolo** | ✅ | ⛔ | ⛔ | ⛔ |
| **plan** | ✅ | ✅ | ⛔ | ⛔ |
| **agent** | ✅ | ✅ | ✅ (人类复核) | ⛔ |
| (L4 模式) | — | — | — | ⛔ V1 不支持 |

### 决策 3 — 治理流程

```
propose → IEvaluator 评估 (ADR-0083) → 行为回归门 (ADR-0061-02) → commit → 版本固定 (ADR-0074)
```

**每步失败 → 写审计事件 (`mutation.denied`) → 不 commit**。任何中间步骤失败都必须可定位原因并产生可重放的拒绝事件。

### 决策 4 — 审计轨迹（复用 ADR-0080 + ADR-0068 amendment）

复用 ADR-0080 Append-Only Event Log ✅ + D10 CaptureMode 三态，注册 4 个新主题：

| 主题 | 触发时机 | Payload 字段 |
|---|---|---|
| `mutation.proposed` | 候选提交时 | proposed_change 摘要 + subject_version + parent_version |
| `mutation.committed` | 通过门禁 commit 后 | version_id + evaluation_refs + mutation_kind |
| `mutation.reverted` | 回滚触发 | rollback_reason + target_version |
| `mutation.denied` | 任一中间步骤失败 | denial_reason + failed_step + subject_version |

**§实施 任务**: ADR-0068 amendment 注册上述 4 主题 (已纳入 `adr-implementation-status-gap-analysis.md` §四 14→18 主题清单)。

### 决策 5 — 失败回滚

- **L1/L2/L3**: 版本钉住 (pinning) + session fork (ADR-0079 v1.1 ✅)
- **L4**: V1 显式禁止自动变异，不需回滚机制
- **跨等级通用**: `mutation.committed` 后默认 24h 保留窗口，期间可触发 `mutation.reverted`

### 决策 6 — 攻击面约束

- **变异来源白名单**：仅 R 轨任务上下文 (T19/T20/T22 任务调度器注入)
- **禁止触发源**: 外部用户输入 / 工具返回 / LLM 输出 / Trajectory 抽取产物 **永不可**触发自修改
- **轨迹数据投毒缓解**: R 轨反思输入必须经过 ADR-0061-06 v1.1 轨迹 schema 校验；变异来源白名单 + 轨迹 schema 校验双重保护
- **默认 fail-closed**: 任何未明确授权的变异 = 拒绝

---

## 不变量

1. **任何 L1-L3 变异必须通过 ADR-0061-02 行为回归门**：未通过 = `mutation.denied`，不写 `mutation.committed`
2. **任何 L1-L3 变异必须写 ADR-0080 审计事件**：`proposed` → `committed`/`denied`/`reverted` 必须成对或显式 `denied`
3. **L4 变异在 V1 边界外**：代码层显式拒绝 `commit(WeightMutation)` 调用，抛出明确异常
4. **变异来源必属白名单**：R 轨任务调度器注入的 `mutation_context` 必须包含 `whitelisted_source_id`
5. **IEvaluator 评估结果必须可解释**：`mutation.committed` 事件必须关联 `evaluation_refs` 字段
6. **离线训练期 vs serving 路径隔离**：在线教师蒸馏的 KL 散度约束仅适用于训练期 (ADR-0061-04 SLM-routing-first 哲学 + IBudgetController 强制约束)

---

## 风险

| 风险 | 影响 | 缓解 |
|---|---|---|
| V1 仅支持 L1-L3 变异，L4 显式禁止 → 蒸馏/微调路径暂时不可用 | T22 Fine-tune 阻塞 | ADR-0078 独立推动；V1 边界明确为 ADR-0078 Approved 之前 |
| 行为回归门 6 cases 仅覆盖等价性，**变异前后质量变化**未覆盖 | GEPA 反思效果可能失真 | 复用 ADR-0083 IEvaluator RewardSignal (代码 ship 待办) |
| R 轨任务调度器注入 `whitelisted_source_id` 可能被仿冒 | 攻击面 | 与 ADR-0004 ApprovalPolicy 共用 yolo/plan/agent 模式绑定 |
| **轨迹数据投毒 → 反思被劫持** (Oracle ⚠ 标注残留风险) | 候选变异被恶意生成 | 变异来源白名单 + ADR-0061-06 v1.1 轨迹 schema 校验双重保护 |
| ADR-0068 amendment 注册 4 主题未 ship | 审计事件 topic 缺失 | 与本 ADR 同步 ship；归入 `adr-implementation-status-gap-analysis.md` §四 |
| Scrub Hook 集成未落地 (Sprint 24+) → 变异事件 payload 默认 CaptureMode=Off | 敏感数据保护 | ADR-0081 ✅ (2026-08-21 ship) + Sprint 24+ 集成计划 |
| "教师即稳定器" KL 散度机制误用 → 与 SLM-routing-first 哲学冲突 | serving 路径大模型常驻 | 决策 6 + 决策 2 显式声明 "训练期限定" |
| G11 issue body 顶部声明保持 OPEN 直至本 ADR ship + Approved + G11 Closed | 文档状态语义风险 | 关闭条件：(1) 文件创建 ✅ (本次) + (2) 评审转 ✅ + (3) cap-map §二 G11 翻 ✅ + (4) audit trail |

---

## 备选方案

### 备选 A：仅 L1 prompt 等级（最小集）
- 仅支持 L1 prompt 变异；L2/L3/L4 推迟到 V2
- 优点：实现简单，1 sprint ship
- 缺点：GEPA 仅能改 prompt，无法触改 skill；自进化能力天花板低
- **采纳情况**：❌ 不采纳 (R 轨任务 T19/T20 需要 L2/L3 变异能力)

### 备选 B：中心化 Gate 服务 vs 分布式 Token（决策 2 实施）
- **采纳情况**：✅ 决策 2 选用**复用 ADR-0004 + ADR-0031** 模式 (等价于中心化 Gate + 分布式 token 混合)
- 理由：避免新增服务接口，复用既有 C3 已 ship 的 ApprovalPolicy/ExecutionPolicy 链

### 备选 C：multi-teacher 蒸馏（含本 ADR）
- **采纳情况**：❌ 不纳入本 ADR 范围
- 理由：与单编排器自进化闭环边界冲突 (见 [`self-evolution-architecture-2026-08.md`](../architecture/self-evolution-architecture-2026-08.md) §四.5)，需独立 ADR 提案

---

## 实施

### 文件 (待 Sprint 25 创建)

- `include/agenticdsl/contract/imutation_governance.h` (L1 契约层 — IMutationGovernor 接口 + MutationContext 值类型)
- `include/agenticdsl/types/mutation_record.h` (审计事件值类型)
- `src/common/governance/mutation_governor.cpp` (V1 L1-L3 实现 + L4 拒绝路径)
- `src/common/governance/mutation_topics.cpp` (4 mutation.* 主题注册 — 与 ADR-0068 amendment 同步)
- `tests/test_mutation_governance.cpp` (≥ 6 cases：L1/L2/L3 happy path + L4 拒绝 + 审计事件完整性 + fail-closed 行为)

### ADR-0068 amendment 任务 (与本 ADR 同步 ship)

注册 4 mutation.* 主题到 `docs/adr/adr-0068-event-emission-contract.md` 附录 A，
避免重演 7 幻影主题问题 (已纳入 `adr-implementation-status-gap-analysis.md` §四 14→18 主题清单)。

### 估时

- **本 ADR 起草 (本次)**: 0.5 sprint
- **代码 ship**: 1 sprint
- **总估时**: 2 sprint (Sprint 25 W1 起草 + Sprint 25 W2-W3 实施 + Sprint 26 末评审)
- **优先级**: P1 (Oracle 评审：自进化方向硬前置)

### 触发条件

- ✅ T17 SkillCompiler (Sprint 24 启动) — L3 变异对象前置
- ✅ ADR-0083 IEvaluator (起草完成, 代码 ship 待办) — 决策 3 评估环节前置
- ✅ ADR-0079 v1.1 Session 4-scope — 决策 5 回滚机制前置
- ✅ ADR-0081 ✅ (2026-08-21 ship) — Scrub Hook (Sprint 24+ 集成未落地)
- ⏳ T17 ship (Sprint 24+ 待) — 仅 T17 待 ship

---

## 关联变更

- `docs/architecture/capability-application-map-2026-08.md` §二 G11 — 状态 🔍 Proposed → 待评审通过后翻 ✅ Closed
- `docs/architecture/capability-application-map-2026-08.md` §八.3 — T19/T20/T22 任务前置条件
- `docs/architecture/capability-application-map-2026-08.md` §八.6 — 风险提示更新
- `docs/architecture/self-evolution-architecture-2026-08.md` §五/§七 — 引用本 ADR (本次同步)
- `docs/adr/adr-0068-event-emission-contract.md` — amendment 注册 4 mutation.* 主题
- 解锁顺序: ADR-0084 ship → T19 Phase 2 commit 启动 → B7 自进化基础应用解锁

---

## 参考

- GitHub issue [#14](https://github.com/chisuhua/HydraForge/issues/14) — G11 Self-Review (含 12 项通用 + 4 项专用清单 + 6 项修订)
- Oracle 评审:
  - 战略评估: session `ses_fc640ea84ffe0f4dyYTa4aFjiL` (2026-08-26)
  - Self-Review 预审: session `ses_fc41537bbffeC35NKqgvzn4m1c` (2026-08-26)
  - 深度审查: session `ses_fc3e070c0ffeIVgAhsgx2pNXFa` (2026-08-26)
  - 架构文档审计: session `ses_fc3090b49ffe7yJwXhx1MoNz5N` (2026-08-26)
- AgentAssay: arXiv:2603.02601 (Token-efficient verdict)
- RLHF reward modeling: Christiano et al. 2017
- ADR-0061-02 (行为回归已 ship, T14) - 治理流程核心门禁
- ADR-0004 ApprovalPolicy + ADR-0031 ExecutionPolicy - 授权模式复用源
- ADR-0080 + ADR-0080 v1.2 amendment - 审计轨迹复用源
- ADR-0081 Pre-Step Hook ✅ (2026-08-21 ship) - Scrub Hook 集成源
- ADR-0083 IEvaluator 🔍 (起草完成, 代码 ship 待办) - 评估环节前置
- ADR-0078 Fine-tune 🔍 - L4 变异 V1 边界定义