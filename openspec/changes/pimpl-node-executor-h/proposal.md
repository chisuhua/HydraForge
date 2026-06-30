# Proposal: 解耦 NodeExecutor ApprovalHandler 依赖 (PIMPL NodeExecutor Approval)

> **STATUS: PROPOSAL** — 2026-06-30 全项目审计 D-9

## Why

`src/modules/executor/node_executor.h` 仍 include 具体类 `common/policy/approval_handler.h` (line 14)，违反 ADR-0019 §1.4 "依赖抽象" 原则:

```cpp
// node_executor.h line 14 (现状)
#include "common/policy/approval_handler.h" // ADR-0031 (2026-07-31): 审批处理器
// node_executor.h line 77 (成员)
ApprovalHandler* approval_handler_{nullptr};
```

**问题**:
1. `node_executor.h` 拖入 `ApprovalHandler` 完整类型 → 任何 `ApprovalHandler` 实现修改触发 `node_executor.cpp` 重编
2. `engine.h` 已通过 `IExecutionPolicy` 抽象解耦 policy，但 `node_executor` 未跟随同一模式
3. 已有抽象 `IExecutionPolicy` (5 虚函数) + `ToolMetadata` / `ToolCallContext` 值类型可复用 — 仅缺 `IApprovalHandler` 接口

**已 ship 模式参考**:
- Sprint 1b P1.T4: `engine.h` 通过 `IExecutionPolicy` 解耦 policy
- Sprint 13 C3: `IExecutionPolicy` 5-method 接口 (ADR-0031 §决策 1)
- Sprint 14 C4: `ToolCoordinator` 引入但保持 `ApprovalHandler` 具体类依赖

**估算影响**:
- 头文件 1 个具体类 include 移除
- 1 个新接口 `IApprovalHandler` 引入
- 1 个具体类实现 `ApprovalHandler : public IApprovalHandler`
- 公开方法签名不变 (仅类型 `ApprovalHandler*` → `IApprovalHandler*`)

## What Changes

### 1. 创建 `IApprovalHandler` 抽象接口
- 新建 `include/agenticdsl/policy/iapproval_handler.h`
- 1 个虚函数: `virtual bool process_request(const ToolMetadata&, const ToolCallContext&, std::string& preview) = 0`
- 虚析构 + 默认实现
- 复用 `iexecution_policy.h` 的 `ToolMetadata` / `ToolCallContext` 值类型

### 2. `ApprovalHandler` 改为实现 `IApprovalHandler`
- `src/common/policy/approval_handler.h` 添加 `: public IApprovalHandler`
- `process_request` 添加 `override` 标记
- 公开 API 保持兼容 (现有调用方不受影响)

### 3. `node_executor.h` 解耦
- 移除 `#include "common/policy/approval_handler.h"`
- 添加 `#include "agenticdsl/policy/iapproval_handler.h"`
- `ApprovalHandler*` 成员改为 `IApprovalHandler*`
- `set_approval_handler(ApprovalHandler*)` 签名改为 `set_approval_handler(IApprovalHandler*)` (BREAKING API)
  - 现有 4 个测试调用方 + DSLEngine 集成需更新

### 4. 验证集成路径
- `src/core/engine.cpp`: 持有 `ApprovalHandler` (具体) 但通过 `IApprovalHandler*` 传给 `NodeExecutor`
- `tests/test_tool_coordinator.cpp` / `test_layer_profile.cpp` / `test_executor_with_mock_provider.cpp`: 更新调用签名

## Capabilities

### New Capabilities
- `iapproval-handler`: 1 虚函数抽象接口 (`agenticdsl/policy/iapproval_handler.h`)

### Modified Capabilities
- `execution-policy`: 引用 `IApprovalHandler` 作为 `IExecutionPolicy` 的执行层 (与 C3 ship 一致)

## Impact

| 维度 | 影响 |
|------|------|
| 源代码变更 | 2 新文件 (iapproval_handler.h) + 2 改文件 (approval_handler.h, node_executor.h) + 1 改调用方 (engine.cpp) |
| 测试变更 | 4 个测试文件更新调用签名 |
| 行为变更 | 无 (process_request 行为完全保持) |
| API 变更 | **BREAKING** — `NodeExecutor::set_approval_handler(ApprovalHandler*)` → `set_approval_handler(IApprovalHandler*)` |
| 兼容性 | 现有 4 个测试 + 1 个 DSLEngine 集成需更新 (影响范围可控) |

## Non-goals

- **不重构 `ApprovalHandler` 内部实现** — 仅添加 `: public IApprovalHandler`
- **不创建 `MockApprovalHandler`** — 测试可用现有 `make_test_auto_callback(true)` 路径
- **不改 `IExecutionPolicy` 5-method 接口** — C3 已 ship
- **不解决 D-10 (httplib 模板重复)** — 独立 P3 change

## Estimated Effort

- `IApprovalHandler` 接口设计: 0.25 天
- `ApprovalHandler` 适配 + 验证: 0.25 天
- `node_executor.h` 解耦 + 编译: 0.25 天
- 调用方更新 (engine.cpp + 4 测试): 0.25 天
- 完整 ctest 验证: 0.25 天

**总计**: ~1.25 天 (1 个 Sprint 工作日)

## Test Strategy

- 现有 4 个相关测试 (`test_tool_coordinator` + `test_layer_profile` + `test_executor_with_mock_provider` + `test_pdk_macros`) 覆盖审批路径
- 关键场景: auto-approve / 拒绝 / 超时 / 并发
- 完整 ctest 48/48 PASS ship gate
- `set_approval_handler` 签名更新后, grep 验证零残留旧签名调用