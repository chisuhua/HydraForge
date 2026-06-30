## ADDED Requirements

### Requirement: iapproval-handler-interface

`include/agenticdsl/policy/iapproval_handler.h` MUST 定义 `class IApprovalHandler` 抽象接口，至少 1 个纯虚函数 `process_request(ToolMetadata, ToolCallContext, std::string& preview)`。

#### Scenario: 接口定义
- **WHEN** 编译 `include/agenticdsl/policy/iapproval_handler.h`
- **THEN** `class IApprovalHandler` 存在
- **AND** 至少 1 个纯虚函数 `process_request` 签名
- **AND** 虚析构 `virtual ~IApprovalHandler() = default;` 存在

#### Scenario: 值类型复用
- **WHEN** `IApprovalHandler::process_request` 签名
- **THEN** 参数使用 `ToolMetadata` + `ToolCallContext` (来自 `iexecution_policy.h`)
- **AND** 返回 `bool` (true=approve, false=deny)
- **AND** preview 通过引用参数返回 (避免 string copy)

### Requirement: approval-handler-implements-iapproval-handler

`ApprovalHandler` (具体类 in `src/common/policy/approval_handler.h`) MUST 实现 `IApprovalHandler` 接口。`process_request` MUST 添加 `override` 标记。

#### Scenario: 继承关系
- **WHEN** 检查 `class ApprovalHandler` 定义
- **THEN** 继承 `: public IApprovalHandler`
- **AND** `process_request` 方法有 `override` 标记
- **AND** 现有 4 个公开方法 (构造 / process_request / set_callback / 默认 timeout) 签名不变

#### Scenario: 行为保持
- **WHEN** 运行现有 4 个相关测试 (`test_tool_coordinator` + `test_layer_profile` + `test_executor_with_mock_provider` + `test_pdk_macros`)
- **THEN** 100% PASS，审批行为完全保持
- **AND** 0 警告 0 错误 (c++20 编译)

### Requirement: node-executor-approval-abstraction

`src/modules/executor/node_executor.h` MUST NOT include `common/policy/approval_handler.h`。所有 ApprovalHandler 引用 MUST 通过 `IApprovalHandler*` 抽象类型持有。

#### Scenario: 头文件无具体类 include
- **WHEN** 编译 `src/modules/executor/node_executor.h`
- **THEN** `grep -n 'approval_handler.h' src/modules/executor/node_executor.h` 输出 0 行
- **AND** `grep -n 'iapproval_handler.h' src/modules/executor/node_executor.h` 输出 ≥1 行
- **AND** 成员 `approval_handler_` 类型为 `IApprovalHandler*`

#### Scenario: set_approval_handler 签名更新
- **WHEN** 检查 `NodeExecutor::set_approval_handler` 签名
- **THEN** 参数类型为 `IApprovalHandler*`
- **AND** 实现中 `approval_handler_` 持有 `IApprovalHandler*`
- **AND** 调用方 5 处 (engine.cpp + 4 测试) 全部更新

#### Scenario: DSLEngine 集成保持
- **WHEN** `src/core/engine.cpp` 调用 `node_executor_->set_approval_handler(...)`
- **THEN** 传 `approval_handler_.get()` (自动 upcast `ApprovalHandler*` → `IApprovalHandler*`)
- **AND** DSLEngine 仍持有 `unique_ptr<ApprovalHandler>` (具体类不变)

### Requirement: test-call-sites-migrated

所有 `set_approval_handler(...)` 调用方 MUST 更新签名。零调用方 MUST 继续传 `ApprovalHandler*` 直接参数。

#### Scenario: 调用方零残留
- **WHEN** 运行 `grep -rn 'set_approval_handler(' src/ tests/`
- **THEN** 所有调用方传 `IApprovalHandler*` (编译时强制)
- **AND** 编译 0 错误 (c++20 strict)

#### Scenario: 行为完全保持
- **WHEN** 运行 48 个 ctest
- **THEN** 100% PASS
- **AND** 重点测试 `test_tool_coordinator` 18 assertions / `test_executor_with_mock_provider` 20 assertions / `test_layer_profile` X assertions 全 PASS