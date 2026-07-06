# execution-session Specification

## Purpose
`src/modules/scheduler/execution_session.h` 是 scheduler 模块的 "god header" (11 include: 6 `modules/` 子模块 + 1 同模块 `resource_manager.h` + 3 `core/types/` + 1 `agenticdsl/contract/`) 触发任何子模块修改即级联 `execution_session.cpp` + `topo_scheduler.cpp` / `factory.cpp` 消费者重编;本 spec 锁定 PIMPL-lite 解耦(前向声明 + `std::unique_ptr<X>` 间接持有 + 析构 out-of-line)后行为契约,公开 API 签名零修改。
## Requirements
### Requirement: execution_session.h exposes no modules/ subproject includes
The `ExecutionSession` class header SHALL NOT include any header under the `modules/` subproject tree nor the same-module `resource_manager.h`. All subproject dependencies SHALL be satisfied by forward declarations plus out-of-line definitions in the `.cpp` translation unit, mirroring the PIMPL-lite pattern established for `MarkdownParser` (Sprint 18 D-7) and `ResourceManager` (Sprint 17 C.4).

#### Scenario: Header has zero modules/ includes
- **WHEN** `grep -c '#include "modules/' src/modules/scheduler/execution_session.h` is executed
- **THEN** the count is exactly 0 (zero `modules/` subproject includes remain)

#### Scenario: Header forward-declares all owned-by-value PIMPL members
- **WHEN** the `ExecutionSession` class declares value-type members
- **THEN** each such type is forward-declared in the header and stored via `std::unique_ptr<T>` rather than direct value composition

#### Scenario: Destructor declared out-of-line
- **WHEN** `ExecutionSession` owns any `std::unique_ptr<T>` member where `T` is only forward-declared
- **THEN** the destructor is declared in the header and defined out-of-line in `execution_session.cpp` as `~ExecutionSession() = default;` to ensure complete types are visible at instantiation point

### Requirement: ExecutionSession preserves public API contract
The PIMPL-lite refactor MUST NOT alter the observable behavior or the signature of any public method. All existing callers (`TopoScheduler`, `ExecutionSession::Factory`, tests) SHALL compile without source modification except for adding `#include` directives that were previously transitively provided.

#### Scenario: Public method signatures preserved
- **WHEN** the public methods of `ExecutionSession` are inspected
- **THEN** the following methods retain their Sprint 18 signatures:
  - `ExecutionResult execute_node(Node* node, const Context& context)`
  - `bool check_and_requeue_dynamic_deps()`
  - `bool is_budget_exceeded() const`
  - `bool needs_snapshot(const Node* node) const`
  - `void set_approval_handler(IApprovalHandler* handler)`
  - `void set_tool_coordinator(ToolCoordinator* coordinator)`
  - `ContextEngine& get_context_engine()` (signature itself preserved; type alias targets complete type via .cpp)

#### Scenario: No source change required in TopoScheduler public surface
- **WHEN** `topo_scheduler.cpp` is compiled against the new `execution_session.h`
- **THEN** only `#include` directives are added (for `ContextEngine::merge`, `IApprovalHandler`); no call-site code changes are required

### Requirement: PIMPL-lite preserves runtime behavior
The PIMPL-lite refactor MUST NOT change any observable runtime behavior of `ExecutionSession`. All 49 existing unit tests SHALL pass without modification (except `#include` additions where previously transitively provided).

#### Scenario: All tests pass without behavior changes
- **WHEN** `ctest --test-dir build -j1` is executed after the refactor
- **THEN** the test result is **100% tests passed, 0 tests failed out of 49**

#### Scenario: No memory or performance regressions
- **WHEN** AddressSanitizer and ThreadSanitizer are run against the refactored binary
- **THEN** no new memory errors, leaks, or data races are introduced compared to the pre-refactor baseline

