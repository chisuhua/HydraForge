# ADR-0084: Mutation Governance 契约 (变异治理 / 授权契约)

**日期**: 2026-08-26
**父主题**: Phase 6 Agent 自进化方向 / cap-map §二 Gap G11

## 状态

✅ Approved (评审通过 2026-08-26 — V1 gate-and-audit 代码 ship, commit `a2b2d52`)

> **Ship 证据 (2026-08-26)**:
> - `IMutationGovernor` 契约 + `MutationGovernor` gate-and-audit V1 实现完整 ship,
>   `tests/test_mutation_governance.cpp` **13 测试用例 / 139 assertions 全部 PASS**,
>   ctest **187/187 PASS 零回归** (基线 = ship 启动时 main 实测 186 + 净增 1 测试 target)。
> - 6 项决策全部落地 (§决策 1-6: gate-and-audit V1 范围 / 不存 subject 版本 / 无 24h 保留窗口 /
>   revert() 纯审计 API / 4 mutation.* 事件主题 / capability 门禁 + BudgetGate 前置)。
> - ADR-0068 附录 A v1.2.1 amendment 已登记 `mutation.{proposed,committed,reverted,denied}`
>   4 主题 (commit `cfc3532`)。
> - OpenSpec change `2026-08-26-adr-0084-mutation-governance-contract` 已 archive
>   (commits `0ed5604` / `cfc3532` / `a2b2d52` / `b4da196`)。
> - Oracle sessions: `ses_fc41537bbffeC35NKqgvzn4m1c` (Self-Review 预审) /
>   `ses_fc3e070c0ffeIVgAhsgx2pNXFa` (深度审查) / `ses_fc3090b49ffe7yJwXhx1MoNz5N`
>   (架构文档审计) / `ses_fc640ea84ffe0f4dyYTa4aFjiL` (战略评估)。
> - G11 ✅ Closed (cap-map §二/§三 B7/§八.5 同步), issue #14 已关闭 (2026-08-26)。

> **V1 代码 ship 完成 (2026-08-26)**: `IMutationGovernor` 契约 + `MutationGovernor`
> gate-and-audit 实现 + 13 测试用例全部落地 (OpenSpec change
> `2026-08-26-adr-0084-mutation-governance-contract`, commits `0ed5604` / `cfc3532` /
> `a2b2d52`; ctest 187/187 PASS 零回归, 基线 = ship 启动时 main 实测 186 + 净增 1 测试
> target)。ADR-0068 附录 A v1.2.1 amendment 同步 ship (4 mutation.* 主题注册 +
> `mutation.approved` 修正为 `mutation.reverted` + payload schema 对齐 design D4)。
> **状态已翻转为 ✅ Approved (2026-08-26)** — 评审翻转 / cap-map §二 G11 翻 ✅ Closed /
> issue #14 关闭均由 OpenSpec change `g11-closed-adr-0084-approved` 完成 (见 §关联变更范围说明)。

> **状态说明**:
> 本 ADR 文件于 2026-08-26 创建 (Sprint 25 W1 起草触发器),承接 GitHub issue #14 中已
> 通过 Self-Review 的 6 维度契约骨架 (12 项通用 + 4 项专用清单全部 ✅)。2026-08-26 Oracle
> 评审后修订: **V1 明确为 gate-and-audit contract only** — 不存储 subject 版本、不恢复
> subject、无 24h 保留窗口、无可操作 revert recovery (`revert()` 为纯审计记录 API)。
>
> **Approved 判定 (全部满足, 2026-08-26 翻转)**: V1 门禁 + 审计代码已 ship +
> `tests/test_mutation_governance.cpp` 全部用例通过 (13 cases / 139 assertions) +
> ctest 全量零回归 (187/187) + ADR-0068 附录 A 已登记
> `mutation.{proposed,committed,reverted,denied}` 4 主题 → 从 🔍 Proposed 翻转为 ✅ Approved。
> 硬前置: OpenSpec `2026-08-26-ship-ievaluator-reward-contract` (IEvaluator) ✅ 已 ship。

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
| **yolo** | ✅ | ⛔ `plan_required` | ⛔ `plan_required` | ⛔ `l4_forbidden_v1` |
| **plan** | ✅ | ✅ | ⛔ `plan_insufficient` | ⛔ `l4_forbidden_v1` |
| **agent** | ✅ | ✅ | ✅ (经 IApprovalHandler 人类复核) | ⛔ `l4_forbidden_v1` |

**L3 语义统一** (2026-08-26 Oracle 修正)：
- plan + L3 → 拒绝，`denial_reason="plan_insufficient"` + `failed_step="authorization"`，不进入复核
- agent + L3 → 调用 `IApprovalHandler::process_request(meta, ctx, preview)`
  (`include/agenticdsl/policy/iapproval_handler.h`)，**仅当返回 true** 才继续评估/回归门禁；
  返回 false → `denial_reason="approval_denied"`；handler 未注入 (nullptr) → fail-closed
  `denial_reason="approval_handler_unavailable"`

### 决策 3 — 治理流程

```
propose: 白名单 fail-closed → L4 拒绝 → 模式×等级矩阵 → agent+L3 IApprovalHandler 复核
         → IEvaluator 评估 (ADR-0083) → 行为回归门 (ADR-0061-02) → emit mutation.proposed
commit:  evaluation_refs 非空校验 → 版本固定 (ADR-0074) → emit mutation.committed
```

**审计事件顺序** (2026-08-26 Oracle 修正)：
- 每次 `propose()` / `commit()` / `revert()` 调用**恰好发射一个**终态 mutation.* 事件
- **每步失败 → 先写审计事件 (`mutation.denied` 含 denial_reason + failed_step) → 不 commit**；
  任何中间步骤失败都必须可定位原因并产生可重放的拒绝事件
- **L4 拒绝为 emit-then-throw**: 先 emit `mutation.denied` (`l4_forbidden_v1`)，**随后**才抛出
  `std::runtime_error`；测试通过"先捕获事件再 catch 异常"验证顺序

### 决策 4 — 审计轨迹（复用 ADR-0080 + ADR-0068 amendment 文档登记）

复用 ADR-0080 Append-Only Event Log ✅ + D10 CaptureMode 三态，登记 4 个新主题：

| 主题 | 触发时机 | Payload 字段 |
|---|---|---|
| `mutation.proposed` | 通过全部门禁后 | proposed_change 摘要 + subject_ref + parent_ref (均为调用方提供的不透明字符串) |
| `mutation.committed` | commit 后 | version_id + evaluation_refs + mutation_kind |
| `mutation.reverted` | revert() 调用 (audit-only) | rollback_reason + target_version (不透明标识；不触发任何恢复动作) |
| `mutation.denied` | 任一中间步骤失败 | denial_reason + failed_step + subject_ref |

**evaluation_refs 定义** (2026-08-26 Oracle 修正)：`std::vector<std::string>`，元素为**不透明
evaluation_id 字符串**，由 IEvaluator 契约层 (ADR-0083 评估环节) 产出与消费；governor 仅透传，
不解析、不生成、不验证格式。`commit()` 时 evaluation_refs 为空 → fail-closed
`denial_reason="missing_evaluation_refs"` + `failed_step="evaluation"`。

**§实施 任务**: ADR-0068 amendment 在附录 A **文档登记**上述 4 主题 payload schema (计数同步至
`docs/architecture/adr-implementation-status-gap-analysis.md` §四 14→18 主题清单)。
**注意**: EventBuilder 仅为 payload builder，无 `register_topic` / `TopicRegistry` API
(全库 grep 0 命中)，主题为文档注册 only，**不存在 `mutation_topics.cpp` 注册编译单元**。

### 决策 5 — 失败回滚 (V1: audit-only，2026-08-26 Oracle 修正)

- **V1 边界**: 本契约为 gate-and-audit only — **不存储 subject 版本、不恢复 subject、
  无 24h 或任何时长保留窗口、不实现可操作 revert recovery**
- `revert(target_version, rollback_reason)` 为**纯审计记录 API**：仅 emit `mutation.reverted`
  (rollback_reason + target_version，均为调用方提供的不透明标识)，不读取/不修改任何 subject 状态
- **实际恢复由调用方负责**：经 ADR-0079 v1.1 session fork 机制执行，不属于本契约范围
- **L4**: V1 显式禁止自动变异，不需回滚机制
- 可操作 revert recovery (含版本存储/保留窗口) 留待 V2 OpenSpec follow-up

### 决策 6 — 攻击面约束

- **变异来源白名单** (2026-08-26 Oracle 修正，内容/所有权/注入/fail-closed 显式化)：
  - **内容**：部署方声明的 source_id 字符串集合（如 `R_T19_GEPA` / `R_T20_AFLOW`），仅 R 轨任务上下文
  - **所有权**：编排应用 (R 轨任务 runner)，非 governor 自身
  - **注入**：构造函数参数 `std::unordered_set<std::string>`，**构造后不可变**（V1 无运行时增删 API）
  - **fail-closed**：source_id 缺失 / 空串 / 不在集合 → `denial_reason="non_whitelisted_source"` +
    `failed_step="source_whitelist"`，且**不执行任何后续门禁**；默认空白名单 = 全部拒绝
- **禁止触发源**: 外部用户输入 / 工具返回 / LLM 输出 / Trajectory 抽取产物 **永不可**触发自修改
- **轨迹数据投毒缓解**: R 轨反思输入必须经过 ADR-0061-06 v1.1 轨迹 schema 校验；变异来源白名单 + 轨迹 schema 校验双重保护
- **默认 fail-closed**: 任何未明确授权的变异 = 拒绝

---

## 不变量

1. **任何 L1-L3 变异必须通过 ADR-0061-02 行为回归门**：未通过 = `mutation.denied`，不写 `mutation.committed`
2. **任何 L1-L3 变异必须写 ADR-0080 审计事件**：`proposed` → `committed`/`denied`/`reverted` 必须成对或显式 `denied`
3. **L4 变异在 V1 边界外**：代码层 emit-then-throw — 先 emit `mutation.denied` (`l4_forbidden_v1`) 再抛 `std::runtime_error`
4. **变异来源必属白名单**：R 轨任务调度器注入的 `MutationContext` 必须含白名单内 `source_id`；白名单构造注入不可变，默认空 = 全部拒绝
5. **IEvaluator 评估结果必须可追溯**：`mutation.committed` 事件必须关联非空 `evaluation_refs`（不透明 evaluation_id 字符串数组，由 IEvaluator 契约层产出）
6. **离线训练期 vs serving 路径隔离**：在线教师蒸馏的 KL 散度约束仅适用于训练期 (ADR-0061-04 SLM-routing-first 哲学 + IBudgetController 强制约束)

---

## 风险

| 风险 | 影响 | 缓解 |
|---|---|---|
| V1 仅支持 L1-L3 变异，L4 显式禁止 → 蒸馏/微调路径暂时不可用 | T22 Fine-tune 阻塞 | ADR-0078 独立推动；V1 边界明确为 ADR-0078 Approved 之前 |
| 行为回归门 6 cases 仅覆盖等价性，**变异前后质量变化**未覆盖 | GEPA 反思效果可能失真 | 复用 ADR-0083 IEvaluator RewardSignal (代码 ship 待办) |
| R 轨任务调度器注入 `source_id` 可能被仿冒 | 攻击面 | 白名单构造注入不可变 + 默认空 fail-closed + 与 ADR-0004 ApprovalPolicy 共用 yolo/plan/agent 模式绑定 |
| **轨迹数据投毒 → 反思被劫持** (Oracle ⚠ 标注残留风险) | 候选变异被恶意生成 | 变异来源白名单 + ADR-0061-06 v1.1 轨迹 schema 校验双重保护 |
| ADR-0068 amendment 4 主题文档登记未 ship | 审计事件 topic 无文档登记 | 与本 ADR 同步 ship；归入 `docs/architecture/adr-implementation-status-gap-analysis.md` §四 |
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
- `include/agenticdsl/types/mutation_record.h` (审计事件值类型；标识字段均为不透明字符串，无 subject 存储)
- `src/common/governance/mutation_governor.cpp` (V1 L1-L3 gate-and-audit 实现 + L4 emit-then-throw 拒绝路径；构造强制注入非空 `std::shared_ptr<IEvaluator>` (nullptr 抛 `std::invalid_argument`) + 不可变白名单 + 可空 `IApprovalHandler*`)
- `tests/test_mutation_governance.cpp` (≥ 8 cases：L1 happy / L2 yolo 拒绝 / L3 plan `plan_insufficient` / L3 agent 审批通过与拒绝 / L3 handler 缺失 fail-closed / L4 emit-then-throw / 审计事件顺序 + evaluation_refs 透传 / 白名单 fail-closed / IEvaluator 空构造 fail-fast / missing evaluation_refs / revert audit-only)

> **移除项** (2026-08-26 Oracle 修正)：~~`src/common/governance/mutation_topics.cpp`~~ — EventBuilder
> 无主题注册 API，主题为 ADR-0068 附录 A 文档登记，无运行时代码注册编译单元。

### ADR-0068 amendment 任务 (与本 ADR 同步 ship)

在 `docs/adr/adr-0068-event-emission-contract.md` 附录 A **文档登记** 4 mutation.* 主题 payload schema，
避免重演 7 幻影主题问题 (计数同步至 `docs/architecture/adr-implementation-status-gap-analysis.md` §四 14→18 主题清单)。

### 估时

- **本 ADR 起草 (本次)**: 0.5 sprint
- **代码 ship**: 1 sprint
- **总估时**: 2 sprint (Sprint 25 W1 起草 + Sprint 25 W2-W3 实施 + Sprint 26 末评审)
- **优先级**: P1 (Oracle 评审：自进化方向硬前置)

### 触发条件

- ✅ ADR-0083 IEvaluator + OpenSpec `2026-08-26-ship-ievaluator-reward-contract` **ship** — **硬前置**（决策 3 评估门 + MutationGovernor 构造注入；用户批准的执行顺序：IEvaluator 先 ship，变异治理后启动）
- ✅ ADR-0079 v1.1 Session 4-scope — revert 后实际恢复由调用方经 session fork 负责（非本契约职责）
- ✅ ADR-0081 ✅ (2026-08-21 ship) — Scrub Hook (Sprint 24+ 集成未落地)
- ⤵️ T17 SkillCompiler — **降级为后续 L3 producer 集成依赖**（非启动阻塞）：本契约 L3 用例使用合成 MutationContext 测试；T17 ship 后由独立 producer-wiring change 接线

---

## 关联变更

> **范围说明** (2026-08-26 Oracle 修正)：本 ADR 的 V1 ship change 仅覆盖代码 + ADR-0068 附录 A 文档登记 +
> gap-analysis 计数同步。下列 cap-map / self-evolution-architecture / issue #14 关闭均为**评审通过后**
> 的后续步骤，不属于 V1 ship change 范围。

- `docs/architecture/capability-application-map-2026-08.md` §二 G11 — ✅ Closed (2026-08-26, 评审通过后翻转)
- `docs/architecture/capability-application-map-2026-08.md` §三 B7 行 — ✅ G11 Closed 同步 (2026-08-26)
- `docs/architecture/capability-application-map-2026-08.md` §八.5 排期表 — ✅ 完成日期标注 (2026-08-26)
- `docs/architecture/self-evolution-architecture-2026-08.md` §四.2 — ✅ G11 Closed 标注 (2026-08-26)
- `docs/adr/adr-0068-event-emission-contract.md` — ✅ amendment 附录 A 已登记 4 mutation.* 主题
- GitHub issue #14 — ✅ 已关闭 (2026-08-26, 含 audit trail)
- 解锁顺序: ADR-0084 ship + 评审 ✅ (2026-08-26) → T19 Phase 2 commit 已解锁 → B7 自进化基础应用解锁

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