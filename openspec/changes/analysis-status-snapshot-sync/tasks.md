# Tasks: analysis-status-snapshot-sync

> **关键不变量**：纯文档同步；零代码/测试改动；分析文档为"滚动快照 + 权威参照"（非"唯一事实源"）
> **TDD 5 步结构**：每任务按 Write failing assertion → Verify fail → Implement → Verify pass → Commit

## Phase 0: 基线快照（pre-fix 验证）

- [x] **T0.1** Snapshot `python3 tools/adr_lint.py` baseline（须 exit 0）
- [x] **T0.2** Snapshot `python3 tools/doc_metrics.py` baseline
- [x] **T0.3** Snapshot `python3 tools/docs_drift_audit.py` baseline（须 0 DRIFT）
- [x] **T0.4** 记录 baseline grep 结果
  - `grep -c "唯一事实源" docs/ -r` → 4
  - `grep "ADR-0070" docs/README.md | head -1` → 含 🔍 Proposed
  - `grep "ADR-0085" docs/architecture/layer-based-missing-capabilities-analysis.md` → 0
  - `grep "ADR-0068" docs/architecture/adr-implementation-status-gap-analysis.md | head -3` → 含 Proposed
- [x] **T0.5** 实测 ctest 通过数（`ctest --output-on-failure 2>&1 | tail -5`）

## Phase 1: gap-analysis 状态更新 + 快照横幅（估时 0.3h）

- [x] **T1.1** Read `docs/architecture/adr-implementation-status-gap-analysis.md` L1-30（Header + §一）、L155-180（§2.3 表）、L417（ctest 计数）
- [x] **T1.2** Write failing assertion：期望 ADR-0068 状态行更新
- [x] **T1.3** Modify L166-168 三行
  - L166 `ADR-0068 (Event Emission Contract) | 🔍 Proposed (Wave 1 在审)...` → `ADR-0068 (Event Emission Contract) | ✅ Approved (2026-08-03, 经 v1.1-v2.0 持续修订) | Wave 1 §1-§5 全 ship，7 幻影主题已全部真实发射`
  - L167 `ADR-0069 (ToolCoordinator Hooks) | 🔍 Proposed (Wave 1 在审)...` → `ADR-0069 (ToolCoordinator Hooks) | 🟡 Partial (2026-08-04) | middleware 改造 + budget_agent pre-hook + 5 类测试 ship，待 HookErrorPolicy amendment`
  - L168 `ADR-0070 (DECLARE_COMMAND) | 🔍 Proposed (Wave 1 在审)...` → `ADR-0070 (DECLARE_COMMAND) | 🟡 Partial (2026-08-04) | 见 D4 立项 + 实施排期 Wave 1`
- [x] **T1.4** Modify §一 总览表 L12-22 计数（重算 Approved/Partial/Proposed + 占比）
- [x] **T1.5** Modify §一 L16 6 个 LLM-native ADR 表行：0074 → ✅ Approved (2026-08-25)、0075 → ✅ Approved (2026-08-18)
- [x] **T1.6** Modify Header 加快照横幅（置于 §一 之前）：
  ```
  > ⚠️ **本文档为滚动快照**。ADR 状态以 `docs/adr/*.md` 各自状态行为准；本表滞后风险自负。
  > 数据修订记录见 `git log docs/architecture/adr-implementation-status-gap-analysis.md`。
  ```
- [x] **T1.7** Modify §决策 5 / 文档定位段：把"ADR 状态唯一事实源"措辞改为"ADR 状态权威参照（最终以 `docs/adr/*.md` 状态字段为准）"
- [x] **T1.8** Modify L417 `ctest 97/98` 等陈旧计数 → 当前实测（如"截至 2026-09-01: 184/185 PASS，可复现 `ctest --output-on-failure`"）
- [x] **T1.9** Verify：`grep "ADR-0068" docs/architecture/adr-implementation-status-gap-analysis.md | grep "Proposed"` 0 命中（L166 修复后）
- [x] **T1.10** Verify：`grep "唯一事实源" docs/architecture/adr-implementation-status-gap-analysis.md` 0 命中
- [x] **T1.11** Commit：`docs(adr-implementation-status-gap-analysis): 状态更新 + 快照横幅 (issue 1+2+5+6)`

## Phase 2: llm-native-blueprint 状态更新 + 快照横幅（估时 0.2h）

- [x] **T2.1** Read `docs/architecture/llm-native-blueprint-vs-code-gap-analysis.md` §一 + §2.1/§2.4
- [x] **T2.2** Write failing assertion：期望 §2.1 ADR-0071 状态行更新
- [x] **T2.3** Modify §一 L14/L23 表格
  - 8 个 ADR 全部 🔍 Proposed → 0071 ✅ / 0072 🔍 / 0073 🟡 / 0074 ✅ / 0075 ✅ / 0076 🔍 / 0077 🔍 / 0078 🔍
  - "0 Approved + 已 ship" → "3 Approved (0071/0074/0075) + 1 Partial (0073) + 4 Proposed (0072/0076/0077/0078)"
- [x] **T2.4** Modify §2.1 L34 ADR-0071 状态行：🔍 Proposed (2026-08-02) → ✅ Approved (2026-08-25 评审通过)
- [x] **T2.5** Modify §2.4 L92 ADR-0074 状态行：🔍 Proposed (2026-08-03) → ✅ Approved (2026-08-25 评审通过)
- [x] **T2.6** Modify §1 Overview D5/L5 行（D5 指向 ADR-0074）：补 ✅ Approved 状态
- [x] **T2.7** Modify 文首加快照横幅：
  ```
  > ⚠️ **本文档为 2026-08-03 基线快照**（commit ec27bad 提交日），ADR 状态已过期。决策请以 `docs/adr/*.md` 各自状态行为准。
  ```
- [x] **T2.8** Verify：`grep "ADR-0071" docs/architecture/llm-native-blueprint-vs-code-gap-analysis.md | grep "Proposed"` 仅出现在 0072 等仍 Proposed 的项，0071 0 命中
- [x] **T2.9** Verify：`grep "基线快照" docs/architecture/llm-native-blueprint-vs-code-gap-analysis.md` ≥ 1
- [x] **T2.10** Commit：`docs(llm-native-blueprint): 状态更新 + 快照横幅 (issue 3)`

## Phase 3: layer-based 落地标注 + ADR-0085 引用 + 快照横幅（估时 0.3h）

- [x] **T3.1** Read `docs/architecture/layer-based-missing-capabilities-analysis.md` L1-1 L198-210、L1-2 L212-228、L804-806
- [x] **T3.2** Write failing assertion：期望 L1-1 段尾含"已落地"标注
- [x] **T3.3** Modify L1-1 L198-210 段尾：在"建议: 新增 ADR-0068"段后加
  ```
  > **→ 已落地**: ADR-0068 ✅ Approved (2026-08-03, Wave 1 §1-§5 ship, 7 幻影主题真实发射, 附录 A v2.0)。
  ```
- [x] **T3.4** Modify L1-2 L212-228 段尾：在"建议 ToolCoordinator 改 middleware"段后加
  ```
  > **→ 已落地**: ADR-0069 🟡 Partial (2026-08-04, middleware 改造 ship, budget_agent pre-hook ship, 5 类测试 PASS, 待 HookErrorPolicy amendment)。
  > **另见**: ADR-0085 Cross-Cutting Pattern PDK ✅ Approved (2026-08-28) 提供横切 DSL `*.cc.md` 替代方案，与本节 hook 机制互补。
  ```
- [x] **T3.5** Modify L804-806 拟新增 ADR-0069/0070 行：补"→ ADR-0069 🟡 Partial (2026-08-04) | ADR-0070 🟡 Partial (2026-08-04)"落地标注
- [x] **T3.6** Modify 文首加快照横幅（不动四字段）：
  ```
  > ⚠️ **本文档为 2026-07-31 数据基线 v1.2.1**。后续 ship 见 `docs/architecture/capability-application-map-2026-08.md`（当前架构现状权威）。
  ```
- [x] **T3.7** Verify：`grep "已落地" docs/architecture/layer-based-missing-capabilities-analysis.md` ≥ 2（L1-1 + L1-2）
- [x] **T3.8** Verify：`grep "ADR-0085" docs/architecture/layer-based-missing-capabilities-analysis.md` ≥ 1
- [x] **T3.9** Commit：`docs(layer-based-missing-capabilities): 落地标注 + ADR-0085 引用 + 快照横幅 (issue 4)`

## Phase 4: 4 处"唯一事实源"引用同步降级（估时 0.2h）

- [x] **T4.1** Read `docs/GOVERNANCE.md:68` / `docs/README.md:152` / `docs/architecture/README.md:17` / `docs/architecture/adr-implementation-status-gap-analysis.md §决策 5`
- [x] **T4.2** Write failing assertion：期望"唯一事实源"4 处全部 0 命中
- [x] **T4.3** Modify `docs/GOVERNANCE.md:68`：`本仓库的 ADR 状态基线（唯一事实源）...` → `本仓库的 ADR 状态权威参照（最终以 docs/adr/*.md 各自状态行为准）。本表为滚动快照，与 docs/adr/*.md 冲突时以 ADR 文件为准。`
- [x] **T4.4** Modify `docs/README.md:152`：同上去重措辞
- [x] **T4.5** Modify `docs/architecture/README.md:17`：同上去重措辞
- [x] **T4.6** Modify `docs/architecture/adr-implementation-status-gap-analysis.md §决策 5`：同上去重措辞（与 T1.7 一致）
- [x] **T4.7** Verify：`grep -r "唯一事实源" docs/` 0 命中
- [x] **T4.8** Verify：`grep -r "权威参照" docs/GOVERNANCE.md docs/README.md docs/architecture/README.md` ≥ 3
- [x] **T4.9** Commit：`docs: 4 处"唯一事实源"措辞同步降级为"权威参照" (issue 5)`

## Phase 5: 附带修复 + ship gate（估时 0.1h）

- [x] **T5.1** Read `docs/README.md` ADR-0070 行
- [x] **T5.2** Modify：`adr-0070-declare-command.md | 🔍 Proposed (2026-07-31, D4 立项, 实施排期 Wave 1)` → `adr-0070-declare-command.md | 🟡 Partial (2026-08-04, D4 立项 + 实施排期 Wave 1)`
- [x] **T5.3** Verify：`grep "ADR-0070\|adr-0070" docs/README.md | grep "🟡 Partial"` ≥ 1
- [x] **T5.4** `python3 tools/adr_lint.py` exit 0 且新增 ERROR = 0
- [x] **T5.5** `python3 tools/docs_drift_audit.py` 0 DRIFT
- [x] **T5.6** `openspec validate analysis-status-snapshot-sync --strict` PASS
- [x] **T5.7** `grep -r "唯一事实源" docs/` 0 命中
- [x] **T5.8** `git log --oneline` 检查所有 commit 存在
- [x] **T5.9** `openspec archive analysis-status-snapshot-sync`

## 总估时

- Phase 0: 0.1h
- Phase 1: 0.3h
- Phase 2: 0.2h
- Phase 3: 0.3h
- Phase 4: 0.2h
- Phase 5: 0.1h
- **总估时: ~1.2h**（纯文档同步）

## 关键不变量（强制遵守）

- ❌ 任何代码改动（仅 6 个文档文件）
- ❌ 任何测试改动
- ❌ 任何 ctest 数字硬编码（用 doc_metrics 实测）
- ❌ 改 GOVERNANCE 数据纪律条款（仅改"唯一事实源"措辞）
- ❌ 改 architecture/ 文档头四字段（仅在 Header 下加快照横幅段）

## 明确 out of scope (需独立 change)

- 6 幻影主题机制修复（与 change ① 同样 defer）
- 3 份分析文档的 §决策 段重组与重写（仅修状态行）
- 引入自动状态同步脚本（每日 cron 跑 adr_lint + diff 写入 gap-analysis）
- 修正 `docs/adr/adr-0019-impl-scope.md` 中可能存在的旧状态描述
