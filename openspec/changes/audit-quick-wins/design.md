# Design: Audit Quick Wins (Sprint 15)

## Architecture Decisions

### AD-1: ToolCoordinator opt-in (Sprint 14 C4 backwards-compat)

**Decision:** Change `DSLEngine::tool_coordinator_` from default-on to opt-in.

**Rationale:** Sprint 14 C4 (commit `a48e563`) injected `ToolCoordinator` as a default middleware between `NodeExecutor` and the legacy `ApprovalHandler` path. The intent was a new safety/audit layer (ADR-0031 §决策 5). However, the default-on behavior caused 2 pre-existing tests to fail because `ToolCoordinator::execute()` didn't handle tools registered via `DSLEngine::register_tool()` callback in the same way the legacy path did.

The cleanest fix is to make `ToolCoordinator` opt-in:
- Backward compatibility: existing user code (using `ApprovalHandler` or direct `call_tool()`) continues to work without any changes
- Forward compatibility: users who want the new middleware can explicitly activate it via `DSLEngine::set_tool_coordinator(unique_ptr<ToolCoordinator>)`
- ADR-0031 §决策 5 design unchanged: only the activation timing changed (default-on → explicit)

**Trade-offs considered:**
- Option A (chosen): opt-in via new API — zero breaking changes, slight API surface increase
- Option B: Fix ToolCoordinator to handle tests correctly — requires deeper refactor, risk to other tests
- Option C: Update tests to match new behavior — breaks user-facing API (users with `register_tool` would break)

**Implementation:**
```cpp
// engine.h
void set_tool_coordinator(std::unique_ptr<ToolCoordinator> coordinator);

// engine.cpp constructor (was):
tool_coordinator_ = std::make_unique<ToolCoordinator>(...);  // remove
// engine.cpp constructor (now):
tool_coordinator_ = nullptr;  // opt-in
```

### AD-2: Behavior-preserving refactors (8 items)

**Decision:** Apply 8 small-scope refactors that improve code quality without changing behavior.

**Items:**
1. **fork/join stubs** → throw `std::logic_error` (signals routing bug, not runtime error). Scheduler handles fork/join.
2. **execution_session commented code** → delete (Sprint 2 refactor leftover, no behavior).
3. **cloud_adapter compute_backoff** → extract static helper (eliminates 2x duplication).
4. **topo_scheduler build_dag** → remove debug drain loop + copy ready_queue_ instead of recomputing.
5. **plugin_loader logging** → replace `std::cerr` with project `LOG_*` macros (consistent log facade).
6. **plugin_loader non-Linux** → replace `#error` with stub implementation (cross-platform compilation).
7. **node_factory make_llm_call/dsl_call** → extract shared helper, factories become thin wrappers.
8. **layered_context** → merge `navigate()` double walk (system layer probe + real nav) into single walk; simplify `split_path()` return type.

**Verification:** Each refactor preserves existing 41-test pass rate. Behavior verified via `ctest --output-on-failure` after each commit.

### AD-3: Documentation drift fix (AGENTS.md)

**Decision:** Correct engine.h cross-module include count from "2→1" to actual "1 types + 3 common/policy" (Sprint 13 ADR-0031 added 3 policy headers).

**Rationale:** AGENTS.md is the project knowledge base. Inaccurate documentation leads to wrong architectural assumptions in future audits.

## File-Level Changes

```
src/core/
  engine.h           [+] set_tool_coordinator() declaration
  engine.cpp         [~] constructor: tool_coordinator_ = nullptr; + set_tool_coordinator() impl

src/modules/executor/
  node_executor.cpp  [-] execute_fork() / execute_join() removed
  node_executor.h    [-] 2 declarations removed
                     [~] execute_node() switch: FORK/JOIN → throw logic_error

src/modules/scheduler/
  execution_session.cpp  [-] commented legacy code block removed
  topo_scheduler.cpp     [~] build_dag() debug drain removed + ready_queue_ copy in overload

src/common/llm/
  cloud_adapter.cpp  [+] compute_backoff() static helper + 2 inline blocks replaced

src/modules/plugin/
  plugin_loader.cpp  [+] non-Linux stub impl, [~] std::cerr → LOG_* wrappers

src/modules/parser/
  node_factory.cpp   [+] parse_llm_params() + make_dsl_or_llm_call() helpers
                      [~] make_dsl_call / make_llm_call become 1-line wrappers

include/agenticdsl/types/
  layered_context.h  [~] navigate() single-pass merge + split_path() void return

AGENTS.md            [~] engine.h cross-module count corrected + Sprint 15 ship note
```

## Risk Surface

| Area | Risk | Mitigation |
|------|------|------------|
| Core engine behavior change | Medium | Full ctest 41/41 PASS + 4 existing ToolCoordinator tests cover both opt-in and direct paths |
| Scheduler fork/join logic | Low | TopoScheduler tests cover all parallel execution paths; stubs only removed from NodeExecutor (which never legitimately handled them) |
| LayeredContext system layer write protection | Low | test_layered_context covers all system-layer scenarios |
| Cross-platform build | Low | Stub only activates on non-Linux; Linux behavior unchanged |

## Compatibility Matrix

| User API | Before | After |
|----------|--------|-------|
| `DSLEngine::from_markdown(...)` | Works (ApprovalHandler path) | **Unchanged** |
| `engine->register_tool(name, fn)` | Works | **Unchanged** |
| `engine->set_execution_policy(mode)` | Works | **Unchanged** |
| `engine->set_tool_coordinator(coord)` | **New** | Opt-in middleware activation |
| `ToolCoordinator` direct usage | Works | **Unchanged** |

## Out of Scope (留 Sprint 16/17)

- Execute_single_branch() 117 行 split (中等架构债, 需要 characterization tests)
- LayeredContext thread_local 改造 (Sprint 2 CognitiveWorker 引入后才必要)
- Topo_scheduler.h 跨模块解耦 (PIMPL-lite 化, 中等风险)
- 测试覆盖率补齐 (9 个 MISSING/PARTIAL 文件)