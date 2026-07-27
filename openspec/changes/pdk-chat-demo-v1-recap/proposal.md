## Why

Phase 6a roadmap 要求 `pdk_chat_demo` v1 收尾。当前状态：
- pdk_chat_demo 已具备基础 Chat 功能（Agent-as-Plugin + SkillInterpreter + DeepSeek LLM）
- 存在已知问题：Session 持久化未验证 / Budget 告警未修复 / 缺少 Schema 校验
- `ctest -R pdk_chat` 未全绿

## What Changes

### T1: Session 持久化验证 + Budget 告警修复 (3h)
- 验证并修通 `pdk/session_agent` 已有的 `session/persist` 工具链路（ChatSession 已 emit `session.persist_request` 事件）
- 补 Session restore-on-startup：新增 `--session <id>` CLI flag + 列出可用 sessions
- 修复 Budget 告警静默失败（根因：mock 模式下 Loop Agent 不经 BudgetController 扣费）：
  - **example 侧方案**：ChatSession 每轮后轮询 `engine->get_budget_controller().exceeded()`
  - 超限时 emit `budget.checked` 事件（接通已有 EventHandler 订阅 `config.json:112`）
  - **零核心代码改动**

### T2: DSL Schema 校验 (5h)
- 新增 example 侧 `.agent.md` 格式校验（拒绝错误 DSL，在 pdk_chat_demo 入口处调用）
- 校验项：必填字段缺失（name/version/agent_loop）/ 节点类型不合法 / 工具依赖完整性
- 明确不包含 DAG 循环检测（`topo_scheduler.cpp:588` 执行时已有等价检查）
- 拒绝后返回明确错误信息（含节点路径）
- 引用 ADR-0058（tool schema 校验）并声明非重叠——本校验针对 DSL 图结构，非 tool input/output
- 新增 test case：`test_dsl_validation.cpp`（≥5 场景）

## Capabilities

- `pdk-chat-demo-v1`: pdk_chat_demo v1 收尾（Session/Budget/Schema）

## Impact

- `examples/pdk_chat_demo/`：Bug 修复 + DSL 校验新增
- `examples/pdk_chat_demo/tests/`：新增 `test_dsl_validation.cpp`
- **不影响 HydraForge 核心代码**（所有改动限定在 `examples/pdk_chat_demo/` + `tests/`）

## Non-Goals

- 不新增 Chat 功能特性
- 不修改 SkillInterpreter 核心逻辑
- 不修改 `src/modules/parser/`（核心 Parser）
- 不修改 `BudgetController` / `IBudgetController`
- 不引入新的 LLM 后端
- 不在 `src/modules/` 下新增/修改文件

## ADR 引用

- ADR-0058（tool-schema-validation）：本变更的 DSL 校验与 ADR-0058 非重叠——前者校验 `.agent.md` 图结构，后者校验 tool input/output schema
- ADR-0033（session-hierarchy）：Session restore 需对齐 UserSession/TaskSession 的 deque 地址稳定性
