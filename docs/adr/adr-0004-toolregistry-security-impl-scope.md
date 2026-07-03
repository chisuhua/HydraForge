# ADR-0004 Implementation Scope Audit

> **生成时间**: 2026-07-03 (C9 — Phase 4.5 impl-scope audit)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0004-toolregistry-security.md](adr-0004-toolregistry-security.md)
> **状态**: ✅ Approved (V2) (audit 后保持)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved (V2), 但 7/20 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `ToolCategory` | ✅ Shipped | `src/common/policy/execution_policy.h` | V2 安全分类枚举 (ReadOnly/WriteFile/Execute/Network/StateModify) |
| `ApprovalPolicy` | ✅ Shipped | `src/common/policy/execution_policy.h` | V2 审批策略结构体 |
| `LayerProfile` | ✅ Shipped | `src/common/policy/execution_policy.h` | V2 调用层级限制枚举 |
| `ToolMetadata` | ✅ Shipped | `src/common/policy/execution_policy.h` | V2 完整工具元数据 (含 category/min_layer/approval) |
| `PathPolicy` | ✅ Shipped | `include/agenticdsl/policy/path_policy.h` | 路径策略 (Sprint 16 C6 direction 4 实施) |
| `ShellGuard` | ✅ Shipped | `include/agenticdsl/policy/path_policy.h` | Shell 命令守卫 (同上) |
| `SecureToolRegistry` | ✅ Shipped | `include/agenticdsl/tools/secure_tool_registry.h` | 安全 ToolRegistry 装饰器 (Sprint 16 C6) |
| `SecurityError` | ✅ Shipped | `include/agenticdsl/policy/path_policy.h` | 安全错误类型 |
| `ToolPermission` | 📅 Deferred | — | Allow/Ask/Deny 枚举被 `ApprovalPolicy` (V2) 替代; 未独立实现 |
| `ToolSecurityConfig` | 📅 Deferred | — | 默认安全配置 struct 未实现; 配置通过 `ToolMetadata` per-tool 注册时内联 |
| `RiskLevel` | 📅 Deferred | — | Low/Medium/High/Critical 枚举未实现; 风险由 `ToolCategory` + `ApprovalPolicy` 隐式表达 |
| `CostHint` | 📅 Deferred | — | `ToolMetadata` 中 `cost_estimate` 字段 (C6 新增) 部分覆盖; `consumes_llm_tokens` 等细化字段未实现 |
| `ConfirmationDialog` | 📅 Deferred | — | TUI 确认对话框依赖 Phase 5 TUI 子系统; 当前 `ApprovalHandler` 通过 sync callback 实现 |
| `SandboxedExecutor` | 📅 Deferred | — | ADR §Phase 2 OS 级沙箱 (Landlock/Seatbelt), 留待 Phase 5+ |
| `ContainerExecutor` | 📅 Deferred | — | ADR §Phase 3 容器级隔离 (Docker), 留待 Phase 5+ |
| `ToolSpec` | 🔁 Evolved | `src/common/policy/execution_policy.h` (`ToolMetadata`) | `ToolSpec` 演进为 `ToolMetadata` (V2 合并) |
| `ToolRegistry` | ✅ Shipped | `src/common/tools/registry.h` | 底层注册表 |
| `ToolFunc` | ✅ Shipped | `src/common/tools/registry.h` | 工具函数类型 |
| `IToolRegistry` | ✅ Shipped | `include/agenticdsl/contract/itool_registry.h` | 抽象接口 (Sprint 18 P1.T2) |
| `IExecutionPolicy` | ✅ Shipped | `include/agenticdsl/policy/iexecution_policy.h` | 执行策略抽象 (Sprint 13 C3) |

## 分类详情

### 📅 Deferred (7 个) — Phase 2/3 安全扩展

7 个 Deferred 类全部属于 ADR §Phase 2/3 扩展范畴:
- **`ToolPermission` / `ToolSecurityConfig` / `RiskLevel`**: V2 升级后, Allow/Ask/Deny 三级权限被 `ApprovalPolicy` (per-mode 审批矩阵) 替代, 不再需要独立的 `ToolPermission` 枚举
- **`CostHint`**: `ToolMetadata.cost_estimate` 字段部分覆盖; 细化的 `consumes_llm_tokens` / `estimated_duration_ms` 留待 Phase 5 预算控制精细化
- **`ConfirmationDialog`**: 依赖 Phase 5 TUI 子系统; 当前 `ApprovalHandler` 通过 sync callback (3 transport: stdin/event_bus/test_auto) 实现用户确认
- **`SandboxedExecutor` / `ContainerExecutor`**: ADR 明确标注 Phase 2 (Landlock/Seatbelt) + Phase 3 (Docker 容器), 留待 Phase 5+ 安全隔离需求明确后实施

### 🔁 Evolved — `ToolSpec` → `ToolMetadata`

ADR-0004 §2 的 `ToolSpec` (name + permission + path_policy + description + risk_level) 在 V2 升级 (Sprint 16 C6) 中合并为 `ToolMetadata` (name + description + category + min_layer + approval + allowed_layers + cost_estimate + timeout_ms), 字段更丰富且与 `IExecutionPolicy` 对齐。

## 决策

- **ADR 状态**: ✅ Approved (V2) (保持)
- **理由**: ADR-0004 V2 核心契约 (`ToolCategory` / `ApprovalPolicy` / `LayerProfile` / `ToolMetadata` / `PathPolicy` / `ShellGuard` / `SecureToolRegistry`) 13/20 已 Shipped; 7 个 Deferred 全部属于 ADR 明确标注的 Phase 2/3 扩展 (OS 沙箱 + 容器隔离 + TUI 确认对话框), 不影响 Phase 4.5 核心安全能力
- **风险**: 中 — `ConfirmationDialog` 缺失意味着当前 Ask 模式只能通过 sync callback (stdin/auto-approve) 实现, 无 TUI 交互式确认; Phase 5 TUI 启动时需补齐

## 后续行动

- Phase 5 TUI 子系统启动时, 实施 `ConfirmationDialog` (TUI 确认对话框组件)
- Phase 5+ 安全隔离需求明确后, 评估 `SandboxedExecutor` (Landlock/Seatbelt) + `ContainerExecutor` (Docker) 实施
- `CostHint` 细化字段留待 Phase 5 预算控制精细化时补齐
- 本 audit 文档供 Phase 5 backlog 参考
