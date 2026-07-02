# Proposal: ADR-0004 V2 — ToolRegistry Security (Metadata + Approval)

> **STATUS: ACTIVE** 🟢 — 前置依赖（C3+C4）已全部 ship，设计完成
> **预估工时**: 1 周（Sprint 16 主体）
> **关联 ADR**: `docs/adr/adr-0004-toolregistry-security.md` (✅ Approved V1，🟡 Partial 实施中) / ADR-0031 P3-P4 (✅ Approved)
> **追溯范围**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C6
> **前置依赖**: C3 (IExecutionPolicy + ApprovalHandler) + C4 (ToolCoordinator + LayerProfile) — **已全部 ship**
> **后续依赖**: C8 (Phase 4.5 MVP 清理)

## Why

ADR-0004 描述 ToolRegistry V1 安全模型 (ToolCategory / ApprovalPolicy / LayerProfile).
V1 核心设计已验证通过 `include/agenticdsl/policy/iexecution_policy.h` 接口族和 `src/common/tools/tool_coordinator.cpp`，
但**工具注册流程本身**未集成元数据校验与审批工作流。

当前现状（C4 已 ship 后）：
- ✅ ToolMetadata V2 字段（allowed_layers / cost_estimate / timeout_ms）已定义在 `execution_policy.h`
- ✅ ToolCoordinator 5-step 执行时审批已实现
- ✅ LayerProfile + check_layer_permission 执行时检查已实现
- ✅ Audit 事件 (tool.audit.*) 已通过 IInteractionBus 发射
- ❌ `ToolRegistry::register_tool_function()` 只接受 name+func，不接受 ToolMetadata
- ❌ `DECLARE_TOOL` 宏不强制 category/approval_policy
- ❌ 无注册时元数据冲突检查（e.g. `category=Execute + approval=never_approval`）
- ❌ 层权限仅在执行时检查，注册时无静态 validation

不解决此问题：
1. 工具开发者可绕过安全分类注册危险工具
2. 注册时元数据冲突在运行时才暴露（Layer mismatch）
3. PDK 宏不提供安全最佳实践引导

## What Changes

### 1. ToolRegistry register_tool_function() 升级 (Day 1-2)

```cpp
// 当前 (C4 ship 后)
void register_tool_function(std::string name, ToolFunc fn);

// 升级后 (C6)
void register_tool_function(std::string name, ToolMetadata meta, ToolFunc fn);
```

- `register_tool_function` 签名增加 `ToolMetadata meta` 参数
- 注册时执行 validation：
  - `category × approval_policy` 冲突检测（e.g. Execute + never_approval → throw）
  - `min_layer vs allowed_layers` 一致性检查（`min_layer` 必须在 `allowed_layers` 中或 `allowed_layers` 为空）
  - 名称冲突检测（已存在同名工具 → throw）
- `register_tool<Func>` 模板签名不变（name + meta + func），内部委托

### 2. DECLARE_TOOL 宏升级 (Day 3-4)

```cpp
// 当前 (Sprint 4 MVP)
DECLARE_TOOL(name, description, ...)

// 升级后 (C6)
DECLARE_TOOL(name, description, category, approval_policy, ...)
```

- 强制第 3 参数为 `ToolCategory`（枚举值：ReadOnly / WriteFile / Execute / Network / StateModify）
- 强制第 4 参数为 `ApprovalPolicy` 或简化字符串（"plan" / "agent" / "yolo" / "always"）
- 展开时生成 `ToolSpec` 自动填充 metadata 字段
- 可选参数通过 `ToolPermissions` 结构体传递（保持向后兼容）

### 3. 注册时 Layer × ToolCategory 权限矩阵 (Day 5)

权限矩阵（与 ADR-0004 §8 一致，从执行时扩展到注册时）：

| Layer \ Category | ReadOnly | WriteFile | Execute | Network | StateModify |
|:----------------:|:--------:|:---------:|:-------:|:-------:|:-----------:|
| **Cognitive**    | ✅       | ❌        | ❌      | ❌      | ❌          |
| **Thinking**     | ✅       | ✅(审批)  | ❌      | ❌      | ❌          |
| **Workflow**     | ✅       | ✅        | ✅(审批)| ✅(审批)| ✅(审批)    |

- 注册时检查：若 `allowed_layers` 非空，验证每个条目在矩阵中对该 category 合法
- 运行时执行 `check_layer_permission()` 保持不变

### 4. TUI `/apply` 桥接增强 (Day 6)

当前 `/apply` 显示基本信息（工具名、参数 key）。
增强后显示：

```
Tool:  code::edit_file
Category:  WriteFile
Layer:  Workflow (allowed: Workflow, Thinking)
Cost est:  $0.002
Timeout:  30000ms
Approval:  requires_approval_in_plan=true, in_agent=true, in_yolo=false
```

### 5. 审批桥接 ToolPreview 增强 (Day 6)

当前 `ApprovalHandler::process_request()` 收到的 `ToolPreview` 仅含 args keys（工具参数名列表）。
增强后 `preview.metadata_json` 字段包含完整 ToolMetadata V2 JSON，供 TUI `/apply` 渲染器展示：

```json
{
  "name": "code::edit_file",
  "category": "WriteFile",
  "min_layer": "Workflow",
  "allowed_layers": ["Workflow", "Thinking"],
  "cost_estimate": 0.002,
  "timeout_ms": 30000,
  "approval": {
    "requires_approval_in_plan": true,
    "requires_approval_in_agent": true,
    "requires_approval_in_yolo": false
  }
}
```

> **设计决策对齐** (design.md Decision 5): 元数据在 ToolCoordinator 审批流程中附加到 `ToolPreview` 结构体，
> 而非嵌入 `tool.audit.invoked` 审计日志 payload（保持审计日志简洁）。

## Capabilities

### ADDED Requirements

- `toolregistry-v2-register-with-meta`: ToolRegistry 注册时 MUST 接受 ToolMetadata 参数
- `toolregistry-v2-validate-conflict`: 注册时 MUST 检测 category×approval_policy 冲突
- `pdk-declare-tool-v2-mandatory`: DECLARE_TOOL 宏 MUST 强制要求 category + approval_policy
- `pdk-declare-tool-v2-compile-check`: 缺失必要宏参数 MUST 编译错误（static_assert）
- `layer-matrix-registration-check`: 权限矩阵 MUST 在注册时验证 layer 合法性
- `approval-bridge-tui-enhanced`: TUI `/apply` MUST 显示 V2 metadata

## Impact

**修改文件**:
- `src/common/tools/registry.h` — register_tool_function 签名加 ToolMetadata 参数
- `src/common/tools/registry.cpp` — 注册时 validation 实现
- `include/agenticdsl/contract/itool_registry.h` — IToolRegistry 接口同步加 meta
- `include/agenticdsl/pdk/tool_macros.h` — DECLARE_TOOL 宏升级
- `src/common/tools/tool_coordinator.cpp` — 审计日志增强
- `tests/test_tool_registry_v2.cpp` (新增)
- `tests/test_pdk_macros_v2.cpp` (新增)
- `tests/test_layer_profile_matrix.cpp` (新增)

**API 兼容性**:
- **BREAKING**: `register_tool_function()` 签名增加 ToolMetadata → 所有调用点必须更新
- **BREAKING**: `DECLARE_TOOL` 宏增加 2 个强制参数 → 所有 plugin 必须更新
- 向后兼容建议：ToolMetadata 默认构造函数允许 V1→V2 过渡期

**现有调用点**（需迁移）:
- `src/common/tools/registry.cpp` — register_default_tools() 内部注册
- `include/agenticdsl/pdk/tool_macros.h` — DECLARE_TOOL 宏展开处
- `examples/phase1_plugin_demo/` — 示例 plugin

## Non-goals

- **不修改** 运行时 check_layer_permission（已有，保持）
- **不修改** ToolCoordinator.call_tool() 内部流程（保持 5-step）
- **不修改** CognitiveWorker / DomainWorkerPool
- **不集成** IBudgetController（C4 defer，留 C8）
- **不集成** std::async 超时（C4 defer，留 C8）
- **不重写** Phase 4.5 清理

## Estimated Effort

| 阶段 | 时间 | 依赖 |
|------|:----:|:----:|
| Day 1-2: ToolRegistry 签名升级 + validation | 2d | — |
| Day 3-4: DECLARE_TOOL 宏升级 | 2d | Day 2 |
| Day 5: 权限矩阵注册时检查 | 1d | Day 2 |
| Day 6: TUI 桥接 + 审计增强 | 1d | Day 3 |
| Day 7-8: 测试 | 2d | Day 6 |
| Day 9: ship gate + 同步 | 1d | Day 8 |
| **总计** | **1 周 (9 工作日)** | |

## 详细制定 TODO

- [x] 1. ✅ 评估 DECLARE_TOOL breaking change —— 2 个调用点（PDK 宏 + phase1_plugin_demo）
- [x] 2. ✅ 决策: Layer×ToolCategory 矩阵默认值 —— 采用 ADR-0004 §8 标准矩阵
- [x] 3. ✅ 写本 change proposal.md（当前文件）—— What Changes 详细化完成
- [x] 4. ✅ 写 design.md（5 个 Decision）—— 与 proposal.md 同步完成
- [x] 5. ✅ 写 tasks.md（10 sections, 47 tasks）—— fill 完成
- [x] 6. ✅ 写 specs/toolregistry-security-v2/spec.md —— 6 ADDED Requirements 全部填充
- [x] 7. ✅ 移除所有 "PLACEHOLDER" 标记，更新 STATUS 行
- [ ] 8. `openspec validate 2026-06-26-adr-0004-v2-metadata-approval` exit 0（CLI 缺陷无法运行）
- [ ] 9. 更新 master plan C6 状态: ⚪ placeholder → 🟡 active
- [ ] 10. 启动 Sprint 16 实施