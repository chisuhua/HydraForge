# pdk-chat-demo-v1-recap

**优先级**: P0 | **来源**: roadmap.md Phase 6a §任务 T1+T2
**阶段**: phase-6a | **分类**: demo-chat-v1
**类型**: feature

## 架构依据
- roadmap.md Phase 6a: pdk_chat_demo v1 收尾 (Session 持久化 + Budget 告警)
- roadmap.md Phase 6a: Schema 校验基础版 (拒绝错误格式)

## 范围
- Session 持久化: save_to_disk / load_from_disk / 过期清理
- Budget 告警: IInteractionBus event → TUI 显示
- Schema 校验: SchemaValidator → 拒绝非法 DSL

## 关键场景
- GIVEN mock mode, WHEN chat session starts, THEN session persisted to disk
- GIVEN budget exceeded, WHEN LLM call attempted, THEN alert shown in TUI
- GIVEN invalid .agent.md, WHEN parsed, THEN rejected with line-level error

## 技术约束
（无）

## 验收标准
- ctest -R pdk_chat 全绿
- ./pdk_chat_demo --mock Session 持久化 + Budget 告警正常工作
