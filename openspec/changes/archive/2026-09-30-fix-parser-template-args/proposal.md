## Why

`test_e2e_real_llm` ChatSession case 在 `fix-lib-loop-args-parsing` (commit `46a3958`) ship 后**仍未 PASS**，错误从 parser bug (`Missing 'response' argument`) 演化为新错误：

```
Tool '{{decision.action_tool}}' not registered for node: /main/act
```

### 根因 (CONFIRMED — inline execution 实测 2026-09-30 post-`46a3958`)

`lib/loop/react.agent.md:30-35` act 节点使用**两层动态渲染**（act 是 ReAct 模式的"决策 → 执行"动态分支）：

```yaml
- id: act
  type: tool_call
  tool: "{{decision.action_tool}}"        # ← template literal, parser 直接存储
  arguments: "{{decision.action_args}}"  # ← string value, parser 静默忽略 (is_object check)
  output_keys: [tool_result]
```

`src/modules/parser/node_factory.cpp:171-189` 的 `make_tool_call` **两条限制**导致 act 节点失败：

1. **`tool` 字段**：parser 读取 `j.at("tool").get<std::string>()` 存为 literal (line 173)。Executor (`node_executor.cpp:269-270` + `:294-295`) 用 `tool_registry_.has_tool(node->tool_name)` 直查 registry，**未渲染 `{{decision.action_tool}}`**，报 "not registered"。
2. **`arguments` 字段**：parser 仅在 `j["arguments"].is_object()` 时处理 (line 176)。当 `arguments` 是**字符串值**时（如 `"{{decision.action_args}}"`），整个 map 为空，args 永远空。

`parse_react_decision` (pdk_entry.cpp:167-248) 输出:
- `action_tool`: 字符串（`"finish"` / `"fs/read"` / etc.）
- `action_args`: JSON 对象或 `{"input": "<value>"}` wrapper（line 198 fallback）

act 节点设计意图：`tool` 字段是模板（在执行时按 `decision.action_tool` 渲染），`arguments` 是模板（在执行时按 `decision.action_args` 渲染）。但 parser 和 executor 都没有实现这个**动态分发**模式。

### 诊断时间线

- `fix-chatsession-empty-llm-response` (commit `1c729e3`, 2026-09-30) — 修 whitespace silent pass
- `fix-lib-loop-args-parsing` (commit `46a3958`, 2026-09-30) — 修 `args:` → `arguments:` canonical schema
- **本 change (FIX)** — 修 string-value arguments + tool template render

`fix-lib-loop-args-parsing` ship 报告明确记录："错误从 parser bug → 第 2 潜伏 gap，独立 follow-up"，本 change 即关闭该 follow-up。

## What Changes

- **修复 1（parser 扩展）**：`node_factory.cpp:171-189 make_tool_call` 扩展 `arguments` 支持 string value（wrap 为 `{"input": "<value>"}`）+ 兼容 `null`（empty args）。
- **修复 2（executor 模板渲染）**：`node_executor.cpp execute_tool_call` 在 main path (L288-298) + stream path (L259-285) 对 `node->tool_name` 调用 `InjaTemplateRenderer::render()` against `ctx`，渲染后查 registry + dispatch。
- **修复 3（regression guard）**：新增 `tests/test_parser_template_args.cpp` 验证 (a) string arguments wrap、(b) tool template render、(c) 3 层 fallback 不破坏既有 path。
- **修复 4（lib/loop follow-up 关闭）**：`react.agent.md:33` `arguments: "{{decision.action_args}}"` 现在 parser 能识别（string value 路径），**无需修改 lib/loop 文件**。
- **修复 5（同步治理）**：`AGENTS.md` Recent Changes entry + `docs/audits/2026-09-29-harness-self-evolution-rsi-audit.md:443` ChatSession 状态修正 + `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md §1.4` 新增 Bug 4 记录（独立 entry，与 Bug 3 平级）+ `docs/specs/dsl.md §5.2` 扩展 string arguments 契约。

## Capabilities

### New Capabilities
- `parser-template-args`: DSL `tool_call` 节点接受 string-value arguments (single-arg convention `input`) + `tool` 字段模板渲染 (replaces `.rddf/improvements/parser-string-template-args.md`)

### Modified Capabilities
- `openspec/specs/dsl.md §5.2`: 明确 `arguments` 字段 string-value 契约（单参数模式 + `input` 约定）+ `tool` 字段模板渲染契约
- `openspec/specs/lib-loop-args-parsing/spec.md` (新): 文档化 react.agent.md act 节点模板分发语义

## Impact

**Production code (2 files)**:
- `src/modules/parser/node_factory.cpp:171-189` (make_tool_call 扩展 string arguments, ~5 lines)
- `src/modules/executor/node_executor.cpp:259-285, 288-298` (execute_tool_call tool_name 渲染, ~4 lines)

**Test (1 new file)**:
- `tests/test_parser_template_args.cpp` (新增, ~80 行, 3-4 cases)

**Spec / docs (4 files)**:
- `openspec/specs/dsl.md §5.2` (扩展 string arguments + tool template 契约)
- `docs/audits/2026-09-29-harness-self-evolution-rsi-audit.md:443` (ChatSession 状态修正: V2 + parser fix ship + 本 change 关闭 ChatSession case)
- `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md §1.4` (新增 Bug 4 记录, 与 Bug 3 平级)
- `AGENTS.md` (Recent Changes entry)

**Total: 7 files (3 production + 4 spec/docs/test), 1 atomic commit** (per AGENTS.md 模式 #4 SHIP-with-fixes 流程).

## Non-goals

- 不重写 `parse_react_decision` (loop_agent.py 的 L1/L2/L3 fallback 保留, 不动)
- 不扩展其他 DSL 节点类型 (assign, llm_call 等维持现状)
- 不实现完整的 string-template value 解析 (如 `key1: "a", key2: "{{b}}"` 混编 - 留后续 follow-up)
- 不修复 test_loop_agent_autonomous vs test_e2e_real_llm 矛盾点 (独立调研)
- 不动 `chat_session.cpp` 或 `pdk_chat_demo/tests/` (本 change 是 parser+executor 修复, ChatSession 是消费者)

## Acceptance Criteria

- [ ] `node_factory.cpp make_tool_call` 接受 `arguments: "<string>"` 形式，wrap 为 `{"input": "<value>"}`
- [ ] `node_executor.cpp execute_tool_call` 在 main path + stream path 都对 `node->tool_name` 调用 `InjaTemplateRenderer::render()`
- [ ] `tests/test_parser_template_args.cpp` 3-4 cases PASS (含 RED 状态验证)
- [ ] `test_e2e_real_llm` 4 cases (ChatSession + generate_subgraph + multi_turn + errors) 全部 PASS
- [ ] core tree ctest `-LE must_realllm` 262+/262+ PASS (零回归)
- [ ] examples tree ctest `-LE must_realllm` 33/33 PASS (零回归)
- [ ] `openspec validate` PASS
- [ ] AGENTS.md Recent Changes entry + audit §6.1 line 443 + roadmap §1.4 Bug 4 + dsl.md §5.2 全部对齐

## Cooling-Off

Per AGENTS.md 治理链: 24h cooling-off 触发。cooling_off_until = 2026-10-01T00:00:00Z (UTC+8 时区 2026-10-01 08:00)。