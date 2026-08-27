# g11-closed-adr-0084-approved Specification

## ADDED Requirements

### Requirement: ADR-0084 状态字段必须标记 Approved (评审通过 2026-08-26)

The `docs/adr/adr-0084-mutation-governance-contract.md` §状态 section MUST display "✅ Approved (评审通过 2026-08-26 — V1 gate-and-audit 代码 ship, commit `a2b2d52`)"` immediately following the line heading.

#### Scenario: ADR 状态字段合规

- **WHEN** 静态检查 `grep -E "(🔍 Proposed|✅ Approved)" docs/adr/adr-0084-mutation-governance-contract.md | head -5`
- **THEN** 第 1-5 行内必须出现 ✅ Approved 字符串, 不应出现 🔍 Proposed

### Requirement: capability-map G11 状态字段必须标记 Closed

The `docs/architecture/capability-application-map-2026-08.md` §二 G11 row MUST display "✅ Closed (ADR-0084 Approved + V1 gate-and-audit 代码 ship, 2026-08-26)"。

#### Scenario: cap-map G11 行合规

- **WHEN** 静态检查 `grep "G11.*Closed\|G11.*Proposed" docs/architecture/capability-application-map-2026-08.md | head -5`
- **THEN** G11 行必须包含 ✅ Closed 标识

### Requirement: GitHub issue #14 必须关闭且含 audit trail

The GitHub issue #14 MUST be closed with a comment that includes:
1. ADR-0084 ✅ Approved 状态翻转声明
2. V1 代码 ship commit SHA (`a2b2d52`)
3. G11 Closed 同步声明
4. 决议依据 Oracle sessions 引用
5. 相关 OpenSpec change 路径 (`2026-08-26-adr-0084-mutation-governance-contract` 已 archive + `2026-08-26-g11-closed-adr-0084-approved` 本 change)

#### Scenario: Issue #14 state = CLOSED

- **WHEN** 验证 `gh issue view 14 --json state --jq .state`
- **THEN** 输出必须为 "CLOSED"

#### Scenario: Issue #14 comments 包含 audit trail

- **WHEN** 验证 `gh issue view 14 --comments`
- **THEN** 最后一条 comment 必须包含 5 项 audit trail 必填字段

### Requirement: adr_lint 必须通过

The `python3 tools/adr_lint.py` exit code MUST be 0 after all doc modifications。

#### Scenario: adr_lint 全过

- **WHEN** 运行 `python3 tools/adr_lint.py`
- **THEN** 退出码 0, 无 ADR lint 错误

### Requirement: docs_drift_audit 必须通过且无新增 CRITICAL

The `python3 tools/docs_drift_audit.py` exit code MUST be 0, with no NEW CRITICAL drift introduced by this change。

#### Scenario: docs_drift_audit 干净

- **WHEN** 运行 `python3 tools/docs_drift_audit.py`
- **THEN** 退出码 0, G11 相关 drift 为 0 项

### Requirement: openspec validate --strict 必须通过

The `openspec validate --changes --strict` MUST exit 0。

#### Scenario: openspec validate 干净

- **WHEN** 运行 `openspec validate --changes --strict`
- **THEN** 退出码 0, 本 change 通过

### Requirement: ctest 全量零回归

The `ctest --output-on-failure` MUST report ALL tests PASS with zero regressions relative to the baseline at V1 ship (commit `a2b2d52`)。本 change 零代码改动, 因此测试总数不变 (基线 = `a2b2d52` 实测 ctest 计数)。

#### Scenario: ctest 全量 PASS

- **WHEN** 运行 `ctest --output-on-failure`
- **THEN** 所有测试 PASS, 0 failures
- **AND** 测试计数 ≥ V1 ship 基线 (动态计数, 禁止硬编码)

### Requirement: active-status G11 跟踪段必须移除 issue #14 OPEN 声明

The `docs/active-status.md` §一 G11 跟踪段 MUST NOT contain "issue #14 保持 OPEN" or "T19 GEPA Phase 1 只读反思约束（不执行 commit(PromptEdit)）直至 G11 ADR Approved" 声明。

#### Scenario: G11 跟踪段同步

- **WHEN** 静态检查 `grep "issue #14\|T19 GEPA Phase 1" docs/active-status.md`
- **THEN** 不应出现 issue #14 保持 OPEN 或 T19 GEPA Phase 1 只读约束声明
- **AND** 应出现 issue #14 Closed + T19 GEPA Phase 2 commit 解锁声明