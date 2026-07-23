# PKM Temporal Agent Demo (PoC)

> **PKGM Temporal 集成的 PoC demo — 基于 HydraForge PDK Plugin**
> 关联设计: [DESIGN.md](./DESIGN.md)
> 来源: `openspec/changes/pkgm-temporal-agent/`

## 概述

`pkm_temporal_demo` 演示 PKGM 的 Temporal Workflow 集成：

- **Temporal Agent 作为 PDK Plugin** — 独立 `.so`，5 个工具
- **Temporal CLI 封装** — 零新依赖（`popen` + JSON，复用 `shell_tools` 模式）
- **Mock 模式优先** — 不依赖外部 Temporal Server，CI 即可验证
- **渐进升级路径** — Phase 2 迁移到 gRPC 直连时接口不变

## 编译

```bash
# 在项目根目录
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DAGENTICDSL_BUILD_EXAMPLES=ON
make -j$(nproc) TemporalAgent pkm_temporal_demo
```

编译完成后:
- `build/pdk/temporal_agent/libTemporalAgent.so` — PDK Plugin
- `build/examples/pkm_temporal_demo/pkm_temporal_demo` — Demo 可执行文件

## 运行

### Mock 模式（CI 验证，零依赖）

```bash
./build/examples/pkm_temporal_demo/pkm_temporal_demo --mock
```

输出 4 个演示场景：

```
=== PKM Temporal Agent Demo ===

[Scenario 1] Blocking Workflow (1s)...
  Result: {"success":true,"workflow_id":"demo-blocking-001","duration_ms":1250,...}

[Scenario 2] Async + Poll...
  Started: demo-async-001, polling...
  Poll #1: running
  Poll #2: running
  Poll #3: completed

[Scenario 3] Idempotency check...
  Result: success=true, error_code=WORKFLOW_ALREADY_EXISTS

[Scenario 4] Query nonexistent workflow...
  Result: status=not_found

=== Demo Complete ===
```

### Live 模式（需要 Temporal CLI + Temporal Server）

```bash
# 前提: Temporal Server 运行在 localhost:7233
temporal server start-dev &

# 启动 demo
./build/examples/pkm_temporal_demo/pkm_temporal_demo --live
```

## Plugin 工具列表

| 工具 | 用途 | 模式 |
|------|------|------|
| `temporal/start_workflow` | 启动 Workflow 并阻塞等待（短任务） | 同步 |
| `temporal/start_async` | 异步创建 Workflow，返回 job_id（长任务） | 异步 |
| `temporal/poll` | 轮询 Workflow 状态 | 查询 |
| `temporal/signal` | 发送 Signal 到运行中的 Workflow | 控制 |
| `temporal/query` | 查询 Workflow 元数据 | 只读 |

## 测试

```bash
cd build
ctest -R temporal --output-on-failure
```

测试用例:

- `test_metadata`: 工具注册覆盖率 + ToolMetadata 完整性（5 assertions）
- `test_tools_mock`: Mock 模式下 5 个工具输入/输出验证（6 test cases）
- `test_temporal_e2e_mock`: 端到端 mock 4 场景（4 assertions）

预期全部 PASS: `8 test cases, 15+ assertions`

## 架构概览

```
examples/pkm_temporal_demo/main.cpp (编排器)
       │ PluginLoader::load_so()
       ▼
pdk/temporal_agent/libTemporalAgent.so (PDK Plugin)
       │ ITemporalClient 抽象接口
       ├── MockTemporalClient     (--mock, 预置 JSON)
       └── TemporalCLIClient      (--live, popen temporal CLI)
              │
              ▼
         /usr/local/bin/temporal
              │ gRPC
              ▼
         Temporal Server (:7233)
```

## 与 `pdk_chat_demo` 的关系

本 demo 与 `examples/pdk_chat_demo/` 使用相同的 HydraForge 基础设施：

- 同一个 `PluginLoader` → 加载 `.so`
- 同一个 `IToolRegistry` → 工具注册/调用
- 同一个 `DSLEngine` → 引擎初始化
- 同一个 Mock 模式策略

## 设计文档

完整设计见 [DESIGN.md](./DESIGN.md)（~400 行），包括:

- 完整架构图 + 组件关系
- ITemporalClient 接口契约
- pdk_entry.cpp 注册代码
- MockClient 测试策略
- Phase 1→2 gRPC 升级路径
- 与原始 `pkgm-temporal-agent` change 的对应关系

## 常见问题

| 问题 | 原因 | 解决 |
|------|------|------|
| `temporal: command not found` | 未安装 Temporal CLI | 使用 `--mock` 模式，或 `brew install temporal` / `curl -sSf https://temporal.download/cli.sh \| sh` |
| `Connection refused (localhost:7233)` | Temporal Server 未启动 | `temporal server start-dev` |
| `pdk_register_tools` 符号未找到 | Plugin 未编译 | `make TemporalAgent -j$(nproc)` |
