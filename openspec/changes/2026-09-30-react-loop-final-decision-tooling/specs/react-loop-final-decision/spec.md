## ADDED Requirements

### Requirement: parse_react_decision L3 fallback 返回 finish + final_text

`pdk/loop_agent/src/pdk_entry.cpp:230-247` `parse_react_decision` L3 fallback（自然语言无工具调用或 LLM 返回 "Final Answer:"）MUST 返回 `action_tool="finish"` + `action_args=final_text` (JSON string 而非 object) + `final=true` + `response=final_text`。此契约恢复原始设计意图（per `pdk_entry.cpp:114-117` 注释 "act node, typically 'finish'"），让 react loop `act` 节点调 `finish` 工具作为 ReAct 终止语义。

#### Scenario: LLM 返回 "Final Answer: X"

- **WHEN** LLM 响应为 `"Final Answer: The result is 42."`
- **THEN** `parse_react_decision` MUST 返回 `{final: true, action_tool: "finish", action_args: "The result is 42.", response: "The result is 42."}`
- **AND** react DSL act 节点 MUST dispatch 到 `finish` 工具并成功返回

#### Scenario: LLM 返回自然语言（无工具调用）

- **WHEN** LLM 响应为 `"The answer is hello world."`（无 XML / JSON 工具调用结构）
- **THEN** `parse_react_decision` MUST 返回 `{final: true, action_tool: "finish", action_args: "The answer is hello world.", response: "The answer is hello world."}`
- **AND** react DSL MUST 完整走通 think → decide → act(finish) → observe → end

### Requirement: finish 工具读 input 兜底

`pdk/loop_agent/src/pdk_entry.cpp:152-160` `finish` 工具 MUST 采用三级取参：`answer` → 非空 `input` → `"Task complete"`。保证 `action_args` 渲染结果（`args["input"]`）作为最终答案能传递到 `tool_result.answer`。

#### Scenario: finish 接 answer 参数（向后兼容）

- **WHEN** `finish` 工具收到 `args = {"answer": "Task done"}`
- **THEN** MUST 返回 `{ok: true, success: true, error_code: null, answer: "Task done"}`

#### Scenario: finish 接 input 参数（action_args 渲染结果）

- **WHEN** `finish` 工具收到 `args = {"input": "Final answer text"}` (L3 fallback path)
- **THEN** MUST 返回 `{ok: true, success: true, error_code: null, answer: "Final answer text"}`

#### Scenario: finish 接空 input 兜底

- **WHEN** `finish` 工具收到 `args = {"input": ""}`（LLM 返回 "Final Answer:" 后无内容）
- **THEN** MUST 返回 `{ok: true, success: true, error_code: null, answer: "Task complete"}`（不返回空）

#### Scenario: finish 接 answer + input 都存在时优先 answer

- **WHEN** `finish` 工具收到 `args = {"answer": "explicit", "input": "fallback"}`
- **THEN** MUST 返回 `{ok: true, success: true, error_code: null, answer: "explicit"}`

### Requirement: loop/run response 优先 decision.response

`pdk/loop_agent/src/pdk_entry.cpp:772-784` `loop/run` 响应提取 MUST 在现有提取链（`{response, output, llm_response, plan_response, final_result}`）之后检查 `decision.response`：当 `decision` 是 JSON object 且含非空 string `response` 字段时，优先使用之。让 ChatSession 拿到 parsed final text 而非原始 LLM 输出（含 "Final Answer:" 前缀）。

#### Scenario: L3 final 时 ChatSession 拿 parsed final text

- **WHEN** LLM 返回 `"Final Answer: The result is 42."` + react DSL 完整走通
- **THEN** `loop/run` MUST 返回 `response = "The result is 42."` (而非原始 LLM 文本)
- **AND** ChatSession e2e test MUST PASS（含 "hello" 关键词的 response 提取仍由测试断言负责）

#### Scenario: L1/L2 action 路径无行为变化

- **WHEN** LLM 返回合法 L1 JSON（如 `{"name":"fs/read","arguments":{...}}`）
- **THEN** `loop/run` response 提取 MUST 保持现有行为（`decision.response` == 原始文本 == `llm_response`）
- **AND** L1/L2 测试 MUST 零回归

### Requirement: test_loop_agent_plugin.cpp L3 断言更新

`examples/pdk_chat_demo/tests/test_loop_agent_plugin.cpp:253-254` MUST 更新 L3 fallback 期望值：`action_tool` 从 `""` 改为 `"finish"`，`action_args` 从 `is_null()` 改为 `is_string() && == final_text`。同 commit 提交（per AGENTS.md Pattern #4 atomicity）。

#### Scenario: L3 Final Answer 测试断言

- **WHEN** 测试 L3 Final Answer fallback 路径
- **THEN** MUST 断言 `action_tool == "finish"` + `action_args.is_string() == "The result is 42."`

### Requirement: 新增 L3 final e2e case

`examples/pdk_chat_demo/tests/test_loop_agent_plugin.cpp` MUST 新增 1 个 e2e case：L3 final answer 完整走通 react DSL（think → decide → act → observe → end）。

#### Scenario: L3 final e2e 完整走通

- **WHEN** MockProvider 返回 `"Final Answer: The result is 42."` + 设置 parent provider + 不设 mock_fallback（走真 DSL 路径）
- **THEN** MUST 断言 `result.ok == true`, `error_code == null`, `response 非空 (若 D4 done: == "The result is 42.")`, `steps >= 4`

#### Scenario: RED 状态验证（回归守卫）

- **WHEN** 临时还原 `pdk_entry.cpp:244` 回 `action_tool = ""`
- **THEN** 新 e2e case MUST FAIL（"Tool '' not registered" 或 template render error）
- **AND** 恢复 fix 后 MUST PASS

### Requirement: dsl.md §5.2 react loop 终止路径契约

`docs/specs/dsl.md §5.2 tool_call` MUST 明确 react loop 终止路径契约：`decision.final=true` 时 act 节点调 `finish` 工具作为 ReAct 终止语义（per Path D 设计意图）。

#### Scenario: dsl.md §5.2 文档契约

- **WHEN** 用户查阅 `docs/specs/dsl.md §5.2 tool_call`
- **THEN** MUST 明确说明 react loop 终止路径 + reference this OpenSpec change archive
- **AND** MUST 说明 `finish` 工具作为 terminal action 的语义约定

## MODIFIED Requirements

### Requirement: lib/loop/react.agent.md 保持不变（act 节点是 dynamic dispatch 抽象）

`lib/loop/react.agent.md` MUST 保持当前结构（act 节点 `tool: "{{decision.action_tool}}"` + `arguments: "{{decision.action_args}}"` 模板渲染形式）。act 节点**不是问题根源**，是 ReAct 模式 dynamic dispatch 抽象层；问题在 `parse_react_decision` L3 fallback 返回值偏离设计意图。本 change 不动 react.agent.md。

#### Scenario: react.agent.md 不变 + 修复后正常工作

- **WHEN** 实施本 change 后
- **THEN** `lib/loop/react.agent.md` 文件内容 MUST 不变（git diff 仅碰 `pdk_entry.cpp` + `test_loop_agent_plugin.cpp` + `dsl.md` + `AGENTS.md`）
- **AND** react DSL MUST 完整走通 think → decide → act(finish) → observe → end 路径

### Requirement: parse_react_decision L1/L2 action 路径不变

`parse_react_decision` L1（OpenAI function_call JSON）和 L2（XML `<tool>/<args>`）路径 MUST 保持现有契约：`final=false` + `action_tool` 为 LLM 声明的工具名 + `action_args` 为 object。L1/L2 既有测试 MUST 零回归。

#### Scenario: L1 function_call JSON 路径不变

- **WHEN** LLM 返回 `{"name":"fs/read","arguments":{"path":"a.txt"}}`
- **THEN** `parse_react_decision` MUST 返回 `{final: false, action_tool: "fs/read", action_args: {"path":"a.txt"}, response: "<original>"}`
- **AND** react DSL act 节点 MUST dispatch 到 fs/read 工具

#### Scenario: L2 XML 路径不变

- **WHEN** LLM 返回 `<tool>fs/read</tool><args>{"path":"a.txt"}</args>`
- **THEN** `parse_react_decision` MUST 返回 `{final: false, action_tool: "fs/read", action_args: {"path":"a.txt"}, response: "<original>"}`