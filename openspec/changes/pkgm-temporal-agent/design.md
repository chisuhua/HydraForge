# Temporal Agent Plugin 设计（纯 C++）

## Context

PoC-02 已证明 Temporal blocking 和 async polling 两种模式均可行。HydraForge 的 PDK v2 提供了标准化 Agent 契约。本设计将 Temporal Worker 作为纯 C++ PDK Plugin，通过 gRPC 直连 Temporal Server。

### 技术选型

| 组件 | 选型 | 依据 |
|------|------|------|
| 通信协议 | gRPC (protobuf) | Temporal Server 原生协议 |
| protobuf 来源 | FetchContent(temporal-sdk-protos) | 官方 proto 定义 |
| gRPC 库 | FetchContent(grpc) 或 find_package | 标准 C++ gRPC |
| JSON 解析 | nlohmann_json | HydraForge 已有（external/） |
| HTTP fallback | cpp-httplib | HydraForge 已有（external/） |

## Design

### gRPC 客户端架构

```
pdk/temporal_agent/
├── CMakeLists.txt          # 编译 libTemporalAgent.so
├── pdk_manifest.json       # PDK manifest
├── include/
│   ├── temporal_agent.h    # 公共头文件
│   └── temporal_client.h   # gRPC 客户端封装
├── src/
│   ├── pdk_entry.cpp       # pdk_register_tools + pdk_register_agent
│   ├── temporal_client.cpp # gRPC stub 初始化 + 请求封装
│   └── proto/
│       └── temporal.api.workflowservice.v1.grpc.pb.h  # 编译生成
├── proto/
│   └── CMakeLists.txt      # protobuf 编译规则（FetchContent temporal-sdk-protos）
└── tests/
    ├── test_client.cpp     # gRPC 连接测试（需 Temporal Server）
    └── test_tools.cpp      # 工具注册 + mock 测试
```

### gRPC 客户端封装 (`temporal_client.h/cpp`)

```cpp
// 单例连接池，线程安全
class TemporalClient {
public:
    static TemporalClient& instance();
    
    // 连接管理
    bool connect(const std::string& host = "localhost:7233");
    void disconnect();
    bool is_connected() const;
    
    // Workflow 操作
    WorkflowResult start_workflow_blocking(
        const std::string& workflow_type,
        const std::string& workflow_id,
        const std::string& task_queue,
        const nlohmann::json& args,
        int timeout_ms = 300000);
    
    AsyncJob start_workflow_async(
        const std::string& workflow_type,
        const nlohmann::json& args,
        const std::string& task_queue = "async-queue");
    
    WorkflowStatus poll_workflow(const std::string& workflow_id);
    bool signal_workflow(const std::string& workflow_id,
                         const std::string& signal_name,
                         const nlohmann::json& payload);
    WorkflowStatus query_workflow(const std::string& workflow_id);

private:
    std::unique_ptr<WorkflowService::Stub> stub_;
    std::shared_ptr<grpc::Channel> channel_;
    std::mutex mutex_;
};
```

### 关键 gRPC 调用映射

| PDK 工具 | gRPC 调用 |
|----------|----------|
| `temporal/start_workflow` | `StartWorkflowExecution` → 阻塞轮询 `GetWorkflowExecutionHistory` |
| `temporal/start_async` | `StartWorkflowExecution` + 立即返回 |
| `temporal/poll` | `DescribeWorkflowExecution` |
| `temporal/signal` | `SignalWorkflowExecution` |
| `temporal/query` | `QueryWorkflowExecution` |

### 阻塞等待实现 (`start_workflow_blocking`)

```cpp
WorkflowResult TemporalClient::start_workflow_blocking(...) {
    // 1. 构造 StartWorkflowExecutionRequest
    StartWorkflowExecutionRequest req;
    req.set_namespace("pkgm");
    req.set_workflow_id(workflow_id);
    req.set_workflow_type_name(workflow_type);
    req.set_task_queue_name(task_queue);
    req.mutable_input()->PackFrom(/* args as Payloads */);
    
    // 2. 发送 gRPC 请求
    StartWorkflowExecutionResponse resp;
    grpc::ClientContext ctx;
    ctx.set_deadline(/* timeout_ms */);
    auto status = stub_->StartWorkflowExecution(&ctx, req, &resp);
    
    // 3. 阻塞轮询直到完成
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        DescribeWorkflowExecutionRequest dreq;
        dreq.set_namespace("pkgm");
        dreq.mutable_execution()->set_workflow_id(workflow_id);
        
        DescribeWorkflowExecutionResponse dresp;
        auto dstatus = stub_->DescribeWorkflowExecution(&dctx, dreq, &dresp);
        
        if (dresp.workflow_execution_info().status() == WORKFLOW_EXECUTION_STATUS_COMPLETED) {
            return /* parse result from dresp */;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    throw TemporalTimeoutException();
}
```

### pdk_entry.cpp 结构

```cpp
extern "C" const hydraforge::PluginInfo pdk_plugin_info = {
    hydraforge::CURRENT_ABI_VERSION,
    "temporal_agent",             // name
    0, 1, 0,                      // semver
    "Temporal Workflow Agent",    // description
    "workflow_execution,long_running_tasks,idempotent_operations",
    ""
};

extern "C" void pdk_register_tools(agenticdsl::IToolRegistry& registry) {
    auto& client = TemporalClient::instance();
    
    registry.register_tool_function(
        "temporal/start_workflow",
        ToolMetadata{
            .name = "temporal/start_workflow",
            .description = "Start a Temporal Workflow and block until completion",
            .domain = "temporal",
            .category = ToolCategory::Execute,
            .min_layer = LayerProfile::Workflow,
            .approval = ApprovalPolicy{...},
            .allowed_layers = {LayerProfile::Workflow}
        },
        [&client](const auto& args) -> nlohmann::json {
            auto result = client.start_workflow_blocking(
                str_arg(args, "workflow_type"),
                str_arg(args, "workflow_id"),
                str_arg(args, "task_queue", "blocking-queue"),
                json_arg(args, "args"),
                int_arg(args, "timeout_ms", 300000)
            );
            return {
                {"success", true},
                {"workflow_id", result.workflow_id},
                {"result", result.payload},
                {"duration_ms", result.duration_ms},
                {"history_size_bytes", result.history_size_bytes},
                {"event_count", result.event_count}
            };
        }
    );
    
    // ... 其余 4 个工具
}
```

### 错误处理

| 场景 | gRPC Status | 返回 |
|------|------------|------|
| Temporal 不可达 | UNAVAILABLE | `{success: false, error: "TEMPORAL_UNREACHABLE", retry: true}` |
| Workflow 超时 | DEADLINE_EXCEEDED | `{success: false, error: "TIMEOUT"}` |
| Workflow 不存在 | NOT_FOUND | `{success: false, error: "WORKFLOW_NOT_FOUND"}` |
| 幂等键冲突 | ALREADY_EXISTS | `{success: true, workflow_id: "..."}` ← 幂等成功 |
| 参数非法 | INVALID_ARGUMENT | `{success: false, error: "INVALID_ARGS"}` |

### 连接管理

- gRPC channel 在首次 `call_tool` 时懒初始化
- 使用默认的 gRPC 负载均衡策略（pick_first）
- 连接断开时自动重连（gRPC 内置）
- PluginLoader unload 时调用 `TemporalClient::disconnect()`

### 构建依赖

```cmake
# pdk/temporal_agent/CMakeLists.txt

# Protobuf + gRPC
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# 或使用 FetchContent
include(FetchContent)
FetchContent_Declare(
  temporal-proto
  GIT_REPOSITORY https://github.com/temporalio/temporal-sdk-protos.git
  GIT_TAG v1.27.0
)
FetchContent_MakeAvailable(temporal-proto)

# 编译 protobuf → .grpc.pb.cc
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS
  ${temporal-proto_SOURCE_DIR}/temporal/api/workflowservice/v1/service.proto
)

# 编译 plugin .so
add_library(TemporalAgent SHARED
  src/pdk_entry.cpp
  src/temporal_client.cpp
  ${PROTO_SRCS}
)
target_link_libraries(TemporalAgent
  hydraforge_pdk
  protobuf::libprotobuf
  gRPC::grpc++
)
```

### 可观测性

通过 `IInteractionBus`（进程内）发送事件，由 HydraForge 的 OTel Exporter 统一导出：

- `temporal.client.connect` — {host, latency_ms}
- `temporal.workflow.start` — {workflow_id, workflow_type}
- `temporal.workflow.complete` — {workflow_id, duration_ms, history_size_bytes, event_count}
- `temporal.workflow.failed` — {workflow_id, error, grpc_status}
- `temporal.poll` — {workflow_id, status, poll_count}

---

## 方案 B：Temporal CLI 快速验证（备选）

如果 gRPC 依赖太重，可先用 Temporal CLI 做 0-day 验证，接口保持不变：

```
temporal workflow execute \
  --workflow BlockingLLMWorkflow \
  --task-queue blocking-queue \
  --input '{"delay": 5}' \
  --output json
```

```cpp
// proxy_executor.cpp (方案 B)
ProxyResult execute_temporal_cli(const std::string& cmd, const std::string& input_json) {
    FILE* pipe = popen(cmd.c_str(), "r");
    // 读 stdout JSON → ProxyResult
    pclose(pipe);
}
```

方案 B 不引入新依赖，适合快速验证。之后迁移到方案 A。

## Risks / Trade-offs

| 风险 | 缓解 |
|------|------|
| protobuf/gRPC 编译链复杂 | FetchContent 自动化，CI Docker 缓存 |
| Temporal proto 版本与 Server 不匹配 | 锁定 temporal-sdk-protos 版本，CI 检查 |
| gRPC 连接泄漏 | RAII 封装（TemporalClient 析构时 shutdown） |
| 首次 protobuf 编译慢 | CMake FetchContent 本地缓存 |
