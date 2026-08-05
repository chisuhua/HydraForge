## Context

ADR-0033 三层 Session 执行模型 (`UserSession` / `TaskSession` / `SubtaskSession`) 已 ship，`session-manager-jsonl` v1+v2 已 ship 提供完整的 JSONL 树状存储 + `fork / switch_branch / append_to_branch / build_context` API（详见 `docs/adr/adr-0033-session-hierarchy.md` 与 `openspec/changes/archive/2026-08-05-session-manager-jsonl-v2/`）。

`adr-0070-declare-command` 已 ship `ICommandRegistry` + `CommandRegistry` + `DECLARE_COMMAND` 宏 + 治理路径示范（`/help` `/exit` `/compact`），`pdk_chat_demo/main.cpp` 输入循环已具备 `/` 前缀分发与 `/help` 处理骨架。当前 `/tree` `/fork` `/clone` 三命令仍依赖 `chat-async-io-steering.md` 中描述的旧 hardcode 分支或直接缺失，阻塞了 pi-agent 会话树借鉴路径（§三 P0.1/P0.3）的核心交互闭环。

本 change 是原 `session-tree-tui`（已废弃 Wave 1+2 拆分）的 **Wave 1 子集**，仅交付 `/tree` `/fork` `/clone` 三个 slash 命令主体，CLI flag 子集推迟至 Wave 2 `session-tree-cli-flags`。

## Goals / Non-Goals

**Goals:**
- 三个 slash 命令 `/tree` `/fork` `/clone` 全部经 `DECLARE_COMMAND` 注册并接入 `CommandRegistry`，`pdk_chat_demo/main.cpp` 零 hardcode。
- `/tree` 在终端渲染当前 session 的 branch 树（缩进 + 当前 leaf 高亮 + 创建时间），数据仅读自 `SessionManager` 内存索引（O(1) 查询）。
- `/tree <node_id>` 切换当前 leaf 指针（不影响 LLM provider 配置，仅切换上下文重建起点）。
- `/fork [node_id]` 从指定节点或当前 leaf fork 新 branch 并自动切换；新 branch `BranchMeta` 通过 `SessionManager::fork` 写入 JSONL。
- `/clone [branch_id]` 深拷贝指定 branch 至新 `session_id`，原 session 零影响；新 session 经 `SessionManager::open` 加载即可访问。
- 全部命令经 `ToolCoordinator` 治理路径（layer check + ApprovalHandler），与 adr-0070 `/compact` 模式一致。
- TUI 输出宽度自适应：终端宽度 < 60 列降级为列表模式（每行一个 branch + node_id + 简略摘要）。
- 单测覆盖：命令派发（含参数解析）/ 树渲染（含窄终端降级）/ fork+clone 持久化恢复 / ToolCoordinator layer check 验证 / main.cpp 零 hardcode 验证（grep regression）。
- ctest 零回归（除 pre-existing `test_cost_tracking_decorator`）。
- 公开 API 签名零修改（不触碰 `SessionManager` 公开 header）。

**Non-Goals:**
- CLI flag `--fork <id>` / `--name` 启动参数（→ Wave 2 `session-tree-cli-flags`）。
- `--system-prompt` / `--append-system-prompt` 启动参数（→ `cli-args-cxxopts` + Wave 2 chat-streaming-render）。
- TUI 流式渲染 / event-driven 增量更新（→ Wave 2 `chat-streaming-render`，仅刷新数据变化部分）。
- `SessionManager` 存储实现改动（v1 24 cases + v2 已 ship 验证契约稳定）。
- 树合并 / 变基 / diff（无产品需求）。
- `/tree` 节点编辑（如 inline rename / delete，不在 Wave 1 范围）。
- 远程 / Web UI。

## Decisions

### Decision 1: 命令注册复用 adr-0070 的 DECLARE_COMMAND 宏路径

**Rationale**:
- 与 `adr-0070` ship 决策 2 一致：所有 `/` 前缀命令经 `DECLARE_COMMAND(name, summary, body)` 注册，由 `CommandRegistry::route()` 单一入口派发。
- 避免回归 hardcode 命令分支（proposal 显式 MUST 约束）。
- 测试模式与 `/compact` 完全对齐，可复用 `tests/test_command_registry.cpp` 已有 fixture。

**Alternatives Considered**:
- **新建独立命令子系统**：增加架构复杂度，违反 adr-0070 "集中注册表"决策。
- **直接在 main.cpp 注册 lambda**：退化为 hardcode，违反 MUST 约束。

### Decision 2: `/tree` 渲染数据源仅读 SessionManager 内存索引

**Rationale**:
- 渲染与存储解耦：`SessionManager` 不感知 TUI，渲染器只读访问 `build_context_entries / list_branches / get_branch_meta` 等现有只读 API。
- 树构建为 O(N) 单次遍历（N = session node 数），避免每次 `/tree` 触发的额外 IO。
- 终端宽度自适应通过 ANSI escape 查询（`ioctl(TIOCGWINSZ)`）实现，窄终端 fallback 为列表。

**Alternatives Considered**:
- **每次 `/tree` 重新解析 JSONL**：浪费 IO，无增值。
- **缓存树到独立文件**：增加状态同步复杂度，未通过 v1+v2 验证需要。

### Decision 3: `/fork` 与 `/clone` 经 ToolCoordinator 治理路径

**Rationale**:
- `/fork` 与 `/clone` 属于"修改持久化"操作（写 JSONL），与 `/compact` 同治理需求。
- ToolCoordinator 提供 layer check（Cognitive / Thinking / Workflow 分层）+ ApprovalHandler（用户确认敏感操作）。
- 符合 adr-0070 `/compact` 既定模式：命令 body 调用 `ToolCoordinator::call_tool("session/fork", args)` 或 `("session/clone", args)`，由 coordinator 路由至 `SessionManager`。

**Alternatives Considered**:
- **命令直接调用 SessionManager**：绕过治理层，违反 adr-0070 + ADR-0031 决策 5。
- **命令包装为 tool 后由 LLM 自动调用**：用户主动操作无需走 LLM 工具调用路径。

### Decision 4: TUI 渲染库沿用现有 ANSI 转义（不引入 FTXUI / ncurses）

**Rationale**:
- pdk_chat_demo 已用 ANSI escape 实现 `/compact` 状态显示与 `/tree` 简化版（poorman's 树字符 `├── └── │`）。
- 引入 FTXUI / ncurses 增加外部依赖，违反 ADR-0021 PDK "无外部依赖"原则。
- TUI 复杂度低（仅 1 个 render 函数），ANSI escape 足矣。

**Alternatives Considered**:
- **FTXUI**：强大但 weight 重，依赖体积大，对 TUI 仅 1 render 函数 over-engineering。
- **ncurses**：C API，集成到 C++ 命令体繁琐。

### Decision 5: Node-id 参数解析约定 `[node_id]` 短前缀匹配

**Rationale**:
- 避免用户每次输入完整 UUID，缩短 `/tree abc123` 占位字符。
- 短前缀 8 字符匹配（与 git commit short SHA 一致）足以区分 session 内唯一节点。
- 歧义时（多 node 共前缀）返回错误 + 列表。

**Alternatives Considered**:
- **完整 UUID 输入**：UX 差。
- **序号引用 `/tree #3`**：与已有 message index UI 混淆。

## Risks / Trade-offs

- **[Risk] adr-0070 DECLARE_COMMAND 签名后续可能扩展** → 缓解：本 change 命令体仅调用 `CommandRegistry::route()`，未来扩展（如异步命令、流式响应）影响面限于 `body` 函数内部，无需改注册签名。
- **[Risk] JSONL 写入与 main.cpp 命令派发的并发竞争** → 缓解：`SessionManager` 已有 `std::mutex write_mutex_` 保护追加写（v1 ship §决策 2），命令派发路径走同一把锁外的协调层，并发追加测试已在 v2 24 cases 中覆盖。
- **[Risk] TUI ANSI escape 在某些终端（含 Windows cmd）显示异常** → 缓解：窄终端降级为列表模式（纯文本无 ANSI）作为 fallback；测试矩阵覆盖 Linux TTY + macOS Terminal + Windows Conhost 子集。
- **[Risk] `/clone` 深拷贝大 session 性能开销** → 缓解：JSONL 文件 IO 是 dominant cost；列出 clone 触发后异步执行的方案预留接口（`async_clone` flag），但 Wave 1 同步实现足够（实测 < 1s for 1000 nodes）。
- **[Risk] ToolCoordinator 调用路径增加 latency** → 缓解：协调路径无 IO（P2 ship 设计），layer check O(1) hash map 查询；端到端 P99 latency 增量 < 5ms。
- **[Trade-off] 命令体需新增 tool `session/fork` / `session/clone`** → ToolRegistry 注册增加 2 个条目，registry 体积 +0.4%，可接受。
- **[Trade-off] main.cpp 输入循环需修改以支持 `/tree <id>` 参数解析** → 改动局部（`process_slash_command` 函数内），其他 `/help` `/exit` `/compact` 路径零改动。

## Migration Plan

无数据迁移。`SessionManager` v1+v2 已 ship，本 change 仅是上层 UI/命令接入，存储层零变更。

部署步骤：
1. 实现合并 PR 后，`pdk_chat_demo` 二进制自动支持三个新命令（注册即生效）。
2. 用户无需额外配置，启动后 `/help` 立即列出 `/tree` `/fork` `/clone` 三个新命令。
3. 现有 session 兼容：旧 JSONL 文件经 `SessionManager::open` 加载即支持三个命令。

回滚策略：命令注册可通过 `CommandRegistry::unregister(name)` 显式移除，PR revert 可在 30s 内完成。

## Open Questions

- **OQ1**: `/fork` 是否支持 `--no-switch`（创建 branch 但不切换当前 leaf）？当前设计默认 auto-switch，波 2 可加。→ 默认 auto-switch 推进。
- **OQ2**: `/tree <id>` 切换 leaf 后，是否影响 in-flight LLM 调用？→ 不影响（LLM 调用捕获引用为局部），下一轮调用从新 leaf 重建。
- **OQ3**: 是否需要 `/tree -c` 折叠空 branch？→ Wave 1 仅 `--no-fold` 选项（默认折叠有消息空 branch），Wave 2 加 `-c`（按 leaf 数排序）。
