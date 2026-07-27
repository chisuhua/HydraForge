## T1: Session 持久化 + Budget 告警 (3h)

### Session 持久化（复用 session/persist 链路）

- [ ] 1.1 验证 `session/persist` 工具写入路径 — 确认 `persist_dir/<session_id>.json` 实际创建
- [ ] 1.2 `~` 路径展开 — `std::filesystem::path` + HOME env / getpwuid fallback
- [ ] 1.3 新增 `--session <id>` CLI flag（`examples/pdk_chat_demo/main.cpp`）
- [ ] 1.4 启动恢复 `ChatSession::load_from_disk(session_id)` — 反序列化 + 版本校验
- [ ] 1.5 损坏 JSON 降级 — `nlohmann::json::parse` catch → 打印 `[session/load] invalid JSON: <path>` + 空 session
- [ ] 1.6 原子写入 — 临时文件 + `std::filesystem::rename`
- [ ] 1.7 provider_mode reconcile — 存档 deepseek / 本次 --mock → 恢复 history + 用本次 provider
- [ ] 1.8 列出可用 sessions（`persist_dir` 下 *.json 文件）
- [ ] 1.9 Stale cleanup（>24h 未活跃）— 启动时执行，删除失败不阻塞
- [ ] 1.10 目录自动创建 — `std::filesystem::create_directories`，权限 `0700`

### Budget 告警（example 侧轮询，零核心改动）

- [ ] 2.1 `ChatSession::chat()` 每轮后轮询 `engine->get_budget_controller().exceeded()`
- [ ] 2.2 超限时构造 `budget.checked` 事件 payload → `bus.emit("budget.checked", payload)`
- [ ] 2.3 payload 字段: session_id, limit, used, unit, reason（见 design.md）
- [ ] 2.4 `EventHandler` 已有订阅（`event_handler.cpp:74` + `config.json:112`）→ 验证告警渲染
- [ ] 2.5 超限时 `chat()` 返回 success=false（当前 turn 失败但不崩溃）

### 线程安全

- [ ] 3.1 Bus 回调仅置 `std::atomic<bool>` / 线程安全队列，不直接触 TUI
- [ ] 3.2 主循环检查 atomic flag → 渲染告警 → 重置
- [ ] 3.3 shutdown 前显式检查 flag + 最后一次 render（防止 dispatch 未完成）

### 测试（T1）

- [ ] 4.1 `TEST_CASE("session persistence: save and restore across processes")` — 临时目录 + 跨进程
- [ ] 4.2 `TEST_CASE("session persistence: corrupted JSON degrades gracefully")` — 损坏文件降级
- [ ] 4.3 `TEST_CASE("session persistence: stale cleanup removes old files")` — >24h 清理
- [ ] 4.4 `TEST_CASE("budget alert: exceeded triggers budget.checked event")` — 超限 emit
- [ ] 4.5 `TEST_CASE("budget alert: exactly at limit does not alert")` — 恰好不告警
- [ ] 4.6 `TEST_CASE("budget alert: bus callback does not touch TUI directly")` — 线程安全验证

> T1 测试用 MockBus（同步 emit）+ 临时目录 fixture，不依赖真实 `~/.hydraforge`

---

## T2: DSL Schema 校验 (5h)

### 核心实现（example 侧，不修改 src/modules/parser/）

- [ ] 5.1 新建 `examples/pdk_chat_demo/dsl_validator.h` — `DslValidator` 类声明
- [ ] 5.2 新建 `examples/pdk_chat_demo/dsl_validator.cpp` — 校验逻辑
  - 必填字段: `name`, `version`, `agent_loop`
  - 节点类型白名单: `start`, `end`, `call_tool`, `llm_generate`, `condition`, `fork`, `join`, `assign`, `resource`
  - 工具依赖完整性: `call_tool` 节点的 `tool_name` 在 ToolRegistry 中注册
  - **不包含** DAG 循环检测（scheduler 执行时已有）
- [ ] 5.3 `pdk_chat_demo/main.cpp` 入口集成 — `parse_config()` → 加载 .agent.md → `DslValidator::validate()` → 失败退出(1)
- [ ] 5.4 注释引用 ADR-0058 并声明非重叠（本校验 = DSL 图结构 / ADR-0058 = tool input/output）
- [ ] 5.5 错误格式：`ValidationError{type, node_path, message}`，返回所有错误（非 fail-fast）

### 测试（T2）

- [ ] 6.1 新建 `tests/test_dsl_validation.cpp`（≥5 场景）
- [ ] 6.2 `TEST_CASE("valid minimal DSL passes validation")` — 合法 fixture 通过
- [ ] 6.3 `TEST_CASE("missing required field agent_loop → rejected")` — 缺必填字段
- [ ] 6.4 `TEST_CASE("unknown node type 'foobar' → rejected")` — 非法节点类型
- [ ] 6.5 `TEST_CASE("call_tool references unregistered tool → rejected")` — 工具依赖缺失
- [ ] 6.6 `TEST_CASE("malformed JSON in Nodes section → rejected")` — 非法 JSON
- [ ] 6.7 `TEST_CASE("missing ## Nodes section → rejected")` — 缺 Nodes 节

### 回归验证

- [ ] 7.1 `ctest -R pdk_chat` 全绿（不含新增 test_dsl_validation.cpp 的 pre-existing 失败）
- [ ] 7.2 `./pdk_chat_demo --mock` 正常启动（不受 DslValidator 影响）
- [ ] 7.3 现有 `examples/pdk_chat_demo/` 的 .agent.md 文件全部通过 DslValidator

---

## 估时汇总

| 模块 | 估时 |
|------|:---:|
| T1 Session 持久化 | 1.5h |
| T1 Budget 告警 | 1.0h |
| T1 线程安全 | 0.5h |
| T2 DslValidator 实现 | 2.0h |
| T2 测试 (5+6 scenarios) | 2.0h |
| T2 集成 + 回归 | 1.0h |
| **合计** | **8h** |

## 已完成条件

- [ ] `ctest -R pdk_chat` 全绿
- [ ] `./pdk_chat_demo --mock --session test_001` 跨进程 session 恢复正确
- [ ] 超限时 TUI 显示 `[⚠ Budget exceeded]`（mock 模式下可验证）
- [ ] 非法 `.agent.md` 被 `DslValidator` 拒绝 + 明确错误信息
- [ ] 所有新测试使用临时目录 fixture，不依赖真实 `~/.hydraforge`
- [ ] 零核心代码改动（`src/modules/` / `include/agenticdsl/` 下无变更）
