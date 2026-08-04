# ADR-0076: DSL Engine as MCP Server (控制面, MCP 2025-11-25)

## 状态

🔍 Proposed (2026-08-03 — 派生自 ADR-0071 §决策 D7, Wave 3 末 ADR; **INTEGRATES WITH Phase 6 Candidate B (服务化)** 但 **gated by active-status.md §四 (Candidate B 结构性暂缓)**: ship 需 AgentForge ≥ Sprint 25 milestone + Solo Dev 容量 ≥2 人; stdio + HTTP+SSE 双 transport, 静态 token MVP; 衔接 ADR-0073 (Tool JSON Schema) 零转换 + ADR-0074 (Prompt) prompts/* + ADR-0075 (EnvBackend) stdio 复用; 待架构组评审; 实施 2-3 周, 启动需候选启动条件满足)

## 领域

L1 OS Services / 服务化 / MCP 协议 / 控制面 / 跨进程互操作 / Phase 6 Candidate B 落地

## 关联

### 父 ADR (最高优先级)
- [ADR-0071 — LLM-native AgenticDSL 架构](./adr-0071-llm-native-agenticdsl-architecture.md) §决策 D7 (本 ADR 是 D7 的具体实施) — **INTEGRATES WITH Phase 6 Candidate B (ADR-0050 §决策推荐 B 服务化)**, 但 ship gated by [`docs/active-status.md` §四](../active-status.md#四) (Candidate B 结构性暂缓, 需 AgentForge ≥ Sprint 25 + Solo Dev ≥2 人)
- ADR-0071 §决策 D5 (本 ADR `prompts/*` 复用 D5 训练数据 JSONL)
- ADR-0071 §战略协调 — 本 ADR 取代 ADR-0050 原"InferenceServer MCP" 路径, 范围收窄聚焦 DSL Engine

### 上游锚定
- [ADR-0004 — ToolRegistry 安全模型](./adr-0004-toolregistry-security.md) — 暴露哪些 tool 由 ToolCategory 矩阵决定 (本 ADR Dangerous 工具仅在 approval_granted 时响应)
- [ADR-0019 — IInteractionBus MVP](./adr-0019-iinteraction-bus-mvp.md) — MCP server 内部事件通过 IInteractionBus 上报
- [ADR-0050 — Phase 6 战略评估](./adr-0050-phase6-strategic-evaluation.md) — Candidate B 方向 (本 ADR 是其具体落地)
- [ADR-0051 — Phase 6 PDK 组合 Spike](./adr-0051-phase6-pdk-composition-spike.md) — 复用 spike 产出作为 MCP server 实现参考 (`docs/service-composition/`)
- [ADR-0068 — 事件发射契约](./adr-0068-event-emission-contract.md) — 本 ADR 设计 **6 个候选幻影主题** `mcp.server.{connected,disconnected,request,response}` + `mcp.client.{request,response}` (注册前置: ADR-0068 §附录 A amendment PR)
- [ADR-0069 — ToolCoordinator Hooks](./adr-0069-tool-coordinator-hooks.md) — MCP tool call 走 ToolCoordinator execute 流 (含 EnvValidationHook)
- [ADR-0073 — Tool JSON Schema 契约](./adr-0073-tool-json-schema-contract.md) — MCP `inputSchema` = ToolMetadata V3 schema (零转换)
- [ADR-0074 — Prompt Engineering + Evidence Gate](./adr-0074-prompt-evidence-gate.md) — `prompts/*` 内容来源 (baseline JSONL + few-shot examples)
- [ADR-0075 — EnvBackend Local + Docker](./adr-0075-env-backend-local-docker.md) — stdio 传输复用 LocalBackend 模式

### 平行/下游
- ADR-0077 (gRPC Data Plane, Wave 4, descoped) — 本 ADR MCP server 是控制面, gRPC 是数据面, 互不替代
- ADR-0078 (Fine-tune 基模, Wave 5+, AgenticMind 回流) — MCP `prompts/*` 数据回流到 AgenticMind 训练

### 规范
- [MCP Spec 2025-11-25](https://modelcontextprotocol.io/specification/2025-11-25) — 官方规范 (latest stable)
- [JSON-RPC 2.0](https://www.jsonrpc.org/specification) — MCP 底层传输协议
- [JSON Schema Draft 2020-12](https://json-schema.org/draft/2020-12/schema) — `inputSchema` 字段 (ADR-0073 衔接)
- [`docs/specs/dsl.md`](../specs/dsl.md) v3.10 — DSL 语法 (本 ADR 不变更)
- [`docs/specs/architecture.md`](../specs/architecture.md) §3 — L1 OS Services 服务化抽象

---

## 背景

### 问题

ADR-0050 (2026-07-23) 决议 Phase 6 走 **Candidate B (服务化)**, 但 "InferenceServer MCP + OpenAI-compatible API" 范围过宽 (估时 4-6 周, 1-2 工程师), 与 Solo Dev 容量不匹配 (Phase 6a 37h / Phase 6b 44h)。具体 5 个空白:

1. **服务化路径不清晰** — 暴露什么能力? 用什么协议? 鉴权如何? 都未定义
2. **ToolMetadata 与 MCP `inputSchema` 字段不兼容** — 当前手工转换, 易漂移
3. **std 协议 vs Streamable HTTP 选型未决** — MCP 2025-11-25 spec 双 transport, 决策缺失
4. **静态 token MVP 未实现** — 远程访问无鉴权, 安全风险
5. **PDK 组合 spike 产出未落地** — ADR-0051 spike (`docs/service-composition/`) 仅实验, 未 ship

### 解决方案

**聚焦实现**: DSL Engine 作为 **MCP server (stdio + HTTP+SSE 双 transport)** 暴露 **3 类能力** (tools/* + prompts/* + resources/*), 用 **静态 token** 做 MVP 鉴权, ToolMetadata V3 schema 零转换映射为 MCP `inputSchema`:

```
┌─────────────────────────────────────────────────────────────┐
│              DSL Engine as MCP Server                        │
│                                                               │
│  ┌────────────────────────────────────────────┐              │
│  │  Transport Layer (D1)                       │              │
│  │  ├─ stdio (本地进程集成)                     │              │
│  │  └─ HTTP + SSE (远程, 静态 token)           │              │
│  └────────────────────────────────────────────┘              │
│                          ↓                                     │
│  ┌────────────────────────────────────────────┐              │
│  │  Auth Layer (D2)                            │              │
│  │  静态 token MVP (Bearer / X-MCP-Token)      │              │
│  └────────────────────────────────────────────┘              │
│                          ↓                                     │
│  ┌────────────────────────────────────────────┐              │
│  │  MCP Capability Router (D3-D5)              │              │
│  │  ├─ tools/list, tools/call (D3)             │              │
│  │  ├─ prompts/list, prompts/get (D4)          │              │
│  │  └─ resources/list, resources/read (D5)     │              │
│  └────────────────────────────────────────────┘              │
│                          ↓                                     │
│  ┌────────────────────────────────────────────┐              │
│  │  Backend Adapter (D6)                       │              │
│  │  ToolMetadata V3 schema → MCP inputSchema   │              │
│  │  (零转换, ADR-0073 衔接)                    │              │
│  └────────────────────────────────────────────┘              │
│                          ↓                                     │
│  ┌────────────────────────────────────────────┐              │
│  │  ToolCoordinator (D7)                       │              │
│  │  MCP tool call → ToolCoordinator.execute    │              │
│  │  (含 EnvValidationHook, ADR-0069 衔接)     │              │
│  └────────────────────────────────────────────┘              │
└─────────────────────────────────────────────────────────────┘
```

### 已实证证据

- **MCP 2025-11-25 spec 已锁定**: tools/list 的 `inputSchema` = JSON Schema 2020-12 (ADR-0073 已确认)
- **ADR-0051 spike 产出**: `docs/service-composition/spike-onboarding.md` + Layer 3 dual memos 可复用
- **Project lib/ 12 个 stdlib subgraphs**: 已可作为 MCP `resources/*` (D5) 内容
- **Project pdk/ 12+ 工具**: 已注册到 ToolCoordinator, 可作为 MCP `tools/*` (D3) 内容
- **ADR-0074 baseline 数据**: 已规划 30+ few-shot + 50 golden tasks JSONL, 可作为 MCP `prompts/*` (D4) 内容
- **ToolCoordinator hook 体系** (ADR-0069): MCP tool call 复用现有 execute 流零成本

---

## 决策

### D1. 双 Transport — stdio + HTTP+SSE (MCP 2025-11-25)

**stdio 传输** (本地进程集成):

- **用途**: 同一进程 / 子进程集成 (e.g. Claude Desktop, IDE 插件, VS Code extension)
- **实现**: 复用 ADR-0075 LocalBackend 的 pipe 模式 (`stdin` → JSON-RPC, `stdout` ← JSON-RPC)
- **零额外成本**: 项目已 vendor `nlohmann_json` + pipe I/O 模式已验证

**HTTP + SSE 传输** (远程访问):

- **用途**: 跨主机访问 (e.g. 远程 IDE, 团队共享 server, AgentForge 集成)
- **实现**: HTTP POST 接收 JSON-RPC request, SSE (`text/event-stream`) 推送 JSON-RPC response / notification
- **依赖**: 项目已 vendor `httplib` (header-only, 验证: `examples/test_http_adapter.cpp`)
- **端口**: 默认 `localhost:7333` (RFC: HydraForge 端口段 7300-7399)

**Transport 选择**:

```bash
# stdio 模式 (默认)
dsl_engine_mcp_server --stdio

# HTTP+SSE 模式
dsl_engine_mcp_server --http --port 7333 --token-file /etc/dsl_engine/token

# 双 transport 并存 (D1 默认)
dsl_engine_mcp_server --stdio --http --port 7333 --token-file /etc/dsl_engine/token
```

**为什么不选 Streamable HTTP (2025-06-18 spec 末班)**:

- Streamable HTTP 是 MCP spec 2025-06-18 引入, 2025-11-25 已稳定但生态尚未普及
- stdio + HTTP+SSE 是 MCP 2024-11-05 起双 transport, Claude Desktop / Cline / Continue.dev 全部支持
- 估时 +1 周可接受, 但 Phase 6 容量紧张 (37h/44h)
- Phase 7+ 评估 Streamable HTTP 升级

### D2. 静态 Token MVP 鉴权 (Bearer / X-MCP-Token)

**MVP 鉴权方案**: 静态 token (256-bit random), 文件存储, Bearer header 注入。

**Token 生成**:

```bash
dsl_engine_mcp_server init-token > /etc/dsl_engine/token
# 输出: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx (UUID v4)
chmod 600 /etc/dsl_engine/token
```

**Token 验证**:

- **stdio 模式**: 信任子进程, 跳过鉴权 (本地进程集成由 OS 用户权限保证)
- **HTTP+SSE 模式**: 强制鉴权
  - Header 1: `Authorization: Bearer <token>` (RFC 6750 标准)
  - Header 2: `X-MCP-Token: <token>` (MCP 生态兼容)
  - 二选一即可; 缺 token → 401 Unauthorized
  - Token 错误 → 403 Forbidden

**Token 存储**:

- 文件路径: `--token-file` 指定, 默认 `/etc/dsl_engine/token`
- 格式: 单行 UUID v4, 无前缀
- 权限: 启动时校验 mode 0600 (否则拒绝启动)
- 加载: 启动时读入内存, 不重读 (热加载 Phase 7+)

**安全约束**:

- ❌ 不允许 `--token-file /dev/stdin` (拒绝从 stdin 读, 防泄露)
- ❌ 不允许 `token=` URL query 参数 (防日志泄露)
- ✅ 允许 `--allow-no-token` 仅 stdio 模式 (开发用, 默认拒绝远程无 token)

**未来演进** (Phase 7+):

- OAuth 2.1 动态 token (估时 2-3 周)
- mTLS (估时 1 周)
- 当前静态 token 满足 MVP 需求

### D3. tools/* 暴露 — PDK 工具调用

**暴露内容**: DSL Engine 已注册到 `IToolRegistry` 的所有工具 (PDK 工具 + 标准库 subgraphs)。

**MCP 协议映射**:

| MCP method | DSL Engine adapter |
|------------|-------------------|
| `tools/list` | 返回所有 registered tools 的 metadata |
| `tools/call` | 路由到 `IToolRegistry::call_tool(name, args)` |
| `notifications/tools/list_changed` | 工具增删时主动推送 |

**tools/list 响应格式** (示例 `fs.read`):

```json
{
  "name": "fs.read",
  "description": "Read file contents",
  "inputSchema": {                              // ← ADR-0073 衔接, 零转换
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "type": "object",
    "properties": {
      "path": {"type": "string", "minLength": 1},
      "max_lines": {"type": "integer", "minimum": 0, "default": 0}
    },
    "required": ["path"],
    "additionalProperties": false
  },
  "annotations": {
    "category": "ReadOnly",
    "min_layer": "Cognitive",
    "allowed_layers": ["Cognitive", "Thinking", "Workflow"],
    "approval_required": false,
    "cost_estimate": 0.001
  }
}
```

**`annotations` 字段扩展** (MCP 2025-11-25 spec 允许自定义注解):

- `category` — ToolCategory (ReadOnly / WriteFile / Dangerous)
- `min_layer` / `allowed_layers` — LayerProfile (Cognitive / Thinking / Workflow)
- `approval_required` — ToolMetadata.approval
- `cost_estimate` — ToolMetadata.cost_estimate

**工具过滤** (ToolCategory × Layer):

- **默认**: 暴露所有 tools
- **可配置** (`--expose-tools=ReadOnly,Workflow`): 仅暴露 ReadOnly + Workflow 类
- **Mandatory filtering**: Dangerous 类必须 `approval_required=true`, 否则不暴露

**tools/call 路由**:

```
MCP client (remote)
   ↓ POST /tools/call {name: "fs.read", arguments: {...}}
DSL Engine MCP server
   ↓ 鉴权 + capability check
ToolCoordinator.execute(name, args, metadata)  // ADR-0069 EnvValidationHook 注入
   ↓
IToolRegistry::call_tool(name, args)
   ↓
ToolResult → JSON-RPC response
```

### D4. prompts/* 暴露 — DSL 生成模板

**暴露内容**: DSL 生成 Prompt 模板 (衔接 ADR-0074), 用于 MCP client (e.g. Claude Desktop) 调用 DSL 生成。

**MCP 协议映射**:

| MCP method | DSL Engine adapter |
|------------|-------------------|
| `prompts/list` | 返回已注册的 prompt 模板列表 |
| `prompts/get` | 返回单个 prompt 模板内容 + 参数 schema |
| `prompts/refresh` | 重新加载 (用于 baseline 测量后更新) |

**Prompts 类型**:

| Prompt name | 用途 | 来源 |
|-------------|------|------|
| `generate_dsl_v0` | 裸 prompt (无 schema, 无 few-shot) | ADR-0074 §决策 D3 V0 |
| `generate_dsl_v1` | + ToolMetadata V3 schema 约束 | ADR-0074 §决策 D3 V1 |
| `generate_dsl_v2` | + 5 few-shot examples | ADR-0074 §决策 D3 V2 |
| `generate_dsl_v3` | + 两阶段 subgraph 注入 | ADR-0074 §决策 D3 V3 |
| `select_subgraphs_v3` | Stage 1 subgraph 选择 (≤500 tokens) | ADR-0074 §决策 D5 |
| `evidence_gate_audit` | Wave 推进决议模板 | ADR-0074 §决策 D4 |

**prompts/get 响应** (示例 `generate_dsl_v3`):

```json
{
  "name": "generate_dsl_v3",
  "description": "Generate AgenticDSL with two-stage subgraph injection (ADR-0074 V3)",
  "arguments": [
    {
      "name": "user_input",
      "description": "Natural language task description",
      "required": true
    },
    {
      "name": "selected_subgraphs",
      "description": "Pre-selected subgraph names (from select_subgraphs_v3)",
      "required": false
    }
  ],
  "messages": [
    {
      "role": "system",
      "content": {
        "type": "text",
        "text": "You are an AgenticDSL generator. Use AgenticDSL markdown syntax to express workflows. Available subgraphs: {{selected_subgraphs}}\n\nFew-shot examples:\n[few-shot examples from ADR-0074 D1]\n\nSchema: [ToolMetadata V3 from ADR-0073]\n\nOutput: YAML DSL"
      }
    }
  ]
}
```

**Prompt 内容动态生成**:

- `few-shot examples` — 从 `data/training/2026-XX-XX-baseline-V*.jsonl` 抽取 (ADR-0074 D1)
- `Schema` — 从 ToolRegistry 当前 V3 schema 生成 (ADR-0073)
- `selected_subgraphs` — 调用方传入或服务端默认 top-3 most-used

**Prompt 版本管理**:

- 每次 baseline 测量更新 prompt 模板 → 触发 `notifications/prompts/list_changed`
- Prompt 内容 hash 用于客户端缓存 (`etag`-like)

### D5. resources/* 暴露 — stdlib 子图作为 MCP Resources

**暴露内容**: `lib/` 目录下所有 stdlib 子图 (Markdown DSL), 作为 MCP resources 提供给 client 读取/引用。

**MCP 协议映射**:

| MCP method | DSL Engine adapter |
|------------|-------------------|
| `resources/list` | 返回所有 stdlib subgraph 的 URI + metadata |
| `resources/read` | 返回单个 subgraph 的 Markdown 内容 |
| `resources/templates/list` | 返回 URI 模板 (用于动态引用) |
| `notifications/resources/list_changed` | lib/ 文件变化时推送 |

**Resources URI 方案**:

```
agenticdsl://stdlib/inference/engine.md
agenticdsl://stdlib/inference/model.md
agenticdsl://stdlib/inference/session.md
agenticdsl://stdlib/auth/oauth.md
agenticdsl://stdlib/utils/noop.md
...
```

**resources/list 响应** (示例):

```json
{
  "resources": [
    {
      "uri": "agenticdsl://stdlib/inference/engine.md",
      "name": "inference.engine",
      "description": "LLM inference engine initialization",
      "mimeType": "text/markdown",
      "annotations": {
        "category": "ReadOnly",
        "tags": ["inference", "engine"]
      }
    },
    {
      "uri": "agenticdsl://stdlib/auth/oauth.md",
      "name": "auth.oauth",
      "description": "OAuth 2.0 flow",
      "mimeType": "text/markdown",
      "annotations": {
        "category": "Dangerous",
        "tags": ["auth", "oauth"]
      }
    }
  ]
}
```

**Resource 访问控制**:

- ✅ ReadOnly / WriteFile 类 — 默认暴露
- ⚠️ Dangerous 类 — 需 `--expose-dangerous` 标志 (默认拒绝)
- ✅ 子图 metadata (tags, category) 暴露, 实现细节不暴露

**Hot-reload**:

- `lib/` 文件变化 (inotify) → 触发 `resources/list_changed` notification
- 客户端 (Claude Desktop) 收到后自动刷新

### D6. inputSchema 零转换 — ToolMetadata V3 → MCP (ADR-0073 衔接)

**核心承诺**: `ToolMetadata V3::input_schema` (ADR-0073 字段) 直接序列化为 MCP `tools/list.inputSchema`, **零字段映射**, **零手工转换**。

**转换流程**:

```cpp
// MCP server 启动时一次性构建 (缓存, 不每次重建)
nlohmann::json tool_to_mcp_input_schema(const ToolMetadata& meta) {
  if (!meta.input_schema.has_value()) {
    // 无 schema 的工具不暴露 inputSchema 字段 (MCP spec 允许)
    return nullptr;
  }
  return meta.input_schema.value();  // 直接返回, 零转换
}

// 字段映射确认 (MCP 2025-11-25 spec):
// ✅ $schema (JSON Schema 2020-12 identifier) — 1:1
// ✅ type (object/string/integer) — 1:1
// ✅ properties — 1:1
// ✅ required — 1:1
// ✅ additionalProperties — 1:1
// ✅ pattern, format, enum — 1:1
// ✅ minimum, maximum, minLength, maxLength — 1:1
// ✅ oneOf, anyOf, allOf — 1:1 (nlohmann validator 支持)
// ✅ $ref, $defs — 1:1
```

**Validation 流程** (MCP tool call):

```
MCP client → tools/call {name, arguments}
   ↓
DSL Engine MCP server
   ↓
1. 鉴权 (D2)
2. Capability check (D3)
3. ToolMetadata V3 schema validate (nlohmann validator, ADR-0073)
   ↓ 失败 → ERR_SCHEMA_VALIDATION (-32602 Invalid params)
4. ToolCoordinator.execute() (含 EnvValidationHook)
   ↓ 失败 → ERR_BACKEND_* (mapped to MCP error codes)
5. ToolResult → JSON-RPC response
```

**MCP 错误码映射** (JSON-RPC 2.0):

| DSL Engine error | JSON-RPC code | 含义 |
|------------------|:-------------:|------|
| `ERR_AUTH_REQUIRED` | -32001 | 鉴权缺失 |
| `ERR_AUTH_INVALID` | -32002 | Token 无效 |
| `ERR_TOOL_NOT_FOUND` | -32601 | Method not found |
| `ERR_SCHEMA_VALIDATION` | -32602 | Invalid params |
| `ERR_BACKEND_TIMEOUT` | -32003 | Backend timeout |
| `ERR_BACKEND_DENIED` | -32004 | Approval/hook denied |
| 其他 | -32603 | Internal error |

### D7. MCP Client 反向拉取 — 外部 Tool 注册本地 ToolCoordinator

**目标**: DSL Engine 作为 MCP client 拉取外部 MCP server 的 tools, 注册到本地 `ToolCoordinator`, 实现"组合 MCP"。

**双向角色**:

```
DSL Engine ←→ 外部 MCP Server (e.g. GitHub MCP, Slack MCP)
   │                │
   ├─ 作为 server ──┤  (D1-D5: 暴露 tools/prompts/resources)
   └─ 作为 client ──┘  (D7: 拉取外部 tools)
```

**外部 Tool 注册流程**:

1. **配置** (`--mcp-client-config <file>`):

```json
{
  "external_servers": [
    {
      "name": "github",
      "transport": "http",
      "url": "https://api.github.com/mcp",
      "token_file": "/etc/dsl_engine/github_token",
      "expose_tools": ["github.*"]
    },
    {
      "name": "slack",
      "transport": "stdio",
      "command": ["slack-mcp-server", "--stdio"],
      "expose_tools": ["slack.*"]
    }
  ]
}
```

2. **启动时拉取**:

```
DSL Engine MCP server 启动
   ↓
并行连接所有 external_servers
   ↓
对每个 server 调用 tools/list
   ↓
将外部 tools 注册到本地 ToolCoordinator
   ↓
外部 tool 在 DSL Engine 中作为本地 tool 出现
```

3. **Backend policy**:

- 外部 tool 默认 backend policy = `docker:* ephemeral` (ADR-0075 衔接)
- EnvValidationHook 自动应用 (防止外部 tool 执行恶意代码)
- 审计: 外部 tool 调用记录 `mcp.client.{request,response}` 事件 (候选主题, 注册前置: ADR-0068 §附录 A amendment)

**风险缓解**:

- ❌ **外部 tool 不暴露 Dangerous 类** (除非 `--allow-dangerous-external` 标志)
- ❌ **外部 tool 不允许递归拉取** (防 tool chain 无限)
- ✅ **外部 tool 调用 timeout 严格** (e.g. 10s)

---

### D8. Stateless 设计原则 — 无 Server-Side Session State

**核心原则**: MCP server 是**无状态服务**, 每个 request 完全独立, 不维护 server-side session state (除 stream context 外). 这是 **Phase 6b 路线图 v3 三平面架构 (Control Plane) 的核心战略原则**.

**实现要求**:

- ✅ **每个 request 独立鉴权**: 静态 token per-request (D2 已 ship, Bearer / X-MCP-Token header 注入)
- ✅ **无 server-side cache**: 除 capability metadata (仅 startup 加载一次) 外, 不维护运行时 mutable state
- ✅ **工具调用无历史依赖**: LayeredContext per-request (不跨 request 共享 context)
- ✅ **tools/list 每次重新构造**: 无 mutation, 每次返回当前快照
- ⚠️ **Streaming context 保留**: SSE 长连接期间保留 connection context (request_id + peer_addr + timestamp), 但 stream 终止后立即 GC
- ⚠️ **idempotency-key 支持 retry** (Phase 8+ 演进): 当前可选, Phase 8+ 强制要求

**为什么不维护 session state**:

1. **Horizontal scaling**: 无状态 = 多实例部署, 无 sticky session 路由要求 (Phase 7 ship 后多实例)
2. **故障恢复**: 实例崩溃不影响 in-flight request (新实例接管, 重新鉴权)
3. **A/B 测试**: 新版本可独立部署, request-level routing
4. **Prometheus metrics 简单**: 无 sticky session = 每个 instance 独立 metric, 无需汇总
5. **避免 session 污染**: 不同 client 不共享状态, 安全性提升

**与 stateful MCP 的边界** (Streamable HTTP spec 2025-06-18 演进):

| 维度 | Stateless (本 ADR) | Stateful (Streamable HTTP 2025-06-18) |
|------|-------------------|--------------------------------------|
| Session 标识 | 无 | session_id (server 分配 + client 持有) |
| 客户端状态 | 完全无 | 持有 session_id, 跨 request 复用 |
| Server 资源 | 仅 metadata (startup) | session 上下文 + 资源分配 |
| 实现复杂度 | 低 | 高 (session lifecycle + GC) |
| Horizontal scaling | ✅ 天然支持 | ⚠️ 需要 session affinity 路由 |
| 适用场景 | 控制面 capability 暴露 | 长生命周期 stream + 多步骤协议 |

**本 ADR 结论**: Phase 7 ship 期间采用 **Stateless** 模式, 与 MCP 2025-11-25 spec 兼容. Streamable HTTP (stateful) 模式留待 Phase 8+ 评估 (需 session_id 协议层支持).

**实施检查清单** (Phase 7a 验证):

- [ ] `src/services/mcp_server/` 启动时仅读取 metadata 一次 (ToolCoordinator 静态视图)
- [ ] 每个 tools/call 请求构建独立 LayeredContext (不跨 request 共享)
- [ ] SSE streaming 期间仅保留 connection-level 元数据 (request_id, peer_addr, start_time)
- [ ] SSE stream 终止立即清理 connection context (无 zombie state)
- [ ] 单元测试: 1000× 并发 tools/call 验证无 session 冲突

---

## 不变量

### 长期不变量

1. **MCP server 是控制面** — 不承担数据流 (Phase 4 gRPC 数据面 ADR-0077 推迟)
2. **MCP server 是无状态服务** — 每个 request 独立, 无 server-side session state (D8 新增)
3. **ToolMetadata V3 → MCP inputSchema 零转换** — 不允许中间层 schema 映射 (避免漂移)
4. **静态 token 仅 MVP** — 不允许扩展为 OAuth/mTLS (独立 ADR Phase 7+)
4. **stdio + HTTP+SSE 双 transport** — 不引入 Streamable HTTP (Phase 7+ 评估)
5. **5 类 MCP capability 中仅暴露 tools/prompts/resources** — sampling/elicitation 推迟 (Phase 7+)
6. **外部 MCP client tool 强制 backend policy** — 不允许直接调用 (防 sandbox 逃逸)
7. **MCP server 内部事件通过 IInteractionBus 上报** — 保持单一 observability 通道

### 鉴权不变量 (D2)

```
stdio 模式: 信任 OS 用户权限, 跳过 token 验证
HTTP+SSE 模式: 必须 token (Bearer / X-MCP-Token), 缺 → 401, 错 → 403
token 文件权限必须 0600, 否则启动失败
不允许 token URL query 参数 (防日志泄露)
```

### Schema 不变量 (D6, ADR-0073 衔接)

```
ToolMetadata V3.input_schema 必为 JSON Schema 2020-12 (MCP 兼容)
无 schema 的 tool 不暴露 inputSchema 字段 (MCP spec 允许)
schema validate 失败 → ERR_SCHEMA_VALIDATION (-32602)
不允许手工转换 schema (零转换原则)
```

---

## 风险

### 高风险

| 风险 | 缓解 |
|------|------|
| **静态 token 泄露** — token 文件被读 / URL 参数 / 日志泄露 → 远程 RCE | token 文件 0600 权限校验; 启动时检查; 拒绝 stdin 读; 拒绝 URL query; README 显式警告 (与 ADR-0071 §风险一致) |
| **外部 MCP tool 沙箱逃逸** — 外部 tool 在本地执行恶意代码 | 外部 tool 强制 backend policy (`docker:* ephemeral`); EnvValidationHook 拦截; Dangerous 类默认拒绝 |
| **MCP protocol 升级** — spec 2026-XX 变更破坏 inputSchema / tools/list | spec 版本字段 `protocolVersion`; 启动时校验 spec 兼容性; 不允许 silent upgrade |
| **MCP server 进程成为攻击目标** — 公开端口暴露被 DDoS / 0day | 默认绑定 `127.0.0.1`; `--bind 0.0.0.0` 显式声明; rate limiting (DDoS 缓解); fail2ban 集成 (D5 文档) |

### 中风险

| 风险 | 缓解 |
|------|------|
| **stdio pipe buffer 满** — MCP client 输出过大阻塞 pipe | stdout buffer 监控; 超限返回 ERR_OUTPUT_TOO_LARGE; 与 ADR-0075 LocalBackend 一致 |
| **HTTP+SSE 长连接断开** — 客户端网络抖动导致 notification 丢失 | SSE 自动重连; notification 持久化 (最近 100 条); client 启动时 `resources/list` 重拉 |
| **ToolCoordinator hook 链叠加延迟** — MCP tool call + EnvValidationHook + 业务 hook | hook 链顺序优化; benchmark target ≤100µs per hook chain |
| **Prompts 内容动态生成失败** — `data/training/*.jsonl` 文件读取失败 | 启动时校验文件存在; 失败则 prompts/get 返回 503 + 详细错误 |
| **外部 MCP server 不可用** — GitHub MCP 宕机导致 DSL Engine 启动失败 | external_servers 启动失败标记 `unhealthy`, 本地 tools 仍可用; 健康检查 60s 重试 |

### 低风险

| 风险 | 缓解 |
|------|------|
| **MCP spec 升级 → 协议破坏** — 2026-XX spec 变更 | protocolVersion 字段 + spec 兼容性测试; 升级路径独立 ADR (Phase 7+) |
| **资源 URI 冲突** — stdlib 子图 URI 与未来扩展冲突 | URI 方案固定 `agenticdsl://<category>/<name>.md`; 未来扩展走 `agenticdsl://ext/<...>` |
| **Token 轮换** — 长期使用同一 token 泄露风险高 | Phase 7+ OAuth 动态 token; 当前 MVP 文档化 "季度轮换建议" |

---

## 替代方案

### 替代 1: 不做 MCP server, 仅本地 ToolCoordinator (拒绝)

**否决理由**: 与 ADR-0050 Candidate B 决议不符; 与 ADR-0071 §战略协调 "DSL Engine as MCP Server = Candidate B 集成路径" 不兼容; 失去服务化战略价值 (但本 ADR ship 仍 gated by active-status.md §四 Candidate B 启动条件)。

### 替代 2: 仅 stdio, 不做 HTTP+SSE (拒绝)

**否决理由**: 失去远程访问能力; 团队协作场景不可用; AgentForge 集成困难; 用户明确选择双 transport。

### 替代 3: 仅 Streamable HTTP (2025-06-18 spec) (拒绝)

**否决理由**: 生态尚未普及 (MCP 2025-11-25 仍是主流); Claude Desktop / Cline 未完全适配; 估时 +1 周超容量。

### 替代 4: 全部 5 类 MCP capability (tools/prompts/resources/sampling/elicitation) (拒绝)

**否决理由**: sampling/elicitation 与 ADR-0074 Prompt baseline + Evidence Gate 重叠; 范围过宽; 估时 +2 周超容量; 用户选择仅 3 类。

### 替代 5: OAuth 2.1 动态 token (拒绝)

**否决理由**: 估时 +2 周超容量; 与 Phase 6 Solo Dev 节奏不符; Phase 7+ 独立 ADR 实施; 静态 token 满足 MVP 需求。

### 替代 6: 自研协议替代 MCP (拒绝)

**否决理由**: 维护成本 × N; 与 LLM 生态脱节 (Claude / Cline / Continue.dev 已支持 MCP); 重复造轮子。

---

## 影响范围

### 文档
- `docs/specs/mcp-server.md` (新增) — DSL Engine MCP server 契约 + 鉴权 + capability 路由
- `docs/security/mcp-token.md` (新增) — Token 生成 / 存储 / 轮换规范
- `docs/llm/mcp-prompts-catalog.md` (新增) — `prompts/*` 列表 + 用途说明
- `docs/README.md` §mcp-server (新增章节) — 用户入口

### 代码
- `src/services/mcp_server/main.cpp` (新增) — MCP server 入口 (CLI 解析 + transport 选择)
- `include/agenticdsl/services/mcp_server.h` (新增) — MCPServer 类 (transport + capability router)
- `src/services/mcp_server/transport/` (新增目录):
  - `stdio_transport.cpp` — stdin/stdout JSON-RPC (复用 LocalBackend pipe 模式)
  - `http_sse_transport.cpp` — httplib + SSE (复用 `examples/test_http_adapter.cpp`)
- `src/services/mcp_server/auth/` (新增目录):
  - `token_auth.cpp` — Bearer / X-MCP-Token 验证
  - `token_loader.cpp` — `--token-file` 加载 + 0600 校验
- `src/services/mcp_server/capabilities/` (新增目录):
  - `tools_capability.cpp` — `tools/list`, `tools/call` (D3)
  - `prompts_capability.cpp` — `prompts/list`, `prompts/get` (D4)
  - `resources_capability.cpp` — `resources/list`, `resources/read` (D5)
- `src/services/mcp_server/schema_bridge.cpp` (新增) — ToolMetadata V3 ↔ MCP inputSchema (D6, 零转换)
- `src/services/mcp_server/client/` (新增目录):
  - `external_mcp_client.cpp` — 外部 MCP server 连接 + tools/list 拉取 (D7)

### 测试
- `tests/test_mcp_server_stdio.cpp` (新增) — stdio transport JSON-RPC round-trip
- `tests/test_mcp_server_http_sse.cpp` (新增) — HTTP+SSE transport + SSE notification
- `tests/test_mcp_auth.cpp` (新增) — token 验证 + 错误码 + 0600 权限校验
- `tests/test_mcp_tools_capability.cpp` (新增) — tools/list + tools/call 路由
- `tests/test_mcp_prompts_capability.cpp` (新增) — prompts/list + prompts/get (含 dynamic content)
- `tests/test_mcp_resources_capability.cpp` (新增) — resources/list + resources/read (含 hot-reload)
- `tests/test_mcp_schema_bridge.cpp` (新增) — ToolMetadata V3 → MCP inputSchema round-trip (零转换验证)
- `tests/test_mcp_external_client.cpp` (新增) — 外部 MCP server 连接 + tool 注册 backend policy
- `tests/test_mcp_error_codes.cpp` (新增) — 错误码映射 (-32602 等)

### 事件 (ADR-0068 衔接)
- 设计 4 个候选 server 幻影主题 (注册前置: ADR-0068 §附录 A amendment): `mcp.server.connected`, `mcp.server.disconnected`, `mcp.server.request`, `mcp.server.response`
- 设计 2 个候选 client 主题 (注册前置: ADR-0068 §附录 A amendment): `mcp.client.request`, `mcp.client.response`
- Payload schema 标准化

### 生态
- `examples/mcp_client_demo/` (新增) — 示例 MCP client (Python stdlib + httplib) 调用 DSL Engine
- `examples/mcp_server_demo/` (新增) — 启动 DSL Engine as MCP server 的 CLI 示例
- `lib/` 12 个 stdlib subgraphs — 自动暴露为 resources (D5)
- `pdk/` 12+ 工具 — 自动暴露为 tools (D3)

---

## 后续

### 短期 (Wave 3 末启动后 1 周内)

1. 创建 `src/services/mcp_server/` 目录骨架
2. 实施 stdio transport + JSON-RPC 2.0 解析 (nlohmann_json 已 vendor)
3. 实施 token auth + `--token-file` 加载
4. 实施 `tools/list` + `tools/call` capability (D3, ToolCoordinator.execute 复用)
5. 基础单元测试 (stdio round-trip + token 验证)

### 中期 (Wave 3 末准出前)

6. 实施 HTTP+SSE transport (httplib 已 vendor)
7. 实施 `prompts/list` + `prompts/get` (D4, 衔接 ADR-0074 baseline 数据)
8. 实施 `resources/list` + `resources/read` (D5, hot-reload lib/)
9. 实施外部 MCP client (D7, 外部 tool 注册 ToolCoordinator)
10. CI 测试覆盖: MCP spec 2025-11-25 conformance + JSON-RPC 2.0 + inputSchema round-trip

### Wave 3 末衔接

11. ADR-0051 spike 产出 (`docs/service-composition/`) 集成到 MCP server
12. ADR-0074 baseline 测量工具 (`tools/measure_prompt_baseline.cpp`) 与 `prompts/*` 同步
13. ADR-0075 LocalBackend 架构复用 stdio transport

### Phase 6b 衔接 (Sprint 25, 2026-08-05 ~ 2026-08-19)

14. `examples/pdk_chat_demo` 升级为 MCP client (D7) — 拉取外部 tool 演示组合能力
15. AgentForge 第 2 agent 通过 MCP server 调用 DSL Engine

### Phase 7+ 推迟 (估时 +4 周)

16. Streamable HTTP transport (MCP 2025-06-18 spec)
17. OAuth 2.1 动态 token (估时 2-3 周)
18. sampling / elicitation capability (估时 2-3 周)
19. mTLS / rate limiting / fail2ban (估时 1 周)

---

## 复审节点

- **Wave 3 末 MCP server MVP ship 时**: 本 ADR 状态从 🔍 Proposed → 🟡 Partial (D1+D2+D3 已实施)
- **Wave 3 末 prompts + resources ship 时**: 本 ADR 状态保持 🟡 Partial (D4+D5 已实施)
- **Wave 3 末外部 MCP client ship 时**: 本 ADR 状态从 🟡 Partial → ✅ Approved (D1-D7 全部 ship)
- **Phase 7+ OAuth 升级 ship 时**: 独立 ADR 立项; 本 ADR 仅追加 §后续

---

*文档版本: v1.0*
*创建日期: 2026-08-03*
*作者: HydraForge 架构组*
*状态: 🔍 Proposed (Wave 3 末 ADR; INTEGRATES WITH Phase 6 Candidate B (gated by active-status.md §四); stdio + HTTP+SSE; 静态 token; 衔接 ADR-0073/0074/0075; 待架构组评审)*