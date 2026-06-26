# Tasks: ADR-0031 P1-P2 — IExecutionPolicy + Approval Mechanism

> **STATUS: PLACEHOLDER** ⚠️ — 详细 tasks 待 Sprint 12 启动前 (2026-07-30 前后) 填充
> **预估工时**: 2 周 (Sprint 13 主体)
> **关联 master plan**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C3

---

## 1. 决策前置 (Sprint 12 收官后)

- [ ] 1.1 咨询 Oracle: 4 虚函数 vs 6 虚函数 (参考 ADR-0031 当前设计)
- [ ] 1.2 决策: 审批机制 EventBus vs callback (依 ADR-0019 集成现状)
- [ ] 1.3 业务确认: Plan/Agent/YOLO 模式定义是否需要扩展

## 2. 详细制定 (Sprint 13 Day 0)

- [ ] 2.1 写本 change proposal.md (What Changes 详细化)
- [ ] 2.2 写 design.md (5 个 Decision)
- [ ] 2.3 写本 tasks.md (10-15 sections, 30-50 tasks)
- [ ] 2.4 写 specs/execution-policy/spec.md (5-8 ADDED Requirements)
- [ ] 2.5 移除所有 "PLACEHOLDER" 标记, 更新 STATUS 行

## 3. P1 实施 (Sprint 13 Day 1-3) — IExecutionPolicy 完整实现

- [ ] 3.1 TBD: 头文件扩展 4 虚函数
- [ ] 3.2 TBD: PlanPolicy 实现
- [ ] 3.3 TBD: AgentPolicy 实现
- [ ] 3.4 TBD: YOLOPolicy 实现
- [ ] 3.5 TBD: ToolMetadata V1 结构

## 4. P2 实施 (Sprint 13 Day 4-7) — 审批机制

- [ ] 4.1 TBD: EventBus event 类型定义
- [ ] 4.2 TBD: ApprovalCoordinator 中间件
- [ ] 4.3 TBD: TUI `/apply` 命令桥接

## 5. 集成 (Sprint 13 Day 8-9)

- [ ] 5.1 TBD: NodeExecutor 集成 ApprovalCoordinator
- [ ] 5.2 TBD: DSLEngine 注入默认 Policy (YOLOPolicy 作为 MVP)
- [ ] 5.3 TBD: CognitiveWorker 集成

## 6. 测试 (Sprint 13 Day 10-12)

- [ ] 6.1 TBD: 3 Policy 单元测试 (5 case × 3 = 15 tests)
- [ ] 6.2 TBD: ApprovalCoordinator 单元测试 (5 tests)
- [ ] 6.3 TBD: TUI `/apply` 集成测试
- [ ] 6.4 TBD: 端到端审批流程 E2E

## 7. 验证 (Sprint 13 Day 13)

- [ ] 7.1 `ctest --output-on-failure` ≥ 47/47 + 新增 20+ PASS
- [ ] 7.2 `cmake --preset tsan && ctest` 0 race
- [ ] 7.3 `cmake --preset asan && ctest` 0 leak
- [ ] 7.4 `python3 tools/adr_lint.py` exit 0
- [ ] 7.5 `python3 tools/docs_drift_audit.py` 0 critical drift
- [ ] 7.6 `openspec validate 2026-06-26-adr-0031-p1p2-execution-policy` exit 0
- [ ] 7.7 `git status` clean

## 8. 同步与归档 (Sprint 13 Day 14)

- [ ] 8.1 更新 `docs/adr/adr-0031-execution-policy.md`: 🟡 Partial → ✅ Approved (P1-P2 部分)
- [ ] 8.2 更新 `docs/README.md` § adr/ 状态表
- [ ] 8.3 更新 `docs/roadmap-status.md` §一
- [ ] 8.4 更新 `AGENTS.md` § Recent Changes
- [ ] 8.5 同步 PDK 头文件: `./scripts/sync-pdk.sh`
- [ ] 8.6 `openspec archive 2026-06-26-adr-0031-p1p2-execution-policy --yes`
- [ ] 8.7 同步 master plan C3 行: 状态更新
- [ ] 8.8 触发 C4 (P3-P4) 启动准备

---

## 验证检查清单 (C3 ship gate)

- [ ] 1. ADR-0031 P1-P2 完整 design
- [ ] 2. IExecutionPolicy 4 虚函数完整
- [ ] 3. 3 个默认 Policy 可用
- [ ] 4. ApprovalCoordinator 工作
- [ ] 5. TUI `/apply` 桥接工作
- [ ] 6. ctest 全绿 (含新增 20+ tests)
- [ ] 7. ASan/TSan 100% clean
- [ ] 8. `openspec validate` exit 0
- [ ] 9. ADR-0031 P1-P2 status ✅ Approved
- [ ] 10. master plan C3 状态更新
