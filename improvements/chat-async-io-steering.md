# chat-async-io-steering

**优先级**: P2 | **来源**: layer-based-missing-capabilities-analysis.md §八 L4-2 + §五 L1-4（中断/切换模型原语）
**阶段**: wave-3（依赖 wave-1 bypass 修复 + wave-2 流式渲染） | **分类**: demo-chat-v2
**类型**: feature

## 架构依据
- `main.cpp:388` `while(getline)` 同步循环：Agent 运行时用户无法输入，pi-agent `agent.steer()` / `agent.followUp()` 借鉴路径（§六）无承载。
- L1-4：`stop_token` 已支持完全中断，但缺**双生产者协调原语**（stdin 与 LLM turn 中断点的协调）与中途切换模型（`/model`）抽象。
- 长任务场景下 steering 是 Agent 可用性的关键交互（pi-agent 核心差异化能力）。

## 范围
- **In Scope**: ChatSession 异步 I/O 改造（输入线程 + Agent 线程）；`steering_queue_` / `follow_up_queue_`；turn 中断点注入（steering 消息在 turn 边界插入 LLM 上下文）；中断传播（stop_token 链路到 loop_agent）；`/model` 运行中切换（L1-4 抽象 + provider_agent 协同）。
- **Out Scope**: thinking_level 抽象（依赖 provider 支持，缓建）；多并发会话；TUI 渲染层（chat-streaming-slash-tui 已覆盖）。

## 关键场景
- GIVEN Agent 正在执行长 turn，WHEN 用户输入 steering 消息，THEN 当前 turn 在下一个工具调用边界被中断，steering 消息注入上下文，Agent 调整方向继续。
- GIVEN Agent 执行中，WHEN 用户输入 follow-up 消息，THEN 进入 follow_up_queue_，当前 turn 完成后立即接续处理（不丢失）。
- GIVEN 会话进行中，WHEN 用户输入 `/model <name>`，THEN 下一个 turn 起切换到目标模型（当前 turn 不被强制中断）。

## 技术约束
- MUST 中断经 `stop_token` 传播（ADR-0001/0042 已奠定的取消语义），禁止引入第二套中断机制。
- MUST steering/follow-up 语义与 pi-agent 对齐（steering=中断调整，follow-up=排队接续），命名保持一致。
- MUST NOT 破坏 `--mock` 测试路径（mock provider 也需支持中断点）。
- SHOULD 双队列有界（防内存膨胀），溢出策略文档化。

## 验收标准
- steering 中断注入 + follow-up 排队接续 E2E 测试通过（mock 模式）。
- `/model` 切换测试通过；长 turn 中 Ctrl+C/中断经 stop_token 正确清理（无泄漏，ASan 验证）。
- ctest 全量零回归。
