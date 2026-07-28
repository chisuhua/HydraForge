## Why

PKGM 的 adr-000 需要决定"LLM 调用边界放哪一层"。PoC-02 已验证 Temporal blocking vs async polling 数据。当前 OpenClaw 没有 HTTP API，HydraForge 的 PDK 提供了标准化 Agent 契约。

Temporal Agent 作为纯 C++ PDK Plugin，通过 gRPC 直连 Temporal Server，与 HydraForge 全 C++ 技术栈一致。

### 为什么纯 C++（非 Python）

1. **技术栈统一**：HydraForge 全部使用 C++20 + CMake，Python proxy 破坏技术栈一致性
2. **进程模型清晰**：无 subprocess 管理开销，PluginLoader unload 即清理
3. **延迟更低**：gRPC 直连比 popen 省去 IPC 开销（~10-50ms/subprocess）
4. **部署简单**：单一 `.so` 文件，无需 Python + temporalio 环境

## What Changes

新增 `pdk/temporal_agent/` PDK Plugin（纯 C++），提供 5 个工具。

### 两种实现路径

| | 方案 A: gRPC 直连（推荐） | 方案 B: Temporal CLI（快速MVP） |
|------|---------------------------|------------------------------|
| 依赖 | protobuf + gRPC (FetchContent) | `temporal` CLI 二进制 |
| 功能完整度 | 完整 Temporal API | 基础 workflow 操作 |
| 构建复杂度 | 中（需 protobuf 编译） | 低（仅 popen + JSON） |
| 启动延迟 | gRPC 连接池复用 | 每次 popen 新进程 |
| 适合阶段 | Phase 1 直接实现 | 0-day 验证，随后迁移到 A |

### 设计方案 A（推荐）：C++ gRPC 直连

```
┌───────── HydraForge (C++) ──────────────────────┐
│  PKGM Pipeline Agent (.agent.md DSL)               │
│  ├─ call_tool("temporal/start_workflow", ...)      │
│  └─ call_tool("temporal/poll", ...)                │
│                                                    │
│  Temporal Agent (pdk/temporal_agent/lib.so)        │
│  ├─ temporal_client.cpp: gRPC → 连接池             │
│  ├─ pdk_entry.cpp: 5 工具注册                      │
│  └─ IInteractionBus: 事件发送                      │
└───────────────────┬────────────────────────────────┘
                    │ gRPC (protobuf)
                    ▼
┌───────── Temporal Server (:7233) ─────────────────┐
│  Namespace: pkgm                                    │
│  Task Queues: blocking-queue / async-queue          │
│  Workers: Python PoC-02 (已有)                      │
└────────────────────────────────────────────────────┘
```

### 工具列表

| 工具 | 用途 |
|------|------|
| `temporal/start_workflow` | 启动并阻塞等待（短任务） |
| `temporal/start_async` | 异步创建，返回 job_id（长任务） |
| `temporal/poll` | 轮询 job 状态 |
| `temporal/signal` | 发送 Signal |
| `temporal/query` | 查询 Workflow 状态 |

## Impact

- `pdk/temporal_agent/`：新增 PDK Plugin 目录
- `external/temporal-proto/`：temporal-sdk-protos（FetchContent）
- `examples/pkm_agent/`：新增 PKGM 示例
- 不涉及 HydraForge 核心代码修改

## Non-Goals

- 不实现 Temporal Server 部署
- 不实现多租户隔离（PKGM 业务层处理）
- 不实现 Workflow 的 DSL 自动生成
