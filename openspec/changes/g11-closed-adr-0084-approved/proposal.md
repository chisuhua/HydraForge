# g11-closed-adr-0084-approved

## Why

ADR-0084 (Mutation Governance Contract) 🔍 Proposed 自 2026-08-26 起草完成，V1 代码已 ship (commit `a2b2d52`, 13 cases / 139 assertions PASS, ctest 187/187 全量零回归)。代码 ship 后状态翻转与文档同步为独立收口工作，**非**再次实施。

**Oracle 评审关键发现** (Oracle session `ses_fc2705995ffeyAqiGqr6jQTeKD` 综合):
- ADR-0084 §决策 1-6 全部落地：变异对象 L1-L4 分级 + 授权绑定复用 ADR-0004 + ADR-0031 + 治理流程 (propose→evaluator→回归门→commit) + 审计复用 ADR-0080 + 4 mutation.* 主题 + 失败回滚 (audit-only) + 攻击面 fail-closed
- V1 边界严格遵守 (Oracle 审查后设计文件已收窄)：无 subject 版本存储 / 无恢复逻辑 / 无 24h 保留窗口 / revert 是 audit-only
- 13 个测试用例覆盖全部门禁链分支 (Oracle 设计 D6 严格一致)
- adr_lint + docs_drift_audit + openspec validate --strict 全 PASS
- 6 个下游 ADR (T19/T20/T22) 硬前置已解锁

**审计依据**:
- `openspec/changes/archive/2026-08-26-2026-08-26-adr-0084-mutation-governance-contract/` 含 spec.md + 4 requirements + 7 scenarios 全部落盘
- ADR-0068 附录 A v1.2.1 已 ship 4 mutation.* 主题注册 (`mutation.proposed` / `committed` / `reverted` / `denied`)
- `docs/architecture/capability-application-map-2026-08.md` §二 G11 仍标 🔍 Proposed (待本 change ship 后翻 ✅ Closed)
- `docs/active-status.md` §一 G11 跟踪段 + §四 阻塞项仍写 "issue #14 保持 OPEN 直至 ADR-0084 Approved + G11 Closed"

**前置依赖** (全部已满足):
- ✅ ADR-0084 文件已创建 (2026-08-26, 🔍 Proposed)
- ✅ V1 代码 ship + 13 测试 PASS (commit `a2b2d52`)
- ✅ ADR-0068 附录 A v1.2.1 (commit `cfc3532`)
- ✅ IEvaluator 契约 ship (`openspec/changes/archive/2026-08-26-2026-08-26-ship-ievaluator-reward-contract/`)
- ✅ ADR-0004 ApprovalPolicy / ADR-0031 ExecutionPolicy 已 Approved
- ✅ ADR-0080 + v1.2 amendment Approved (审计轨迹前置)
- ✅ T14 行为回归套件 ship (回归门前置)
- ✅ ADR-0079 v1.1 Session 4-scope ✅ (fork 回滚机制前置)
- ✅ ADR-0081 Pre-Step Hook ✅ (决策 6 S 防护前置)

## What Changes

**范围外 (本 change 明确不做)**:
- ❌ 任何 C++ 源代码改动 (V1 已 ship, 禁止回改)
- ❌ V2 评估器 / L4 权重支持 (V1 边界显式禁止, deferred)
- ❌ T17 SkillCompiler 集成 (独立 change, 本 change 不阻塞)
- ❌ 实施行为回归门内部实现 (复用现有 `behavioral_regression.h` 接口, 已 ship)
- ❌ mutation_topics.cpp 主题注册 (EventBuilder 无该 API, 仅文档层, 已 ship)

**本 change 收口工作** (5 项文档 + 1 项 issue 关闭):

1. **ADR-0084 状态翻转**: 🔍 Proposed → ✅ Approved (评审通过 2026-08-26, V1 代码 ship)
   - 文件: `docs/adr/adr-0084-mutation-governance-contract.md`
   - §状态章节追加评审通过日期与 ship 证据段

2. **capability-map §二 G11 行更新**: 🔍 Proposed → ✅ Closed
   - 文件: `docs/architecture/capability-application-map-2026-08.md`
   - §二 G11 行标注 ✅ Closed + ship 证据 (commit `a2b2d52`, 13 cases, 2026-08-26)
   - §七 changelog 新增 v1.7 条目
   - 头部版本 v1.6.1 → v1.7 + 最后验证 2026-08-26

3. **capability-map §三 B7 自进化基础状态**: 同步 G11 ✅ Closed
   - §三 B7 行更新前置依赖 "G11 🔍 (ADR-0084 起草中)" → "G11 ✅ Closed (V1 code ship)"
   - §八 §八.5 排期表新增 "Sprint 26 末 11: ADR-0084 mutation-governance 评审通过 → G11 ✅ Closed" 行
   - §八.6 风险段 "变异治理缺位" 已 resolved 状态保持

4. **self-evolution-architecture §四.2 评估/奖励/变异治理行**: G11 状态同步
   - 文件: `docs/architecture/self-evolution-architecture-2026-08.md`
   - §四.2 行新增 "G11 Closed (2026-08-26)" 标注
   - 头部 v1.1.1 → v1.2 + 最后验证 2026-08-26

5. **active-status.md G11 跟踪段更新**:
   - 文件: `docs/active-status.md`
   - §一 G11 跟踪段：移除 "issue #14 保持 OPEN" 声明
   - §一 G11 跟踪段：移除 "T19 GEPA Phase 1 只读反思约束" 声明 (T19 Phase 2 commit 现在解锁)
   - §一 OpenSpec active 计数（如有）+ G11 跟踪段更新

6. **GitHub issue #14 关闭**:
   - body 内追加 audit trail (本 change ship 后, ship 物摘要)
   - 关闭原因: "ADR-0084 ✅ Approved + V1 code ship + G11 Closed (2026-08-26)"

## Impact

- **影响范围**:
  - **零代码改动**: 仅文档状态同步 + Issue 关闭
  - 6 个下游 ADR (T19/T20/T22 + ADR-0061-08 AFlow + ADR-0061-09 GEPA + ADR-0078 Fine-tune) 硬前置已解锁
  - T19 GEPA Phase 2 commit (实际执行 `commit(PromptEdit)`) 现在可以启动

- **下游解锁**:
  - T19 GEPA MVP Phase 2 commit 启动 (失败→反思→修订 prompt → commit MutationGovernor)
  - T20 AFlow MCTS 工作流改写授权前置
  - T22 Fine-tune 事件驱动训练路径 + 治理契约前置
  - B7 自进化基础应用解锁
  - 闭环 2 第 3 环 (commit 信号) 解锁

- **Breaking Changes**: 无 (纯文档 + issue 关闭)

## ship gate 验证

- `python3 tools/adr_lint.py` 通过 (≥82 ADR)
- `python3 tools/docs_drift_audit.py` 通过 (无新增 CRITICAL drift)
- `openspec validate --changes --strict` PASS
- ctest 全量当前总数 PASS 零回归（动态计数，基线 = ship 时实测 + 0 净增）
- GitHub issue #14 关闭 (body 内留 audit trail)
- `docs/README.md` §adr/ 表 ADR-0084 行：🔍 Proposed → ✅ Approved (2026-08-26 — V1 ship + 评审通过)
- `docs/architecture/adr-implementation-status-gap-analysis.md` §一 总计行：72 → 73 ADR 同步 (ADR-0084 状态翻 ✅)

## 关联文档

- `docs/adr/adr-0084-mutation-governance-contract.md` (待翻 ✅)
- `docs/adr/adr-0083-evaluator-reward-contract.md` (G10 ✅, 6 个下游 ADR 硬前置)
- `openspec/changes/archive/2026-08-26-2026-08-26-adr-0084-mutation-governance-contract/` (本 change 的 ship 物依据)
- `docs/architecture/capability-application-map-2026-08.md` §二 G11 + §七 changelog
- `docs/architecture/self-evolution-architecture-2026-08.md` §四.2
- `docs/active-status.md` §一 G11 跟踪段
- `docs/architecture/adr-implementation-status-gap-analysis.md` §一
- `docs/README.md` §adr/ 表
- `docs/specs/mutation-governance-contract/spec.md` (本 change 已 archive, 4 requirements + 7 scenarios)
- ADR-0068 附录 A v1.2.1 (4 mutation.* 主题)
- GitHub issue #14