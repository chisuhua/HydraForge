# Spec Delta: tech-debt-cleanup

> **关联 proposal**: `2026-06-24-tech-debt-and-phase1-closure/proposal.md`
> **关联 design**: `2026-06-24-tech-debt-and-phase1-closure/design.md`
> **关联 tasks**: `2026-06-24-tech-debt-and-phase1-closure/tasks.md`
>
> **说明**:本 delta spec 在 `openspec/specs/tech-debt-cleanup/spec.md` 末尾追加
> 6.3 follow-up 列表全关闭状态。Sprint 6 ship 时未在 spec.md 中明文记录,
> 仅在 `tech-debt-cleanup-sprint-6/tasks.md` §6.1/§6.3 留 STATUS NOTE。本 delta 形式化
> 关闭承诺,使 spec 完整。

## MODIFIED Requirements

### Requirement: tech-debt-cleanup-sprint6-followup-closed

The system MUST complete all 6 follow-up items from Sprint 6 STATUS NOTE §6.3
before `tech-debt-cleanup-sprint-6` is archived.

The system SHALL ship the 6 follow-up items per the table below, with `6.3.5` items routed to the
new change `2026-07-xx-engine-include-final-decoupling` if 1.5 day timebox overflows.

| ID | Item | Ship | Commit / Spec |
|---|---|---|---|
| 6.3.1 | Fork 处理重复块修复 | ✅ Sprint 7 Day 1 Blocker | `84c4c0a` |
| 6.3.2 | scheduler factory 死代码 | ✅ 本 change Step 8 | `tech-debt-and-phase1-closure` |
| 6.3.3 | handle_node_completion + execute ≤ 60 | ✅ Sprint 8 | `76c8d49` + `bd936af` |
| 6.3.4 | 15 个测试(7 scheduler + 5 parser + 3 engine_factory) | ✅ 本 change Step 10 | `tech-debt-and-phase1-closure` |
| 6.3.5 | engine.cpp 跨模块 include 10→≤3 | ✅ 本 change Step 12 | `tech-debt-and-phase1-closure` |
| 6.3.6 | `pending_dynamic_deps_` 访问器一致 | ✅ **Sprint 7 Day 8 (已 ship, 非本 change 工作)** | `75ded94` |

#### Scenario: STATUS NOTE §6.1 表格全 ✅

- **WHEN** `openspec/changes/tech-debt-cleanup-sprint-6/tasks.md` 更新后
- **THEN** §6.1 表格 6.3.1/2/3/4/5/6 行 MUST 状态标 ✅
- **AND** 引用本 OpenSpec change `tech-debt-and-phase1-closure` 作为完成依据

#### Scenario: archive 前置条件

- **WHEN** `openspec archive tech-debt-cleanup-sprint-6 --yes` 执行前
- **THEN** `openspec/changes/tech-debt-and-phase1-closure` MUST 全部 13 step [x]
- **AND** `ctest --output-on-failure` MUST ~49/49 PASS
- **AND** `cmake --preset asan && ctest --output-on-failure` MUST 0 error
- **AND** `cmake --preset tsan && ctest --output-on-failure` MUST 0 race
- **AND** `python3 tools/adr_lint.py docs/adr/` MUST exit 0
- **AND** `python3 tools/docs_drift_audit.py` MUST 0 critical drift
- **AND** `mcp__code-review-graph__get_hub_nodes --top_n 5` MUST execute out_degree < 30

#### Scenario: P2.C handoff 变体不阻塞 archive

- **WHEN** P2.C 1.5 day 时间盒超时,触发 handoff 变体
- **THEN** MUST 创建新 OpenSpec change `2026-07-xx-engine-include-final-decoupling` 正式 handoff 6.3.5
- **AND** `tech-debt-cleanup-sprint-6` archive MUST 仍可进行(非 ship-as-is 留账)
- **AND** 本 spec §6.3.5 行 MUST 标"⏳ Handoff to 2026-07-xx-engine-include-final-decoupling"而非 ✅

#### Scenario: P2.F TSan/ASan 历史 race 优雅降级

- **WHEN** P2.F 复验发现历史 race/leak(非本 change 引入)
- **THEN** MUST 记录为 pre-existing
- **AND** MUST 创建独立 OpenSpec change 跟踪修复
- **AND** `tech-debt-cleanup-sprint-6` archive MUST 仍可进行(ship gate 不阻塞 pre-existing)
- **AND** 本 spec §6.3 全 ✅ 不被历史 race 反推
