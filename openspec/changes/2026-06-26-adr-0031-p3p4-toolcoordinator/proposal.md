# Proposal: ADR-0031 P3-P4 — ToolCoordinator + Layer Profile

> **STATUS: PLACEHOLDER** ⚠️
> **本 change 详细 design/spec/tasks 待 C3 (Sprint 13) 完成后填充**
> **触发条件**: C3 (`2026-06-26-adr-0031-p1p2-execution-policy`) ship + archive
> **关联 ADR**: docs/adr/adr-0031-execution-policy.md (P1-P2 部分 ✅ Approved)
> **追溯范围**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C4
> **后续 change**: C6 (`2026-06-26-adr-0004-v2-metadata-approval`) 依赖本 change ToolCoordinator

## Why

ADR-0031 P3-P4 部分 (ToolCoordinator + Layer Profile) 在 C3 中未实施, 拆到本 change 单独执行.

**P3 ToolCoordinator** (Sprint 14 Day 1-5):
- 中间件层包装所有 tool 调用
- 替换 NodeExecutor 直接 call_tool 路径
- 集成 IExecutionPolicy 决策 (来自 C3)

**P4 Layer Profile** (Sprint 14 Day 6-9):
- Cognitive / Thinking / Workflow 三层权限
- 与 ToolMetadata V2 扩展集成
- Layer × Tool Category 权限矩阵

不解决此问题: (a) ADR-0031 P3-P4 永远处于待实施; (b) C6 ADR-0004 V2 无法启动 (依赖 ToolCoordinator); (c) 安全策略与执行流程无法完整集成.

## What Changes (待 C3 完成后详细制定)

### P3: ToolCoordinator 中间件 (Sprint 14 Day 1-5)

1. `class ToolCoordinator`:
   - 包装所有 `IToolRegistry::call_tool()` 调用
   - 集成 `IExecutionPolicy` 决策
   - 集成 `ApprovalCoordinator` 审批流程 (来自 C3)
   - 记录工具调用审计日志 (EventBus)

2. NodeExecutor 集成:
   - 替换直接 call_tool → ToolCoordinator.call_tool_with_policy()
   - 异常传播路径保持

### P4: Layer Profile 集成 (Sprint 14 Day 6-9)

1. `enum class Layer { Cognitive, Thinking, Workflow }`
2. `ToolMetadata` V2 扩展:
   - `allowed_layers: vector<Layer>` (默认全部)
   - `cost_estimate: double` (USD)
   - `timeout_ms: int` (默认 30s)
3. Layer × Tool Category 权限矩阵 (C6 ADR-0004 V2 详细)

## Capabilities (待详细制定)

### ADDED Requirements (placeholder)

- `toolcoordinator-middleware`: ToolCoordinator MUST 包装所有 tool 调用 + 集成 IExecutionPolicy
- `toolcoordinator-executor-integration`: NodeExecutor MUST 改用 ToolCoordinator
- `toolcoordinator-audit-log`: 工具调用 MUST 记录审计日志到 EventBus
- `layer-profile-three-tiers`: Layer MUST 含 Cognitive/Thinking/Workflow 三层
- `tool-metadata-v2-extensions`: ToolMetadata V2 MUST 含 allowed_layers / cost_estimate / timeout_ms

## Impact (待 C3 完成后评估)

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
