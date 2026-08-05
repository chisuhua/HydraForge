## 1. 数据访问层 - SessionManager 只读 API 抽取与扩展

- [ ] 1.1 在 `src/core/session_manager.h` 添加 `list_all_nodes() const` 返回 `std::vector<SessionNode>`（O(N)）
- [ ] 1.2 添加 `list_branches() const` 返回 `std::vector<BranchMeta>`（按 created_at 排序）
- [ ] 1.3 添加 `get_node_by_short_id(const std::string& short_id)` 短前缀匹配，歧义返回 std::optional + 错误列表
- [ ] 1.4 添加 `get_branch_leaf_node(const std::string& branch_id)` 返回 BranchMeta + 最新节点
- [ ] 1.5 在 `pdk/session_agent/SessionStore` 对应方法（如缺失，v1 SessionManager 已 ship 则 0 改动需求）
- [ ] 1.6 验证 API header 编译：`cmake --build build --target agenticdsl_core`
- [ ] 1.7 提交：`git commit -m "feat(session): expose read-only list_branches + get_branch_leaf for slash commands"`

## 2. 命令注册 - DECLARE_COMMAND 三个 slash 命令

- [ ] 2.1 创建 `examples/pdk_chat_demo/commands/tree_command.{h,cpp}`：`DEFINE_COMMAND(tree)` 渲染当前 session 树
- [ ] 2.2 创建 `examples/pdk_chat_demo/commands/fork_command.{h,cpp}`：`DEFINE_COMMAND(fork)` 调用 `ToolCoordinator::call_tool("session/fork", args)`
- [ ] 2.3 创建 `examples/pdk_chat_demo/commands/clone_command.{h,cpp}`：`DEFINE_COMMAND(clone)` 调用 `ToolCoordinator::call_tool("session/clone", args)`
- [ ] 2.4 在 `examples/pdk_chat_demo/commands/CMakeLists.txt` 注册 3 个新源文件
- [ ] 2.5 在 `examples/pdk_chat_demo/main.cpp::register_default_commands()` 中 wire 三个命令到 `CommandRegistry`
- [ ] 2.6 验证命令注册 E2E：`./pdk_chat_demo` 启动后输入 `/help` 列出 `/tree` `/fork` `/clone`
- [ ] 2.7 提交：`git commit -m "feat(chat-demo): register DECLARE_COMMAND for /tree /fork /clone"`

## 3. ToolCoordinator - 注册 session/fork + session/clone 工具

- [ ] 3.1 在 `examples/pdk_chat_demo/tools/session_fork.{h,cpp}` 实现 `register_session_fork_tool(tool_registry)`，调用 `SessionManager::fork(node_id, branch_name)`
- [ ] 3.2 在 `examples/pdk_chat_demo/tools/session_clone.{h,cpp}` 实现 `register_session_clone_tool(tool_registry)`，调用 `SessionManager` 深拷贝逻辑
- [ ] 3.3 在 `examples/pdk_chat_demo/main.cpp::setup()` 中调用两个 register function
- [ ] 3.4 设置工具 `ToolMetadata`：category=Workflow + approval_policy=agent（用户确认即可）
- [ ] 3.5 验证 layer check: Cognitive / Thinking layer 拒绝调用（test 含 mock layer）
- [ ] 3.6 提交：`git commit -m "feat(coordinator): register session/fork + session/clone tools with Workflow layer"`

## 4. /tree 渲染 - ANSI tree + 窄终端降级

- [ ] 4.1 创建 `examples/pdk_chat_demo/tui/tree_renderer.{h,cpp}`：接收 `std::vector<BranchMeta>` + `std::string current_leaf_id`
- [ ] 4.2 实现 `render_tree(terminal_width)`：宽终端（≥60 列）输出 ANSI `├── └── │` 缩进树，当前 leaf 用 `*` 标识
- [ ] 4.3 实现窄终端 fallback：每行 `<branch_id>  <node_count>  <created_at>` 列表模式
- [ ] 4.4 实现 ANSI escape：`ioctl(TIOCGWINSZ)` 取得 `ws_col`，STDOUT_FILENO 通道
- [ ] 4.5 当前 leaf 高亮 ANSI: `\033[1;32m` + reset
- [ ] 4.6 验证渲染输出快照测试（catch2 golden file mode，宽 + 窄 + 空 session 三场景）
- [ ] 4.7 提交：`git commit -m "feat(tree-renderer): ANSI tree with terminal-width adaptive fallback"`

## 5. /tree <id> 参数解析 + leaf 切换

- [ ] 5.1 `tree_command.cpp::execute(args)` 解析 args：`arg.empty()` → 渲染；非空 → 尝试切 leaf
- [ ] 5.2 短前缀匹配调用 `SessionManager::get_node_by_short_id(arg)`，歧义返回错误 + 候选列表
- [ ] 5.3 成功匹配后切换 `SessionState::current_leaf_id_`（不调 LLM，纯内存指针切换）
- [ ] 5.4 切换后下一轮 LLM 调用从 `build_context_entries(new_leaf)` 重建（已经在 adr-0033 v1 24 cases 验证）
- [ ] 5.5 单元测试：参数解析 3 场景（空/单匹配/歧义）+ leaf 切换副作用为零
- [ ] 5.6 提交：`git commit -m "feat(tree-cmd): add /tree <id> arg parsing with short-id prefix match"`

## 6. /fork 实现 + 持久化恢复

- [ ] 6.1 `fork_command.cpp::execute(args)`：解析 `[node_id]`（可选），空 → 用当前 leaf
- [ ] 6.2 构造 `ToolCallContext` + `json args{node_id, branch_name = "fork-<timestamp>"}`
- [ ] 6.3 调用 `ToolCoordinator::call_tool("session/fork", args)` → 返回 `ToolResult` 含新 `branch_id`
- [ ] 6.4 成功后切换 `SessionState::current_leaf_id_` 到新 branch 最新节点（auto-switch 决策 3）
- [ ] 6.5 输出结果：`Forked to branch <branch_id> (auto-switched)` + ANSI 绿色高亮
- [ ] 6.6 错误处理：`ToolResult.ok==false` → 输出错误 message + 不切换 leaf
- [ ] 6.7 集成测试：mock LLM + 真实 SessionManager，fork 后退出重启 `SessionManager::open` 验证 branch 持久化
- [ ] 6.8 提交：`git commit -m "feat(fork-cmd): implement /fork with auto-switch and persistence"`

## 7. /clone 实现 + 深拷贝隔离

- [ ] 7.1 `clone_command.cpp::execute(args)`：解析 `[branch_id]`（默认当前 leaf 所在 branch）
- [ ] 7.2 调 `ToolCoordinator::call_tool("session/clone", args{branch_id})` → 返回 `new_session_id`
- [ ] 7.3 输出：`Cloned to session <new_session_id> (use --session <id> to switch)`
- [ ] 7.4 不自动切换到新 session（避免意外覆盖当前 context）；提供 hint 引导用户重启
- [ ] 7.5 单元测试：mock tool 返回新 session_id，验证输出包含 `/` 分隔的 session ID
- [ ] 7.6 验证原 session 零修改（写入次数 = 0，diff 验证）
- [ ] 7.7 提交：`git commit -m "feat(clone-cmd): implement /clone with deep copy isolation"`

## 8. main.cpp 零 hardcode 验证 + 输入循环回归

- [ ] 8.1 修复 `examples/pdk_chat_demo/main.cpp` 中任何残留 `if (input.starts_with("/tree")` 等 hardcode 分支
- [ ] 8.2 验证 grep：`grep -nE '"\/(tree|fork|clone)' examples/pdk_chat_demo/main.cpp` 应返回 0 行
- [ ] 8.3 验证 grep：`grep -n '"/compact\|"/help\|"/exit' examples/pdk_chat_demo/main.cpp` 仍为 0（保持）
- [ ] 8.4 回归测试：`./pdk_chat_demo` 启动 → 输入 `/help` → 列出 6 个命令 (`/help /exit /compact /tree /fork /clone`) → 退出
- [ ] 8.5 集成测试：mock LLM + 真实 SessionManager，连续执行 `/tree` → `/fork` → `/tree` → `/clone` 4 命令验证状态正确
- [ ] 8.6 提交：`git commit -m "refactor(chat-demo): remove residual slash hardcode branches"`

## 9. 测试矩阵 + ctest 全量

- [ ] 9.1 单元测试：`tests/test_session_tree_commands.cpp` 新增（命令派发 3 + 参数解析 5 + tree 渲染 4 共 12 cases）
- [ ] 9.2 集成测试：`tests/test_pdk_chat_demo_session_tree.cpp` 新增（mock LLM + 真实 SessionManager，fork/clone 全场景）
- [ ] 9.3 TUI 测试：catch2 golden file 模式覆盖宽终端 + 窄终端 + 极端短终端（10 列） 三种宽度
- [ ] 9.4 ToolCoordinator 测试：mock ToolRegistry，验证 session/fork + session/clone 调用时 layer check 工作
- [ ] 9.5 回归测试：`ctest --output-on-failure` 全量
- [ ] 9.6 公开 API 签名对比：`git diff main -- src/core/session_manager.h` 应仅新增方法，零修改既有
- [ ] 9.7 提交：`git commit -m "test(tree-commands): add unit + integration + golden file coverage"`

## 10. ADR 状态同步 + docs sync

- [ ] 10.1 ADR-0070 状态保持 ✅ Approved（后续增量 ship 注记追加）
- [ ] 10.2 ADR-0033 状态保持 ✅ Approved（v1 24 cases 已 ship 验证）
- [ ] 10.3 `docs/active-status.md` §一 Phase 6a 行追加本 change ship 注记
- [ ] 10.4 `openspec validate session-tree-commands --strict` exit 0
- [ ] 10.5 `tools/adr_lint.py` exit 0 + `tools/docs_drift_audit.py` 0 DRIFT
- [ ] 10.6 commit + merge archive
