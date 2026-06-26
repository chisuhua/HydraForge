# doc-alignment Specification

> **Purpose**: 追踪 ADR 状态与文档一致性, 确保 Phase 2-5 后续 change 有准确 ADR 引用

## ADDED Requirements

### Requirement: adr-0030-v2-exists

`docs/adr/adr-0030-async-runtime-v2.md` MUST 存在, 描述 Phase 2 异步架构 (替代 V1)

#### Scenario: ADR-0030 V2 文件存在

- **WHEN** `test -f docs/adr/adr-0030-async-runtime-v2.md`
- **THEN** MUST 返回 0 (文件存在)
- **AND** frontmatter MUST 含 `status: Proposed` 或 `🔍 Proposed`

### Requirement: adr-0030-v1-superseded-marked

`docs/archive/adr/adr-0030-async-runtime-dual-layer.md` MUST 在 frontmatter 顶部标记 V1 SUPERSEDED by V2

#### Scenario: V1 SUPERSEDED 标记

- **WHEN** `head -5 docs/archive/adr/adr-0030-async-runtime-dual-layer.md`
- **THEN** MUST 含 "SUPERSEDED" + "V2" 字样

### Requirement: adr-0032-status-corrected

`docs/archive/adr/adr-0032-cost-collector.md` (或 `docs/adr/adr-0032-cost-collector.md` 视决策方案) MUST 状态修正为 `🟡 Partial`

#### Scenario: ADR-0032 状态非 ❌ Not Implemented

- **WHEN** `head -10 docs/archive/adr/adr-0032-cost-collector.md (or docs/adr/)`
- **THEN** MUST 含 `🟡 Partial` 字样
- **AND** MUST NOT 仍标记 `❌ Not Implemented`

### Requirement: implementation-roadmap-phase2-updated

`docs/implementation-roadmap.md` §Phase 2 段 MUST 引用 ADR-0030 V2 (而非 V1) + 标注 Slice 00 已 ship

#### Scenario: Phase 2 ADR 引用 V2

- **WHEN** `grep -A 3 "Phase 2" docs/implementation-roadmap.md | head -20`
- **THEN** MUST 含 `ADR-0030 V2` 或 `adr-0030-async-runtime-v2` 字样
- **AND** MUST 含 `Slice 00` 已 ship 提示

### Requirement: roadmap-status-phase2-pending-sprint-12

`docs/roadmap-status.md` §一 Phase 2 行 MUST 状态更新为 "待启动 Sprint 12"

#### Scenario: Phase 2 状态描述

- **WHEN** `grep "Phase 2" docs/roadmap-status.md | head -5`
- **THEN** MUST 含 "Sprint 12" 或 "待启动" 字样
- **AND** MUST NOT 仍描述为"阻塞中"

### Requirement: agents-md-recent-changes-2026-06-26

`AGENTS.md` § Recent Changes 顶部 MUST 含 2026-06-26 行 (Sprint 11 启动)

#### Scenario: AGENTS.md 顶部 Recent Changes

- **WHEN** `head -50 AGENTS.md | grep "2026-06-26"`
- **THEN** MUST 命中 ≥ 1 行
- **AND** MUST 含 `doc-alignment-adr-states` 字样

### Requirement: ship-gate-validations

C0 ship gate 全部验证 MUST pass

#### Scenario: 验证脚本全通过

- **WHEN** `cd /workspace/project/HydraForge && python3 tools/adr_lint.py docs/adr/ docs/archive/adr/ docs/adr/plugin/`
- **THEN** MUST exit 0
- **WHEN** `python3 tools/docs_drift_audit.py`
- **THEN** MUST 返回 0 critical drift
- **WHEN** `openspec validate 2026-06-26-doc-alignment-adr-states`
- **THEN** MUST exit 0
- **AND** `git status` MUST clean
