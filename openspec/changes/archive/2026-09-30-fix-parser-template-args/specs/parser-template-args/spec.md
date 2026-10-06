## ADDED Requirements

### Requirement: DSL parser `tool_call` 接受 string-value arguments (single-arg convention)

`src/modules/parser/node_factory.cpp make_tool_call` MUST 接受 `arguments:` 字段为 string value 形式. 当 `arguments` 是 string 时, parser MUST 存储为 `{"input": "<value>"}` 单参数 map (与 `parse_react_decision` L1/L2 fallback `{"input", args_str}` 对齐, per pdk_entry.cpp:198, 219).

#### Scenario: arguments 是字符串模板（如 react.act 节点）

- **WHEN** DSL 节点声明 `arguments: "{{some_template}}"`
- **THEN** parser MUST 存储 `ToolCallNode.arguments["input"] = "{{some_template}}"`
- **AND** executor 端 `InjaTemplateRenderer::render(args["input"], ctx)` MUST 正常工作

#### Scenario: arguments 是 object（既有契约，不破坏）

- **WHEN** DSL 节点声明 `arguments: { key1: "value1", key2: "{{template}}" }`
- **THEN** parser MUST 存储既有契约 (per `fix-lib-loop-args-parsing` ship)，行为不变
- **AND** executor 渲染每个 key 的 value against context

### Requirement: DSL executor `tool_call` tool_name 模板渲染

`src/modules/executor/node_executor.cpp execute_tool_call` MUST 在 main path (L288-298) + stream path (L259-285) 对 `node->tool_name` 调用 `InjaTemplateRenderer::render()` against context, 渲染后查 registry + dispatch.

#### Scenario: tool 是模板字符串（如 react.act 节点）

- **WHEN** `tool:` 字段含 `{{...}}` 模板（如 `tool: "{{decision.action_tool}}"`)
- **AND** Context 提供 `decision.action_tool = "fs/read"`
- **THEN** executor MUST 渲染后查 registry → `fs/read` 注册则 dispatch 成功

#### Scenario: tool 是字面值（向后兼容）

- **WHEN** `tool:` 字段是字面值（如 `tool: "loop/decide_react"`）
- **AND** `loop/decide_react` 在 registry
- **THEN** executor MUST 渲染后仍 = `"loop/decide_react"`，dispatch 成功

#### Scenario: tool 模板渲染失败（tool 不存在）

- **WHEN** 模板渲染结果在 registry 不存在
- **THEN** executor MUST 抛 `std::runtime_error("Tool '<rendered>' not registered for node: <path>")`
- **AND** 错误消息 MUST 包含渲染后值（而非模板字符串）

### Requirement: DSL spec dsl.md §5.2 扩展 string arguments 契约

`docs/specs/dsl.md §5.2 tool_call` MUST 明确说明：
- `arguments` 字段接受 string-value 形式（单参数 `input` 约定）
- `arguments` 字段接受 object 形式（既有契约）
- `tool` 字段在 execute-time 模板渲染

#### Scenario: dsl.md §5.2 文档更新

- **WHEN** 用户查阅 `docs/specs/dsl.md §5.2 tool_call`
- **THEN** MUST 说明上述 3 项契约
- **AND** MUST reference this OpenSpec change archive

### Requirement: parser template args regression guard

`tests/test_parser_template_args.cpp` MUST 提供至少 3 cases regression test 覆盖 parser 扩展 + executor 渲染 + 既有契约兼容.

#### Scenario: Case 1 parser string arguments wrap

- **WHEN** 解析 `arguments: "{{x.y}}"` 形式 DSL
- **THEN** `tc->arguments.size() == 1 && tc->arguments.count("input") == 1`
- **AND** `tc->arguments.at("input") == "{{x.y}}"`

#### Scenario: Case 2 parser object arguments preserved（防回归）

- **WHEN** 解析既有 `arguments: { key1: "value1" }` 形式
- **THEN** 行为不变 (per `fix-lib-loop-args-parsing` ship)
- **AND** `tc->arguments` 含 `key1` 而非 `input`

#### Scenario: Case 3 executor tool_name render

- **WHEN** 构造 mock ToolRegistry 注册 `finish` 工具
- **AND** Context 注入 `decision.action_tool = "finish"`
- **THEN** `execute_tool_call` MUST dispatch 成功

#### Scenario: Case 4 executor tool_name literal

- **WHEN** `tool: "literal_tool"` literal 路径
- **THEN** 渲染后仍 = `"literal_tool"` (template 不变)

### Requirement: test_e2e_real_llm ChatSession case 必须 PASS

`test_e2e_real_llm` ChatSession case MUST 在本 change ship 后 PASS（真实 DeepSeek LLM 端到端）.

#### Scenario: ChatSession case 4/4 PASS

- **WHEN** 真实 DeepSeek LLM 调用 ChatSession e2e（`./build/examples/pdk_chat_demo/tests/test_e2e_real_llm`）
- **THEN** `test_e2e_real_llm` MUST 4/4 cases PASS
- **AND** ChatSession case 不再 FAIL "Tool '{{decision.action_tool}}' not registered"

## MODIFIED Requirements

### Requirement: lib/loop react.agent.md act 节点保持 string-value arguments（不破坏既有 DSL）

`lib/loop/react.agent.md:33` act 节点 MUST 保持 `arguments: "{{decision.action_args}}"` 字符串模板值形式. 本 change 实施后此 DSL 形式 MUST 正常工作.

#### Scenario: react.agent.md act 节点可执行

- **WHEN** react loop execute 解析 `react.agent.md` act 节点（含 `tool: "{{decision.action_tool}}"` + `arguments: "{{decision.action_args}}"`）
- **AND** Context 含 `decision` (来自 `loop/decide_react` 输出)
- **THEN** executor MUST 渲染 tool name + arguments 后成功 dispatch 到 action_tool
- **AND** 行为 MUST 与 React 模式语义一致 (think → decide → act → observe → end)