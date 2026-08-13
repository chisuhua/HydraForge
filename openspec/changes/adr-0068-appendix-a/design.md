# design.md — adr-0068-appendix-a

## Why

ADR-0068 Appendix A 中 lines 196-209 有 14 个主题标记为 `📡`（已发射但无注册订阅方）。这些主题已在代码中发射，缺少的是正式注册为"已注册主题"的状态更新。

Wave 1 (2026-08-03) 完成了 7 个幻影主题的真实发射 + EventBuilder V2 扩展，ADR-0068 已晋升 ✅ Approved。W6 要求将这 14 个 `📡` 主题正式注册。

## What Changes

| 产物 | 动作 |
|------|------|
| `docs/adr/adr-0068-event-emission-contract.md` | **修改** — Appendix A lines 196-209，14 个 `📡` → `✅ registered`，附 evidence 引用（source file:line） |

**Out of Scope**（禁止变更）：
- 任何 C++ 代码
- 现有 `✅` 或 `👻` 行
- EventBuilder 行为或 topic payload
- 其他 ADR 文档

## Impact

- **零代码变更**：纯文档状态同步。
- **零测试变更**：ctest 147/147 保持不变。

## 14 个 📡 主题及预期 Evidence

| 主题 | Owner | Evidence 来源 |
|------|-------|-------------|
| `tool.audit.invoked` | ToolCoordinator | `src/common/tools/tool_coordinator.cpp:220` (EventBuilder emit) |
| `tool.audit.completed` | ToolCoordinator | `src/common/tools/tool_coordinator.cpp` (via EventBuilder) |
| `tool.audit.denied` | ToolCoordinator | `src/common/tools/tool_coordinator.cpp:245` (EventBuilder emit) |
| `tool.coordinator.cycle_detected` | ToolCoordinator | `src/common/tools/tool_coordinator.cpp` (cycle_detected emit) |
| `policy.approval.requested` | ApprovalHandler | `src/common/policy/approval_handler.cpp` (emit 调用) |
| `compliance.log` | ComplianceDecorator | `src/common/llm/compliance_decorator.cpp` (emit 调用) |
| `cognitive.task.started` | CognitiveWorker | `src/modules/cognitive/cognitive_worker.cpp` (emit 调用) |
| `cognitive.task.completed` | CognitiveWorker | `src/modules/cognitive/cognitive_worker.cpp:198` (EventBuilder emit) |
| `domain.task.started` | DomainWorkerPool | `src/modules/cognitive/domain_worker_pool.cpp` (emit 调用) |
| `domain.task.completed` | DomainWorkerPool | `src/modules/cognitive/domain_worker_pool.cpp:226` (EventBuilder emit) |
| `domain.task.failed` | DomainWorkerPool | `src/modules/cognitive/domain_worker_pool.cpp:262` (EventBuilder emit) |
| `dsl.call.started` | NodeExecutor | `src/modules/executor/node_executor.cpp` (emit 调用) |
| `dsl.call.completed` | NodeExecutor | `src/modules/executor/node_executor.cpp` (emit 调用) |
| `execution.failed` | NodeExecutor | `src/modules/executor/node_executor.cpp:411` (EventBuilder emit) |

## Acceptance

- [ ] `tools/adr_lint.py` exit 0
- [ ] `tools/docs_drift_audit.py` 0 new DRIFT
- [ ] `openspec validate --strict` exit 0
- [ ] `ctest` 输出不变（147/147）
