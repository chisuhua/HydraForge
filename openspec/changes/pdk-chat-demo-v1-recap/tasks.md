## T1: Session 持久化 + Budget 告警 (4h)

### Session 持久化

- [ ] 1.1 实现 `ChatSession::save_to_disk()` — JSON 序列化 + 原子写入
- [ ] 1.2 实现 `ChatSession::load_from_disk(session_id)` — 反序列化 + 校验
- [ ] 1.3 每轮对话后自动保存（`chat_loop()` 末尾）
- [ ] 1.4 启动时列出可用 sessions（`~/.hydraforge/sessions/`）
- [ ] 1.5 清理过期 sessions（>24h 未活跃）

### Budget 告警

- [ ] 2.1 `BudgetController` 添加 `get_remaining()` / `get_warning_threshold()` API
- [ ] 2.2 超出 budget 时通过 IInteractionBus emit `budget.exceeded` 事件
- [ ] 2.3 `ChatSession` 订阅 `budget.exceeded` → TUI 显示告警
- [ ] 2.4 修复静默失败：LLM 调用被拒绝时返回明确错误信息

### 测试

- [ ] 3.1 `test_session_persistence.cpp` — 保存/加载/过期清理 (3 cases)
- [ ] 3.2 `test_budget_alert.cpp` — 告警触发/显示/恢复 (3 cases)

---

## T2: Schema 校验 (4h)

### 核心实现

- [ ] 4.1 新建 `src/modules/parser/schema_validator.h` — SchemaValidator 类声明
- [ ] 4.2 新建 `src/modules/parser/schema_validator.cpp` — 校验逻辑实现
  - 必填字段: name, version, agent_loop
  - 节点类型: call_tool, llm_generate, condition, fork, join
  - 工具依赖完整性
  - DAG 循环检测 (DFS)
- [ ] 4.3 MarkdownParser 集成：`parse()` 前调用 `SchemaValidator::validate()`
- [ ] 4.4 `--skip-schema-validation` CLI flag（绕过校验的 escape hatch）

### 测试

- [ ] 5.1 `test_schema_validation.cpp` — 新增测试文件
  - 合法 DSL 通过校验
  - 缺必填字段 → 拒绝 + 错误信息
  - 工具依赖不存在 → 拒绝 + 提示缺失工具名
  - 循环依赖 → 拒绝 + 指出循环路径
  - 非法节点类型 → 拒绝

### 集成

- [ ] 6.1 `pdk_chat_demo` 集成 SchemaValidator → 非法配置早期报错
- [ ] 6.2 `ctest -R pdk_chat` 全绿确认