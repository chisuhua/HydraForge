# chat-async-io-queue-infra

**优先级**: P1 | **来源**: docs/audits/2026-08-08-chat-async-io-steering-pre-approval.md Phase A
**阶段**: chat-async-io Phase A | **分类**: demo-chat-v2
**类型**: feature
**主题**: PlanExecute循环

> **拆分说明**: 本提案是原 `chat-async-io-steering` (Wave 3) 的 Phase A 拆分。
> 原提案按 4-phase 拆分为：Phase 0 (SIGSEGV fix) / Phase A (本提案 queue 基础设施) / Phase B (stop_token 链路) / Phase C (/model 切换)。
> 拆分理由：Phase A 仅添加队列数据结构 + 输入线程，不依赖 stop_token 链路，可独立 ship 与验证。
> 本提案 blocked-by: 无（可与 Phase 0 并行启动）。

## 架构依据
- `main.cpp:466` `while(getline)` 同步循环：Agent 运行时用户无法输入，pi-agent `agent.steer()` / `agent.followUp()` 借鉴路径（§六）无承载。
- 双队列（steering/follow-up）为 Phase B 的中断点注入提供注入目标，是 Phase B 的前置依赖。
- 有界队列防止内存膨胀（SHOULD 约束），溢出策略文档化。

## 范围
- **In Scope**: `ChatSession::Impl` 添加 `steering_queue_` (有界) + `follow_up_queue_` (有界)；独立 input 线程从 stdin 读取消息分类入队；sync 路径下两个队列行为验证（mock 模式 E2E 测试）。
- **Out Scope**: turn 中断点注入（→ Phase B）；`/model` 切换（→ Phase C）；TUI 渲染层（chat-streaming-slash-tui 已覆盖）。

## 关键场景
- GIVEN 用户输入 `/foo` 命令，WHEN ChatSession 接收，THEN 入 steering_queue_ (因为是命令意图调整)。
- GIVEN 用户输入普通消息，WHEN Agent turn 正在执行，THEN 入 follow_up_queue_ (当前 turn 完成后接续)。
- GIVEN steering_queue_ 满，WHEN 用户继续输入，THEN 溢出策略触发（丢弃最旧 / 拒绝新 / 阻塞，文档化选择）。
- GIVEN follow_up_queue_ 满，THEN 同样溢出策略。

## 技术约束
- MUST 双队列有界（默认上限 32 条，可配置），溢出策略文档化（推荐：steering 拒绝新 + log warning；follow-up 拒绝新 + log warning）。
- MUST steering/follow-up 命名与 pi-agent 对齐（steering=中断调整，follow-up=排队接续）。
- MUST 双 mutex 保护（避免单 mutex 死锁风险），producer-consumer 模式。
- MUST NOT 阻塞 Agent turn 执行路径（队列操作 O(1)）。
- SHOULD 提供 queue_size() / clear() 测试辅助 API。

## 验收标准
- sync mock 路径下，steering 入队 + 出队 E2E 测试 PASS。
- sync mock 路径下，follow-up 入队 + Agent turn 完成后接续测试 PASS。
- 队列溢出策略单元测试 PASS（达到上限后行为符合文档化策略）。
- ctest 全量零回归。

## Status: ✅ SHIPPED (2026-08-09, Wave 3-A Phase A)

**Ship evidence** (验证日期 2026-10-15 via 本会话 5-维静态 ship 判定):
- **OpenSpec archive**: `openspec/changes/archive/2026-08-08-chat-async-io-queue-infra/` (5 files tracked per commit `14f1ffe`)
- **代码 ship commits**:
  - `3c7f801` (2026-08-09) feat(pdk-chat-demo): OpenSpec change 创建 (Phase A of chat-async-io-steering decomposition)
  - `d90b8b1` (2026-08-09) docs(sync): chat-async-io-queue-infra ship record
  - `14f1ffe` (2026-08-09) archive: move chat-async-io-queue-infra to archive/
  - `c30b2b3` (2026-09-07) fix(pdk_chat_demo): stdin pipe 模式下禁用双读 input thread (race condition 修复: `enable_input_thread` 默认值改 `isatty(STDIN_FILENO) != 0`)
- **代码实证**:
  - `pdk/chat_session/include/agenticdsl/pdk/chat_session.h:191` `enum class QueueKind { Steering, FollowUp }` — 双队列类型标识
  - `pdk/chat_session/include/agenticdsl/pdk/chat_session.h:252-257` `queue_size(QueueKind)` + `try_clear_queue(QueueKind)` API
  - `pdk/chat_session/include/agenticdsl/pdk/chat_session.h` 内含 `try_push_steering_for_test` / `try_push_follow_up_for_test` 测试辅助 API
- **测试实证**: `examples/pdk_chat_demo/tests/test_chat_session_queues.cpp` 60 行 (3 TEST_CASE: queue_size / overflow 拒绝 / multi-round)
- **配套 ship**: `c30b2b3` 修复 input thread + main loop 双读 stdin race (TTY 启用双读，pipe/重定向场景关闭双读, 避免 follow_up_queue_ 抢走 stdin prompt 致 main loop 永远等待)

**依赖 chain 闭环**:
- Phase A (本提案, queue 基础设施) → Phase B (chat-async-io-cancellation-chain, 已 ship `e629893` + `d1ecca2`) → Phase C (chat-async-io-model-switching, 已 ship `cc37d2d` + `6625072`)
- 3 个子提案全部 ✅ SHIPPED, 父提案 `chat-async-io-steering` ⛔ DECOMPOSED 2026-08-08 (已记录于 `.rddf/improvements/chat-async-io-steering.md`)

**Pattern 沉淀**: Pattern #10 drift 第 8 实例 (本会话 5-维判定). 提案文件无 Status 段达 2 个月, 仅靠 OpenSpec archive + commit message + 代码实证反推 ship 状态. Status 段补全, 防未来 audit 误判 Phase A 未 ship.