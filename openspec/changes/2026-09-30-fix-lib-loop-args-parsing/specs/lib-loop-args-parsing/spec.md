## ADDED Requirements

### Requirement: lib/loop/*.agent.md DSL tool_call 节点使用 `arguments:` canonical key

`lib/loop/{react,plan_execute,fork_join}.agent.md` 文件中的 `tool_call` 节点 MUST 使用 `arguments:` 字段 (per `docs/specs/dsl.md §5.2` canonical 规范) 而非 `args:`。`arguments` 字段 MUST 为 object, key 为参数名, value 为字符串模板。

#### Scenario: react.agent.md decide 节点使用 arguments

- **WHEN** 解析 `lib/loop/react.agent.md` 的 decide 节点 (`type: tool_call`, `tool: loop/decide_react`)
- **THEN** 节点 MUST 包含 `arguments: {response: "{{llm_response}}"}` 字段
- **AND** parser (`node_factory.cpp make_tool_call`) MUST 能读取 `response` 参数并在 dispatch 时传递给 `loop/decide_react` 工具

#### Scenario: plan_execute.agent.md execute 节点使用 arguments

- **WHEN** 解析 `lib/loop/plan_execute.agent.md` 的 execute 节点
- **THEN** 节点 MUST 包含 `arguments: {plan: "{{plan_response}}"}` 字段
- **AND** parser MUST 能读取 `plan` 参数并在 dispatch 时传递给 `loop/execute_plan` 工具

#### Scenario: fork_join.agent.md task 节点使用 arguments

- **WHEN** 解析 `lib/loop/fork_join.agent.md` 的 task_a / task_b / task_c 节点
- **THEN** 每个节点 MUST 包含 `arguments: {input: "..."}` 字段
- **AND** parser MUST 能读取 `input` 参数并在 dispatch 时传递给 `loop/process_task` 工具

### Requirement: DSL parser schema 一致性 (防御纵深)

`src/modules/parser/node_factory.cpp` 的 `make_tool_call` MUST 接受 `arguments:` canonical key. 可选 alias (向后兼容): 同时接受 `args:` 但要优先 `arguments:`。

#### Scenario: arguments: canonical 优先

- **WHEN** `tool_call` 节点同时包含 `arguments:` 和 `args:` 字段
- **THEN** parser MUST 使用 `arguments:` 的值, 忽略 `args:`

#### Scenario: 仅 args: (向后兼容 fallback)

- **WHEN** `tool_call` 节点仅包含 `args:` 字段 (legacy 第三方 .agent.md)
- **THEN** parser MUST 接受 `args:` 作为 fallback (向后兼容)

#### Scenario: arguments 缺失 (无 fallback)

- **WHEN** `tool_call` 节点既无 `arguments:` 也无 `args:`
- **THEN** parser MUST 使用空 map, dispatch 时 args 为空

### Requirement: lib/* DSL schema audit CI 脚本

`tools/check_dsl_schema.sh` MUST 验证 `lib/*.agent.md` 所有 `tool_call` 节点使用 `arguments:` 而非 `args:`。CI 脚本 MUST 在 PR 流程中执行.

#### Scenario: CI 脚本检测 spec drift

- **WHEN** 任何 `lib/*.agent.md` 文件的 `tool_call` 节点包含 `args:` 字段
- **THEN** `tools/check_dsl_schema.sh` MUST exit non-zero 并打印违规文件:行号

#### Scenario: CI 脚本通过 (无 drift)

- **WHEN** 所有 `lib/*.agent.md` 文件使用 `arguments:`
- **THEN** `tools/check_dsl_schema.sh` MUST exit zero

### Requirement: DSL parser regression test

`tests/test_parser_dsl_schema.cpp` MUST 提供 regression test 覆盖 lib/loop 3 文件 schema 一致性. CI 拦截未来 spec drift.

#### Scenario: react.agent.md 解析成功

- **WHEN** `tests/test_parser_dsl_schema.cpp` Test Case 1 解析 `lib/loop/react.agent.md`
- **THEN** `decide` 节点的 `arguments` 非空, 含 `response` key
- **AND** parser 不抛错

#### Scenario: plan_execute.agent.md 解析成功

- **WHEN** Test Case 2 解析 `lib/loop/plan_execute.agent.md`
- **THEN** `execute` 节点的 `arguments` 非空, 含 `plan` key

#### Scenario: fork_join.agent.md 解析成功

- **WHEN** Test Case 3 解析 `lib/loop/fork_join.agent.md`
- **THEN** 3 个 task 节点的 `arguments` 非空, 含 `input` key

#### Scenario: 其他 lib DSL 回归 (load.agent.md)

- **WHEN** Test Case 4 解析 `lib/inference/load.agent.md`
- **THEN** 节点的 `arguments` 非空, 与修复前行为一致 (零回归)

## MODIFIED Requirements

### Requirement: dsl.md §5.2 tool_call 节点兼容性说明

`docs/specs/dsl.md §5.2` MUST 明确 `arguments:` 为 canonical key, 同时说明 parser 对 legacy `args:` 向后兼容. 防止 spec 与实现 漂移.

#### Scenario: dsl.md §5.2 spec drift 检测

- **WHEN** `tools/check_dsl_schema.sh` 运行
- **THEN** 验证所有 `lib/*.agent.md` 文件与 `docs/specs/dsl.md §5.2` 一致
- **AND** CI MUST 拦截未来 spec drift

### Requirement: act 节点字符串模板值 args 契约 (独立 follow-up, 不在本 change scope)

`react.agent.md:33` 的 act 节点 `args: "{{decision.action_args}}"` 是**字符串模板值** (非 object), 即使改名为 `arguments:` 也会被 parser 的 `is_object()` 检查跳过. **本 change 不修**, 登记为独立 follow-up improvement (扩展 parser 接受字符串值). MUST 不在本 change scope.

#### Scenario: act 节点字符串模板值 args (pre-existing 限制)

- **WHEN** react loop execute 解析 `react.agent.md` act 节点
- **THEN** 因 `args: "{{decision.action_args}}"` 是字符串, parser `is_object()` 检查跳过, args 为空
- **AND** act 节点 dispatch 时实际传递空 args (不包含 decision.action_args 模板渲染结果)
- **NOTE**: 改名为 `arguments: "{{decision.action_args}}"` 不会改变此行为. 需 parser 扩展接受字符串值 (`j["arguments"].is_string() ? render as single-arg : object-iter`). **独立 follow-up, 不在本 change**.