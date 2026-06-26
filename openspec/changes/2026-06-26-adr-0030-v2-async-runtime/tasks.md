# Tasks: ADR-0030 V2 — Async Runtime (Phase 2 入口)

> **STATUS: PLACEHOLDER** ⚠️ — 详细 tasks 待 C1 完成后填充
> **触发条件**: C1 (`2026-06-26-sprint-7-tech-debt-execution`) ship
> **预估工时**: 1.5-2 周 (Sprint 12 主体)
> **关联 master plan**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C2

---

## 1. 决策前置 (Sprint 11 收官前)

- [ ] 1.1 咨询 Oracle: 双层架构 vs std::jthread 替代 (Open Question 1)
- [ ] 1.2 业务确认: Fleet 16 路 LLM 并行真实使用场景 (Open Question 2)
- [ ] 1.3 决策: LLM Token 流式推送用协程 yield 还是 IGenerationStream 扩展 (Open Question 3)

## 2. 详细制定 (Sprint 12 Day 1)

- [ ] 2.1 写 ADR-0030 V2 完整 design (基于 §1 决策)
- [ ] 2.2 完善本 change proposal.md (What Changes 详细化)
- [ ] 2.3 写 design.md (5 个 Decision)
- [ ] 2.4 写本 tasks.md (10-15 sections, 30-50 tasks)
- [ ] 2.5 写 specs/async-runtime/spec.md (5-8 ADDED Requirements)
- [ ] 2.6 移除所有 "PLACEHOLDER" 标记, 更新 STATUS 行

## 3. ADR-0030 V2 Approved (Sprint 12 Day 1)

- [ ] 3.1 编辑 `docs/adr/adr-0030-async-runtime-v2.md` frontmatter: status=🔍 Proposed → ✅ Approved
- [ ] 3.2 编辑 `docs/README.md` § adr/ 状态表: 更新 ADR-0030 V2 行
- [ ] 3.3 同步 `docs/implementation-roadmap.md` §Phase 2 描述
- [ ] 3.4 同步 `docs/roadmap-status.md` §一 Phase 2 状态
- [ ] 3.5 提交: `git commit -m "docs(adr): approve ADR-0030 V2 (Phase 2 async runtime)"`

## 4. Phase 2 实施 (Sprint 12 Day 2-10) — 待设计

- [ ] 4.1 TBD: 并行 DAG executor 实施
- [ ] 4.2 TBD: Fleet 模式 16 路 LLM 并行
- [ ] 4.3 TBD: LLM Token 流式推送
- [ ] 4.4 TBD: 用户审批等待

## 5. 测试 (Sprint 12 Day 8-10)

- [ ] 5.1 TBD: 并行调度测试
- [ ] 5.2 TBD: Fleet 16 路并发测试
- [ ] 5.3 TBD: TSan 验证 (高并发)

## 6. 验证 (Sprint 12 Day 11)

- [ ] 6.1 `ctest --output-on-failure` ≥ 47/47 + 新增 N 个 PASS
- [ ] 6.2 `cmake --preset tsan && ctest` 0 race
- [ ] 6.3 `cmake --preset asan && ctest` 0 leak
- [ ] 6.4 `python3 tools/adr_lint.py` exit 0
- [ ] 6.5 `python3 tools/docs_drift_audit.py` 0 critical drift
- [ ] 6.6 `openspec validate 2026-06-26-adr-0030-v2-async-runtime` exit 0
- [ ] 6.7 `git status` clean

## 7. 同步与归档 (Sprint 12 Day 12)

- [ ] 7.1 更新 `docs/roadmap-status.md` §一 Phase 2 状态: 0% → 100%
- [ ] 7.2 更新 `AGENTS.md` § Recent Changes
- [ ] 7.3 同步 PDK 头文件: `./scripts/sync-pdk.sh`
- [ ] 7.4 `openspec archive 2026-06-26-adr-0030-v2-async-runtime --yes`
- [ ] 7.5 同步 master plan C2 行: 状态更新

---

## 验证检查清单 (C2 ship gate)

- [ ] 1. ADR-0030 V2 完整 design
- [ ] 2. 3 个 Open Questions 全部决策
- [ ] 3. ctest 全绿 (含新增测试)
- [ ] 4. ASan/TSan 100% clean
- [ ] 5. `openspec validate` exit 0
- [ ] 6. ADR-0030 V2 status ✅ Approved
- [ ] 7. Phase 2 在 roadmap-status.md 100%
- [ ] 8. master plan C2 状态更新
