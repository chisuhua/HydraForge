# pdk_chat_demo v1 收尾设计

## Context

pdk_chat_demo 是 HydraForge Phase 5-6 的旗舰 Demo，展示 Agent-as-Plugin + SkillInterpreter + DeepSeek LLM 端到端能力。当前 v1 基础功能已运行，但存在质量负债。

**现有基建**（本设计依赖的关键现状）：
- `pdk/session_agent` 已注册 `session/persist` 工具
- `ChatSession` 已 emit `session.persist_request` 事件（`chat_session.cpp:267`）
- `SessionConfig.persist_dir` 已从 config 解析（默认 `~/.hydraforge/sessions/`）
- `event_handler.cpp:74` 已订阅 `budget.checked` topic
- `config.json:112` 已声明 `budget.checked` activation_event
- InMemoryBus 是**异步 dispatch 线程**模型（Sprint 12/C2 起）

## Design

### T1: Budget 告警修复

**根因**：mock 模式下 Loop Agent 不经 `BudgetController` 扣费，budget 超限被静默忽略；且无人 emit `budget.checked` 事件。

**方案（example 侧，零核心改动）**：

```
ChatSession::chat()
  ├─ 执行一轮对话（loop_agent.run()）
  ├─ 轮询 engine->get_budget_controller()
  │   ├─ exceeded() → 构造 budget.checked BusEvent → bus.emit("budget.checked", payload)
  │   └─ 未超限 → 不 emit
  └─ EventHandler 已有订阅 → 渲染 TUI 告警
```

**Budget checked 事件 payload**：

```json
{
  "session_id": "sess_...",
  "limit": 1.0,
  "used": 1.2,
  "unit": "llm_calls",
  "reason": "cost_limit"
}
```

**边界行为**：
- 恰好达到 limit → 不告警（`exceeded()` 返回 false）
- 超限后当前 turn 返回 success=false
- 告警每轮重复出现（直到 budget 不再超限或程序退出）
- 不保存超限后的 user message

**与核心 BudgetController 的关系**：
- **不修改** `BudgetController` / `IBudgetController` / `budget_controller.h`
- 仅**读取** `exceeded()` + `get_total_cost_usd()` 两个公开 const 方法
- 在 mock 模式下 mock provider 返回的 cost 需通过 `MockLLMProvider` 的 cost_usd 字段反映到 BudgetController

### T1: Session 持久化

**复用现有链路**（不重写存储层）：

```
ChatSession 已有流程:
  chat() → loop_agent.run() → on success → emit session.persist_request
  ↑ 本变更补全: 确保 session/persist 工具实际写入磁盘
```

**补全项**：

1. **验证 `session/persist` 工具链路**：确认 payload 正确传递并写入 `persist_dir/<session_id>.json`
2. **启动恢复**：新增 `--session <id>` CLI flag
   - 若 `--session sess_001` → 从 `persist_dir/sess_001.json` 恢复 `UserSession`
   - 若 JSON 损坏 → 打印 `[session/load] invalid JSON: <path>` + 降级为空 session
   - 若文件不存在 → 创建新 session（等同无 `--session` 行为）
3. **`~` 路径展开**：`persist_dir` 中的 `~` 由 `std::filesystem::path` + HOME 环境变量展开
4. **原子写入**：临时文件 + `std::filesystem::rename`（防止写入中断留半截文件）
5. **provider_mode reconcile**：存档 deepseek、本次 `--mock` → 恢复 history 但用本次 provider 执行

**Session JSON schema**：

```json
{
  "schema_version": 1,
  "session_id": "sess_20260726_001",
  "created_at": "2026-07-26T12:00:00Z",
  "updated_at": "2026-07-26T14:30:00Z",
  "provider_mode": "deepseek",
  "budget": { "total": 1000, "used": 342 },
  "history": [
    { "role": "user", "content": "...", "timestamp": "..." },
    { "role": "assistant", "content": "...", "timestamp": "..." }
  ]
}
```

**边界行为**：
- 未知 `schema_version` → 拒绝加载 + 明确错误
- 损坏 JSON → `nlohmann::json::parse` 异常 catch → 降级为空 session（非空 catch，打印警告）
- 保存失败 → emit `session.persisted` 且 ok=false，本轮返回失败
- 目录不存在 → `std::filesystem::create_directories` 自动创建，权限 `0700`
- Stale cleanup（>24h）→ 启动时执行，删除失败不阻塞启动

### T2: DSL Schema 校验

**落位**：`examples/pdk_chat_demo/` 内（**不修改 `src/modules/parser/`**）

```
pdk_chat_demo 入口 (main.cpp):
  parse_config → load .agent.md → DslValidator::validate(content)
    ├─ 必填字段检查: name, version, agent_loop
    ├─ 节点类型校验: call_tool, llm_generate, condition, fork, join
    ├─ 工具依赖完整性: 所有 call_tool 的 tool_name 在 ToolRegistry 中存在
    └─ 失败 → 打印错误 + 退出(1)
```

**新增文件**：`examples/pdk_chat_demo/dsl_validator.h/cpp`
- `DslValidator::validate(const std::string& markdown_content) → ValidationResult`
- 与 MarkdownParser 解耦，独立测试，仅 pdk_chat_demo 使用

**明确不包含**（defer 到后续 change）：
- DAG 循环检测（`topo_scheduler.cpp:588` 执行时已有等价检查，解析时重复检测性价比低）
- MarkdownParser 集成（需改核心代码，独立 OpenSpec change 处理）
- `--skip-schema-validation` flag（当前默认严格校验即可，escape hatch 待核心 Parser 集成时一并添加）

**与非重叠 ADR-0058 的关系**：
ADR-0058 校验 **tool input/output** schema（`call_tool` 执行路径）。本校验针对 **.agent.md DSL 图**结构（解析前）。两者非重叠、可独立演进。在 `DslValidator` 注释中引用 ADR-0058 并声明差异。

**合法 DSL fixture 示例**：

```markdown
# test-chat-agent
- **name**: test-chat-agent
- **version**: 1.0.0
- **agent_loop**: react

## Nodes
```json
[
  { "id": "start", "type": "start", "next": "llm_1" },
  { "id": "llm_1", "type": "llm_generate", "next": "tool_1" },
  { "id": "tool_1", "type": "call_tool", "tool_name": "echo", "next": "end" },
  { "id": "end", "type": "end" }
]
```
```

**非法场景及预期错误**：
| 场景 | 错误类型 | 错误信息示例 |
|------|---------|-------------|
| 缺 `agent_loop` | MISSING_REQUIRED_FIELD | `missing required field: agent_loop` |
| 未知节点类型 `foobar` | INVALID_NODE_TYPE | `node[1]: unknown type 'foobar'` |
| `call_tool` 引用不存在工具 `missing_tool` | MISSING_TOOL_DEPENDENCY | `node[2]: tool 'missing_tool' not registered` |
| 非法 JSON 结构 | PARSE_ERROR | `invalid JSON at line N: <detail>` |
| 缺 Nodes 节 | MISSING_SECTION | `missing '## Nodes' section` |

## 线程模型

**关键约束**：InMemoryBus 自 Sprint 12/C2 起使用**后台 dispatch_thread 异步分发**事件。

```
主线程 (TUI)               dispatch 线程 (InMemoryBus)
    │                           │
    ├─ chat()                   │
    │   ├─ loop_agent.run()     │
    │   ├─ 轮询 BudgetController│
    │   ├─ bus.emit(...) ──────→│─ 回调执行
    │   │                       │   ├─ EventHandler::on_event()
    │   │                       │   │   └─ ⚠ 不得直接操作 TUI
    │   │                       │   └─ 仅置 std::atomic flag
    │   └─ 检查 flag ──────────→│
    │       └─ 渲染告警 (安全)   │
    └─ render()                 │
```

**规则**：
- dispatch 线程回调：**仅读写 `std::atomic<bool>` / 线程安全队列，不得触 TUI**
- 主线程渲染循环：每帧检查 atomic flag → 显示告警 → 重置 flag
- 测试用 MockBus（同步 emit）可简化，但 e2e 测试必须覆盖异步路径

## Risks / Trade-offs

| 风险 | 缓解 |
|------|------|
| DslValidator 与 MarkdownParser 的 DSL 格式不一致 | DslValidator 复制 MarkdownParser 的 JSON schema 提取逻辑（只读参考，独立实现） |
| `~` 展开在不同环境下行为不一致 | 优先 `HOME` 环境变量，fallback `getpwuid` |
| Session JSON schema 未来版本不兼容 | `schema_version` 字段 + 未知版本拒绝加载 |
| Bus 异步回调遗漏告警（进程退出前未 dispatch） | 主线程在 shutdown 前显式检查 flag + 最后一次 render |
| Mock LLM 不经过 BudgetController 扣费 | MockProvider 通过 `cost_usd` 字段写入 BudgetController（MockLLMProvider 已有此路径） |