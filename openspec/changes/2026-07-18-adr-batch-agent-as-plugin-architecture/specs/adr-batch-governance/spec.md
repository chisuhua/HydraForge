# Capability: ADR Batch Governance (Agent-as-Plugin Architecture)

## Purpose

建立 OpenSpec change ↔ 14 个顶层 ADR (0052-0065) + 12 个 Skill 子 ADR (0061-01~12) 的双向链接, 提供 ADR ship gate 的 OpenSpec 跟踪载体。**本 capability 仅做治理链接, 不实施任何 ADR 的内容**。

## ADDED Requirements

### Requirement: ADR-to-OpenSpec bidirectional linkage

14 个顶层 ADR (`docs/adr/adr-0052-agent-plugin-manifest.md` 到 `docs/adr/adr-0065-multi-language-pdk-python.md`) + 12 个 Skill 子 ADR (`docs/adr/skill/adr-0061-{01..12}-*.md`) **MUST** 有 OpenSpec change 关联, 用于 ship gate 跟踪。

#### Scenario: ADR file links to OpenSpec change

- **WHEN** 读者打开任意一个 `docs/adr/adr-005*.md` 或 `docs/adr/adr-006*.md` 文件
- **THEN** 该 ADR 文件的 §关联 (或 §Status) 段落必须引用至少一个 `openspec/changes/` 目录名
- **AND** 该引用必须在 git 上存在 (即 OpenSpec change 已经创建且未删除)

#### Scenario: OpenSpec change references all 14 top-level ADRs

- **WHEN** 运行 `openspec change show 2026-07-18-adr-batch-agent-as-plugin-architecture --json`
- **THEN** 输出包含 14 个 ADR 文件路径 (在 `proposal.md` 的 ADR 关联表格中)
- **AND** 包含 12 个 Skill 子 ADR 文件路径 (在 `design.md` 的 ADR 关系图注释中)

#### Scenario: OpenSpec change validates before commit

- **WHEN** 在 commit OpenSpec change 前运行 `openspec validate 2026-07-18-adr-batch-agent-as-plugin-architecture --strict`
- **THEN** 退出码为 0 (无错误, 无警告)
- **AND** 至少 1 个 delta spec 文件存在 (本 capability 自身)

### Requirement: ADR status preserved until Sprint 25+ implementation

所有 14 个顶层 ADR + 12 个 Skill 子 ADR 在本 change ship 时**MUST 保持 🔍 Proposed 状态**, **SHALL NOT** 在此阶段翻转。后续 Sprint 实施对应 ADR 时, 单独创建 OpenSpec change 跟踪实施 + 翻转状态 (Proposed → Approved)。

#### Scenario: No ADR status flip in this change

- **WHEN** `openspec archive 2026-07-18-adr-batch-agent-as-plugin-architecture` 执行
- **THEN** 14 + 12 个 ADR 文件的 `## Status` (或 `## 状态`) 字段保持 `🔍 Proposed` (或等价表述)
- **AND** 不修改任何 ADR 的设计内容

### Requirement: Per-ADR implementation follow-up tracked

每个 ADR 的实施 **SHALL** 拆分为独立 OpenSpec change (例如 `2026-MM-DD-adr-0053-agent-descriptor-impl/`), 跟踪实施 + ADR 状态翻转 (Proposed → Approved)。

#### Scenario: Future ADR implementation change pattern

- **WHEN** 任意 ADR 进入实施阶段 (Sprint 25+)
- **THEN** 创建独立 OpenSpec change, name = `YYYY-MM-DD-adr-NNNN-{short-name}-impl/`
- **AND** 该 change 的 proposal.md 引用原 ADR 文件路径
- **AND** 该 change 包含至少 1 个 delta spec (实施的具体 capability)
- **AND** ship 时翻转对应 ADR 的 Status 字段 (Proposed → Approved)