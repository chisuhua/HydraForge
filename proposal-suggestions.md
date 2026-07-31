# 提案池（待架构讨论）

> arch 阶段输入。guide-arch Phase 5.5 逐个审查，批准后添加到 `proposal-approved.md`。
>
> **生命周期**: 提案从此文件创建 → 审查批准 → 移至 `proposal-approved.md` 等待实施 → 实施后归档。
> **自动清理**: 提案被批准或实施后，`sync_suggestions()` 会自动从本表移除该行（不再停留）。
> **手动审计**: 发现过期条目时，运行 `skill_use("guide")` 的审计功能自动清理。
> **格式**: 索引表（仅链接 + 元数据）。完整内容在 `improvements/<name>.md`。

| 提案 | 优先级 | 来源 | 添加时间 | 状态 |
|------|--------|------|----------|------|
| [fix-loop-agent-bypass](improvements/fix-loop-agent-bypass.md) | P0 | layer-based-missing-capabilities-analysis.md §三 X4 / §八 L4-1 + Wave 1 #1 | 2026-07-31 | 待审查 |
| [adr-0068-event-emission-contract](improvements/adr-0068-event-emission-contract.md) | P0 | ADR-0068 (D2 立项) + §三 X1 + Wave 1 #2 | 2026-07-31 | 待审查 |
| [adr-0070-declare-command](improvements/adr-0070-declare-command.md) | P0 | ADR-0070 (D4 立项) + §三 X3 + Wave 1 #3 | 2026-07-31 | 待审查 |
| [adr-0069-tool-coordinator-hooks](improvements/adr-0069-tool-coordinator-hooks.md) | P0 | ADR-0069 (D3 立项) + §三 X2 + Wave 1 #4 | 2026-07-31 | 待审查 |
