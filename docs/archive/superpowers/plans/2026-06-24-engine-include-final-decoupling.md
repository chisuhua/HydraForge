# Engine Include Final Decoupling Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the `tech-debt-and-phase1-closure` handoff by verifying 12 existing scheduler/parser tests, adding 3 engine-construction tests, decoupling `src/core/engine.cpp` cross-module includes from 10 to ≤3, revalidating sanitizers, and archiving the change chain.

**Architecture:** Replace direct concrete-class construction in `engine.cpp` with small internal factory functions (`common/tools/factory`, forward-declared `llm/budget` factories). Keep `DSLEngine` public API unchanged. Rely on `engine.h` already exposing abstract contract interfaces (`IToolRegistry`, `IProviderFactory`) and `topo_scheduler.h` transitively providing the complete `BudgetController` type for `~DSLEngine()`.

**Tech Stack:** C++20, CMake 3.20+, Catch2, llama.cpp (no new third-party deps).

---

## File Map

| File | Responsibility | Change |
|------|----------------|--------|
| `src/core/engine.cpp` | Main decoupling target | Remove 7 includes, replace 2 direct constructions with factories, delete `load_llm_config()` |
| `src/common/tools/factory.h` | New factory header | Declare `agenticdsl::tools::create_tool_registry()` returning `unique_ptr<IToolRegistry>` |
| `src/common/tools/factory.cpp` | New factory implementation | Construct `ToolRegistry` |
| `CMakeLists.txt` | Root build config | Add `src/common/tools/factory.cpp` to `agenticdsl_common` |
| `src/common/llm/llm_provider_factory.cpp` | LLM factory router | Add final mock fallback so `engine.cpp` never directly constructs `MockLLMProvider` |
| `tests/test_engine_factory.cpp` | New test binary | 3 Catch2 TEST_CASE covering default/custom/dependency-injection construction |
| `openspec/changes/tech-debt-cleanup-sprint-6/tasks.md` | Prior change ledger | Mark 6.3.4/6.3.5/6.3.6 as closed via this change |
| `openspec/changes/tech-debt-and-phase1-closure/tasks.md` | Prior change ledger | Mark handoff tasks as shipped to this change |
| `AGENTS.md` | Project knowledge base | Append this change to Recent Changes |

---

## Task 1: Verify Scheduler 7 DagState Test Cases

**Files:**
- Inspect: `tests/test_scheduler.cpp:241-490`
- No code changes expected if cases already exist and pass.

**Context:** Sprint 7 already added 7 state-based cases for the `TopoScheduler` sub-function contract. This task only verifies them.

- [ ] **Step 1: Build the test binary**

```bash
cmake --preset debug
cmake --build build/debug --target test_scheduler
```

Expected: `test_scheduler` compiles with no errors.

- [ ] **Step 2: Run the scheduler binary and count cases**

```bash
./build/debug/tests/test_scheduler --reporter compact
```

Expected output contains at least 13 TEST_CASE entries and all PASS, including:

- `prepare_dag_state_simple_linear` `[scheduler][stage2][dag_state][linear]`
- `prepare_dag_state_diamond` `[scheduler][stage2][dag_state][diamond]`
- `prepare_dag_state_cycle_detection` `[scheduler][stage2][dag_state][cycle]`
- `dispatch_ready_nodes_initial` `[scheduler][stage2][dispatch][initial]`
- `dispatch_ready_nodes_parallel` `[scheduler][stage2][dispatch][parallel]`
- `handle_node_completion_success` `[scheduler][stage2][completion][downstream]`
- `handle_node_completion_failure` `[scheduler][stage2][completion][failure]`

- [ ] **Step 3: Run the CTest entry**

```bash
ctest -R test_scheduler --output-on-failure
```

Expected: `1/1 Test Passed`.

- [ ] **Step 4: Commit STATUS NOTE (only if any fix was required)**

If all 7 cases already pass, no commit is required. If any case is missing or failing, add/fix it first, then:

```bash
git add tests/test_scheduler.cpp
git commit -m "test(scheduler): verify 7 DagState subfunction test cases (6.3.4 part 1)"
```

---

## Task 2: Verify Parser 5 NodeFactoryRegistry Test Cases

**Files:**
- Inspect: `tests/test_parser.cpp:382-455`
- No code changes expected if cases already exist and pass.

**Context:** Sprint 6/7 already added 5 cases covering `NodeFactoryRegistry`. This task verifies them and runs TSan on the concurrent case.

- [ ] **Step 1: Build the parser test binary**

```bash
cmake --build build/debug --target test_parser
```

Expected: `test_parser` compiles with no errors.

- [ ] **Step 2: Run the parser binary and count cases**

```bash
./build/debug/tests/test_parser --reporter compact
```

Expected output contains at least 17 TEST_CASE entries and all PASS, including:

- `factory_registry_registers_all_types`
- `factory_registry_creates_correct_subtype`
- `factory_registry_unknown_type_returns_nullptr`
- `factory_registry_global_singleton`
- `factory_registry_concurrent_access` `[parser][day4][tsan]`

- [ ] **Step 3: Run CTest entry**

```bash
ctest -R test_parser --output-on-failure
```

Expected: `1/1 Test Passed`.

- [ ] **Step 4: Run TSan on the parser test**

```bash
cmake --preset tsan
cmake --build build/tsan --target test_parser
ctest -R test_parser --output-on-failure
```

Expected: `1/1 Test Passed`, output contains no `WARNING: ThreadSanitizer: data race`.

- [ ] **Step 5: Commit STATUS NOTE (only if any fix was required)**

If all 5 cases pass under both normal and TSan builds, no commit is required. Otherwise, fix and:

```bash
git add tests/test_parser.cpp
git commit -m "test(parser): verify 5 NodeFactoryRegistry test cases (6.3.4 part 2)"
```

---

## Task 3: Add `tests/test_engine_factory.cpp`

**Files:**
- Create: `tests/test_engine_factory.cpp`
- Verify: `tests/CMakeLists.txt:56` (`file(GLOB SINGLE_TEST_SOURCES "test_*.cpp")`) auto-discovers the new file.

**Context:** Covers the current `DSLEngine` construction path after the Sprint 6 factory removal. Uses only existing public API (`from_markdown`, `from_file`, `set_llm_provider`, `set_interaction_bus`, `get_llm_provider`, `get_tool_registry`, `get_budget_controller`).

- [ ] **Step 1: Create the test file**

Create `tests/test_engine_factory.cpp`:

```cpp
// tests/test_engine_factory.cpp
// 验证 DSLEngine 默认/自定义/依赖注入构造路径
// 覆盖 P2.A 删除 engine.cpp 工厂后的直接构造路径

#include "catch_amalgamated.hpp"
#include "core/engine.h"
#include "common/llm/mock_provider.h"
#include "agenticdsl/contract/inmemory_bus.h"

using namespace agenticdsl;

static const char* kMinimalMain = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
entry: start
nodes:
  - id: start
    type: assign
    assign:
      ok: true
    next: /main/end
  - id: end
    type: end
    termination_mode: hard
# --- END AgenticDSL ---
```
)";

TEST_CASE("test_engine_create_with_default_config", "[engine][factory][default]") {
    auto engine = DSLEngine::from_markdown(kMinimalMain);
    REQUIRE(engine != nullptr);

    auto result = engine->run();
    REQUIRE(result.success);
    REQUIRE(result.final_context.contains("ok"));
    // AssignNode 通过 inja 将 bool 强制序列化为字符串 "true"/"false" (node_factory.cpp:104)
    REQUIRE(result.final_context["ok"] == "true");
}

TEST_CASE("test_engine_create_with_custom_config", "[engine][factory][custom]") {
    auto engine = DSLEngine::from_markdown(kMinimalMain);
    REQUIRE(engine != nullptr);

    // 自定义配置：注入一个固定响应的 MockLLMProvider
    auto mock = std::make_unique<MockLLMProvider>();
    mock->set_fixed_response("hello");
    engine->set_llm_provider(std::move(mock));

    REQUIRE(engine->get_llm_provider() != nullptr);
}

TEST_CASE("test_engine_create_with_dependencies", "[engine][factory][di]") {
    auto engine = DSLEngine::from_markdown(kMinimalMain);
    REQUIRE(engine != nullptr);

    // 依赖注入 1：自定义 LLM provider
    auto mock = std::make_unique<MockLLMProvider>();
    mock->set_fixed_response("mocked");
    engine->set_llm_provider(std::move(mock));
    REQUIRE(engine->get_llm_provider() != nullptr);

    // 依赖注入 2：交互总线
    auto bus = std::make_shared<InMemoryBus>();
    engine->set_interaction_bus(bus);
    REQUIRE(engine->get_interaction_bus() == bus);

    // 依赖注入 3：通过 get_tool_registry() 注册工具（IToolRegistry 抽象接口）
    engine->get_tool_registry().register_tool_function(
        "echo",
        [](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
            auto it = args.find("value");
            return nlohmann::json{{"value", it != args.end() ? it->second : "empty"}};
        });
    REQUIRE(engine->get_tool_registry().has_tool("echo"));

    // 预算控制器可用（由 BudgetController 工厂创建）
    REQUIRE(engine->get_session_cost() == 0.0);
}
```

- [ ] **Step 2: Verify GLOB registration**

Open `tests/CMakeLists.txt` and confirm line 56 is:

```cmake
file(GLOB SINGLE_TEST_SOURCES "test_*.cpp")
```

If it is commented out and a manual list is active instead, add `test_engine_factory.cpp` to that manual list.

- [ ] **Step 3: Build and run the new test**

```bash
cmake --build build/debug --target test_engine_factory
./build/debug/tests/test_engine_factory --reporter compact
ctest -R test_engine_factory --output-on-failure
```

Expected:
- Build succeeds.
- Binary reports exactly 3 TEST_CASE PASS.
- CTest reports `1/1 Test Passed`.

- [ ] **Step 4: Run full baseline before proceeding to P2.C**

```bash
ctest --output-on-failure
```

Expected: `34/34 Test Passed` (P2.B 完成时新增 `test_engine_factory` 后净计数为 34;后续 P2.C 审查修复移除 `test_path_resolution`,最终保持 34)。

- [ ] **Step 5: Commit**

```bash
git add tests/test_engine_factory.cpp
git commit -m "test(engine): add 3 test cases for engine construction post-factory-removal (6.3.4 part 3)"
```

---

## Task 4: P2.B Ship Gate

- [ ] **Step 1: Confirm `test_scheduler`, `test_parser`, `test_engine_factory` combined case count**

```bash
./build/debug/tests/test_scheduler --reporter compact | tail -5
./build/debug/tests/test_parser --reporter compact | tail -5
./build/debug/tests/test_engine_factory --reporter compact | tail -5
```

Expected: combined PASS count ≥ 13 + 17 + 3 = 33.

- [ ] **Step 2: Confirm ctest baseline**

```bash
ctest --output-on-failure
```

Expected: `34/34 Test Passed`.

- [ ] **Step 3: Block P2.C until gate is green**

Do not start Task 5 until Task 4 Step 2 shows `34/34`. This is a TDD hard constraint from the parent plan.

---

## Task 5: Commit A — Introduce ToolRegistry Factory

**Files:**
- Create: `src/common/tools/factory.h`
- Create: `src/common/tools/factory.cpp`
- Modify: `CMakeLists.txt:68-84` (add `src/common/tools/factory.cpp` to `agenticdsl_common`)
- Modify: `src/core/engine.cpp:9` (replace `common/tools/registry.h` include with factory forward declaration)

**Context:** `engine.cpp` currently constructs `ToolRegistry` directly. Move that construction to a factory so the concrete header can be dropped from `engine.cpp`.

- [ ] **Step 1: Create `src/common/tools/factory.h`**

```cpp
// src/common/tools/factory.h
// 功能描述：ToolRegistry 工厂函数
//          为 engine.cpp 等调用方提供不依赖 registry.h 完整类型的构造入口
// 设计依据：ADR-0019 §1.4 (P1.T4 tool_registry_ PIMPL-lite 解耦的延伸)
// 作者：2026-06-24-engine-include-final-decoupling
// 最后修改日期：2026-06-24

#pragma once

#include <memory>

namespace agenticdsl {

class IToolRegistry;

namespace tools {

/**
 * @brief 创建默认 ToolRegistry 实现
 * @return unique_ptr<IToolRegistry> 指向 ToolRegistry 实例
 *
 * 工厂返回抽象接口，调用方无需 include common/tools/registry.h 完整类型。
 */
std::unique_ptr<IToolRegistry> create_tool_registry();

} // namespace tools
} // namespace agenticdsl
```

- [ ] **Step 2: Create `src/common/tools/factory.cpp`**

```cpp
// src/common/tools/factory.cpp
#include "common/tools/factory.h"
#include "common/tools/registry.h" // 完整类型仅工厂实现 TU 可见

namespace agenticdsl::tools {

std::unique_ptr<IToolRegistry> create_tool_registry() {
    return std::make_unique<ToolRegistry>();
}

} // namespace agenticdsl::tools
```

- [ ] **Step 3: Register `factory.cpp` in `agenticdsl_common`**

Edit `CMakeLists.txt` and add `src/common/tools/factory.cpp` to the `agenticdsl_common` source list:

```cmake
add_library(agenticdsl_common STATIC
    src/common/llm/cloud_adapter.cpp
    src/common/llm/http_adapter.cpp
    src/common/llm/llama_adapter.cpp
    src/common/llm/llama_adapter_provider.cpp
    src/common/llm/llama_tool.cpp
    src/common/llm/llm_provider_factory.cpp      # P1.T1: LLMProviderFactory 路由
    src/common/llm/mock_provider.cpp
    src/common/llm/mock_provider_factory.cpp    # P1.T1: MockProviderFactory
    src/common/llm/factory.cpp                  # Sprint 6 P2-7
    src/common/llm/sse_stream.cpp
    src/common/tools/registry.cpp
    src/common/tools/factory.cpp                # engine-include-final-decoupling: ToolRegistry 工厂
    src/common/utils/parser_utils.cpp
    src/common/utils/template_renderer.cpp
    src/common/utils/yaml_json.cpp
    src/core/types/tool_result.cpp
)
```

- [ ] **Step 4: Update `src/core/engine.cpp`**

1. Remove `#include "common/tools/registry.h"`.
2. Add a forward declaration near the top of the anonymous/file namespace:

```cpp
namespace agenticdsl::tools {
std::unique_ptr<IToolRegistry> create_tool_registry();
} // namespace agenticdsl::tools
```

3. Replace the constructor initializer:

```cpp
// Before:
tool_registry_(std::make_unique<ToolRegistry>()),

// After:
tool_registry_(agenticdsl::tools::create_tool_registry()),
```

- [ ] **Step 5: Verify include count dropped to 9**

```bash
grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp
```

Expected output: `9`.

- [ ] **Step 6: Build and run tests**

```bash
cmake --build build/debug
ctest --output-on-failure
```

Expected: build succeeds, `34/34 Test Passed`.

- [ ] **Step 7: Commit**

```bash
git add src/common/tools/factory.h src/common/tools/factory.cpp CMakeLists.txt src/core/engine.cpp
git commit -m "refactor(core): introduce ToolRegistry factory, remove tools/registry.h from engine.cpp (6.3.5 batch 1)"
```

---

## Task 6: Commit B — Remove LLM Concrete Includes

**Files:**
- Modify: `src/core/engine.cpp`
- Modify: `src/common/llm/llm_provider_factory.cpp`

**Context:** `engine.cpp` currently includes `llama_adapter.h`, `llama_adapter_provider.h`, `llm_config.h`, `mock_provider.h`, and `common/llm/factory.h`. It also defines `load_llm_config()` and directly constructs `MockLLMProvider` as a fallback. Move the mock fallback into `LLMProviderFactory::create()` and delete the dead code.

- [ ] **Step 1: Move mock fallback into `LLMProviderFactory::create()`**

Edit `src/common/llm/llm_provider_factory.cpp`. Change the final `return nullptr;` to return a mock provider:

```cpp
std::unique_ptr<ILLMProvider> LLMProviderFactory::create(const LLMConfig& config) {
  const std::string& backend = config.provider;

  if (backend == "mock" || backend.empty()) {
    return mock_factory->create(config);
  }

  if (backend == "openai" || backend == "anthropic" ||
      backend == "deepseek" || backend == "qwen" ||
      backend == "moonshot" || backend == "custom") {
    return cloud_factory->create(config);
  }

  if (backend == "local" || backend == "llama") {
    return llama_factory->create(config);
  }

  // Fallback: 未识别的 provider 也返回 MockLLMProvider，保证 engine.cpp 无需直接构造具体类
  return mock_factory->create(config);
}
```

- [ ] **Step 2: Remove LLM includes and `load_llm_config()` from `engine.cpp`**

1. Delete these lines from `src/core/engine.cpp`:

```cpp
#include "common/llm/llama_adapter.h"
#include "common/llm/llama_adapter_provider.h"
#include "common/llm/llm_config.h"
#include "common/llm/mock_provider.h"
#include "common/llm/factory.h"
```

2. Add a forward declaration for the LLM factory function:

```cpp
namespace agenticdsl::llm {
std::unique_ptr<IProviderFactory> create_provider_factory();
} // namespace agenticdsl::llm
```

3. Delete the entire `load_llm_config()` function (lines 24-70).

4. Delete the `(void)load_llm_config();` call in `DSLEngine::from_markdown`.

5. Simplify the LLM provider construction in `DSLEngine::DSLEngine`:

```cpp
// Before (lines 114-128):
LLMConfig default_config;
// ... comments ...
if (provider_factory_) {
    LLMConfig mock_config;
    mock_config.provider = "mock";
    llm_provider_ = provider_factory_->create(mock_config);
}
if (!llm_provider_) {
    llm_provider_ = std::make_unique<MockLLMProvider>();
}

// After:
if (provider_factory_) {
    LLMConfig mock_config;
    mock_config.provider = "mock";
    llm_provider_ = provider_factory_->create(mock_config);
}
```

`LLMConfig` remains usable because `engine.h` → `common/llm/llm_types.h` → `llm_config.h` transitively defines it.

- [ ] **Step 3: Verify include count dropped to 4**

```bash
grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp
```

Expected output: `4` (10 baseline - 1 removed in Commit A - 5 removed in Commit B).

- [ ] **Step 4: Build and run tests**

```bash
cmake --build build/debug
ctest --output-on-failure
```

Expected: build succeeds, `34/34 Test Passed`.

- [ ] **Step 5: Commit**

```bash
git add src/common/llm/llm_provider_factory.cpp src/core/engine.cpp
git commit -m "refactor(core): factory-inject LLM provider, remove llama/mock/factory includes from engine.cpp (6.3.5 batch 2)"
```

---

## Task 7: Commit C — Forward-Declare Budget Factory

**Files:**
- Modify: `src/core/engine.cpp`

**Context:** `engine.cpp` includes `modules/budget/factory.h` only for `agenticdsl::budget::create_controller()`. Replace the include with a forward declaration. `BudgetController` complete type remains available transitively via `modules/scheduler/topo_scheduler.h` → `scheduler/execution_session.h` → `modules/budget/budget_controller.h`, so `~DSLEngine()` keeps compiling.

- [ ] **Step 1: Replace budget factory include with forward declaration**

In `src/core/engine.cpp`:

1. Delete:

```cpp
#include "modules/budget/factory.h"
```

2. Add near the top with the other forward declarations:

```cpp
namespace agenticdsl {
class BudgetController;
namespace budget {
std::unique_ptr<BudgetController> create_controller();
} // namespace budget
} // namespace agenticdsl
```

3. Leave `budget_controller_(agenticdsl::budget::create_controller())` unchanged.

- [ ] **Step 2: Verify include count dropped to ≤3**

```bash
grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp
```

Expected output: `3` (the remaining includes are `common/log/log.h`, `modules/scheduler/topo_scheduler.h`, `modules/system/system_nodes.h`).

- [ ] **Step 3: Build and run tests**

```bash
cmake --build build/debug
ctest --output-on-failure
```

Expected: build succeeds, `34/34 Test Passed`.

- [ ] **Step 4: Commit**

```bash
git add src/core/engine.cpp
git commit -m "refactor(core): forward-declare budget factory, remove budget/factory.h from engine.cpp (6.3.5 batch 3)"
```

---

## Task 8: P2.C Ship Gate

- [ ] **Step 1: Final include count check**

```bash
grep '#include.*\(modules/\|common/\)' src/core/engine.cpp
```

Expected: exactly 3 lines:

```cpp
#include "common/log/log.h"
#include "modules/scheduler/topo_scheduler.h"
#include "modules/system/system_nodes.h"
```

- [ ] **Step 2: Full ctest run**

```bash
ctest --output-on-failure
```

Expected: `34/34 Test Passed`.

- [ ] **Step 3: Hub-node out-degree check**

Use the code-review-graph MCP tool:

```text
mcp__code-review-graph__get_hub_nodes --top_n 5
```

Expected: `TopoScheduler::execute` out_degree < 30 and its 3 sub-functions out_degree < 25.

- [ ] **Step 4: Timebox check**

P2.C must complete within the 1.5-day timebox. If it exceeds the timebox, do **not** continue coding—execute Task 12 (handoff variant) instead.

---

## Task 9: P2.F — TSan/ASan Revalidation

- [ ] **Step 1: Ensure `34/34` baseline before sanitizer builds**

```bash
ctest --output-on-failure
```

Expected: `34/34 Test Passed`.

- [ ] **Step 2: ASan build and test**

```bash
cmake --preset asan
cmake --build build/asan
ctest --output-on-failure
```

Expected: all tests pass, output contains no `ERROR: AddressSanitizer`, no memory-leak report, no use-after-free report.

- [ ] **Step 3: TSan build and test**

```bash
cmake --preset tsan
cmake --build build/tsan
ctest --output-on-failure
```

Expected: all tests pass, output contains no `WARNING: ThreadSanitizer: data race`, no thread leak.

- [ ] **Step 4: TSan parser concurrency check**

```bash
ctest -R test_parser --output-on-failure
```

Expected: `factory_registry_concurrent_access` reports 0 data race.

- [ ] **Step 5: Handle pre-existing sanitizer findings (graceful degradation)**

If ASan/TSan reports an issue not introduced by this change:

1. Document it in `openspec/changes/2026-06-24-engine-include-final-decoupling/tasks.md` §2.5 as pre-existing.
2. Create a new OpenSpec change to track it.
3. Do **not** let it block this change's archive.

---

## Task 10: 6.3.6 — `pending_dynamic_deps_` Accessor Consistency

- [ ] **Step 1: Grep for external bare-member access**

```bash
grep "session_\.pending_dynamic_deps_" src/ --include='*.cpp' --include='*.h' -r
grep "->pending_dynamic_deps_" src/ --include='*.cpp' --include='*.h' -r
```

Expected: both commands return 0 hits.

- [ ] **Step 2: Fix any external hits**

If either grep returns hits outside `ExecutionSession`'s own implementation, change those call sites to use `session_.get_pending_dynamic_deps()` and rerun:

```bash
cmake --build build/debug
ctest --output-on-failure
```

Expected: `34/34 Test Passed`.

- [ ] **Step 3: Commit (only if fixes were made)**

```bash
git add src/...
git commit -m "refactor(scheduler): use get_pending_dynamic_deps() accessor (6.3.6 final regression)"
```

---

## Task 11: Archive Chain Closure

**Files:**
- Modify: `openspec/changes/tech-debt-cleanup-sprint-6/tasks.md`
- Modify: `openspec/changes/tech-debt-and-phase1-closure/tasks.md`
- Modify: `AGENTS.md`
- Delete (via `openspec archive`): `openspec/changes/tech-debt-cleanup-sprint-6/`, `openspec/changes/sprint-9-handle-node-completion/`, `openspec/changes/tech-debt-and-phase1-closure/`, `openspec/changes/2026-06-24-engine-include-final-decoupling/`

- [ ] **Step 1: Update `tech-debt-cleanup-sprint-6/tasks.md`**

In §6.1 table, mark 6.3.4, 6.3.5, and 6.3.6 as ✅ closed by this change. Add a reference line such as:

```markdown
- 6.3.4 ✅ closed by `2026-06-24-engine-include-final-decoupling` (P2.B)
- 6.3.5 ✅ closed by `2026-06-24-engine-include-final-decoupling` (P2.C)
- 6.3.6 ✅ closed by `2026-06-24-engine-include-final-decoupling`
```

- [ ] **Step 2: Update `tech-debt-and-phase1-closure/tasks.md`**

Find the handoff tasks that point to `2026-06-24-engine-include-final-decoupling` and mark them as ✅ shipped to this change.

- [ ] **Step 3: Validate and archive `tech-debt-cleanup-sprint-6`**

```bash
openspec validate tech-debt-cleanup-sprint-6
openspec archive tech-debt-cleanup-sprint-6 --yes
ls openspec/changes/tech-debt-cleanup-sprint-6/
```

Expected: validate exits 0, archive succeeds, `ls` reports "No such file or directory".

- [ ] **Step 4: Validate and archive `sprint-9-handle-node-completion`**

```bash
openspec validate sprint-9-handle-node-completion
openspec archive sprint-9-handle-node-completion --yes
ls openspec/changes/sprint-9-handle-node-completion/
```

Expected: validate exits 0, archive succeeds, `ls` reports "No such file or directory".

- [ ] **Step 5: Validate and archive this change**

`tech-debt-and-phase1-closure` references this change, so archive this one first.

```bash
openspec validate 2026-06-24-engine-include-final-decoupling
openspec archive 2026-06-24-engine-include-final-decoupling --yes
ls openspec/changes/2026-06-24-engine-include-final-decoupling/
```

Expected: validate exits 0, archive succeeds, `ls` reports "No such file or directory".

- [ ] **Step 6: Commit archive bookkeeping for the first three changes**

```bash
git add -A
git commit -m "chore(openspec): archive tech-debt-cleanup-sprint-6, sprint-9, and 2026-06-24-engine-include-final-decoupling"
```

- [ ] **Step 7: Update `AGENTS.md` Recent Changes**

Append a new entry at the top of the Recent Changes section:

```markdown
- 2026-06-24: `2026-06-24-engine-include-final-decoupling` shipped — engine.cpp cross-module includes 10→3, 34/34 ctest pass, 4-change archive chain closed, Sprint 10 starts with 0 active OpenSpec changes.
```

- [ ] **Step 8: Validate and archive `tech-debt-and-phase1-closure` last**

Archive this last because its tasks.md references the current change's commit hash.

```bash
openspec validate tech-debt-and-phase1-closure
openspec archive tech-debt-and-phase1-closure --yes
ls openspec/changes/tech-debt-and-phase1-closure/
```

Expected: validate exits 0, archive succeeds, `ls` reports "No such file or directory".

- [ ] **Step 9: Validate zero active changes and final commit**

```bash
openspec list
git add -A
git commit -m "chore(openspec): archive tech-debt-and-phase1-closure (Sprint 10 0 backlog)"
```

Expected: `openspec list` shows 0 active changes.

---

## Task 12: Final Ship Gate Verification

Run the full verification checklist before declaring the change complete.

- [ ] `git status` is clean.
- [ ] `ctest --output-on-failure` → `34/34 Test Passed`.
- [ ] `cmake --preset asan && ctest --output-on-failure` → 0 ASan error.
- [ ] `cmake --preset tsan && ctest --output-on-failure` → 0 TSan race.
- [ ] `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` → ≤3.
- [ ] `awk '/^ExecutionResult TopoScheduler::execute/,/^}$/' src/modules/scheduler/topo_scheduler.cpp | wc -l` → ≤60.
- [ ] `mcp__code-review-graph__get_hub_nodes --top_n 5` → `topo_scheduler::execute` out_degree < 30 and 3 sub-functions out_degree < 25.
- [ ] `python3 tools/adr_lint.py docs/adr/` exits 0.
- [ ] `python3 tools/docs_drift_audit.py` → 0 critical drift.
- [ ] `openspec list` → 0 active changes.
- [ ] `git log --oneline -30` contains all commits from this plan in order.
- [ ] 34 baseline tests remain PASS; 1 new `test_engine_factory` binary adds 3 PASS cases.

---

## Task 13: P2.C Handoff Variant (Only If 1.5-Day Timebox Exceeded)

If Task 8 Step 4 triggers, do **not** keep hacking past the timebox.

- [ ] **Step 1: Create `2026-07-xx-engine-include-final-decoupling-v2`**

Create the OpenSpec change directory with:

- `proposal.md` — Why P2.C exceeded the timebox and what remains.
- `tasks.md` — Reference this plan and continue unfinished P2.C batches.
- `specs/engine-include-decoupling/spec.md` — Capability spec for the remaining work.

- [ ] **Step 2: Validate the new change**

```bash
openspec validate 2026-07-xx-engine-include-final-decoupling-v2
```

Expected: exit 0.

- [ ] **Step 3: Update this change's `tasks.md`**

In `openspec/changes/2026-06-24-engine-include-final-decoupling/tasks.md` §3.5, add:

```markdown
- [ ] 3.5.5 ⏳ Handoff to 2026-07-xx-engine-include-final-decoupling-v2 (P2.C timebox overflow)
```

- [ ] **Step 4: Commit handoff bookkeeping**

```bash
git add openspec/changes/2026-07-xx-engine-include-final-decoupling-v2/ openspec/changes/2026-06-24-engine-include-final-decoupling/tasks.md
git commit -m "chore(openspec): handoff 6.3.5 to engine-include-final-decoupling-v2 (timebox overflow)"
```

- [ ] **Step 5: Archive this change anyway**

Proceed with Task 11 and Task 12, then archive `2026-06-24-engine-include-final-decoupling`. A timebox overflow is an explicit handoff, not ship-as-is debt.

---

## Self-Review Checklist

**1. Spec coverage:**

| Spec Requirement | Implementing Task |
|------------------|-------------------|
| `engine-include-decoupling-progress` (10→≤3 includes) | Tasks 5, 6, 7, 8 |
| `baseline-tests-coverage-extension` (verify 12 + add 3) | Tasks 1, 2, 3, 4 |
| `sanitizer-revalidation` | Task 9 |
| `pending-dynamic-deps-accessor-consistency` | Task 10 |
| `archive-chain-closure` | Task 11 |
| P2.C timebox handoff variant | Task 13 |

**2. Placeholder scan:** No `TBD`, `TODO`, "add appropriate error handling", or "write tests for the above" remain. Every code step contains concrete code.

**3. Type consistency:**

- `agenticdsl::tools::create_tool_registry()` returns `std::unique_ptr<IToolRegistry>` everywhere.
- `agenticdsl::llm::create_provider_factory()` forward declaration matches `src/common/llm/factory.h`.
- `agenticdsl::budget::create_controller()` forward declaration matches `src/modules/budget/factory.h`.
- `LLMConfig` variable name `mock_config` is consistent in Task 6.

**4. Dependency/coupling notes:**

- `engine.h` already transitively provides `LLMConfig` via `common/llm/llm_types.h` → `llm_config.h`, so Commit B does not need to keep `common/llm/llm_config.h`.
- `BudgetController` complete type remains available in `engine.cpp` via `modules/scheduler/topo_scheduler.h` → `scheduler/execution_session.h` → `modules/budget/budget_controller.h`, so Commit C does not break `~DSLEngine()`.
- `src/common/tools/factory.cpp` is added to the existing `agenticdsl_common` static library target in the root `CMakeLists.txt`; no new CMake target is needed.
