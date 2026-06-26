# Proposal: ADR-0031 P1-P2 — IExecutionPolicy + Approval Mechanism

> **STATUS: PLACEHOLDER** ⚠️
> **本 change 详细 design/spec/tasks 待 Sprint 12 启动前 (2026-07-30 前后) 填充**
> **触发条件**: 无依赖 (独立启动)
> **关联 ADR**: docs/adr/adr-0031-execution-policy.md (🟡 Partial, 仅头文件 stub)
> **追溯范围**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C3
> **后续 change**: C4 (`2026-06-26-adr-0031-p3p4-toolcoordinator`) 依赖本 change P1-P2 实施

## Why

ADR-0031 IExecutionPolicy 当前状态 🟡 Partial, 仅 `include/agenticdsl/policy/iexecution_policy.h` 头文件 stub 存在. docs/adr/adr-0031-execution-policy.md §决策 1 定义完整接口 (4 虚函数: requires_approval / should_execute / can_skip / get_layer) + §决策 2 描述 Plan/Agent/YOLO 三模式 + §决策 3 描述审批机制 (EventBus ApprovalRequired event → 等待用户响应).

Sprint 13 (Phase 3 入口) 启动本 change 实施 P1-P2 部分 (P3-P4 拆到 C4):
- **P1**: IExecutionPolicy 完整接口 + 3 个默认实现
- **P2**: 审批机制 (EventBus + IInteractionBus 桥接用户响应)

不解决此问题: (a) Phase 3 执行策略与安全无法落地; (b) ADR-0031 长期处于 🟡 Partial; (c) C4 ToolCoordinator 依赖本 change 审批机制; (d) C6 ADR-0004 V2 依赖本 change 决策.

## What Changes (待 Sprint 12 收官后详细制定)

### P1: IExecutionPolicy 完整实现 (Sprint 13 Day 1-3)

1. `class IExecutionPolicy` 4 虚函数:
   - `virtual bool requires_approval(const ToolMetadata&, const ToolCallContext&) const = 0;`
   - `virtual bool should_execute(const ToolMetadata&, const ToolCallContext&) const = 0;`
   - `virtual bool can_skip(const ToolMetadata&, const ToolCallContext&) const = 0;`
   - `virtual LayerProfile get_layer() const = 0;`

2. 3 个默认实现:
   - `PlanPolicy`: 全部 requires_approval=true, 用户必须显式确认
   - `AgentPolicy`: 读操作自动, 写操作 requires_approval
   - `YOLOPolicy`: 全部 should_execute=true, 无审批 (危险模式)

3. `ToolMetadata` V1 (基础字段):
   - `name`, `description`, `category` (read/write/dangerous)
   - `risk_level` (low/medium/high)
   - `approval_policy` (always/never/conditional)

### P2: 审批机制 (Sprint 13 Day 4-7)

1. EventBus event 类型:
   - `ApprovalRequired`: { tool_metadata, context, response_promise }
   - `ApprovalGranted` / `ApprovalDenied`: { request_id, decision, user_comment }

2. `ApprovalCoordinator` 中间件:
   - 订阅 `ApprovalRequired` event
   - 通过 `IInteractionBus` 推送到 TUI
   - 等待 `/apply` 命令响应
   - emit `ApprovalGranted` / `ApprovalDenied` event

3. TUI `/apply` 命令桥接:
   - TUI 接收 ApprovalRequired 显示
   - 用户输入 `/apply <request_id>` 或 `/reject <request_id>`
   - 通过 IInteractionBus 响应

## Capabilities (待详细制定)

### ADDED Requirements (placeholder)

- `execution-policy-interface`: IExecutionPolicy 4 虚函数 MUST 完整实现
- `execution-policy-three-defaults`: Plan/Agent/YOLO 3 个默认 MUST 可用
- `execution-policy-tool-metadata-v1`: ToolMetadata V1 MUST 含 category/risk_level/approval_policy
- `approval-coordinator-event-bus`: ApprovalCoordinator MUST 订阅 ApprovalRequired + emit Granted/Denied
- `approval-tui-apply-bridge`: TUI `/apply` 命令 MUST 桥接到 ApprovalCoordinator

## Impact (待 Sprint 12 收官后评估)

**预期修改文件**:
- `include/agenticdsl/policy/iexecution_policy.h` (头文件已存在, 扩展)
- `src/common/policy/execution_policy.cpp` (新建, 3 个默认实现)
- `src/common/policy/approval_coordinator.{h,cpp}` (新建)
- `src/core/types/tool_metadata.h` (新建或扩展)
- `src/common/contract/event_types.h` (扩展 ApprovalRequired/Granted/Denied)
- `src/modules/executor/node_executor.cpp` (集成 ApprovalCoordinator)
- `tests/test_execution_policy.cpp` (扩展 + 新增 Approval 测试)

**API 稳定性**:
- `IExecutionPolicy` 头文件扩展需兼容现有 stub
- 新增 3 个 Policy 类, 公共 API 增量
- `ApprovalCoordinator` 新增, 不破坏现有

## Non-goals (placeholder)

- **不实施** ADR-0031 P3 (ToolCoordinator) — 拆到 C4
- **不实施** ADR-0031 P4 (Layer Profile 集成) — 拆到 C4
- **不修改** ADR-0004 (ToolRegistry 安全) — 升级到 V2 在 C6
- **不重写** TUI 框架 — 仅扩展 `/apply` 命令

## Estimated Effort (placeholder)

参考 docs/implementation-roadmap.md §Phase 3 估时: **2 周** (Sprint 13 主体)

**前置依赖**: 无 (独立启动, 可与 Sprint 12 并行)
**后续依赖**: C4 (P3-P4 ToolCoordinator) + C6 (ADR-0004 V2) + C8 (Phase 4.5 MVP 清理)

## 详细制定 TODO (待 Sprint 12 启动前执行)

- [ ] 1. 咨询 Oracle: 4 虚函数接口 vs 6 虚函数 (如 ADR-0031 V2 修订)
- [ ] 2. 决策: 审批机制用 EventBus 还是 callback (依 ADR-0019 集成现状)
- [ ] 3. 写本 change proposal.md (What Changes 详细化)
- [ ] 4. 写 design.md (5 个 Decision: 接口签名 / 3 个 Policy 实现 / 审批流程 / TUI 桥接 / 测试策略)
- [ ] 5. 写 tasks.md (10-15 sections, 30-50 tasks)
- [ ] 6. 写 specs/execution-policy/spec.md (5-8 ADDED Requirements)
- [ ] 7. 移除所有 "PLACEHOLDER" 标记, 更新 STATUS 行
- [ ] 8. `openspec validate 2026-06-26-adr-0031-p1p2-execution-policy` exit 0
- [ ] 9. 更新 master plan C3 状态: ⚪ placeholder → 🟡 active
- [ ] 10. 启动 Sprint 13 实施
