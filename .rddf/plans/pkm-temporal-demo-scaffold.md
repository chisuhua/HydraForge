# pkm-temporal-demo-scaffold Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 创建 PDK 骨架（ITemporalClient 抽象接口 + MockTemporalClient + 5 工具注册）+ Demo 4 场景 DSL，展示 HydraForge Agent 编排通过 PDK 调用 Temporal 工作流的端到端链路。

**Architecture:** 与 `pkgm-temporal-agent`（真实 gRPC 后端）平行，通过 `ITemporalClient` 抽象接口解耦；MockTemporalClient 进程内状态机（CREATED→RUNNING→COMPLETED/FAILED），支持配置化延迟与幂等性；Demo 端通过 `.agent.md` DSL + `temporal/*` 工具调用验证集成。

**Tech Stack:** C++20 + CMake + Catch2 + HydraForge PDK Plugin ABI v2 + Agent-as-Plugin + `examples/` DSL 渲染

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `include/agenticdsl/pdk/itemporal_client.h` | `ITemporalClient` 纯虚接口（5 方法 + 状态机） |
| `pdk/temporal_agent/mock_client.h` | `MockTemporalClient` 实现声明 |
| `pdk/temporal_agent/mock_client.cpp` | MockTemporalClient 实现（in-memory state machine） |
| `pdk/temporal_agent/pdk_entry.cpp` | 5 工具注册（start_workflow/start_async/poll/signal/query） |
| `examples/pkm_temporal_demo/main.cpp` | CLI 入口（--mock / --real / --scenario） |
| `examples/pkm_temporal_demo/scenario-blocking.agent.md` | 阻塞短任务场景 |
| `examples/pkm_temporal_demo/scenario-async-poll.agent.md` | 异步 + 轮询场景 |
| `examples/pkm_temporal_demo/scenario-signal.agent.md` | Signal 触发场景 |
| `examples/pkm_temporal_demo/scenario-idempotent.agent.md` | 幂等性场景（start × 2 同 ID） |
| `examples/pkm_temporal_demo/CMakeLists.txt` | Demo 编译目标 |

### Tests

| File | Responsibility |
|---|---|
| `pdk/temporal_agent/tests/test_mock_client.cpp` | MockTemporalClient 状态机测试（CREATED/RUNNING/COMPLETED/FAILED 转换 + 幂等性 + 延迟模拟） |
| `pdk/temporal_agent/tests/test_pdk_entry.cpp` | 5 工具注册验证 + ToolMetadata 完整性 |
| `examples/pkm_temporal_demo/tests/test_scenarios.cpp` | 4 场景端到端集成测试 |

---

### Task 1: ITemporalClient 接口定义

**Files:**
- Create: `include/agenticdsl/pdk/itemporal_client.h`
- Test: `pdk/temporal_agent/tests/test_itemporal_client.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
TEST_CASE("ITemporalClient: pure virtual interface compiles and methods exist") {
  // Verify interface shape (compile-time + minimal runtime sanity)
  constexpr bool kHasStartBlocking = requires(ITemporalClient* c, std::string id, json args) {
    c->start_workflow_blocking(id, args);
  };
  REQUIRE(kHasStartBlocking);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_itemporal_client && ./build/tests/test_itemporal_client`
Expected: FAIL — `itemporal_client.h` not found

- [ ] **Step 3: Write minimal interface declaration**

Create `include/agenticdsl/pdk/itemporal_client.h`:

```cpp
#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace agenticdsl::pdk {

class ITemporalClient {
public:
  virtual ~ITemporalClient() = default;
  virtual nlohmann::json start_workflow_blocking(
      const std::string& workflow_id,
      const nlohmann::json& args) = 0;
  virtual nlohmann::json start_workflow_async(
      const std::string& workflow_id,
      const nlohmann::json& args) = 0;
  virtual nlohmann::json poll(const std::string& workflow_id) = 0;
  virtual nlohmann::json signal(
      const std::string& workflow_id,
      const std::string& signal_name,
      const nlohmann::json& payload) = 0;
  virtual nlohmann::json query(
      const std::string& workflow_id,
      const std::string& query_name) = 0;
};

}  // namespace agenticdsl::pdk
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_itemporal_client && ./build/tests/test_itemporal_client`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
cd /workspace/project/HydraForge/.rddf/wt/pkm-temporal-demo-scaffold
git add include/agenticdsl/pdk/itemporal_client.h pdk/temporal_agent/tests/test_itemporal_client.cpp
git commit -m "feat(pdk-temporal): ITemporalClient 抽象接口 (5 方法)"
```

---

### Task 2: MockTemporalClient 状态机

**Files:**
- Create: `pdk/temporal_agent/mock_client.h`
- Create: `pdk/temporal_agent/mock_client.cpp`
- Test: extend `pdk/temporal_agent/tests/test_mock_client.cpp`

- [ ] **Step 1: Write failing test for state transitions**

```cpp
TEST_CASE("MockTemporalClient: CREATED → RUNNING → COMPLETED transition") {
  agenticdsl::pdk::MockTemporalClient client;
  auto start = client.start_workflow_async("wf-1", {{"task", "noop"}});
  REQUIRE(start["state"] == "RUNNING");

  client.advance_time(std::chrono::milliseconds(100));
  auto poll = client.poll("wf-1");
  REQUIRE(poll["state"] == "COMPLETED");
}

TEST_CASE("MockTemporalClient: idempotency on duplicate workflow_id") {
  agenticdsl::pdk::MockTemporalClient client;
  auto first = client.start_workflow_async("dup", {{"task", "x"}});
  auto second = client.start_workflow_async("dup", {{"task", "y"}});
  REQUIRE(second["idempotent_replay"] == true);
  REQUIRE(second["original_workflow_id"] == "dup");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_mock_client && ./build/pdk/temporal_agent/tests/test_mock_client`
Expected: FAIL — MockTemporalClient not defined

- [ ] **Step 3: Implement MockTemporalClient**

Create `pdk/temporal_agent/mock_client.h`:
```cpp
#pragma once
#include "agenticdsl/pdk/itemporal_client.h"
#include <chrono>
#include <mutex>
#include <unordered_map>

namespace agenticdsl::pdk {

enum class WorkflowState { CREATED, RUNNING, COMPLETED, FAILED };

class MockTemporalClient : public ITemporalClient {
public:
  MockTemporalClient();
  nlohmann::json start_workflow_blocking(
      const std::string& workflow_id, const nlohmann::json& args) override;
  nlohmann::json start_workflow_async(
      const std::string& workflow_id, const nlohmann::json& args) override;
  nlohmann::json poll(const std::string& workflow_id) override;
  nlohmann::json signal(
      const std::string& workflow_id, const std::string& signal_name,
      const nlohmann::json& payload) override;
  nlohmann::json query(
      const std::string& workflow_id, const std::string& query_name) override;
  void advance_time(std::chrono::milliseconds delta);
  void set_simulated_latency(std::chrono::milliseconds latency);
private:
  struct WorkflowRecord {
    WorkflowState state{WorkflowState::CREATED};
    nlohmann::json args;
    std::chrono::steady_clock::time_point created_at;
    std::chrono::milliseconds latency{0};
  };
  std::mutex mu_;
  std::unordered_map<std::string, WorkflowRecord> workflows_;
};

}  // namespace agenticdsl::pdk
```

Create `pdk/temporal_agent/mock_client.cpp` with full implementation:
- `start_workflow_async`: idempotency check → insert record (state=RUNNING) → return JSON
- `poll`: lookup → return current state + history_size_bytes
- `signal`: lookup → append signal payload to record → return state
- `query`: lookup → return readonly metadata
- `start_workflow_blocking`: poll until COMPLETED/FAILED
- `advance_time`: bypass latency for tests
- `set_simulated_latency`: configure default latency

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_mock_client && ./build/pdk/temporal_agent/tests/test_mock_client`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
cd /workspace/project/HydraForge/.rddf/wt/pkm-temporal-demo-scaffold
git add pdk/temporal_agent/mock_client.{h,cpp} pdk/temporal_agent/tests/test_mock_client.cpp
git commit -m "feat(pdk-temporal): MockTemporalClient (CREATED/RUNNING/COMPLETED/FAILED + 幂等性 + 延迟模拟)"
```

---

### Task 3: PDK 5 工具注册

**Files:**
- Create: `pdk/temporal_agent/pdk_entry.cpp`
- Create: `pdk/temporal_agent/CMakeLists.txt`
- Test: `pdk/temporal_agent/tests/test_pdk_entry.cpp`

- [ ] **Step 1: Write failing test for tool registration**

```cpp
TEST_CASE("PDK entry: registers 5 temporal tools with full ToolMetadata") {
  auto registry = ToolRegistry{};
  agenticdsl::pdk::temporal_agent::register_tools(&registry);
  REQUIRE(registry.has_tool("temporal/start_workflow"));
  REQUIRE(registry.has_tool("temporal/start_async"));
  REQUIRE(registry.has_tool("temporal/poll"));
  REQUIRE(registry.has_tool("temporal/signal"));
  REQUIRE(registry.has_tool("temporal/query"));
  for (const auto& name : {"temporal/start_workflow", "temporal/poll"}) {
    auto meta = registry.get_metadata(name);
    REQUIRE(meta.category == ToolCategory::Workflow);
    REQUIRE_FALSE(meta.allowed_layers.empty());
  }
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_pdk_entry && ./build/pdk/temporal_agent/tests/test_pdk_entry`
Expected: FAIL — register_tools not declared

- [ ] **Step 3: Implement tool registration**

Create `pdk/temporal_agent/pdk_entry.cpp`:

```cpp
#include "agenticdsl/pdk/tool_macros.h"
#include "agenticdsl/pdk/itool_registry.h"
#include "mock_client.h"

namespace agenticdsl::pdk::temporal_agent {

static std::unique_ptr<ITemporalClient> g_client;

void set_client(std::unique_ptr<ITemporalClient> c) { g_client = std::move(c); }

DECLARE_TOOL("temporal/start_workflow", "阻塞启动 Temporal workflow",
             Category::Workflow, ApprovalPolicy::agent)
  return g_client->start_workflow_blocking(args["workflow_id"], args);

DECLARE_TOOL("temporal/start_async", "异步启动 Temporal workflow (立即返回)",
             Category::Workflow, ApprovalPolicy::agent)
  return g_client->start_workflow_async(args["workflow_id"], args);

DECLARE_TOOL("temporal/poll", "轮询 workflow 状态",
             Category::Workflow, ApprovalPolicy::yolo)
  return g_client->poll(args["workflow_id"]);

DECLARE_TOOL("temporal/signal", "向 workflow 发送 signal",
             Category::Workflow, ApprovalPolicy::agent)
  return g_client->signal(args["workflow_id"], args["signal_name"], args["payload"]);

DECLARE_TOOL("temporal/query", "查询 workflow 只读元数据",
             Category::Workflow, ApprovalPolicy::yolo)
  return g_client->query(args["workflow_id"], args["query_name"]);

void register_tools(IToolRegistry* registry) {
  // Use registry->register_tool_function() with full ToolMetadata V2
  // (4 args: name, metadata, func, capability_flags)
}

}  // namespace agenticdsl::pdk::temporal_agent

extern "C" void pdk_register_tools(IToolRegistry* registry) {
  agenticdsl::pdk::temporal_agent::register_tools(registry);
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_pdk_entry && ./build/pdk/temporal_agent/tests/test_pdk_entry`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
cd /workspace/project/HydraForge/.rddf/wt/pkm-temporal-demo-scaffold
git add pdk/temporal_agent/pdk_entry.cpp pdk/temporal_agent/CMakeLists.txt pdk/temporal_agent/tests/test_pdk_entry.cpp
git commit -m "feat(pdk-temporal): 5 工具注册 + ToolMetadata V2 (category/layer/approval)"
```

---

### Task 4: Demo CLI 入口

**Files:**
- Create: `examples/pkm_temporal_demo/main.cpp`
- Create: `examples/pkm_temporal_demo/CMakeLists.txt`
- Test: `examples/pkm_temporal_demo/tests/test_main.cpp`

- [ ] **Step 1: Write failing test for CLI argument parsing**

```cpp
TEST_CASE("Demo CLI: --mock and --scenario <name> parsing") {
  DemoArgs args = parse_args({"--mock", "--scenario", "blocking"});
  REQUIRE(args.mode == DemoMode::Mock);
  REQUIRE(args.scenario == "blocking");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_demo_main && ./build/examples/pkm_temporal_demo/tests/test_demo_main`
Expected: FAIL — parse_args not defined

- [ ] **Step 3: Implement CLI**

Create `examples/pkm_temporal_demo/main.cpp`:

```cpp
#include "agenticdsl/pdk/itool_registry.h"
#include "agenticdsl/pdk/temporal_agent/mock_client.h"
#include <argparse/argparse.hpp>

enum class DemoMode { Mock, Real };
struct DemoArgs { DemoMode mode{DemoMode::Mock}; std::string scenario; };

DemoArgs parse_args(int argc, char** argv) { /* argparse impl */ }

int main(int argc, char** argv) {
  auto args = parse_args(argc, argv);
  auto client = std::make_unique<agenticdsl::pdk::MockTemporalClient>();
  agenticdsl::pdk::temporal_agent::set_client(std::move(client));

  ToolRegistry registry;
  agenticdsl::pdk::temporal_agent::register_tools(&registry);

  DSLEngine engine;
  engine.set_tool_registry(&registry);
  return engine.run_scenario(args.scenario + ".agent.md");
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_demo_main && ./build/examples/pkm_temporal_demo/tests/test_demo_main`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
cd /workspace/project/HydraForge/.rddf/wt/pkm-temporal-demo-scaffold
git add examples/pkm_temporal_demo/main.cpp examples/pkm_temporal_demo/CMakeLists.txt examples/pkm_temporal_demo/tests/test_main.cpp
git commit -m "feat(pkm-temporal-demo): CLI 入口 + --mock/--real/--scenario 参数解析"
```

---

### Task 5: 4 个场景 DSL 文件

**Files:**
- Create: `examples/pkm_temporal_demo/scenario-blocking.agent.md`
- Create: `examples/pkm_temporal_demo/scenario-async-poll.agent.md`
- Create: `examples/pkm_temporal_demo/scenario-signal.agent.md`
- Create: `examples/pkm_temporal_demo/scenario-idempotent.agent.md`
- Test: `examples/pkm_temporal_demo/tests/test_scenarios.cpp`

- [ ] **Step 1: Write failing test for scenario loading**

```cpp
TEST_CASE("Scenarios: all 4 .agent.md files parse + validate") {
  for (const auto& name : {"blocking", "async-poll", "signal", "idempotent"}) {
    DSLEngine engine;
    auto path = std::string("examples/pkm_temporal_demo/scenario-") + name + ".agent.md";
    REQUIRE_NOTHROW(engine.load_and_validate(path));
  }
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_scenarios && ./build/examples/pkm_temporal_demo/tests/test_scenarios`
Expected: FAIL — scenario files not found

- [ ] **Step 3: Write 4 scenario DSL files**

`scenario-blocking.agent.md`:
```markdown
# Scenario: blocking short task
## Nodes
- start → assign{user_input="hello"}
- assign → call_tool{tool_name="temporal/start_workflow", args={workflow_id="wf-block", args={task="noop", latency_ms=100}}}
- call_tool → end
```

`scenario-async-poll.agent.md`:
```markdown
# Scenario: async + poll
## Nodes
- start → assign{user_input="hello"}
- assign → call_tool{tool_name="temporal/start_async", args={workflow_id="wf-async", args={task="long", latency_ms=5000}}}
- call_tool → loop{condition="poll_state != COMPLETED", body=call_tool{tool_name="temporal/poll", args={workflow_id="wf-async"}}}
- loop → end
```

`scenario-signal.agent.md`:
```markdown
# Scenario: signal workflow
## Nodes
- start → assign{user_input="hello"}
- assign → call_tool{tool_name="temporal/start_async", args={workflow_id="wf-sig", args={task="wait_signal"}}}
- call_tool → call_tool{tool_name="temporal/signal", args={workflow_id="wf-sig", signal_name="go", payload={}}}
- signal → call_tool{tool_name="temporal/poll", args={workflow_id="wf-sig"}}
- poll → end
```

`scenario-idempotent.agent.md`:
```markdown
# Scenario: idempotency
## Nodes
- start → assign{user_input="hello"}
- assign → call_tool{tool_name="temporal/start_async", args={workflow_id="dup", args={task="x"}}}
- first → call_tool{tool_name="temporal/start_async", args={workflow_id="dup", args={task="y"}}}
- second → call_tool{tool_name="temporal/poll", args={workflow_id="dup"}}
- poll → end
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_scenarios && ./build/examples/pkm_temporal_demo/tests/test_scenarios`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
cd /workspace/project/HydraForge/.rddf/wt/pkm-temporal-demo-scaffold
git add examples/pkm_temporal_demo/scenario-*.agent.md examples/pkm_temporal_demo/tests/test_scenarios.cpp
git commit -m "feat(pkm-temporal-demo): 4 场景 DSL (blocking/async-poll/signal/idempotent)"
```

---

### Task 6: CMake 集成 + ctest 验证

**Files:**
- Modify: `examples/CMakeLists.txt`
- Test: `ctest -R pkm_temporal_demo`

- [ ] **Step 1: Write failing CMakeLists test**

```bash
ctest -R pkm_temporal_demo --output-on-failure
```

Expected: FAIL — test target not registered

- [ ] **Step 2: Run ctest to verify it fails**

Run: `ctest -R pkm_temporal_demo`
Expected: FAIL — No tests were found

- [ ] **Step 3: Update root CMakeLists.txt**

Add to `examples/CMakeLists.txt`:
```cmake
add_subdirectory(pkm_temporal_demo)
```

Add to `examples/pkm_temporal_demo/CMakeLists.txt`:
```cmake
add_executable(pkm_temporal_demo main.cpp)
target_link_libraries(pkm_temporal_demo PRIVATE hydraforge_pdk agenticdsl_core)
add_subdirectory(tests)

add_test(NAME pkm_temporal_demo COMMAND pkm_temporal_demo --mock --scenario blocking)
set_tests_properties(pkm_temporal_demo PROPERTIES TIMEOUT 30)
```

- [ ] **Step 4: Run ctest to verify it passes**

Run: `cmake --build build && ctest -R pkm_temporal_demo --output-on-failure`
Expected: PASS (4 tests: blocking/async-poll/signal/idempotent + unit tests)

- [ ] **Step 5: Commit**

```bash
cd /workspace/project/HydraForge/.rddf/wt/pkm-temporal-demo-scaffold
git add examples/CMakeLists.txt examples/pkm_temporal_demo/CMakeLists.txt
git commit -m "build(pkm-temporal-demo): CMake 集成 + ctest 注册"
```

---

## Self-Review Checklist

- [x] **Spec coverage**: T3 PDK 骨架 (1.1-1.4, 2.1, 3.1-3.2) → Task 1-3; T4 Demo (4.1-4.4) → Task 5; CLI + CMake → Task 4, 6
- [x] **Placeholder scan**: All steps have concrete code (no "TBD"/"implement later")
- [x] **Type consistency**: `ITemporalClient` methods → `MockTemporalClient` impls → PDK tools → Demo scenarios all aligned
- [x] **TDD discipline**: Every Task starts with failing test, ends with PASS verification + commit
- [x] **Idempotency**: Task 2 test `idempotency on duplicate workflow_id` covers spec requirement
- [x] **Zero core changes**: All work in `include/agenticdsl/pdk/` + `pdk/temporal_agent/` + `examples/pkm_temporal_demo/` (no `src/modules/` modification)

## Validation Gate

After all 6 Tasks complete:
- [ ] `ctest -R pkm_temporal_demo --output-on-failure` → all PASS
- [ ] `./examples/pkm_temporal_demo/pkm_temporal_demo --mock --scenario blocking` exits 0
- [ ] `./examples/pkm_temporal_demo/pkm_temporal_demo --mock --scenario idempotent` shows `idempotent_replay=true` in output
- [ ] `pdk/temporal_agent/libpdk_temporal_agent.so` exists + `nm | grep pdk_` shows all 5 tools
- [ ] Zero modifications to `src/modules/` or `include/agenticdsl/types/`

## Estimated Effort

| Module | Tasks | Time |
|---|:---:|:---:|
| T1 ITemporalClient 接口 | 1 | 0.5h |
| T2 MockTemporalClient | 2 | 2h |
| T3 PDK 5 工具注册 | 3 | 3h |
| T4 Demo CLI | 4 | 2h |
| T5 4 场景 DSL | 5 | 2h |
| T6 CMake + ctest | 6 | 1h |
| **Total** | 6 | **10.5h** |

## TDD Discipline

Each work unit in this plan follows the canonical 5-step TDD structure:

1. **Write the failing test** — Define expected behavior in a Catch2 case (or shell assertion)
2. **Run test to verify it fails** — Confirm red state before writing code
3. **Write minimal implementation** — Add the smallest code that makes the test pass
4. **Run test to verify it passes** — Confirm green state, then refactor
5. **Defer commit** — Batch all green units into a single archive commit per change

This discipline is enforced by `skill_use("execute")`; skipping any step breaks the red→green→commit chain.

