# test-coverage Specification

## Purpose
2026-06-30 全项目审计 (`docs/superpowers/plans/2026-06-30-audit-remediation-roadmap.md`) 发现 9 个生产源文件覆盖不足 (4 MISSING + 5 PARTIAL):MISSING — `trace_exporter.cpp` (111 行) / `context_engine.cpp` (193 行) / `http_adapter.cpp` (281 行) / `resource_manager.cpp` (31 行);PARTIAL — `system_nodes.cpp` (28 行) / `llama_adapter_provider.cpp` (124 行) / `llama_adapter.cpp` (Config 结构体 only) / `budget/factory.cpp` + `common/llm/factory.cpp` (各 12 行);**当前覆盖率 31/48 FULL (64.6%) → 目标 45/48 FULL (93.8%)**,新增 8 个测试文件 (`llama_adapter.cpp` 已被 Sprint 5 已存在 test_llm_tool.cpp 部分覆盖,本次不重复)。
## Requirements
### Requirement: trace-exporter-tests

`src/modules/trace/trace_exporter.cpp` MUST 有专用单元测试覆盖核心 API.

#### Scenario: TraceExporter lifecycle works end-to-end

- **WHEN** 测试创建 `TraceExporter`, 调用 `on_node_start` / `on_node_end` / `export_traces` / `export_json` / `reset`
- **THEN** 每个方法返回符合预期的结果
- **AND** 至少 5 个 test case PASS

### Requirement: context-engine-tests

`src/modules/context/context_engine.cpp` MUST 有专用单元测试覆盖 merge/size/trim.

#### Scenario: ContextEngine merge strategy is testable

- **WHEN** 测试构造两个 Context, 调用 `ContextEngine::merge(disjoint)` 和 `merge(overlapping)`
- **THEN** disjoint 全部 keys 合并, overlapping 子覆盖父
- **AND** 至少 4 个 test case PASS

### Requirement: resource-manager-tests

`src/modules/scheduler/resource_manager.cpp` MUST 有专用单元测试覆盖 register/get/serialize.

#### Scenario: ResourceManager CRUD operations work

- **WHEN** 测试 register / get (existing + unknown) / serialize / deserialize
- **THEN** 注册后 get 返回, unknown 返回 null, 序列化往返一致
- **AND** 至少 3 个 test case PASS

### Requirement: http-adapter-tests

`src/common/llm/http_adapter.cpp` MUST 有专用单元测试覆盖 generate/stream/error-mapping.

#### Scenario: HttpLLMAdapter handles HTTP status codes via mock server

- **WHEN** 测试用 `httplib::Server` (端口 0) 启动 mock HTTP server, 配置 HttpLLMAdapter 指向该 server
- **THEN** 200 返回正常结果, 4xx 返回 InvalidRequest, 5xx 返回 ProviderError
- **AND** generate_stream 正确 yield SSE chunks
- **AND** 至少 4 个 test case PASS

### Requirement: system-nodes-tests

`src/modules/system/system_nodes.cpp` MUST 有专用单元测试覆盖 `create_system_nodes()`.

#### Scenario: create_system_nodes returns expected metadata

- **WHEN** 测试调用 `create_system_nodes()`
- **THEN** 返回预期数量的 Node, 每个 Node 有正确的 type 和 layer
- **AND** 至少 2 个 test case PASS

### Requirement: llama-adapter-provider-tests

`src/common/llm/llama_adapter_provider.cpp` MUST 有专用单元测试覆盖 `create_adapter(config)`.

#### Scenario: LlamaAdapterProvider factory works for valid/invalid configs

- **WHEN** 测试调用 `create_adapter(valid_config)` 和 `create_adapter(invalid_config)`
- **THEN** valid 返回非 null adapter, invalid 返回 null 或抛异常
- **AND** 至少 2 个 test case PASS

### Requirement: budget-factory-tests

`src/modules/budget/factory.cpp` MUST 有专用单元测试覆盖 `create_controller()`.

#### Scenario: budget::create_controller returns IBudgetController

- **WHEN** 测试调用 `budget::create_controller()` 和 `budget::create_controller(json_config)`
- **THEN** 返回 IBudgetController 指针, 可正常调用 budget 方法
- **AND** 至少 2 个 test case PASS

### Requirement: llm-factory-tests

`src/common/llm/factory.cpp` MUST 有专用单元测试覆盖 `create_provider_factory()`.

#### Scenario: llm::create_provider_factory dispatches by backend

- **WHEN** 测试调用 `create_provider_factory()` 和 dispatch 不同 backend name (mock / llama / cloud)
- **THEN** 返回 IProviderFactory, dispatch 返回正确的 LLM provider 实例
- **AND** 至少 2 个 test case PASS

### Requirement: coverage-matrix-target

Sprint 16 ship 后, FULL 覆盖率 MUST ≥ 83% (40/48 文件).

#### Scenario: coverage matrix shows 83%+ after Sprint 16

- **WHEN** 运行 `tools/coverage_matrix.py` 或等价检查
- **THEN** FULL 覆盖率 ≥ 83% (was 64.6% before Sprint 16)
- **AND** MISSING 文件数 = 0 (was 4 before Sprint 16)
- **AND** PARTIAL 文件数 ≤ 1 (was 5 before Sprint 16, 仅 llama_adapter.cpp 保留 PARTIAL 因需要 llama.cpp 运行时依赖)

