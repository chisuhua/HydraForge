# PKM Agent - Temporal 工作流编排示例

> **pkgm-temporal-agent** PDK Plugin 使用示例
> 关联: `pdk/temporal_agent/` (libTemporalAgent.so)
> 关联 OpenSpec: `openspec/changes/pkgm-temporal-agent/`

## 概述

PKM Agent 演示通过 HydraForge PDK Plugin 加载 `libTemporalAgent.so`，
调用 5 个 Temporal 工作流编排工具完成持久化任务执行 (durable execution)。

## Plugin 工具清单

| 工具 | 类型 | 审批 | 说明 |
|------|------|------|------|
| `temporal/start_workflow` | Execute | plan+agent | 阻塞启动 + 轮询直到完成 |
| `temporal/start_async` | Execute | plan+agent | 异步启动 + 立即返回 |
| `temporal/poll` | ReadOnly | 无 | 轮询工作流状态 |
| `temporal/signal` | Execute | plan+agent | 发送信号 |
| `temporal/query` | ReadOnly | 无 | 查询只读元数据 |

## 编译

```bash
# 在项目根目录
cmake --preset tests -B build/tests
cmake --build build/tests --target TemporalAgent -j$(nproc)

# 产物: build/tests/pdk/temporal_agent/libTemporalAgent.so
```

## 加载与调用

```cpp
#include "agenticdsl/plugin/plugin_loader.h"
#include "agenticdsl/contract/itool_registry.h"

auto engine = DSLEngine::from_markdown(dsl_source);
engine.load_plugin("pdk/temporal_agent");  // dlopen libTemporalAgent.so

// 调用 temporal/start_async
std::unordered_map<std::string, std::string> args = {
  {"workflow_type", "ExampleWorkflow"},
  {"task_queue", "task-queue-1"},
  {"input_json", R"({"key":"value"})"},
  {"workflow_id", "wf-demo-001"}
};
auto result = engine.call_tool("temporal/start_async", args);
// result["status"] == "RUNNING"
```

## DSL 调用示例 (.agent.md)

```markdown
# Temporal Workflow Agent

## workflow
- start:
    - tool_call: temporal/start_async
      args:
        workflow_type: "DelayWorkflow"
        task_queue: "task-queue-1"
        input_json: '{"delay_ms":5000}'
        workflow_id: "wf-demo-001"
    - tool_call: temporal/poll
      args:
        workflow_id: "wf-demo-001"
        timeout_seconds: 10
```

## 事件集成

Plugin 在关键操作后发射 IInteractionBus 事件 (通过 EventEmitFunc 回调注入):

- `temporal.workflow.start` - 工作流启动
- `temporal.workflow.complete` - 工作流完成
- `temporal.workflow.failed` - 工作流失败
- `temporal.poll` - 轮询 (含 poll_count)

```cpp
TemporalClient::instance().set_event_emitter(
  [](const std::string& event_type, const nlohmann::json& payload) {
    // 转发到 IInteractionBus::emit()
  });
```

## gRPC 集成状态 (Phase 2)

当前实现使用 `InMemoryTemporalBackend` (进程内模拟), 零 gRPC 依赖,
适用于测试与开发。Phase 2 将替换为 `GrpcTemporalBackend` (真实 gRPC):

- 启用方式: `cmake -DTEMPORAL_ENABLE_GRPC=ON`
- 依赖: `protoc` + `gRPC dev` + `temporalio/api v1.27.0`
- 接口不变: 工具签名 + ToolMetadata + 事件契约完全保持

## 测试

```bash
cd build/tests && ctest -R temporal_agent --output-on-failure
```

真实 Temporal dev server 测试需编译时定义 `TEMPORAL_DEV_SERVER`:

```bash
cmake --preset tests -B build/tests -DCMAKE_CXX_FLAGS="-DTEMPORAL_DEV_SERVER"
cmake --build build/tests --target test_temporal_agent_integration
```
