# Design: Decompose execution_session.h into PIMPL-lite components

## 架构合规性

- **ADR-0019 §1.4**: 本 change 直接推进该 ADR 的剩余执行入口。`execution_session.h` 当前 6 个 `modules/` 子模块 include 是 ADR-0019 识别的 engine.h 解耦残余。完成本 change 后，scheduler 模块内部 cross-module include 计数清零。
- **ADR-0019 §1.4 决胜模式**: 与 Sprint 17 C.4 (`ResourceManager` PIMPL-lite) + Sprint 18 D-7 (`MarkdownParser` PIMPL-lite) 同模式。三个 change 形成 ADR-0019 §1.4 的完整收官。
- **C++20 标准合规**: 使用 `std::unique_ptr<T>` + 前向声明 + `= default` out-of-line 析构。C++17 起保证的 forward-declared unique_ptr 析构语义 (PIMPL 友好)。

## 设计方案

### 1. Header (.h) 变更

```cpp
// 移除 (6 个 `modules/` + 1 个同模块):
- #include "modules/context/context_engine.h"
- #include "modules/budget/budget_controller.h"
- #include "modules/trace/trace_exporter.h"
- #include "modules/executor/node_executor.h"
- #include "modules/parser/markdown_parser.h"
- #include "modules/library/library_loader.h"
- #include "resource_manager.h"

// 保留 (3 个 core/types/ + 1 个 contract):
#include "core/types/context.h"
#include "core/types/node.h"
#include "core/types/budget.h"
#include "agenticdsl/contract/itool_registry.h"  // P1.T2 IToolRegistry 抽象

// 新增 (前向声明 7 类):
class ContextEngine;
class BudgetController;
class TraceExporter;
class NodeExecutor;
class MarkdownParser;
class StandardLibraryLoader;
class ResourceManager;
```

### 2. Header 成员变更

```cpp
// 旧 (值成员):
private:
    ResourceManager& resource_manager_;        // ← 引用 (保持)
    ContextEngine context_engine_;             // ← 改为 unique_ptr
    BudgetController budget_controller_;       // ← 改为 unique_ptr
    TraceExporter trace_exporter_;             // ← 改为 unique_ptr
    NodeExecutor node_executor_;               // ← 改为 unique_ptr

// 新 (PIMPL-lite 间接持有):
private:
    ResourceManager& resource_manager_;                       // ← 引用 (保持, 不拥有)
    std::unique_ptr<ContextEngine> context_engine_;            // ← PIMPL
    std::unique_ptr<BudgetController> budget_controller_;      // ← PIMPL
    std::unique_ptr<TraceExporter> trace_exporter_;            // ← PIMPL
    std::unique_ptr<NodeExecutor> node_executor_;              // ← PIMPL
```

### 3. Header 析构声明 (新增)

```cpp
class ExecutionSession {
public:
    ExecutionSession(...);
    ~ExecutionSession();  // PIMPL-lite 要求 unique_ptr 不完整类型成员的析构 out-of-line
    // ...
};
```

### 4. Header set_approval_handler / set_tool_coordinator 透传外移

```cpp
// 旧 (inline 透传, header 中直接解引用):
void set_approval_handler(ApprovalHandler* handler) {
    node_executor_.set_approval_handler(handler);  // ← header 中解引用值成员
}
void set_tool_coordinator(ToolCoordinator* coordinator) {
    node_executor_.set_tool_coordinator(coordinator);
}

// 新 (header 声明, .cpp 定义):
void set_approval_handler(IApprovalHandler* handler);  // (类型来自姊妹 change)
void set_tool_coordinator(ToolCoordinator* coordinator);
```

理由：`node_executor_` 是 `unique_ptr<不完整类型>`，header 不能解引用。必须移到 .cpp。

### 5. .cpp 构造器 (make_unique)

```cpp
ExecutionSession::ExecutionSession(
    std::optional<ExecutionBudget> initial_budget,
    IToolRegistry& tool_registry,
    ILLMProvider* llm_provider,
    ResourceManager& resource_manager,
    const std::vector<ParsedGraph>* full_graphs,
    AppendGraphsCallback append_graphs_callback)
    : resource_manager_(resource_manager),
      context_engine_(std::make_unique<ContextEngine>()),
      budget_controller_(std::make_unique<BudgetController>(std::move(initial_budget))),
      trace_exporter_(std::make_unique<TraceExporter>()),
      node_executor_(std::make_unique<NodeExecutor>(tool_registry, llm_provider)),
      full_graphs_(full_graphs),
      append_graphs_callback_(std::move(append_graphs_callback)) {
    node_executor_->set_append_graphs_callback(append_graphs_callback_);
    if (initial_budget.has_value()) {
        context_engine_->set_snapshot_limits(...);
    } else {
        context_engine_->set_snapshot_limits(10, 512);
    }
}

ExecutionSession::~ExecutionSession() = default;
```

### 6. 级联 include 适配

`topo_scheduler.cpp` / `factory.cpp` / `test_execute_parallel.cpp` 直接调用 `ContextEngine::merge` / `ContextMergePolicy` / `ResourceManager::~ResourceManager()` 等需要完整类型。execution_session.h PIMPL 后不再传递这些 include，需消费者显式添加：

```cpp
// src/modules/scheduler/topo_scheduler.cpp
+#include "modules/context/context_engine.h"  // ContextEngine::merge, get_snapshot
+#include "agenticdsl/policy/iapproval_handler.h"  // 姊妹 change 类型

// src/modules/scheduler/factory.cpp
+#include "resource_manager.h"  // unique_ptr<TopoScheduler> 销毁需 ResourceManager 完整类型

// tests/test_execute_parallel.cpp
+#include "resource_manager.h"  // 同 factory
```

### 7. ADR-0019 §1.4 推进

完成本 change 后：
- `execution_session.h` cross-module include 计数清零 (header 仅含 `core/types/` POD + 1 个 `agenticdsl/contract/`)
- ADR-0019 §1.4 全部 ship (engine.h + execution_session.h 均 PIMPL-lite 化)
- 整体 pattern 收敛: Sprint 17 C.4 (ResourceManager) + Sprint 18 D-7 (MarkdownParser) + Sprint 19 D-8 (ExecutionSession 4 个成员)

## 决策记录

### 决策 1: 完整类型移到 .cpp

**选项**: A) 移到 .cpp + unique_ptr (PIMPL-lite) / B) 保留 header include 但加注释 / C) 引入抽象接口 (`IContextEngine` 等)

**决议**: A

**理由**:
- 与 Sprint 17 C.4 + Sprint 18 D-7 同模式 (一致性)
- 零运行时开销 (unique_ptr 解引用与值成员等价)
- C++17 起 unique_ptr<T 不完整> 在 header 中合法
- 不引入新抽象层 (C 选项会增加 4 个接口类, 违反 YAGNI)

### 决策 2: `set_approval_handler` / `set_tool_coordinator` 透传外移

**选项**: A) header inline 透传 (旧) / B) .cpp 透传 (新)

**决议**: B

**理由**:
- PIMPL 后 `node_executor_` 是 unique_ptr<不完整类型>，header 不能 `.` 解引用
- 透传逻辑本身 1-2 行，外移到 .cpp 无成本

### 决策 3: ResourceManager 保持引用而非 unique_ptr

**选项**: A) 改 unique_ptr<ResourceManager> / B) 保持 ResourceManager& (引用)

**决议**: B

**理由**:
- ResourceManager 由 TopoScheduler 拥有并通过引用注入 (Sprint 17 C.4 已建立契约)
- ExecutionSession 不拥有 ResourceManager 生命周期，无需 PIMPL
- 引用类型仅需前向声明 (header 中 `class ResourceManager;` 已足够)

## ADR 影响

- **ADR-0019 §1.4**: ✅ **完全退出标准达成** (engine.h + execution_session.h 均 PIMPL-lite)
- 无新增 ADR
- 无修改现有 ADR

## 兼容性

- **二进制兼容**: 保持 (公开 API 零修改)
- **源码兼容**: 保持 (PIMPL 是实现细节，不影响 #include 用户的 .cpp)
- **行为兼容**: 100% 保持 (49/49 测试零回归)

## 风险评估

| 风险 | 等级 | 缓解 |
|---|---|---|
| Unique_ptr 不完整类型成员析构报错 | 🟡 中 | out-of-line `~ExecutionSession() = default;` 已在 .cpp 完整类型可见 |
| 消费者遗漏 include | 🟢 低 | 编译器报错明确指出缺失符号，链接即可发现 |
| 公开 API 签名意外变化 | 🟢 低 | 公开方法签名逐个对比，零变化 |
| 测试覆盖不足 | 🟢 低 | 49/49 测试零回归已验证 |

## 实施时间线

- Day 1: Header 重构 (前向声明 + unique_ptr 化 + 析构声明) + .cpp 完整类型 include + make_unique 化
- Day 2: 消费者 include 适配 (topo_scheduler.cpp + factory.cpp + test_execute_parallel.cpp)
- Day 3: build + ctest 49/49 PASS 验证 + AGENTS.md 更新
- Day 4: archive openspec change