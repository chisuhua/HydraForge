# 提案池（待架构讨论）

> arch 阶段输入。guide-arch Phase 5.5 逐个审查，批准后添加到 `proposal-approved.md`。
>
> **生命周期**: 提案从此文件创建 → 审查批准 → 移至 `proposal-approved.md` 等待实施 → 实施后归档。
> **自动清理**: 提案被批准或实施后，`sync_suggestions()` 会自动从本表移除该行（不再停留）。
> **手动审计**: 发现过期条目时，运行 `skill_use("guide")` 的审计功能自动清理。
> **格式**: 索引表（仅链接 + 元数据）。完整内容在 `improvements/<name>.md`。

| 提案 | 优先级 | 来源 | 添加时间 | 状态 |
|------|--------|------|----------|------|
| [context-compactor](improvements/context-compactor.md) | P1 | 同上 §四 L0-3 + §八 L4-5 + ADR-0007 Partial 收尾 | 2026-07-31 | 已拒绝 |
| [chat-streaming-slash-tui](improvements/chat-streaming-slash-tui.md) | P1 | 同上 §八 L4-3（依赖 Wave 1 四项落地） | 2026-07-31 | 已拒绝 |
| [chat-async-io-steering](improvements/chat-async-io-steering.md) | P2 | 同上 §八 L4-2 + §五 L1-4（steering/follow-up + /model 切换） | 2026-07-31 | 延迟 |
| [cli-args-cxxopts](improvements/cli-args-cxxopts.md) | P2 | 同上 §八 L4-4（CLI 解析层重写, 3 天） | 2026-07-31 | 延迟 |
| [session-tree-tui](improvements/session-tree-tui.md) | P2 | 同上 §八 L4-6（依赖 session-manager-jsonl + chat-streaming-slash-tui） | 2026-07-31 | 延迟 |
| [provider-dynamic-discovery](improvements/provider-dynamic-discovery.md) | P2 | 同上 §五 L1-5 + §六 L2-1（provider 动态注册/refresh/switch） | 2026-07-31 | 延迟 |
