## Why

Phase 6a roadmap 要求 `pkm_temporal_demo` PDK 骨架落地。当前 `pkgm-temporal-agent` change 已提交（设计文档完整），需要配套的 Demo 项目：
- 展示 temporal_agent PDK Plugin 实际集成
- 验证 4 个演示场景（阻塞/异步/信号/查询）
- `ctest -R temporal` ≥8 test cases

## What Changes

### T3: PDK 骨架 (ITemporalClient + Mock + CLI + pdk_entry, 10h)
- 实现 `ITemporalClient` 抽象接口（解耦真实 gRPC 与 Mock）
- `MockTemporalClient`：内存模拟（无需 Temporal Server）
- CLI 入口 `pkm_temporal_demo`：`--mock` / `--real` 双模式
- `pdk_entry.cpp`：5 工具注册（start_workflow / start_async / poll / signal / query）

### T4: Demo 项目 (main.cpp + 4 场景 + config.json, 8h)
- `examples/pkm_temporal_demo/` 目录结构
- 4 个演示场景 `.agent.md` DSL 文件
- `config.json`：Plugin 加载配置

### T5: 测试 (unit + e2e mock, ≥8 test cases, 6h)
- Unit: MockTemporalClient 5 工具覆盖
- E2E: `--mock` 模式 4 场景端到端

## Capabilities

- `pkm-temporal-demo`: PKM Temporal Demo PDK 骨架

## Impact

- `examples/pkm_temporal_demo/`：新增 Demo 项目目录
- `tests/test_temporal_demo_*.cpp`：新增测试文件（≥2 文件, ≥8 cases）
- 不影响 HydraForge 核心代码

## Non-Goals

- 不实现真实 gRPC 连接（由 pkgm-temporal-agent change 负责）
- 不实现 Temporal Server 部署
- 不实现多租户隔离