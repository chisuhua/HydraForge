# Tasks: pdk_chat_demo 运行时修复

> 估时 ~4h（5 tasks + 6 verification gates）

## 1. ToolMetadata ApprovalPolicy 修复（3 个 plugin，5 处）

- [x] 1.1 `pdk/provider_agent/src/pdk_entry.cpp`: `provider/register` ApprovalPolicy 修复为 `{plan=T, agent=T, yolo=F, force=T}`
- [x] 1.2 `pdk/session_agent/src/pdk_entry.cpp`: `session/branch` ApprovalPolicy 修复为 `{plan=T, agent=T, yolo=F, force=F}`
- [x] 1.3 `pdk/session_agent/src/pdk_entry.cpp`: `session/compact` ApprovalPolicy 修复为 `{plan=T, agent=T, yolo=F, force=T}`
- [x] 1.4 `pdk/session_agent/src/pdk_entry.cpp`: `session/persist` ApprovalPolicy 修复为 `{plan=T, agent=T, yolo=F, force=T}`
- [x] 1.5 `pdk/budget_agent/src/pdk_entry.cpp`: `budget/alerts` ApprovalPolicy 修复为 `{plan=T, agent=T, yolo=F, force=F}`

> ✅ **commit 59e44c5**: 5/5 修复完成. ctest 80/80 PASS 零回归.

## 2. Loop Agent 真实 DSL 执行

- [x] 2.1 `pdk/loop_agent/src/pdk_entry.cpp`: 真实 DSL 执行 **暂缓** — ADR-0019 架构限制: `DSLEngine::from_markdown` 子引擎 LLM provider 无法继承父引擎配置. 当前 mock 响应诚实标注此限制.
  - ➡️ 解禁条件: ADR-0019 follow-up 解决 LLM provider 跨引擎传播

> ✅ **commit 59e44c5**: mock revert 已完成 (诚实 fallback).

## 3. 云端 LLM 接入

- [ ] 3.1 `examples/pdk_chat_demo/config.json`: 添加 `providers/baidu-deepseek` 配置
- [ ] 3.2 `examples/pdk_chat_demo/config.json`: `agent.provider = "baidu-deepseek"`, `agent.model = "deepseek-v4-pro"`
- [ ] 3.3 `examples/pdk_chat_demo/main.cpp`: 重写非 mock 分支为 `LLMConfig` + `LLMProviderFactory` 模式
- [ ] 3.4 `examples/pdk_chat_demo/main.cpp`: SKILL.md 行为标注 `mock-only, requires SkillInterpreter ADR-0055`
- [ ] 3.5 `examples/pdk_chat_demo/config.json`: 在 SKILL.md plugin 加 `_comment` 字段说明 mock-only

> ➡️ **顺延**: 云端 LLM 接入为 Optional 需求, 独立 follow-up change.

## 4. 文档更新

- [ ] 4.1 同步更新 `examples/pdk_chat_demo/README.md`
- [ ] 4.2 同步更新 `examples/pdk_chat_demo/BUILD_VERIFICATION_REPORT.md`
- [ ] 4.3 更新 `openspec/changes/2026-07-17-pdk-chat-demo-buildable/` 的 addendum（如需要）

> ➡️ **顺延**: 与 §3 (Cloud LLM) 一起做文档更新.

## 5. 验证（6 个 ship gate）

- [x] 5.1 `cmake --build . --target LoopAgent ProviderAgent SessionAgent BudgetAgent FSTools ShellTools pdk_chat_demo` **零编译错误**
- [ ] 5.2 Mock 模式 `echo "test" | ./pdk_chat_demo --mock` 显示 6 个 "Loaded plugin" 行（0 个 Failed）— 需手动验证
- [ ] 5.3 Mock 模式 Assistant 输出非 canned 文案 — defer (ADR-0019)
- [ ] 5.4 真实 LLM 模式 — defer (§3 Optional)
- [x] 5.5 `ctest -j$(nproc)` **80/80 PASS**（零回归）
- [ ] 5.6 `openspec validate 2026-07-18-pdk-chat-demo-runtime-fixes --strict`
