# 提案池（待架构讨论）

> arch 阶段输入。guide-arch Phase 5.5 逐个审查，批准后添加到 `proposal-approved.md`。
>
> **生命周期**: 提案从此文件创建 → 审查批准 → 移至 `proposal-approved.md` 等待实施 → 实施后归档。
> **自动清理**: 提案被批准或实施后，`sync_suggestions()` 会自动从本表移除该行（不再停留）。
> **手动审计**: 发现过期条目时，运行 `skill_use("guide")` 的审计功能自动清理。
> **格式**: 索引表（仅链接 + 元数据）。完整内容在 `improvements/<name>.md`。

| 提案 | 优先级 | 来源 | 添加时间 | 状态 |
|------|--------|------|----------|------|

## 已归档（2026-08-12 cleanup）

- ✅ **chat-async-io-steering** (P2) — 2026-08-08 DECOMPOSED → 7 个子 change ship 完成 (Phase 0 + A + B×5 + C)
  - Phase 0: [fix-tool-registry-signal-handler-shutdown](openspec/changes/archive/2026-08-08-fix-tool-registry-signal-handler-shutdown/) ✅ 2026-08-08
  - Phase A: [chat-async-io-queue-infra](openspec/changes/archive/2026-08-08-chat-async-io-queue-infra/) ✅ 2026-08-08
  - Phase B: [chat-async-io-cancellation-chain](openspec/changes/archive/2026-08-09-chat-async-io-cancellation-chain/) + step3/4/5 ✅ 2026-08-09
  - Phase C: [chat-async-io-model-switching](openspec/changes/archive/2026-08-09-chat-async-io-model-switching/) ✅ 2026-08-09
  - 原始 `.rddf/improvements/chat-async-io-steering.md` 保留作为设计意图记录（已标注 DECOMPOSED）