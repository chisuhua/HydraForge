## Context

Wave 1 `session-tree-commands` 已 ship，`pdk_chat_demo` 通过 `/tree`、`/fork`、`/clone` 支持交互式会话树操作。Wave 2 `cli-args-cxxopts` 将提供声明式 CLI flag 基础设施和集中化 help 文案。本 change 必须排在 `cli-args-cxxopts` 之后，不能独立 ship 或恢复 `main.cpp` 手写参数扫描。

`SessionManager` JSONL tree storage 与 `fork`、`switch_branch`、`append_to_branch`、`build_context_entries` API 已由 `session-manager-jsonl` v1+v2 ship。当前 `examples/pdk_chat_demo/main.cpp` 仍以局部参数扫描处理 `--session`，并在启动后加载 session。此 change 在该入口上增加两个 session-tree 专用启动 flag：`--fork <node_id>` 和 `--name <session_name>`。

`--fork` 复用已持久化的 session tree，在进入 `chat_loop` 前加载指定 session，并从指定 node 创建新 branch、切换到新 branch。`--name` 用于新启动 session 的名称持久化。两者应与现有 `--mock`、`--session <id>` 一起工作，并通过声明式 registry 自动出现在 `--help` 中。

## Goals / Non-Goals

**Goals:**

- 通过 `cli-args-cxxopts` 的声明式 registry 注册 `--fork <node_id>`，禁止新增手写 argv 循环。
- 注册 `--name <session_name>`，在新 session 创建时设置并持久化 session metadata。
- 将启动顺序固定为解析 flags、加载目标 session、执行 fork 和 name 操作、再进入正常 chat loop。
- 对不存在的 session、节点和非法 node id 提供非零退出码、稳定错误前缀和 `--help` 指引。
- 保持 `--mock`、`--session`、无参数启动和现有 slash commands 的行为不变，并覆盖单测与 E2E。

**Non-Goals:**

- 不实现 `/tree`、`/fork`、`/clone` slash commands，它们属于 `session-tree-commands` Wave 1。
- 不实现 `--mode json|rpc`，该能力依赖 ADR-0059。
- 不实现 `-c` 最近会话续接或 `-r` 会话选择，它们属于 `session-manager-jsonl` 的独立增量。
- 不实现通用 flag parser 或替代 `cli-args-cxxopts`，本 change 只消费其声明式 API。
- 不实现运行时 fork UI、分支合并、重基或 TUI 增量渲染，运行时 UI 属于 Wave 3。

## Decisions

### Decision 1: `--fork` 是启动时 flag，不新增交互命令

**Rationale:**

`/fork` 已由 Wave 1 提供给正在运行的用户。`--fork` 面向脚本、快捷方式和恢复工作流，语义是启动前选择一个历史节点并以新 branch 开始。两者入口不同但复用同一 `SessionManager::fork` API。这样避免在 slash command 层复制启动逻辑，也避免同一功能出现两套错误语义。

**Alternatives Considered:**

- **只提供 `/fork`**：无法在进入 chat loop 前恢复指定节点，不满足启动脚本和自动化场景。
- **让 `--fork` 转换成首条交互输入**：会先启动错误的 session，且可能触发 LLM 或输出不稳定的提示，不符合启动 flag 的确定性。

### Decision 2: `--name` 只应用于新 session

**Rationale:**

`--name` 的作用是为本次启动创建的 session 提供稳定的人类可读名称。与 `--session` 同时使用时，目标是恢复已有 session，名称不能隐式改写历史 metadata。这样避免脚本重启时意外重命名已有会话，也让 `--name` 的持久化范围清晰。

若后续需要重命名已有 session，应增加显式 `--rename` 或 slash command，并单独定义审计、冲突和权限语义。本 change 不扩展 `SessionManager` 的重命名范围以外的功能。

**Alternatives Considered:**

- **`--name` 同时重命名已有 session**：方便但有隐式持久化副作用，且与 `--fork` 组合时会混淆“源 session”和“新 branch”的名称归属。
- **只在内存中保存名称**：重启后丢失，违反 proposal 对 metadata 持久化的要求。

### Decision 3: `--fork` 失败时立即退出并返回非零状态

**Rationale:**

不存在的 node id 表示用户指定的启动目标无法满足。静默忽略会继续在错误的 leaf 上对话，可能写入错误 branch，造成数据风险。启动阶段应输出包含 `--fork`、目标 id 和 `--help` 的清晰错误，并在进入 chat loop 前返回非零状态。不存在的 session 同样沿用现有 `--session` 错误契约，不降级到新 session。

**Alternatives Considered:**

- **静默忽略并继续新 session**：不安全，用户无法确认实际操作对象。
- **打印 warning 后继续当前 leaf**：仍可能把后续消息写入错误上下文，错误难以恢复。

### Decision 4: `--fork` 先加载 session，再调用 fork

**Rationale:**

node index 属于已加载的 `SessionManager` 状态。固定顺序为 parse、open/load、fork、apply new-session name、chat loop，能确保 node lookup 使用正确的 JSONL 文件。`--fork` 没有 `--session` 时使用默认 session 选择规则，若该规则无法确定目标则在启动阶段报错。

**Alternatives Considered:**

- **先 fork 再加载**：无法解析节点所属 session。
- **让 `SessionManager::fork` 自己扫描所有 session**：扩大 core API 和 IO 范围，违背本 change 仅做 CLI wiring 的边界。

## Risks / Trade-offs

- **`cli-args-cxxopts` 在本 change 编码时尚未 ship**：必须把 `cli-args-cxxopts` 作为硬依赖并按 wave 顺序执行。缓解方式是先锁定其声明式 registry 接口，再实现本 change，不在本 change vendoring 第二套 parser。
- **node_id 格式可能存在完整 ID、短前缀和旧格式差异**：短前缀解析可能产生歧义。缓解方式是 startup flag 使用 SessionManager 已定义的完整节点查找契约，非法格式和多重匹配都转为明确错误，不猜测目标。
- **`--name` 的 persistence scope 容易与已有 session 混淆**：本设计明确只写新 session metadata，`--session` 与 `--name` 组合必须拒绝或按稳定规则报错，测试锁定该行为。
- **main.cpp 启动顺序涉及 ChatSession 与 SessionManager 两个状态对象**：错误地在 chat loop 后 fork 会产生错误上下文。缓解方式是将 startup operation 封装为可测试的顺序步骤，并在进入输入循环前完成所有失败检查。
- **`--fork` 可能创建 branch 后在后续启动步骤失败**：fork 是持久化操作，不能依赖 chat loop 成功。缓解方式是先完成参数和 session 校验，再执行唯一写操作；成功 fork 后只允许无失败的 name/session wiring。

## Migration Plan

无需数据迁移。已有 JSONL session 文件保持兼容，新增 flag 只在启动时读取并调用现有 SessionManager API。部署后，旧命令和不带新 flag 的启动路径保持原行为。

验证顺序：先验证 `cli-args-cxxopts` 的 registry 和 help 输出，再实现 flag wiring，随后运行 flag 单测、pdk chat demo E2E 和全量 ctest。回滚时移除新增 flag registrations 和 startup wiring，不改动已有 JSONL 数据。
