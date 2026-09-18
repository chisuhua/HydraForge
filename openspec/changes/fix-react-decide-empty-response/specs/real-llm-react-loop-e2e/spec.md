## ADDED Requirements

### Requirement: react loop 真实 LLM 端到端测试覆盖

`pdk_chat_demo` 在真实 DeepSeek LLM provider 下, react loop 必须 (SHALL) 能完整端到端执行至少 1 turn, 用户消息 → `think` (LLM call) → `decide` (tool_call) → `act` (tool_call) → `observe` (assign) → `end`, assistant 返回非空文本。

#### Scenario: react loop "Hello" prompt 端到端
- **WHEN** `pdk_chat_demo` 配置 real DeepSeek LLM (`DEEPSEEK_API_KEY` 已 export)
- **AND** 用户输入 `hello`
- **THEN** `loop.done` event `total_steps >= 1`
- **AND** Assistant 文本非空 (含 LLM 实际响应)
- **AND** 无 `loop.error` event 发射

#### Scenario: react loop 中文 prompt 端到端
- **WHEN** 用户输入 `用一句话解释 std::jthread`
- **THEN** Assistant 文本包含 `std::jthread` 或 `jthread` 关键词 (LLM 真实回复)
- **AND** `loop.done` event `total_steps >= 1`, `total_tokens > 0`

#### Scenario: react loop 多轮对话
- **WHEN** 用户连续输入 2 条消息 (turn 1 + turn 2)
- **THEN** 2 次 Assistant 响应均非空
- **AND** 第 2 turn Assistant 响应包含对第 1 turn 的上下文感知 (LLM 利用 history)

#### Scenario: react loop tool 调用路径
- **WHEN** 用户输入触发 LLM tool_call 决策 (e.g., "列出当前目录文件")
- **THEN** fs/read 工具被调用 (event `tool.execution.start` 发射)
- **AND** Assistant 文本包含文件列表内容

### Requirement: plan_execute loop 真实 LLM 端到端测试覆盖

`pdk_chat_demo` 真实 DeepSeek LLM 下, plan_execute loop 必须 (SHALL) 完整端到端: 用户消息 → `plan` (LLM 生成 DSL) → `execute` (DSLEngine 解析+执行) → `verify` (LLM 验证) → 完整闭环。

#### Scenario: plan_execute "研究 X" prompt
- **WHEN** 用户输入 `研究量子计算的最新进展`
- **THEN** plan 节点生成非空 DSL markdown
- **AND** execute 节点成功解析+执行 DSL
- **AND** verify 节点返回 yes/no, success 路径走完
- **AND** Assistant 文本含 plan_response 或 execution_result 字段内容

#### Scenario: plan_execute verify 失败 retry 路径
- **WHEN** verify 节点返回 no (max_retries=3)
- **THEN** 重新 plan → execute → verify 循环, 最多 3 次
- **AND** 3 次都失败时, result.success=false, error_message 含 retry 计数

### Requirement: fork_join loop 真实 LLM 端到端测试覆盖

`pdk_chat_demo` 真实 DeepSeek LLM 下, fork_join loop 必须 (SHALL) 端到端: 用户消息 → fork 派发 N 分支 → 各分支 LLM → join 聚合 → synthesize 节点。

#### Scenario: fork_join 3 分支并行
- **WHEN** `lib/loop/fork_join.agent.md` 加载, 用户输入 `对比 X / Y / Z 三方案`
- **THEN** 3 个 task 分支并行执行, 每分支 `process_task` 调 1 次 LLM
- **AND** synthesize 节点 prompt 含 3 个分支结果 (result_a/b/c)
- **AND** Assistant 文本含 3 分支内容合成

#### Scenario: fork_join 1 分支失败 fail-fast
- **WHEN** 某分支 LLM call 抛异常 (e.g., timeout)
- **THEN** 整个 fork_join result.success=false
- **AND** error_message 含失败分支 ID + 错误描述

### Requirement: 真实 LLM 测试 CI 守卫模式

真实 LLM 测试必须 (SHALL) 遵循 `chat-real-llm-coverage` 已 ship 的 CI 守卫模式 (per `tests/AGENTS.md` §REAL-LLM TEST PATTERNS): tag `[realllm]`, `require_real_llm_env()` 硬门槛, CI 默认 `HYDRAFORGE_SKIP_REAL_LLM=1` skip, 手动验证需显式 unset。

#### Scenario: 无 API key + 无 SKIP → FAIL
- **WHEN** CI 环境无 `DEEPSEEK_API_KEY` 或 `MINIMAX_API_KEY`
- **AND** `HYDRAFORGE_SKIP_REAL_LLM` 未设或 != 1
- **THEN** 真实 LLM 测试 FAIL (硬门槛, 防止 silent regression)

#### Scenario: 有 API key → run
- **WHEN** `DEEPSEEK_API_KEY` 已 export 或 `MINIMAX_API_KEY` 已 export
- **THEN** 真实 LLM 测试正常执行, 不被 skip

#### Scenario: SKIP=1 → SUCCEED
- **WHEN** `HYDRAFORGE_SKIP_REAL_LLM=1`
- **THEN** 真实 LLM 测试 SUCCEED (不执行, RAII 析构正常完成 — 不抛 SkipException 避免 hang, per AGENTS.md §REAL-LLM TEST PATTERNS 第 3 条)

### Requirement: 真实 LLM helper 三态分离

`tests/test_helpers/real_llm_env.h` 必须 (SHALL) 提供 `require_real_llm_env()` + `real_llm_env_skipped()` 双函数, 不合并为单函数 (per AGENTS.md §helper 三态分离 模式)。

#### Scenario: SKIP=1 静默通过
- **WHEN** 调用 `require_real_llm_env()` 且 `real_llm_env_skipped() == true`
- **THEN** 函数静默返回, 测试可继续 (不抛 SkipException)

#### Scenario: 无 key + 无 skip → FAIL
- **WHEN** 调用 `require_real_llm_env()` 且无 API key 且 skip != "1"
- **THEN** `FAIL("real LLM env required: ...")` (硬门槛)