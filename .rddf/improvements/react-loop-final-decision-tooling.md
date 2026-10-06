# Improvement: react-loop-final-decision-tooling

## Why

`test_e2e_real_llm` ChatSession case 自 2026-09-18 F1 ship 起一直 FAIL。三个连续 ship 解决了前 2 个潜伏 gap:

1. **F1 → V2 fix** (commit `1c729e3`, `2026-09-30-fix-chatsession-empty-llm-response`): node_executor fail-fast 扩展覆盖 null/empty/whitespace 3 类空响应. **错误从 `Missing 'response' argument` 不变** (F1 V2 残量在 decide 节点前的链路被修).

2. **Parser rename** (commit `46a3958`, `2026-09-30-fix-lib-loop-args-parsing`): lib/loop 3 文件 `args:` → `arguments:` (canonical schema 对齐). **错误从 `Missing 'response' argument` 不变** (decide 节点前链路被修).

3. **Parser template args** (commit pending, `2026-09-30-fix-parser-template-args`): node_factory.cpp make_tool_call 接受 string arguments + node_executor execute_tool_call 渲染 tool_name. **错误从 `Missing 'response' argument` → `Template render error: variable 'decision.action_args' not found`** (decide 节点开始正常, 但 act 节点渲染空 `decision.action_args` 失败).

**第 3 潜伏 gap** 根因：react.agent.md:30-35 act 节点无条件执行，即使 `decision.final=true`（LLM 直接回答而非工具调用）也尝试渲染空 `decision.action_args`。

### Why not 修在本 change scope

- react loop 流程控制扩展需独立 change（react.agent.md 改造 / DSL 条件节点 / executor tool_name 渲染空值 skip 等多种路径）
- 当前 ship 是 parser+executor 修复，正交于 react loop 控制流
- AGENTS.md Pattern #4: atomic commit, 每 change 独立 ship + 独立 cooling-off

## What Changes

- **修复 1 (推荐路径 A: react.agent.md 改造)** — DSL 条件节点支持。修改 `react.agent.md` 让 decide 节点输出 `decision.final` 到一个新的条件路由节点，根据 `final` 跳到 `end` 或 `act`。需要 DSL 引入 condition 类型节点 + NodeType 新枚举 + parser + executor 支持。

- **修复 2 (备选路径 B: executor 跳过空 tool_name)** — `node_executor.cpp execute_tool_call` 在 main path + stream path 检测 `rendered_tool_name.empty()` 时，throw `runtime_error("React loop final state - skipping act")` 让 chat_session 捕获并继续，输出 `decision.response` 直接给用户。需要 chat_session 侧适配。

- **修复 3 (备选路径 C: act 节点动态 skip)** — DSL 增加 `output_keys_conditional` 或类似机制，executor 在 dispatch 前检查条件，不满足则跳过整个节点，next 节点直接执行。

- **修复 4 (备选路径 D: parse_react_decision L3 fallback 改造)** — `parse_react_decision` L3 fallback 改成 `action_tool="finish"`, `action_args={"input": final_text}`，复用 `finish` 工具作为 terminal action。最小改动但语义混乱（`final=true` vs `action_tool="finish"` 都是终止，但 path 不同）。

## Acceptance

- test_e2e_real_llm ChatSession case PASS (real DeepSeek)
- test_e2e_real_llm 4/4 cases PASS (零回归)
- core tree ctest `-LE must_realllm` 263+/263+ PASS (零回归)
- examples tree ctest `-LE must_realllm` 33/33 PASS (零回归)
- react loop 流程控制文档化（AGENTS.md + dsl.md §5.2）

## Capabilities

### New Capabilities
- `react-loop-decision-final-routing`: react loop 支持 `decision.final=true` 终止路径

### Modified Capabilities
- `openspec/specs/dsl.md §5.2`: 增加 condition 节点契约或 tool_name 空字符串 skip 语义

## Impact

**Production code (~2-4 files)**:
- `lib/loop/react.agent.md` (act 节点改造 或 条件路由节点)
- `src/modules/executor/node_executor.cpp` (execute_tool_call 空字符串跳过)
- `src/modules/parser/markdown_parser.cpp` + `node_factory.cpp` (若选 path A: 条件节点类型)
- `pdk/loop_agent/src/pdk_entry.cpp` (parse_react_decision L3 fallback, 若选 path D)

**Test (~1 file)**:
- `tests/test_react_loop_decision_routing.cpp` (新 regression guard)

**Spec / docs (3 files)**:
- `openspec/specs/dsl.md §5.2`
- `AGENTS.md` Recent Changes entry
- `.rddf/improvements/react-loop-final-decision-tooling.md` (本文件) → archive

## Non-goals

- 不动 PlanExecute / ForkJoin loop (本 change 仅修 React loop)
- 不实现复杂 DSL 条件表达式（如 `if decision.final == true and action_tool != "finish"`）
- 不动 parse_react_decision L1/L2 fallback (仅 L3)

## 优先级

**P1** (阻塞 must_realllm 8/10 → 10/10 完整达标)

## 依赖

- 当前 `fix-parser-template-args` ship 完成 (2026-09-30) — 这是前置
- `AGENTS.md` Pattern #8 建议 ship 后 Oracle/Metis dual-agent review (避免再次设计缺陷)

## Cooling-Off

`2026-09-30-fix-parser-template-args` ship 后 24h cooling-off 触发。next change 在 cooling-off 满后独立立项。