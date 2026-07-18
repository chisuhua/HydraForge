# Spec: pdk_chat_demo 运行时修复

## ADDED Requirements

### Requirement: ToolMetadata ApprovalPolicy Compliance

`pdk_chat_demo` 中所有 PDK plugin 注册 dangerous category 工具（`Execute` / `Network` / `StateModify`）时 SHALL 设置 `ApprovalPolicy.requires_approval_in_plan == true` 或 `requires_approval_in_agent == true`，满足 ToolRegistry V2 校验规则（`src/common/tools/registry.cpp:102-106`）。

#### Scenario: provider/register 注册成功

- **GIVEN** `pdk/provider_agent/src/pdk_entry.cpp` 的 `provider/register` 工具 metadata
- **WHEN** `ToolRegistry::register_tool_function(...)` 被调用
- **THEN** ApprovalPolicy 为 `{plan=T, agent=T, yolo=F, force=T}`
- **AND** `ToolRegistry` 不抛 `std::invalid_argument` 异常
- **AND** `has_tool("provider/register")` 返回 true

#### Scenario: session/persist 注册成功

- **GIVEN** `pdk/session_agent/src/pdk_entry.cpp` 的 `session/persist` 工具 metadata
- **WHEN** 磁盘写入操作被执行
- **THEN** ApprovalPolicy 为 `{plan=T, agent=T, yolo=F, force=T}`（force_approval_always=true 因为不可逆写盘）
- **AND** `ToolRegistry` 接受该工具（不抛异常）
- **AND** 强制审批机制启用

#### Scenario: budget/alerts 注册成功

- **GIVEN** `pdk/budget_agent/src/pdk_entry.cpp` 的 `budget/alerts` 工具 metadata
- **WHEN** ToolRegistry 验证 ApprovalPolicy
- **THEN** 接受 `{plan=T, agent=T, yolo=F, force=F}` 配置（force=false 因为可逆订阅）

#### Scenario: 5 个违规工具全部修复

- **WHEN** 完成所有 5 个 ToolMetadata 修改（provider/register, session/branch, session/compact, session/persist, budget/alerts）
- **THEN** 运行 `./pdk_chat_demo --mock` 输出 6 个 `[main] Loaded plugin: ...` 行
- **AND** 0 个 `Failed to load plugin` 行

### Requirement: Loop Agent 真实 DSL 执行

`pdk/loop_agent/src/pdk_entry.cpp` 的 `loop/run` handler SHALL 真正加载 `lib/loop/<type>.agent.md` 文件并通过 `DSLEngine::from_markdown` + `run(LayeredContext)` 执行。文件不存在时 SHALL fallback 到 mock 响应（不抛异常）。

#### Scenario: react loop 真实执行

- **GIVEN** `lib/loop/react.agent.md` 存在
- **AND** `loop/run` 工具被 `ChatSession::chat()` 调用
- **WHEN** handler 读取 react.agent.md 内容并创建 sub-engine
- **THEN** 返回的 JSON `response` 字段**不是** canned 文本（"[loop_agent/react] Processed: ..."）
- **AND** 返回的 `steps` / `tokens_used` 来自 `ExecutionResult.final_context`

#### Scenario: 文件不存在 fallback

- **GIVEN** `lib/loop/<type>.agent.md` 不存在
- **WHEN** handler 尝试打开文件失败
- **THEN** 返回 fallback mock 响应
- **AND** `error` 字段包含 "loop_agent: cannot open <path>"
- **AND** 不抛异常给上游（ChatSession 仍可继续）

### Requirement: Cloud LLM Integration (Optional)

当 `QIANFAN_API_KEY` 环境变量已设置且 `config.agent.provider == "baidu-deepseek"` 时，`pdk_chat_demo` SHALL 使用 `LLMProviderFactory` + `CloudLLMAdapter` 调用真实云端 LLM（百炼 deepseek-v4-pro）。

#### Scenario: 真实 LLM 响应

- **GIVEN** `QIANFAN_API_KEY=sk-xxx` 已 export
- **AND** `config.json` 的 `agent.provider = "baidu-deepseek"`, `agent.model = "deepseek-v4-pro"`
- **WHEN** 运行 `./pdk_chat_demo` (无 --mock flag)
- **THEN** 调用 `https://qianfan.baidubce.com/v2/coding/chat/completions` 端点
- **AND** `Authorization: Bearer <api_key>` header 正确
- **AND** 返回真实 LLM 响应（而非 MockLLMProvider 的 canned 响应）

#### Scenario: API key 缺失 fallback

- **GIVEN** `QIANFAN_API_KEY` 未设置
- **WHEN** 尝试调用真实 LLM
- **THEN** fallback 到 `MockLLMProvider`
- **AND** stderr 输出 `[main] QIANFAN_API_KEY not set, falling back to MockLLMProvider`

### Requirement: SKILL.md 行为诚实化

`main.cpp` 处理 `plugin_cfg.type == "skill"` 时 SHALL 显式标注 mock-only 状态和 ADR-0055 依赖，避免误导用户认为 SKILL.md 已实际执行。

#### Scenario: skill.code_review 注册日志

- **GIVEN** `config.json` 包含 `skill.code_review` plugin（type=skill）
- **WHEN** `PluginLoader` 处理该 plugin
- **THEN** stdout 输出 `"[main] Skill registered (mock-only, requires SkillInterpreter ADR-0055): skill.code_review"`
- **AND** 不调用任何 SKILL.md 解释器（避免误导用户）
- **AND** `config.json` 的 skill plugin 条目包含 `_comment` 字段说明 mock-only 原因

### Requirement: 零回归

所有现有 ctest 测试 SHALL 在修复完成后继续通过。

#### Scenario: ctest 全量通过

- **WHEN** 运行 `ctest -j$(nproc) --output-on-failure`
- **THEN** 80/80 测试 PASS
- **AND** 总耗时 < 5 秒
- **AND** `test_chat_session`、`test_e2e_mock`、`test_itoolregistry_json_args` 等新增测试不受影响
