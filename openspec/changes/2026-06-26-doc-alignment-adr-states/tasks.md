# Tasks: Doc Alignment & ADR State Reconciliation (Sprint 11 P0)

> **来源**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C0
> **关联 change**: 无
> **后续 change**: `2026-06-26-sprint-7-tech-debt-execution` (C1) 依赖本 change

---

## 1. ADR-0030 V2 草案撰写

- [ ] 1.1 新建 `docs/adr/adr-0030-async-runtime-v2.md` (frontmatter: status=Proposed, date=2026-06-26, supersedes=adr-0030-async-runtime-dual-layer)
- [ ] 1.2 §背景: 引用 Slice 00 已 ship 状态 (Taskflow v4.0 + async_simple v1.4 引入)
- [ ] 1.3 §决策 1: 保留双层架构 vs std::jthread 替代 (列出 3 个 open questions, 待 C2 实施时由 Oracle 咨询)
- [ ] 1.4 §决策 2: Phase 2 范围切分 (并行 DAG executor + Fleet 模式 16 路)
- [ ] 1.5 §关联: ADR-0002 / 0019 / 0020 / 0025
- [ ] 1.6 §影响: 与 Sprint 12 任务列表交叉引用
- [ ] 1.7 验证: `head -10 docs/adr/adr-0030-async-runtime-v2.md` 显示 frontmatter + 标题

## 2. ADR-0030 V1 状态追加

- [ ] 2.1 编辑 `docs/archive/adr/adr-0030-async-runtime-dual-layer.md`, 在 frontmatter 顶部追加:
  ```
  > **V1 SUPERSEDED by V2** (2026-06-26, OpenSpec change `2026-06-26-doc-alignment-adr-states`)
  > 详见 `docs/adr/adr-0030-async-runtime-v2.md`
  > 保留原因: 历史可追溯, V2 不复用 V1 编号
  ```
- [ ] 2.2 验证: `grep "SUPERSEDED" docs/archive/adr/adr-0030-async-runtime-dual-layer.md` 1 命中

## 3. ADR-0032 状态修正

- [ ] 3.1 决策: 移回 active vs 留 archive+修正 (Sprint 11 Day 1 启动时咨询 Oracle)
  - **方案 A (推荐)**: 留 archive, frontmatter 修正状态 `❌ Not Implemented` → `🟡 Partial (test_cost_collector 已 ship, 2026-06-14)`, 标注 "Phase 3 收尾: 集成 BudgetController::CostTracker 推迟到 C8"
  - **方案 B**: `git mv` 移回 `docs/adr/adr-0032-cost-collector.md`, 恢复 active ADR
- [ ] 3.2 编辑 `docs/archive/adr/adr-0032-cost-collector.md` (或 `docs/adr/adr-0032-cost-collector.md` 如选方案 B)
- [ ] 3.3 更新 `docs/README.md` § adr/ 状态表: 移除 ADR-0032 行 (如选 B) 或保留但标记 🟡 Partial (如选 A)
- [ ] 3.4 验证: `python3 tools/adr_lint.py` exit 0

## 4. 同步 docs/implementation-roadmap.md

- [ ] 4.1 §Slice 00 行: 已存在 `[x]` 状态, 验证无误
- [ ] 4.2 §Phase 2 段: 追加 "**前置**: Slice 00 (Taskflow + async_simple) 已 ship" 提示
- [ ] 4.3 §Phase 2 ADR 引用: `ADR-0030 Taskflow + async_simple + 并行调度` → `ADR-0030 V2 (见 docs/adr/adr-0030-async-runtime-v2.md)`
- [ ] 4.4 §代码状态真实断面 行: ADR 状态表追加 ADR-0030 V2 行
- [ ] 4.5 验证: `grep "V2" docs/implementation-roadmap.md` ≥ 1 命中

## 5. 同步 docs/roadmap-status.md

- [ ] 5.1 §一 Phase 2 行: 0% 状态描述追加 "(Sprint 12 待启动, 依赖 C1 + C0)"
- [ ] 5.2 §四 实施日志 顶部: 追加 2026-06-26 行, 描述本 change 收官
- [ ] 5.3 §七 已知遗留 段: 移除"ADR-0030 依赖未引入"遗留 (如已存在)
- [ ] 5.4 验证: `grep "2026-06-26" docs/roadmap-status.md` ≥ 2 命中 (实施日志 + Recent Changes)

## 6. 同步 AGENTS.md

- [ ] 6.1 顶部 Recent Changes 段追加:
  ```
  - 2026-06-26 (Sprint 11 启动): OpenSpec change `2026-06-26-doc-alignment-adr-states` 创建, 修 ADR-0030 V2 + ADR-0032 状态 + 4 处 docs 同步
  ```
- [ ] 6.2 验证: `head -50 AGENTS.md | grep "2026-06-26"` 命中

## 7. 验证 ship gate

- [ ] 7.1 `cd /workspace/project/HydraForge && python3 tools/adr_lint.py docs/adr/ docs/archive/adr/ docs/adr/plugin/` exit 0
- [ ] 7.2 `python3 tools/docs_drift_audit.py` 0 critical drift
- [ ] 7.3 `openspec validate 2026-06-26-doc-alignment-adr-states` exit 0
- [ ] 7.4 `git status` clean
- [ ] 7.5 `git log --oneline -1` 显示本 change final commit

## 8. 提交

- [ ] 8.1 提交 1: `git add docs/adr/adr-0030-async-runtime-v2.md && git commit -m "docs(adr): draft ADR-0030 V2 (Phase 2 async runtime, replaces V1)"`
- [ ] 8.2 提交 2: `git add docs/archive/adr/adr-0030-async-runtime-dual-layer.md && git commit -m "docs(adr): mark ADR-0030 V1 as SUPERSEDED by V2"`
- [ ] 8.3 提交 3: `git add docs/archive/adr/adr-0032-cost-collector.md (or docs/adr/) && git commit -m "docs(adr): correct ADR-0032 status to 🟡 Partial (test_cost_collector ships 2026-06-14)"`
- [ ] 8.4 提交 4: `git add docs/implementation-roadmap.md && git commit -m "docs(roadmap): update Phase 2 description (Slice 00 done) + ADR-0030 V2 reference"`
- [ ] 8.5 提交 5: `git add docs/roadmap-status.md AGENTS.md && git commit -m "docs(status): sync Phase 2 status + Recent Changes (Sprint 11 P0 alignment)"`
- [ ] 8.6 提交 6 (optional): `git add openspec/changes/2026-06-26-doc-alignment-adr-states/ && git commit -m "chore(openspec): finalize doc-alignment-adr-states change artifacts"`

## 9. 同步到 master plan

- [ ] 9.1 编辑 `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C0 行:
  - 状态: `🟡 准备中` → `✅ archived (2026-06-26, 待 archive)` → `✅ archived (2026-06-26, archive 链接)`
  - 追加 archive 链接
- [ ] 9.2 编辑 master plan §六 风险表: C0 风险行标记 ✅ 已解决

## 10. 归档

- [ ] 10.1 `cd /workspace/project/HydraForge && openspec validate 2026-06-26-doc-alignment-adr-states` exit 0 (最后一次)
- [ ] 10.2 `openspec archive 2026-06-26-doc-alignment-adr-states --yes` 成功
- [ ] 10.3 验证: `openspec list` 不再显示本 change
- [ ] 10.4 验证: `openspec list --specs` 显示 `doc-alignment` spec 已合并

---

## 验证检查清单 (C0 ship gate)

- [ ] 1. ADR-0030 V2 草案存在 + V1 SUPERSEDED 标记
- [ ] 2. ADR-0032 状态修正 (方案 A 或 B)
- [ ] 3. 4 个 docs 同步 (implementation-roadmap / roadmap-status / AGENTS / README)
- [ ] 4. `python3 tools/adr_lint.py` exit 0
- [ ] 5. `python3 tools/docs_drift_audit.py` 0 critical drift
- [ ] 6. `openspec validate 2026-06-26-doc-alignment-adr-states` exit 0
- [ ] 7. `git status` clean
- [ ] 8. 6 commits 按文件分组
- [ ] 9. master plan C0 行状态更新
- [ ] 10. `openspec archive` 成功
