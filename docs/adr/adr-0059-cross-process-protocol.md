# ADR-0059: 跨进程/跨网络 Agent 协议

## 状态

✅ Approved (2026-07-16, 架构评审确认)
✅ Updated (2026-07-16, 与 ADR-0060 的 6 种协作模式对齐)

## 领域

Agent-as-Plugin 架构 / 跨进程通信

## 关联

- [ADR-0060 — Agent 组合协议](./adr-0060-agent-composition.md) — 定义 6 种协作模式
- [ADR-0019 — IInteractionBus](../adr-0019-iinteraction-bus-mvp.md) — 进程内事件总线
- [ADR-0053 — AgentDescriptor](./adr-0053-agent-descriptor-interface.md) — Agent 注册
- [ADR-0054 — Capability Discovery](./adr-0054-capability-discovery.md) — Agent 发现
- [ADR-0058 — Tool Schema Validation](./adr-0058-tool-schema-validation.md) — 契约校验

## 背景

### 问题

当前所有 Agent 都必须在**同一进程**内运行，通过进程内 `call_tool` 通信。这限制了：

- 分布式部署（Agent 运行在不同机器上）
- 外部生态接入（MCP / A2A 协议的第三方 Agent）
- 语言无关性（其他语言写的 Agent 无法接入）
- Agent Marketplace（远程安装的 Agent）

ADR-0059 v1 只关注 `call_tool` → MCP `tools/call` 的一对一映射。ADR-0060 扩展为 6 种协作模式后，ADR-0059 需要对齐：

| 模式 | 进程间映射 |
|------|-----------|
| ① call（同步 RPC） | MCP `tools/call` |
| ② call_async（异步 RPC） | MCP `tools/call` + notifications |
| ③ emit/subscribe | MCP `notifications` |
| ④ delegate（子 Agent） | MCP `tasks/create` + `tasks/get` |
| ⑤ parallel（fork/join） | MCP N × `tasks/create` |
| ⑥ stream（流式） | MCP SSE streaming |

### 目标

定义 `RemoteAgentAdapter`：把进程内协作 API 透明地翻译为 MCP/A2A 协议。

## 决策

### 决策 1 — 透明路由：IToolRegistry 统一后端选择

```
调用方代码（不变）:
  auto result = call_tool("loop/run", {prompt, tools});
  
IToolRegistry::call_tool(name, args):
  1. CapabilityRegistry.query(name) → 得到 agent_id + metadata
  2. 判断 backend 类型:
     ├─ 本地 PDK Plugin (C++)
     ├─ 本地 SKILL
     ├─ 本地 Wasm
     └─ 远程 RemoteAgent（新增）
  3. 本地：直接调用
  4. 远程：RemoteAgentAdapter::call_remote()
  5. 返回 ToolResult
```

**关键设计**：调用方对 backend 完全无感。

### 决策 2 — RemoteAgentAdapter 接口

```cpp
class RemoteAgentAdapter {
public:
    // 注册远程 Agent（加载 manifest + transport 配置）
    ErrorCode register_remote(
        const std::string& agent_id,
        const RemoteTransportConfig& transport
    );
    
    // 注销
    void unregister_remote(const std::string& agent_id);
    
    // ① 同步 RPC
    ToolResult call_remote(
        const std::string& agent_id,
        const std::string& tool_name,
        const nlohmann::json& args
    );
    
    // ② 异步 RPC
    std::string call_async_remote(
        const std::string& agent_id,
        const std::string& tool_name,
        const nlohmann::json& args,
        std::function<void(ToolResult)> callback
    );
    // 返回 request_id 用于关联响应
    
    // ④ 子 Agent 委派
    std::string delegate_remote(
        const SubAgentSpec& spec,
        std::function<void(SubAgentEvent)> monitor
    );
    
    // ⑤ 并行执行
    std::vector<ToolResult> parallel_remote(
        const std::string& agent_id,
        const std::string& tool_name,
        const std::vector<nlohmann::json>& task_args,
        const ParallelOptions& options
    );
    
private:
    std::unordered_map<std::string, std::unique_ptr<McpClient>> connections_;
    std::unordered_map<std::string, std::function<void(ToolResult)>> pending_callbacks_;
};
```

### 决策 3 — 协议映射（5 种协作模式 + 1 Phase 2）

| 协作模式 | 进程内实现 | MCP 协议映射 | A2A 协议映射 |
|---------|-----------|-------------|--------------|
| **① 同步 RPC** | `IToolRegistry::call_tool` | `tools/call` → 阻塞等 `result` | `message/send` → 等 `Task` |
| **② 异步 RPC** | `emit` + `subscribe` request_id | `tools/call` + `notifications/tools/callback` | `message/stream` + `TaskStatusUpdateEvent` |
| **③ pub/sub** | `IInteractionBus` | `notifications/*` | `TaskArtifactUpdateEvent` |
| **④ 子 Agent** | `SubtaskSession` | `tasks/create` + `tasks/get` (轮询 or notification) | `Task` 对象 + `TaskStatus` |
| **⑤ parallel** | `DomainWorkerPool` | N × `tasks/create` + 等所有完成 | N × `message/send` |
| **⑥ stream** (Phase 2) | 流式 Bus | SSE + `notifications/sse` | `message/stream` (SSE) |

**v1 优先 MCP**：生态最广、与 `call_tool` 模型最匹配。
**Phase 2 加 A2A**：跨框架 Agent 协调。

### 决策 4 — transport 配置

```cpp
struct RemoteTransportConfig {
    std::string protocol;                       // "mcp" / "a2a"
    TransportType type;                         // stdio / http / sse / websocket
    std::string endpoint;                       // URL 或命令行
    std::string auth_token;                     // 可选认证
    std::chrono::milliseconds timeout_ms{30000};
    std::string agent_id;                       // 远程 Agent 标识
    
    // 安全限制（远程 Agent 默认 untrusted）
    struct {
        bool verify_schema = true;              // 输入/输出 schema 校验
        uint32_t max_concurrent = 4;            // 最大并发
        double budget_limit_usd = 1.0;          // 远程调用预算上限
        std::vector<std::string> allowed_tools; // 白名单工具
    } security;
};

enum class TransportType {
    Stdio,      // 启动子进程，stdin/stdout 通信
    HTTP,       // HTTP POST + JSON-RPC
    SSE,        // Server-Sent Events (用于流式)
    WebSocket   // 双向流式
};
```

**默认**：远程 Agent `trust_level = "untrusted"`，必须 schema 校验。

### 决策 5 — 安全边界

```cpp
// 远程 Agent 默认是 untrusted
RemoteTransportConfig defaults:
  - trust_level: "untrusted"
  - verify_schema: true (输入/输出)
  - max_concurrent: 4
  - budget_limit_usd: 1.0
  - allowed_tools: 必须显式声明（白名单）
  - require_approval: high-risk 操作需要本地审批
```

**远程调用的安全检查**：
1. 调用方 check `allowed_tools`
2. 调用方 check `max_concurrent`（限流）
3. 调用方 check `budget_limit_usd`（防止远程 Agent 大量消耗本地预算）
4. 远程响应必须 schema 校验（防止恶意 Agent 返回伪造数据）
5. 超时必须 SIGKILL / disconnect

### 决策 6 — MCP 协议实现（v1 优先）

**MCP JSON-RPC 2.0 over stdio/http/sse**：

```json
// request: ① 同步 RPC
{
  "jsonrpc": "2.0",
  "id": "req-001",
  "method": "tools/call",
  "params": {
    "name": "code_review/run",
    "arguments": {"code": "...", "language": "cpp"}
  }
}

// response
{
  "jsonrpc": "2.0",
  "id": "req-001",
  "result": {
    "content": [{"type": "text", "text": "{...}"}],
    "isError": false
  }
}
```

```json
// ② 异步 RPC: 同 tools/call, 加上 notification 回调
{
  "jsonrpc": "2.0",
  "method": "notifications/progress",
  "params": {
    "requestId": "req-001",
    "progress": {"tokensUsed": 1500, "currentStep": 3}
  }
}
```

```json
// ④ 子 Agent: tasks/create
{
  "jsonrpc": "2.0",
  "id": "task-001",
  "method": "tasks/create",
  "params": {
    "agent": "code.review",
    "input": {"code": "...", "language": "cpp"}
  }
}

// tasks/get (轮询)
{
  "jsonrpc": "2.0",
  "id": "task-001",
  "method": "tasks/get",
  "params": {"taskId": "task-001"}
}

// response: status: running / completed / failed
```

### 决策 7 — 与 ADR-0060 的协作模式对齐

```
进程内 API:                    进程间 (ADR-0059):
─────────────────              ──────────────────
call_tool()            →       RemoteAgentAdapter::call_remote()
                                → MCP tools/call

call_async()           →       RemoteAgentAdapter::call_async_remote()
                                → MCP tools/call + notifications

emit()                 →       RemoteAgentAdapter::publish_remote()
                                → MCP notifications

delegate()             →       RemoteAgentAdapter::delegate_remote()
                                → MCP tasks/create + tasks/get

parallel()             →       RemoteAgentAdapter::parallel_remote()
                                → MCP N × tasks/create

open_stream()          →       RemoteAgentAdapter::open_stream_remote()
                                → MCP SSE (Phase 2)
```

**调用方代码完全一致**——`IToolRegistry::call_tool` 透明分发。

## 替代方案

### 方案 A：每种协作模式独立实现

**否决理由**：
- 大量重复代码（每种模式都要写本地/远程两个版本）
- 维护成本高
- API 形状不统一

### 方案 B：只支持 MCP，不考虑 A2A

**否决理由**：
- A2A 在跨框架场景下有价值（Phase 2）
- 未来 A2A 可能成为主流
- 设计阶段预留接口比事后重构好

### 方案 C：不隔离远程调用，与进程内等价

**否决理由**：
- 安全风险（远程 Agent 不可信）
- 性能差异（远程调用 ms-秒级，本地 μs 级）
- 需要限流、预算、超时机制

## 不变量

- `IToolRegistry::call_tool` 对调用方透明，不暴露 backend
- 远程 Agent 默认 `untrusted`，必须 schema 校验 + 限流 + 预算
- 协议映射是同步的：v1 MCP + 进程内 / Phase 2 A2A + 进程间 streaming
- 安全检查统一在 `IToolRegistry` 入口，不在后端内部

## 权衡

| 决策 | 选择 | 理由 |
|------|------|------|
| 后端选择 | `IToolRegistry` 透明路由 | 调用方无感 |
| 协议 v1 | MCP | 生态最广 |
| 安全 | 默认 untrusted | 远程不可信 |
| Phase 2 协议 | A2A + SSE | 跨框架、流式 |

## 后续行动

- 实现 `RemoteAgentAdapter` 与 `McpClient`
- 集成到 `IToolRegistry::call_tool` 的透明路由
- 添加测试 `tests/test_remote_adapter.cpp`
- Phase 2: A2A 协议支持 + SSE streaming

## 参考

- [ADR-0060 — Agent 组合协议](./adr-0060-agent-composition.md)
- [ADR-0053 — AgentDescriptor](./adr-0053-agent-descriptor-interface.md)
- [ADR-0058 — Tool Schema Validation](./adr-0058-tool-schema-validation.md)
- MCP 2026-07-28 RC: `blog.modelcontextprotocol.io/posts/2026-07-28-release-candidate/`
- MCP Tasks Extension: 远程任务管理
- A2A Protocol: `github.com/a2a/spec`