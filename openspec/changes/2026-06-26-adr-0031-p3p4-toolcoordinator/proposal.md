# Proposal: ADR-0031 P3-P4 — ToolCoordinator + Layer Profile

> **状态**: 🟡 active (C3 Sprint 13 ship 后启动, Oracle 咨询已完成 2026-06-29)
> **Oracle 决议**: session `ses_0ed4408faffeLv8VfrC0s5PzW7` (Option C middleware + 复用 LayerProfile + 4 defer 至 C6)
> **触发条件**: C3 (`2026-06-26-adr-0031-p1p2-execution-policy`) ship + archive ✅
> **关联 ADR**: ADR-0031 (P3-P4 部分, 🟡 Partial → ✅ Approved 待 C4 ship) / ADR-0004 §8 (Layer × Category 矩阵) / ADR-0019 (IInteractionBus 审计)
> **追溯范围**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C4
> **后续 change**: C6 (`2026-06-26-adr-0004-v2-metadata-approval`) 依赖本 change ToolCoordinator
> **估时**: **7d (1.5 周)** (Oracle 校正, 复用 LayerProfile + category 矩阵, 比 placeholder 1.5-2 周略短)

## Why

ADR-0031 P3-P4 部分 (ToolCoordinator + Layer Profile) 在 C3 中未实施, 拆到本 change 单独执行.

**P3 ToolCoordinator** (Day 1-5, 5d):
- **Option C (Oracle 决议)**: standalone middleware, 通过 `set_tool_coordinator(ToolCoordinator*)` 注入 NodeExecutor (非 IToolRegistry 装饰器, 避免 9-method 中 7 个无意义 forwarder)
- Absorbs ApprovalHandler 职责 (组合, 内部委托 `ApprovalHandler::process_request()`), 保留 C3 `set_approval_handler()` 标记 `[[deprecated]]` 零回归
- 当 `tool_coordinator_ == nullptr` 时, NodeExecutor 回退直接 `call_tool()` (preserve C3 auto-approve)
- 新增 audit log via `IInteractionBus::emit("tool.audit.<phase>", ToolResult)` (invoked/completed/denied/timed_out)

**P4 Layer Profile** (Day 6-7, 2d):
- **复用既有 `LayerProfile` enum** (`src/common/policy/execution_policy.h:52`, Workflow=0/Thinking=1/Cognitive=2), **不新建** `Layer` (与 C3 `IExecutionPolicy::get_layer()` 返回类型一致)
- 权限矩阵复刻 ADR-0004 §8 (Workflow=all / Thinking=ReadOnly / Cognitive=none)
- 强制点: execution time (ToolCoordinator 内, 返回 `ToolResult::error(ErrorCode::PermissionDenied)`), 非 registration
- `meta.min_layer` 与 `IExecutionPolicy::get_layer(meta)` 在 C4 视为 advisory/未使用, 强制语义留 C6

**ToolMetadata V2 扩展** (同 struct 原地扩展, ABI 兼容):
- `allowed_layers: vector<LayerProfile>` (默认空 = 全允许, 走 category 矩阵)
- `cost_estimate: double` (USD, 默认 0.0 = 不触发预算)
- `timeout_ms: int` (默认 30000)

**C4 defer 至 C6** (Oracle §决策 8):
- (a) `cost_estimate` ↔ `IBudgetController` 实际预算强制 (C4 仅 emit `tool.audit.cost` 事件)
- (b) `timeout_ms` std::async 强制 (C4 仅作元数据)
- (c) `meta.min_layer` 强制语义
- (d) `IExecutionPolicy::get_layer(meta)` 在 ToolCoordinator 中的使用

不解决此问题: (a) ADR-0031 P3-P4 永远处于待实施; (b) C6 ADR-0004 V2 无法启动 (依赖 ToolCoordinator); (c) 安全策略与执行流程无法完整集成.

## What Changes

### P3: ToolCoordinator 中间件 (Sprint 14 Day 1-5)

1. **`class ToolCoordinator`** (`src/common/tools/tool_coordinator.{h,cpp}`):
   - 持有 `IToolRegistry&`, `shared_ptr<IExecutionPolicy>`, `ApprovalCallback`, `IInteractionBus* (可选, null=skip audit)`, `ApprovalHandler` 内部实例
   - 主方法: `ToolResult execute(const ToolMetadata&, const ToolCallContext&, const std::unordered_map<std::string,std::string>& args)`
   - 内部流程: ① layer check → ② ApprovalHandler.process_request → ③ emit invoked → ④ registry.call_tool → ⑤ emit completed → return ToolResult

2. **NodeExecutor 集成** (`src/modules/executor/node_executor.{h,cpp}`):
   - 新增 `ToolCoordinator* tool_coordinator_{nullptr}` + `set_tool_coordinator(ToolCoordinator*)`
   - **保留** `set_approval_handler()` 标记 `[[deprecated("use set_tool_coordinator")]]`
   - `execute_tool_call` 内优先级: `tool_coordinator_` > `approval_handler_` > 直接 `call_tool()`
   - 互斥约定: 二者皆设则 `tool_coordinator_` 优先, 忽略 `approval_handler_` 并 log warning

3. **DSLEngine 集成** (`src/core/engine.{h,cpp}`):
   - 构造 ToolCoordinator, 注入 NodeExecutor (`node_executor_->set_tool_coordinator(coordinator_.get())`)
   - 镜像 C3 `set_approval_handler()` 注入模式 (PIMPL-lite, 不引入新跨模块 include)

### P4: Layer Profile 集成 (Sprint 14 Day 6-7)

1. **`LayerProfile parse_layer(string_view) noexcept(false)`** (`src/common/policy/layer_profile.{h,cpp}`):
   - case-insensitive ("cognitive"/"Cognitive"/"COGNITIVE" 全部接受)
   - unknown 抛 `std::invalid_argument` (fail-fast)

2. **`bool check_layer_permission(LayerProfile caller, ToolCategory category)`** (同文件):
   - 复刻 ADR-0004 §8 矩阵

3. **ToolMetadata V2 扩展** (`src/common/policy/execution_policy.h` 原地扩展, 不新建 V2 类型):
   - `allowed_layers: vector<LayerProfile>` (默认空 = 全允许)
   - `cost_estimate: double` (默认 0.0)
   - `timeout_ms: int` (默认 30000)

### Capabilities (ADDED Requirements)

- `toolcoordinator-middleware`: ToolCoordinator MUST 包装所有 tool 调用 + 集成 IExecutionPolicy + audit log
- `toolcoordinator-executor-integration`: NodeExecutor MUST 支持 `set_tool_coordinator()`, 与 `set_approval_handler()` 互斥
- `toolcoordinator-audit-log`: 工具调用 MUST 通过 `IInteractionBus::emit("tool.audit.{invoked,completed,denied}", ToolResult)` 记录 (args 仅 key, 不 value)
- `layer-profile-three-tiers`: Layer MUST 含 Workflow/Thinking/Cognitive 三层 (复用既有 enum)
- `layer-profile-category-matrix`: Layer × ToolCategory 权限 MUST 复刻 ADR-0004 §8
- `tool-metadata-v2-extensions`: ToolMetadata MUST 含 `allowed_layers`/`cost_estimate`/`timeout_ms` 3 字段

## Impact

**预期修改文件**:
- `src/common/tools/tool_coordinator.{h,cpp}` (新建, ~150 行)
- `src/common/policy/layer_profile.{h,cpp}` (新建, ~50 行)
- `src/common/policy/execution_policy.h` (扩展 ToolMetadata + parse_layer 引用)
- `src/modules/executor/node_executor.{h,cpp}` (新增 set_tool_coordinator + 互斥逻辑)
- `src/core/engine.{h,cpp}` (构造 + 注入 ToolCoordinator)
- `src/common/tools/CMakeLists.txt` (添加 tool_coordinator)
- `src/common/policy/CMakeLists.txt` (添加 layer_profile)
- `tests/test_tool_coordinator.cpp` (新建, 8 cases)
- `tests/test_layer_profile.cpp` (新建, 5 cases)
- `docs/adr/adr-0031-execution-policy.md` (新增 §决策 4/5/7/8)

**API 稳定性**:
- 既有 `set_approval_handler()` 保留 (deprecated alias), 39 C3 测试零回归
- 既有 `IExecutionPolicy::get_layer(meta)` 保留 (C4 不调用, C6 router 钩子)
- 新增 `set_tool_coordinator()` (主入口) + `ToolCoordinator::execute()` (统一执行点)

## Non-goals

- **不实施** C6 ADR-0004 V2 (`cost_estimate` BudgetController 强制 + `param_schema` + `cost_hint`)
- **不实施** `timeout_ms` std::async 强制
- **不修改** `meta.min_layer` 强制语义 (C4 advisory only)
- **不新增** AuditLogSubscriber (emit-only, TraceExporter 消费留 C6)
- **不重写** LayerProfile enum (复用 C3)

## Estimated Effort

**7d (1.5 周)**, 比 proposal placeholder 1.5-2 周略短:

| Phase | 天数 | 内容 |
|---|---|---|
| **P3 Day 1** | 1d | ToolCoordinator 骨架 + ToolMetadata V2 3 字段 + `parse_layer` helper |
| **P3 Day 2** | 1d | ApprovalHandler 委托 + IToolRegistry 委托 + layer check + audit emit |
| **P3 Day 3** | 1d | NodeExecutor 集成 (set_tool_coordinator + deprecated alias + fallback) |
| **P3 Day 4** | 1d | DSLEngine 注入 + ctest 39 baseline 零回归 |
| **P3 Day 5** | 1d | `test_tool_coordinator.cpp` 8 case + TSan/ASan 验证 |
| **P4 Day 6** | 1d | `layer_profile.{h,cpp}` + `test_layer_profile.cpp` 5 case |
| **P4 Day 7** | 1d | ADR-0031 §决策 4/5/7/8 文档 + openspec validate + archive |

**预期修改文件**:
- `src/common/tools/tool_coordinator.{h,cpp}` (新建)
- `src/modules/executor/node_executor.{h,cpp}` (集成 ToolCoordinator)
- `src/core/types/tool_metadata.h` (V2 扩展)
- `include/agenticdsl/policy/layer_profile.h` (新建)
- `src/common/policy/layer_profile.cpp` (新建)
- `tests/test_tool_coordinator.cpp` (新建)
- `tests/test_layer_profile.cpp` (新建)
- `tests/test_execution_policy.cpp` (扩展)

**API 稳定性**:
- `NodeExecutor` 公共 API 必须保持向后兼容
- `IToolRegistry::call_tool()` 保持原签名, ToolCoordinator 是包装层

## Non-goals (placeholder)

- **不重新实施** P1-P2 (C3 已完成)
- **不实质化** ADR-0004 V2 (C6 范围)
- **不修改** CognitiveWorker / DomainWorkerPool (Sprint 2/3 ship)
- **不重写** TUI (仅扩展)

## Estimated Effort (placeholder)

**总计**: 1.5-2 周 (Sprint 14 主体)

**前置依赖**: C3 (P1-P2 ship + archive)
**后续依赖**: C6 (ADR-0004 V2 依赖 ToolCoordinator) + C8 (Phase 4.5 MVP 清理)

## 详细制定 TODO (待 C3 完成后执行)

- [ ] 1. 评估: ToolCoordinator 是否需要异步路径 (依 Phase 2 决策)
- [ ] 2. 写本 change proposal.md (What Changes 详细化)
- [ ] 3. 写 design.md (5 个 Decision: ToolCoordinator 签名 / NodeExecutor 集成点 / Layer Profile 权限矩阵 / ToolMetadata V2 字段 / 审计日志格式)
- [ ] 4. 写 tasks.md (10-15 sections, 30-50 tasks)
- [ ] 5. 写 specs/toolcoordinator/spec.md (5-8 ADDED Requirements)
- [ ] 6. 移除所有 "PLACEHOLDER" 标记, 更新 STATUS 行
- [ ] 7. `openspec validate 2026-06-26-adr-0031-p3p4-toolcoordinator` exit 0
- [ ] 8. 更新 master plan C4 状态: ⚪ placeholder → 🟡 active
- [ ] 9. 启动 Sprint 14 实施
