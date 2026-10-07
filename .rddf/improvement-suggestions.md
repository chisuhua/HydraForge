# Improvement Suggestions

> Project improvement tracking index. New improvements are registered in the table below; each entry links to its detailed proposal file under `.rddf/improvements/<name>.md`.
>
> See AGENTS.md §SINGLE-DEVELOPER MODE for governance flow.

## Index

| Name | Priority | Status | Source | Created | Last Update |
|------|----------|--------|--------|---------|-------------|
| [pdk-chat-demo-followups](pdk-chat-demo-followups.md) | P2 | pending | pdk-chat-demo-deduplicate change 审查 | 2026-09-24 | — |
| [l2-evolution-real-llm-provider](l2-evolution-real-llm-provider.md) | 🔴 P0 | ✅ shipped (commit 7fc6d46, 2026-10-07) | 2026-10-06 全量回归测试完备性审查 (G1) | 2026-10-06 | 2026-10-07 |
| [harness-mutation-commit-real-llm](harness-mutation-commit-real-llm.md) | 🔴 P0 | ⚪ pending (待立项) | 2026-10-06 全量回归测试完备性审查 (G4) | 2026-10-06 | — |
| [rsi-anti-cheat-real-llm](rsi-anti-cheat-real-llm.md) | 🟠 P1 | ⚪ pending (待立项) | 2026-10-06 全量回归测试完备性审查 (G5) | 2026-10-06 | — |
| [r13-4-confidential-redaction-real-llm](r13-4-confidential-redaction-real-llm.md) | 🟠 P1 | ⚪ pending (待立项) | 2026-10-06 全量回归测试完备性审查 (G6) | 2026-10-06 | — |
| [l2-release-metrics-real-llm-drop-ratio](l2-release-metrics-real-llm-drop-ratio.md) | 🟠 P1 | ⚪ pending (partial resolution 2026-10-07 per Oracle post-impl verdict) | 2026-10-06 全量回归测试完备性审查 (G7) | 2026-10-06 | 2026-10-07 |
| [self-evolution-9-stage-real-llm-end-to-end](self-evolution-9-stage-real-llm-end-to-end.md) | 🟢 P2 | 🚫 blocked (依赖 G1+G4+G7) | 2026-10-06 全量回归测试完备性审查 (G8) | 2026-10-06 | — |

## Status Legend

| Symbol | Meaning |
|--------|---------|
| ⚪ pending | 已创建 improvement 文件, 待立项 OpenSpec change |
| 🟡 active | 立项 OpenSpec change 进行中 |
| ✅ shipped | OpenSpec change ship + improvement 闭环 |
| 🚫 blocked | 依赖上游 ship 后才能立项 |

## Maintenance Rules

1. **新 improvement 创建**: 添加行 (Name | 🟠/🔴 priority | ⚪ pending | 🤝 source | created)
2. **OpenSpec change 立项**: 更新 Status `⚪ pending` → `🟡 active`, 在对应 OpenSpec change 关联
3. **OpenSpec change ship**: 更新 Status `🟡 active` → `✅ shipped`, 注明 commit hash + date
4. **Deferral**: 更新 Status 注明 deferral reason + link 到后续 OpenSpec change

## 关联 Roadmap

- **Pre-Wave3** (2026-09-21 → Wave 3 shipped): `.rddf/roadmap/2026-09-21-pre-wave3-to-wave3-execution-plan.md`
- **Post-L2-RealLLM** (2026-10-07 → ?): `.rddf/roadmap/2026-10-07-post-l2-real-llm-roadmap.md`