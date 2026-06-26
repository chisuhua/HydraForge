# Proposal: ADR-0004 V2 — ToolRegistry Security (Metadata + Approval)

> **STATUS: PLACEHOLDER** ⚠️
> **本 change 详细 design/spec/tasks 待 C4 (Sprint 14) 完成后填充**
> **触发条件**: C4 (`2026-06-26-adr-0031-p3p4-toolcoordinator`) ship + archive
> **关联 ADR**: docs/adr/adr-0004-toolregistry-security.md (✅ Approved, V1 基础) / ADR-0031 P3-P4 (✅ Approved)
> **追溯范围**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C6

## Why

ADR-0004 当前状态 ✅ Approved, 描述 ToolRegistry V1 安全模型 (ToolCategory / ApprovalPolicy / LayerProfile). 但 V1 仅设计文档, 实际工具注册流程未集成元数据 + 审批工作流.

C4 (ADR-0031 P3-P4) 实施 ToolCoordinator + LayerProfile, 提供执行层面的元数据决策. C6 在此基础上升级 ToolRegistry 注册流程:
- ToolMetadata V2 (含 category / risk_level / approval_policy / allowed_layers / cost_estimate / timeout)
- DECLARE_TOOL 宏扩展 (PDK 集成)
- 审批工作流集成 (ToolCoordinator + IExecutionPolicy 联动)
- TUI `/apply` 桥接 (用户确认/拒绝 UI)
- Layer × Tool Category 权限矩阵

不解决此问题: (a) 工具安全仅靠 ToolCoordinator 单点决策, ToolRegistry 注册端无 metadata 校验; (b) DECLARE_TOOL 宏不强制要求元数据; (c) Layer Profile 仅在执行时检查, 注册时无 validation.

## What Changes (待 C4 完成后详细制定)

### 1. ToolMetadata V2 完整集成 (Sprint 16 Day 1-3)

1. `ToolMetadata` V2 字段完整实施:
   - `name` / `description` (V1 已存在)
   - `category` (read / write / dangerous) (V1)
   - `risk_level` (low / medium / high) (V1)
   - `approval_policy` (always / never / conditional) (V1)
   - `allowed_layers: vector<Layer>` (C4 引入)
   - `cost_estimate: double` (USD) (C4 引入)
   - `timeout_ms: int` (默认 30s) (C4 引入)

2. ToolRegistry 升级:
   - 注册时强制要求完整 ToolMetadata V2
   - 启动时 validation: 检测元数据冲突 (如 `category=dangerous` 但 `approval_policy=never`)

### 2. DECLARE_TOOL 宏扩展 (Sprint 16 Day 4-5)

1. PDK 宏升级 (`include/agenticdsl/pdk/tool_macros.h`):
   - 强制声明 category / risk_level / approval_policy
   - 可选声明 allowed_layers / cost_estimate / timeout
2. 示例 plugin 更新 (phase1_plugin_demo)
3. 编译时检查: 缺失必要字段 → 编译错误

### 3. 审批工作流集成 (Sprint 16 Day 6-7)

1. ToolCoordinator + IExecutionPolicy 联动 (C3 引入):
   - 执行前: IExecutionPolicy.requires_approval() 决策
   - 需要审批: ApprovalCoordinator 介入
   - 审批后: ToolCoordinator.call_tool_with_policy() 执行
2. TUI `/apply` 桥接增强:
   - 显示 ToolMetadata V2 完整信息
   - 用户可查看 cost_estimate / risk_level

### 4. Layer × Tool Category 权限矩阵 (Sprint 16 Day 8-9)

1. 权限矩阵定义 (C4 LayerProfile 扩展):
   - Cognitive: 仅 read 类工具
   - Thinking: read + write (审批)
   - Workflow: 全部 (含 dangerous, 审批)
2. 启动时 validation: 检测违规注册
3. 运行时 enforcement: ToolCoordinator 检查 caller layer

## Capabilities (待详细制定)

### ADDED Requirements (placeholder)

- `toolregistry-v2-metadata-full`: ToolMetadata V2 MUST 完整集成 (含 V1 + C4 字段)
- `toolregistry-v2-register-validation`: ToolRegistry 注册时 MUST validation 元数据冲突
- `pdk-declare-tool-v2-mandatory`: DECLARE_TOOL 宏 MUST 强制要求 category / risk_level / approval_policy
- `pdk-declare-tool-v2-compile-check`: 缺失必要字段 MUST 编译错误
- `approval-workflow-toolcoordinator-link`: ToolCoordinator + IExecutionPolicy MUST 联动
- `layer-profile-permission-matrix`: Layer × Tool Category 权限矩阵 MUST 强制 enforcement

## Impact (待 C4 完成后评估)

**预期修改文件**:
- `src/common/tools/registry.{h,cpp}` (V2 升级)
- `src/core/types/tool_metadata.h` (V2 字段完整)
- `include/agenticdsl/pdk/tool_macros.h` (DECLARE_TOOL 升级)
- `src/common/tools/tool_coordinator.cpp` (审批联动增强)
- `src/common/policy/layer_profile.cpp` (权限矩阵)
- `tests/test_tool_registry_v2.cpp` (新建)
- `tests/test_pdk_macros_v2.cpp` (新建)
- `tests/test_layer_profile_matrix.cpp` (新建)
- `examples/phase1_plugin_demo/` (更新示例)

**API 兼容性**:
- **breaking change**: ToolMetadata V1 字段保留, V2 字段可选, 但 DECLARE_TOOL 升级为强制
- 现有 plugin 需要更新 DECLARE_TOOL 声明
- 向后兼容: 旧 plugin 编译错误, 必须更新 (Phase 4.5 处理)

## Non-goals (placeholder)

- **不重写** Phase 4.5 清理 (C8 范围)
- **不实质化** ADR-0007 (上下文压缩)
- **不修改** CognitiveWorker / DomainWorkerPool

## Estimated Effort (placeholder)

**总计**: 1 周 (Sprint 16 主体)

**前置依赖**: C3 (P1-P2) + C4 (P3-P4) 全部 ship
**后续依赖**: C8 (Phase 4.5 MVP 清理依赖 ToolMetadata V2)

## 详细制定 TODO (待 C4 完成后执行)

- [ ] 1. 评估: DECLARE_TOOL breaking change 迁移路径 (现有 plugin 数量)
- [ ] 2. 决策: Layer × Tool Category 权限矩阵默认值
- [ ] 3. 写本 change proposal.md (What Changes 详细化)
- [ ] 4. 写 design.md (5 个 Decision: ToolMetadata V2 字段 / DECLARE_TOOL 宏升级 / 审批联动 / 权限矩阵 / migration 路径)
- [ ] 5. 写 tasks.md (10-15 sections, 30-50 tasks)
- [ ] 6. 写 specs/toolregistry-security-v2/spec.md (5-8 ADDED Requirements)
- [ ] 7. 移除所有 "PLACEHOLDER" 标记, 更新 STATUS 行
- [ ] 8. `openspec validate 2026-06-26-adr-0004-v2-metadata-approval` exit 0
- [ ] 9. 更新 master plan C6 状态: ⚪ placeholder → 🟡 active
- [ ] 10. 启动 Sprint 16 实施
