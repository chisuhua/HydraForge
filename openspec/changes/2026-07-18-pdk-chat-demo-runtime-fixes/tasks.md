# Tasks: pdk_chat_demo 运行时修复

> 估时 ~4h（5 tasks + 6 verification gates）

## 1. ToolMetadata ApprovalPolicy 修复（3 个 plugin，5 处）

- [ ] 1.1 `pdk/provider_agent/src/pdk_entry.cpp`: `provider/register` (line 57-64) ApprovalPolicy 修复为 `{plan=T, agent=T, yolo=F, force=T}`
- [ ] 1.2 `pdk/session_agent/src/pdk_entry.cpp`: `session/branch` (line 108-115) ApprovalPolicy 修复为 `{plan=T, agent=T, yolo=F, force=F}`
- [ ] 1.3 `pdk/session_agent/src/pdk_entry.cpp`: `session/compact` (line 136-142) ApprovalPolicy 修复为 `{plan=T, agent=T, yolo=F, force=T}`
- [ ] 1.4 `pdk/session_agent/src/pdk_entry.cpp`: `session/persist` (line 173-179) ApprovalPolicy 修复为 `{plan=T, agent=T, yolo=F, force=T}`
- [ ] 1.5 `pdk/budget_agent/src/pdk_entry.cpp`: `budget/alerts` (line 106-112) ApprovalPolicy 修复为 `{plan=T, agent=T, yolo=F, force=F}`

## 2. Loop Agent 真实 DSL 执行

- [ ] 2.1 `pdk/loop_agent/src/pdk_entry.cpp`: 重写 `loop/run` handler（line 92-128）为真实 DSL 执行：
  - 读取 `lib/loop/<type>.agent.md`
  - `DSLEngine::from_markdown(content)` 创建 sub-engine
  - 构造 `LayeredContext` 注入 user_input / system_prompt / history
  - `sub_engine->run(ctx)` 执行
  - 从 `ExecutionResult.final_context` 提取 response/steps/tokens
  - 文件不存在时 fallback 到 mock 响应

## 3. 云端 LLM 接入

- [ ] 3.1 `examples/pdk_chat_demo/config.json`: 添加 `providers/baidu-deepseek` 配置（api_key_env / api_url / api_endpoint / models）
- [ ] 3.2 `examples/pdk_chat_demo/config.json`: `agent.provider = "baidu-deepseek"`, `agent.model = "deepseek-v4-pro"`
- [ ] 3.3 `examples/pdk_chat_demo/main.cpp` (line 132-144): 重写非 mock 分支为 `LLMConfig` + `LLMProviderFactory` 模式
- [ ] 3.4 `examples/pdk_chat_demo/main.cpp` (line 105-108): SKILL.md 行为标注 `mock-only, requires SkillInterpreter ADR-0055`
- [ ] 3.5 `examples/pdk_chat_demo/config.json`: 在 SKILL.md plugin 加 `_comment` 字段说明 mock-only

## 4. 文档更新

- [ ] 4.1 同步更新 `examples/pdk_chat_demo/README.md`: 注明 6 plugins 全部加载 + Loop Agent 真实执行 + 云端 LLM 用法
- [ ] 4.2 同步更新 `examples/pdk_chat_demo/BUILD_VERIFICATION_REPORT.md`: 标注本 change 修复的运行时 gap
- [ ] 4.3 更新 `openspec/changes/2026-07-17-pdk-chat-demo-buildable/` 的 addendum（如需要）

## 5. 验证（6 个 ship gate）

- [ ] 5.1 `cmake --build . --target LoopAgent ProviderAgent SessionAgent BudgetAgent FSTools ShellTools pdk_chat_demo` 零编译错误
- [ ] 5.2 Mock 模式 `echo "test" | ./pdk_chat_demo --mock` 显示 6 个 "Loaded plugin" 行（0 个 Failed）
- [ ] 5.3 Mock 模式 Assistant 输出非 canned 文案（来自真实 react.agent.md execution）
- [ ] 5.4 真实 LLM 模式 `QIANFAN_API_KEY=... ./pdk_chat_demo` 成功调用 deepseek-v4-pro（如果 API key 可用）
- [ ] 5.5 `ctest -j$(nproc)` 80/80 PASS（零回归）
- [ ] 5.6 `openspec validate 2026-07-18-pdk-chat-demo-runtime-fixes --strict` 通过
