# Decompose execution_session.h into PIMPL-lite components

## Why

`src/modules/scheduler/execution_session.h` 是 scheduler 模块的"god header" —— 单文件直接 `#include` 6 个 `modules/` 子模块 + 1 个同模块 `resource_manager.h` + 3 个 `core/types/` + 1 个 `agenticdsl/contract/`，共 11 个项目头文件。这导致每次任何子模块 (`ContextEngine` / `BudgetController` / `TraceExporter` / `NodeExecutor` / `MarkdownParser` / `StandardLibraryLoader`) 修改都会触发 `execution_session.cpp` 重编，进而级联到所有 `topo_scheduler.cpp` / `factory.cpp` 消费者。ADR-0019 §1.4 (engine.h 解耦) 已识别该问题但尚未处理。Sprint 18 D-7 (MarkdownParser PIMPL-lite) + Sprint 17 C.4 (ResourceManager PIMPL-lite) 已分别给出同模块先例，本 change 沿用相同 PIMPL-lite 模式完成 execution_session 的解耦。

## What Changes

- **`src/modules/scheduler/execution_session.h`** 移除 6 个 `modules/` 子模块 include + 1 个同模块 `resource_manager.h` include，改为前向声明 + `std::unique_ptr<X>` 间接持有。`#include` 计数 11 → 4（保留 3 个 `core/types/` + 1 个 `agenticdsl/contract/itool_registry.h`）。
- **`src/modules/scheduler/execution_session.cpp`** 添加上述 7 个 `modules/` 子模块完整 include + `make_unique<>` 构造 + 析构函数 out-of-line (`~ExecutionSession() = default;`)。`set_approval_handler` / `set_tool_coordinator` 透传逻辑从 header inline 移到 .cpp（因为 `node_executor_` 是 `unique_ptr<不完整类型>`，header 不能解引用）。
- **公开 API 签名零修改**：`execute_node()` / `check_and_requeue_dynamic_deps()` / `is_budget_exceeded()` / `get_context_engine()` / `set_approval_handler()` / `set_tool_coordinator()` 等所有公开方法签名保持不变。`set_approval_handler(IApprovalHandler*)` 参数类型变化属于姊妹 change (`pimpl-node-executor-h`) 的范畴。
- **级联 include 适配**：`src/modules/scheduler/topo_scheduler.cpp` / `src/modules/scheduler/factory.cpp` 需要补充 `context_engine.h` / `approval_handler.h` 等 PIMPL 后被间接切断的依赖。

## Capabilities

### New Capabilities

(none — 不引入新能力，纯重构)

### Modified Capabilities

- `execution-session` (existing) — 行为完全保持，仅依赖抽象 (ADR-0019 §1.4)。新增 `specs/execution-session/spec.md` delta 描述 PIMPL-lite 不变量。

## Impact

- **构建影响**：消除 cross-module include，缩短 `execution_session.h` 依赖链。`execution_session.h` 修改不再触发 6 个子模块的 include 缓存失效。
- **运行时影响**：零。PIMPL-lite 通过 `unique_ptr` 间接持有，运行时行为与重构前等价。
- **API 影响**：所有公开方法签名零变化（除 `set_approval_handler(IApprovalHandler*)` 属于姊妹 change 范畴）。
- **测试影响**：`tests/test_execute_parallel.cpp` 增加 1 行 `context_engine.h` include（用于 `ContextEngine::merge` 直接调用）。现有 49/49 测试零回归。
- **文档影响**：`AGENTS.md` 记录 Sprint 19 ship，`ADR-0019` §1.4 推进。

## Non-goals

- 不引入新的依赖注入框架 (无 di::Injector / 无 boost::di)
- 不拆分 ExecutionSession 类本身 (保持单类，PIMPL-lite 是解耦而非拆分)
- 不修改 ExecutionBudget / ParsedGraph / Context 等 POD 类型定义
- 不迁移姊妹 change (`pimpl-node-executor-h`) 的 IApprovalHandler 工作