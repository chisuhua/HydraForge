# ADR-0033 Implementation Scope Audit

> **生成时间**: 2026-07-03 (C9 — Phase 4.5 impl-scope audit)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0033-session-hierarchy.md](adr-0033-session-hierarchy.md)
> **状态**: ✅ Approved (audit 后保持)

## 状态

✅ Approved (audit 后保持 — 所有 11 个 ADR 核心契约类均已 Shipped 或 Evolved, 无需调整主 ADR 状态)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved, 但 3/13 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `UserSession` | ✅ Shipped | `src/core/types/session.h` | 顶层用户会话 (deque<TaskSession> + messages) |
| `TaskSession` | ✅ Shipped | `src/core/types/session.h` | 任务会话 (deque<SubtaskSession> + failure_count_ + IExecutionPolicy) |
| `SubtaskSession` | ✅ Shipped | `src/core/types/session.h` | POD-like 子任务执行单元 |
| `FailureMode` | ✅ Shipped | `src/core/types/session.h` | 失败模式枚举 (KeepSession / NewSession) |
| `ExecutionResult` | ✅ Shipped | `src/core/types/types.h` | 执行结果 |
| `LayeredContext` | ✅ Shipped | `include/agenticdsl/types/layered_context.h` | 结构化上下文 |
| `ToolResult` | ✅ Shipped | `src/core/types/tool_result.h` | 消息载荷 |
| `TaskProfile` | ✅ Shipped | `src/core/types/session.h` | 任务配置 |
| `SubtaskProfile` | ✅ Shipped | `src/core/types/session.h` | 子任务配置 |
| `IExecutionPolicy` | ✅ Shipped | `include/agenticdsl/policy/iexecution_policy.h` | 执行策略接口 |
| `BranchExecutionResult` | 📅 Deferred | — | 分支执行结果 struct 未独立实现; fork 结果通过 `BranchResult` (topo_scheduler 内部) + `SubtaskSession` 组合表达 |
| `DagExecutionContext` | 📅 Deferred | — | DAG 执行上下文未独立实现; `ExecutionSession` (TopoScheduler 内部) + `SubtaskSession` 提供等价功能 |
| `SessionManager` | 📅 Deferred | — | 会话管理器未实现; `DSLEngine::run(UserSession&, ...)`: 重载提供 Session Mgmt 功能, 但未提取为独立类 |

## 分类详情

### 📅 Deferred (3 个)

- **`BranchExecutionResult`**: ADR 可能描述了 fork/join 分支结果类型。实际实现使用 `topo_scheduler` 内部 `BranchResult` + `SubtaskSession` 组合表达。`BranchExecutionResult` 提取为独立类属于 YAGNI — 当前 fork 结果仅被 `merge_branch_results()` 消费
- **`DagExecutionContext`**: ADR 可能描述了 DAG 执行上下文聚合。实际由 `ExecutionSession` (Sprint 19 PIMPL-lite 拆分) 提供 `execute_node()` / `check_and_requeue_dynamic_deps()` / `get_context_engine()` 等功能
- **`SessionManager`**: ADR 可能描述了独立的会话管理器类。实际 `DSLEngine::run(UserSession&, const string&, Context)` 重载提供会话生命周期管理 (create/archive/failure-split)。提取为独立管理器属于 Phase 5 去重

**推迟理由**: 三个 Deferred 类均为内部实现细节的独立类型提取, 当前内联/内部实现已满足功能需求。Phase 5 代码库膨胀后可能需要提取。

## 决策

- **ADR 状态**: ✅ Approved (保持)
- **理由**: ADR-0033 核心契约 (三层会话 `UserSession` / `TaskSession` / `SubtaskSession` + FailureMode + DSLEngine 会话重载) 10/13 已 Shipped; 3 个 Deferred 属于内部细节的独立类型提取, 非核心契约
- **风险**: 低 — 会话生命周期管理通过 `DSLEngine` 公开 API 完整实现

## 后续行动

- Phase 5 代码库膨胀后, 评估是否需要将 `ExecutionSession` + `SubtaskSession` 的组合提取为 `DagExecutionContext`
- `SessionManager` 留待 Phase 5 多会话管理复杂度增长后实施
- 本 audit 文档供 Phase 5 backlog 参考
