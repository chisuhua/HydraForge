# adr-0084-mutation-governance-contract

## Why

ADR-0084 (`docs/adr/adr-0084-mutation-governance-contract.md`) 文件已创建于 2026-08-26 (🔍 Proposed),承接 GitHub issue #14 中已通过 Self-Review 的 6 维度契约骨架（12 项通用 + 4 项专用清单全部 ✅）。

**Oracle 评审关键发现**（`docs/architecture/capability-application-map-2026-08.md` §八 G11）：

- G11（变异治理/授权契约缺失）是 R 轨任务（T18 PASTE / T19 GEPA / T20 AFlow / T22 Fine-tune）的**硬前置**
- 这些任务的核心动作（`commit(PromptEdit)` / 工作流改写 / 权重微调）本质都是 Agent 自修改
- 没有变异授权契约就无门禁，无门禁就无审计，无审计就存在安全攻击面

**审计依据**:

- 22 项 ship 能力中**无变异治理契约**（`grep -r "mutation.governor\|MutationGovernor" include/ src/` 0 命中）
- T19 GEPA Phase 1 已采纳"只读反思约束"（不执行 `commit(PromptEdit)`）直至 G11 ADR Approved
- cap-map §八.6 风险提示："变异治理缺位，Agent 自修改无审计无授权" — 2026-08-26 issue #14 Approved 启动起草

**前置依赖**:

| 依赖 | 状态 | 说明 |
|---|---|---|
| T17 SkillCompiler (ADR-0061-03) | 🔄 Sprint 24 启动中 | L3 变异对象生成器前置 |
| IEvaluator (ADR-0083) | 🔍 Proposed + OpenSpec `2026-08-26-ship-ievaluator-reward-contract` 启动 | 治理流程评估门前置 |
| ADR-0004 ApprovalPolicy | ✅ Approved | yolo/plan/agent 模式复用源 |
| ADR-0031 ExecutionPolicy | 🟡 Partial (C3 已 ship 部分) | 决策模式绑定源 |
| ADR-0061-02 行为回归 | ✅ Approved (T14 ship) | 治理流程回归门前置 |
| ADR-0079 v1.1 Session 4-scope | ✅ Approved | 决策 5 回滚（fork）前置 |
| ADR-0080 + v1.2 amendment | ✅ Approved | 决策 4 审计轨迹前置 |
| ADR-0081 Pre-Step Hook | ✅ Approved (2026-08-22 ship) | 决策 6 S 防护前置 |

## What Changes

本 change 将 ADR-0084 6 维度契约从"ADR 起草"阶段推进到"代码 ship + 评审转 Approved + G11 Closed"阶段：

- **新增契约类**:
  - `include/agenticdsl/contract/imutation_governance.h` — `class IMutationGovernor` 接口（`propose()` + `commit()` + `revert()` 三方法 + `MutationContext` 值类型）
  - `include/agenticdsl/types/mutation_record.h` — `struct MutationRecord`（4 mutation.* 主题 payload）

- **新增 4 个 mutation.* 事件主题注册** (per ADR-0068 amendment 任务):
  - `mutation.proposed` (payload: subject_version + parent_version + proposed_change)
  - `mutation.committed` (payload: version_id + evaluation_refs + mutation_kind)
  - `mutation.reverted` (payload: rollback_reason + target_version)
  - `mutation.denied` (payload: denial_reason + failed_step + subject_version)

- **新增 V1 实现** (per 决策 1 + 决策 2):
  - `src/common/governance/mutation_governor.cpp` — L1-L3 happy path + L4 显式拒绝 + 4 mutation.* 主题 emit
  - `src/common/governance/mutation_topics.cpp` — 4 主题注册 (与 ADR-0068 amendment 同步)

- **新增测试**:
  - `tests/test_mutation_governance.cpp` — ≥ 6 cases（L1/L2/L3 happy path + L4 拒绝 + 审计事件完整性 + fail-closed 行为）

- **ADR-0068 amendment**:
  - `docs/adr/adr-0068-event-emission-contract.md` 附录 A 注册 4 mutation.* 主题

## Impact

- **影响范围**:
  - L1 编排层（cap-map L1）新增 IMutationGovernor 抽象 — 不破坏既有 CognitiveWorker/DomainWorkerPool
  - ADR-0068 EventLog 主题注册表扩展 14 → 18 主题
  - 既有 ApprovalPolicy/ExecutionPolicy 链复用，不新增授权层

- **V1 边界** (per ADR-0084 §决策 1):
  - ✅ L1 prompt 资产（ADR-0074 Prompt Evidence）
  - ✅ L2 DSL 图（ADR-0061-06 Trajectory IR）
  - ✅ L3 SKILL.md（ADR-0061-03 SkillCompiler）
  - ⛔ L4 权重（ADR-0078 Fine-tune）**V1 显式禁止** — 代码层抛出明确异常

- **下游解锁**:
  - T19 GEPA MVP Phase 2 commit 启动
  - T20 AFlow MCTS 工作流改写授权前置
  - T22 Fine-tune 事件驱动训练路径 + 治理契约前置
  - B7 自进化基础应用解锁

- **Breaking Changes**: 无（新增契约类 + 主题注册，不修改既有 API）

## ship gate 验证

- `python3 tools/adr_lint.py` 通过
- `ctest --output-on-failure -R test_mutation_governance` ≥ 6 cases / ≥ 12 assertions PASS
- `cd build && ctest --output-on-failure` 185/185 ctest PASS 零回归
- `grep -r "class IMutationGovernor" include/agenticdsl/contract/` 命中
- ADR-0068 附录 A 4 mutation.* 主题注册完整
- cap-map §二 G11 "🔍 Proposed" → "✅ Closed"
- GitHub issue #14 Closed（body 内留 audit trail）
- ADR-0084 头部 `## 状态` 章节更新为 `✅ Approved (评审通过 2026-XX-XX + 代码 ship)`

## 关联文档

- ADR-0084-mutation-governance-contract.md
- ADR-0080-append-only-event-log.md + ADR-0080-v1-2-amendment-d10-decouple.md
- ADR-0068-event-emission-contract.md (amendment 注册 4 主题)
- ADR-0004-toolregistry-security.md (ApprovalPolicy 复用)
- ADR-0031-execution-policy.md (ExecutionPolicy 复用)
- ADR-0061-02-behavioral-regression.md (T14 行为回归门)
- ADR-0079-unified-session-4scope.md (fork 回滚机制)
- `docs/architecture/self-evolution-architecture-2026-08.md` §四.3/§五/§六/§七
- `docs/architecture/capability-application-map-2026-08.md` §二 G11 + §八.3-§八.6
- `docs/research/agent-distillation-sota-2026-08.md` §四 G11 起草要点
- GitHub issue #14