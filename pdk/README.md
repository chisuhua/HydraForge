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
