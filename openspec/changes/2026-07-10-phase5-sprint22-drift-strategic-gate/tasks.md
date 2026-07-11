# Tasks: Phase 5 — Sprint 22 Drift + Strategic Gate (C18)

> **STATUS: ACTIVE** 🟡
> **关联 proposal**: `proposal.md`
> **关联 spec**: `specs/sprint22-gates/spec.md`
> **前置依赖**: C17 ship ✅ (soft)
> **预估工时**: 2-3 天
> **最后更新**: 2026-07-10

---

## 1. Drift Audit (Day 1)

- [x] 1.1 跑 `python3 tools/check_roadmap_drift.py` 获取当前 drift 列表
- [x] 1.2 跑 `python3 tools/adr_lint.py docs/adr/` 获取 ADR lint 结果
- [x] 1.3 跑 `python3 tools/docs_drift_audit.py` 获取 ADR drift 结果
- [x] 1.4 比对 `docs/README.md` §adr/ 表格 Approved 行数 vs 实际 ADR Approved 数
- [x] 1.5 比对 `docs/adr-management/relationships.md` vs 主文档
- [x] 1.6 比对 `docs/active-status.md` §一 vs `master plan §一`
- [x] 1.7 输出 `docs/audits/2026-07-10-drift-gate.md` (含 4 路检测结果)
- [x] 1.8 若发现 CRITICAL drift, 创建 fix change (本 change 范围之外)

---

## 2. Oracle Strategic Alignment Gate (Day 2)

- [x] 2.1 准备 Oracle session 输入:
  - Phase 5 收官状态 (8 changes shipped, 72 ctest, 19 Approved/72 ctest, 19 Approved ctest, 19 Approved ADR)
  - 4 个 Phase 6 候选方向 (A 自进化 / B 服务化 / C 第三方生态 / D Cloud-native)
  - 当前资源约束 (团队 4-6 周 vs 8-12 周)
- [x] 2.2 发起 Oracle session (估时 30-60min)
- [x] 2.3 记录 Oracle 决议 + 引用 session ID
- [x] 2.4 写 `docs/adr/adr-0050-phase6-strategic-evaluation.md`:
  - §背景 (Phase 5 收官评估)
  - §候选方向 (4 个 + 估时 + 依赖 + 风险)
  - §决策 (推荐方向 + Oracle 引用)
  - §启动条件 (Phase 6 硬前置)
  - §不变量 (跨候选保持的契约)
- [x] 2.5 验证 `python3 tools/adr_lint.py adr-0050-*.md` exit 0

---

## 3. Stage Gate Evaluation (Day 3 上午)

- [x] 3.1 准备 Stage 1 → 2 切换清单 (7 项):
  - C10 ship + 2 周稳定 (2026-07-03 ~ 2026-07-17)
  - C11 ship + 2 周稳定 (2026-07-04 ~ 2026-07-18)
  - C12 ship + 2 周稳定 (2026-07-04 ~ 2026-07-18)
  - ctest 72 ctest, 19 Approved/72 ctest, 19 Approved + ASan 72 ctest, 19 Approved/72 ctest, 19 Approved + TSan 验证
  - 推理标准库 7/7 子图 ship
  - C19 触发条件 (deep_copy 瓶颈 OR Session 迁移需求)
  - 团队 3-5 天时间投入可用
- [x] 3.2 写 `docs/handoff/2026-07-31-stage-gate-evaluation.md`:
  - §背景 (Stage 1 收官状态)
  - §评估清单 (7 项 + 结果 + 证据)
  - §决议 (启动 Stage 2 / 推迟 / 部分启动)
  - §决策依据 (Oracle 引用 + 用户决策记录)
  - §后续行动 (C19 placeholder 填实或保留)
- [x] 3.3 若决议启动 Stage 2, 准备 C19 启动 proposal (含 tasks.md + spec.md), 但不创建 OpenSpec change (留 Sprint 23)

---

## 4. 文档同步 (Day 3 下午)

- [x] 4.1 更新 `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md`:
  - §十 Drift Log 追加 C17 + C18 行
  - §十一 Adjustment Log 追加 C19/C20 触发条件 + C16 §5 实施依赖
  - §十二 Strategic Pivots Log 追加 Phase 5 收官评估 (C18 决议)
- [x] 4.2 更新 `docs/superpowers/plans/2026-07-10-phase5-remainder-adr-sync.md`:
  - §四 C18 状态从 ⚪ immediate → ✅ shipped
  - §十 Drift Log 追加本 change 行
  - §十一 Adjustment Log 追加 C19/C20 评估结果
- [x] 4.3 更新 `docs/active-status.md`:
  - §一 添加 `**ADR Approved**` 计数 +5 (C17 后)
  - §一 添加 `Phase 6` 状态行 (基于 C18 决议, 例如 `🟡 待启动`)
  - §六 下一步行动更新 (Phase 6 启动条件 + C19/C20 决策)
- [x] 4.4 AGENTS.md § Recent Changes 追加 C18 ship 记录

---

## 5. 验证 (Day 3 下午)

- [x] 5.1 `python3 tools/check_roadmap_drift.py` exit 0
- [x] 5.2 `python3 tools/adr_lint.py` exit 0
- [x] 5.3 `python3 tools/docs_drift_audit.py` 0 DRIFT
- [x] 5.4 `git grep "🔍 Proposed" docs/adr/` 计数 ≤ 13 (含 adr-0050 新增)
- [x] 5.5 `ls docs/handoff/2026-07-31-stage-gate-evaluation.md` 文件存在
- [x] 5.6 `openspec validate 2026-07-10-phase5-sprint22-drift-strategic-gate` exit 0

---

## 6. 收尾 (Day 3 下午)

- [x] 6.1 `git add . && git commit -m "docs(review-gates): C18 — Sprint 22 Drift + Strategic + Stage Gate evaluation"`
- [x] 6.2 `openspec archive 2026-07-10-phase5-sprint22-drift-strategic-gate --yes`
- [x] 6.3 验证: `openspec list` 仍返回 "No active changes found"
- [x] 6.4 通知用户 C18 ship + archived, 准备后续 Sprint 23 决策

---

## 备注

- 本 change 估时 2-3 天, Oracle session 估时 30-60min
- 若 Stage Gate 评估发现 C12 稳定运行不足 2 周 (实际 2026-07-04 ship, 评估 2026-07-10 仅 6 天), 文档化为 "提前评估 + 持续监控", Stage 2 启动决策延后至 2026-07-18 (2 周整)
- Oracle session ID 需记录在 adr-0050 §决策 段
- C19/C20 placeholder 在本 change 中保持不变, 仅追加触发条件到 §十一 Adjustment Log
- 若决议启动 Phase 6, 不在本 change 范围, 需独立新 master plan