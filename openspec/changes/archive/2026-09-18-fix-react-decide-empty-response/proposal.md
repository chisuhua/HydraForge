## Why

React agent loop 在 `decide` 节点真实 LLM 端到端失败：`Tool 'loop/decide_react' failed: Missing 'response' argument`。原因：`react.agent.md` 的 `decide` 节点 `args: response: "{{llm_response}}"` 通过 inja 模板从上一节点 `think` (llm_call) 的 `output_keys: [llm_response]` 拉取值，但渲染结果为空字符串，导致 `loop/decide_react` 收到空 `response` 参数 → 返回 `Missing 'response' argument` 错误。

**完整根因待诊断**（本 change 范畴）：可能由 (a) `node_executor.cpp` llm_call 节点的 output_keys 数据类型（json object vs string）与 inja 渲染期望不匹配；(b) ctx 跨节点传递链路（fork/branch 节点、output_keys→input/output_keys 等）有断点；(c) `lib/loop/react.agent.md` 的 next/arg schema 与当前 node_executor 实现的 output_keys→args 桥接语义不一致。

`commit 0b0da50` (`fix(tool_result): bridge top-level 'error' field to meta.error_message`) 修复了 PDK 工具失败错误信息隐形系统性问题，使本 bug 可见。

## What Changes

- **诊断 react.agent.md 真实 LLM 端到端失败的完整根因**（node_executor.cpp llm_call output_keys → 后续 node args 桥接链）
- **修复 react.agent.md decide 节点 `{{llm_response}}` 渲染失败**（最小修复：data type 适配 OR ctx 传递修复 OR schema 重设计）
- **加 真实 LLM 端到端回归测试**（`react.agent.md` + 真实 DeepSeek LLM 完整链路：think → decide → 解析 LLM 响应 → 返回非空 assistant 文本）
- **写 chat-real-llm-coverage Phase H** 正式化（包含 react/plan_execute/fork_join 三种 loop 的真实 LLM 覆盖）

## Capabilities

### New Capabilities
<!-- Capabilities being introduced. Replace <name> with kebab-case identifier -->
- `react-agent-llm-ctx-bridge`: react loop 节点间 LLM 响应上下文桥接契约 — output_keys 数据类型 + 跨节点 ctx 传递 + inja 模板渲染语义保证
- `real-llm-react-loop-e2e`: react agent loop 真实 LLM 端到端测试覆盖 — 输入用户消息 → 真实 DeepSeek LLM → 多轮 react → 返回非空 assistant 文本

### Modified Capabilities
<!-- Existing capabilities whose REQUIREMENTS are changing. Check openspec/specs/. Leave empty if no requirement changes. -->
（无 — 本 change 是 bug fix，新增 capability 范畴）

## Impact

- `src/modules/executor/node_executor.cpp:147` (`llm_call` output_keys → new_context 数据赋值)
- `src/modules/executor/node_executor.cpp:228-230` (`tool_call` args inja 渲染)
- `src/common/utils/template_renderer.cpp:29-37` (`InjaTemplateRenderer::render` 错误处理)
- `lib/loop/react.agent.md` (decide 节点 args schema)
- `lib/loop/plan_execute.agent.md` (execute 节点 args schema — 类似 pattern)
- `lib/loop/fork_join.agent.md` (3 个 task 节点 args schema — 类似 pattern)
- `openspec/specs/loop-run-contract/spec.md` (R1-R5 may need amendment for ctx bridge semantics)
- `openspec/specs/chat-session-loop-result-ok/spec.md` (相关)

## Non-goals

- 不重写 ReactLoop C++ class（保留 Sprint 20 ship 行为）
- 不重写 DSL node 类型（start/llm_call/tool_call/assign/end 5 类保持不变）
- 不引入新的 DSL 节点类型
- 不修复 `pdk/loop_agent` 工具内部 bug（focus 在 ctx bridge 语义层）
- 不修 `ToolResult::error()` path（已在 commit 0b0da50 修复）

## Dependencies

- `commit 0b0da50` (fix(tool_result): bridge error field) ✅ 已 ship
- `openspec/specs/loop-run-contract/spec.md` (上游契约)
- `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md §1.4 Bug 3 残量风险` (跟踪文档)

## Oracle Session

待起草时 fill — 计划 `task(subagent_type="oracle")` 咨询以下 design 决策:
- D1: data type 适配策略（output_keys 默认存 json object vs 优先 string）
- D2: inja strict 模式 vs silent 模式选择（throw vs 空字符串）
- D3: 跨节点 ctx 隔离 vs 共享语义（fork/branch 节点是否应隔离 LLM response）