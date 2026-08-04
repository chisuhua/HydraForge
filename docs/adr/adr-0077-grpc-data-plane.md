# ADR-0077: gRPC Data Plane (High-Throughput Channels)

## 状态

🔍 Proposed (2026-08-03 — 派生自 ADR-0071 §决策 D8, **Wave 4 Phase 4 descoped**; docs-only 未来设计 ADR; 实施 Phase 7+ 重新评估; 衔接 ADR-0076 MCP 控制面; 待架构组评审; 估时 2-3 周实施待 Phase 7 重新评估)

## 领域

L1 OS Services / 数据面传输 / 高吞吐通道 / 跨进程协议 / Phase 4 推迟 / 性能边界

## 关联

### 父 ADR
- [ADR-0071 — LLM-native AgenticDSL 架构](./adr-0071-llm-native-agenticdsl-architecture.md) §决策 D8 (本 ADR 是 D8 的具体实施: gRPC + protobuf 数据面)
- ADR-0071 §决策 D7 (本 ADR 与 D7 MCP 控制面对称: MCP 是控制面, gRPC 是数据面)

### 上游锚定
- [ADR-0019 — IInteractionBus MVP](./adr-0019-iinteraction-bus-mvp.md) — gRPC Telemetry service 与 IInteractionBus 事件互通
- [ADR-0030 — 异步运行时 V2](./adr-0030-async-runtime-v2.md) — 异步 stream (双向 stream / half-close) 复用 V2 runtime
- [ADR-0068 — 事件发射契约](./adr-0068-event-emission-contract.md) — `grpc.*` 事件主题 (D5 新增)
- [ADR-0073 — Tool JSON Schema 契约](./adr-0073-tool-json-schema-contract.md) — BlobTransfer / RemoteExecutor payload 用 ToolMetadata V3 schema 校验
- [ADR-0075 — EnvBackend Local + Docker](./adr-0075-env-backend-local-docker.md) — RemoteExecutor 是 EnvBackend 抽象的 gRPC 实现
- [ADR-0076 — DSL Engine as MCP Server](./adr-0076-dsl-engine-mcp-server.md) — MCP/gRPC 路由规则 (D2 payload 阈值); MCP = 控制面; gRPC = 数据面

### 平行/下游
- ADR-0078 (Fine-tune 基模, Wave 5+, AgenticMind) — LLMDataPlane.StreamTokens 是 Fine-tune 数据采集底层

### 规范
- [gRPC C++ Tutorial](https://grpc.io/docs/languages/cpp/) — `grpc-cpp` 库 (vendored or system)
- [Protocol Buffers v3](https://protobuf.dev/) — IDL + 序列化 (`protoc` + `protobuf-cpp`)
- [gRPC Service Configuration](https://github.com/grpc/grpc-proto/blob/master/grpc/service_config/service_config.proto) — 负载均衡 / 超时 / 重试配置
- [`docs/specs/dsl.md`](../specs/dsl.md) v3.10 — DSL 规范 (本 ADR 不变更)

---

## 背景

### 问题

ADR-0076 (MCP Server) 提供 **控制面** (capability 暴露: tools/prompts/resources 调用), 但 **数据面** (high-throughput payload) 仍缺失。具体 5 个空白:

1. **LLM token 流无独立通道** — MCP `tools/call` 一次性返回, 长 prompt 输出阻塞; 无法 server-streaming 多 client
2. **大文件传输无优化路径** — 模型权重 / 数据集上传走 MCP JSON-RPC base64 编码, 体积 ×1.33 + 解析开销大
3. **远程执行受限于 stdio / SSH** — EnvBackend Local + Docker (ADR-0075) 是本地或 Docker daemon; 跨主机执行无统一抽象
4. **OTel trace/span 上报无独立通道** — 当前走 IInteractionBus (in-process); 分布式部署需独立上报通道
5. **MCP 与 high-throughput 边界模糊** — 何时用 MCP, 何时用 gRPC 无明确规则, 易产生架构漂移

### 解决方案

引入 **gRPC Data Plane** 作为 high-throughput 通道, 与 ADR-0076 MCP 控制面对称。4 个核心 service + 明确的 MCP/gRPC 路由规则:

```
┌─────────────────────────────────────────────────────────────┐
│                  DSL Engine Dual-Plane                       │
│                                                               │
│  ┌────────────────────────────────────────────┐              │
│  │  Control Plane: MCP (ADR-0076)              │              │
│  │  - tools/list, tools/call (元数据 + 调用)   │              │
│  │  - prompts/list, prompts/get                │              │
│  │  - resources/list, resources/read           │              │
│  │  - 鉴权: 静态 token MVP                     │              │
│  └────────────────────────────────────────────┘              │
│                          ↑↓ 路由规则                          │
│  ┌────────────────────────────────────────────┐              │
│  │  Data Plane: gRPC (本 ADR)                  │              │
│  │  - LLMDataPlane (token stream)              │              │
│  │  - BlobTransfer (大文件 / 模型权重)         │              │
│  │  - RemoteExecutor (远程执行)                │              │
│  │  - Telemetry (OTel trace/span)              │              │
│  │  - 鉴权: mTLS Phase 2 (TBD)                │              │
│  └────────────────────────────────────────────┘              │
└─────────────────────────────────────────────────────────────┘
```

### 已实证证据

- **ADR-0071 §D8 已定义 4 services + 路由规则**: LLMDataPlane / BlobTransfer / RemoteExecutor / Telemetry + payload 64KB 阈值
- **grpc-cpp + protobuf 成熟生态**: 项目 external/ 已有 protobuf 类似依赖 (yaml-cpp / nlohmann_json); grpc-cpp vendor 成本 ~10MB 二进制
- **ADR-0075 LocalBackend 模式可复用**: RemoteExecutor 复用 fork+exec, 仅替换 transport (stdio → gRPC stream)
- **现有 IInteractionBus (ADR-0019)**: Telemetry 服务复用 in-process event bus, gRPC 上报是分布式扩展
- **MCP 2025-11-25 spec**: MCP 不支持双向 stream + 大 payload; gRPC 填补数据面

---

## 决策

### D1. 4 个核心 gRPC Service — LLMDataPlane / BlobTransfer / RemoteExecutor / Telemetry

```protobuf
syntax = "proto3";
package agenticdsl.v1;

import "google/protobuf/timestamp.proto";
import "google/protobuf/struct.proto";

// ===== D1.1 LLMDataPlane: LLM token 流 =====
service LLMDataPlane {
  // 单向 server-streaming: client 发请求, server 流式返回 token chunks
  rpc StreamTokens(LLMStreamRequest) returns (stream TokenChunk);

  // 双向 streaming: client 流式输入 (multi-turn), server 流式响应
  rpc BidirectionalStream(stream LLMStreamChunk) returns (stream LLMStreamChunk);

  // 取消流 (强制终止)
  rpc CancelStream(StreamCancel) returns (StreamCancelAck);
}

message LLMStreamRequest {
  string model = 1;
  string prompt = 2;
  google.protobuf.Struct params = 3;       // max_tokens, temperature, etc.
  string request_id = 4;
}

message TokenChunk {
  string request_id = 1;
  string token = 2;
  uint32 token_index = 3;
  bool is_final = 4;
  google.protobuf.Timestamp timestamp = 5;
  google.protobuf.Struct usage = 6;        // prompt_tokens, completion_tokens
}

message LLMStreamChunk {
  oneof payload {
    LLMStreamRequest request = 1;
    TokenChunk response = 2;
  }
}

message StreamCancel {
  string request_id = 1;
  string reason = 2;
}

message StreamCancelAck {
  bool cancelled = 1;
  uint32 tokens_emitted = 2;
}

// ===== D1.2 BlobTransfer: 大文件 / 模型权重 =====
service BlobTransfer {
  // Client-streaming upload
  rpc Upload(stream BlobChunk) returns (BlobResult);

  // Server-streaming download
  rpc Download(BlobRequest) returns (stream BlobChunk);

  // 列出可用 blob (模型清单)
  rpc ListBlobs(BlobListRequest) returns (BlobList);
}

message BlobChunk {
  oneof payload {
    BlobMetadata metadata = 1;    // 仅 first chunk
    bytes data = 2;               // 后续 chunks
  }
  uint64 offset = 3;
  uint32 chunk_size = 4;
}

message BlobMetadata {
  string blob_id = 1;
  string content_type = 2;        // "model/gguf", "dataset/jsonl"
  uint64 total_size = 3;
  string sha256_hash = 4;         // 校验
}

message BlobRequest {
  string blob_id = 1;
  uint64 offset = 2;              // 支持断点续传
  uint64 length = 3;              // 0 = 全部
}

message BlobResult {
  string blob_id = 1;
  uint64 bytes_transferred = 2;
  string sha256_hash = 3;
  bool success = 4;
  string error_message = 5;
}

message BlobListRequest {
  string content_type_filter = 1;  // e.g. "model/gguf"
  uint32 limit = 2;
}

message BlobList {
  repeated BlobMetadata blobs = 1;
  uint32 total_count = 2;
}

// ===== D1.3 RemoteExecutor: 远程执行 (EnvBackend gRPC 实现) =====
service RemoteExecutor {
  // 双向 streaming: client 流式 stdin, server 流式 stdout/stderr
  rpc Exec(stream ExecCommand) returns (stream ExecOutput);

  // 单次执行 (无 stdin)
  rpc ExecOnce(ExecRequest) returns (ExecResult);
}

message ExecCommand {
  oneof payload {
    ExecRequest init = 1;          // 仅 first message
    bytes stdin_chunk = 2;         // 后续 messages
    ExecSignal signal = 3;         // SIGTERM / SIGKILL
  }
}

message ExecRequest {
  string backend = 1;              // "local" | "docker:container_id" (ADR-0075 复用)
  string cmd = 2;
  repeated string args = 3;
  map<string, string> env_vars = 4;
  string working_dir = 5;
  int32 timeout_ms = 6;
  uint64 max_output_bytes = 7;
  string request_id = 8;
}

message ExecOutput {
  oneof payload {
    ExecChunk chunk = 1;           // stdout / stderr chunks
    ExecResult result = 2;         // 仅 final message
  }
}

message ExecChunk {
  enum StreamType { STDOUT = 0; STDERR = 1; }
  StreamType stream = 1;
  bytes data = 2;
  uint64 offset = 3;
}

message ExecResult {
  int32 exit_code = 1;
  string stdout_buf = 2;          // 仅 ExecOnce 返回 (无 streaming)
  string stderr_buf = 3;
  int64 duration_ms = 4;
  bool timed_out = 5;
  string error_code = 6;          // ERR_BACKEND_* (ADR-0075 D2/D3 错误码复用)
  string request_id = 7;
}

message ExecSignal {
  int32 signum = 1;               // 15 (SIGTERM), 9 (SIGKILL)
}

// ===== D1.4 Telemetry: OTel trace/span 上报 =====
service Telemetry {
  // Client-streaming 批量上报 (高效)
  rpc PushMetrics(stream Metric) returns (Ack);

  // Server-streaming 实时 trace subscription (推送重要事件)
  rpc SubscribeTraces(TraceFilter) returns (stream TraceEvent);
}

message Metric {
  string name = 1;                // "llm.tokens_per_second", "tool.exec.duration_ms"
  double value = 2;
  google.protobuf.Timestamp timestamp = 3;
  map<string, string> labels = 4; // {"model": "gpt-4", "tool": "fs.read"}
  string trace_id = 5;            // OTel W3C trace context
  string span_id = 6;
}

message Ack {
  uint32 accepted = 1;
  uint32 rejected = 2;
  string error_message = 3;
}

message TraceFilter {
  string trace_id_prefix = 1;
  map<string, string> label_filter = 2;
  uint32 min_severity = 3;        // 0=debug, 9=critical
}

message TraceEvent {
  string trace_id = 1;
  string span_id = 2;
  string parent_span_id = 3;
  string name = 4;
  uint32 severity = 5;
  google.protobuf.Timestamp start_time = 6;
  google.protobuf.Timestamp end_time = 7;
  map<string, string> attributes = 8;
  repeated TraceEvent children = 9;
}
```

### D2. MCP/gRPC 路由规则 — payload 64KB 阈值 (per ADR-0071 §D8)

**核心规则**:

| 条件 | 路由 | 理由 |
|------|------|------|
| `payload < 64KB && !streaming` | **MCP** | MCP JSON-RPC 适合小 payload; 静态 token 鉴权简单 |
| `payload >= 64KB \|\| streaming` | **gRPC** | gRPC 高效序列化 + 流式; 大 payload / streaming 性能优势 |

**决策树**:

```
DSL 节点调用
    │
    ├─ streaming=true?
    │   ├─ 是 → gRPC (强制)
    │   └─ 否 ↓
    │
    └─ payload size 预估
        ├─ < 64KB → MCP
        ├─ 64KB-1MB → gRPC (JSON 序列化开销 vs protobuf 二进制优势)
        └─ > 1MB → gRPC (强制)
```

**特殊豁免**:

- `BlobTransfer` 始终走 gRPC (设计如此, 不论 payload)
- `Telemetry` 始终走 gRPC (high-frequency metrics)
- `LLMDataPlane.StreamTokens` 始终走 gRPC (server-streaming 强制)
- 工具调用 `payload` 含 base64 编码字段 → 视为"原始大小" (解码前), 用阈值判断

**实现位置**: ToolCoordinator.execute() 路由决策 (D7 衔接)

### D3. 鉴权 — mTLS Phase 2 (TBD), MVP 静态 Token 复用

**MVP 鉴权** (Phase 7 实施时):

- **复用 ADR-0076 静态 token 机制**: `Authorization: Bearer <token>` header (gRPC metadata)
- **gRPC metadata 注入**: client 在每个 RPC 调用注入 `authorization: Bearer <token>`
- **server interceptor 验证**: 与 MCP server 共享 token file

**Phase 2 升级** (Phase 8+, 估时 2-3 周):

- **mTLS**: client/server 证书双向验证
- **SPIFFE/SPIRE**: 分布式身份 (K8s 场景)
- **OAuth 2.1 动态 token**: 复用 ADR-0076 OAuth 升级

**当前 ADR 状态**: 静态 token MVP 复用 ADR-0076, mTLS Phase 2 推迟。

### D4. protobuf + grpc-cpp 集成

**依赖**:

- `protobuf` (vendored 或 system) — `apt install libprotobuf-dev protobuf-compiler`
- `grpc-cpp` (vendored 或 system) — `apt install libgrpc++-dev protobuf-compiler-grpc`
- `grpc-cpp-generator` (protoc 插件) — 代码生成

**当前项目状态**:

- `external/` 已有: `yaml-cpp`, `nlohmann_json`, `inja`, `llama.cpp`
- **需新增**: `protobuf`, `grpc-cpp` (vendored)
- 评估: vendored vs system (Phase 7 评估时决策)

**构建集成**:

```cmake
# tests/data/proto/CMakeLists.txt (示例)
find_package(protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS agenticdsl.proto)
protobuf_generate_grpc(PROTO_GRPC_SRCS PROTO_GRPC_HDRS agenticdsl.proto)

add_library(agenticdsl_grpc_proto STATIC
  ${PROTO_SRCS} ${PROTO_HDRS}
  ${PROTO_GRPC_SRCS} ${PROTO_GRPC_HDRS}
)
target_link_libraries(agenticdsl_grpc_proto
  PUBLIC protobuf::libprotobuf gRPC::grpc++ gRPC::grpc
)
```

**代码生成**:

```bash
protoc --cpp_out=src/common/grpc \
       --grpc_out=src/common/grpc \
       --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` \
       -I tests/data/proto \
       tests/data/proto/agenticdsl.proto
```

### D5. Event Bus 衔接 — 4 个新 grpc.* 事件主题

**目标**: gRPC service 内部事件通过 IInteractionBus 上报, 统一 observability (ADR-0068 衔接)。

**4 个新幻影主题**:

| 主题 | 触发 | Payload |
|------|------|---------|
| `grpc.stream.start` ⚠️ pending | gRPC stream 建立 | `{service, method, peer_addr, request_id}` |
| `grpc.stream.chunk` ⚠️ pending | stream chunk 发送/接收 | `{service, method, chunk_index, bytes}` |
| `grpc.stream.end` ⚠️ pending | stream 终止 (成功/失败) | `{service, method, request_id, duration_ms, error_code?}` |
| `grpc.connection.{up,down}` ⚠️ pending | TCP 连接建立/断开 | `{peer_addr, tls?, error?}` |

**Payload schema 标准化**: 与 ADR-0068 EventBuilder 兼容 (使用 `.args(json)` + `.meta(json)`).

### D6. RemoteExecutor 与 EnvBackend 集成 (ADR-0075 衔接)

**目标**: EnvBackend 抽象新增 `GRPCBackend` 实现, 走 gRPC 远程执行。

**新增实现**:

```cpp
class GRPCBackend : public IEnvBackend {
public:
  GRPCBackend(const GRPCBackendConfig& config);
  ExecResult exec(const ExecRequest& req, const ExecOptions& opts) override;
  std::string name() const override { return "grpc:" + peer_addr_; }
  BackendCapabilities capabilities() const override {
    return {.supports_isolation = true, .supports_persistent_fs = false, .max_concurrent_execs = 64};
  }
private:
  std::string peer_addr_;        // e.g. "remote-host:50051"
  std::unique_ptr<RemoteExecutor::Stub> stub_;
  // gRPC bidirectional streaming: Exec(stream ExecCommand) returns (stream ExecOutput)
};
```

**DSL `backend:` 字段支持**:

```yaml
- type: shell.exec
  cmd: "kubectl get pods"
  backend: "grpc:remote-k8s-cluster:50051"  # GRPCBackend
  timeout: 30000
```

**安全约束** (与 ADR-0075 一致):

- EnvValidationHook 强制 backend policy
- Dangerous 类必须 approval_granted
- mTLS Phase 2 启用 (D3)

### D7. ToolCoordinator 路由决策 (D2 阈值执行点)

**目标**: ToolCoordinator.execute() 根据 payload size + streaming 自动选择 MCP 或 gRPC。

**伪代码**:

```cpp
ToolResult ToolCoordinator::execute(const ToolCall& call) {
  // 1. Pre-hook (EnvValidationHook, ADR-0069)
  // ...

  // 2. 路由决策 (本 ADR D2)
  if (call.stream || estimated_payload_size(call) >= 64 * 1024) {
    return execute_via_grpc(call);   // gRPC path
  } else {
    return execute_via_mcp(call);    // MCP path (现有)
  }
}

size_t estimated_payload_size(const ToolCall& call) {
  // 启发式: args JSON 序列化大小 + 已知 tool output 上限
  return serialize_args(call.arguments).size() + call.tool_metadata.estimated_output_size;
}
```

**fallback**: gRPC 不可用 (服务未启动) → 降级到 MCP + warn event (`grpc.unavailable.fallback`)

---

### D8. Phase 8a 启动条件 — 依赖链与评估触发 (路线图 v3 三平面架构)

**核心原则**: gRPC Data Plane 是 **依赖 Execution Plane + Control Plane ship** 的最末平面. 必须三平面演进链完整才能启动 Phase 8a.

**依赖链** (per 路线图 v3 Three-Plane Architecture):

```
Execution Plane (Phase 6b/6c)  ──┐
                                  ├─→  Control Plane (Phase 7, gated) ──→  Data Plane (Phase 8a, gated)
                                  │
Phase 6c ship 后: 评估 C10        │      Phase 7 ship ≥3 月后: 评估本 D8
```

**Phase 8a 实施触发条件** (任一满足, per 路线图 v3 Phase 8a 启动条件):

| 启动条件 | 阈值 | 当前状态 (2026-08-03) | 评估时点 |
|---------|------|------------------|---------|
| **Phase 7 ship ≥3 个月** | MCP server 稳定运行 (零 critical bug ≥90 天) | ⏸ Phase 7 启动评估中 | 2026-12+ |
| **MCP/gRPC 路由阈值实测校准需求** | 64KB 启发式不准, 真实数据驱动阈值 | ⏸ 待 Phase 7 ship 后 | Phase 7 ship 后 |
| **分布式部署需求出现** | K8s / multi-region 部署需求 | ❌ 当前单实例 | 外部触发 |
| **LLMDataPlane 高频需求** | Fine-tune 数据采集 > 100 events/s | ❌ 当前 <10 events/s | AgenticMind ship |

**评估决策树** (Phase 7 ship 90 天后):

```
Phase 8a 启动评估 (Sprint N+6):
  ├─ Phase 7 ship ≥3 个月? ──── ✅/❌
  │   ├─ ❌ → 推迟至下次评估 (默认季度)
  │   └─ ✅ → 继续 ↓
  ├─ MCP 路由阈值实测校准需求? ── ✅/❌
  │   ├─ ✅ → Phase 8a 启动 (核心动机: 阈值准确)
  │   └─ ❌ → 继续 ↓
  ├─ 分布式部署需求 OR LLMDataPlane 高频需求? ── ✅/❌
  │   ├─ ✅ → Phase 8a 启动 (容量压力)
  │   └─ ❌ → Phase 8a descoped 保留 (等再次触发)
```

**Descoped 保留**: 4 service IDL 设计已 ship (本 ADR §D1), 实施可在 Phase 8a 一次性激活. 重复激活不会重新设计.

**与 Execution Plane (Phase 6c EnvBackend) 衔接**:

- **GRPCBackend** 是 EnvBackend 抽象的 gRPC 实现 (本 ADR §D6)
- Phase 6c EnvBackend ship (Local+Docker) 是 Phase 8a gRPC Data Plane 实施前置
- DSL `backend: grpc:remote-host:50051` 调用路径: DSL → EnvValidationHook → IEnvBackend::exec → GRPCBackend → gRPC stream → RemoteExecutor service

---

## 不变量

### 长期不变量

1. **gRPC 是数据面, MCP 是控制面** — 不可互相替代 (ADR-0071 §D7)
2. **gRPC 依赖 Control Plane ship ≥3 个月** — Phase 7 必须稳定运行才能启动 Phase 8a (本 ADR D8 新增)
3. **payload 64KB 阈值是路由唯一标准** — 不允许按 tool 类别 / 优先级手动路由
4. **4 个 service 是 gRPC 全部接口** — 不引入新 service (Phase 7+ 评估扩展)
5. **streaming 强制走 gRPC** — MCP JSON-RPC 不支持 server-streaming (协议层硬限制)
6. **protobuf v3 是 IDL 唯一格式** — 不引入 FlatBuffers / Cap'n Proto (生态成熟度)
7. **EnvBackend 新增 GRPCBackend** — 不破坏 LocalBackend / DockerBackend 接口
8. **gRPC 内部事件通过 IInteractionBus 上报** — 保持单一 observability 通道 (ADR-0068)

### 路由不变 (D2)

```
payload < 64KB && !streaming  →  MCP
payload >= 64KB || streaming  →  gRPC
任一豁免 service (BlobTransfer / Telemetry / LLMDataPlane)  →  强制 gRPC
gRPC 不可用时 fallback 到 MCP + warn event
```

### 鉴权不变 (D3)

```
MVP: 复用 ADR-0076 静态 token (gRPC metadata 注入)
Phase 2: mTLS / SPIFFE / OAuth 2.1 升级 (Phase 8+ 独立 ADR)
```

---

## 风险

### 高风险

| 风险 | 缓解 |
|------|------|
| **gRPC / protobuf 依赖膨胀** — vendor 二进制 +10MB, 编译时间 +30s | Phase 7 评估: vendored vs system; system 包优先 (apt); 仅当 ABI 不兼容时 vendor |
| **protobuf IDL 漂移** — `.proto` 文件 schema 演进破坏向后兼容 | protobuf field number 永不重用; 新字段用新 number; 删除字段标记 `reserved`; CI 验证 backward compat |
| **MCP/gRPC 路由阈值不准** — 64KB 阈值对某些 workload 不优 (e.g. 100KB JSON 在 MCP 仍快) | D2 阈值是启发式起点; Phase 7 评估 per-workload benchmark; 支持 `--grpc-threshold` 可调 |
| **gRPC stream 取消语义不统一** — `CancelStream` vs context cancellation 易混淆 | ADR-0071 §D8 已定义 `CancelStream` RPC; client 必须用此 RPC, 不依赖 context cancel |

### 中风险

| 风险 | 缓解 |
|------|------|
| **RemoteExecutor 与 LocalBackend 行为不一致** — gRPC 远程执行超时 / OOM 处理与本地不同 | 统一 BackendCapabilities + error_code (ADR-0075 D2/D3 错误码复用); CI 测试覆盖 100 个 scenario |
| **Telemetry 上报风暴** — 高频 metrics 阻塞 gRPC stream | client-side 聚合 (1s window); 自适应 batching; 失败时本地缓冲 + 指数退避 |
| **mTLS 证书管理** — 证书轮换 / 过期 / 吊销 | Phase 7+ 评估 cert-manager / SPIFFE; 当前 MVP 静态 token 绕过证书管理 |
| **gRPC client 多语言支持** — Python / Go / Rust 客户端绑定 | protobuf 自动生成多语言 stub; 重点 Python (AgenticMind) + Go (AgentForge); 文档示例 |

### 低风险

| 风险 | 缓解 |
|------|------|
| **gRPC over HTTP/2 防火墙穿透** — 某些网络环境禁 HTTP/2 | 端口 50051 明示; 文档化防火墙配置; 暂不支持 h2c (明文 HTTP/2) |
| **Protobuf 二进制不可读** — 调试时难排查 payload | Wireshark + gRPC plugin; 启用 `--grpc-reflection` 服务 |

---

## 替代方案

### 替代 1: 不做数据面, MCP 承担所有 (拒绝)

**否决理由**: MCP JSON-RPC 无 server-streaming + 大 payload 序列化开销大; LLM token 流必须 server-streaming; 与 ADR-0071 §D8 决策不符。

### 替代 2: 用 REST + WebSocket 替代 gRPC (拒绝)

**否决理由**: REST 性能低于 gRPC (HTTP/1.1 vs HTTP/2); WebSocket 无强类型 IDL; 缺乏 tracing / load balancing 内建支持。

### 替代 3: 用 Cap'n Proto / FlatBuffers 替代 protobuf (拒绝)

**否决理由**: protobuf 生态成熟度 (gRPC / k8s / etcd 全用); FlatBuffers 性能优势对当前 workload 不显著; 维护成本高。

### 替代 4: 4 service 拆分为更多 micro-service (拒绝)

**否决理由**: 部署 / 运维成本 × N; 单进程可承载 4 service; Phase 8+ 评估拆分。

### 替代 5: 不做 RemoteExecutor, 仅靠 MCP + ADR-0075 (拒绝)

**否决理由**: 跨主机执行无统一抽象; ADR-0075 仅 Local + Docker; 与 ADR-0071 §D8 决策不符; 失去 gRPC 优势。

### 替代 6: gRPC 立即实施, 不推迟到 Phase 7 (拒绝)

**否决理由**: Phase 6a/6b 已 37h/44h 满载; 与 ADR-0071 §D8 "Wave 4 descoped" 不符; Solo Dev 容量不足。

---

## 影响范围

### 文档
- `docs/specs/grpc-data-plane.md` (Phase 7+ 新增) — 4 service 详细契约 + 路由规则
- `docs/security/grpc-auth.md` (Phase 7+ 新增) — mTLS / 静态 token 配置
- `docs/protobuf/agenticdsl.proto` (Phase 7+ 新增) — IDL 源文件
- `docs/llm/training-data-grpc-stream.md` (Phase 7+ 新增) — LLMDataPlane 训练数据采集

### 代码 (Phase 7+ 实施, 当前 ⏸)
- `external/protobuf/` (vendored, ~5MB) — protobuf 库
- `external/grpc-cpp/` (vendored, ~10MB) — gRPC C++ 库
- `tests/data/proto/agenticdsl.proto` (新增) — 4 service IDL
- `include/agenticdsl/grpc/` (新增) — gRPC client/server stubs
- `src/common/grpc/` (新增) — LLMDataPlane / BlobTransfer / RemoteExecutor / Telemetry 实现
- `src/common/env/grpc_backend.cpp` (新增) — GRPCBackend (D6)
- `src/common/grpc/event_emitter.cpp` (新增) — 4 个 grpc.* 事件 (D5, 注册前置: ADR-0068 §附录 A amendment)
- `src/modules/executor/tool_coordinator.cpp` — 路由决策 (D7)

### 测试 (Phase 7+)
- `tests/test_grpc_llm_data_plane.cpp` — LLMDataPlane 3 RPC + 取消语义
- `tests/test_grpc_blob_transfer.cpp` — Upload + Download + 断点续传 + sha256 校验
- `tests/test_grpc_remote_executor.cpp` — RemoteExecutor + 与 EnvBackend 集成
- `tests/test_grpc_telemetry.cpp` — PushMetrics + SubscribeTraces + 批量化
- `tests/test_grpc_routing.cpp` — 64KB 阈值边界 + fallback to MCP
- `tests/test_grpc_auth.cpp` — 静态 token + mTLS Phase 2

### 生态
- `examples/grpc_client_demo/` (Phase 7+) — 示例 gRPC client (Python + C++)
- `lib/` stdlib subgraphs — `backend: grpc:remote-host` 示例 (D6 衔接 ADR-0075)
- ADR-0078 (Fine-tune) — LLMDataPlane.StreamTokens 是训练数据采集底层

---

## 后续

### 短期 (Wave 4 descoped, 当前不实施)

1. **保持 ADR 状态 🔍 Proposed** — docs-only 未来设计
2. **Phase 7 重新评估触发条件** (见"复审节点")
3. **不写代码** — `.proto` 文件不在 git 跟踪 (Phase 7 实施时创建)
4. **不创建 OpenSpec change** — change 创建需 Wave 推进决议

### Phase 7+ 实施触发后 (估时 2-3 周)

5. **D4 依赖集成**: vendor protobuf + grpc-cpp; 评估 ABI 兼容性
6. **D1 proto 定义**: `agenticdsl.proto` 4 service + message types
7. **代码生成**: protoc + grpc_cpp_plugin → C++ stubs
8. **D6 GRPCBackend**: 复用 ADR-0075 EnvBackend 抽象
9. **D7 ToolCoordinator 路由决策**: payload 阈值 + streaming 检测
10. **D5 事件上报**: 4 个 grpc.* 主题 + IInteractionBus 集成

### Phase 8+ (估时 4-6 周, 独立 ADR 立项)

11. **mTLS 鉴权升级** (D3): cert-manager / SPIFFE 集成
12. **gRPC reflection 服务**: 调试 + 客户端发现
13. **gRPC load balancing**: xDS / 客户端轮询
14. **多语言 SDK**: Python (AgenticMind) + Go (AgentForge) + Rust (Phase 9)

---

## 复审节点

- **本 ADR 创建时 (2026-08-03)**: 🔍 Proposed + docs-only + Wave 4 descoped
- **Phase 7 容量评估时** (估时 2026-Q4 或 2027-Q1):
  - 检查: Solo Dev 容量 ≥4 周连续?
  - 检查: Wave 2 → Wave 3 Evidence Gate 已 PASS (parse-valid ≥85% + task-success 阈值)
  - 检查: ADR-0076 MCP server 已 ship ≥3 个月, 验证 MCP 边界判定准确
  - 满足全部 → 启动 Phase 7 实施 (估时 2-3 周)
- **Phase 8 评估时** (Phase 7 ship 后): mTLS / load balancing / 多语言 SDK 独立 ADR 立项

---

## Wave 4 descoped 理由 (引用 ADR-0071 §战略)

> 4.1 §Layer 3 dual memos (2026-07-15 C19 spike) 记录: Solo Dev Phase 6a/6b 容量 37h/44h 满载;
> Wave 4 (gRPC 数据面) 估时 2-3 周 + 多语言 SDK 估时 4-6 周, 单独 ≥6 周连续工作;
> 团队 1 人无法承担 Phase 6 demo 推进 + Phase 7 gRPC 实施并行。

**重新激活条件** (任一):

1. 团队规模扩张到 ≥2 人 (≥80h/双周容量)
2. AgenticMind 项目独立 ship + 强需求 LLMDataPlane (Fine-tune 数据采集高频)
3. 分布式部署 (K8s) 需求出现, RemoteExecutor 成为阻塞项
4. Wave 3 MCP server 边界判定不准, 64KB 阈值需实测校准

---

*文档版本: v1.0*
*创建日期: 2026-08-03*
*作者: HydraForge 架构组*
*状态: 🔍 Proposed (Wave 4 descoped; docs-only 未来设计; 实施 Phase 7+ 重新评估; 衔接 ADR-0076 MCP 控制面; 待架构组评审)*