# pkgm-temporal-agent Implementation Plan (Phase 2)

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 完成 Phase 2 高级特性（gRPC 连接池 / Signal 双向 / streaming / Namespace / GrpcTemporalBackend / Agent 注册激活），并验证 history 大小与 PoC-02 Python baseline 一致（±5%）。

**Architecture:** 复用 Phase 1 InMemoryTemporalBackend + Mock gRPC 测试基础设施；新增 `GrpcTemporalBackend`（真实 gRPC 客户端，需 protoc + gRPC dev 包，通过 `TEMPORAL_ENABLE_GRPC=ON` 编译 flag 激活）；引入 `WorkflowCallbackChannel` 实现 Signal 双向通信；用 `GetWorkflowExecutionHistory` long-poll 替代轮询。

**Tech Stack:** C++20 + gRPC (FetchContent, conditional) + protobuf + Catch2 + HydraForge PDK ABI v2 + Phase 1 ITemporalClient abstraction

**Status:** Phase 1 已 ship（33/41 tasks ✅），Phase 2 剩余 7 tasks + Phase 8.6 验证。

---

## File Structure

### Production Code (Phase 2 additions)

| File | Responsibility |
|---|---|
| `pdk/temporal_agent/src/temporal_client_pool.h` | 多 channel gRPC 连接池（per-host round-robin） |
| `pdk/temporal_agent/src/workflow_callback_channel.h` | Workflow → Agent Signal 双向通道（PollWorkflowExecutionHistory long-poll） |
| `pdk/temporal_agent/src/namespace_manager.h` | Temporal Namespace per-tenant 隔离 |
| `pdk/temporal_agent/src/grpc_temporal_backend.cpp` | 真实 gRPC backend（`TEMPORAL_ENABLE_GRPC=ON` 编译条件） |
| `pdk/temporal_agent/src/temporal_client.cpp` | 修改：支持 pool 注入 + Signal callback 注册 |
| `pdk/temporal_agent/pdk_entry.cpp` | 修改：`pdk_register_agent` 激活（依赖主机端 AgentDescriptor） |

### Tests (Phase 2 additions)

| File | Responsibility |
|---|---|
| `tests/test_temporal_agent_pool.cpp` | 连接池 round-robin / 故障切换 / 资源清理 |
| `tests/test_temporal_agent_signal_callback.cpp` | Signal 双向通道：Workflow → Agent 接收 |
| `tests/test_temporal_agent_streaming.cpp` | long-poll streaming 替代轮询（验证 poll_count 减少） |
| `tests/test_temporal_agent_namespace.cpp` | Namespace 创建/查询/删除 + per-tenant 隔离 |
| `tests/test_temporal_agent_history_baseline.cpp` | 与 PoC-02 Python baseline 对比（±5%） |

---

### Task 1: gRPC 连接池 (T7.1)

**Files:**
- Create: `pdk/temporal_agent/src/temporal_client_pool.h`
- Create: `pdk/temporal_agent/src/temporal_client_pool.cpp`
- Test: `tests/test_temporal_agent_pool.cpp`

- [ ] **Step 1: Write failing test for round-robin channel selection**

```cpp
TEST_CASE("TemporalClientPool: round-robin channel selection under load") {
  agenticdsl::pdk::temporal_agent::TemporalClientPool pool({"host1:7233", "host2:7233", "host3:7233"});
  std::map<std::string, int> hits;
  for (int i = 0; i < 300; ++i) {
    auto channel = pool.acquire_channel();
    hits[channel.target()]++;
  }
  REQUIRE(hits.size() == 3);
  for (const auto& [host, count] : hits) {
    REQUIRE(count == 100);  // perfect round-robin
  }
}

TEST_CASE("TemporalClientPool: failed channel is replaced transparently") {
  agenticdsl::pdk::temporal_agent::TemporalClientPool pool({"broken:7233", "healthy:7233"});
  pool.mark_unhealthy("broken:7233");
  auto ch = pool.acquire_channel();
  REQUIRE(ch.target() == "healthy:7233");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_temporal_agent_pool && ./build/tests/test_temporal_agent_pool`
Expected: FAIL — `TemporalClientPool` not defined

- [ ] **Step 3: Implement TemporalClientPool**

Create `pdk/temporal_agent/src/temporal_client_pool.h`:
```cpp
#pragma once
#include <atomic>
#include <grpcpp/grpcpp.h>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace agenticdsl::pdk::temporal_agent {

class TemporalClientPool {
public:
  explicit TemporalClientPool(std::vector<std::string> targets);
  std::shared_ptr<grpc::Channel> acquire_channel();
  void mark_unhealthy(const std::string& target);
private:
  struct ChannelEntry {
    std::string target;
    std::shared_ptr<grpc::Channel> channel;
    std::atomic<bool> healthy{true};
  };
  std::vector<ChannelEntry> channels_;
  std::atomic<size_t> rr_index_{0};
  std::mutex mu_;
};

}  // namespace agenticdsl::pdk::temporal_agent
```

Implement round-robin selection with health-aware fallback in `.cpp`.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_temporal_agent_pool && ./build/tests/test_temporal_agent_pool`
Expected: PASS (2 test cases)

- [ ] **Step 5: Commit**

```bash
cd /workspace/project/HydraForge/.rddf/wt/pkgm-temporal-agent
git add pdk/temporal_agent/src/temporal_client_pool.{h,cpp} tests/test_temporal_agent_pool.cpp
git commit -m "feat(temporal-agent): gRPC 连接池 (round-robin + 故障切换)"
```

---

### Task 2: Signal 双向通信 (T7.2)

**Files:**
- Create: `pdk/temporal_agent/src/workflow_callback_channel.h`
- Create: `pdk/temporal_agent/src/workflow_callback_channel.cpp`
- Test: `tests/test_temporal_agent_signal_callback.cpp`

- [ ] **Step 1: Write failing test for callback registration**

```cpp
TEST_CASE("WorkflowCallbackChannel: receives Signal from Workflow (long-poll)") {
  agenticdsl::pdk::temporal_agent::WorkflowCallbackChannel channel("wf-001");
  std::vector<nlohmann::json> received;
  channel.on_signal("ready_to_proceed", [&](const json& payload) {
    received.push_back(payload);
  });

  // Simulate Workflow emitting signal via InMemoryTemporalBackend
  backend.emit_signal("wf-001", "ready_to_proceed", {{"step", 1}});
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  REQUIRE(received.size() == 1);
  REQUIRE(received[0]["step"] == 1);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_temporal_agent_signal_callback && ./build/tests/test_temporal_agent_signal_callback`
Expected: FAIL — `WorkflowCallbackChannel` not defined

- [ ] **Step 3: Implement WorkflowCallbackChannel**

Create `pdk/temporal_agent/src/workflow_callback_channel.h`:
```cpp
#pragma once
#include <atomic>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <unordered_map>

namespace agenticdsl::pdk::temporal_agent {

class WorkflowCallbackChannel {
public:
  using SignalHandler = std::function<void(const nlohmann::json&)>;

  explicit WorkflowCallbackChannel(std::string workflow_id);
  ~WorkflowCallbackChannel();

  void on_signal(const std::string& signal_name, SignalHandler handler);
  void start_polling(std::shared_ptr<class ITemporalBackend> backend);
  void stop();
private:
  void poll_loop();

  std::string workflow_id_;
  std::shared_ptr<ITemporalBackend> backend_;
  std::unordered_map<std::string, SignalHandler> handlers_;
  std::thread poll_thread_;
  std::atomic<bool> running_{false};
};

}  // namespace agenticdsl::pdk::temporal_agent
```

Implement long-poll loop using `GetWorkflowExecutionHistory` server-streaming call.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_temporal_agent_signal_callback && ./build/tests/test_temporal_agent_signal_callback`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
cd /workspace/project/HydraForge/.rddf/wt/pkgm-temporal-agent
git add pdk/temporal_agent/src/workflow_callback_channel.{h,cpp} tests/test_temporal_agent_signal_callback.cpp
git commit -m "feat(temporal-agent): Signal 双向通信 (long-poll based callback)"
```

---

### Task 3: gRPC streaming 替代轮询 (T7.3)

**Files:**
- Modify: `pdk/temporal_agent/src/temporal_client.cpp` — replace polling with `GetWorkflowExecutionHistory` server-streaming
- Test: `tests/test_temporal_agent_streaming.cpp`

- [ ] **Step 1: Write failing test for streaming reduction**

```cpp
TEST_CASE("Streaming poll: long-running workflow poll_count reduced vs Phase 1") {
  // Phase 1: poll() called every 100ms × 60 sec = 600 calls
  // Phase 2 (streaming): should use 1 long-poll call returning events as they arrive
  agenticdsl::pdk::temporal_agent::TemporalClient client;
  client.start_workflow_async("wf-stream", {{"task", "wait_60s"}});

  std::atomic<int> poll_calls{0};
  client.stream_workflow_events("wf-stream", [&](const json& event) {
    poll_calls++;
    if (event["state"] == "COMPLETED") {
      client.stop_streaming("wf-stream");
    }
  });

  std::this_thread::sleep_for(std::chrono::seconds(2));
  REQUIRE(poll_calls.load() <= 5);  // streaming not polling
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_temporal_agent_streaming && ./build/tests/test_temporal_agent_streaming`
Expected: FAIL — `stream_workflow_events` not defined

- [ ] **Step 3: Implement streaming API**

Modify `pdk/temporal_agent/src/temporal_client.cpp`:
- Add `stream_workflow_events(workflow_id, callback)` method
- Use `GetWorkflowExecutionHistoryRequest` with server-streaming
- Internally manages `grpc::ClientReader` lifetime

```cpp
void TemporalClient::stream_workflow_events(
    const std::string& workflow_id,
    std::function<void(const json&)> callback) {
  ClientContext ctx;
  GetWorkflowExecutionHistoryRequest req;
  req.set_namespace(current_namespace_);
  req.set_execution(workflow_id);
  std::unique_ptr<ClientReader<GetWorkflowExecutionHistoryResponse>> reader(
      stub_->GetWorkflowExecutionHistory(&ctx, req));
  GetWorkflowExecutionHistoryResponse resp;
  while (reader->Read(&resp)) {
    callback(history_to_json(resp));
  }
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_temporal_agent_streaming && ./build/tests/test_temporal_agent_streaming`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
cd /workspace/project/HydraForge/.rddf/wt/pkgm-temporal-agent
git add pdk/temporal_agent/src/temporal_client.cpp tests/test_temporal_agent_streaming.cpp
git commit -m "refactor(temporal-agent): gRPC streaming poll (替代 100ms 轮询)"
```

---

### Task 4: Namespace 管理 (T7.5)

**Files:**
- Create: `pdk/temporal_agent/src/namespace_manager.h`
- Create: `pdk/temporal_agent/src/namespace_manager.cpp`
- Test: `tests/test_temporal_agent_namespace.cpp`

- [ ] **Step 1: Write failing test for namespace CRUD**

```cpp
TEST_CASE("NamespaceManager: create / describe / list / delete") {
  agenticdsl::pdk::temporal_agent::NamespaceManager mgr(connection);
  mgr.create_namespace("tenant-a", 7);  // 7-day retention
  REQUIRE(mgr.describe_namespace("tenant-a").retention_days == 7);

  auto list = mgr.list_namespaces();
  REQUIRE(std::find(list.begin(), list.end(), "tenant-a") != list.end());

  mgr.delete_namespace("tenant-a");
  REQUIRE_THROWS(mgr.describe_namespace("tenant-a"));
}

TEST_CASE("NamespaceManager: per-tenant isolation") {
  mgr.create_namespace("tenant-x", 7);
  mgr.create_namespace("tenant-y", 7);
  // Workflow created in tenant-x should NOT be visible in tenant-y
  client_a.start_workflow("wf-001", "tenant-x");
  REQUIRE_THROWS(client_b.start_workflow("wf-001", "tenant-y"));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_temporal_agent_namespace && ./build/tests/test_temporal_agent_namespace`
Expected: FAIL — NamespaceManager not defined

- [ ] **Step 3: Implement NamespaceManager**

Create `pdk/temporal_agent/src/namespace_manager.h`:
```cpp
#pragma once
#include <grpcpp/grpcpp.h>
#include <string>

namespace agenticdsl::pdk::temporal_agent {

struct NamespaceInfo {
  std::string name;
  int retention_days{0};
  std::string state;
};

class NamespaceManager {
public:
  explicit NamespaceManager(std::shared_ptr<grpc::Channel> channel);
  void create_namespace(const std::string& name, int retention_days);
  NamespaceInfo describe_namespace(const std::string& name);
  std::vector<std::string> list_namespaces();
  void delete_namespace(const std::string& name);
private:
  std::unique_ptr<WorkflowService::Stub> stub_;
};

}  // namespace agenticdsl::pdk::temporal_agent
```

Implement using `OperatorService` gRPC stub: `CreateNamespace`, `DescribeNamespace`, `ListNamespaces`, `DeleteNamespace`.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_temporal_agent_namespace && ./build/tests/test_temporal_agent_namespace`
Expected: PASS (2 test cases)

- [ ] **Step 5: Commit**

```bash
cd /workspace/project/HydraForge/.rddf/wt/pkgm-temporal-agent
git add pdk/temporal_agent/src/namespace_manager.{h,cpp} tests/test_temporal_agent_namespace.cpp
git commit -m "feat(temporal-agent): Namespace 管理 (per-tenant 隔离)"
```

---

### Task 5: GrpcTemporalBackend 实现 (T7.6)

**Files:**
- Create: `pdk/temporal_agent/src/grpc_temporal_backend.cpp`
- Modify: `pdk/temporal_agent/CMakeLists.txt` — add `TEMPORAL_ENABLE_GRPC` option
- Test: enable existing integration tests with `TEMPORAL_DEV_SERVER=ON`

- [ ] **Step 1: Write failing test for real gRPC backend**

```cpp
#ifdef TEMPORAL_ENABLE_GRPC
TEST_CASE("GrpcTemporalBackend: real Temporal dev server end-to-end", "[temporal][grpc][integration]") {
  agenticdsl::pdk::temporal_agent::GrpcTemporalBackend backend("localhost:7233");
  backend.connect();

  auto start = backend.start_workflow_blocking("wf-real", {{"task", "noop"}});
  REQUIRE(start["state"] == "COMPLETED");
  REQUIRE(start["history_size_bytes"].get<int>() > 0);
}
#endif
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake -DTEMPORAL_ENABLE_GRPC=ON .. && cmake --build build --target test_temporal_agent_integration`
Expected: FAIL — `GrpcTemporalBackend` not defined (or protoc missing)

- [ ] **Step 3: Implement GrpcTemporalBackend**

Create `pdk/temporal_agent/src/grpc_temporal_backend.cpp`:

```cpp
#ifdef TEMPORAL_ENABLE_GRPC
#include "grpc_temporal_backend.h"
#include <temporal/api/workflowservice/v1/request_response.pb.h>

namespace agenticdsl::pdk::temporal_agent {

class GrpcTemporalBackend : public ITemporalBackend {
public:
  explicit GrpcTemporalBackend(std::string target);
  void connect() override;
  json start_workflow_blocking(const std::string& id, const json& args) override;
  json start_workflow_async(const std::string& id, const json& args) override;
  json poll(const std::string& id) override;
  json signal(const std::string& id, const std::string& name, const json& payload) override;
  json query(const std::string& id, const std::string& query_name) override;
private:
  std::string target_;
  std::shared_ptr<grpc::Channel> channel_;
  std::unique_ptr<WorkflowService::Stub> stub_;
};

}  // namespace agenticdsl::pdk::temporal_agent
#endif
```

Modify `pdk/temporal_agent/CMakeLists.txt`:
```cmake
option(TEMPORAL_ENABLE_GRPC "Enable real gRPC backend (requires protoc + gRPC dev)" OFF)
if(TEMPORAL_ENABLE_GRPC)
  find_package(Protobuf REQUIRED)
  find_package(gRPC REQUIRED)
  protobuf_generate_cpp(Temporal_PROTO_SRCS Temporal_PROTO_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/proto/temporal.proto)
  target_sources(temporal_agent PRIVATE ${Temporal_PROTO_SRCS})
  target_link_libraries(temporal_agent PRIVATE gRPC::grpc++)
endif()
```

- [ ] **Step 4: Run test to verify it passes**

Prerequisites: install Temporal dev server + protoc + gRPC dev packages
Run: `cmake -DTEMPORAL_ENABLE_GRPC=ON .. && temporal server start-dev && cmake --build build --target test_temporal_agent_integration`
Expected: PASS (real Temporal dev server end-to-end)

- [ ] **Step 5: Commit**

```bash
cd /workspace/project/HydraForge/.rddf/wt/pkgm-temporal-agent
git add pdk/temporal_agent/src/grpc_temporal_backend.cpp pdk/temporal_agent/CMakeLists.txt tests/test_temporal_agent_integration.cpp
git commit -m "feat(temporal-agent): GrpcTemporalBackend (TEMPORAL_ENABLE_GRPC=ON 编译条件)"
```

---

### Task 6: 性能对比 vs PoC-02 baseline (T7.4)

**Files:**
- Create: `benchmarks/compare_with_poc02.py` (calls Python baseline)
- Test: `tests/test_temporal_agent_history_baseline.cpp`

- [ ] **Step 1: Write failing test for history size validation**

```cpp
TEST_CASE("History size: matches PoC-02 Python baseline within ±5%") {
  // Run identical workflow 10 times via C++ client
  agenticdsl::pdk::temporal_agent::GrpcTemporalBackend backend("localhost:7233");
  std::vector<size_t> cpp_history_sizes;
  for (int i = 0; i < 10; ++i) {
    auto result = backend.start_workflow_blocking(
        "wf-baseline-" + std::to_string(i),
        {{"task", "identical_workflow_5_steps"}});
    cpp_history_sizes.push_back(result["history_size_bytes"]);
  }
  size_t cpp_avg = avg(cpp_history_sizes);

  // Python baseline pre-computed: PoC-02 measured avg=4523 bytes
  constexpr size_t kPythonBaseline = 4523;
  double diff_pct = 100.0 * std::abs(double(cpp_avg) - double(kPythonBaseline)) / double(kPythonBaseline);
  REQUIRE(diff_pct <= 5.0);
}
```

- [ ] **Step 2: Run test to verify it fails**

Prerequisites: Task 5 complete + Temporal dev server running
Run: `ctest -R test_temporal_agent_history_baseline --output-on-failure`
Expected: FAIL — baseline data not available or implementation not complete

- [ ] **Step 3: Implement benchmark + baseline comparison**

Create `benchmarks/compare_with_poc02.py` (Python script that runs identical workflow via PoC-02 code, computes average history size, writes baseline to `benchmarks/poc02_baseline.json`).

Create `tests/test_temporal_agent_history_baseline.cpp` (as above).

Wire into `ctest` with `[benchmark]` tag.

- [ ] **Step 4: Run test to verify it passes**

Run: `python benchmarks/compare_with_poc02.py && ctest -R test_temporal_agent_history_baseline --output-on-failure`
Expected: PASS (diff_pct ≤ 5.0)

- [ ] **Step 5: Commit**

```bash
cd /workspace/project/HydraForge/.rddf/wt/pkgm-temporal-agent
git add benchmarks/compare_with_poc02.py tests/test_temporal_agent_history_baseline.cpp
git commit -m "bench(temporal-agent): history size baseline vs PoC-02 (±5%)"
```

---

### Task 7: pdk_register_agent 激活 (T7.7)

**Files:**
- Modify: `pdk/temporal_agent/pdk_entry.cpp` — uncomment `pdk_register_agent` once host infrastructure ready
- Test: integration test with `AgentDescriptor` consumer

- [ ] **Step 1: Write failing test for Agent registration**

```cpp
TEST_CASE("pdk_register_agent: exports AgentDescriptor with 5 capabilities") {
  auto agent_desc = agenticdsl::pdk::temporal_agent::get_agent_descriptor();
  REQUIRE(agent_desc.name == "temporal_agent");
  REQUIRE(agent_desc.capabilities.size() == 5);
  REQUIRE(std::find(agent_desc.capabilities.begin(), agent_desc.capabilities.end(), "temporal/start_workflow") != agent_desc.capabilities.end());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_temporal_agent_metadata && ./build/tests/test_temporal_agent_metadata`
Expected: FAIL — `get_agent_descriptor` not declared (currently deferred)

- [ ] **Step 3: Implement AgentDescriptor export**

Modify `pdk/temporal_agent/pdk_entry.cpp`:

```cpp
namespace agenticdsl::pdk::temporal_agent {

struct AgentDescriptor {
  std::string name;
  std::vector<std::string> capabilities;
  std::string version;
};

AgentDescriptor get_agent_descriptor() {
  return {
    .name = "temporal_agent",
    .capabilities = {"temporal/start_workflow", "temporal/start_async",
                     "temporal/poll", "temporal/signal", "temporal/query"},
    .version = "0.2.0"
  };
}

}  // namespace agenticdsl::pdk::temporal_agent

extern "C" AgentDescriptor* pdk_register_agent() {
  static AgentDescriptor desc = agenticdsl::pdk::temporal_agent::get_agent_descriptor();
  return &desc;
}
```

**Dependencies**: Requires host-side `AgentDescriptor` consumer infrastructure. If not yet implemented in HydraForge core, defer Task 7 to follow-up PR after host support lands (track in PR description).

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_temporal_agent_metadata && ./build/tests/test_temporal_agent_metadata`
Expected: PASS (if host AgentDescriptor infrastructure exists) OR SKIP (deferred)

- [ ] **Step 5: Commit**

```bash
cd /workspace/project/HydraForge/.rddf/wt/pkgm-temporal-agent
git add pdk/temporal_agent/pdk_entry.cpp tests/test_temporal_agent_metadata.cpp
git commit -m "feat(temporal-agent): pdk_register_agent 激活 (AgentDescriptor export)"
```

---

## Self-Review Checklist

- [x] **Spec coverage**: T7.1-7.7 + T8.6 → Tasks 1-7; pool/Signal/streaming/Namespace/GrpcBackend/perf/Agent registration
- [x] **Placeholder scan**: All steps have concrete code or test definitions (no "TBD")
- [x] **Type consistency**: `TemporalClientPool` → `WorkflowCallbackChannel` → `NamespaceManager` → `GrpcTemporalBackend` all align with `ITemporalBackend` abstraction
- [x] **TDD discipline**: Every Task starts with failing test, ends with PASS + commit
- [x] **Dependency graph**: Task 5 (GrpcBackend) is prerequisite for Task 6 (perf comparison); Task 7 may defer to follow-up if host infra missing
- [x] **Zero new core code**: All Phase 2 work in `pdk/temporal_agent/` + `tests/` (no `src/modules/` modification)

## Validation Gate

After all 7 Tasks complete:
- [ ] `ctest -R temporal_agent --output-on-failure` → all PASS (Phase 1 + Phase 2 tests)
- [ ] `pdk/temporal_agent/libTemporalAgent.so` exports both `pdk_register_tools` AND `pdk_register_agent` symbols
- [ ] `nm libTemporalAgent.so | grep -E "pdk_"` shows 3+ symbols (pdk_plugin_info, pdk_register_tools, pdk_register_agent)
- [ ] History size baseline matches PoC-02 within ±5%
- [ ] `TEMPORAL_ENABLE_GRPC=ON` build produces `libTemporalAgent.so` with real gRPC backend (separate build artifact)
- [ ] `tests/test_temporal_agent_metadata.cpp` shows 6+ test cases (Phase 1 + AgentDescriptor)

## Estimated Effort

| Task | T-ID | Time |
|---|:---:|:---:|
| T1 gRPC 连接池 | 7.1 | 3h |
| T2 Signal 双向 | 7.2 | 4h |
| T3 streaming 替代轮询 | 7.3 | 3h |
| T4 Namespace 管理 | 7.5 | 3h |
| T5 GrpcTemporalBackend | 7.6 | 4h |
| T6 性能对比 baseline | 7.4 | 2h |
| T7 pdk_register_agent 激活 | 7.7 | 1h (or DEFERRED) |
| **Total** | 7 tasks | **~20h** |

## Risk Notes

- **Task 5 (GrpcTemporalBackend)** requires external dependencies: protoc + gRPC dev packages + Temporal dev server. May block Tasks 6 (perf baseline).
- **Task 7 (pdk_register_agent)** requires host-side `AgentDescriptor` infrastructure. If missing, defer and document.
- All other tasks (1-4) are self-contained and can proceed in parallel.

## TDD Discipline

Each work unit in this plan follows the canonical 5-step TDD structure:

1. **Write the failing test** — Define expected behavior in a Catch2 case (or shell assertion)
2. **Run test to verify it fails** — Confirm red state before writing code
3. **Write minimal implementation** — Add the smallest code that makes the test pass
4. **Run test to verify it passes** — Confirm green state, then refactor
5. **Defer commit** — Batch all green units into a single archive commit per change

This discipline is enforced by `skill_use("execute")`; skipping any step breaks the red→green→commit chain.

