## ADDED Requirements

### Requirement: llm_call 节点 output_keys 数据类型契约

`llm_call` 节点的 `output_keys` 字段必须 (SHALL) 把 LLM 响应的 `result.text` 字符串（而非完整 result JSON 对象）存入 `new_context[output_keys[0]]`，供后续节点的 inja `{{...}}` 模板渲染时直接解析为字符串。

#### Scenario: 单 output_key 存 LLM 文本
- **WHEN** `llm_call` 节点执行完成, `result.success == true`, `result.text` 非空
- **THEN** `new_context[output_keys[0]] == result.text` (类型为 std::string)
- **AND** 后续节点的 `args` inja 模板 `{{output_keys[0]}}` 渲染为非空字符串

#### Scenario: 多 output_keys 按顺序存 JSON 子结构
- **WHEN** `llm_call` 节点 `output_keys.size() > 1`
- **THEN** `new_context[output_keys[i]]` 按 JSON 路径分别存 `result` JSON 子字段

#### Scenario: streaming 路径同契约
- **WHEN** `llm_call` 节点走 streaming 路径 (line 134-167)
- **THEN** `new_context[output_keys[0]] == result.text` 完整文本 (类型为 std::string, 与 non-streaming 一致; 由 stream_sink 切片后推送, new_context 持有完整值)

### Requirement: 跨节点 ctx 传递数据完整性

DSL 节点执行链中, 上一节点 `output_keys` 写入的 ctx 字段在下一节点 `args` inja 渲染时必须 (SHALL) **严格可访问**, 无任何数据丢失或类型退化。

#### Scenario: think → decide 节点链 (react loop)
- **WHEN** `think` (llm_call) 节点完成, output_keys=['llm_response']
- **AND** `decide` (tool_call) 节点 `args: response: "{{llm_response}}"`
- **THEN** inja 渲染结果为 LLM 实际响应文本 (非空)
- **AND** `decide_react` 工具收到非空 `response` 参数

#### Scenario: llm_call → llm_call 节点链 (plan_execute loop)
- **WHEN** `plan` (llm_call) 节点完成, output_keys=['plan_response']
- **AND** `verify` (llm_call) 节点的 prompt_template 含 `{{plan_response}}`
- **THEN** verify 节点 prompt 渲染包含 plan 节点的实际 LLM 输出

#### Scenario: tool_call → assign 节点链
- **WHEN** `act` (tool_call) 节点完成, output_keys=['tool_result']
- **AND** `observe` (assign) 节点 `assign: {history: "{{history}}\nObservation: {{tool_result}}"}`
- **THEN** assign 节点的 history 字段拼接 tool_result 内容 (非空字符串)

### Requirement: inja 模板渲染错误处理契约

`InjaTemplateRenderer::render` 在遇到未定义变量或类型不匹配时, 必须 (SHALL) 抛出 `std::runtime_error` (而非返回空字符串), 让 `NodeExecutor` 抛出可识别的 TemplateRenderError 异常, 避免**静默错误传播**。

#### Scenario: 未定义变量抛错
- **WHEN** 模板 `"{{undefined_var}}"` 渲染, `context` 不含 `undefined_var`
- **THEN** `InjaTemplateRenderer::render` 抛 `std::runtime_error` (message 含 "Template render error")
- **AND** `NodeExecutor` 在 line 230 (`tool_call` 渲染) catch 后抛 `ToolCallRenderError` (含模板路径)

#### Scenario: 类型不匹配抛错
- **WHEN** 模板 `"{{ctx_field}}"` 渲染, `ctx_field` 类型非 string/non-null (e.g., nested object)
- **THEN** inja 抛 InjaError, `InjaTemplateRenderer::render` 转抛 runtime_error
- **AND** NodeExecutor 暴露完整 template path + ctx_field type 信息给上层

#### Scenario: 模板字符串无变量时直接返回
- **WHEN** 模板 `"literal text, no variables"`
- **THEN** 渲染返回原字符串, 无任何错误

### Requirement: react.agent.md decide 节点 args 契约

`lib/loop/react.agent.md` 的 `decide` 节点 `args: response: "{{llm_response}}"` 必须 (SHALL) 保证 `llm_response` 在 inja 渲染时为非空字符串, 满足 `loop/decide_react` 工具的 `parse_react_decision(response)` 输入要求。

#### Scenario: think 节点正常输出后 decide 节点渲染
- **WHEN** LLM 返回真实文本 (≥1 token)
- **THEN** `decide` 节点 `response` 参数 = LLM 文本 (length ≥ 1)
- **AND** `parse_react_decision` 3 级 fallback 解析非失败

#### Scenario: think 节点空响应时显式失败 (F1 fix)
- **WHEN** LLM 返回空 `result.text` (e.g., reasoning 模型 content 在 reasoning_content, 或 model 遮蔽返回 400 但 success=true)
- **THEN** think 节点检测空字符串, **NodeExecutor 抛 `std::runtime_error`** 含 "LLM call succeeded but returned empty text for node '...' output_key '...'. Check provider model availability or prompt template."
- **AND** 错误沿 scheduler → engine run → DSL CALL 路径传播, 调用方捕获
- **NOTE**: 不引入新 ErrorCode (per AGENTS.md 模式 #1 minimal fix); 不发射 `loop.error` event (事件契约保留 chat_session 层错误 emit, 不在 NodeExecutor 层重复)

### Requirement: fork/join 节点 ctx 共享契约 (D3 RESOLVED)

`fork_join.agent.md` 当前 schema 用 `{{user_input}}` 顶层访问 (fork 前共享), 不依赖跨分支 ctx — **当前 fork_join 不受影响**。`loop/decide_react` 在 `pdk_entry.cpp:106` (child engine registry) 和 `:405` (plugin/parent registry) 注册两次, 是两个不同 registry 的独立 ToolMetadata, **不是 bug** (per Oracle ses_f4caa8cf0ffeajSx05M235DwDz 审计)。`fork/join` 节点 ctx 共享契约必须 (SHALL) 满足上述现状。

#### Scenario: fork_join loop 当前 schema 工作 (D3 现状 OK)
- **WHEN** `lib/loop/fork_join.agent.md` 当前 3 task 分支都用 `{{user_input}}` (fork 前共享)
- **THEN** 不需要 fork/join ctx 隔离策略, 当前实现满足需求
- **AND** 未来若 fork 分支需 LLM response 跨分支共享, 须显式 declare (新 schema 设计, 独立 change)

### Requirement: plan_execute.agent.md / fork_join.agent.md 同契约

`plan_execute.agent.md` 的 `execute` 节点 `args: plan: "{{plan_response}}"` 与 `fork_join.agent.md` 的 `task_a/b/c` 节点 `args: input: "{{user_input}}"` 必须 (SHALL) 满足与 react.agent.md 相同的 ctx bridge 契约 (output_keys→args 桥接).

#### Scenario: plan_execute loop execute 节点
- **WHEN** `plan` (llm_call) 节点完成, output_keys=['plan_response']
- **AND** `execute` (tool_call) 节点 `args: plan: "{{plan_response}}"`
- **THEN** `execute_plan` 工具收到非空 `plan` 参数 (含 LLM 生成的 DSL markdown)

#### Scenario: fork_join loop task 节点
- **WHEN** fork 节点派发 3 个 task 分支
- **AND** 每个 task 节点 `args: input: "{{user_input}}"`
- **THEN** `process_task` 工具收到非空 `input` 参数