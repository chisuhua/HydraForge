# PKM Temporal Agent PoC 设计 v0.2

**日期**: 2026-07-23 (v0.1: 2026-07-22, v0.2: 架构对齐)
**状态**: 🟡 实施中 (已对齐 `docs/architecture/agent-as-plugin-architecture-v1.2.md`)
**来源**: `openspec/changes/pkgm-temporal-agent/`
**前置文档**: `docs/architecture/agent-as-plugin-architecture-v1.2.md` (五层模型, L3 ITemporalClient 契约, L4 Temporal Agent)
**参考项目**: `examples/pdk_chat_demo/` (Agent-as-Plugin 模式)

---

## 一、设计目标

`examples/pkm_temporal_demo` 是 PKGM 的 Temporal 集成的**最小可行 PoC**，演示：

1. **Temporal Agent 作为 PDK Plugin** — `pdk/temporal_agent/` 独立 `.so`，5 个工具
2. **Temporal CLI 封装** — 方案 B（`popen` + JSON），零新依赖
3. **Mock 模式优先** — 不依赖外部 Temporal Server，CI 即可验证完整调用链路
4. **渐进升级路径** — Phase 2 迁移到 gRPC 直连时，工具接口不变

---

## 二、架构全景

> 对应 `docs/architecture/agent-as-plugin-architecture-v1.2.md` 图层模型：
> - **L4**: Temporal Agent (编排 + 暴露外部 API)
> - **L3**: ITemporalClient (PDK 接口契约)
> - **L2**: 通过 L3 调用的原子工具 (shell_tools 等)
> - **L1**: HydraForge OS 服务 (PluginLoader, IToolRegistry, IInteractionBus)

```
┌────────── HydraForge (C++20) ────────────────────────────────┐
│                                                               │
│  L4 ┌─ Temporal Agent (pdk/temporal_agent/lib.so) ────────┐  │
│     │  pdk_entry.cpp: 5 工具注册                           │  │
│     │  ├─ temporal/start_workflow ──┐                      │  │
│     │  ├─ temporal/start_async    ──┤                      │  │
│     │  ├─ temporal/poll           ──┤→ ITemporalClient     │  │
│     │  ├─ temporal/signal         ──┤  (L3 PDK 契约)       │  │
│     │  └─ temporal/query          ──┘                      │  │
│     │                                                      │  │
│     │  L3 ITemporalClient (抽象接口):                       │  │
│     │  ├─ TemporalCLIClient:   popen "temporal ..."        │  │
│     │  └─ MockTemporalClient: 预置 JSON 应答               │  │
│     └──────────────────────────────────────────────────────┘  │
│                                                               │
│  L4 PKM Temporal Demo (examples/pkm_temporal_demo/)           │
│  ├─ main.cpp: 加载 Plugin + 编排演示场景 (L4 编排者)         │
│  └─ 演示模式:                                                  │
│      ├─ --mock: 使用 MockTemporalClient (预置 JSON 应答)      │
│      └─ --live: 使用 TemporalClient  (popen temporal CLI)     │
│                                                               │
│  L1 HydraForge 基础设施 (复用已有):                            │
│  ├─ DSLEngine + PluginLoader                                 │
│  ├─ IToolRegistry + IInteractionBus                          │
│  └─ nlohmann_json + Catch2 (复用现有)                        │
└───────────────────────────────────────────────────────────────┘
```

---

## 三、与原始 `pkgm-temporal-agent` change 的对应关系

| 原始 change | PoC 决策 | 理由 |
|------------|---------|------|
| 方案 A: gRPC 直连 | **推迟到 Phase 2** | protobuf + gRPC FetchContent 打破 HydraForge 零 FetchContent 惯例 |
| 方案 B: Temporal CLI | **PoC 采用** | 零新依赖，与 `shell_tools` 同模式 |
| `pdk_register_agent` | **推迟到 Phase 2** | v1.2 架构已定义 `AgentDescriptor` (L3, ADR-0053); PoC 仅实现 `pdk_register_tools` + `pdk_manifest` |
| IInteractionBus 集成 | **PoC 不做** | 现有 PDK 插件未集成，保持简单 |
| Integration tests with Temporal dev server | **Mock 模式替代** | PoC 不要求外部 Temporal Server |

---

## 四、文件结构

```
examples/pkm_temporal_demo/          # ← PoC demo 项目
├── CMakeLists.txt                   # 编译 demo 可执行文件 + mock 测试
├── DESIGN.md                        # 本文件
├── README.md                        # 运行说明
├── config.json                      # 应用配置
├── main.cpp                         # 入口：加载 plugin + 演示 4 个场景
└── tests/
    ├── CMakeLists.txt
    ├── test_temporal_mock.cpp       # MockTemporalClient 单元测试
    └── test_temporal_e2e_mock.cpp   # 端到端 mock 测试

pdk/temporal_agent/                  # ← PDK Plugin (工具实现)
├── CMakeLists.txt                   # 独立 .so 编译
├── pdk_manifest.json                # PDK manifest (合规的)
├── include/
│   ├── temporal_agent.h             # 公共头 (ITemporalClient 接口)
│   ├── temporal_cli_client.h        # TemporalCLIClient: popen 实现
│   └── temporal_mock_client.h       # MockTemporalClient: 预置应答
├── src/
│   ├── pdk_entry.cpp                # pdk_plugin_info + pdk_register_tools (5 tools)
│   ├── temporal_cli_client.cpp      # popen("temporal workflow execute --output json ...")
│   └── temporal_mock_client.cpp     # Mock 应答生成器
└── tests/
    ├── CMakeLists.txt
    ├── test_metadata.cpp            # 工具注册覆盖率 + ToolMetadata 完整性
    └── test_tools_mock.cpp          # Mock 模式下 5 个工具输入/输出验证
```

---

## 五、核心接口：ITemporalClient (L3 PDK 契约层)

> **层归属**: `ITemporalClient` 属于 v1.2 架构的 **L3 PDK 接口契约层**，
> 与 `IToolRegistry`、`IModelRouter`、`IExecutionPolicy`、`IApprovalHandler` 同级。
> 实现者 (TemporalCLIClient / MockTemporalClient) 位于 L4 Temporal Agent 内部。

```cpp
// pdk/temporal_agent/include/temporal_agent.h
#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <chrono>

namespace pkm::temporal {

// 统一返回结构体
struct WorkflowResult {
    bool success = false;
    std::string workflow_id;
    std::string run_id;           // Temporal run_id
    nlohmann::json result;        // Workflow 返回值
    int64_t duration_ms = 0;
    int history_size_bytes = 0;
    int event_count = 0;
    std::string error;            // 错误消息 (success=false 时)
    std::string error_code;       // 错误码: UNREACHABLE / TIMEOUT / NOT_FOUND / ...
};

struct AsyncJob {
    std::string job_id;
    std::string workflow_id;
    std::string status;  // "running"
};

struct WorkflowStatus {
    std::string status;  // running | completed | failed | timeout | cancelled
    nlohmann::json result;
    int progress = 0;    // 0-100
    std::string error;
};

// 抽象接口 — Phase 1: CLI popen, Phase 2: gRPC
class ITemporalClient {
public:
    virtual ~ITemporalClient() = default;

    // 同步启动 Workflow 并阻塞等待 (短任务 <10s)
    virtual WorkflowResult start_workflow_blocking(
        const std::string& workflow_type,
        const std::string& workflow_id,
        const nlohmann::json& input,
        const std::string& task_queue = "blocking-queue",
        std::chrono::milliseconds timeout = std::chrono::minutes(5)) = 0;

    // 异步启动 Workflow (长任务)
    virtual AsyncJob start_workflow_async(
        const std::string& workflow_type,
        const nlohmann::json& input,
        const std::string& task_queue = "async-queue") = 0;

    // 轮询状态
    virtual WorkflowStatus poll(const std::string& workflow_id) = 0;

    // 发送 Signal
    virtual bool signal(const std::string& workflow_id,
                        const std::string& signal_name,
                        const nlohmann::json& payload) = 0;

    // 查询元数据
    virtual WorkflowStatus query(const std::string& workflow_id) = 0;
};

} // namespace pkm::temporal
```

---

## 六、TemporalCLIClient 实现（popen 封装）

```cpp
// pdk/temporal_agent/include/temporal_cli_client.h
// Phase 1: popen("temporal workflow execute --output json ...")
// 完全复刻 shell_tools/src/pdk_entry.cpp 的 popen 模式:
//   - fork() + execl() + pipe() + poll() + WNOHANG + timeout + SIGTERM/SIGKILL

#include "temporal_agent.h"

namespace pkm::temporal {

class TemporalCLIClient : public ITemporalClient {
public:
    explicit TemporalCLIClient(const std::string& host = "localhost:7233",
                                const std::string& namespace_ = "pkgm");

    // ITemporalClient 接口实现
    WorkflowResult start_workflow_blocking(...) override;
    AsyncJob start_workflow_async(...) override;
    WorkflowStatus poll(const std::string& workflow_id) override;
    bool signal(...) override;
    WorkflowStatus query(const std::string& workflow_id) override;

private:
    // 底层 popen 调用
    // shell_tools 已验证的 secure popen 模式: fork + pipe + poll + timeout
    struct ExecResult {
        int exit_code;
        std::string stdout_output;
        std::string stderr_output;
        bool timed_out = false;
        int64_t duration_ms = 0;
    };

    ExecResult exec_temporal(const std::string& cmd, int timeout_ms);
    std::string host_;
    std::string namespace_;
};

} // namespace pkm::temporal
```

**关键 CLI 命令映射**:

| 工具 | Temporal CLI 命令 |
|------|------------------|
| `start_workflow_blocking` | `temporal workflow execute --workflow <type> --task-queue <q> --input '<json>' --output json` |
| `start_workflow_async` | 同上 + `--detach` → 解析 `run_id` |
| `poll` | `temporal workflow describe --workflow-id <id> --output json` |
| `signal` | `temporal workflow signal --workflow-id <id> --name <name> --input '<json>'` |
| `query` | `temporal workflow describe --workflow-id <id> --output json` |

---

## 七、MockTemporalClient（CI 测试）

```cpp
// pdk/temporal_agent/include/temporal_mock_client.h
// 预置 JSON 应答, 不依赖 Temporal Server

#include "temporal_agent.h"
#include <queue>

namespace pkm::temporal {

class MockTemporalClient : public ITemporalClient {
public:
    // 预置应答: enqueue 一个 WorkflowResult, 下次调用 start_workflow_blocking 返回它
    void enqueue_response(WorkflowResult result);
    void enqueue_async_response(AsyncJob job);
    void enqueue_poll_response(WorkflowStatus status);

    // 记录调用历史 (测试断言用)
    struct CallRecord {
        std::string method;
        std::string workflow_id;
        nlohmann::json args;
    };
    std::vector<CallRecord> call_history() const;

    // ITemporalClient 接口
    WorkflowResult start_workflow_blocking(...) override;
    AsyncJob start_workflow_async(...) override;
    WorkflowStatus poll(...) override;
    bool signal(...) override;
    WorkflowStatus query(...) override;

private:
    std::queue<WorkflowResult> blocking_responses_;
    std::queue<AsyncJob> async_responses_;
    std::queue<WorkflowStatus> poll_responses_;
    std::vector<CallRecord> call_history_;
};

} // namespace pkm::temporal
```

---

## 八、pdk_entry.cpp — 工具注册

```cpp
// pdk/temporal_agent/src/pdk_entry.cpp
// 遵循 shell_tools/provider_agent 的注册模式:
//   - extern "C" hydraforge::PluginInfo pdk_plugin_info
//   - extern "C" void pdk_register_tools(IToolRegistry&)

#include <agenticdsl/contract/itool_registry.h>
#include <agenticdsl/plugin/plugin_info.h>
#include "temporal_agent.h"
#include "temporal_cli_client.h"
#include "temporal_mock_client.h"

// 全局 client 指针 — PluginLoader load 时根据模式选择
// (mock/live 切换由 PluginLoader 侧决定, pdk_entry 不感知)
static std::unique_ptr<pkm::temporal::ITemporalClient> g_client;

extern "C" const hydraforge::PluginInfo pdk_plugin_info = {
    hydraforge::CURRENT_ABI_VERSION,       // abi_version = 2
    "pkm.temporal",                        // name[64]
    0, 1, 0,                               // semver
    "PKM Temporal Agent - Workflow execution via CLI", // description[256]
    "workflow_execution,long_running_tasks,idempotent_operations", // capabilities[512]
    ""                                     // dependencies[256]
};

extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry) {
    // 默认: MockClient (demo 可 safe 启动), main.cpp 可切换为 TemporalCLIClient
    g_client = std::make_unique<pkm::temporal::MockTemporalClient>();

    // 5 个工具 — 每个都通过 g_client 委托
    registry.register_tool_function(
        "temporal/start_workflow",
        ::agenticdsl::ToolMetadata{
            .name = "temporal/start_workflow",
            .description = "Start Temporal Workflow and block until completion. For short tasks (<10s).",
            .domain = "temporal",
            .category = ::agenticdsl::ToolCategory::Execute,
            .min_layer = ::agenticdsl::LayerProfile::Workflow,
            .approval = ::agenticdsl::ApprovalPolicy{
                .requires_approval_in_plan = true,
                .requires_approval_in_agent = true,
                .requires_approval_in_yolo = false,
                .force_approval_always = false
            },
            .allowed_layers = {::agenticdsl::LayerProfile::Workflow}
        },
        [](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
            // ... 参数解析 → g_client->start_workflow_blocking(...)
        }
    );

    // temporal/start_async, temporal/poll, temporal/signal, temporal/query
    // ... (同模式)
}
```

> **Phase 2 待实现**: `extern "C" void pdk_register_agent(AgentDescriptor& desc)` —
> v1.2 架构已定义 `AgentDescriptor` (L3 contract, ADR-0053), 含 `{id, forms, entry_tool, provided_tools, requires_isolation, interface_versions}`。
> PoC 阶段仅实现 `pdk_register_tools` + `pdk_manifest`, 完整 Agent 注册留 Phase 2。

---

## 九、config.json

```json
{
  "$schema": "https://schemas.hydraforge.io/pkm_temporal_demo-v1.json",
  "schema_version": "1.0",
  "app_id": "pkm_temporal_demo",

  "temporal": {
    "host": "localhost:7233",
    "namespace": "pkgm",
    "blocking_task_queue": "blocking-queue",
    "async_task_queue": "async-queue",
    "default_timeout_ms": 300000
  },

  "plugins": [
    {
      "id": "pkm.temporal",
      "path": "@CMAKE_BINARY_DIR@/pdk/temporal_agent/libTemporalAgent.so",
      "type": "so"
    }
  ],

  "demo_scenarios": [
    {
      "name": "Blocking Workflow (1s delay)",
      "tool": "temporal/start_workflow",
      "args": {
        "workflow_type": "BlockingLLMWorkflow",
        "workflow_id": "demo-blocking-001",
        "args_json": "{\"delay\": 1}"
      },
      "expected": { "success": true, "event_count": 11 }
    },
    {
      "name": "Async + Poll (5s delay)",
      "tools": ["temporal/start_async", "temporal/poll"],
      "args": {
        "workflow_type": "AsyncLLMWorkflow",
        "args_json": "{\"delay\": 5}"
      },
      "expected": { "status": "completed" }
    },
    {
      "name": "Idempotency (相同 workflow_id)",
      "tool": "temporal/start_workflow",
      "args": {
        "workflow_type": "BlockingLLMWorkflow",
        "workflow_id": "demo-blocking-001"
      },
      "expected": { "success": true, "error_code": "WORKFLOW_ALREADY_EXISTS" }
    },
    {
      "name": "Timeout (不存在 workflow_id)",
      "tool": "temporal/poll",
      "args": {
        "workflow_id": "nonexistent-wf"
      },
      "expected": { "status": "not_found" }
    }
  ]
}
```

---

## 十、main.cpp 骨架

```cpp
// examples/pkm_temporal_demo/main.cpp
// 参照 pdk_chat_demo/main.cpp 模式
//
// 演示模式:
//   --mock: 使用 MockTemporalClient, 零外部依赖
//   --live: 使用 TemporalCLIClient (需要 temporal CLI + Temporal Server)

#include <iostream>
#include <memory>
#include <filesystem>
#include <nlohmann/json.hpp>

#include <core/engine.h>
#include <agenticdsl/contract/itool_registry.h>
#include <agenticdsl/plugin/plugin_loader.h>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    bool mock_mode = (argc > 1 && std::string(argv[1]) == "--mock");
    bool live_mode = (argc > 1 && std::string(argv[1]) == "--live");

    if (argc < 2) {
        std::cout << "Usage: pkm_temporal_demo [--mock|--live]\n";
        std::cout << "  --mock : CI mode, uses MockTemporalClient (default)\n";
        std::cout << "  --live : Requires temporal CLI + Temporal Server\n";
        return 1;
    }

    // 1. 初始化引擎
    auto engine = std::make_unique<agenticdsl::DSLEngine>(
        std::vector<agenticdsl::ParsedGraph>{});
    auto& registry = engine->get_tool_registry();

    // 2. 加载 Temporal Agent Plugin
    hydraforge::PluginLoader loader;
    std::string plugin_path = "@CMAKE_BINARY_DIR@/pdk/temporal_agent/libTemporalAgent.so";
    if (!fs::exists(plugin_path)) {
        std::cerr << "Plugin not found: " << plugin_path << std::endl;
        return 1;
    }
    if (!loader.load_so(plugin_path, registry)) {
        std::cerr << "Failed to load plugin" << std::endl;
        return 1;
    }
    std::cout << "[main] Loaded: pkm.temporal\n";

    // 3. 演示场景
    std::cout << "\n=== PKM Temporal Agent Demo ===\n";

    // 场景 1: 阻塞 Workflow
    std::cout << "\n[Scenario 1] Blocking Workflow (1s)...\n";
    auto r1 = registry.call_tool("temporal/start_workflow", {
        {"workflow_type", "BlockingLLMWorkflow"},
        {"workflow_id", "demo-blocking-001"},
        {"args_json", R"({"delay": 1})"}
    });
    std::cout << "  Result: " << r1.dump(2) << "\n";

    // 场景 2: 异步 + 轮询
    std::cout << "\n[Scenario 2] Async + Poll...\n";
    auto r2 = registry.call_tool("temporal/start_async", {
        {"workflow_type", "AsyncLLMWorkflow"},
        {"args_json", R"({"delay": 3})"}
    });
    std::string wf_id = r2["workflow_id"];
    std::cout << "  Started: " << wf_id << ", polling...\n";
    // 轮询直到完成
    for (int i = 0; i < 10; i++) {
        auto poll_r = registry.call_tool("temporal/poll", {{"workflow_id", wf_id}});
        std::cout << "  Poll #" << (i+1) << ": " << poll_r["status"] << "\n";
        if (poll_r["status"] == "completed" || poll_r["status"] == "failed") break;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // 场景 3: 幂等性
    std::cout << "\n[Scenario 3] Idempotency check...\n";
    auto r3 = registry.call_tool("temporal/start_workflow", {
        {"workflow_type", "BlockingLLMWorkflow"},
        {"workflow_id", "demo-blocking-001"}  // 同 ID
    });
    std::cout << "  Result: success=" << r3["success"]
              << ", error_code=" << r3.value("error_code", "none") << "\n";

    // 场景 4: 查询不存在的 Workflow
    std::cout << "\n[Scenario 4] Query nonexistent workflow...\n";
    auto r4 = registry.call_tool("temporal/query", {
        {"workflow_id", "nonexistent-wf"}
    });
    std::cout << "  Result: status=" << r4.value("status", "unknown") << "\n";

    std::cout << "\n=== Demo Complete ===\n";
    return 0;
}
```

---

## 十一、pdk_manifest.json

```json
{
  "$schema": "https://schemas.hydraforge.io/pdk-manifest-v1.json",
  "id": "pkm.temporal",
  "name": "PKM Temporal Agent",
  "version": "0.1.0",
  "abi_version": 2,
  "min_host_version": "0.3.0",
  "max_host_version": "1.0.0",
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
  "input_schema": {
    "type": "object",
    "properties": {
      "workflow_type": { "type": "string" },
      "workflow_id": { "type": "string" },
      "args_json": { "type": "string" }
    },
    "required": ["workflow_type", "workflow_id"]
  },
  "output_schema": {
    "type": "object",
    "properties": {
      "success": { "type": "boolean" },
      "workflow_id": { "type": "string" },
      "result": { "type": "object" },
      "duration_ms": { "type": "integer" }
    }
  },
  "resources": {
    "timeout_ms": 600000,
    "max_concurrent": 10
  },
  "publisher": "pkm-team",
  "trust_level": "high"
}
```

---

## 十二、测试策略

### 12.1 Mock 模式单元测试（CI 通过）

```cpp
// pdk/temporal_agent/tests/test_metadata.cpp
TEST_CASE("temporal agent - tool metadata validation") {
    ToolRegistry registry;
    pdk_register_tools(registry);

    // 5 个工具全部注册
    REQUIRE(registry.has_tool("temporal/start_workflow"));
    REQUIRE(registry.has_tool("temporal/start_async"));
    REQUIRE(registry.has_tool("temporal/poll"));
    REQUIRE(registry.has_tool("temporal/signal"));
    REQUIRE(registry.has_tool("temporal/query"));

    // 验证 ToolMetadata 完整性 (category/layer/approval)
}

// pdk/temporal_agent/tests/test_tools_mock.cpp
TEST_CASE("temporal/start_workflow - happy path") {
    auto client = std::make_unique<MockTemporalClient>();
    client->enqueue_response(WorkflowResult{
        .success = true,
        .workflow_id = "wf-001",
        .duration_ms = 1250,
        .event_count = 11
    });
    // ... 验证 call_tool 输出
}

TEST_CASE("temporal/start_workflow - timeout") { ... }
TEST_CASE("temporal/start_async - returns job_id") { ... }
TEST_CASE("temporal/poll - completed status") { ... }
TEST_CASE("temporal/signal - success") { ... }
TEST_CASE("temporal/query - metadata") { ... }
```

### 12.2 端到端 Mock 测试

```cpp
// examples/pkm_temporal_demo/tests/test_temporal_e2e_mock.cpp
TEST_CASE("PKM Temporal Demo - 4 scenarios in mock mode") {
    // 模拟 main.cpp 的 4 个演示场景
    // ... all pass with MockTemporalClient
}
```

### 12.3 CI 集成

```bash
# 编译
cmake --preset tests -DAGENTICDSL_BUILD_EXAMPLES=ON
make -j$(nproc)

# 测试
ctest -R temporal --output-on-failure
# 期望: test_metadata + test_tools_mock + test_temporal_e2e_mock 全绿
```

---

## 十三、实施计划（分 Phase）

### Phase 1a: PDK Plugin 骨架（1-2 天）

| 任务 | 文件 |
|------|------|
| 1.1 创建 `pdk/temporal_agent/` 目录结构 | CMakeLists.txt + pdk_manifest.json |
| 1.2 实现 `ITemporalClient` 接口 | `include/temporal_agent.h` |
| 1.3 实现 `MockTemporalClient` | `include/temporal_mock_client.h` + `.cpp` |
| 1.4 实现 `TemporalCLIClient`（popen 封装）| `include/temporal_cli_client.h` + `.cpp` |
| 1.5 实现 `pdk_entry.cpp`（5 工具注册）| `src/pdk_entry.cpp` |
| 1.6 编译验证: `libTemporalAgent.so` | `cmake .. && make libTemporalAgent` |

### Phase 1b: Demo 项目 + 测试（1-2 天）

| 任务 | 文件 |
|------|------|
| 2.1 创建 `examples/pkm_temporal_demo/` | CMakeLists.txt + config.json + README.md |
| 2.2 实现 `main.cpp`（4 演示场景）| `main.cpp` |
| 2.3 编写单元测试 | `tests/test_metadata.cpp` + `test_tools_mock.cpp` |
| 2.4 编写端到端测试 | `tests/test_temporal_e2e_mock.cpp` |
| 2.5 更新根 `pdk/CMakeLists.txt` | 添加 `add_subdirectory(temporal_agent)` |

### Phase 1c: CI 集成 + 文档（0.5 天）

| 任务 | 文件 |
|------|------|
| 3.1 CI 验证: `ctest -R temporal` 全绿 | — |
| 3.2 更新 `AGENTS.md` 记录 | `AGENTS.md` |
| 3.3 更新 `examples/README.md` | `examples/README.md` |

### Phase 2（Post-PoC，独立 change）: gRPC 直连

| 评估项 | 说明 |
|--------|------|
| protobuf + gRPC 在 CI 的构建成本 | 独立分支验证 FetchContent 构建时间 |
| ITemporalClient 接口不变 | Mock 测试复用保证零回归 |
| TemporalCLIClient 保留 | 作为 `--cli` fallback 模式 |

---

## 十四、与 `pdk_chat_demo` 的模式对照

| 维度 | pdk_chat_demo | pkm_temporal_demo (PoC) |
|------|---------------|------------------------|
| Plugin 数量 | 6 `.so` + 1 SKILL.md | 1 `.so` (temporal_agent) |
| 外部依赖 | DeepSeek API (真实 LLM) | Temporal CLI (可选, --mock 零依赖) |
| Mock 模式 | MockLLMProvider | MockTemporalClient |
| 测试 | 8 test cases, 34 assertions | 8+ test cases |
| 交互方式 | stdin 循环 (chat) | 命令行参数 (演示场景) |
| 代码量 | ~1500 行 | ~800 行 |
| 输入 | 自有 `config.json` | 自有 `config.json` |
| 输出 | 终端 + 事件总线 | 终端 JSON 输出 |

---

## 十五、关键设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| Phase 1 通信 | Temporal CLI (popen) | 零新依赖, shell_tools 已验证模式 |
| Phase 2 通信 | gRPC 直连 | 需要独立评估 protobuf 构建链 |
| Plugin 形态 | 纯 C++ `.so` | 与现有 6 个 PDK Plugin 一致 |
| 测试策略 | Mock 优先 | CI 不依赖 Temporal Server |
| `pdk_register_agent` | 暂不实现 | 代码库未定义此符号 |
| IInteractionBus | PoC 不做 | 保持简单, Phase 2 添加 |
| Layer 权限 | 与 spec 文档一致 | temporal/query 移除 Cognitive layer |

---

## 十六、完成定义 (v0.1 PoC)

- [ ] `cmake --preset tests -DAGENTICDSL_BUILD_EXAMPLES=ON && make` 编译成功
- [ ] `ctest -R temporal --output-on-failure` 全绿 (≥8 test cases)
- [ ] `./pkm_temporal_demo --mock` 4 个演示场景全部 PASS
- [ ] `libTemporalAgent.so` 的 `nm | grep pdk_` 输出: pdk_plugin_info + pdk_register_tools
- [ ] `pdk_manifest.json` 格式符合 agent-as-plugin-manifest 规范
- [ ] 文档: DESIGN.md + README.md 完整

---

**下一步**: 基于本设计创建 OpenSpec change `pkm-temporal-agent-poc` 或直接开始 Phase 1a 实施。
