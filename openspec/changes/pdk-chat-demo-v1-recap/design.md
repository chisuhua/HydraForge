# pdk_chat_demo v1 收尾设计

## Context

pdk_chat_demo 是 HydraForge Phase 5-6 的旗舰 Demo，展示 Agent-as-Plugin + SkillInterpreter + DeepSeek LLM 端到端能力。当前 v1 基础功能已运行，但存在质量负债。

## Design

### T1: Session 持久化

```
ChatSession 生命周期:
  load_from_disk() → new ChatSession()
  │
  ├─ 每轮对话后: save_to_disk()
  │   ├─ session_id, provider_ipc_mode, budget_total, budget_used
  │   └─ conversation_history (最近 N 条)
  │
  └─ 退出时: save_to_disk() + cleanup stale sessions (>24h)
```

**持久化格式**: JSON 文件，路径 `~/.hydraforge/sessions/<session_id>.json`

```json
{
  "session_id": "chat-20260726-001",
  "created_at": "2026-07-26T12:00:00Z",
  "last_active": "2026-07-26T14:30:00Z",
  "budget": { "total": 1000, "used": 342 },
  "history": [...],
  "provider_mode": "deepseek"
}
```

### T1: Budget 告警

当前问题：budget 超限后 LLM 调用被静默拒绝，无用户提示。

修复：
1. `BudgetController` 检查后通过 IInteractionBus emit `budget.exceeded` 事件
2. `ChatSession` 订阅该事件，在 TUI 中显示 `[⚠ Budget 已达上限 ($1000/1000)]`
3. Mock 模式下通过 `test_e2e_mock.cpp` 验证告警输出

### T2: Schema 校验

```
输入 .agent.md
  → MarkdownParser::validate_schema(content)
    ├─ 必填字段检查: name/version/agent_loop
    ├─ 节点类型校验: call_tool / llm_generate / condition / fork / join
    ├─ 依赖完整性: 所有 call_tool 的 tool_name 必须在 ToolRegistry 中存在
    └─ 循环检测: DFS 检测 DAG 是否有环
  → 失败返回 SchemaValidationError{line, column, message}
  → 成功返回 ParsedGraph
```

**新增文件**: `src/modules/parser/schema_validator.h/cpp`
- `SchemaValidator::validate(const std::string& markdown_content) → ValidationResult`
- 与 MarkdownParser 解耦，可独立测试

## Risks / Trade-offs

| 风险 | 缓解 |
|------|------|
| Schema 校验误报（拒绝合法 DSL） | `--skip-schema-validation` flag 允许绕过 |
| Session 文件写入磁盘IO阻塞 | 异步写入（std::async）+ 非关键路径 |