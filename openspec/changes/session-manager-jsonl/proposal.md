## Why

ADR-0033 三层 Session 执行模型已 ship,但存储层仍是 `chat_session.cpp` 中的线性 JSON 单文件原子写,无 JSONL 树状持久化、无 `open/fork/branch/compact` API,成为 `/tree` `/fork` 借鉴路径（§三 P0.1）的核心阻塞。pi-agent `SessionManager`（`buildContextEntries` 从叶子到根遍历）为借鉴蓝本。

本 change 填补上述缺口,使会话存储支持树状分支、崩溃安全追加、旧格式无损迁移,并与 ADR-0068 生命周期事件集成。`session.persisted` 是当前 ADR-0068 附录 A 中唯一已注册的会话生命周期主题，本 change 实施其发射载体；`session.before.*` 系列主题仍为缺失项（见 ADR-0068 §决策 2 与 `layer-based-missing-capabilities-analysis.md`），需独立提案先行在附录 A 注册，本 change 不实施。

`pdk/session_agent/` 已具备独立的 `SessionStore` JSONL 实现（`pdk/session_agent/src/session_store.cpp`，含 `sess_<hex>` ID 生成、`~/.hydraforge/sessions/` 目录、`HYDRAFORGE_SESSION_DIR` 环境变量），与 `chat_session` 零引用（`grep -rn "chat_session" pdk/session_agent/` 返回 0），不存在复制代码需消除。本 change 不修改 `pdk/session_agent/`，仅新增独立 `SessionManager` core 组件。

## What Changes

- **新增** `src/core/session_manager.{h,cpp}`：JSONL 树状存储（每消息一条记录 + parent 指针 + branch 元数据）。
- **新增** `SessionManager::open` / `fork` / `branch` / `compact` API。
- **新增** 叶子到根上下文重建（`build_context_entries`），保证分支隔离。
- **新增** 旧线性 JSON 迁移工具，保留用户历史会话。
- **新增** `session.persisted` 生命周期事件发射（ADR-0068 附录 A 唯一已注册的会话主题）。
- **不修改** `pdk/session_agent/`（已具备独立 SessionStore，无复制代码需消除）。
- **不修改** `/tree` `/fork` TUI 渲染（归 `session-tree-tui` 提案）。
- **不修改** ContextCompactor LLM 摘要（归 `context-compactor` 提案）。
- **不修改** 分布式会话（无需求）。
- **不实施** `session.before.switch` / `session.before.fork` / `session.before.compact`（需先在 ADR-0068 附录 A 注册，本 change 提交独立 follow-up 提案）。

## Capabilities

### New Capabilities
- `jsonl-session-storage`：`SessionManager` + `open/fork/branch/compact` + JSONL 树状存储，支持每消息一条记录、parent 指针、branch 元数据、append-only 崩溃安全写，以及叶子到根上下文重建。
- `legacy-json-migration`：旧线性 JSON 会话文件迁移工具，输出等价的 JSONL 树并保留原文件备份。
- `session-persisted-emission`：在 `SessionManager::flush_append` 成功后按 ADR-0068 附录 A 字段发射 `session.persisted` 事件（session_id、node_id、branch_id、timestamp）。

### Modified Capabilities
- 无（`pdk/session_agent/` 已独立实现，本 change 不修改）。

## Impact

- **生产代码**:
  - `src/core/session_manager.h`（新，公共 API）
  - `src/core/session_manager.cpp`（新，JSONL 存储实现）
- **测试代码**:
  - `tests/test_session_manager.cpp`（新，fork/branch/compact + 并发追加）
  - `tests/test_session_manager_migration.cpp`（新，旧格式迁移等价性）
  - `tests/test_session_persisted_event.cpp`（新，`session.persisted` 发射）
- **API 兼容性**:
  - 旧线性 JSON 文件通过迁移工具保留，不自动破坏。
  - 新增 `SessionManager` API 不修改现有 `DSLEngine` 会话执行签名。
- **依赖**:
  - 无新外部依赖（复用 nlohmann_json 与现有文件 IO）。
- **文档**:
  - `AGENTS.md` CODE MAP 追加 `SessionManager` 关键符号。
  - ADR-0033 实施范围说明更新为存储层已 ship。
- **风险**:
  - 中 - 新增核心存储组件，需保证追加写崩溃安全与旧格式无损迁移。
  - 并发追加需单测覆盖 TSan 场景，避免数据竞争。