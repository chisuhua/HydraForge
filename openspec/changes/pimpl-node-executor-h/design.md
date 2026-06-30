# Design: 解耦 NodeExecutor ApprovalHandler 依赖

## Context

`node_executor.h` 仍 include 具体类 `ApprovalHandler`，违反 ADR-0019 §1.4 抽象依赖原则。Sprint 13 C3 已 ship `IExecutionPolicy` 5-method 接口，但 `ApprovalHandler` (具体) 仍是 executor 唯一选择。

## Goals / Non-Goals

**Goals:**
- 引入 `IApprovalHandler` 抽象接口
- `node_executor.h` 仅依赖抽象
- 现有 `ApprovalHandler` 行为完全保持
- 公开方法签名更新 (`ApprovalHandler*` → `IApprovalHandler*`)

**Non-Goals:**
- 不重构 `ApprovalHandler` 内部
- 不改 `IExecutionPolicy` 5-method 接口 (C3 ship)
- 不创建 Mock 类

## Decisions

### Decision 1: 接口位置 — `include/agenticdsl/policy/iapproval_handler.h`
**理由**: 与 `iexecution_policy.h` 同目录，符合项目 policy 抽象集中模式。

**结论**: 标准位置。

### Decision 2: 接口最小化 — 1 虚函数
**`IApprovalHandler` 唯一虚函数**:
```cpp
class IApprovalHandler {
 public:
  virtual ~IApprovalHandler() = default;
  
  /// @brief 处理一次工具调用的审批请求
  /// @return true=approve, false=deny/timeout
  virtual bool process_request(const ToolMetadata& meta,
                               const ToolCallContext& ctx,
                               std::string& preview) = 0;
};
```

**替代方案**:
- 多虚函数 (requires_approval / approve / deny 分离) → 增大接口面，无明显收益
- 完整复用 `IExecutionPolicy` 5-method → executor 不关心 policy 决策 (那是 ApprovalHandler 内部)，只关心"一次请求结果"

**结论**: 1 虚函数最简。`ApprovalHandler` 内部封装 `IExecutionPolicy` 决策 + callback 调用。

### Decision 3: `ApprovalHandler` 适配 — `public IApprovalHandler`
```cpp
class ApprovalHandler : public IApprovalHandler {
 public:
  explicit ApprovalHandler(...);
  
  bool process_request(const ToolMetadata& meta,
                       const ToolCallContext& ctx,
                       std::string& preview) override;
  
  // ... 其他 ApprovalHandler 内部 API 保持不变
};
```

**结论**: 继承 + override，最小侵入。

### Decision 4: 公开 API 更新策略 — BREAKING
```cpp
// 旧: NodeExecutor::set_approval_handler(ApprovalHandler*)
// 新: NodeExecutor::set_approval_handler(IApprovalHandler*)
```

**调用方影响** (5 处):
- `src/core/engine.cpp:204` (DSLEngine 集成)
- `tests/test_tool_coordinator.cpp`
- `tests/test_layer_profile.cpp`
- `tests/test_executor_with_mock_provider.cpp`
- `tests/test_pdk_macros.cpp` (可能)

**结论**: BREAKING 变更，所有调用方需更新为 `IApprovalHandler*`。
- DSLEngine 持有 `unique_ptr<ApprovalHandler>` (具体) 不变
- 通过 `approval_handler_.get()` 传给 `NodeExecutor` (自动 upcast)
- 测试调用方: `auto concrete = std::make_unique<ApprovalHandler>(...)` → 传 `concrete.get()`

### Decision 5: 编译顺序 — 接口先 + 适配次 + executor 解耦最后
1. Step 1: 创建 `iapproval_handler.h` (新文件, 0 风险)
2. Step 2: `ApprovalHandler` 适配 `: public IApprovalHandler` (0 风险，已有客户不受影响)
3. Step 3: `node_executor.h` 解耦 (BREAKING, 需 1+2 已 ship)
4. Step 4: 调用方更新 (5 处, 1-2 行/处)
5. Step 5: 验证

## Risks / Trade-offs

| Risk | Mitigation |
|------|-----------|
| BREAKING API 影响外部用户 | 项目处于 Pre-Phase 0 / 内部使用，零外部依赖 |
| 调用方遗漏更新 | grep `set_approval_handler` 全代码库 + 编译错误暴露 |
| `IApprovalHandler` 接口设计不当 (未来需扩展) | YAGNI 原则，仅 1 虚函数，需要时再扩展 |
| 多态调用增加 1 次虚函数跳转 | executor 是热路径但 process_request 调用频率低 (< 1K/s)，影响 < 0.1% |

## Migration Plan

### Step 1: 新建 `iapproval_handler.h`
1. 创建 `include/agenticdsl/policy/iapproval_handler.h`
2. 添加 `class IApprovalHandler` 1 虚函数
3. 包含 `agenticdsl/policy/iexecution_policy.h` (复用值类型)
4. 验证编译 0 错误

### Step 2: `ApprovalHandler` 适配
1. `src/common/policy/approval_handler.h`: 添加 `: public IApprovalHandler`
2. `process_request` 添加 `override` 标记
3. 验证编译 0 错误 (此时 IApprovalHandler 仍 0 调用方)
4. 验证 ctest 48/48 PASS

### Step 3: `node_executor.h` 解耦
1. 移除 `#include "common/policy/approval_handler.h"`
2. 添加 `#include "agenticdsl/policy/iapproval_handler.h"`
3. `ApprovalHandler* approval_handler_` → `IApprovalHandler* approval_handler_`
4. `set_approval_handler(ApprovalHandler*)` → `set_approval_handler(IApprovalHandler*)`
5. 验证编译错误 (调用方未更新)

### Step 4: 调用方更新 (5 处)
1. `src/core/engine.cpp` — `node_executor_->set_approval_handler(approval_handler_.get())` (自动 upcast)
2. `tests/test_tool_coordinator.cpp` — 更新签名
3. `tests/test_layer_profile.cpp` — 更新签名
4. `tests/test_executor_with_mock_provider.cpp` — 更新签名
5. `tests/test_pdk_macros.cpp` (如存在) — 更新签名

### Step 5: 完整验证
1. `cmake --build build/tests -j$(nproc)` 0 错误
2. `cd build/tests && ctest` 48/48 PASS
3. 重点验证: `test_tool_coordinator` 18 assertions / `test_executor_with_mock_provider` 20 assertions

### Rollback
revert 5 个 commit 即可。Step 2 之前零风险 (Step 1 仅新文件)。Step 3-5 单原子 commit 整体回滚。

## Open Questions

无 — 模式与 Sprint 13 C3 `IExecutionPolicy` ship 一致。