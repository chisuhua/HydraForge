# Tasks: ADR-0004 V2 — ToolRegistry Security (Metadata + Approval)

> **STATUS: PLACEHOLDER** ⚠️ — 详细 tasks 待 C4 完成后填充
> **预估工时**: 1 周 (Sprint 16 主体)
> **关联 master plan**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C6
> **前置依赖**: C3 + C4 全部 ship

---

## 1. 决策前置 (C4 收官后)

- [ ] 1.1 评估: DECLARE_TOOL breaking change 迁移路径
- [ ] 1.2 决策: Layer × Tool Category 权限矩阵默认值
- [ ] 1.3 业务确认: 现有 plugin 迁移窗口

## 2. 详细制定 (Sprint 16 Day 0)

- [ ] 2.1 写本 change proposal.md (What Changes 详细化)
- [ ] 2.2 写 design.md (5 个 Decision)
- [ ] 2.3 写本 tasks.md (10-15 sections, 30-50 tasks)
- [ ] 2.4 写 specs/toolregistry-security-v2/spec.md (5-8 ADDED Requirements)
- [ ] 2.5 移除所有 "PLACEHOLDER" 标记, 更新 STATUS 行

## 3. ToolMetadata V2 集成 (Sprint 16 Day 1-3)

- [ ] 3.1 TBD: ToolMetadata V2 字段完整
- [ ] 3.2 TBD: ToolRegistry 注册 validation

## 4. DECLARE_TOOL 宏扩展 (Sprint 16 Day 4-5)

- [ ] 4.1 TBD: PDK 宏升级
- [ ] 4.2 TBD: 编译时检查
- [ ] 4.3 TBD: 示例 plugin 更新

## 5. 审批工作流集成 (Sprint 16 Day 6-7)

- [ ] 5.1 TBD: ToolCoordinator + IExecutionPolicy 联动
- [ ] 5.2 TBD: TUI `/apply` 桥接增强

## 6. Layer × Tool Category 权限矩阵 (Sprint 16 Day 8-9)

- [ ] 6.1 TBD: 权限矩阵定义
- [ ] 6.2 TBD: 启动时 validation
- [ ] 6.3 TBD: 运行时 enforcement

## 7. 测试 (Sprint 16 Day 10-12)

- [ ] 7.1 TBD: ToolRegistry V2 单元测试
- [ ] 7.2 TBD: DECLARE_TOOL 编译测试
- [ ] 7.3 TBD: 权限矩阵 enforcement 测试
- [ ] 7.4 TBD: 集成测试

## 8. 验证 (Sprint 16 Day 13)

- [ ] 8.1 `ctest --output-on-failure` ≥ 47/47 + 新增 N 个 PASS
- [ ] 8.2 `cmake --preset tsan && ctest` 0 race
- [ ] 8.3 `cmake --preset asan && ctest` 0 leak
- [ ] 8.4 `python3 tools/adr_lint.py` exit 0
- [ ] 8.5 `python3 tools/docs_drift_audit.py` 0 critical drift
- [ ] 8.6 `openspec validate 2026-06-26-adr-0004-v2-metadata-approval` exit 0
- [ ] 8.7 `git status` clean

## 9. 同步与归档 (Sprint 16 Day 14)

- [ ] 9.1 更新 `docs/adr/adr-0004-toolregistry-security.md`: ✅ Approved → ✅ Approved (V2)
- [ ] 9.2 更新 `docs/README.md` § adr/ 状态表
- [ ] 9.3 更新 `docs/roadmap-status.md` §一
- [ ] 9.4 更新 `AGENTS.md` § Recent Changes
- [ ] 9.5 同步 PDK 头文件: `./scripts/sync-pdk.sh` (双仓库同步)
- [ ] 9.6 `openspec archive 2026-06-26-adr-0004-v2-metadata-approval --yes`
- [ ] 9.7 同步 master plan C6 行: 状态更新
- [ ] 9.8 触发 C8 (Phase 4.5 MVP 清理) 启动准备

---

## 验证检查清单 (C6 ship gate)

- [ ] 1. ADR-0004 V2 完整 design
- [ ] 2. ToolMetadata V2 完整集成
- [ ] 3. DECLARE_TOOL 宏扩展工作
- [ ] 4. 审批工作流集成
- [ ] 5. 权限矩阵 enforcement
- [ ] 6. ctest 全绿 (含新增 tests)
- [ ] 7. ASan/TSan 100% clean
- [ ] 8. `openspec validate` exit 0
- [ ] 9. ADR-0004 status ✅ Approved (V2)
- [ ] 10. master plan C6 状态更新
