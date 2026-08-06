# Proposals Approved

本页登记 plan 阶段可消费的已批准提案。每条对应 `improvements/<name>.md`。

## 已批准提案

| [name](improvements/name.md) | priority | 来源 | 批准日期 | 批准人 |
| --- | --- | --- | --- | --- |
| （2 个 design-pre-created skeleton 待 fill — plan Phase 2.5 流程处理） | | | | |


| [session-tree-commands](improvements/session-tree-commands.md) | P2 | 2026-08-05 | guide-arch |

## 已实施
| [pdk-chat-demo-v1-recap](improvements/pdk-chat-demo-v1-recap.md) | P0 | 2026-07-27 | [Drift close 2026-08-05](openspec/changes/archive/2026-07-27-pdk-chat-demo-v1-recap/) — T1+T2 Session 持久化 + Budget 告警 + Schema 校验 99/99 ctest 全 PASS |
| [pkm-temporal-demo-scaffold](improvements/pkm-temporal-demo-scaffold.md) | P0 | 2026-07-28 | [Drift close 2026-08-05](openspec/changes/archive/2026-07-28-pkm-temporal-demo-scaffold/) — ITemporalClient + 4 scenario + 9 test_temporal_*.cpp 全部 ship |
| [adr-0037-causal-clock](improvements/adr-0037-causal-clock.md) | P2 | 2026-07-29 | Drift close 2026-08-05 — [archived 2026-07-29](openspec/changes/archive/2026-07-29-adr-0037-causal-clock/) `src/common/contract/causal_clock.h` + InMemoryBus::emit 自动 tick 已 ship（ADR-0037 状态需更新） |
| [adr-0019-subscribe-glob](improvements/adr-0019-subscribe-glob.md) | P1 | 2026-07-29 | Drift close 2026-08-05 — [archived 2026-07-29](openspec/changes/archive/2026-07-29-adr-0019-subscribe-glob/) InMemoryBus 双路径分发 + `glob_match()` + `test_interaction_bus_glob.cpp` 6 cases 全部 ship（ADR-0019 状态 🟡 → ✅ 需更新） |
| [adr-0070-declare-command](improvements/adr-0070-declare-command.md) | P0 | 2026-08-04 |
| [adr-0069-tool-coordinator-hooks](improvements/adr-0069-tool-coordinator-hooks.md) | P0 | 2026-08-04 |
| [fix-loop-agent-bypass](improvements/fix-loop-agent-bypass.md) | P0 | 2026-08-03 |
| [adr-0002-busevent-contract](improvements/adr-0002-busevent-contract.md) | P1 | 2026-07-29 |
| [adr-0068-event-emission-contract](improvements/adr-0068-event-emission-contract.md) | P0 | 2026-08-01 | [Wave 1 partial ship](openspec/changes/archive/2026-08-03-adr-0068-event-emission-contract/) — 110/111 ctest, ADR-0068 🟡 Partial, 留 follow-up `promote-event-builder-full-toolresult-support` + §6 E2E mock + fix-loop-agent-bypass |
| [tf-integration-coverage](improvements/tf-integration-coverage.md) | P1 | 2026-08-01 |
| [fix-markdown-parser-yaml](improvements/fix-markdown-parser-yaml.md) | P1 | 2026-08-01 | Drift close 2026-08-06 — [archived 2026-08-04](openspec/changes/archive/2026-08-04-fix-markdown-parser-yaml/) ✅ ship |
| [session-manager-jsonl](improvements/session-manager-jsonl.md) | P1 | 2026-08-01 | Drift close 2026-08-06 — [archived v1 2026-08-04](openspec/changes/archive/2026-08-04-session-manager-jsonl/) + [v2 2026-08-05](openspec/changes/archive/2026-08-05-session-manager-jsonl-v2/) ✅ ship |
| [chat-slash-commands-migration](improvements/chat-slash-commands-migration.md) | P1 | 2026-08-06 | [archived 2026-08-06](openspec/changes/archive/2026-08-06-chat-slash-commands-migration/) ✅ ship — /model DECLARE_COMMAND + provider_switch_stub 工具 + main.cpp 零 hardcode + 3 新增测试 PASS; 122/123 ctest (1 expected live fail) |
