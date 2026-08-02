## Context

ADR-0033 三层 Session 执行模型已 ship（`UserSession` / `TaskSession` / `SubtaskSession`），但存储层仍是 `chat_session.cpp` 中的线性 JSON 单文件原子写。该设计无法表达会话树的分支结构，也没有 `open/fork/branch/compact` 原语，阻塞了 `/tree` `/fork` TUI 借鉴路径（§三 P0.1）。pi-agent 的 `SessionManager` 通过 `buildContextEntries` 从叶子到根遍历重建上下文，是本 change 的参考实现。

ADR-0068 附录 A 当前已注册 `session.persisted` 一个会话生命周期主题（owner: ChatSession / session_agent）。订阅端已在 EventHandler 就位（`event_handler.cpp`），但生产路径零发射——属于"有订阅无发射"的幻影主题。`session.before.switch` / `session.before.fork` / `session.before.compact` 三个主题在 `layer-based-missing-capabilities-analysis.md:232` 列为 MISSING，按 ADR-0068 §决策 2 必须先 PR 修订附录 A 才能新增。本 change 仅实施 `session.persisted` 发射，三个 `session.before.*` 主题需独立提案先行注册。

`pdk/session_agent/` 已具备独立的 `SessionStore` JSONL 实现（`pdk/session_agent/src/session_store.cpp:1-54`，含 `sess_<hex>` ID 生成、`~/.hydraforge/sessions/` 目录、`HYDRAFORGE_SESSION_DIR` 环境变量），与 `chat_session` 零引用（`grep -rn "chat_session" pdk/session_agent/` 返回 0）。无需委托化重构。

## Goals / Non-Goals

**Goals:**
- 新建 `src/core/session_manager.{h,cpp}`，实现 JSONL 树状存储（每消息一条记录 + parent 指针 + branch 元数据）。
- 实现 `open/fork/branch/compact` API，支持会话树的创建、分支切换、分支追加与压缩回收。
- 实现叶子到根上下文重建，保证不同 branch 之间隔离。
- 提供旧线性 JSON 迁移工具，保证用户历史会话不丢失。
- 发射 `session.persisted` 事件（ADR-0068 附录 A 唯一已注册的会话主题，字段：`session_id`、`node_id`、`branch_id`、`timestamp`）。
- 单测覆盖 fork/branch/compact（含并发追加）、迁移等价性、`session.persisted` 发射。
- 维持 ctest 全量零回归与 `docs_drift_audit.py` 0 DRIFT。

**Non-Goals:**
- 不实施 `session.before.switch` / `session.before.fork` / `session.before.compact` 三个主题的发射（需独立提案先行在 ADR-0068 附录 A 注册；命名须用点号 `<domain>.<entity>.<verb>`，违反此命名将不被 ADR-0068 接受）。
- 不实现 `/tree` `/fork` TUI 渲染（归 `session-tree-tui` 提案）。
- 不实现 ContextCompactor LLM 摘要（归 `context-compactor` 提案）。
- 不实现分布式会话（当前无需求）。
- 不改写 ADR-0033 三层模型本身的接口契约。
- 不修改 `pdk/session_agent/`（已独立实现）。

## Decisions

### Decision 1: 存储格式采用 JSONL append-only

**Rationale**:
- 崩溃安全：追加写失败时最多丢失最后一条未完成记录，不会破坏已提交历史。
- 与线性 JSON 单文件重写相比，无需全量序列化整个会话树，分支追加成本低。
- JSONL 行式结构便于从叶子到根反向扫描（parent 指针）。

**Alternatives Considered**:
- **SQLite**：引入外部依赖与 SQL schema 管理，超出本 change 范围。
- **分文件目录树**：实现复杂，与现有文件存储习惯差异大，迁移成本高。

### Decision 2: 每条记录包含 parent 指针 + branch 元数据

**Rationale**:
- parent 指针直接支持从叶子到根反向遍历，与 pi-agent `buildContextEntries` 模式一致。
- branch 元数据（`branch_id` / `branch_name` / `fork_source_node` / `created_at`）支持 TUI 后续渲染，不依赖额外索引文件。
- 与线性 JSON 相比，每条记录自描述，便于追加和并发控制。

**Alternatives Considered**:
- **单独索引文件维护 parent/branch 关系**：多文件一致性风险，恢复复杂。
- **嵌套 JSON 对象表达树**：append-only 语义下难以增量更新，违背 Decision 1。

### Decision 3: 旧格式迁移工具随本提案交付

**Rationale**:
- 验收标准明确要求旧格式迁移等价性测试通过，禁止丢弃用户历史会话。
- 迁移工具一次性读取线性 JSON，按时间顺序生成 JSONL 记录链，parent 指针指向前一条记录。
- 迁移后保留原文件为 `.backup`，支持回滚与对比验证。

**Alternatives Considered**:
- **运行时自动检测并迁移**：增加 `SessionManager` 复杂度，且首次启动行为不可控。
- **要求用户手动删除旧文件**：违反"禁止丢弃用户历史会话"约束。

### Decision 4: 仅发射 ADR-0068 附录 A 已注册主题（`session.persisted`）

**Rationale**:
- ADR-0068 §决策 2 明确要求"新增/修改主题必须 PR 修订本附录"——任何未注册主题不得新增发射。
- 当前附录 A 仅 `session.persisted` 一个会话主题；`session.before.*` 三个主题列为 MISSING，需独立提案。
- `session.persisted` 在订阅端已注册（`event_handler.cpp`），发射侧补齐即可形成完整路径。

**Alternatives Considered**:
- **同时发射 `session.before.*`**：违反 ADR-0068 §决策 2；附录 A 未注册即发射将导致下游 EventBus 警告，且无法被设计中的订阅方识别。
- **延后整个本 change 等待附录 A 修订**：阻塞本 change 范围外的独立工作，违背 ADR-0068 决策的本意。

### Decision 5: 不修改 `pdk/session_agent/`

**Rationale**:
- 该 plugin 已有独立 JSONL `SessionStore` 实现，与 `chat_session` 无引用关系，不存在需消除的复制代码。
- `SessionManager` 作为 core 组件可被 PDK plugin 与 future TUI 复用，无需改造既有 plugin。
- 后续 `pdk/session_agent/` 是否委托 `SessionManager` 属独立演进，本 change 不强制。

**Alternatives Considered**:
- **强制 `pdk/session_agent/` 委托 `SessionManager`**：破坏既有独立实现的稳定性；无明确收益（已无复制代码）。
- **将 `SessionManager` 直接放入 `pdk/session_agent/`**：破坏 core 通用性，导致其它模块无法复用。

## Risks / Trade-offs

### Risk 1: JSONL 并发追加的完整性

**Mitigation**:
- 使用 `std::mutex` 保护写操作，保证行级原子追加。
- 每行以换行符结尾，崩溃后读取时丢弃不完整最后一行。
- TSan 单测覆盖并发 fork/append 路径。

### Risk 2: 旧格式迁移等价性

**Mitigation**:
- 迁移前后分别重建上下文并逐条对比。
- 保留 `.backup` 文件，支持对比验证与回滚。
- 单测覆盖空会话、单消息、多消息三种旧格式。

### Risk 3: `session.persisted` 与其他发射方重复

**Mitigation**:
- 本 change 由 `SessionManager` 发射；ADR-0068 附录 A 当前 owner 为 `ChatSession / session_agent`，未来如 ChatSession / pdk/session_agent 也发射，需 ADR-0068 明确单一所有者。
- 本 change 在附录 A 修订前仅一个 owner：`SessionManager`。

### Trade-off 1: 追加写 vs 查询效率

**Trade-off**: JSONL 追加写崩溃安全，但重建上下文需要反向扫描全文件，分支多时时间复杂度 O(N)。
**Decision**: 接受（当前 N 为单会话消息数，无分布式需求；`compact` 操作可周期性回收碎片）。

### Trade-off 2: branch 元数据冗余存储

**Trade-off**: 每条记录携带 branch 元数据增加存储，但避免单独索引文件。
**Decision**: 接受（单会话消息量级在 10^4 以内，冗余元数据可忽略）。