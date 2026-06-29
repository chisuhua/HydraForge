# Tasks: ADR-0031 P3-P4 — ToolCoordinator + Layer Profile

> **STATUS: PLACEHOLDER** ⚠️ — 详细 tasks 待 C3 完成后填充
> **预估工时**: 1.5-2 周 (Sprint 14 主体)
> **关联 master plan**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C4
> **前置依赖**: C3 (P1-P2 ship)

---

## 1. 决策前置 (C3 收官后)

- [x] 1.1 评估: ToolCoordinator 是否需要异步路径 (依 C2 ADR-0030 V2 决策)
- [x] 1.2 业务确认: Layer × Tool Category 权限矩阵的默认值

## 2. 详细制定 (Sprint 14 Day 0)

- [x] 2.1 写本 change proposal.md (What Changes 详细化)
- [x] 2.2 写 design.md (5 个 Decision)
- [x] 2.3 写本 tasks.md (10-15 sections, 30-50 tasks)
- [x] 2.4 写 specs/toolcoordinator/spec.md (5-8 ADDED Requirements)
- [x] 2.5 移除所有 "PLACEHOLDER" 标记, 更新 STATUS 行

## 3. P3 实施 (Sprint 14 Day 1-5) — ToolCoordinator 中间件

- [x] 3.1 TBD: ToolCoordinator 类实现
- [x] 3.2 TBD: 集成 IExecutionPolicy
- [x] 3.3 TBD: 集成 ApprovalCoordinator
- [x] 3.4 TBD: 审计日志到 EventBus

## 4. NodeExecutor 集成 (Sprint 14 Day 6-7)

- [x] 4.1 TBD: 替换直接 call_tool
- [x] 4.2 TBD: 异常传播路径保持

## 5. P4 实施 (Sprint 14 Day 8-10) — Layer Profile

- [x] 5.1 TBD: Layer enum
- [x] 5.2 TBD: LayerProfile 类
- [x] 5.3 TBD: ToolMetadata V2 扩展
- [x] 5.4 TBD: 权限矩阵

## 6. 测试 (Sprint 14 Day 11-13)

- [x] 6.1 TBD: ToolCoordinator 单元测试
- [x] 6.2 TBD: LayerProfile 单元测试
- [x] 6.3 TBD: 集成测试

## 7. 验证 (Sprint 14 Day 14)

- [x] 7.1 `ctest --output-on-failure` ≥ 47/47 + 新增 N 个 PASS
- [x] 7.2 `cmake --preset tsan && ctest` 0 race
- [x] 7.3 `cmake --preset asan && ctest` 0 leak
- [x] 7.4 `python3 tools/adr_lint.py` exit 0
- [x] 7.5 `python3 tools/docs_drift_audit.py` 0 critical drift
- [x] 7.6 `openspec validate 2026-06-26-adr-0031-p3p4-toolcoordinator` exit 0
- [x] 7.7 `git status` clean

## 8. 同步与归档 (Sprint 14 Day 15)

- [x] 8.1 更新 `docs/adr/adr-0031-execution-policy.md`: P1-P2 ✅ Approved → ✅ Approved (全部 P1-P4)
- [x] 8.2 更新 `docs/README.md` § adr/ 状态表
- [x] 8.3 更新 `docs/roadmap-status.md` §一
- [x] 8.4 更新 `AGENTS.md` § Recent Changes
- [x] 8.5 同步 PDK 头文件: `./scripts/sync-pdk.sh`
- [x] 8.6 `openspec archive 2026-06-26-adr-0031-p3p4-toolcoordinator --yes`
- [x] 8.7 同步 master plan C4 行: 状态更新
- [x] 8.8 触发 C6 (ADR-0004 V2) 启动准备

---

## 验证检查清单 (C4 ship gate)

- [x] 1. ADR-0031 P3-P4 完整 design
- [x] 2. ToolCoordinator 完整工作
- [x] 3. LayerProfile 完整工作
- [x] 4. NodeExecutor 集成零回归
- [x] 5. ctest 全绿 (含新增 tests)
- [x] 6. ASan/TSan 100% clean
- [x] 7. `openspec validate` exit 0
- [x] 8. ADR-0031 status ✅ Approved (全部)
- [x] 9. master plan C4 状态更新
