## ADDED Requirements

### Requirement: ADR-0072 翻牌必须通过完整 single-developer 治理流程

ADR-0072 状态从 🔍 Proposed 翻牌至 🟡 Partial 的决策 MUST 走完整 single-developer 治理流程：GitHub issue 用 `adr-review.md` 模板创建 + 12 项 Self-Review Checklist 全勾选 + ≥24h cooling-off 期间 + OpenSpec change archive。该流程由本次 governance change `2026-09-02-adr-0072-flip-to-partial` 显式落地证据链。

#### Scenario: 翻牌治理证据链完整
- **WHEN** ADR-0072 状态字段从 🔍 Proposed 翻牌至 🟡 Partial
- **THEN** `openspec/changes/archive/` 存在对应 archive 目录，包含 4 个 artifacts（proposal.md + design.md + tasks.md + specs/adr-0072-status-update/spec.md）

#### Scenario: Self-Review Checklist 12 项全勾选
- **WHEN** GitHub issue 用 `adr-review.md` 模板创建
- **THEN** 12 项 Self-Review Checklist 全 ✅ 勾选并留评论记录

#### Scenario: 24h cooling-off 满足
- **WHEN** GitHub issue 创建时间 vs OpenSpec change archive 时间
- **THEN** 时间差 ≥ 86400 秒（≥ 24 小时），cooling-off 证据完整

### Requirement: ADR 状态字段与 OpenSpec 证据链必须一致

ADR 状态字段（`docs/adr/<name>.md` §状态）的任何变化 MUST 有对应的 OpenSpec change archive 证据链。任何"实施先于翻牌"异常必须在 ADR 文件中显式声明（如 footer / §治理异常段）并待后续 OpenSpec change 补建。

#### Scenario: 翻牌异常显式文档化
- **WHEN** ADR 状态字段已翻牌但 OpenSpec change 不存在
- **THEN** ADR 文件含治理异常声明段（如 "实施先于翻牌异常，待建 OpenSpec change `2026-09-02-adr-0072-flip-to-partial`"）

#### Scenario: 翻牌证据链可追溯
- **WHEN** 审计或 Phase 7a 复评需要验证 ADR-0072 翻牌证据
- **THEN** `iteration.json` 含 archived entry + `openspec/changes/archive/2026-09-02-adr-0072-flip-to-partial/` 目录完整 + GitHub issue Closed

### Requirement: 翻牌 change 不重新翻牌 ADR

本次治理补建 change MUST NOT 改变 ADR-0072 的 §状态字段（已翻牌至 🟡 Partial）。本次仅补证据链，不再次翻牌，避免反复。

#### Scenario: 状态字段保持不变
- **WHEN** archive 完成
- **THEN** `docs/adr/adr-0072-dsl-node-extensions.md` §状态字段仍为 `🟡 Partial (2026-09-02)`，未变更
