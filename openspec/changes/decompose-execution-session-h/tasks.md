# Tasks: PIMPL-lite execution_session.h decoupling (Sprint 19 D-8)

## 1. Header 重构

- [x] 1.1 移除 6 个 `modules/` 子模块 include (`context_engine.h` + `budget_controller.h` + `trace_exporter.h` + `node_executor.h` + `markdown_parser.h` + `library_loader.h`) + 1 个同模块 `resource_manager.h` include
- [x] 1.2 添加 7 个类型前向声明 (`ContextEngine` + `BudgetController` + `TraceExporter` + `NodeExecutor` + `MarkdownParser` + `StandardLibraryLoader` + `ResourceManager`)
- [x] 1.3 4 个值成员改为 `std::unique_ptr<T>` (`context_engine_` + `budget_controller_` + `trace_exporter_` + `node_executor_`)；`ResourceManager& resource_manager_` 保持引用
- [x] 1.4 添加析构函数声明 `~ExecutionSession();` (PIMPL-lite 要求 out-of-line)
- [x] 1.5 `set_approval_handler(IApprovalHandler*)` + `set_tool_coordinator(ToolCoordinator*)` 透传逻辑从 header inline 移到 .cpp 声明 (header 不能解引用 unique_ptr 不完整类型)
- [x] 1.6 验证 `grep -c '#include "modules/' src/modules/scheduler/execution_session.h` 输出 0

## 2. .cpp 实现适配

- [x] 2.1 添加 6 个 `modules/` 子模块完整 include + 1 个 `resource_manager.h` include 到 execution_session.cpp
- [x] 2.2 构造器改用 `std::make_unique<>` 构造 4 个 PIMPL 成员 (直接构造 init list 替换)
- [x] 2.3 4 个 PIMPL 成员的成员访问从 `.` 改为 `->` (例如 `context_engine_.save_snapshot(...)` → `context_engine_->save_snapshot(...)`)
- [x] 2.4 添加析构函数 out-of-line 定义 `ExecutionSession::~ExecutionSession() = default;`
- [x] 2.5 `set_approval_handler` + `set_tool_coordinator` 透传逻辑 .cpp 实现 (含 `if (node_executor_)` 防御 null 指针)

## 3. 消费者级联 include 适配

- [x] 3.1 `src/modules/scheduler/topo_scheduler.cpp` 添加 `#include "modules/context/context_engine.h"` (用于 `ContextEngine::merge` + `get_snapshot` + `ContextMergePolicy`)
- [x] 3.2 `src/modules/scheduler/topo_scheduler.cpp` 添加 `#include "agenticdsl/policy/iapproval_handler.h"` (姊妹 change 类型)
- [x] 3.3 `src/modules/scheduler/factory.cpp` 添加 `#include "resource_manager.h"` (`unique_ptr<TopoScheduler>` 销毁需 `ResourceManager` 完整类型)
- [x] 3.4 `src/modules/scheduler/topo_scheduler.cpp` 添加 `#include "trace_exporter.h"` (用于 `TraceExporter& get_trace_exporter()` 内联 getter 的 PIMPL 解耦适配)
- [x] 3.5 `tests/test_execute_parallel.cpp` 添加 `#include "resource_manager.h"` (同 factory)

## 4. 验证

- [x] 4.1 完整构建: `cmake --preset tests -S . -B build && cmake --build build -j$(nproc)` 0 错误
- [x] 4.2 全量 ctest: `ctest --test-dir build -j1` 输出 **100% tests passed, 0 tests failed out of 49**
- [x] 4.3 LSP diagnostics: `lsp_diagnostics src/modules/scheduler/execution_session.h` 仅显示 pre-existing 错误 (与本 change 无关)
- [x] 4.4 验证 `execution_session.h` line count: 109 → 122 (净 +13, 前向声明 + 析构声明 - 7 个 include 行)
- [x] 4.5 验证公开 API 签名零变化: `execute_node` / `check_and_requeue_dynamic_deps` / `is_budget_exceeded` / `needs_snapshot` / `get_context_engine` / `get_budget_controller` / `get_trace_exporter` / `get_dynamic_wait_for_expressions` 签名保持

## 5. 文档与归档

- [x] 5.1 更新 `AGENTS.md` Sprint 19 ship 记录 (本 change + 姊妹 change `pimpl-node-executor-h` + `examples-mockllm-migration`)
- [x] 5.2 更新 `ADR-0019` §1.4 状态: ✅ 完全退出标准达成 (engine.h + execution_session.h 均 PIMPL-lite)
- [x] 5.3 OpenSpec validate: `openspec validate decompose-execution-session-h --strict` exit 0
- [x] 5.4 OpenSpec archive: `openspec archive decompose-execution-session-h --yes`

## 6. 提交

- [x] 6.1 git add openspec/changes/decompose-execution-session-h/ + src/modules/scheduler/{execution_session.{h,cpp}, factory.{h,cpp}, topo_scheduler.{h,cpp}} + tests/test_execute_parallel.cpp
- [x] 6.2 git commit -m "refactor(scheduler): PIMPL-lite execution_session.h 解耦 (Sprint 19 D-8)" 携带 Sisyphus co-author
- [x] 6.3 验证 `git log -1` 提交内容正确