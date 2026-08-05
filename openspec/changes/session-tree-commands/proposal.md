# session-tree-commands

## Why

- ADR-0033 三层 Session 执行模型已 ship，session-manager-jsonl (v1+v2) 提供完整树状存储与 fork / switch_branch / append_to_branch / build_context API。
- pi-agent 会话树导航（§三 P0.1/P0.3）的核心闭环：存储（session-manager-jsonl）→ 命令层（adr-0070 DECLARE_COMMAND）→ 用户交互（本提案）。
- adr-0070 已 ship ICommandRegistry + DECLARE_COMMAND + `/compact` 委托治理路径示范，本提案扩展 `/tree` `/fork` `/clone` 三个纯 UI / 治理命令。

## What Changes

**In Scope**:

- `/tree` 会话树 TUI 渲染（branch 缩进树 + 当前 leaf 高亮，从 SessionManager 只读构建）
- `/fork` slash 命令：从当前节点 fork 新 branch 并切换（调用 SessionManager::fork）
- `/clone` slash 命令：从指定 branch 节点克隆整个会话（深拷贝 + 新 session_id）
- 树节点导航（用户输入 `/tree <id>` 切换当前 leaf 指针至指定节点）
- 全部命令经 DECLARE_COMMAND 注册，main.cpp 零 hardcode
- **Out of Scope**:
- `--fork <id>` / `--name` 启动 CLI flag（→ session-tree-cli-flags，Wave 2）
- SessionManager 存储实现（已 ship）
- 树合并/变基（无需求）
- Web UI

### 关键场景

- GIVEN 含 3 个 branch 的会话，WHEN 用户输入 `/tree`，THEN 渲染缩进树（当前 leaf 高亮），数据来自 SessionManager 内存索引（O(1) 查询）。
- GIVEN 会话任意节点（当前 leaf 或历史节点），WHEN `/fork [node_id]`，THEN 从指定节点 fork 新 branch 并切换到新 branch；新 branch JSONL 持久化可恢复。
- GIVEN 当前会话，WHEN `/clone [branch_id]`，THEN 深拷贝整个 branch 至新 session_id，原 session 不受影响。
- GIVEN 树中某节点，WHEN `/tree <node_id>`，THEN 切换当前 leaf 指针至该节点，后续 LLM 调用从该节点上下文重建。
- GIVEN 输入 `/fork`（无参数），WHEN 当前 leaf 节点，THEN 从该节点 fork 并自动切换。

**Out of Scope**:

- (TBD)

## Capabilities

- MUST slash 命令经 DECLARE_COMMAND/CommandRegistry（adr-0070）注册，禁止 main.cpp hardcode。
- MUST 树渲染只读 SessionManager（渲染与存储解耦，SessionManager 不感知 TUI）。
- MUST `/fork` `/clone` 调用经 ToolCoordinator 治理路径（layer check + ApprovalHandler，参考 adr-0070 `/compact` 模式）。
- SHOULD TUI 输出宽度自适应（窄终端降级为列表，避免折行）。
- SHOULD `/tree` 命令无破坏性副作用（只读查询）。
- MUST NOT 修改 SessionManager 公开 API 签名（v1 24 cases 已 ship 验证契约稳定）。

## Impact

- MUST slash 命令经 DECLARE_COMMAND/CommandRegistry（adr-0070）注册，禁止 main.cpp hardcode。
- MUST 树渲染只读 SessionManager（渲染与存储解耦，SessionManager 不感知 TUI）。
- MUST `/fork` `/clone` 调用经 ToolCoordinator 治理路径（layer check + ApprovalHandler，参考 adr-0070 `/compact` 模式）。
- SHOULD TUI 输出宽度自适应（窄终端降级为列表，避免折行）。
- SHOULD `/tree` 命令无破坏性副作用（只读查询）。
- MUST NOT 修改 SessionManager 公开 API 签名（v1 24 cases 已 ship 验证契约稳定）。

## Acceptance

- `/tree` `/fork` `/clone` `/tree <id>` 四条命令 E2E 测试通过（mock 模式 + 重启恢复）。
- `grep -n '"/tree\|"/fork\|"/clone' examples/pdk_chat_demo/main.cpp` 无 hardcode 分支。
- 命令经 ToolCoordinator 治理路径（test 含 layer check 验证）。
- TUI 窄终端降级测试（终端宽度 40 列 vs 120 列）。
- ctest 全量零回归（除 pre-existing `test_cost_tracking_decorator`）。
- 公开 API 签名零变化（grep `SessionManager::fork` 签名一致）。

