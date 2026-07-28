## T3: PDK 骨架 (10h)

### ITemporalClient + Mock

- [x] 1.1 创建 `include/agenticdsl/pdk/itemporal_client.h` — ITemporalClient 纯虚接口
- [x] 1.2 创建 `pdk/temporal_agent/mock_client.h/cpp` — MockTemporalClient 实现
  - 内存状态机: CREATED → RUNNING → COMPLETED/FAILED
  - 幂等性: workflow_id 查重
  - 可配置模拟延迟
- [x] 1.3 创建 `pdk/temporal_agent/pdk_entry.cpp` — 5 工具注册
  - `temporal/start_workflow` → `client->start_workflow_blocking(...)`
  - `temporal/start_async` → `client->start_workflow_async(...)`
  - `temporal/poll` → `client->poll(...)`
  - `temporal/signal` → `client->signal(...)`
  - `temporal/query` → `client->query(...)`
- [x] 1.4 ToolMetadata: 每个工具完整 category/layer/approval

### CLI

- [x] 2.1 `main.cpp` — CLI 入口
  - `--mock`: 使用 MockTemporalClient
  - `--real`: 使用 gRPC TemporalClient (stub, 实际连接由 pkgm-temporal-agent 实现)
  - `--scenario <name>`: 指定场景运行

### CMake

- [x] 3.1 `examples/pkm_temporal_demo/CMakeLists.txt` — 编译目标
- [x] 3.2 构建验证: `cmake --preset tests && make pkm_temporal_demo`

---

## T4: Demo 项目 (8h)

### 场景 DSL

- [x] 4.1 `scenario-blocking.agent.md` — 阻塞短任务 (call_tool temporal/start_workflow)
- [x] 4.2 `scenario-async-poll.agent.md` — 异步+轮询 (start_async → loop(poll))
- [x] 4.3 `scenario-signal.agent.md` — Signal (start_async → signal → poll)
- [x] 4.4 `scenario-idempotent.agent.md` — 幂等性 (start_workflow ×2, 相同 id)

### 配置与文档

- [x] 5.1 `config.json` — Plugin 加载 + temporal mode 配置
- [x] 5.2 `README.md` — 编译/运行说明 + 场景描述

---

## T5: 测试 (6h)

### Unit Tests

- [x] 6.1 `test_mock_client.cpp` — MockTemporalClient 5 工具覆盖
  - start_workflow_blocking: 正常完成 → COMPLETED
  - start_workflow_async: 立即返回 job_id
  - poll: CREATED → RUNNING → COMPLETED 状态演进
  - signal: 改变 MockWorkflow 内部状态
  - query: 返回当前状态
  - 幂等性: 相同 workflow_id → 返回已有结果
  - 超时: blocking 超时 → TIMEOUT 错误

### E2E Tests

- [x] 6.2 `test_e2e_scenarios.cpp` — 4 场景 --mock 端到端
  - blocking: 启动 → 等待 → COMPLETED
  - async-poll: 启动 → 轮询 → COMPLETED
  - signal: 启动 → Signal → 状态变更确认
  - idempotent: 两次相同 id → 同一结果

### 验收

- [x] 7.1 `ctest -R temporal` ≥8 cases 全绿
- [x] 7.2 `./pkm_temporal_demo --mock` 4 场景全部 PASS