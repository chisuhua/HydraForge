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

> ⚠️ **2026-06-26 Oracle 决议修正 (master plan §十一.1 resolved)**: 当前 stub `include/agenticdsl/policy/iexecution_policy.h` 实际声明 **8 个纯虚函数** (`requires_approval` / `should_auto_execute` / `should_show_plan` / `should_show_result_summary` / `mode_name` / `should_auto_decide_retry` / `should_show_reflection` / `fleet_max_concurrency`), 与本 proposal 列出的 4 方法集**方法名完全不重叠** (除 `requires_approval` 签名相似). C3 实施时是**重写接口**, 不是扩展 stub. Oracle session `ses_0faa4dabeffeHGFoLdXE7AqwH7`.

### P1: IExecutionPolicy 完整实现 (Sprint 13 Day 1-3) — Oracle 推荐版

1. **重写 `class IExecutionPolicy` 5 虚函数** (替换现有 8 方法 stub):
   - `virtual bool requires_approval(const ToolMetadata&, const ToolCallContext&) const = 0;`
   - `virtual bool should_execute(const ToolMetadata&, const ToolCallContext&) const = 0;`
   - `virtual bool can_skip(const ToolMetadata&, const ToolCallContext&) const = 0;`
   - `virtual LayerProfile get_layer(const ToolMetadata&) const = 0;`
   - `virtual bool request_approval(const ToolMetadata&, const ToolCallContext&, const ToolPreview&, ApprovalCallback) const = 0;` *(Oracle 5th method: sync callback 接口, transport 可插拔)*

2. **删除 stub 现有 8 方法中的 7 个** (保留 `requires_approval` 改签名):
   - 删除 `should_auto_execute` / `should_show_plan` / `should_show_result_summary` / `should_auto_decide_retry` / `should_show_reflection` / `fleet_max_concurrency` (per-mode 常量或 IPER 推测性方法, 移出虚接口)
   - 保留 `mode_name()` 作为非虚方法或并入 `ModeConfig` 值结构体

3. **3 个默认实现** (Oracle 推荐默认 Agent 模式):
   - `PlanPolicy`: `requires_approval`=meta.category!=ReadOnly, `should_execute`=false, `can_skip`=false, `get_layer`=Workflow
   - `AgentPolicy` (默认): `requires_approval`=meta.approval_policy=="always", `should_execute`=true, `can_skip`=meta.category==ReadOnly
   - `YoloPolicy`: `requires_approval`=force_approval_always (defense-in-depth floor), `should_execute`=true, `can_skip`=true — **YOLO 切换需用户确认对话框** (防误操作)

4. **同步修订 ADR-0031** (`docs/adr/adr-0031-execution-policy.md`):
   - §决策 1: 8 方法 → 5 方法 (重写)
   - §附录"议题5最小集成": 标记 SUPERSEDED (per Oracle 决议)

5. `ToolMetadata` V1 (基础字段):
   - `name`, `description`, `category` (read/write/dangerous)
   - `risk_level` (low/medium/high)
   - `approval_policy` (always/never/conditional)

### P2: 审批机制 (Sprint 13 Day 4-7) — Oracle 推荐 sync callback 路径

> ⚠️ **路径变更**: 原 proposal 方案 (EventBus + IInteractionBus + request_id 关联 + promise/future) **被 Oracle 推翻**, 改为 sync callback 接口 + 可插拔 transport (callback 内部可选用 IInteractionBus 桥接 TUI, 但 policy 接口不依赖 bus).

1. **sync callback 接口** (Oracle 推荐):
   ```cpp
   struct ApprovalRequest {
     std::string tool_name;
     ToolMetadata meta;
     ToolCallContext ctx;
     ToolPreview preview;
     std::string request_id;
   };
   using ApprovalCallback = std::function<bool(const ApprovalRequest&, int timeout_ms)>;
   ```

2. **callback 实现由 executor 层注入** (可选用 IInteractionBus):
   - `TuiApprovalCallback`: 阻塞 stdin 读 `/apply` 命令, 解析 yes/no
   - `RemoteTuiApprovalCallback`: 内部用 IInteractionBus emit `policy.approval.requested` + 阻塞等待 `policy.approval.responded` (复用 ADR-0004 §request_confirmation 模式, **不造新基础设施**)
   - `TestAutoApprovalCallback`: 测试立即返回 true/false

3. **不实现 EventBus request_id 关联基础设施**:
   - 当前 `IInteractionBus` API 只有 emit/subscribe/unsubscribe, 无 request/response 关联原语
   - 净造基础设施 (request_id + wait_for_event) 不优于 callback 接口
   - ADR-0030 协程落地后可包成协程 wrapper, callback 边界保留
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

> **Oracle 校正 (2026-06-26)**: sync callback 路径无需造 EventBus request_id 关联基础设施 (省 2-3 天), 实际估时 **1.5 周** 更现实. 重写 stub 8→5 方法 + 删除 IPER 推测方法不增加成本 (stub 无生产消费者).

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
