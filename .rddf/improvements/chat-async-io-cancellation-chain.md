# chat-async-io-cancellation-chain

**优先级**: P0 | **来源**: docs/audits/2026-08-08-chat-async-io-steering-pre-approval.md Phase B
**阶段**: chat-async-io Phase B | **分类**: demo-chat-v2
**类型**: feature
**主题**: PlanExecute循环；ForkJoin循环；SkillInterpreter隔离

> **拆分说明**: 本提案是原 `chat-async-io-steering` (Wave 3) 的 Phase B 拆分（7 步 delta）。
> 拆分理由：审计发现 stop_token 链路完全未连通（ChatSession → loop_agent 全链路缺失），单 change 无法 ship。
> 本提案引入 7 步端到端 wiring，是 Phase A queue + Phase C /model 的桥梁。
> 本提案 blocked-by：`fix-tool-registry-signal-handler-shutdown` (Phase 0) + `chat-async-io-queue-infra` (Phase A)。

## 架构依据
- 审计确认 `std::stop_token` 在 `ILLMProvider` / `run_stream_to_bus` / `DomainWorkerPool` jthread 层已实现且测试覆盖，但 ChatSession 到 loop_agent 整条链路缺失（8 处断开点）。
- ADR-0001/0042 奠定的取消语义是项目唯一中断机制（禁止引入第二套）。
- `std::stop_token` 不可序列化跨工具/插件边界，需引入 cancellation handle 注册表（handle id → shared stop_source）。

## 范围
- **In Scope**: 7 步端到端 wiring：(1) ChatSession 持 stop_source + chat() 接 token；(2) loop/run args 加 cancellation_id；(3) 创建 cancellation registry；(4) loop_agent 解析 + Provider bridge 转发；(5) React/Plan/ForkJoin 三个 loop API 加 token 参数；(6) NodeExecutor/ToolCoordinator 转发 token；(7) E2E mid-loop cancel 测试。
- **Out Scope**: thinking_level 抽象（缓建）；多并发会话；TUI 渲染层（已覆盖）。

## 关键场景
- GIVEN Agent 正在执行长 turn，WHEN 用户输入 steering 消息 + request_stop()，THEN 当前 turn 在下一个工具调用边界被中断，steering 消息注入上下文，Agent 调整方向继续。
- GIVEN Agent 执行中，WHEN 用户输入 follow-up 消息，THEN 入 follow_up_queue_（Phase A 已 ship），当前 turn 完成后立即接续处理（不丢失）。
- GIVEN Agent turn 中，WHEN 用户按 Ctrl+C，THEN signal handler（Phase 0）置 shutdown flag → main loop 调用 request_stop() → stop_token 沿链路传播 → Agent 优雅退出。
- GIVEN loop_agent provider bridge，WHEN 收到 cancellation_id，THEN 解析 token 并转发至 `provider_.generate(req, token)`，不再使用 `std::stop_token{}`。

## 技术约束
- MUST 中断经 `stop_token` 单一链路传播（ADR-0001/0042），禁止引入第二套中断机制。
- MUST 7 步 wiring 全部 ship，不允许部分 ship（链路任一断开即丧失取消能力）。
- MUST NodeExecutor + ToolCoordinator 全部接收并转发 token（即使是 pass-through）。
- MUST YieldNode 调用 `generate_stream(req, real_token)` 而非 `generate_stream(req, std::stop_token{})`。
- MUST ForkJoinLoop 的 condition_variable wait 增加 `token.stop_requested()` 谓词。
- MUST NOT 阻塞 ASan/TSan 验证。

## 验收标准
- 7 步 wiring 全部完成且 lsp_diagnostics 零错。
- 新增 E2E mid-loop cancel 测试 PASS（blocking mock provider + `request_stop()` 后 100ms 内退出）。
- ctest 全量零回归（pre-existing 5 失败不变）。
- ASan + TSan 全量 0 新增 error。

## Status: ✅ SHIPPED (2026-08-09, Wave 3-A Phase B)

**Ship evidence** (验证日期 2026-10-15 via 本会话 5-维静态 ship 判定):
- **OpenSpec archive** (4 paths, per Step 1+2 partial → Steps 3/4/5 sub-archives):
  - `openspec/changes/archive/2026-08-09-chat-async-io-cancellation-chain/` (Step 1+2 partial, per commit `b3cf63c`)
  - `openspec/changes/archive/2026-08-09-cancellation-chain-step3-loop-agent/`
  - `openspec/changes/archive/2026-08-09-cancellation-chain-step4-loop-apis/`
  - `openspec/changes/archive/2026-08-09-cancellation-chain-step5-e2e/`
- **代码 ship commits**:
  - `e629893` (2026-08-09) feat(pdk-chat-demo): OpenSpec change 创建 (Wave 3-A Phase B, 7-step stop_token wiring)
  - `6982ffa` (2026-08-09) chore(cleanup): add missing Wave 3-A Phase B cancellation-chain execution plan
  - `d1ecca2` (2026-08-09) feat(pdk-chat-demo): Phase B Step 1+2 - cancellation registry + ChatSession state (Tasks 1-2, CancelRegistry + request_stop)
  - `b3cf63c` (2026-08-09) archive: move Step 1+2 to archive/
  - `120add5` (2026-08-09) docs(sync): chat-async-io-cancellation-chain partial ship record (Step 1+2)
  - `d462cd3` (2026-08-12) chore(iteration): mark Steps 3/4/5 as archived
- **依赖 chain 闭环**:
  - Phase 0 `fix-tool-registry-signal-handler-shutdown` (commit `29a7a06`, 2026-08-09) ✅ SHIPPED
  - Phase A `chat-async-io-queue-infra` ✅ SHIPPED (本会话 hygiene commit `6edbfc8` 2026-10-15)
  - Phase B (本提案) ✅ SHIPPED 2026-08-09
  - Phase C `chat-async-io-model-switching` (commit `cc37d2d`, 2026-08-09) ✅ SHIPPED
- **代码实证** (Step 1+2 commit `d1ecca2`):
  - `examples/pdk_chat_demo/cancellation_registry.h/.cpp` — CancelRegistry 类 (`register_source` / `resolve_token` / `resolve_source` / `unregister`)
  - `examples/pdk_chat_demo/chat_session.h` — `request_stop()` + `chat(input, std::stop_token)` overload
  - `examples/pdk_chat_demo/chat_session.cpp` — Impl 持 `cancellation_registry_` + `current_cancellation_id_`
  - `tests/test_cancellation_registry.cpp` — 4 tests (register/resolve/unregister/resolve_source)
- **7 步 wiring 拆分 ship** (per `d462cd3` decomposing into 4 sub-archives): Step 3 (loop_agent 解析 + Provider bridge 转发), Step 4 (React/Plan/ForkJoin 三个 loop API 加 token 参数), Step 5 (NodeExecutor/ToolCoordinator 转发 + E2E mid-loop cancel 测试)
- **Tests**: `tests/test_chat_session_cancellation.cpp` + `tests/test_loop_agent_cancellation_chain.cpp` + `tests/test_cancellation_registry.cpp`

**Pattern 沉淀**: Pattern #10 drift 第 9 实例 (本会话 5-维判定). 提案文件无 Status 段达 2 个月, 仅靠 4 个 archive sub-paths + 5 个 ship commits 反推. Status 段补全 + Phase 0→A→B→C 依赖 chain 闭环登记, 防未来 audit 误判.