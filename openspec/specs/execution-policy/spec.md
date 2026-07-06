# execution-policy Specification

## Purpose
ADR-0031 Sprint 13 C3 P1-P2 IExecutionPolicy 5-method 接口重写(替换原 8 方法 stub):`requires_approval` / `should_execute` / `can_skip` / `get_layer` / `request_approval` (sync callback,transport 可插拔);3 个默认实现(PlanPolicy/AgentPolicy/YoloPolicy) + `PolicyFactory` + 审批机制 via IInteractionBus 桥接用户响应;P3-P4 ToolCoordinator 延期至 C4。
## Requirements
### Requirement: execution-policy-interface-5-method

`IExecutionPolicy` MUST 包含**恰好 5 个纯虚函数** (Oracle 推荐版, 替换现有 8 方法 stub):

```cpp
class IExecutionPolicy {
 public:
  virtual ~IExecutionPolicy() = default;
  virtual bool requires_approval(const ToolMetadata&, const ToolCallContext&) const = 0;
  virtual bool should_execute(const ToolMetadata&, const ToolCallContext&) const = 0;
  virtual bool can_skip(const ToolMetadata&, const ToolCallContext&) const = 0;
  virtual LayerProfile get_layer(const ToolMetadata&) const = 0;
  virtual bool request_approval(const ToolMetadata&, const ToolCallContext&,
                                const ToolPreview&,
                                ApprovalCallback) const = 0;
};
```

#### Scenario: 头文件含 5 纯虚函数

- **WHEN** `grep "= 0" include/agenticdsl/policy/iexecution_policy.h`
- **THEN** MUST 返回 5 (仅 5 个纯虚函数)
- **AND** MUST NOT 含原 stub 的 8 方法 (`should_auto_execute` / `should_show_plan` / `should_show_result_summary` / `should_auto_decide_retry` / `should_show_reflection` / `fleet_max_concurrency`)

#### Scenario: 接口纯虚签名一致

- **WHEN** 任何 IExecutionPolicy 子类
- **THEN** MUST 实现全部 5 方法 (override 关键字)
- **AND** 缺失任一方法 MUST 导致编译错误 (因纯虚)

### Requirement: execution-policy-three-defaults

`PlanPolicy` / `AgentPolicy` / `YoloPolicy` 3 个默认 Policy 实现 MUST 可用, factory.create() MUST 接受 `PolicyMode` 枚举并返回对应实例。

#### Scenario: Policy 工厂创建三种 mode

- **WHEN** `policy_factory::create(PolicyMode::Plan)`
- **THEN** MUST 返回 `PlanPolicy` 实例 (类型断言: `dynamic_cast<PlanPolicy*>(...) != nullptr`)
- **WHEN** `policy_factory::create(PolicyMode::Agent)`
- **THEN** MUST 返回 `AgentPolicy` 实例
- **WHEN** `policy_factory::create(PolicyMode::Yolo)`
- **THEN** MUST 返回 `YoloPolicy` 实例

#### Scenario: AgentPolicy 是默认

- **WHEN** `policy_factory::create()` 无参数调用 (或 PolicyMode 默认值)
- **THEN** MUST 返回 `AgentPolicy` (Oracle 决议: Agent 是默认 mode)

### Requirement: execution-policy-agent-default-behavior

`AgentPolicy` MUST 遵循"读操作自动, 写操作需审批"原则 (ADR-0004 §"为什么 Ask 是默认而非 Deny?"):

#### Scenario: AgentPolicy 读操作自动执行

- **WHEN** `meta.category == ReadOnly` (如 file::read_file)
- **THEN** `requires_approval(meta, ctx)` MUST 返回 false
- **AND** `should_execute(meta, ctx)` MUST 返回 true
- **AND** `can_skip(meta, ctx)` MUST 返回 true

#### Scenario: AgentPolicy 写操作需审批

- **WHEN** `meta.category == Write` 且 `meta.approval_policy == "always"`
- **THEN** `requires_approval(meta, ctx)` MUST 返回 true
- **AND** `get_layer(meta)` MUST 返回 `LayerProfile::Bash`
- **AND** `request_approval(...)` MUST 同步阻塞 5min 等待用户响应

### Requirement: execution-policy-plan-mode-no-execute

`PlanPolicy` MUST 仅生成计划, 不实际执行:

#### Scenario: PlanPolicy 写操作需审批且不执行

- **WHEN** `meta.category == Write`
- **THEN** `should_execute(meta, ctx)` MUST 返回 false (Plan 模式不执行, 仅展示)
- **AND** `requires_approval(meta, ctx)` MUST 返回 true (强制审批)
- **AND** `get_layer(meta)` MUST 返回 `LayerProfile::Workflow`

### Requirement: execution-policy-yolo-defense-in-depth

`YoloPolicy` MUST 保留 approval_policy=="always" 工具的强制审批 (defense-in-depth), 但跳过其他审批:

#### Scenario: YoloPolicy 跳过 approval_policy=="conditional" 审批

- **WHEN** `meta.approval_policy == "conditional"`
- **THEN** `requires_approval(meta, ctx)` MUST 返回 false (Yolo 跳过条件审批)
- **AND** `should_execute(meta, ctx)` MUST 返回 true
- **AND** `can_skip(meta, ctx)` MUST 返回 true

#### Scenario: YoloPolicy 保留 approval_policy=="always" 审批

- **WHEN** `meta.approval_policy == "always"` (如 fs::rm_rf 等破坏性操作)
- **THEN** `requires_approval(meta, ctx)` MUST 返回 true (defense-in-depth floor, Yolo 也不跳过)
- **AND** `request_approval(...)` MUST 仍弹确认 (不可绕过)

### Requirement: execution-policy-sync-callback-approval

审批机制 MUST 采用 **sync callback 接口** (Oracle 推荐), 不可用 EventBus async (推迟到 ADR-0030 协程落地):

```cpp
using ApprovalCallback = std::function<bool(const ApprovalRequest&, int timeout_ms)>;

struct ApprovalRequest {
  std::string tool_name;
  ToolMetadata meta;
  ToolCallContext ctx;
  ToolPreview preview;
  std::string request_id;  // 唯一, 用于日志关联
};
```

#### Scenario: ApprovalHandler 调用 callback

- **WHEN** `ApprovalHandler::process_request(meta, ctx, preview)`
- **AND** policy.requires_approval(meta, ctx) == true
- **THEN** MUST 构造 `ApprovalRequest{tool_name=meta.name, meta, ctx, preview, request_id=uuid}`
- **AND** MUST 调用 `cb(req, 5000)` (5min timeout)
- **AND** MUST 返回 cb 的返回值

#### Scenario: callback 超时返回拒绝

- **WHEN** callback 阻塞 > timeout_ms
- **THEN** MUST 返回 false (拒绝, defense-in-depth)
- **AND** MUST 记录 `WARN: approval timeout, tool=meta.name, request_id=...`

#### Scenario: callback 实现可插拔

- **WHEN** `make_tui_stdin_callback()` (阻塞 stdin 读 /apply 命令)
- **WHEN** `make_event_bus_callback(bus)` (内部用 IInteractionBus 桥接 TUI, 复用 ADR-0004 §request_confirmation 模式)
- **WHEN** `make_test_auto_callback(true)` (测试立即返回)
- **THEN** 三种 callback MUST 满足 `ApprovalCallback` 签名, 可互换

### Requirement: execution-policy-mode-switch-yolo-confirmation

`ModeSwitchDialog` MUST 强制 YOLO 切换用户确认 (Oracle 决议: defense-in-depth, 防误操作):

#### Scenario: Plan↔Agent 切换可静默

- **WHEN** `set_execution_policy(Agent)` 当前为 Plan
- **THEN** MUST 静默切换 (无对话框)
- **WHEN** `set_execution_policy(Plan)` 当前为 Agent
- **THEN** MUST 静默切换 (无对话框)

#### Scenario: Agent→Yolo 切换需确认

- **WHEN** `set_execution_policy(Yolo)` 当前为 Agent
- **THEN** MUST 调用 `confirm_yolo_switch("Agent")` 弹出 "Switch to YOLO mode? All non-essential approvals will be skipped. [y/N]"
- **AND** 用户输入 "y" → 切换; 输入 "n" 或 timeout → 拒绝

#### Scenario: Yolo→任何 mode 切换可静默

- **WHEN** `set_execution_policy(Agent)` 当前为 Yolo
- **THEN** MUST 静默切换 (回退到安全 mode 不需确认)

### Requirement: execution-policy-engine-integration

DSLEngine MUST 持有 `IExecutionPolicy` 成员, 构造函数默认 `AgentPolicy`, Tool 调用前 MUST 查 policy + 调用 ApprovalHandler。

#### Scenario: DSLEngine 构造默认 AgentPolicy

- **WHEN** `DSLEngine::from_markdown(...)` 无参数构造
- **THEN** `engine.policy()` MUST 返回 `AgentPolicy` 实例

#### Scenario: NodeExecutor Tool 调用前查 policy

- **WHEN** NodeExecutor 准备调用 `tool::call(name, args)`
- **AND** `engine.policy().requires_approval(meta, ctx)` 返回 true
- **THEN** MUST 调用 `ApprovalHandler.process_request(meta, ctx, preview)`
- **AND** 拒绝时 MUST 抛 `ToolApprovalDenied` 异常
- **AND** 超时时 MUST 抛 `ToolApprovalTimeout` 异常

### Requirement: execution-policy-mode-config-non-virtual

per-mode 常量 (`show_plan` / `show_result_summary` / `auto_decide_retry` / `show_reflection` / `fleet_max_concurrency` / `mode_name`) MUST 移出虚接口, 改为 `ModeConfig` 值结构体 (Oracle 决议):

#### Scenario: ModeConfig 是值结构体

- **WHEN** `include/agenticdsl/policy/mode_config.h`
- **THEN** MUST 定义 `struct ModeConfig { bool show_plan; bool show_result_summary; bool auto_decide_retry; bool show_reflection; int fleet_max_concurrency; std::string mode_name; }`
- **AND** MUST 提供 `PlanModeConfig` / `AgentModeConfig` / `YoloModeConfig` 三个 `static constexpr ModeConfig` 常量

#### Scenario: stub 现有 8 方法被删除

- **WHEN** C3 实施完成
- **THEN** `grep "should_auto_execute\|should_show_plan\|should_show_result_summary\|should_auto_decide_retry\|should_show_reflection\|fleet_max_concurrency" include/agenticdsl/policy/iexecution_policy.h`
- **THEN** MUST 返回 0 命中 (全部删除)

### Requirement: execution-policy-adr-revision

ADR-0031 (`docs/adr/adr-0031-execution-policy.md`) MUST 同步修订以匹配新接口:

#### Scenario: §决策 1 重写为 5 虚函数

- **WHEN** C3 实施完成
- **THEN** ADR §决策 1 MUST 含 5 虚函数 (`requires_approval` / `should_execute` / `can_skip` / `get_layer` / `request_approval`)
- **AND** MUST NOT 含原 8 方法描述

#### Scenario: §附录"议题5最小集成" 标记 SUPERSEDED

- **WHEN** C3 实施完成
- **THEN** ADR §附录 MUST 含 "议题5最小集成: SUPERSEDED by C3 (Sprint 13), 新接口见 C3 proposal.md"

### Requirement: execution-policy-test-coverage

ctest MUST 含 ≥ 11 新测试 (覆盖 3 Policy × 5 方法 + callback + dialog):

#### Scenario: 新测试 ≥ 11 个

- **WHEN** `ctest -R "test_(execution_policy|mode_switch|approval)" --output-on-failure`
- **THEN** MUST ≥ 11 pass
- **AND** MUST 覆盖:
  - PlanPolicy 写操作 requires_approval=true, should_execute=false
  - AgentPolicy 读操作自动, 写操作需审批
  - YoloPolicy approval_policy=="always" 仍强制审批
  - policy_factory 默认 Agent
  - ApprovalHandler sync callback 调用 + 超时拒绝
  - 3 种 callback 实现 (stdin / event_bus / test_auto) 可互换
  - ModeSwitchDialog Yolo 切换确认 + Plan↔Agent 静默
  - engine integration: 默认 AgentPolicy, Tool 调用前查 policy

#### Scenario: 回归测试零退化

- **WHEN** `ctest --output-on-failure` 全量
- **THEN** MUST ≥ 47/47 + 11 新 = 58/58 PASS
- **AND** `cmake --preset tsan && ctest` MUST 0 race
- **AND** `cmake --preset asan && ctest` MUST 0 leak

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

