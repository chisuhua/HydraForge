# jsonl-session-storage Specification

## Purpose

定义 `SessionManager` 的 JSONL 树状会话存储契约，包括 `open/fork/branch/compact` API、叶子到根上下文重建、旧线性 JSON 迁移，以及 `session.persisted` 生命周期事件发射（ADR-0068 附录 A 当前唯一已注册的会话主题），确保 ADR-0033 三层会话模型具备崩溃安全的持久化层。`session.before.*` 系列主题因尚未在 ADR-0068 附录 A 注册，本 spec 不涉及。

## ADDED Requirements

### Requirement: jsonl-append-only-format

`SessionManager` MUST 以 JSONL append-only 格式持久化会话，每条记录独占一行并以换行符结尾，禁止全量重写整个文件。

#### Scenario: 新消息追加写
- **GIVEN** 已存在 JSONL 会话文件
- **WHEN** 调用 `append_to_branch(branch_id, message)`
- **THEN** 仅向文件末尾追加一行 JSON 记录
- **AND** 不读取、不修改、不覆盖已存在的历史记录

#### Scenario: 崩溃后只丢失未完成记录
- **GIVEN** 追加过程中进程崩溃
- **WHEN** 重新加载会话文件
- **THEN** 已完整写入的换行结尾记录全部可读
- **AND** 最后一行若缺少换行符则丢弃，不破坏前面记录

### Requirement: open-creates-or-loads-session

`SessionManager::open` MUST 在 JSONL 文件存在时加载已有会话，不存在时创建新会话并写入根节点记录。

#### Scenario: 创建新会话
- **WHEN** 调用 `open("session_001")` 且文件不存在
- **THEN** 创建 JSONL 文件
- **AND** 写入根节点记录（root node）与默认 branch "main"
- **AND** 返回的 session handle 指向根节点

#### Scenario: 加载已有会话
- **WHEN** 调用 `open("session_001")` 且 JSONL 文件已存在
- **THEN** 逐行解析 JSONL 重建内存节点索引
- **AND** 返回的 session handle 指向最新节点

### Requirement: fork-produces-isolated-branch

`SessionManager::fork(node_id, branch_name)` MUST 在指定节点处产生新 branch，JSONL 追加 branch 元数据记录。

#### Scenario: fork 创建新 branch
- **GIVEN** 会话中存在 `node_id="n3"`
- **WHEN** 调用 `fork("n3", "feature-x")`
- **THEN** 生成唯一 `branch_id`
- **AND** 写入 branch 记录，`forked_from_node="n3"`
- **AND** 新 branch 的初始叶子为 "n3"

### Requirement: branch-isolation

两个 branch 分别追加的消息 MUST 互不可见，`build_context_entries` 各自从叶子到正确根路径。

#### Scenario: branch 消息隔离
- **GIVEN** branch "main" 已追加消息 M1，branch "feature-x" 已追加消息 M2
- **WHEN** 调用 `build_context_entries` 分别获取两个 branch 的叶子
- **THEN** "main" 的上下文包含 M1 不包含 M2
- **AND** "feature-x" 的上下文包含 M2 不包含 M1

### Requirement: leaf-to-root-context-rebuild

`SessionManager::build_context_entries(leaf_node_id)` MUST 从叶子节点沿 parent 指针反向遍历至根节点，返回从根到叶的上下文数组。

#### Scenario: 线性链重建
- **GIVEN** JSONL 包含 root -> n1 -> n2 -> n3（parent 指针链）
- **WHEN** 调用 `build_context_entries("n3")`
- **THEN** 返回 [root, n1, n2, n3]

#### Scenario: fork 后分支重建
- **GIVEN** root -> n1 -> n2，branch "feature-x" 从 n2 fork 后追加 n3
- **WHEN** 调用 `build_context_entries("n3")`
- **THEN** 返回 [root, n1, n2, n3]

### Requirement: legacy-json-migration

`SessionManager` MUST 提供旧线性 JSON 迁移工具，将旧格式转为等价的 JSONL 树，且上下文重建结果与旧格式等价。

#### Scenario: 旧格式 messages 数组迁移
- **GIVEN** 旧格式文件包含 `{"messages": [{"role":"user","content":"hi"}, {"role":"assistant","content":"hello"}]}`
- **WHEN** 迁移工具执行
- **THEN** 生成 JSONL：root, n1(user), n2(assistant)，parent 指针 n2->n1->root
- **AND** 迁移后 `build_context_entries` 结果与旧格式 `messages` 数组顺序一致

#### Scenario: 迁移后保留备份
- **GIVEN** 旧格式文件路径为 `legacy.json`
- **WHEN** 迁移完成
- **THEN** 原文件保留为 `legacy.json.backup`
- **AND** JSONL 文件路径与原文件一致（扩展名替换为 `.jsonl`）

### Requirement: session-persisted-emission

`SessionManager` MUST 在 `flush_append` 成功后发射 `session.persisted` 事件，字段遵循 ADR-0068 附录 A（`session_id`, `node_id`, `branch_id`, `timestamp`）。

#### Scenario: persisted 在 flush 成功后发射
- **WHEN** 任意节点成功追加到 JSONL 并返回前
- **THEN** 发射 `session.persisted` 事件
- **AND** payload 包含 `session_id`、`node_id`、`branch_id`、`timestamp` 字段

#### Scenario: persisted 失败时不发射
- **WHEN** `flush_append` 因 I/O 错误失败
- **THEN** `session.persisted` 事件不得发射
- **AND** 错误传播给调用方

### Requirement: no-unsubscribed-topics-emitted

`SessionManager` MUST NOT 发射未在 ADR-0068 附录 A 注册的会话生命周期主题（含 `session.before.switch` / `session.before.fork` / `session.before.compact`），遵循 ADR-0068 §决策 2 强制规定。

#### Scenario: 未注册主题不被发射
- **WHEN** 调用 `fork` / `switch_branch` / `compact`
- **THEN** 不发射 `session.before.fork` / `session.before.switch` / `session.before.compact` 等未注册主题
- **AND** 仅在订阅端就位且附录 A 已注册的主题方可发射（本 spec 仅 `session.persisted`）