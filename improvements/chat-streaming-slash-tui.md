# chat-streaming-slash-tui

**优先级**: P1 | **来源**: layer-based-missing-capabilities-analysis.md §八 L4-3（Wave 2 拆分 — 流式渲染子集）
**阶段**: wave-2 | **分类**: demo-chat-v2
**类型**: feature

> **Wave 拆分说明**: 本提案是原 `chat-streaming-slash-tui` 的 Wave 2 拆分（仅含流式渲染 + CLI flag 子集）。
> 原提案拆分为：(Wave 1) `chat-slash-commands-migration` slash 命令迁移收尾 + (Wave 2) 本提案流式渲染子集。
> 拆分理由：adr-0070 ✅ ship（2026-08-04）已提供完整命令层基础设施，slash 命令迁移可独立 ship；流式渲染依赖 fix-loop-agent-bypass 真实路径 + cli-args-cxxopts（Wave 2）。
> 本提案 blocked-by：`chat-slash-commands-migration` + `cli-args-cxxopts` + `provider-dynamic-discovery`（/model 完整切换）。

## 架构依据
- adr-0068 Wave 1 partial ship（2026-08-03）已落地 `llm.request` / `llm.response` / `tool.execution.start` / `tool.execution.end` / `session.persisted` 5 主题强制发射点 + EventBuilder V2（2026-08-03）。
- fix-loop-agent-bypass（2026-08-03）已 ship 真实 `loop_agent` 路径，`loop.*` 事件链路打通。
- Wave 1 `chat-slash-commands-migration` 提供完整命令层入口（含 `/model` stub）。
- cli-args-cxxopts（Wave 2）提供声明式 CLI flag 基础设施。
- pi-agent 借鉴路径 §五 Streaming 核心差异化能力：增量渲染 + budget 告警不被打断 + 异步输入协调。

## 范围
- **In Scope**:
  - EventHandler 流式渲染：订阅 `llm.response` 主题（chunk 级别）+ `loop.*` 主题，增量输出至 TUI（不等整段）
  - 渲染与 ChatSession 解耦（渲染订阅 bus，不回调 session）
  - 与 fix-loop-agent-bypass 后真实 loop_agent 路径对齐（流式经 `loop.token` / `loop.decision` 触达用户）
  - 流式渲染不破坏已有 budget 告警 / 事件行（增量追加 + 滚动策略）
  - `--system-prompt <text>` / `--append-system-prompt <text>` CLI flag（依赖 cli-args-cxxopts，集成至启动加载流程）
  - 流式渲染 E2E 测试（mock 模式 + 真实 LLM 模式双路径）
- **Out of Scope**:
  - slash 命令迁移主体（→ chat-slash-commands-migration，Wave 1）
  - `/model` 实际切换功能（依赖 provider-dynamic-discovery，Wave 2 完整实现）
  - 异步输入 steering / follow-up（→ chat-async-io-steering，Wave 3）
  - 通用 CLI 解析基础设施（→ cli-args-cxxopts，Wave 2）

## 关键场景
- GIVEN 真实 LLM 流式响应（OpenAI SSE 风格 chunk），WHEN chunk 到达 EventHandler，THEN TUI 增量渲染（无整段等待），渲染不破坏已有 budget 告警 / 事件行（每行独立追加）。
- GIVEN 流式响应中，WHEN 用户输入 `/interrupt`，THEN 当前 turn 在下一个 token 边界停止（参考 fix-loop-agent-bypass 真实路径）。
- GIVEN 启动 `./pdk_chat_demo --append-system-prompt "Always be terse."`，THEN 系统提示附加至默认 system prompt 后（合并策略文档化），后续 LLM 调用使用合并后提示。
- GIVEN `--system-prompt "Custom prompt"`，WHEN 启动，THEN 覆盖默认 system prompt（而非追加）。
- GIVEN mock 模式流式响应，WHEN EventHandler 处理，THEN 渲染行为与真实 LLM 模式一致（mock 也走 EventHandler 路径）。

## 技术约束
- MUST 流式渲染与事件驱动（禁止轮询 LLM provider）。
- MUST 渲染订阅 bus，不直接调用 session 方法（解耦）。
- MUST 流式中断经 `stop_token`（ADR-0001/0042 既定取消语义）传播，禁止第二套机制。
- MUST `--system-prompt` / `--append-system-prompt` 行为文档化（覆盖 vs 追加语义清晰，避免歧义）。
- SHOULD 流式渲染性能：chunk 处理延迟 < 50ms（P95），不阻塞 EventHandler 主循环。
- MUST 与 chat-async-io-steering 的 steering/follow-up 语义兼容（同一 bus 事件路径）。
- MUST NOT 重复实现 Wave 1 已 ship 的命令迁移。

## 验收标准
- mock + 真实 LLM 两种模式流式渲染 E2E 测试通过。
- `grep -n '"/' examples/pdk_chat_demo/main.cpp` 无 hardcode slash 命令分支（Wave 1 验证过的）。
- 新增 2 个 CLI flag 生效；行为与 cli-args-cxxopts help 文案一致。
- 流式中断 + budget 告警不打断测试通过（注入中断 + 注入告警事件）。
- 复用 adr-0068 EventBuilder + fix-loop-agent-bypass 真实路径，不重写事件链路。
- ctest 全量零回归。
