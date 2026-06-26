# Tasks: ADR-0033 — Session Hierarchy (三层会话模型)

> **STATUS: PLACEHOLDER** ⚠️ — 详细 tasks 待 Sprint 14 启动前填充
> **预估工时**: 1.5-2 周 (Sprint 15 主体)
> **关联 master plan**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C5

---

## 1. 决策前置 (Sprint 14 收官后)

- [ ] 1.1 评估: Session 持久化 (文件/内存) — 默认内存, 持久化推迟到 Phase 5
- [ ] 1.2 决策: breaking change vs 向后兼容 (建议: 向后兼容)
- [ ] 1.3 业务确认: messages 写保护粒度 (append-only vs full immutable)

## 2. 详细制定 (Sprint 15 Day 0)

- [ ] 2.1 写本 change proposal.md (What Changes 详细化)
- [ ] 2.2 写 design.md (5 个 Decision)
- [ ] 2.3 写本 tasks.md (10-15 sections, 30-50 tasks)
- [ ] 2.4 写 specs/session-hierarchy/spec.md (5-8 ADDED Requirements)
- [ ] 2.5 移除所有 "PLACEHOLDER" 标记, 更新 STATUS 行

## 3. Session 三层实施 (Sprint 15 Day 1-5)

- [ ] 3.1 TBD: UserSession 类
- [ ] 3.2 TBD: TaskSession 类
- [ ] 3.3 TBD: SubtaskSession 类
- [ ] 3.4 TBD: 三层 weak_ptr 引用关系

## 4. DSLEngine 重构 (Sprint 15 Day 6-9)

- [ ] 4.1 TBD: DSLEngine::run(session_id, ...) 重载
- [ ] 4.2 TBD: 向后兼容旧 run(Context)
- [ ] 4.3 TBD: ExecutionSession 重组
- [ ] 4.4 TBD: TopoScheduler Fork/Join SubtaskSession
- [ ] 4.5 TBD: CognitiveWorker IPER retry 复用

## 5. 测试 (Sprint 15 Day 10-13)

- [ ] 5.1 TBD: UserSession 单元测试
- [ ] 5.2 TBD: TaskSession 单元测试
- [ ] 5.3 TBD: SubtaskSession 单元测试
- [ ] 5.4 TBD: 集成测试 (DSLEngine 端到端)
- [ ] 5.5 TBD: Fork/Join SubtaskSession 隔离测试
- [ ] 5.6 TBD: messages 追加写保护测试

## 6. 验证 (Sprint 15 Day 14)

- [ ] 6.1 `ctest --output-on-failure` ≥ 47/47 + 新增 N 个 PASS
- [ ] 6.2 `cmake --preset tsan && ctest` 0 race
- [ ] 6.3 `cmake --preset asan && ctest` 0 leak
- [ ] 6.4 `python3 tools/adr_lint.py` exit 0
- [ ] 6.5 `python3 tools/docs_drift_audit.py` 0 critical drift
- [ ] 6.6 `openspec validate 2026-06-26-adr-0033-session-hierarchy` exit 0
- [ ] 6.7 `git status` clean

## 7. 同步与归档 (Sprint 15 Day 15)

- [ ] 7.1 更新 `docs/adr/adr-0033-session-hierarchy.md`: 🟡 Partial → ✅ Approved
- [ ] 7.2 更新 `docs/README.md` § adr/ 状态表
- [ ] 7.3 更新 `docs/roadmap-status.md` §一
- [ ] 7.4 更新 `AGENTS.md` § Recent Changes
- [ ] 7.5 同步 PDK 头文件: `./scripts/sync-pdk.sh`
- [ ] 7.6 `openspec archive 2026-06-26-adr-0033-session-hierarchy --yes`
- [ ] 7.7 同步 master plan C5 行: 状态更新

---

## 验证检查清单 (C5 ship gate)

- [ ] 1. ADR-0033 完整 design
- [ ] 2. 三层 Session 完整工作
- [ ] 3. DSLEngine::run(session_id, ...) 重载可用
- [ ] 4. Fork/Join 隔离工作
- [ ] 5. IPER retry 复用工作
- [ ] 6. messages 追加写保护工作
- [ ] 7. ctest 全绿 (含新增 tests)
- [ ] 8. ASan/TSan 100% clean
- [ ] 9. `openspec validate` exit 0
- [ ] 10. ADR-0033 status ✅ Approved
- [ ] 11. master plan C5 状态更新
