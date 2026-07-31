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
| [fix-markdown-parser-yaml](improvements/fix-markdown-parser-yaml.md) | P1 | layer-based-missing-capabilities-analysis.md §四 L0-4（pdk_chat_demo T2 校验失效, 快速修复不另开 ADR） | 2026-07-31 | 待审查 |
| [session-manager-jsonl](improvements/session-manager-jsonl.md) | P1 | 同上 §四 L0-1 + §五 L1-3 + §六 L2-2（树状会话存储, /tree /fork 前置） | 2026-07-31 | 待审查 |
| [context-compactor](improvements/context-compactor.md) | P1 | 同上 §四 L0-3 + §八 L4-5 + ADR-0007 Partial 收尾 | 2026-07-31 | 待审查 |
| [chat-streaming-slash-tui](improvements/chat-streaming-slash-tui.md) | P1 | 同上 §八 L4-3（依赖 Wave 1 四项落地） | 2026-07-31 | 待审查 |
| [chat-async-io-steering](improvements/chat-async-io-steering.md) | P2 | 同上 §八 L4-2 + §五 L1-4（steering/follow-up + /model 切换） | 2026-07-31 | 待审查 |
| [cli-args-cxxopts](improvements/cli-args-cxxopts.md) | P2 | 同上 §八 L4-4（CLI 解析层重写, 3 天） | 2026-07-31 | 待审查 |
| [session-tree-tui](improvements/session-tree-tui.md) | P2 | 同上 §八 L4-6（依赖 session-manager-jsonl + chat-streaming-slash-tui） | 2026-07-31 | 待审查 |
| [provider-dynamic-discovery](improvements/provider-dynamic-discovery.md) | P2 | 同上 §五 L1-5 + §六 L2-1（provider 动态注册/refresh/switch） | 2026-07-31 | 待审查 |
