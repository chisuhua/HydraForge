## Phase 1：C++ gRPC 直连实现

### 1. 基础设施

- [ ] 1.1 创建 `pdk/temporal_agent/` 目录结构（CMakeLists.txt + pdk_manifest.json + include/ + src/ + tests/）
- [ ] 1.2 CMakeLists.txt：配置 protobuf + gRPC FetchContent
  - `find_package(Protobuf CONFIG)`
  - `find_package(gRPC CONFIG)`
  - `FetchContent(temporalio/api.git v1.27.0)` → `protobuf_generate_cpp`
- [ ] 1.3 本地 protobuf 编译验证：`cmake .. && make`（生成 `.grpc.pb.cc`）
- [ ] 1.4 实现 `pdk_plugin_info`（abi_version=2，capabilities，description）

### 2. gRPC 客户端

- [ ] 2.1 实现 `temporal_client.h/cpp` — 单例连接池
  - `connect(host)` → `grpc::CreateChannel` + `WorkflowService::NewStub`
  - `shutdown()` → 清理 stub + channel
- [ ] 2.2 实现 `start_workflow_blocking` — StartWorkflowExecution + 轮询 DescribeWorkflowExecution
- [ ] 2.3 实现 `start_workflow_async` — StartWorkflowExecution + 立即返回
- [ ] 2.4 实现 `poll` — DescribeWorkflowExecution
- [ ] 2.5 实现 `signal` — SignalWorkflowExecution
- [ ] 2.6 实现 `query` — DescribeWorkflowExecution（只读元数据）
- [ ] 2.7 错误码映射表：gRPC Status ↔ ErrorCode ↔ ToolResult

### 3. 工具注册

- [ ] 3.1 注册 `temporal/start_workflow` → TemporalClient::start_workflow_blocking
- [ ] 3.2 注册 `temporal/start_async` → TemporalClient::start_workflow_async
- [ ] 3.3 注册 `temporal/poll` → TemporalClient::poll
- [ ] 3.4 注册 `temporal/signal` → TemporalClient::signal
- [ ] 3.5 注册 `temporal/query` → TemporalClient::query
- [ ] 3.6 每个工具注册完整的 ToolMetadata（category/layer/approval）
- [ ] 3.7 实现 `pdk_register_agent`（AgentDescriptor）
- [ ] 3.8 `libTemporalAgent.so` 编译成功 + `nm | grep pdk_` 符号验证

### 4. 事件集成

- [ ] 4.1 IInteractionBus 事件发送（若 PluginLoader 提供 bus 引用）
  - `temporal.workflow.start` / `temporal.workflow.complete` / `temporal.workflow.failed`
  - `temporal.poll`（记录 poll_count）
- [ ] 4.2 Mock 模式下事件验证（test）

### 5. 测试

- [ ] 5.1 `test_metadata.cpp`：工具注册覆盖率（5/5）+ schema 校验 + ToolMetadata 完整性
- [ ] 5.2 `test_client.cpp`：gRPC 连接建立 + 错误码映射 + 幂等性模拟（mock gRPC）
- [ ] 5.3 `test_integration.cpp`（需 Temporal dev server）：
  - 阻塞 Workflow 端到端（1s delay）→ 验证 history_size_bytes / event_count
  - 异步 + 轮询（5s delay）→ 验证 poll → completed
  - 幂等性（相同 workflow_id 不创建新 Workflow）
  - 超时场景（workflow_id 不存在 → WORKFLOW_NOT_FOUND）
- [ ] 5.4 `ctest -R temporal_agent` 全绿

### 6. 文档与示例

- [ ] 6.1 创建 `examples/pkm_agent/`
  - `README.md`：编译 + 运行说明
  - `config.json`：Plugin 配置（加载 temporal_agent + loop_agent + provider_agent）
  - `.agent.md` DSL 文件：演示 `call_tool("temporal/start_workflow", ...)` 调用
- [ ] 6.2 更新 `pdk/README.md` 添加 Temporal Agent 条目

---

## Phase 2：高级特性（Phase 1 ship 后）

- [ ] 7.1 gRPC 连接池（多 channel 并发，当前为单 channel）
- [ ] 7.2 Signal 双向通信（Workflow → Agent 回调）
- [ ] 7.3 gRPC streaming（替代轮询，用 `GetWorkflowExecutionHistory` long-poll）
- [ ] 7.4 性能对比：vs PoC-02 Python baseline（latency / history size）
- [ ] 7.5 Temporal Namespace 管理（per-tenant 隔离）

---

## 验收标准（PKGM 侧）

- [ ] 8.1 编译：`cmake --preset tests && make libTemporalAgent.so`
- [ ] 8.2 `PluginLoader::load_so("libTemporalAgent.so")` 成功
- [ ] 8.3 `temporal/start_workflow` 可通过 ToolRegistry 调用
- [ ] 8.4 `temporal/start_async` + `temporal/poll` 端到端通过
- [ ] 8.5 幂等性：相同 workflow_id 返回相同结果
- [ ] 8.6 History 大小与 PoC-02 baseline 一致（±5%）
- [ ] 8.7 错误场景覆盖（Temporal 不可达 / 超时 / 参数非法）
