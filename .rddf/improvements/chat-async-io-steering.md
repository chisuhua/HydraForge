# chat-async-io-steering (decomposed — see sub-proposals)

**优先级**: P2 | **来源**: layer-based-missing-capabilities-analysis.md §八 L4-2 + §五 L1-4（中断/切换模型原语）
**阶段**: wave-3 | **分类**: demo-chat-v2
**类型**: feature
**状态**: ⛔ **DECOMPOSED 2026-08-08** — 不可直接 ship，需按 4 个子提案分别实施
**主题**: PlanExecute循环；ForkJoin循环

> **拆分说明**: 2026-08-08 预审批审计 (`docs/audits/2026-08-08-chat-async-io-steering-pre-approval.md`) 发现：
> 1. `stop_token → loop_agent` 链路完全未连通（8 处断开点），不能 ship 为单 change。
> 2. ToolRegistry SIGSEGV pre-existing 阻塞 Phase B E2E 测试。
> 3. steering/follow-up 队列、stop_token wiring、`/model` 切换 3 个特性相互依赖，不能同步 ship。
>
> 本文件保留作为原始设计意图记录，实施请按以下 4 个子提案顺序：

## 子提案（按依赖顺序）

| 阶段 | improvement 文件 | 优先级 | 依赖 | 估时 | 阻塞性 |
|------|------------------|--------|------|------|--------|
| Phase 0 | [fix-tool-registry-signal-handler-shutdown](fix-tool-registry-signal-handler-shutdown.md) | P0 | 无 | 1-2 天 | 阻塞 Phase B |
| Phase A | [chat-async-io-queue-infra](chat-async-io-queue-infra.md) | P1 | 无（与 Phase 0 并行） | 3-4 天 | 阻塞 Phase B |
| Phase B | [chat-async-io-cancellation-chain](chat-async-io-cancellation-chain.md) | P0 | Phase 0 + A | 1.5-2 周 | 阻塞 Phase C |
| Phase C | [chat-async-io-model-switching](chat-async-io-model-switching.md) | P2 | Phase B + provider-dynamic-discovery ✅ | 1 周 | 无 |

## 原始架构依据（保留）

- `main.cpp:466` `while(getline)` 同步循环：Agent 运行时用户无法输入，pi-agent `agent.steer()` / `agent.followUp()` 借鉴路径（§六）无承载。
- L1-4：`stop_token` 已支持完全中断，但缺**双生产者协调原语**（stdin 与 LLM turn 中断点的协调）与中途切换模型（`/model`）抽象。
- 长任务场景下 steering 是 Agent 可用性的关键交互（pi-agent 核心差异化能力）。

## 原始范围（拆分后分散到子提案）

- **In Scope**（原）：ChatSession 异步 I/O 改造；`steering_queue_` / `follow_up_queue_`；turn 中断点注入；中断传播（stop_token 链路到 loop_agent）；`/model` 运行中切换。
- **Out Scope**（原，保持）：thinking_level 抽象（依赖 provider 支持，缓建）；多并发会话；TUI 渲染层（chat-streaming-slash-tui 已覆盖）。

## 原始关键场景（保留作为子提案验收依据）

- GIVEN Agent 正在执行长 turn，WHEN 用户输入 steering 消息，THEN 当前 turn 在下一个工具调用边界被中断，steering 消息注入上下文，Agent 调整方向继续 → **Phase A + B**
- GIVEN Agent 执行中，WHEN 用户输入 follow-up 消息，THEN 进入 follow_up_queue_，当前 turn 完成后立即接续处理（不丢失）→ **Phase A + B**
- GIVEN 会话进行中，WHEN 用户输入 `/model <name>`，THEN 下一个 turn 起切换到目标模型（当前 turn 不被强制中断）→ **Phase C**

## 原始技术约束（拆分后分散到子提案）

- MUST 中断经 `stop_token` 传播（ADR-0001/0042 已奠定的取消语义），禁止引入第二套中断机制 → **Phase B**
- MUST steering/follow-up 语义与 pi-agent 对齐 → **Phase A**
- MUST NOT 破坏 `--mock` 测试路径 → **所有 phase**
- SHOULD 双队列有界（防内存膨胀），溢出策略文档化 → **Phase A**

## 原始验收标准（拆分后分散到子提案）

- steering 中断注入 + follow-up 排队接续 E2E 测试通过（mock 模式）→ **Phase A + B**
- `/model` 切换测试通过；长 turn 中 Ctrl+C/中断经 stop_token 正确清理（无泄漏，ASan 验证）→ **Phase B + C**
- ctest 全量零回归 → **所有 phase**

## 审计依据

完整审计见 `docs/audits/2026-08-08-chat-async-io-steering-pre-approval.md`（含 stop_token 8 处断开点 + SIGSEGV 根因 + 4 phase 拆分依据）。