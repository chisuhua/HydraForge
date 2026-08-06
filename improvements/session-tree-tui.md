# session-tree-tui

**优先级**: P2 | **来源**: layer-based-missing-capabilities-analysis.md §八 L4-6（Wave 2 拆分 — CLI flag 子集）
**阶段**: wave-2 | **分类**: demo-chat-v2
**类型**: feature

> **Wave 拆分说明**: 本提案是原 `session-tree-tui` 的 Wave 2 拆分（仅含 CLI flag 子集）。
> 原提案拆分为：(Wave 1) `session-tree-commands` slash 命令主体 + (Wave 2) 本提案 CLI flag 子集。
> 拆分理由：session-manager-jsonl ✅ ship + adr-0070 ✅ ship 已解锁 Wave 1 主体；CLI flag 实现依赖 cli-args-cxxopts（Wave 2）。
> 本提案 blocked-by：`cli-args-cxxopts` + `session-tree-commands`。

## 架构依据
- Wave 1 `session-tree-commands` ship 后，slash 命令内已支持 fork / clone，但启动时无法直接 fork 到指定节点继续会话。
- pi-agent 借鉴路径 §九：CLI flag 设计要求 `-c` 续最近 / `-r` 选择 / `--fork` 指定节点 / `--name` 重命名会话。
- cli-args-cxxopts (Wave 2) 提供声明式 flag 解析基础，本提案在其上层叠加 2 个 session-tree 专用 flag。

## 范围
- **In Scope**:
  - `--fork <node_id>` 启动 flag：加载指定 session 后立即从该节点 fork 继续会话（CLI 参数 + main.cpp 入口 + SessionManager::fork 调用串联）
  - `--name <session_name>` 启动 flag：启动时重命名当前会话（持久化到 SessionManager metadata）
  - flag 与 cli-args-cxxopts 集成（声明式注册，非手撸循环）
- **Out of Scope**:
  - 通用 CLI flag 解析基础设施（→ cli-args-cxxopts）
  - `--mode json|rpc`（RPC 模式依赖 ADR-0059 跨进程协议，缓建）
  - `-c/-r` 会话选择（依赖 session-manager-jsonl 的会话枚举，独立增量）
  - slash 命令 `/tree` `/fork` `/clone`（→ session-tree-commands，Wave 1）

## 关键场景
- GIVEN 现有会话含节点 `node_42`，WHEN 启动 `./pdk_chat_demo --session sess_abc --fork node_42`，THEN 加载会话后立即从 node_42 fork 新 branch 并切换，进入正常对话循环。
- GIVEN 启动 `./pdk_chat_demo --name "my-debug-session"`，THEN 创建新会话并命名为 "my-debug-session"，持久化到 SessionManager。
- GIVEN 同时传 `--session sess_abc --fork node_42`，THEN 串联执行（先 load_session 再 fork_node），错误路径有明确报错。
- GIVEN `--fork` 指向不存在的节点，WHEN 启动，THEN 报错并退出（exit code 非零 + 错误信息指向 `--help`）。

## 技术约束
- MUST flag 声明经 cli-args-cxxopts（vendored 到 external/），禁止手撸解析。
- MUST flag 行为与现有 `--mock` / `--session <id>` 完全等价（E2E 回归）。
- MUST `--fork` 错误路径（节点不存在 / session 不存在）有明确报错，不进入对话循环。
- SHOULD flag 声明与 help 文案集中化（cli-args-cxxopts 自动生成）。
- MUST NOT 引入新外部依赖（cli-args-cxxopts vendored 即可）。

## 验收标准
- 4 个 flag 组合 E2E 测试通过（mock 模式 + 真实 LLM 模式）。
- `--help` 输出含新增 flag 说明（自动生成）。
- 错误路径测试（不存在的 node_id / session_id）退出码非零。
- 既有 flag 行为零回归。
- ctest 全量零回归。
