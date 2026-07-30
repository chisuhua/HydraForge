# pi-agent vs pdk-chat-demo 架构能力对比分析

**日期**: 2026-07-30
**状态**: ✅ 已完成
**来源**: pi-agent (earendil-works/pi, 80.8K ⭐, MIT) — `@earendil-works/pi-coding-agent`
**关联**: `examples/pdk_chat_demo/`, `docs/architecture/application-layer-sota-positioning-v2.md`

---

## 目录

- [一、概述](#一概述)
- [二、整体架构对比](#二整体架构对比)
- [三、P0：会话树 / Branching（最值得借鉴）](#三p0会话树--branching最值得借鉴)
- [四、P0：事件钩子体系（最值得借鉴）](#四p0事件钩子体系最值得借鉴)
- [五、P1：Streaming LLM 响应](#五p1streaming-llm-响应)
- [六、P1：消息队列 / Steering](#六p1消息队列--steering)
- [七、P1：上下文压缩（Compaction）](#七p1上下文压缩compaction)
- [八、P2：工具并行执行](#八p2工具并行执行)
- [九、P2：CLI 丰富化](#九p2cli-丰富化)
- [十、P2：Provider 动态发现](#十p2provider-动态发现)
- [十一、pi-agent 的独有能力（不适用或需降级借鉴）](#十一pi-agent-的独有能力不适用或需降级借鉴)
- [十二、总结：借鉴路线图](#十二总结借鉴路线图)

---

## 一、概述

### pi-agent 是什么

pi-agent（`earendil-works/pi`）是 TypeScript 生态中最成熟的 Agent 终端 CLI 之一（80.8K ⭐）。三个 npm 包分层：

| 层 | 包 | 职责 |
|---|-----|------|
| LLM API | `@earendil-works/pi-ai` | 30+ Provider 统一接口（OpenAI/Anthropic/Google/DeepSeek/...） |
| Agent 运行时 | `@earendil-works/pi-agent-core` | 状态机、工具调用、事件流、steering/follow-up |
| Coding Agent CLI | `@earendil-works/pi-coding-agent` | 交互式终端、会话树、扩展系统、技能、主题 |

### 分析范围

本文档聚焦 **pi-agent 有而 pdk-chat-demo 可以借鉴的架构能力**，按优先级（P0/P1/P2）排列。侧重点不在"谁更好"，而在"哪些能力的实现路径清晰、代价可控、收益最高"。

---

## 二、整体架构对比

| 维度 | pi-agent | pdk-chat-demo |
|------|----------|---------------|
| **语言** | TypeScript | C++20 |
| **架构模式** | 库模式（`import` + 扩展） | AgenticOS 模式（Plugin .so 热加载） |
| **Agent 定义** | `Agent` 类 | PDK Plugin (.so/.agent.md) |
| **循环引擎** | `agentLoop()` 事件驱动 | DSL 图 / `DEFINE_AGENT` |
| **Provider** | `pi-ai` 30+ 统一接口 | `ILLMProvider` + `LLMProviderFactory` |
| **会话** | JSONL 树（id/parentId） | ADR-0033 三层 + 线性 JSON |
| **扩展** | TypeScript 扩展（热加载） | `.so` Plugin（`PluginLoader`） |
| **事件模型** | 推模式（扩展订阅事件） | 拉模式（`call_tool` 主动调用） |
| **进程模型** | 单进程（扩展在 Agent 内） | 多进程（SKILL.md seccomp 隔离） |
| **安全** | 容器化（Gondolin/Docker/OpenShell） | 内建（seccomp + ApprovalHandler） |
| **终端** | 差分渲染 TUI（Markdown、图片、选择框） | `std::cout` 纯文本 |

### 关键差异

**pi-agent 的哲学**：核心小，TypeScript 扩展系统覆盖一切。扩展运行在 Agent 进程内，可监听所有事件、注册工具、替换 UI。

**pdk-chat-demo 的哲学**：C++ Plugin OS，Agent 是独立 `.so`，进程隔离，契约调用。扩展有更强的安全边界。

**pi-agent 的成熟度优势**：迭代更久（5193 commits），用户社区大（80.8K ⭐），终端 UX 打磨充分。以下能力是 pdk_chat_demo 可以借鉴的。

---

## 三、P0：会话树 / Branching（最值得借鉴）

### 差距描述

| 能力 | pi-agent | pdk-chat-demo |
|------|----------|---------------|
| **存储格式** | JSONL 树，每条 entry 有 `id` + `parentId` | 线性 JSON 数组，`save_to_disk()` 覆盖写 |
| **树导航** | `/tree` 命令，原地导航，分支间切换 | ❌ |
| **Fork** | `/fork`（从历史点创建新会话）、`/clone`（复制当前分支） | ❌ |
| **CLI fork** | `pi --fork <id>` | ❌ |
| **分支摘要** | 切换分支时 LLM 摘要被放弃分支，附加到新位置 | ❌ |
| **上下文构建** | `buildContextEntries()` 从叶子到根遍历，合并 compaction | 线性拼接 |
| **会话管理** | `SessionManager` API：`open()`, `forkFrom()`, `branch()` | `ChatSession` 内联管理 |
| **会话元数据** | 命名 `--name`、标签 `Shift+L`、搜索 | ❌ |
| **导出/分享** | `/export` HTML、`/share` Gist、`pi-share-hf` 到 Hugging Face | ❌ |

### pi-agent 的实现方式

**文件格式**：`~/.pi/agent/sessions/<path-->/<timestamp>_<uuid>.jsonl`

每条 entry 的结构（约简）：

```jsonl
{"id":"msg_abc","parentId":null,"type":"message","data":{"role":"user","content":"Hello"}}
{"id":"msg_def","parentId":"msg_abc","type":"message","data":{"role":"assistant","content":"Hi"}}
{"id":"msg_ghi","parentId":"msg_def","type":"message","data":{"role":"user","content":"Approach A"}}
{"id":"msg_jkl","parentId":"msg_def","type":"message","data":{"role":"user","content":"Approach B"}}
```

Entry 类型包括：`message`（用户/助手/工具结果），`model_change`，`thinking_level_change`，`compaction`，`branch_summary`，`label`，`custom`，`session_info`。

**SessionManager API**：

```typescript
// 创建/打开
SessionManager.create()
SessionManager.open(sessionPath)
SessionManager.continueRecent()
SessionManager.inMemory()
SessionManager.forkFrom(sessionPath)

// 实例方法
session.appendMessage(msg)
session.branch(entryId)                 // 原地分支
session.branchWithSummary(entryId, opts) // 分支 + 摘要
session.createBranchedSession(entryId)   // 创建新会话文件
session.buildContextEntries()            // 从叶子到根遍历
session.buildSessionContext()            // 构建 LLM 上下文
session.getLeafId()
session.getEntry(id)
session.getTree()
session.getChildren(parentId)
session.getLabel(entryId)
```

**上下文构建流程**：

```
buildContextEntries():
  从叶子节点开始，向上遍历到根
  - 遇到 compaction entry → 跳过已压缩的子树
  - 遇到 branch_summary → 包含摘要文本
  - 遇到 message → 包含消息内容
  → 返回有序的上下文条目列表

buildSessionContext():
  调用 buildContextEntries() → 转换为 LLM 的 Message[] 列表
  - compaction 摘要 → 作为 system 消息
  - branch_summary → 作为 system 消息
  - 普通消息 → 按角色转换
  - custom_message → 按 extension 定义转换
```

### 借鉴方式

**实现路径**：

1. 重写存储层为 JSONL 树格式，每条记录 `id` + `parentId`
2. 新增 `SessionManager` 类（C++）管理 open/fork/branch/compact
3. 修改 `ChatSession` 使用 `SessionManager` 替代线性 `std::vector<Message>`
4. 实现 `build_context_entries()` 和 `build_llm_context()` 从叶子到根遍历
5. 在 `EventHandler` 中实现浅层树导航 TUI（简易版 `/tree`）
6. 后续：`--fork` CLI flag、`--name` 会话命名、`/export` HTML

**为什么不直接抄**：pdk_chat_demo 的 ADR-0033 已有三层会话模型（UserSession/TaskSession/SubtaskSession），但那是**执行作用域**而非**存储格式**。两者可以共存——`SessionManager` 负责持久化树，`UserSession` 保持运行时执行上下文。

**估时**：2-3 Sprint（存储层 + 树导航 + 分支摘要）

---

## 四、P0：事件钩子体系（最值得借鉴）

### 差距描述

| 能力 | pi-agent | pdk-chat-demo |
|------|----------|---------------|
| **Agent 生命周期** | `agent_start`/`agent_end`/`agent_settled` | ❌ 无 |
| **Turn 生命周期** | `turn_start`/`turn_end` | ✅ `IInteractionBus` 有 `loop.turn.*` |
| **消息流** | `message_start`/`message_update`/`message_end` | ❌ 无 |
| **工具调用** | `tool_execution_start`/`update`/`end` | ✅ `tool.execution.*` |
| **工具拦截** | `beforeToolCall` 可 block，`afterToolCall` 可修改 | ❌ 无（`ApprovalHandler` 可拒绝但不可修改） |
| **上下文注入** | `transformContext()` 在 LLM 调用前注入/修剪消息 | ❌ 无 |
| **会话管理** | `session_before_switch`、`session_before_fork`、`session_before_compact` | ❌ 无 |
| **模型切换** | `model_select`、`thinking_level_select` | ❌ 无 |
| **输入拦截** | `input` 事件可拦截、转换或处理 | ❌ 无 |
| **扩展注册** | `pi.registerTool(definition)` | ✅ `pdk_register_tools` |
| **扩展命令** | `pi.registerCommand(name, handler)` | ❌ 无 |
| **扩展快捷键** | `pi.registerShortcut(shortcut, handler)` | ❌ 无 |

### pi-agent 的实现方式

30+ 事件钩子，覆盖完整生命周期：

```
启动:
  project_trust → session_start → resources_discover

Agent 循环:
  before_agent_start (可注入消息、修改 system prompt)
  → agent_start
  → turn_start → message_start/update/end
  → tool_execution_start → tool_call (可block) → tool_execution_update → tool_result (可修改) → tool_execution_end
  → turn_end
  → agent_end → agent_settled

会话管理:
  session_before_switch → session_shutdown → session_start
  session_before_fork → session_shutdown → session_start
  session_before_compact → session_compact
  session_before_tree → session_tree
```

### 借鉴方式

**实现路径**：

1. 利用已有的 `IInteractionBus` 事件基础设施，扩展 topic 命名空间
2. 新增 `before_tool_call` 钩子（`ToolCoordinator` 层，返回值可 block）
3. 新增 `after_tool_call` 钩子（可修改 `ToolResult`）
4. 给 `AgentSession` 添加 `session_before_fork`、`session_before_compact` 事件
5. Plugin 通过 `IInteractionBus::subscribe()` 订阅事件
6. 后续：`transform_context` 钩子（LLM 调用前注入/修剪消息）

**为什么可以借鉴**：pdk_chat_demo 已有 `IInteractionBus` 和 `InMemoryBus`，事件基础设施已经就位，只是事件粒度和钩子机制不够丰富。不需要改架构，只需要扩展 topic 枚举和钩子签名。

**估时**：1 Sprint（事件扩展 + 钩子注册 API）

---

## 五、P1：Streaming LLM 响应

### 差距描述

| 能力 | pi-agent | pdk-chat-demo |
|------|----------|---------------|
| **LLM 调用** | 异步 stream，`text_delta` 事件逐字推送 | 同步阻塞 `generate()` |
| **用户感知** | 逐字输出，可看到推理过程 | 等待完成才看到完整回复 |
| **Thinking 显示** | `thinking_delta` 事件，`Ctrl+T` 折叠/展开 | ❌ 无 |
| **工具调用流** | 渐进式 JSON 解析，部分结果可见 | ❌ 等待完整工具结果 |

### pi-agent 的实现方式

```typescript
// 事件序列
message_start { message: assistantMessage }
message_update { assistantMessageEvent: { type: "text_delta", delta: "Hello" } }
message_update { assistantMessageEvent: { type: "text_delta", delta: " world" } }
message_update { assistantMessageEvent: { type: "toolcall_delta", delta: "{\"na" } }
message_update { assistantMessageEvent: { type: "toolcall_delta", delta: "me\":\"read" } }
message_end   { message: assistantMessage }
```

### 借鉴方式

**实现路径**：

1. 使用 ADR-0001 已有的 `IGenerationStream` 接口（pull-based）
2. 在 `ILLMProvider` 层新增 `stream_generate()` 方法
3. Chat Agent 通过 `IInteractionBus` 推送 `llm.token.{text,thinking,toolcall}` 事件
4. `EventHandler` 订阅这些事件，实时输出到终端
5. `OrchestrationILLMProvider` 已有 decorator 链，可以包装 stream

**估时**：1-2 Sprint（接口扩展 + 事件推送 + 终端渲染）

---

## 六、P1：消息队列 / Steering

### 差距描述

| 能力 | pi-agent | pdk-chat-demo |
|------|----------|---------------|
| **Agent 运行时输入** | 可提交 steering 消息（当前 turn 工具执行完后注入） | 阻塞等待 `getline`，Agent 运行时无法输入 |
| **Follow-up** | 可提交 follow-up 消息（Agent 全部工作完成后注入） | ❌ |
| **队列管理** | Escape 恢复、Alt+Up 取回、Alt+Enter 切换模式 | ❌ |
| **模式** | `"one-at-a-time"`（默认）和 `"all"` | ❌ |

### pi-agent 的实现方式

```typescript
// 用户按 Enter → steering 消息
agent.steer({ role: "user", content: "Stop! Do this instead." });

// 用户按 Alt+Enter → follow-up 消息
agent.followUp({ role: "user", content: "Also summarize the result." });

// 配置
agent.steeringMode = "one-at-a-time";  // 或 "all"
agent.followUpMode = "one-at-a-time";
```

### 借鉴方式

**实现路径**：

1. 重构 `main.cpp` 的交互循环——从同步 `while(getline)` 改为异步双线程（stdin 读取线程 + Agent 执行线程）
2. 新增 `steering_queue_` 和 `follow_up_queue_` 在 `ChatSession` 中
3. Agent 在 turn 结束时检查 steering queue，在全部结束时检查 follow-up queue
4. 通过 `IInteractionBus` 事件通知 `EventHandler` 更新 UI 状态

**估时**：1 Sprint（异步 I/O + 队列 + 事件集成）

---

## 七、P1：上下文压缩（Compaction）

### 差距描述

| 能力 | pi-agent | pdk-chat-demo |
|------|----------|---------------|
| **触发方式** | 自动（threshold/overflow）+ 手动（`/compact`） | ❌ 无 |
| **压缩方式** | LLM 摘要旧消息，保留最近消息 | ❌ |
| **历史保留** | 完整历史在 JSONL，通过 `/tree` 可回溯 | ❌ |
| **扩展钩子** | `session_before_compact` 可取消或自定义，`session_compact` 可观察结果 | ❌ |

### pi-agent 的实现方式

```typescript
// 自动触发条件
// 1. threshold: 接近上下文窗口限制时主动触发
// 2. overflow: 超限时压缩，恢复后重试

// 事件钩子
pi.on("session_before_compact", async (event, ctx) => {
  return { cancel: true };                          // 取消压缩
  return { compaction: { summary: "...", ... } };   // 自定义摘要
});
pi.on("session_compact", async (event, ctx) => {
  // event.reason: "manual" | "threshold" | "overflow"
  // event.willRetry: 溢出恢复后是否重试
});
```

### 借鉴方式

**实现路径**：

1. 在 `ChatSession` 中添加 compaction 阈值检测（基于 token 计数）
2. 调用 `ILLMProvider::generate()` 对历史消息做 LLM 摘要
3. 压缩后的摘要作为 `system` 消息注入，旧消息从活跃上下文移除
4. 完整历史仍保留在 JSONL 存储中
5. `IInteractionBus` 新增 `context.compact.{before,after}` 事件

**为什么可以借鉴**：pdk_chat_demo 的 `LayeredContext` 已有 5 层结构，`LLMProviderFactory` 已可用。LLM 摘要只需要调用 `generate()` 即可。

**估时**：1 Sprint（阈值检测 + 摘要生成 + 上下文替换）

---

## 八、P2：工具并行执行

### 差距描述

| 能力 | pi-agent | pdk-chat-demo |
|------|----------|---------------|
| **执行模式** | 默认并行（preflight 顺序验证，允许的并发执行） | 串行，只取第一个 `tool_calls[0]` |
| **覆盖方式** | 全局 `toolExecution` + 按工具 `executionMode` | ❌ |
| **终止机制** | `terminate: true` 跳过后续 LLM 调用 | ❌ |
| **事件粒度** | `tool_execution_start` → `update` → `end`（按完成顺序） | `tool.execution.start` → `end` |

### pi-agent 的实现方式

```typescript
// 全局配置
const agent = new Agent({
  toolExecution: "parallel"  // 或 "sequential"
});

// 按工具覆盖
const tool: AgentTool = {
  name: "deploy",
  executionMode: "sequential",  // 强制整个批次串行
  execute: async (id, params, signal, onUpdate) => {
    // ...
    return { content: [...], terminate: true };  // 跳过后续 LLM 调用
  }
};
```

### 借鉴方式

**实现路径**：

1. 修改 Loop Agent DSL 的 tool_call 节点——从 `tool_calls[0]` 改为 `tool_calls[]`
2. 使用 `DomainWorkerPool`（Sprint 3 已实现，N 个 `std::jthread`）并行执行多工具
3. 工具结果按 assistant 原始顺序归并
4. 通过 `IInteractionBus` 推送 `tool.execution.update` 事件

**估时**：1 Sprint（Loop 节点修改 + 并行执行 + 结果归并）

---

## 九、P2：CLI 丰富化

### 差距描述

| CLI 能力 | pi-agent | pdk-chat-demo |
|----------|----------|---------------|
| **非交互模式** | `-p`（print mode）、`--mode json`（事件流）、`--mode rpc`（RPC 协议） | ❌ |
| **继续会话** | `-c`（继续最近）、`-r`（浏览选择） | `--session <id>` |
| **Fork 会话** | `--fork <id>` | ❌ |
| **离线模式** | `--offline` | ❌（mock 模式是 fake LLM） |
| **扩展加载** | `-e ./ext.ts`（可重复） | 通过 `config.json` 声明 |
| **Provider 选择** | `--provider <name> --model <pattern>` | `config.json` 静态配置 |
| **工具控制** | `--tools <list>`、`--no-tools`、`--exclude-tools` | ❌ |
| **系统提示词** | `--system-prompt <text>`、`--append-system-prompt <text>` | ❌ |
| **会话命名** | `--name "xxx"`、`-n "xxx"` | ❌ |
| **版本信息** | `-v`、`--version` | ❌ |

### 借鉴方式

**实现路径**：

1. 使用 `cxxopts` 或 `argparse` 库扩展 `main.cpp` 的命令行解析
2. 新增：`-p`（print mode 非交互）、`-c`（继续最近 session）、`-r`（列出 session 供选择）
3. 新增：`--provider`、`--model` 覆盖 `config.json` 配置
4. 新增：`--no-session` 临时模式（不保存）
5. 新增：`--system-prompt`、`--append-system-prompt`
6. 新增：`--offline` 模式（禁用网络）

**估时**：~3 天（参数解析扩展 + 运行模式分发）

---

## 十、P2：Provider 动态发现

### 差距描述

| 能力 | pi-agent | pdk-chat-demo |
|------|----------|---------------|
| **Provider 数量** | 30+ 内置，可扩展 | 4 个（deepseek/openai/anthropic/mock） |
| **模型目录** | 自动拉取最新模型列表 | 静态 `config.json` |
| **凭据管理** | env var + OAuth + 存储凭据 + 动态刷新 | env var 或 `config.json` 明文 |
| **Provider 注册** | 扩展可 `pi.registerProvider()` | `config.json` 静态声明 |
| **模型切换** | 运行时 `/model`（Ctrl+L） | 启动时 `config.json` |

### pi-agent 的实现方式

```typescript
// 扩展动态注册 provider
pi.registerProvider("local-openai", {
  baseUrl: "http://localhost:1234/v1",
  apiKey: "$LOCAL_OPENAI_API_KEY",
  api: "openai-completions",
  models: dynamicModelList.map(m => ({
    id: m.id,
    name: m.name ?? m.id,
    cost: { input: 0, output: 0, cacheRead: 0, cacheWrite: 0 },
    contextWindow: m.context_window ?? 128000,
    maxTokens: m.max_tokens ?? 4096,
  })),
});
```

### 借鉴方式

**实现路径**：

1. Provider Agent 扩展 `provider/refresh` 工具——从 API 拉取最新模型目录
2. 添加 `provider/register_dynamic` 支持运行时注册
3. `LLMProviderFactory` 扩展支持从运行时注册的 provider 创建（已有骨架）
4. 后续：`provider/switch` 工具支持运行时切换模型

**估时**：1 Sprint（动态刷新 + 运行时注册）

---

## 十一、pi-agent 的独有能力（不适用或需降级借鉴）

以下能力是 pi-agent 的核心优势，但**不适合 pdk-chat-demo 直接复制**，原因如下：

| 能力 | 不适用原因 | 替代方案 |
|------|-----------|---------|
| **TypeScript 扩展系统** | pdk_chat_demo 是 C++ 架构。TS 扩展运行在 Agent 进程内，无安全隔离 | 保持 `.so` Plugin 模型，增加事件钩子 API |
| **差分渲染 TUI** | 在 C++ 中实现同等 TUI 需要 `ncurses`/`ftxui` 依赖，且 pdk_chat_demo 定位是 demo 而非产品 CLI | 保持 `std::cout`，改进格式化输出 |
| **30+ 内置 Provider** | 每个 provider 需要单独的 API 适配和 OAuth 流程 | 保持 `LLMProviderFactory` 抽象，按需扩展 |
| **安装包管理** | `pi install/uninstall` 针对 npm 生态 | 后续可做 `pdk install`，但需包注册表基础设施 |
| **会话分享到 Hugging Face** | 依赖 `pi-share-hf` 外部工具 | 后续可做 `/export` HTML 导出 |
| **图片支持** | 需要终端协议（Kitty/iTerm2）和图片处理 | 非核心场景，可推迟 |
| **Thinking/Reasoning** | 依赖 provider 的 thinking API（Claude/OpenAI） | 等 provider 支持后对接 |

---

## 十二、总结：借鉴路线图

### 优先级判定依据

- **P0**：对用户体验有根本性提升，且 pdk_chat_demo 已有基础设施可以复用
- **P1**：对用户体验有明显提升，实现路径清晰但需要一定重构
- **P2**：锦上添花，可在 P0/P1 完成后逐步追加

### 路线图

```
Sprint N     Sprint N+1      Sprint N+2      Sprint N+3
─────────────────────────────────────────────────────────────
P0.1 会话树     P0.2 树导航     P1.1 Streaming   P2.1 工具并行
  ├ 存储层       ├ /tree TUI     ├ IGeneration    ├ 多 tool_call
  ├ Session     ├ 分支摘要      ├ 事件推送       ├ DomainWorker
  └ Manager     └ --fork CLI    └ 终端渲染       └ 结果归并

P0.3 事件钩子    P1.2 消息队列   P1.3 Compaction  P2.2 CLI 丰富化
  ├ before_tool  ├ 异步 stdin    ├ 阈值检测       ├ -p -c -r
  ├ after_tool   ├ steering      ├ LLM 摘要       ├ --offline
  └ 订阅 API     └ follow-up     └ 上下文替换     └ --provider

                                    P2.3 Provider 动态
                                      ├ 刷新工具
                                      └ 运行时注册
```

### 每个 Sprint 的边界

| Sprint | 工作范围 | 不做的 |
|--------|---------|--------|
| **N** | 会话树存储层 + SessionManager + 事件钩子骨架 | 树导航 TUI、分支摘要、Streaming |
| **N+1** | 树导航 TUI + 分支摘要 + 消息队列 + 异步 I/O | Compaction、工具并行、CLI 丰富化 |
| **N+2** | Streaming + Compaction | 工具并行、Provider 动态发现 |
| **N+3** | 工具并行 + CLI 丰富化 + Provider 动态发现 | — |

### 核心约束

1. **不破坏现有的 Plugin 契约**——所有新增钩子走 `IInteractionBus` 事件，不修改 `pdk_register_tools` 签名
2. **不引入新的外部依赖**——TUI 保持 `std::cout`，不引入 `ncurses`/`ftxui`
3. **保持进程隔离**——事件钩子可跨进程（SKILL.md 子进程），不要求 Plugin 在 Agent 进程内
4. **向后兼容**——旧的线性 session 文件可自动迁移到树格式