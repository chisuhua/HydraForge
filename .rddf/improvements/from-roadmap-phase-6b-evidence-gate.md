# from-roadmap-phase-6b-evidence-gate

**优先级**: P0 | **来源**: from-roadmap (phase-6b/evidence-gate, ADR-0074 D4)
**阶段**: phase-6b | **分类**: evidence-gate
**类型**: governance
**主题**: Evidence Gate决议；parse-valid阈值；task-success阈值

## 架构依据

ADR-0074 D4 Evidence Gate 决议是 Phase 6c 启动前置（parse-valid ≥85% + task-success L1 ≥70% 才进入 6c）：

- baseline 测量（execution-baseline 提案）产出原始数据，本提案做"决议"。
- 决策树：parse-valid ≥85% → PASS；85% < x < 90% → 临界带（C5+C6 条件性 skip）；≥90% → 全面 skip C5/C6。
- task-success L1 ≥70% 是辅助指标（L2/L3 留 future follow-up）。
- 决策文档 `docs/audits/<date>-evidence-gate-v1.md` 是 Sprint Review 输入。

## 范围

- **In Scope**:
  - `scripts/evidence-gate-v1.sh` 一键决议脚本（输入 measurement JSON + 输出 PASS/CONDITIONAL/FAIL）。
  - `docs/audits/<date>-evidence-gate-v1.md` 决议模板（基线引用 + 决策表 + 后续建议）。
  - Sprint Review 入项材料化（active-status.md §一 基线引用）。
  - 决策树分支可视化（mermaid 流程图 + JSON 输出）。
  - 3 类测试：PASS 分支 / CONDITIONAL 分支 / FAIL 分支 + 阈值边界。
- **Out of Scope**:
  - baseline 测量本身（→ execution-baseline 提案）。
  - L2/L3 task-success 指标（留 follow-up）。
  - 自动重新测量机制（手动运行，决策权在 human review）。

## 关键场景

- GIVEN measurement JSON 含 parse-valid=87% task-success=72%
  WHEN evidence-gate-v1.sh 执行
  THEN 输出 CONDITIONAL（85% ≤ x < 90% 临界带），决议文档建议条件性 ship C5/C6。

- GIVEN parse-valid=82% task-success=68%
  WHEN evidence-gate-v1.sh 执行
  THEN 输出 FAIL（< 85%），决议文档建议 descope C5+C6+C7（不实施）。

- GIVEN parse-valid=92% task-success=78%
  WHEN evidence-gate-v1.sh 执行
  THEN 输出 PASS（≥ 90%），决议文档建议全面 skip C5/C6。

## 技术约束

- MUST 决策文档 git-tracked（含决议日期 + baseline JSON 路径 + 决策表）。
- MUST decision tree 公式化（脚本实现，禁止人肉解读）。
- MUST threshold 集中可配（`scripts/evidence-gate-v1.sh` 顶部 constant）。
- MUST NOT 引入 LLM 自动决策（决策权在 Sprint Review）。
- SHOULD 决议文档模板保留历史快照（决议原因可追溯）。

## 验收标准

- evidence-gate-v1.sh 脚本可执行（3 分支 + 阈值边界测试）。
- 决议模板 git-tracked。
- 决议文档与 active-status.md 联动（决议日 + baseline 引用）。
- ctest 全量零回归。
- 阻塞 Phase 6c 启动条件（4 项启动条件之一）。