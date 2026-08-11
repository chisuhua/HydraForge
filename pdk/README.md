# PDK Plugins (Plugin Development Kit)

HydraForge PDK 插件目录。每个插件编译为独立 `.so`，通过 `DSLEngine::load_plugin()` 或 `PluginLoader` 加载。

## 插件清单

| 插件 | 产物 | 工具数 | 说明 | 关联 ADR/Change |
|------|------|--------|------|------------------|
| `session_agent/` | libSessionAgent.so | 5 | 多轮会话持久化与分支 | ADR-0033 |
| `llama_engine/` | libhydraforge_llama_engine.so | 12 | llama.cpp 推理引擎 | C14 Phase 5 |
| `model_router/` | (3 .so) | 4 | 模型路由策略 | ADR-0034 |
| `loop_agent/` | libLoopAgent.so | - | Chat Loop Agent (DSL loader) | pdk_chat_demo |
| `provider_agent/` | libProviderAgent.so | - | LLM Provider + 凭据管理 | pdk_chat_demo |
| `budget_agent/` | libBudgetAgent.so | - | 预算查询 + 告警 | pdk_chat_demo |
| `fs_tools/` | libFsTools.so | - | 文件系统 read/write/list/exists | - |
| `shell_tools/` | libShellTools.so | - | shell exec + 危险命令黑名单 | - |
| `g1_coding_assistant/` | - | - | 2-step ReAct loop + G3 orchestration | Phase 6 W1 |
| `g3_knowledge_base/` | - | - | retrieval + LLM Q&A | Phase 6 W1 |
| **`temporal_agent/`** | **libTemporalAgent.so** | **5** | **Temporal 工作流编排 (start/poll/signal/query)** | **pkgm-temporal-agent** |

## Temporal Agent (`pdk/temporal_agent/`)

> **STATUS**: Phase 1 ship (InMemoryTemporalBackend, 零 gRPC 依赖)
> **关联 OpenSpec**: `openspec/changes/pkgm-temporal-agent/`

### 注册工具 (5 个)

| 命名空间 | 工具 | 类型 | 审批策略 |
|---------|------|------|---------|
| `temporal/` | `start_workflow` | Execute | plan + agent |
| `temporal/` | `start_async` | Execute | plan + agent |
| `temporal/` | `poll` | ReadOnly | 无审批 |
| `temporal/` | `signal` | Execute | plan + agent |
| `temporal/` | `query` | ReadOnly | 无审批 |

### 架构

采用抽象后端模式 (`ITemporalBackend`):
- **InMemoryTemporalBackend** (默认): 进程内模拟, 零 gRPC 依赖, 适用于测试与开发
- **GrpcTemporalBackend** (Phase 2): 真实 gRPC + WorkflowService stub, 需 `protoc` + gRPC dev 包

### 构建

```bash
cmake --preset tests -B build/tests
cmake --build build/tests --target TemporalAgent -j$(nproc)
# 产物: build/tests/pdk/temporal_agent/libTemporalAgent.so
```

### 测试

```bash
cd build/tests && ctest -R temporal_agent --output-on-failure
# 3 个测试, 全部 PASS
```

### 事件集成

通过 `TemporalClient::set_event_emitter()` 注入回调, 发射 4 种事件:
- `temporal.workflow.start` / `temporal.workflow.complete` / `temporal.workflow.failed` / `temporal.poll`

### Phase 2 (deferred)

- gRPC 真实连接池 (多 channel 并发)
- Signal 双向通信 (Workflow -> Agent 回调)
- gRPC streaming (替代轮询)
- Temporal Namespace 管理

## SafeExec 实战 (Phase 6a 新增)

> **STATUS**: SafeExec 沙箱执行封装已升级到 `std::jthread + std::stop_source` (取代旧 `std::async`)
> **超时立即返回**: caller 在 ≤ timeout + 50ms grace 后立即抛 `runtime_error`, 不再阻塞至 fn 完成
> **关联 OpenSpec**: `openspec/changes/archive/2026-08-10-pdk-safe-exec-tests/`

### 超时控制 (Stop Token 协同)

```cpp
#include "agenticdsl/pdk/safe_exec.h"
using namespace hydraforge::pdk;

auto result = SafeExec()
    .with_timeout(5s)       // fn 最长执行 5s
    .with_grace_period(50ms) // 超时后给 50ms 清理宽限
    .run([] {
      // 你的领域逻辑 (如 LLM 调用、文件 IO、网络请求)
      return compute_heavy();
    });
// 5s 后立即抛 std::runtime_error (非阻塞至 fn 完成)
```

### 异常传播

```cpp
try {
  SafeExec().with_timeout(1s).run([] {
    throw std::runtime_error("disk full");
  });
} catch (const std::runtime_error& e) {
  // e.what() == "disk full" (透传, 不包装)
}
```

### 与 DECLARE_TOOL 组合 (5 行领域逻辑)

```cpp
DECLARE_TOOL(my_tool, "示例工具", ReadOnly, "agent",
  return SafeExec()
    .with_timeout(2s)
    .run([&] {
      return __pdk_args["input"].get<std::string>();
    });
)
```

## 3 种 Agent Loop 选择指南

| Loop | 适用场景 | 状态 |
|------|---------|:----:|
| **React** (思考 → 行动 → 观察) | 单 agent 工具调用、ReAct 模式 | ✅ Sprint 4 ship |
| **PlanExecute** (规划 → 执行 → 验证) | 多步骤任务、规划验证 | ✅ Sprint 20 ship |
| **ForkJoin** (并行分支 → 合并) | 并行任务聚合 | ✅ Sprint 20 ship |

```cpp
DEFINE_AGENT(coding_assistant, AgentLoopType::React);    // 单 agent ReAct
DEFINE_AGENT(parallel_analyzer, AgentLoopType::ForkJoin); // 多 worker 并行
```

## AgentForge 衔接

AgentForge (Phase 6b MVP) 通过 PDK 调用 DSLEngine:

```cpp
#include "agenticdsl/pdk/pdk.h"
#include "core/engine.h"
using namespace hydraforge::pdk;

DEFINE_AGENT(code_reviewer, AgentLoopType::React);
DECLARE_TOOL(lint_file, "Linter", ReadOnly, "agent",
  return SafeExec().with_timeout(5s).run([&] {
    return lint(__pdk_args["file"].get<std::string>());
  });
)

int main() {
  auto engine = agenticdsl::DSLEngine::from_markdown("workflow.agent.md");
  code_reviewerAgent agent(std::move(engine), std::make_shared<InMemoryBus>());
  return agent.run("review src/main.cpp").final_context.working.empty() ? 0 : 1;
}
```

完整 AgentForge MVP blueprint: `docs/proposals/implementation/agentforge-mvp-blueprint.md`

## 通用构建

```bash
# 构建所有 PDK 插件
cmake --preset debug -B build/debug
cmake --build build/debug -j$(nproc)

# 测试
cmake --preset tests -B build/tests
cmake --build build/tests -j$(nproc)
cd build/tests && ctest --output-on-failure
```

## ABI 版本

当前 ABI 版本: **2** (per ADR-0041, `hydraforge::CURRENT_ABI_VERSION`)
- PluginInfo V2 (含 `dependencies` 字段)
- PluginLoader dual dispatch: V1 + V2 .so 均可加载
