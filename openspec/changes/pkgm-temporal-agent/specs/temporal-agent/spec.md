# Temporal Agent Plugin Spec（纯 C++）

## Overview

Temporal Agent 是 HydraForge 的纯 C++ PDK Plugin，通过 gRPC（protobuf）直连 Temporal Server，将 Workflow 操作暴露为 5 个工具。

## Plugin Manifest (`pdk_manifest.json`)

```json
{
  "$schema": "https://schemas.hydraforge.io/pdk-manifest-v1.json",
  "id": "temporal_agent",
  "name": "Temporal Workflow Agent",
  "version": "0.1.0",
  "abi_version": 2,
  "min_host_version": "0.3.0",
  "interface_versions": ["IAgentV1"],
  "implementation_forms": ["cpp"],
  "entry_tool": "temporal/start_workflow",
  "provided_tools": [
    "temporal/start_workflow",
    "temporal/start_async",
    "temporal/poll",
    "temporal/signal",
    "temporal/query"
  ],
  "capabilities": [
    "workflow_execution",
    "long_running_tasks",
    "idempotent_operations"
  ],
  "requires_isolation": false,
  "resources": {
    "timeout_ms": 600000,
    "max_concurrent": 10
  },
  "publisher": "pkgm-team",
  "trust_level": "high"
}
```

## 调用约定

HydraForge 的 `IToolRegistry::call_tool()` 使用 `std::unordered_map<std::string, std::string>` 作为参数。每个工具按以下约定：

- **传入**：工具用到的 JSON 参数通过 `args_json` 键以 string 传入
- **辅助函数**：`json_arg(args, key)` 从 `args_json` 中解析指定 key 的值
- **返回**：`nlohmann::json`

```cpp
// 公共辅助函数（各工具复用）
inline nlohmann::json json_arg(const std::unordered_map<std::string, std::string>& args,
                                const std::string& key) {
    auto it = args.find(key);
    if (it == args.end()) return nlohmann::json();
    return nlohmann::json::parse(it->second);
}
inline std::string str_arg(const std::unordered_map<std::string, std::string>& args,
                           const std::string& key, const std::string& def = "") {
    auto it = args.find(key);
    return (it != args.end()) ? it->second : def;
}
inline int int_arg(const std::unordered_map<std::string, std::string>& args,
                   const std::string& key, int def = 0) {
    auto it = args.find(key);
    if (it == args.end()) return def;
    try { return std::stoi(it->second); } catch (...) { return def; }
}
```

---

## 工具规格

### 1. temporal/start_workflow

启动 Workflow 并 gRPC 阻塞轮询直到完成。适用于短任务（<10s）。

- **Category**: Execute
- **Allowed Layers**: Workflow
- **Approval**: requires_approval_in_agent=true, requires_approval_in_plan=false, force_approval_always=false

**输入**（`call_tool` 的 flat args）：
| Key | 类型 | 必填 | 默认 | 说明 |
|-----|------|------|------|------|
| `workflow_type` | string | ✅ | — | Workflow 类型名 |
| `workflow_id` | string | ✅ | — | 幂等 Workflow ID |
| `args_json` | string (JSON) | — | `{}` | 传递给 Workflow 的参数 |
| `task_queue` | string | — | `blocking-queue` | Temporal Task Queue |
| `timeout_ms` | string (int) | — | `300000` | 超时（毫秒） |

**输出**（`nlohmann::json`）：
```json
{
  "success": true,
  "workflow_id": "pkgm-ingest-abc123",
  "result": {},
  "duration_ms": 12550,
  "history_size_bytes": 1873,
  "event_count": 11,
  "error": ""
}
```

**gRPC 调用链**：
```
StartWorkflowExecution → 循环 DescribeWorkflowExecution（500ms 间隔）
  → WORKFLOW_EXECUTION_STATUS_COMPLETED → 返回 result
  → 超时 → 返回 success=false, error="TIMEOUT"
```

**幂等行为**：相同 `workflow_id` 第二次调用返回 ALREADY_EXISTS → `{success: true, workflow_id: "..."}`（不创建新 Workflow）

---

### 2. temporal/start_async

异步创建 Workflow，立即返回 job_id 和 workflow_id。适用于长任务（>10s）。

- **Category**: Execute
- **Allowed Layers**: Workflow

**输入**：
| Key | 类型 | 必填 | 默认 | 说明 |
|-----|------|------|------|------|
| `workflow_type` | string | ✅ | — | Workflow 类型名 |
| `args_json` | string (JSON) | — | `{}` | 传递给 Workflow 的参数 |
| `task_queue` | string | — | `async-queue` | |
| `timeout_ms` | string (int) | — | `600000` | |

**输出**：
```json
{
  "job_id": "job-abc123",
  "workflow_id": "pkgm-wikigen-abc123",
  "status": "running"
}
```

**gRPC 调用链**：
```
StartWorkflowExecution → 成功返回 run_id → {job_id: run_id, workflow_id, status: "running"}
```

---

### 3. temporal/poll

查询 Workflow 或 Job 的执行状态。

- **Category**: ReadOnly
- **Allowed Layers**: Workflow, Thinking

**输入**：
| Key | 类型 | 必填 | 说明 |
|-----|------|------|------|
| `workflow_id` | string | ✅ | 或 job_id |

**输出**：
```json
{
  "status": "completed",
  "result": {},
  "progress": 100,
  "error": ""
}
```

`status` 枚举：`running` | `completed` | `failed` | `timeout` | `cancelled`

**gRPC 调用**：`DescribeWorkflowExecution`

---

### 4. temporal/signal

向运行中的 Workflow 发送 Signal。

- **Category**: StateModify
- **Allowed Layers**: Workflow

**输入**：
| Key | 类型 | 必填 | 说明 |
|-----|------|------|------|
| `workflow_id` | string | ✅ | |
| `signal_name` | string | ✅ | Signal 名 |
| `payload_json` | string (JSON) | — | Signal 载荷 |

**输出**：`{"success": true}` 或 `{"success": false, "error": "WORKFLOW_NOT_FOUND"}`

**gRPC 调用**：`SignalWorkflowExecution`

---

### 5. temporal/query

查询 Workflow 元数据（状态、事件数、启动时间）。

- **Category**: ReadOnly
- **Allowed Layers**: Workflow, Thinking, Cognitive

**输入**：
| Key | 类型 | 必填 |
|-----|------|------|
| `workflow_id` | string | ✅ |

**输出**：
```json
{
  "status": "RUNNING",
  "start_time": "2026-07-22T10:00:00Z",
  "history_events": 15,
  "history_size_bytes": 2300
}
```

**gRPC 调用**：`DescribeWorkflowExecution`

---

## TemporalClient 接口契约

```cpp
class TemporalClient {
public:
    static TemporalClient& instance();

    // 生命周期
    bool connect(const std::string& host = "localhost:7233");
    void shutdown();

    // 核心操作
    WorkflowResult start_workflow_blocking(
        const std::string& workflow_type,
        const std::string& workflow_id,
        const std::string& task_queue,
        const nlohmann::json& input,
        std::chrono::milliseconds timeout);

    AsyncJob start_workflow_async(
        const std::string& workflow_type,
        const nlohmann::json& input,
        const std::string& task_queue);

    WorkflowStatus poll(const std::string& workflow_id);
    bool signal(const std::string& workflow_id, const std::string& name, const nlohmann::json& payload);
    WorkflowStatus query(const std::string& workflow_id);

private:
    std::unique_ptr<temporal::api::workflowservice::v1::WorkflowService::Stub> stub_;
    // ...
};
```

### 错误码映射

| gRPC Status | ErrorCode | 重试建议 |
|------------|-----------|---------|
| OK | SUCCESS | — |
| UNAVAILABLE | TEMPORAL_UNREACHABLE | 3 次指数退避 |
| DEADLINE_EXCEEDED | TIMEOUT | 不重试 |
| NOT_FOUND | WORKFLOW_NOT_FOUND | 不重试 |
| ALREADY_EXISTS | WORKFLOW_ALREADY_EXISTS | 幂等成功 |
| INVALID_ARGUMENT | INVALID_ARGS | 不重试 |
| INTERNAL | TEMPORAL_INTERNAL | 1 次重试 |

---

## 构建系统

### 依赖

| 依赖 | 版本 | 获取方式 |
|------|------|---------|
| protobuf | ≥3.21 | find_package / FetchContent |
| gRPC C++ | ≥1.50 | find_package / FetchContent |
| temporal-sdk-protos | v1.27.x | FetchContent(github) |
| nlohmann_json | ≥3.11 | HydraForge 已有（external/） |

### CMakeLists.txt

```cmake
# pdk/temporal_agent/CMakeLists.txt
project(temporal_agent LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

# protobuf + gRPC
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# temporal-sdk-protos
FetchContent_Declare(temporal_api
  GIT_REPOSITORY https://github.com/temporalio/api.git
  GIT_TAG v1.27.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(temporal_api)

# protobuf 编译
file(GLOB_RECURSE TEMPORAL_PROTOS
  ${temporal_api_SOURCE_DIR}/temporal/api/workflowservice/v1/service.proto
)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${TEMPORAL_PROTOS})
grpc_generate_cpp(GRPC_SRCS GRPC_HDRS ${TEMPORAL_PROTOS})

# 编译 plugin
add_library(TemporalAgent SHARED src/pdk_entry.cpp src/temporal_client.cpp ${PROTO_SRCS} ${GRPC_SRCS})
target_include_directories(TemporalAgent PRIVATE include ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(TemporalAgent
  PUBLIC hydraforge_pdk
  PRIVATE protobuf::libprotobuf gRPC::grpc++
)
```

---

## 测试

### 单元测试（--mock 模式，无需 Temporal Server）

```cpp
TEST_CASE("temporal agent - tool metadata validation") {
    ToolRegistry registry;
    pdk_register_tools(registry);
    
    // 验证 5 个工具全部注册
    REQUIRE(registry.has_tool("temporal/start_workflow"));
    REQUIRE(registry.has_tool("temporal/start_async"));
    REQUIRE(registry.has_tool("temporal/poll"));
    REQUIRE(registry.has_tool("temporal/signal"));
    REQUIRE(registry.has_tool("temporal/query"));
}

TEST_CASE("temporal/start_workflow - missing required args") {
    auto result = call_tool("temporal/start_workflow", {});
    REQUIRE(result["success"] == false);
    REQUIRE(result["error"] == "INVALID_ARGS");
}
```

### 集成测试（需 Temporal dev server + PoC-02 Workers）

```cpp
TEST_CASE("temporal/start_workflow - end to end") {
    TemporalClient::instance().connect("localhost:7233");
    
    auto result = call_tool("temporal/start_workflow", {
        {"workflow_type", "BlockingLLMWorkflow"},
        {"workflow_id", "test-e2e-001"},
        {"args_json", R"({"delay": 1})"}
    });
    
    REQUIRE(result["success"] == true);
    REQUIRE(result["history_size_bytes"] > 0);
    REQUIRE(result["event_count"] == 11);
}

TEST_CASE("temporal/start_workflow - idempotency") {
    auto r1 = call_tool("temporal/start_workflow", args);
    auto r2 = call_tool("temporal/start_workflow", args);
    REQUIRE(r1["workflow_id"] == r2["workflow_id"]);
}
```

---

## Conformance

| Level | 要求 |
|-------|------|
| L1 | `pdk_manifest.json` 合法，`pdk_plugin_info` 符号存在 |
| L2 | 5 个工具可注册，schema 校验通过 |
| L3 | mock 模式单元测试通过（不需要 Temporal Server） |
| L4 | 集成测试通过（需 Temporal dev server + PoC-02 Workers） |

## 文件结构

```
pdk/temporal_agent/
├── CMakeLists.txt
├── pdk_manifest.json
├── include/
│   ├── temporal_agent.h
│   └── temporal_client.h
├── src/
│   ├── pdk_entry.cpp
│   └── temporal_client.cpp
└── tests/
    ├── test_metadata.cpp      # 工具注册 + schema 验证（--mock）
    └── test_integration.cpp   # 端到端（需 Temporal Server）
```
