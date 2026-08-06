# session-tree-cli-flags Specification

## Purpose

定义 `pdk_chat_demo` 的两个 session-tree 启动 CLI flag：`--fork <node_id>` 用于加载会话并从指定节点创建新 branch，`--name <session_name>` 用于为新建 session 设置并持久化名称。本规范依赖已 ship 的 SessionManager JSONL tree storage 与 fork API，以及先行 ship 的 `cli-args-cxxopts` 声明式 flag registry。它不修改 slash command 或通用 CLI parser 契约。

## ADDED Requirements

### Requirement: cli-flag-fork-node

`pdk_chat_demo` MUST 通过 `cli-args-cxxopts` 声明式 registry 注册 `--fork <node_id>`。当该 flag 存在时，程序 MUST 在进入 chat loop 前加载目标 session，并调用 `SessionManager::fork(node_id, branch_name)` 创建并切换到新 branch。不得通过手写 argv 循环实现该 flag。

#### Scenario: 从指定节点启动新 branch

- **GIVEN** 已存在 session `sess_abc`，其中包含节点 `node_42`
- **WHEN** 用户启动 `pdk_chat_demo --session sess_abc --fork node_42 --mock`
- **THEN** 程序先成功加载 `sess_abc`
- **AND** 程序调用 `SessionManager::fork("node_42", ...)`
- **AND** 当前 branch 切换到新 branch
- **AND** 程序随后进入正常 chat loop

#### Scenario: fork flag 出现在自动生成的帮助中

- **WHEN** 用户执行 `pdk_chat_demo --help`
- **THEN** 输出包含 `--fork <node_id>`
- **AND** 输出说明该 flag 从指定 session 节点 fork 新 branch

### Requirement: cli-flag-name-session

`pdk_chat_demo` MUST 通过 `cli-args-cxxopts` 声明式 registry 注册 `--name <session_name>`。当未指定已有 session 时，该值 MUST 用于新 session 的 metadata，并在创建后持久化。`--name` MUST NOT 静默重命名由 `--session <id>` 加载的已有 session。

#### Scenario: 新 session 使用指定名称

- **WHEN** 用户执行 `pdk_chat_demo --name my-debug-session --mock` 并退出
- **THEN** 创建的 session metadata 包含名称 `my-debug-session`
- **AND** 重新打开该 session 后名称仍为 `my-debug-session`
- **AND** 程序进入正常 chat loop

#### Scenario: name flag 出现在自动生成的帮助中

- **WHEN** 用户执行 `pdk_chat_demo --help`
- **THEN** 输出包含 `--name <session_name>`
- **AND** 输出说明该值只应用于新建 session

### Requirement: cli-flag-fork-with-name

当 `--session <id>`、`--fork <node_id>` 和 `--name <session_name>` 同时出现时，程序 MUST 按声明的启动顺序先加载 session，再从 node fork，再应用明确规定的 name scope。实现 MUST 保持 operation 顺序可测试，并不得在 chat loop 中延迟 fork。

#### Scenario: session and fork are applied before chat

- **GIVEN** `sess_abc` 包含 `node_42`
- **WHEN** 用户执行 `pdk_chat_demo --session sess_abc --fork node_42 --name child-session --mock`
- **THEN** session load 在 fork 前完成
- **AND** fork 在 chat loop 前完成
- **AND** 新 branch 的启动元数据按照 name scope 持久化
- **AND** 任一 startup operation 失败时不进入 chat loop

#### Scenario: existing session name is not silently overwritten

- **GIVEN** `sess_abc` 已有名称 `original-session`
- **WHEN** 用户执行 `pdk_chat_demo --session sess_abc --name child-session --mock`
- **THEN** 程序不得静默把已有 session 名称改为 `child-session`
- **AND** 程序输出明确的 scope 错误或采用文档化的只读行为
- **AND** 原名称仍为 `original-session`

### Requirement: fork-error-nonexistent-node

当 `--fork` 指向不存在、为空或格式非法的 node id 时，程序 MUST 在进入 chat loop 前失败，返回非零 exit code，并输出包含 flag、目标值和 `--help` 指引的明确错误。程序 MUST NOT 静默忽略错误、创建未指定的 fallback session 或写入错误 branch。

#### Scenario: nonexistent node fails startup

- **GIVEN** session `sess_abc` 不包含 `missing_node`
- **WHEN** 用户执行 `pdk_chat_demo --session sess_abc --fork missing_node --mock`
- **THEN** 程序返回非零 exit code
- **AND** stderr 包含 `--fork` 与 `missing_node`
- **AND** stderr 指向 `--help`
- **AND** chat loop 未启动且没有新 branch 被持久化

#### Scenario: nonexistent session fails before fork

- **WHEN** 用户执行 `pdk_chat_demo --session missing_session --fork node_42 --mock`
- **THEN** 程序返回非零 exit code
- **AND** 错误信息指出 session 不存在并包含 `--help`
- **AND** `SessionManager::fork` 不被调用
- **AND** chat loop 未启动

#### Scenario: missing fork value is rejected by the declarative parser

- **WHEN** 用户执行 `pdk_chat_demo --fork --mock`
- **THEN** `cli-args-cxxopts` parser 报告 `--fork` 缺少 value
- **AND** 程序返回非零 exit code
- **AND** 输出包含 `--help`
- **AND** SessionManager 不执行 load 或 fork 操作
