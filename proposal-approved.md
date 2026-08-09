# Proposals Approved

本页登记 plan 阶段可消费的已批准提案。每条对应 `improvements/<name>.md`。

## 已批准提案

| [name](improvements/name.md) | priority | 来源 | 批准日期 | 批准人 |
| --- | --- | --- | --- | --- |
| （无 — 所有已批准提案均已 ship） | | | | |

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
| [session-tree-commands](improvements/session-tree-commands.md) | P2 | 2026-08-06 | [archived 2026-08-06](openspec/changes/archive/2026-08-06-session-tree-commands/) ✅ ship — /tree /fork /clone DECLARE_COMMAND + session_fork/clone 工具 + SessionManager 3 个新只读 API + ANSI 树渲染; 124/125 ctest |
| [cli-args-cxxopts](improvements/cli-args-cxxopts.md) | P2 | 2026-08-06 | [archived 2026-08-06](openspec/changes/archive/2026-08-06-cli-args-cxxopts/) ✅ ship |
| [provider-dynamic-discovery](improvements/provider-dynamic-discovery.md) | P2 | 2026-08-06 | [archived 2026-08-06](openspec/changes/archive/2026-08-06-provider-dynamic-discovery/) ✅ ship |
| [session-tree-tui](improvements/session-tree-tui.md) | P2 | 2026-08-06 | [archived 2026-08-07](openspec/changes/archive/2026-08-07-session-tree-tui/) ✅ ship — --fork + --name CLI flags + SessionManager::rename_session + StartupCleanupGuard RAII; 134/136 ctest |
| [chat-streaming-slash-tui](improvements/chat-streaming-slash-tui.md) | P1 | 2026-08-06 | [archived 2026-08-07](openspec/changes/archive/2026-08-07-chat-streaming-slash-tui/) ✅ ship — EventHandler 流式渲染 + --system-prompt/--append-system-prompt CLI flags; 13/13 新测试 PASS |
| [fix-tool-registry-signal-handler-shutdown](improvements/fix-tool-registry-signal-handler-shutdown.md) | P0 | 2026-08-08 | [archived 2026-08-08](openspec/changes/archive/2026-08-08-fix-tool-registry-signal-handler-shutdown/) ✅ ship — signal_handler 仅置 atomic flag + main 循环观察走正常有序清理路径; 2/2 新增子进程回归测试 PASS; 135/138 ctest; 解锁 chat-async-io-cancellation-chain (Phase B) E2E 测试稳定性 |
| [chat-async-io-queue-infra](improvements/chat-async-io-queue-infra.md) | P1 | 2026-08-08 | [archived 2026-08-08](openspec/changes/archive/2026-08-08-chat-async-io-queue-infra/) ✅ ship — ChatSession::Impl 新增 steering_queue_ + follow_up_queue_ 双有界队列 (capacity=32, 拒绝新+log warning) + 标准 std::thread 输入线程; 4/4 新增测试 PASS (41 assertions); 135/138 ctest; 解锁 chat-async-io-cancellation-chain (Phase B) 注入目标 |
| [chat-async-io-cancellation-chain](improvements/chat-async-io-cancellation-chain.md) | P0 | 2026-08-08 | [archived 2026-08-09 partial Step 1+2](openspec/changes/archive/2026-08-09-chat-async-io-cancellation-chain/) 🟡 PARTIAL — CancellationRegistry 4/4 测试 PASS (11 assertions) + ChatSession::chat(input, std::stop_token) overload + request_stop() 通过 resolve_source 触发; 135/138 ctest; 剩 7 步 wiring 拆为 3 focused sub-changes (loop_agent/NodeExecutor/ToolCoordinator + 3 BREAKING loop APIs + E2E mock provider test) |
| [cancellation-chain-step3-loop-agent](improvements/chat-async-io-cancellation-chain.md) | P0 | 2026-08-09 | [archived 2026-08-09](openspec/changes/archive/2026-08-09-cancellation-chain-step3-loop-agent/) ✅ ship — Phase B Step 3 of 5; loop_agent 入口解析 cancellation_id + Provider bridge 转发 token + NodeExecutor::dispatch_to_tool/YieldNode 接受 std::stop_token + ToolCoordinator::execute short-circuit + tool.audit.denied emit (reason=cancelled); 3/3 新测试 PASS (15 assertions); 138/141 ctest (3 pre-existing 不变); 修复审计 4 处断开点 (#3/#4/#6/#7); 解锁 Step 4 (3 BREAKING loop APIs) |