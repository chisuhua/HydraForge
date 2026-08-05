# session-tree-commands Specification

## Purpose

定义 `pdk_chat_demo` CLI 中三个 slash 命令 `/tree` `/fork` `/clone` 的契约，包括命令注册、数据访问、ToolCoordinator 治理路径、TUI 渲染与持久化恢复行为。本 spec 在 `session-manager-jsonl` v1+v2 已 ship 提供完整 JSONL 树状存储 + `fork / switch_branch / append_to_branch / build_context` API、`adr-0070-declare-command` 已 ship 提供 `ICommandRegistry` + `DECLARE_COMMAND` 宏的前提下，定义上层 UI/治理命令契约，禁止修改 `SessionManager` 公开 API 签名。

## ADDED Requirements

### Requirement: command-registration-via-declare-command

`/tree` `/fork` `/clone` 三个 slash 命令 MUST 通过 `DECLARE_COMMAND` 宏在 `examples/pdk_chat_demo/commands/` 下注册，由 `CommandRegistry::route()` 单一入口派发。`pdk_chat_demo/main.cpp` 中 MUST 零 hardcode 命令分支（`grep -nE '"\/(tree|fork|clone)' examples/pdk_chat_demo/main.cpp` 返回 0 行）。

#### Scenario: 命令注册后 /help 列出新命令
- **WHEN** 启动 `pdk_chat_demo` 二进制
- **AND** 输入 `/help`
- **THEN** 输出包含 `/tree` `/fork` `/clone` 三行命令说明

#### Scenario: main.cpp 零 hardcode
- **WHEN** 运行 `grep -nE '"\/(tree|fork|clone).*input\|input.*"\/(tree|fork|clone)' examples/pdk_chat_demo/main.cpp`
- **THEN** 退出码 1（无匹配行）

#### Scenario: 命令派发路由至正确 handler
- **WHEN** 用户输入 `/tree`
- **THEN** `CommandRegistry::route("/tree", args)` 返回 `tree_command` handler 的执行结果
- **AND** 不触发 LLM 调用

### Requirement: tree-render-reads-session-only

`/tree` 命令的数据访问 MUST 仅通过 `SessionManager` 内存索引，不触发 JSONL 文件 IO。SessionManager MUST 暴露 `list_branches() const` / `list_all_nodes() const` / `get_branch_leaf_node(branch_id)` / `get_node_by_short_id(short_id)` 四个只读方法。

#### Scenario: /tree 渲染多 branch session
- **GIVEN** 当前 session 含 3 个 branch（main, feature-x, fix-y），各自 leaf 节点分别为 n10, n15, n8
- **WHEN** 用户输入 `/tree`
- **THEN** 输出缩进树，每 branch 显示 branch_id + leaf node_id + 创建时间
- **AND** 当前 leaf（如 n10）前缀 `*` 高亮
- **AND** 渲染过程零文件 IO（mock 文件系统的 open/read 调用次数 = 0）

#### Scenario: /tree 空 session 仅显示 main
- **GIVEN** 新创建的 session 仅含 main branch 与根节点
- **WHEN** 用户输入 `/tree`
- **THEN** 输出 `* main (n0, created 2026-08-06T...)`

#### Scenario: 短前缀歧义报错
- **GIVEN** session 存在节点 n1abc 与 n1abd
- **WHEN** 用户输入 `/tree n1a`
- **THEN** 输出错误：`Ambiguous short id 'n1a': matches n1abc, n1abd`

### Requirement: tree-render-width-adaptive

`/tree` 命令 MUST 根据终端宽度自适应渲染模式：宽度 ≥ 60 列使用 ANSI 缩进树，宽度 < 60 列降级为列表模式（每行 `<branch_id>  <node_count>  <created_at>`）。

#### Scenario: 宽终端输出 ANSI tree
- **GIVEN** 终端宽度 120 列
- **WHEN** 用户输入 `/tree` 在 3 branch session 上
- **THEN** 输出含 `├──` `└──` `│` ANSI 字符
- **AND** 当前 leaf 行使用 `\033[1;32m` 绿色高亮

#### Scenario: 窄终端降级为列表
- **GIVEN** 终端宽度 40 列
- **WHEN** 用户输入 `/tree`
- **THEN** 输出无 ANSI 字符
- **AND** 每行格式：`main (5 msgs, 2026-08-06T...) *` 当前 leaf 末尾 `*` 标识

#### Scenario: 极端窄终端 (10 列)
- **GIVEN** 终端宽度 10 列
- **WHEN** 用户输入 `/tree`
- **THEN** 输出仍可读（每行不折行，branch_id 截断为前 8 字符）

### Requirement: tree-leaf-switch-no-side-effects

`/tree <node_id>` 命令 MUST 仅切换 `SessionState::current_leaf_id_` 内存指针，不触发 LLM 调用、不修改 JSONL 文件、不影响其他 session 状态。

#### Scenario: /tree <id> 切换 leaf 指针
- **GIVEN** 当前 leaf 指向 main 分支的 n5
- **WHEN** 用户输入 `/tree n1`（短前缀匹配唯一 node）
- **THEN** SessionState::current_leaf_id_ 更新为 `n1` 全 ID
- **AND** SessionManager JSONL 文件 mtime 零变化
- **AND** 输出：`Switched to <id>` 含切换目标信息

#### Scenario: 切换后 LLM 调用从新 leaf 重建
- **GIVEN** 当前 leaf 已从 main/n5 切换至 feature-x/n3
- **WHEN** 下一轮 LLM 调用触发
- **THEN** `SessionManager::build_context_entries("n3")` 被调用
- **AND** 返回的 context 数组仅含 root → n1 → n2 → n3 节点（不含 main 分支 n4/n5）

### Requirement: fork-via-tool-coordinator

`/fork [node_id]` slash 命令 MUST 经 `ToolCoordinator::call_tool("session/fork", ...)` 治理路径执行，禁止直接调用 `SessionManager::fork`。命令体 MUST 经 layer check（Cognitive / Thinking layer 拒绝）+ ApprovalHandler（用户确认）。

#### Scenario: /fork 无参数 fork 当前 leaf
- **GIVEN** 当前 leaf 节点 n5
- **WHEN** 用户输入 `/fork`
- **THEN** `ToolCoordinator::route("session/fork", {node_id: "n5"})` 被调用
- **AND** 成功后 auto-switch 至新 branch leaf
- **AND** 输出：`Forked to branch <branch_id> (auto-switched)` 绿色高亮

#### Scenario: /fork 指定 node
- **GIVEN** session 存在节点 n3（非当前 leaf）
- **WHEN** 用户输入 `/fork n3`
- **THEN** `session/fork` 工具调用 args `{node_id: "n3"}`
- **AND** 新 branch 从 n3 fork，新 branch_id 唯一

#### Scenario: Cognitive layer 拒绝调用
- **GIVEN** 当前 layer = Cognitive
- **WHEN** 任意 fork 命令体尝试调用 ToolCoordinator
- **THEN** ToolCoordinator 返回错误：`Layer Cognitive not permitted for session/fork`
- **AND** SessionState 零变化

#### Scenario: 持久化恢复
- **GIVEN** /fork 已成功创建 branch
- **WHEN** 退出 demo 后重新启动 `SessionManager::open(same_session_id)`
- **THEN** 新 branch 仍存在，JSONL 中可见 branch 记录

### Requirement: clone-deep-copy-isolation

`/clone [branch_id]` slash 命令 MUST 经 `ToolCoordinator::call_tool("session/clone", ...)` 治理路径执行。clone 操作 MUST 深拷贝指定 branch 至新 session_id，原 session MUST 零影响（写入次数 = 0）。

#### Scenario: /clone 默认克隆当前 branch
- **GIVEN** 当前 leaf 所在 branch 为 feature-x
- **WHEN** 用户输入 `/clone`
- **THEN** `session/clone` 工具调用 args `{branch_id: "feature-x"}`
- **AND** 返回新 session_id（hex `sess_<16hex>`）
- **AND** 输出：`Cloned to session sess_<id> (use --session <id> to switch)`
- **AND** 不自动切换（避免覆盖当前 context）

#### Scenario: /clone 指定 branch_id
- **GIVEN** session 存在 branch fix-y
- **WHEN** 用户输入 `/clone fix-y`
- **THEN** clone fix-y 至新 session，原 session 零写入（diff 验证 mtime 零变化 + inode 不变）

#### Scenario: clone 后新 session 可独立加载
- **GIVEN** /clone 已返回新 session_id sess_<new>
- **WHEN** 启动 `pdk_chat_demo --session sess_<new>`
- **THEN** `SessionManager::open("sess_<new>")` 成功
- **AND** 新 session 含克隆 branch 的全部节点与消息

### Requirement: tool-layer-governance

`session/fork` 与 `session/clone` 两个 ToolCoordinator 注册的工具 MUST 设置 `ToolMetadata.allowed_layers = {Workflow}`，Cognitive 与 Thinking layer 的调用 MUST 被拒绝。

#### Scenario: ToolRegistry 注册时校验 layer
- **WHEN** `register_session_fork_tool(tool_registry)` 执行
- **THEN** ToolMetadata 包含 `allowed_layers: [Workflow]`
- **AND** 重复注册抛 ToolAlreadyRegistered（防御性）

#### Scenario: tool coordinator 调用时 layer 检查
- **GIVEN** 当前 layer = Thinking
- **WHEN** ToolCoordinator::call_tool("session/fork", args) 被调用
- **THEN** 返回 `ToolResult.ok=false` + error_code = `PermissionDenied`
- **AND** 不触发 SessionManager 任何方法

#### Scenario: Workflow layer 正常调用
- **GIVEN** 当前 layer = Workflow
- **WHEN** ToolCoordinator::call_tool("session/fork", args)
- **THEN** SessionManager::fork 被调用并成功
- **AND** ToolResult.ok=true + data.branch_id 已返回
