# chat-streaming-slash-tui

**优先级**: P1 | **来源**: layer-based-missing-capabilities-analysis.md §八 L4-3（流式渲染 + slash 命令 TUI）
**阶段**: wave-2（依赖 wave-1 四项落地） | **分类**: demo-chat-v2
**类型**: feature

## 架构依据
- EventHandler 已订阅 12 个主题但**无流式渲染逻辑**——`llm.response` 等主题在 adr-0068-event-emission-contract 落地后才有真实发射，本提案消费它们。
- `/tree` `/compact` `/fork` `/clone` 等 slash 命令当前必须 hardcode 在 main.cpp，adr-0070（DECLARE_COMMAND）落地后获得插件化注册载体，本提案是首个真实消费方。
- 缺 `--system-prompt` / `--append-system-prompt` CLI 扩展（与 cli-args-cxxopts 协同）。
- pi-agent 借鉴路径 §五 Streaming / §三 P0.2 的直接落点。

## 范围
- **In Scope**: EventHandler 流式渲染（订阅 `llm.response`/stream chunk 增量输出）；slash 命令解析层（输入以 `/` 开头 → CommandRegistry 派发）；首批 slash 命令迁移（/compact 占位接 context-compactor、/model、/help）；`--system-prompt` / `--append-system-prompt`。
- **Out Scope**: `/tree` `/fork` TUI（session-tree-tui 提案）；`/export`（chat-export-html，缓建）；异步输入 steering（chat-async-io-steering 提案）；CLI 解析库重写（cli-args-cxxopts 提案，仅协同新增 2 个 flag）。

## 关键场景
- GIVEN 真实 LLM 流式响应，WHEN chunk 到达，THEN TUI 增量渲染（无整段等待），且渲染不破坏已有 budget 告警/事件行。
- GIVEN 用户输入 `/help`，WHEN 解析，THEN CommandRegistry 列出已注册命令（含插件注册的），无 hardcode 分支。
- GIVEN 输入以 `/` 开头但命令未注册，WHEN 解析，THEN 给出 "unknown command + /help 提示"，不进入 LLM 调用。

## 技术约束
- MUST slash 命令全部经 DECLARE_COMMAND/CommandRegistry 派发（adr-0070），main.cpp 零 hardcode 命令。
- MUST 流式渲染与事件驱动（禁止轮询 LLM provider）。
- MUST 与 fix-loop-agent-bypass 后的真实 loop_agent 路径对齐（stream 经 `loop.*` 事件触达用户）。
- SHOULD 渲染层与 ChatSession 解耦（渲染订阅 bus，不回调 session）。

## 验收标准
- mock + 真实 LLM 两种模式流式渲染 E2E 通过。
- `grep -n '"/' examples/pdk_chat_demo/main.cpp` 无 hardcode slash 命令分支。
- 新增 2 个 CLI flag 生效；ctest 全量零回归。
