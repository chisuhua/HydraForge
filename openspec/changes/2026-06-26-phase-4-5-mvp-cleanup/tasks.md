# Tasks: Phase 4.5 — MVP Cleanup

> **STATUS: PLACEHOLDER** ⚠️ — 详细 tasks 待 C6 完成后填充
> **预估工时**: 1-2 天 (Sprint 18 收尾)
> **关联 master plan**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C8
> **前置依赖**: C3 + C4 + C5 + C6 全部 ship

---

## 1. 决策前置 (C6 收官后)

- [ ] 1.1 评估: SimpleCognitiveOrchestrator 当前使用情况
- [ ] 1.2 决策: MockLLMProvider 降级 vs 保留
- [ ] 1.3 评估: examples/ 目录保留/移除/合并方案

## 2. 详细制定 (Sprint 18 Day 0)

- [ ] 2.1 写本 change proposal.md (What Changes 详细化)
- [ ] 2.2 写 design.md (5 个 Decision)
- [ ] 2.3 写本 tasks.md (5-10 sections, 10-20 tasks)
- [ ] 2.4 写 specs/phase-4-5-cleanup/spec.md (5-8 ADDED Requirements)
- [ ] 2.5 移除所有 "PLACEHOLDER" 标记, 更新 STATUS 行

## 3. SimpleCognitiveOrchestrator 替换 (Sprint 18 Day 1)

- [ ] 3.1 TBD: 评估当前使用
- [ ] 3.2 TBD: 替换为正式实现
- [ ] 3.3 TBD: 移除 TODO(mvp) 标记

## 4. MockLLMProvider 评估 (Sprint 18 Day 1)

- [ ] 4.1 TBD: 评估降级 vs 保留
- [ ] 4.2 TBD: 文档更新

## 5. examples/ 目录梳理 (Sprint 18 Day 1)

- [ ] 5.1 TBD: 评估保留/移除/合并
- [ ] 5.2 TBD: 更新 CMakeLists.txt

## 6. 文档更新 (Sprint 18 Day 1)

- [ ] 6.1 TBD: docs/specs/layer0.md 更新
- [ ] 6.2 TBD: docs/roadmap-status.md §一 100%
- [ ] 6.3 TBD: AGENTS.md § Recent Changes

## 7. 验证 (Sprint 18 Day 1)

- [ ] 7.1 `ctest --output-on-failure` ≥ 47/47 PASS
- [ ] 7.2 `cmake --preset tsan && ctest` 0 race
- [ ] 7.3 `cmake --preset asan && ctest` 0 leak
- [ ] 7.4 `python3 tools/adr_lint.py` exit 0
- [ ] 7.5 `python3 tools/docs_drift_audit.py` 0 critical drift
- [ ] 7.6 `openspec validate 2026-06-26-phase-4-5-mvp-cleanup` exit 0
- [ ] 7.7 `git status` clean

## 8. 同步与归档 (Sprint 18 Day 1)

- [ ] 8.1 更新 `docs/roadmap-status.md` §一 Phase 0-4.5 全部 100%
- [ ] 8.2 更新 `AGENTS.md` § Recent Changes: Phase 4.5 ship 标记
- [ ] 8.3 同步 PDK 头文件: `./scripts/sync-pdk.sh`
- [ ] 8.4 `openspec archive 2026-06-26-phase-4-5-mvp-cleanup --yes`
- [ ] 8.5 同步 master plan C8 行: 状态更新
- [ ] 8.6 触发 Phase 5 启动评估 (远期)

---

## 验证检查清单 (C8 ship gate)

- [ ] 1. Phase 4.5 完整 design
- [ ] 2. SimpleCognitiveOrchestrator 替换
- [ ] 3. MockLLMProvider 决策落地
- [ ] 4. examples/ 目录梳理
- [ ] 5. TODO(mvp) 全部移除
- [ ] 6. ctest 全绿
- [ ] 7. ASan/TSan 100% clean
- [ ] 8. `openspec validate` exit 0
- [ ] 9. Phase 0-4.5 全部 100%
- [ ] 10. master plan C8 状态更新
