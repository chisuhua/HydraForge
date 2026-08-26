# adr-0084-mutation-governance-contract

## Why

ADR-0084 (`docs/adr/adr-0084-mutation-governance-contract.md`) 文件已创建于 2026-08-26 (🔍 Proposed),承接 GitHub issue #14 中已通过 Self-Review 的 6 维度契约骨架（12 项通用 + 4 项专用清单全部 ✅）。本 change 按 2026-08-26 Oracle 评审发现对契约范围做了关键收口：

**V1 = gate-and-audit contract only（门禁 + 审计契约）**:
- ❌ 不存储 subject 版本（无版本库）
- ❌ 不恢复 subject（无状态恢复逻辑）
- ❌ 不强制 24h 或任何时长保留窗口
- ❌ 不实现可操作的 revert recovery（`revert()` 为纯审计记录 API，实际恢复由调用方经 ADR-0079 session fork 负责）

**Oracle 评审关键发现**（`docs/architecture/capability-application-map-2026-08.md` §八 G11）：

- G11（变异治理/授权契约缺失）是 R 轨任务（T18 PASTE / T19 GEPA / T20 AFlow / T22 Fine-tune）的**硬前置**
- 这些任务的核心动作（`commit(PromptEdit)` / 工作流改写 / 权重微调）本质都是 Agent 自修改
- 没有变异授权契约就无门禁，无门禁就无审计，无审计就存在安全攻击面

**审计依据**:

- 22 项 ship 能力中**无变异治理契约**（`grep -r "mutation.governor\|MutationGovernor" include/ src/` 0 命中）
- T19 GEPA Phase 1 已采纳"只读反思约束"（不执行 `commit(PromptEdit)`）直至 G11 ADR Approved
- cap-map §八.6 风险提示："变异治理缺位，Agent 自修改无审计无授权" — 2026-08-26 issue #14 Approved 启动起草
- **EventBuilder 无主题注册 API**：全库 grep 确认不存在 `register_topic` / `TopicRegistry`，事件主题为纯文档注册（ADR-0068 附录 A），不存在 `mutation_topics.cpp` 注册编译单元

**前置依赖**（启动条件，按批准的执行顺序排列）:

| 依赖 | 状态 | 说明 |
|---|---|---|
| **IEvaluator (ADR-0083) ship** | 🔍 Proposed + OpenSpec `2026-08-26-ship-ievaluator-reward-contract` | **硬前置，必须先 ship** — MutationGovernor 构造时强制注入非空 IEvaluator（评估门），治理流程 propose→evaluate→回归门→commit 依赖其 RewardSignal |
| ADR-0004 ApprovalPolicy | ✅ Approved | yolo/plan/agent 模式复用源 |
| ADR-0031 ExecutionPolicy | 🟡 Partial (C3 已 ship 部分) | 决策模式绑定源；IApprovalHandler (`include/agenticdsl/policy/iapproval_handler.h`) 为 agent+L3 人类复核调用点 |
| ADR-0061-02 行为回归 | ✅ Approved (T14 ship) | 治理流程回归门前置 |
| ADR-0079 v1.1 Session 4-scope | ✅ Approved | revert 后实际恢复由调用方经 session fork 负责（非本契约职责） |
| ADR-0080 + v1.2 amendment | ✅ Approved | 决策 4 审计轨迹前置 |
| ADR-0081 Pre-Step Hook | ✅ Approved (2026-08-22 ship) | 决策 6 S 防护前置 |

**显式降级（非启动阻塞）**:

| 依赖 | 状态 | 说明 |
|---|---|---|
| T17 SkillCompiler (ADR-0061-03) | 🔄 Sprint 24 启动中 | **降级为后续 L3 producer 集成依赖** — 本 gate/audit 契约的 L3 用例使用合成 MutationContext 测试，不经过 SkillCompiler 产物；T17 ship 后由独立 producer-wiring change 接入 |

## What Changes

本 change 将 ADR-0084 6 维度契约从"ADR 起草"阶段推进到"V1 gate-and-audit 代码 ship"阶段（ADR 状态翻转与 G11/issue #14 关闭**不在**本 change 范围，见 Impact §范围外）：

- **新增契约类**:
  - `include/agenticdsl/contract/imutation_governance.h` — `class IMutationGovernor` 接口（`propose()` + `commit()` + `revert()` 三方法；V1 `revert()` 为**纯审计记录**）+ `MutationContext` 值类型（含 `source_id` / `mutation_kind` / `subject_ref` / `evaluation_refs` 等不透明标识字段）
  - `include/agenticdsl/types/mutation_record.h` — `struct MutationRecord`（4 mutation.* 主题 payload 值类型）

- **L3 语义统一**（Oracle 修正）:
  - plan + L3 → 拒绝，`denial_reason="plan_insufficient"`
  - agent + L3 → 调用 `IApprovalHandler::process_request(meta, ctx, preview)` 人类复核，**仅在返回 true 后**才继续评估/回归门禁；返回 false → `denial_reason="approval_denied"`；handler 未注入 → fail-closed `denial_reason="approval_handler_unavailable"`
  - 任意模式 + L4 → 先 emit `mutation.denied`（`denial_reason="l4_forbidden_v1"`）**后抛** `std::runtime_error`（emit-then-throw）

- **evaluation_refs 定义**（Oracle 修正）:
  - `evaluation_refs` 为**不透明 evaluation_id 字符串数组**，由 IEvaluator 契约层（ADR-0083 评估环节）产出/消费，governor 仅透传不解释
  - `commit()` 时 evaluation_refs 为空 → fail-closed `denial_reason="missing_evaluation_refs"`

- **4 个 mutation.* 事件主题（文档注册，per ADR-0068 amendment）**:
  - `mutation.proposed` / `mutation.committed` / `mutation.reverted` / `mutation.denied`
  - **仅 ADR-0068 附录 A 文档注册**（payload schema 表）；EventBuilder 无运行时主题注册 API，**不创建 `mutation_topics.cpp`**

- **新增 V1 实现** (per 决策 1 + 决策 2):
  - `src/common/governance/mutation_governor.cpp` — 白名单 fail-closed → L4 拒绝（emit-then-throw）→ 模式×等级矩阵 → agent+L3 IApprovalHandler → IEvaluator 评估门 → 行为回归门 → commit 的完整门禁链 + 4 mutation.* 主题 emit
  - 构造签名强制 `std::shared_ptr<IEvaluator>` 非空（nullptr 抛 `std::invalid_argument`，fail-fast）
  - 白名单经构造函数注入不可变 `std::unordered_set<std::string>`，默认空 = 全部拒绝

- **新增测试**:
  - `tests/test_mutation_governance.cpp` — ≥ 8 cases（L1 happy path / L2 yolo 拒绝 / L3 plan 拒绝 plan_insufficient / L3 agent 审批通过与拒绝 / L3 agent handler 缺失 fail-closed / L4 emit-then-throw / 审计事件顺序与 evaluation_refs 透传 / 白名单 fail-closed / IEvaluator 空构造 fail-fast / missing evaluation_refs），全部断言为可客观验证的 topic + payload 字段 + 发射顺序 + 返回/异常类型

- **ADR-0068 amendment（文档注册 only）**:
  - `docs/adr/adr-0068-event-emission-contract.md` 附录 A 登记 4 mutation.* 主题 payload schema

## Impact

- **影响范围**:
  - L1 编排层（cap-map L1）新增 IMutationGovernor 抽象 — 不破坏既有 CognitiveWorker/DomainWorkerPool
  - ADR-0068 附录 A 文档注册表扩展 14 → 18 主题（文档登记，无运行时代码注册）
  - 既有 ApprovalPolicy/ExecutionPolicy/IApprovalHandler 链复用，不新增授权层
  - `docs/architecture/adr-implementation-status-gap-analysis.md` §四 主题计数同步

- **V1 边界** (per ADR-0084 §决策 1):
  - ✅ L1 prompt 资产（ADR-0074 Prompt Evidence）
  - ✅ L2 DSL 图（ADR-0061-06 Trajectory IR）
  - ✅ L3 SKILL.md（ADR-0061-03 SkillCompiler，经 agent 模式 + IApprovalHandler 人类复核）
  - ⛔ L4 权重（ADR-0078 Fine-tune）**V1 显式禁止** — emit denied 后抛明确异常

- **范围外（本 change 明确不做）**:
  - ❌ ADR-0084 状态翻转 🔍→✅（留待 Sprint 26 末评审）
  - ❌ cap-map §二 G11 翻 ✅ Closed / §八 R 轨前置条件更新
  - ❌ GitHub issue #14 关闭（保持 OPEN，留 audit trail 直至评审通过）
  - ❌ T17 SkillCompiler producer 接线（后续独立 change）
  - ❌ L4 支持 / 可操作 revert recovery / 版本存储（V2 follow-up）
  - ❌ `docs/active-status.md` / `self-evolution-architecture-2026-08.md` 等共享架构文档更新

- **下游解锁**:
  - T19 GEPA MVP Phase 2 commit 启动（评审通过后）
  - T20 AFlow MCTS 工作流改写授权前置
  - T22 Fine-tune 事件驱动训练路径 + 治理契约前置（V2）
  - B7 自进化基础应用解锁

- **Breaking Changes**: 无（新增契约类 + 文档注册，不修改既有 API）

## ship gate 验证

- `python3 tools/adr_lint.py` 通过
- `ctest --output-on-failure -R test_mutation_governance` ≥ 8 cases PASS（全部断言可客观验证）
- `ctest --output-on-failure` 全量零回归（**基线 = 本 change 启动时 main 分支实测 ctest 计数**，禁止硬编码数字；新增测试数 = 本 change 净增）
- `grep -r "class IMutationGovernor" include/agenticdsl/contract/` 命中
- ADR-0068 附录 A 4 mutation.* 主题文档注册完整
- `openspec validate 2026-08-26-adr-0084-mutation-governance-contract --strict` 通过
- **不翻转 ADR-0084 状态、不关闭 issue #14**（留待评审 change）

## 关联文档

- ADR-0084-mutation-governance-contract.md
- ADR-0080-append-only-event-log.md + ADR-0080-v1-2-amendment-d10-decouple.md
- ADR-0068-event-emission-contract.md (amendment 文档注册 4 主题)
- ADR-0083-evaluator-reward-contract.md + OpenSpec `2026-08-26-ship-ievaluator-reward-contract`（硬前置）
- ADR-0004-toolregistry-security.md (ApprovalPolicy 复用)
- ADR-0031-execution-policy.md (ExecutionPolicy / IApprovalHandler 复用)
- ADR-0061-02-behavioral-regression.md (T14 行为回归门)
- ADR-0079-unified-session-4scope.md (fork 恢复由调用方负责)
- `docs/architecture/adr-implementation-status-gap-analysis.md` §四（主题计数同步）
- `docs/architecture/self-evolution-architecture-2026-08.md` §四.3/§五/§六/§七
- `docs/architecture/capability-application-map-2026-08.md` §二 G11 + §八.3-§八.6
- `docs/research/agent-distillation-sota-2026-08.md` §四 G11 起草要点
- GitHub issue #14（保持 OPEN）
