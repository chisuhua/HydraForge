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