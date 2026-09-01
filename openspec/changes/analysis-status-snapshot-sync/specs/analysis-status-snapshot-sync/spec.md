## ADDED Requirements

### Requirement: 分析文档状态与 ADR 文件一致 (MUST apply)

本条 MUST 满足：3 份架构分析文档中引用的 ADR 状态必须与 `docs/adr/*.md` 各自状态字段一致，不得滞后超过一次评审周期。

#### Scenario: gap-analysis 中 ADR-0068/0069/0070 状态正确
- **WHEN** 阅读 `docs/architecture/adr-implementation-status-gap-analysis.md` L166-168
- **THEN** ADR-0068 标注 ✅ Approved (2026-08-03)
- **AND** ADR-0069/0070 标注 🟡 Partial (2026-08-04)

#### Scenario: gap-analysis §一 计数与 adr_lint 一致
- **WHEN** 运行 `python3 tools/adr_lint.py` 对比 gap-analysis §一 总表
- **THEN** Approved/Partial/Proposed 计数与占比可复现

#### Scenario: llm-native-blueprint 中 0071/0074/0075 状态正确
- **WHEN** 阅读 `docs/architecture/llm-native-blueprint-vs-code-gap-analysis.md` §2.1/§2.4
- **THEN** ADR-0071/0074 标注 ✅ Approved (2026-08-25)
- **AND** ADR-0075 标注 ✅ Approved (2026-08-18)

### Requirement: 分析文档为快照并标识权威源 (MUST apply)

本条 MUST 满足：分析文档必须在文首标注其为滚动快照，并声明 ADR 状态权威源为 `docs/adr/*.md` 各自状态字段。

#### Scenario: gap-analysis 有快照横幅
- **WHEN** 阅读 `docs/architecture/adr-implementation-status-gap-analysis.md` 文首
- **THEN** 含快照横幅声明，且不含"唯一事实源"字样（改用"权威参照"）
- **AND** 指明最终权威为 docs/adr/*.md 状态字段

#### Scenario: llm-native-blueprint 有快照横幅
- **WHEN** 阅读 `docs/architecture/llm-native-blueprint-vs-code-gap-analysis.md` 文首
- **THEN** 含"2026-08-03 基线快照"横幅

#### Scenario: layer-based 有快照横幅
- **WHEN** 阅读 `docs/architecture/layer-based-missing-capabilities-analysis.md` 文首
- **THEN** 含"2026-07-31 数据基线"横幅 + 指向 capability-application-map 的指引

### Requirement: "唯一事实源"声明降级 (MUST apply)

本条 MUST 满足：全仓库不再使用"ADR 状态唯一事实源"描述架构分析文档，统一为"权威参照（最终以 docs/adr/*.md 为准）"。

#### Scenario: 全仓库无"唯一事实源"残留
- **WHEN** grep "唯一事实源" 于 `docs/`（含 GOVERNANCE/README/architecture-README）
- **THEN** 0 命中（改用"权威参照"）

### Requirement: layer-based 引用 ADR-0085 (MUST apply)

本条 MUST 满足：`layer-based-missing-capabilities-analysis.md` 必须至少 1 处引用 ADR-0085（Cross-Cutting Pattern PDK），并标注 L1-1/L1-2 建议项已落地状态。

#### Scenario: ADR-0085 被引用
- **WHEN** grep "ADR-0085" 于 `docs/architecture/layer-based-missing-capabilities-analysis.md`
- **THEN** ≥ 1 处

#### Scenario: L1-1/L1-2 建议项已落地
- **WHEN** 阅读 L1-1/L1-2 段尾
- **THEN** 标注"已落地：ADR-0068 ✅ Approved / ADR-0069 🟡 Partial"

### Requirement: README ADR-0070 行状态正确 (MUST apply)

本条 MUST 满足：`docs/README.md` 中 ADR-0070 行状态必须与 `adr-0070-declare-command.md` 文件一致（🟡 Partial）。

#### Scenario: README 与 ADR 文件一致
- **WHEN** 对比 `docs/README.md` ADR-0070 行与 `docs/adr/adr-0070-declare-command.md` 状态字段
- **THEN** 两者一致（🟡 Partial）