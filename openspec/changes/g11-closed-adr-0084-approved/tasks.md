# Tasks: g11-closed-adr-0084-approved

> **范围前提**: V1 代码已 ship (commit `a2b2d52`, 13 cases / 139 assertions PASS)。本 change **零代码改动**, 仅文档状态同步 + GitHub Issue 关闭。
> **TDD 5 步结构**: 每任务按 Write failing test (静态检查) → Verify fail → Implement (文档改动) → Verify pass → Commit

## Phase 0: ADR-0084 状态翻转 (估时 0.1 sprint)

- [ ] **T0.1** Write check: 静态验证 ADR-0084 当前状态为 🔍 Proposed (grep 文档第 6-8 行)
- [ ] **T0.2** Verify fail: 文档当前仍标 🔍 Proposed (符合预期, ship gate 未通过)
- [ ] **T0.3** Implement: `docs/adr/adr-0084-mutation-governance-contract.md` §状态 章节:
  - 🔍 Proposed → ✅ Approved (评审通过 2026-08-26 — V1 gate-and-audit 代码 ship, commit `a2b2d52`)
  - 追加 ship 证据段: V1 范围 + 13 测试用例 + ctest 187/187 PASS + 6 项决策落地
- [ ] **T0.4** Verify pass: `grep -E "(🔍 Proposed|✅ Approved)" docs/adr/adr-0084-mutation-governance-contract.md` 第 1-20 行显示 ✅ Approved
- [ ] **T0.5** Commit: `docs(adr-0084): Approved - V1 gate-and-audit ship (G11 Closed)`

## Phase 1: 文档状态同步 (估时 0.2 sprint)

### T1.1 capability-map §二 G11 行更新

- [ ] **T1.1.1** Write check: 静态验证 G11 行当前状态 (grep "G11" cap-map §二)
- [ ] **T1.1.2** Verify fail: cap-map §二 G11 行仍标 🔍 Proposed
- [ ] **T1.1.3** Implement: `docs/architecture/capability-application-map-2026-08.md` §二 G11 行:
  - 🔍 Proposed → ✅ Closed (ADR-0084 Approved + V1 gate-and-audit 代码 ship, 2026-08-26)
  - 标注 ship 证据: commit `a2b2d52`, 13 cases / 139 assertions, OpenSpec change `2026-08-26-adr-0084-mutation-governance-contract`
- [ ] **T1.1.4** Verify pass: `grep -E "G11.*Closed\|G11.*Proposed" docs/architecture/capability-application-map-2026-08.md` 显示 ✅ Closed

### T1.2 capability-map §三 B7 自进化基础状态同步

- [ ] **T1.2.1** Write check: grep "B7" cap-map §三
- [ ] **T1.2.2** Verify fail: B7 行 G11 状态仍写 "🔍 (ADR-0084 起草中)"
- [ ] **T1.2.3** Implement: §三 B7 行:
  - "G11 🔍 (ADR-0084 起草中) 契约" → "G11 ✅ Closed (V1 code ship, 2026-08-26)"
- [ ] **T1.2.4** Verify pass: grep 显示 B7 G11 状态 ✅

### T1.3 capability-map §八 §八.5 排期表

- [ ] **T1.3.1** Write check: grep "ADR-0084 mutation-governance" cap-map §八.5
- [ ] **T1.3.2** Verify fail: 排期表当前记录 "Sprint 25 W1 启动 + Sprint 26 末评审 + G11 ✅ Closed" (待完成项)
- [ ] **T1.3.3** Implement: §八.5 排期行追加完成日期列 "2026-08-26 ship" 标记
- [ ] **T1.3.4** Verify pass: grep 显示完成状态

### T1.4 capability-map §七 changelog + 头部版本

- [ ] **T1.4.1** Write check: grep "v1.6.1\|v1.7" cap-map 头部
- [ ] **T1.4.2** Verify fail: 头部仍 v1.6.1
- [ ] **T1.4.3** Implement: 头部 v1.6.1 → v1.7 + 最后验证 2026-08-26 + §七 changelog 新增 v1.7 条目:
  ```
  | 2026-08-26 | **v1.7** | **G11 Closed: ADR-0084 Approved + V1 ship** | (1) §二 G11 🔍 Proposed → ✅ Closed (ship 证据: commit a2b2d52, 13 cases, 2026-08-26); (2) §三 B7 G11 状态同步; (3) §八.5 排期完成标记; (4) §八.6 风险 resolved 保持; (5) §七 changelog (本次); (6) ADR-0084 状态 🔍 → ✅. 依据: OpenSpec change `2026-08-26-g11-closed-adr-0084-approved`. |
  ```
- [ ] **T1.4.4** Verify pass: 头部 + §七 同步

### T1.5 self-evolution-architecture §四.2 + 头部版本

- [ ] **T1.5.1** Write check: grep "G11\|ADR-0084" self-evolution-architecture
- [ ] **T1.5.2** Verify fail: 当前 v1.1.1 + G11 状态未标 ✅ Closed
- [ ] **T1.5.3** Implement:
  - 头部 v1.1.1 → v1.2 + 最后验证 2026-08-26
  - §四.2 评估/奖励/变异治理行追加 "G11 Closed (2026-08-26, ADR-0084 Approved + V1 ship)"
- [ ] **T1.5.4** Verify pass: grep 显示 ✅ Closed

### T1.6 active-status.md G11 跟踪段

- [ ] **T1.6.1** Write check: grep "issue #14\|T19 GEPA Phase 1 只读" active-status §一
- [ ] **T1.6.2** Verify fail: 当前 G11 跟踪段仍写 "issue #14 保持 OPEN" + "T19 GEPA Phase 1 只读反思约束"
- [ ] **T1.6.3** Implement: `docs/active-status.md` §一 G11 跟踪段:
  - 删除 "issue #14 保持 OPEN 直至本 ADR ship + Approved + G11 Closed" 行 (替换为 "issue #14 已 Closed (2026-08-26, 见 audit trail)")
  - 删除 "T19 GEPA Phase 1 只读反思约束（不执行 `commit(PromptEdit)`）直至 G11 ADR Approved" 行 (替换为 "T19 GEPA Phase 2 commit 解锁 (G11 ✅ Closed 2026-08-26)")
- [ ] **T1.6.4** Verify pass: grep 显示 issue #14 关闭声明

### T1.7 adr-implementation-status-gap-analysis §一 总计行

- [ ] **T1.7.1** Write check: grep "总计" gap-analysis §一
- [ ] **T1.7.2** Verify fail: 当前总计 72 ADR (含 G11 起草中注释)
- [ ] **T1.7.3** Implement: 注释更新 "G11 ADR-0084 起草中" → "G11 ADR-0084 ✅ Approved (V1 ship 2026-08-26)" + 状态计数同步 (Approved +1)
- [ ] **T1.7.4** Verify pass: grep 显示 ✅ Approved

### T1.8 docs/README.md §adr/ 表 ADR-0084 行

- [ ] **T1.8.1** Write check: grep "adr-0084" docs/README.md
- [ ] **T1.8.2** Verify fail: 当前仍标 🔍 Proposed
- [ ] **T1.8.3** Implement: ADR-0084 行:
  - "🔍 Proposed 起草中 (2026-08-26 文件创建, ..., G11 缺口解决, ...)" → "✅ Approved (2026-08-26 — V1 gate-and-audit 代码 ship, G11 Closed, 13 cases / 139 assertions PASS, OpenSpec change `2026-08-26-adr-0084-mutation-governance-contract`)"
- [ ] **T1.8.4** Verify pass: grep 显示 ✅ Approved

### T1.9 Phase 1 验证

- [ ] **T1.9.1** `python3 tools/adr_lint.py` 通过
- [ ] **T1.9.2** `python3 tools/docs_drift_audit.py` 通过 (无新增 CRITICAL)
- [ ] **T1.9.3** Commit: `docs(cap-map+self-evolution+active-status): G11 Closed (ADR-0084 Approved + V1 ship)`

## Phase 2: GitHub Issue 关闭 + ship gate (估时 0.1 sprint)

- [ ] **T2.1** Issue #14 audit trail 准备:
  - ship 摘要: "ADR-0084 ✅ Approved + V1 code ship (commit `a2b2d52`) + G11 ✅ Closed (cap-map §二 同步 2026-08-26); V1 范围: gate-and-audit only, 13 cases / 139 assertions, ctest 187/187 PASS zero regression"
  - 相关 OpenSpec: `2026-08-26-adr-0084-mutation-governance-contract` (已 archive) + `2026-08-26-g11-closed-adr-0084-approved` (本 change)
  - 决议依据: Oracle sessions `ses_fc41537bbffeC35NKqgvzn4m1c` (Self-Review) + `ses_fc3e070c0ffeIVgAhsgx2pNXFa` (Deep Review) + `ses_fc3090b49ffe7yJwXhx1MoNz5N` (架构审计)
- [ ] **T2.2** `gh issue close 14 --comment "..."` (使用 T2.1 准备的 audit trail)
- [ ] **T2.3** Verify: `gh issue view 14 --json state` 显示 `CLOSED`
- [ ] **T2.4** Final ship gate:
  - [ ] `ctest --output-on-failure` 全量 PASS (动态计数, 禁止硬编码 187)
  - [ ] `python3 tools/adr_lint.py` 通过
  - [ ] `python3 tools/docs_drift_audit.py` 通过
  - [ ] `openspec validate --changes --strict` PASS
- [ ] **T2.5** Commit: `chore(openspec): archive 2026-08-26-g11-closed-adr-0084-approved + issue #14 closed`
- [ ] **T2.6** `openspec archive 2026-08-26-g11-closed-adr-0084-approved`

## 总估时

- Phase 0: 0.1 sprint
- Phase 1: 0.2 sprint (8 个文档改动)
- Phase 2: 0.1 sprint
- **总估时: ~0.4 sprint (Single-dev 模式 + 无 24h Cooling-Off 因代码已 ship)**

## 范围外 (明确不做)

- ❌ ADR-0084 §状态章节追加 G11 Closed 注释 (Phase 0 仅做状态翻转, G11 Closed 标注留在 cap-map)
- ❌ T19 Phase 2 commit 实际代码 (独立 change)
- ❌ T17 SkillCompiler 集成 (独立 change)
- ❌ mutation_topics.cpp 主题注册 (V1 已确认不存在, 仅文档层)
- ❌ 任何 C++ 源代码改动 (V1 已 ship, 禁止回改)
- ❌ ADR-0084 评审会议纪要单独创建 (本次通过为 Self-Review + Oracle 评审集成模式, 沿用 §六 review 流程)