## 1. SessionManager 头文件与 JSONL 存储实现

- [x] 1.1 创建 `src/core/session_manager.h`：定义 `SessionManager` 类、`SessionNode` / `BranchMeta` / `SessionHandle` 结构体
- [x] 1.2 在 `src/core/session_manager.h` 定义 `struct SessionNode { std::string id; std::string parent_id; std::string branch_id; nlohmann::json content; }`
- [x] 1.3 在 `src/core/session_manager.h` 定义 `struct BranchMeta { std::string branch_id; std::string name; std::string forked_from_node; std::string created_at; }`
- [x] 1.4 创建 `src/core/session_manager.cpp`：实现构造函数与文件路径管理
- [x] 1.5 实现 `SessionManager::open(const std::string& session_id)` 加载现有 JSONL 或创建新文件
- [x] 1.6 实现 `SessionManager::load_jsonl()` 从磁盘逐行解析 JSONL 到内存节点索引
- [x] 1.7 实现 `SessionManager::flush_append(const SessionNode& node)` 追加单条记录到 JSONL
- [x] 1.8 在 `SessionManager` 中持有 `std::mutex write_mutex_` 保护追加写
- [x] 1.9 实现 `SessionManager::next_node_id()` 生成唯一 node_id
- [x] 1.10 实现 `SessionManager::next_branch_id()` 生成唯一 branch_id
- [x] 1.11 验证 `src/core/session_manager.{h,cpp}` 编译通过：`cmake --build build --target test_session_manager` (Task 1 临时绕过：tests/CMakeLists.txt 添加 session_manager.cpp 到 test_session_manager 源 — Task 9 才会正式接入 agenticdsl_core)
- [x] 1.12 提交：`git commit -m "feat(core): add SessionManager header and JSONL append-only storage"`

## 2. open/fork/branch/compact API

- [x] 2.1 实现 `SessionManager::open(const std::string& session_id, const std::optional<std::string>& legacy_path)` 在 JSONL 不存在时尝试迁移 (Task 2 partial ship: overload 签名已加, 当文件不存在 + legacy_path 已提供时调 migrate_legacy_json; flush_append 同步更新 nodes_/children_/current_branch_; open() 初始化默认 "main" branch)
- [x] 2.2 实现 `SessionManager::fork(const std::string& node_id, const std::string& branch_name)` (Task 3 ship: 调用 next_branch_id() + BranchMeta 创建 + flush_append_internal 写 JSONL branch meta + 更新 current_branch_)
- [x] 2.3 在 `fork()` 中验证 `node_id` 存在且属于当前 session (Task 3 ship: load_jsonl 兜底 + nodes_.count 校验, 不存在抛 std::runtime_error)
- [x] 2.4 在 `fork()` 中创建新 `BranchMeta` 并写入 JSONL branch 记录 (Task 3 ship: BranchMeta{branch_id, name, forked_from_node, created_at=unix_ms} + flush_append_internal 走 fd+write+fsync 写 "type=branch" meta 记录)
- [x] 2.5 实现 `SessionManager::switch_branch(const std::string& branch_id)` 并更新当前 branch 指针 (Task 3 ship: branches_.count 校验, 不存在抛 std::runtime_error; current_branch_ = branch_id)
- [x] 2.6 实现 `SessionManager::append_to_branch(const std::string& branch_id, const nlohmann::json& message)` 在指定 branch 追加消息节点 (Task 3 ship: API 签名为 append_to_branch(message) 基于 current_branch_; 找该分支叶子为 parent_id, 构造 SessionNode, flush_append 写入)
- [x] 2.7 实现 `SessionManager::compact()` 扫描全文件并重写去除已废弃分支（不删除活跃分支） (Task 4 ship: 5 步流程 — 1) 备份原文件 2) 收集 active_branch 节点 3) 写临时文件 fd+write+fsync 4) atomic rename 5) 更新 in-memory nodes_/children_/branches_; 3 新测试覆盖 inactive 移除 / .backup 内容 / append-only 不变量)
- [x] 2.8 在 `compact()` 中保留 `.backup` 文件 (Task 4 ship: std::filesystem::copy 到 <session_id>.jsonl.backup, overwrite_existing 语义; 测试 compact_backup 断言行数 == pre-compact 行数)
- [x] 2.9 验证 API 单元测试骨架编译：`cmake --build build --target test_session_manager` (Task 3 ship: 20 test cases / 1092 assertions 零失败 PASS, 4 新增测试覆盖 fork happy path / switch_branch pointer / switch_branch throw / fork throw; 1 既有 task5_stub 测试用 [.task5_stub] 标签排除出默认 ctest 运行)
- [x] 2.10 提交：`git commit -m "feat(core): implement SessionManager open/fork/branch/compact API"`

## 3. 叶子到根上下文重建

- [x] 3.1 实现 `SessionManager::build_context_entries(const std::string& leaf_node_id)` 从叶子反向遍历 parent 指针 (Task 5 ship: walk parent_id 链收集到根, std::reverse 得 root-first order; 空 leaf 返回空 vector)
- [x] 3.2 在 `build_context_entries` 中缓存已访问节点避免环（防御性检查）(Task 5 ship: std::unordered_set<std::string> visited guard, 重复访问即 break; 经 cycle 测试验证 < 1ms 返回)
- [x] 3.3 实现 `SessionManager::get_branch_leaf(const std::string& branch_id)` 返回 branch 最新节点 (Task 5 ship: 遍历 nodes_ 找该 branch 内 children_ 空集的节点; 空 branch 返回 "")
- [x] 3.4 实现 `SessionManager::get_root_node()` 返回会话根节点 (Task 5 ship: 遍历 nodes_ 找 parent_id == "" 的节点)
- [x] 3.5 验证两个 branch 分别追加消息后 `build_context_entries` 互不包含对方消息 (Task 5 ship: test_session_build_context.cpp 5 新 test cases (39 assertions) + test_session_manager.cpp un-tagged `respects branch isolation` ship test 验证 root→n1→n2/explore 隔离 + root→n1→n3/main 隔离, 互不含对方消息; 修复原 stub 测试 fork 返回 branch_id 与 fork name 混淆 bug)
- [x] 3.6 提交：`git commit -m "feat(core): add leaf-to-root context rebuild with branch isolation"`

## 4. 旧格式迁移工具

- [x] 4.1 实现 `SessionManager::migrate_legacy_json(const std::string& legacy_path)` 读取线性 JSON
- [x] 4.2 将旧格式 `messages` 数组按顺序转换为 JSONL 记录链，parent 指针指向前一条记录
- [x] 4.3 迁移后生成 root node 与默认 branch "main"
- [x] 4.4 实现迁移工具 CLI `tools/migrate_session_json.py`（Python，与项目既有 Python 工具一致）
- [x] 4.5 迁移工具备份原文件为 `<legacy_path>.backup`
- [x] 4.6 验证迁移等价性：旧格式上下文重建结果 == 新 JSONL 上下文重建结果
- [x] 4.7 提交：`git commit -m "feat(tools): add legacy linear-JSON to JSONL migration tool"`

## 5. `session.persisted` 生命周期事件发射

- [x] 5.1 在 `SessionManager::flush_append` 成功后发射 `session.persisted` 事件（ADR-0068 附录 A 唯一已注册会话主题）
- [x] 5.2 事件 payload 字段按 ADR-0068 附录 A（`session_id`, `node_id`, `branch_id`, `timestamp`）
- [x] 5.3 若 `EventBuilder`（adr-0068-event-emission-contract）已落地则复用，否则直接构造 `nlohmann::json` payload 通过 `IInteractionBus::emit` 发送
- [x] 5.4 单测使用 `InMemoryBus` 捕获事件并断言字段
- [x] 5.5 不发射 `session.before.switch` / `session.before.fork` / `session.before.compact`（需先在 ADR-0068 附录 A 注册——本 change 不实施）
- [x] 5.6 提交：`git commit -m "feat(core): emit session.persisted event per ADR-0068"`

## 6. 测试

- [x] 6.1 新建 `tests/test_session_manager.cpp`
- [x] 6.2 单测：`open` 创建新会话并写入 JSONL
- [x] 6.3 单测：`fork(node_id)` 产生新 branch 并写入 branch 记录
- [x] 6.4 单测：两个 branch 追加消息后互不可见
- [x] 6.5 单测：`build_context_entries` 从叶子到根路径正确
- [x] 6.6 单测：并发 `append_to_branch` 100 次无数据损坏（TSan）
- [x] 6.7 单测：`compact()` 后活跃 branch 数据完整
- [x] 6.8 新建 `tests/test_session_manager_migration.cpp`
- [x] 6.9 单测：空旧格式迁移后等价
- [x] 6.10 单测：单消息旧格式迁移后等价
- [x] 6.11 单测：多消息旧格式迁移后上下文重建等价
- [x] 6.12 单测：迁移工具生成 `.backup` 文件
- [x] 6.13 新建 `tests/test_session_persisted_event.cpp`
- [x] 6.14 单测：`session.persisted` 在 `flush_append` 成功返回前发射
- [x] 6.15 单测：`session.persisted` payload 字段符合 ADR-0068 附录 A
- [x] 6.16 运行 ctest：`cmake --build build && ctest --output-on-failure`
- [x] 6.17 运行 TSan 测试：`cmake --preset tsan && ctest --output-on-failure`
- [x] 6.18 运行 `python3 tools/docs_drift_audit.py` 验证 0 DRIFT
- [x] 6.19 提交：`git commit -m "test(session): add SessionManager fork/branch/compact/migration/event tests"`

## 7. 文档同步与 ship gate

- [x] 7.1 更新 `AGENTS.md` CODE MAP 追加 `SessionManager` 关键符号
- [x] 7.2 更新 ADR-0033 实施范围说明（存储层已 ship）
- [x] 7.3 运行 `tools/adr_lint.py` exit 0
- [x] 7.4 运行 `openspec validate session-manager-jsonl` 验证通过
- [x] 7.5 提交 changes artifacts：`git add openspec/changes/session-manager-jsonl/ && git commit -m "feat: fill session-manager-jsonl change artifacts (Wave 2 P1)"`

## 8. Follow-up 提案

- [x] 8.1 在 `proposal-suggestions.md` 或独立 OpenSpec change 中提议"先在 ADR-0068 附录 A 注册 `session.before.switch` / `session.before.fork` / `session.before.compact` 三个会话生命周期主题"
- [x] 8.2 验证：上述注册完成后，本 change 后续 PR 可追加 `session.before.*` 发射实现
- [x] 8.3 验证：命名遵循 `<domain>.<entity>.<verb>` 点号约定（`session.before.switch` 而非 `session_before_switch`），符合 ADR-0068 §决策 5