# PKM Temporal Demo PDK 骨架设计

## Context

PKGM 需要 Temporal Workflow 调度能力。`pkgm-temporal-agent` 已定义纯 C++ gRPC 方案（5 工具）。本 change 提供配套 Demo 项目，展示集成方式和验证端到端流程。

## Design

### ITemporalClient 抽象

```cpp
// pdk/temporal_agent/include/itemporal_client.h
class ITemporalClient {
public:
    virtual ~ITemporalClient() = default;
    virtual WorkflowResult start_workflow_blocking(
        const std::string& type, const std::string& id,
        const std::string& queue, const nlohmann::json& args,
        int timeout_ms = 300000) = 0;
    virtual AsyncJob start_workflow_async(
        const std::string& type, const nlohmann::json& args,
        const std::string& queue = "async-queue") = 0;
    virtual WorkflowStatus poll(const std::string& id) = 0;
    virtual bool signal(const std::string& id, const std::string& name,
                        const nlohmann::json& payload) = 0;
    virtual WorkflowStatus query(const std::string& id) = 0;
};
```

### MockTemporalClient

```cpp
class MockTemporalClient : public ITemporalClient {
    std::unordered_map<std::string, MockWorkflow> workflows_;
    std::mutex mutex_;
public:
    // 模拟延迟 (阻塞模式: std::this_thread::sleep_for)
    // 模拟状态机: CREATED → RUNNING → COMPLETED/FAILED
    // 幂等性: 相同 workflow_id 返回已有结果
};
```

### 4 个演示场景

| 场景 | DSL 文件 | ITemporalClient 方法 | 验证点 |
|------|---------|---------------------|--------|
| 阻塞短任务 | `scenario-blocking.agent.md` | `start_workflow_blocking` | 1s delay → COMPLETED |
| 异步长任务+轮询 | `scenario-async-poll.agent.md` | `start_workflow_async` + `poll` | 5s delay → poll → COMPLETED |
| Signal 通信 | `scenario-signal.agent.md` | `start_workflow_async` + `signal` | Signal 改变 Workflow 行为 |
| 幂等性验证 | `scenario-idempotent.agent.md` | `start_workflow_blocking` ×2 | 相同 id 不创建新 Workflow |

### 项目结构

```
examples/pkm_temporal_demo/
├── CMakeLists.txt
├── README.md
├── config.json
├── main.cpp                  # CLI: --mock / --real
├── scenarios/
│   ├── scenario-blocking.agent.md
│   ├── scenario-async-poll.agent.md
│   ├── scenario-signal.agent.md
│   └── scenario-idempotent.agent.md
└── tests/
    ├── test_mock_client.cpp   # Unit: MockTemporalClient 5 工具
    └── test_e2e_scenarios.cpp # E2E: 4 场景 --mock
```

### config.json

```json
{
  "plugins": ["temporal_agent", "loop_agent", "provider_agent"],
  "temporal": {
    "mode": "mock",
    "mock": { "default_delay_ms": 100 }
  }
}
```

## Risks / Trade-offs

| 风险 | 缓解 |
|------|------|
| Mock 行为与真实 Temporal 不一致 | 仅验证接口契约，真实集成由 pkgm-temporal-agent 负责 |
| 测试依赖 `--mock` mode | 所有测试用 mock，CI 无需 Temporal Server |