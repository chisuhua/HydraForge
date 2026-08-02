## 1. SessionManager 头文件与 JSONL 存储实现

- [ ] 1.1 创建 `src/core/session_manager.h`：定义 `SessionManager` 类、`SessionNode` / `BranchMeta` / `SessionHandle` 结构体
- [ ] 1.2 在 `src/core/session_manager.h` 定义 `struct SessionNode { std::string id; std::string parent_id; std::string branch_id; nlohmann::json content; }`
- [ ] 1.3 在 `src/core/session_manager.h` 定义 `struct BranchMeta { std::string branch_id; std::string name; std::string forked_from_node; std::string created_at; }`
- [ ] 1.4 创建 `src/core/session_manager.cpp`：实现构造函数与文件路径管理
- [ ] 1.5 实现 `SessionManager::open(const std::string& session_id)` 加载现有 JSONL 或创建新文件
- [ ] 1.6 实现 `SessionManager::load_jsonl()` 从磁盘逐行解析 JSONL 到内存节点索引
- [ ] 1.7 实现 `SessionManager::flush_append(const SessionNode& node)` 追加单条记录到 JSONL
- [ ] 1.8 在 `SessionManager` 中持有 `std::mutex write_mutex_` 保护追加写
- [ ] 1.9 实现 `SessionManager::next_node_id()` 生成唯一 node_id
- [ ] 1.10 实现 `SessionManager::next_branch_id()` 生成唯一 branch_id
- [ ] 1.11 验证 `src/core/session_manager.{h,cpp}` 编译通过：`cmake --build build --target agenticdsl_core`
- [ ] 1.12 提交：`git commit -m "feat(core): add SessionManager header and JSONL append-only storage"`

## 2. open/fork/branch/compact API

- [ ] 2.1 实现 `SessionManager::open(const std::string& session_id, const std::optional<std::string>& legacy_path)` 在 JSONL 不存在时尝试迁移
- [ ] 2.2 实现 `SessionManager::fork(const std::string& node_id, const std::string& branch_name)`
- [ ] 2.3 在 `fork()` 中验证 `node_id` 存在且属于当前 session
- [ ] 2.4 在 `fork()` 中创建新 `BranchMeta` 并写入 JSONL branch 记录
- [ ] 2.5 实现 `SessionManager::switch_branch(const std::string& branch_id)` 并更新当前 branch 指针
- [ ] 2.6 实现 `SessionManager::append_to_branch(const std::string& branch_id, const nlohmann::json& message)` 在指定 branch 追加消息节点
- [ ] 2.7 实现 `SessionManager::compact()` 扫描全文件并重写去除已废弃分支（不删除活跃分支）
- [ ] 2.8 在 `compact()` 中保留 `.backup` 文件
- [ ] 2.9 验证 API 单元测试骨架编译：`cmake --build build --target test_session_manager`
- [ ] 2.10 提交：`git commit -m "feat(core): implement SessionManager open/fork/branch/compact API"`

## 3. 叶子到根上下文重建

- [ ] 3.1 实现 `SessionManager::build_context_entries(const std::string& leaf_node_id)` 从叶子反向遍历 parent 指针
- [ ] 3.2 在 `build_context_entries` 中缓存已访问节点避免环（防御性检查）
- [ ] 3.3 实现 `SessionManager::get_branch_leaf(const std::string& branch_id)` 返回 branch 最新节点
- [ ] 3.4 实现 `SessionManager::get_root_node()` 返回会话根节点
- [ ] 3.5 验证两个 branch 分别追加消息后 `build_context_entries` 互不包含对方消息
- [ ] 3.6 提交：`git commit -m "feat(core): add leaf-to-root context rebuild with branch isolation"`

## 4. 旧格式迁移工具

- [ ] 4.1 实现 `SessionManager::migrate_legacy_json(const std::string& legacy_path)` 读取线性 JSON
- [ ] 4.2 将旧格式 `messages` 数组按顺序转换为 JSONL 记录链，parent 指针指向前一条记录
- [ ] 4.3 迁移后生成 root node 与默认 branch "main"
- [ ] 4.4 实现迁移工具 CLI `tools/migrate_session_json.py`（Python，与项目既有 Python 工具一致）
- [ ] 4.5 迁移工具备份原文件为 `<legacy_path>.backup`
- [ ] 4.6 验证迁移等价性：旧格式上下文重建结果 == 新 JSONL 上下文重建结果
- [ ] 4.7 提交：`git commit -m "feat(tools): add legacy linear-JSON to JSONL migration tool"`

## 5. `session.persisted` 生命周期事件发射

- [ ] 5.1 在 `SessionManager::flush_append` 成功后发射 `session.persisted` 事件（ADR-0068 附录 A 唯一已注册会话主题）
- [ ] 5.2 事件 payload 字段按 ADR-0068 附录 A（`session_id`, `node_id`, `branch_id`, `timestamp`）
- [ ] 5.3 若 `EventBuilder`（adr-0068-event-emission-contract）已落地则复用，否则直接构造 `nlohmann::json` payload 通过 `IInteractionBus::emit` 发送
- [ ] 5.4 单测使用 `InMemoryBus` 捕获事件并断言字段
- [ ] 5.5 不发射 `session.before.switch` / `session.before.fork` / `session.before.compact`（需先在 ADR-0068 附录 A 注册——本 change 不实施）
- [ ] 5.6 提交：`git commit -m "feat(core): emit session.persisted event per ADR-0068"`

## 6. 测试

- [ ] 6.1 新建 `tests/test_session_manager.cpp`
- [ ] 6.2 单测：`open` 创建新会话并写入 JSONL
- [ ] 6.3 单测：`fork(node_id)` 产生新 branch 并写入 branch 记录
- [ ] 6.4 单测：两个 branch 追加消息后互不可见
- [ ] 6.5 单测：`build_context_entries` 从叶子到根路径正确
- [ ] 6.6 单测：并发 `append_to_branch` 100 次无数据损坏（TSan）
- [ ] 6.7 单测：`compact()` 后活跃 branch 数据完整
- [ ] 6.8 新建 `tests/test_session_manager_migration.cpp`
- [ ] 6.9 单测：空旧格式迁移后等价
- [ ] 6.10 单测：单消息旧格式迁移后等价
- [ ] 6.11 单测：多消息旧格式迁移后上下文重建等价
- [ ] 6.12 单测：迁移工具生成 `.backup` 文件
- [ ] 6.13 新建 `tests/test_session_persisted_event.cpp`
- [ ] 6.14 单测：`session.persisted` 在 `flush_append` 成功返回前发射
- [ ] 6.15 单测：`session.persisted` payload 字段符合 ADR-0068 附录 A
- [ ] 6.16 运行 ctest：`cmake --build build && ctest --output-on-failure`
- [ ] 6.17 运行 TSan 测试：`cmake --preset tsan && ctest --output-on-failure`
- [ ] 6.18 运行 `python3 tools/docs_drift_audit.py` 验证 0 DRIFT
- [ ] 6.19 提交：`git commit -m "test(session): add SessionManager fork/branch/compact/migration/event tests"`

## 7. 文档同步与 ship gate

- [ ] 7.1 更新 `AGENTS.md` CODE MAP 追加 `SessionManager` 关键符号
- [ ] 7.2 更新 ADR-0033 实施范围说明（存储层已 ship）
- [ ] 7.3 运行 `tools/adr_lint.py` exit 0
- [ ] 7.4 运行 `openspec validate session-manager-jsonl` 验证通过
- [ ] 7.5 提交 changes artifacts：`git add openspec/changes/session-manager-jsonl/ && git commit -m "feat: fill session-manager-jsonl change artifacts (Wave 2 P1)"`

## 8. Follow-up 提案

- [ ] 8.1 在 `proposal-suggestions.md` 或独立 OpenSpec change 中提议"先在 ADR-0068 附录 A 注册 `session.before.switch` / `session.before.fork` / `session.before.compact` 三个会话生命周期主题"
- [ ] 8.2 验证：上述注册完成后，本 change 后续 PR 可追加 `session.before.*` 发射实现
- [ ] 8.3 验证：命名遵循 `<domain>.<entity>.<verb>` 点号约定（`session.before.switch` 而非 `session_before_switch`），符合 ADR-0068 §决策 5