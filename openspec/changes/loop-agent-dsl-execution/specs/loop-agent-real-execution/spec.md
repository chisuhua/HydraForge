## ADDED Requirements

### Requirement: Loop/run MUST execute real DSL (not mock)

`pdk/loop_agent` 的 `loop/run` 工具 MUST 从当前 mock 响应改为真实加载 `.agent.md` 文件并通过 `DSLEngine` 执行。SHALL 支持三种 loop_type: `react`, `plan_execute`, `fork_join`。

#### Scenario: React 循环 DSL 执行
- **WHEN** 调用 `loop/run` 工具，参数 `loop_type="react"`, `prompt="hello"`，且父引擎 LLM provider 已就位
- **THEN** 工具加载 `lib/loop/react.agent.md` 文件
- **THEN** 通过 `DSLEngine::from_markdown(content, *parent_provider)` 创建子引擎并执行
- **THEN** 返回的 JSON 包含 `response` 字段（非 mock 文案）、`steps`、`tokens_used`、`cost_usd`、`success`

#### Scenario: PlanExecute 循环 DSL 执行
- **WHEN** 调用 `loop/run` 工具，参数 `loop_type="plan_execute"`, `prompt="hello"`
- **THEN** 工具加载 `lib/loop/plan_execute.agent.md` 文件
- **THEN** 通过 `DSLEngine::from_markdown(content, *parent_provider)` 创建子引擎并执行
- **THEN** 返回的 JSON 包含 `response`、`steps`、`tokens_used`、`cost_usd`、`success`

#### Scenario: ForkJoin 循环 DSL 执行
- **WHEN** 调用 `loop/run` 工具，参数 `loop_type="fork_join"`, `prompt="hello"`
- **THEN** 工具加载 `lib/loop/fork_join.agent.md` 文件
- **THEN** 通过 `DSLEngine::from_markdown(content, *parent_provider)` 创建子引擎并执行
- **THEN** 返回的 JSON 包含 `response`、`steps`、`tokens_used`、`cost_usd`、`success`

### Requirement: Provider tool SHALL support thread_local + security metadata

`loop/set_parent_provider` 工具 SHALL 使用 `thread_local` 存储 provider 指针。MUST 声明安全元数据：`category=SystemConfig`, `force_approval_always=true`, `allowed_layers={Workflow}`。覆盖旧 provider 时 SHOULD 输出 warning 日志。

#### Scenario: Provider 设置工具
- **WHEN** 在调用 `loop/run` 之前调用 `loop/set_parent_provider`
- **THEN** loop agent 内部记录父引擎的 LLM provider 引用
- **THEN** 后续 `loop/run` 调用使用此 provider

#### Scenario: 安全元数据
- **WHEN** `loop/set_parent_provider` 被注册到 ToolRegistry
- **THEN** 工具的 `category` 为 `SystemConfig`
- **THEN** 工具的 `approval_policy` 为 `force_approval_always=true`
- **THEN** 工具的 `allowed_layers` 仅包含 `{Workflow}`
- **THEN** Cognitive/Thinking 层的 DSL 节点不能调用此工具

#### Scenario: 覆盖警告
- **WHEN** `loop/set_parent_provider` 被调用且当前已有一个 provider 注册
- **THEN** 覆盖旧 provider 时输出 warning 日志
- **THEN** 不抛出异常
- **THEN** 返回 `{"success": true}`

### Requirement: Cost MUST be charged exactly once

子引擎通过父引擎 provider 执行的 LLM 调用 MUST 只产生一次 cost tracking 记录（SHALL NOT double-charge）。

#### Scenario: Cost 单次计费
- **WHEN** 父引擎已配置 CostTrackingDecorator
- **AND** 子引擎通过 `from_markdown(content, *parent.get_llm_provider())` 创建并执行 LLM 调用
- **THEN** parent 的 CostTrackingDecorator 记录 N 次 LLM 调用
- **THEN** 子引擎没有自己的 CostTrackingDecorator（无二次计费）
- **THEN** `parent_engine.get_session_cost()` == 子引擎 LLM 调用的总 cost（正确归入 parent budget）

### Requirement: Error handling MUST cover invalid input

`loop/run` 工具 MUST 处理 3 种错误场景：文件不存在（success=false）、非法 loop_type（success=false + 错误信息）、provider 未设置（mock fallback + error log）。

#### Scenario: 文件不存在错误
- **WHEN** 调用 `loop/run`，参数 `loop_type="nonexistent"`
- **THEN** 工具返回错误 `Loop Agent file not found: <path>`
- **THEN** `success=false`

#### Scenario: 非法 loop_type
- **WHEN** 调用 `loop/run`，参数 `loop_type=""` 或 `loop_type="invalid"`
- **THEN** 工具验证 `loop_type` 必须是 `react`/`plan_execute`/`fork_join` 之一
- **THEN** 非法值返回错误 `Invalid loop_type: <value>. Must be one of: react, plan_execute, fork_join`
- **THEN** `success=false`
- **THEN** 不加载任何文件，不创建子引擎

#### Scenario: Provider 未设置时的 Mock fallback
- **WHEN** 调用 `loop/run`，但 `loop/set_parent_provider` 从未被调用
- **THEN** 工具输出 error 日志："parent provider not set, falling back to mock"
- **THEN** 返回 mock 响应（与当前实现格式一致）
- **THEN** `success=true`（mock 执行不视为错误）