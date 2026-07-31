# session-tree-tui

**优先级**: P2 | **来源**: layer-based-missing-capabilities-analysis.md §八 L4-6（/tree /fork /clone TUI）
**阶段**: wave-3（依赖 session-manager-jsonl + chat-streaming-slash-tui） | **分类**: demo-chat-v2
**类型**: feature

## 架构依据
- ADR-0033 三层 Session 执行模型已 ship，session-manager-jsonl 提案落地后获得树状存储与 fork/branch API，本提案是用户侧出口。
- pi-agent 会话树导航（§三 P0.1/P0.3）的完整闭环：存储（session-manager-jsonl）→ 事件（L1-3）→ 命令（adr-0070）→ TUI（本提案）。
- `--fork <id>` / `--name` CLI flag 依赖 cli-args-cxxopts 的声明框架。

## 范围
- **In Scope**: `/tree` 会话树 TUI 渲染（branch 缩进树 + 当前节点高亮）；`/fork` / `/clone` slash 命令（DECLARE_COMMAND 注册）；`--fork <id>` / `--name` CLI flag；树导航（切换当前 leaf 指针）。
- **Out Scope**: SessionManager 存储实现（session-manager-jsonl）；树合并/变基（无需求）；Web UI。

## 关键场景
- GIVEN 含 3 个 branch 的会话，WHEN 用户输入 `/tree`，THEN 渲染缩进树（当前 leaf 高亮），数据来自 SessionManager。
- GIVEN 会话任意节点，WHEN `/fork`，THEN 从当前节点创建新 branch 并切换，JSONL 持久化可恢复。
- GIVEN 启动时 `--fork <id>`，WHEN 加载，THEN 从指定节点 fork 继续会话。

## 技术约束
- MUST slash 命令经 CommandRegistry（adr-0070），禁止 main.cpp hardcode。
- MUST 树渲染只读 SessionManager（渲染与存储解耦）。
- SHOULD TUI 输出宽度自适应（窄终端降级为列表）。

## 验收标准
- `/tree` `/fork` `/clone` E2E 测试通过（mock 模式，含重启恢复）。
- `--fork` / `--name` flag 生效；ctest 全量零回归。
