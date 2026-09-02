# Control Plane 启动条件评估决议文档 (v1)

> **Date**: 2026-09-02
> **ADR**: [ADR-0076 — DSL Engine as MCP Server 控制面](../adr/adr-0076-dsl-engine-mcp-server.md) §启动条件 + ADR-0079 统一会话模型
> **评估脚本**: [`scripts/control-plane-eval.py`](../../scripts/control-plane-eval.py)
> **Author**: Solo Dev (Single-Developer Mode)
> **Status**: ⏳ **DescopeOrContinue** — Phase 7 启动条件 3/6 未满足，继续前置 ship

---

## §6 项条件状态 (含 file:line 引用)

| 条件 | 状态 | 证据 (file:line) | 说明 |
|------|:----:|------------------|------|
| C1 AgentForge ≥2 agent | ❌ FAIL | [proposal.md:7](../../openspec/changes/from-roadmap-phase-6c-control-plane-eval/proposal.md) | 当前 1 个完整 AgentForge agent（pdk_chat_demo / g1 二选一计） |
| C2 Solo Dev 容量 ≥2人/≥80h双周 | ❌ FAIL | [adr-implementation-status-gap-analysis.md:349](../architecture/adr-implementation-status-gap-analysis.md) | 1 人, 37h/44h < 80h/双周 |
| C3 ADR-0068 附录 A amendment ship | ✅ PASS | [adr-0068-event-emission-contract.md:5](../adr/adr-0068-event-emission-contract.md) | 2026-08-13 archived |
| C4 ADR-0073 完整 ship D2+D3 | ✅ PASS | [archive/2026-08-18-from-roadmap-phase-6c-schema-complete/](../../openspec/changes/archive/2026-08-18-from-roadmap-phase-6c-schema-complete/) | ToolCoordinator 4 步校验层 ship (C9, 2026-08-18) |
| C5 Evidence Gate PASS | ❌ FAIL | [evidence-gate-v1.md:14](2026-09-02-evidence-gate-v1.md) | 决议 = Conditional（mock baseline 88.24% 非真实 PASS） |
| C6 ADR-0075 EnvBackend ship | ✅ PASS | [env_backend.h](../../include/agenticdsl/env/env_backend.h) + [local_backend.cpp](../../src/common/env/local_backend.cpp) + [docker_backend.cpp](../../src/common/env/docker_backend.cpp) | Local+Docker 已 ship 2026-08-18 |

> **运行命令**: `python3 scripts/control-plane-eval.py --dry-run` → exit 1（含 C1/C2/C5 FAIL 的决策表）

---

## §决策表 (Decision Table)

**决策**: **DescopeOrContinue** — 阻塞条件 C1, C2, C5 FAIL，建议继续前置 ship 后再启动 Phase 7

| 分支 | 条件 | 结论 |
|------|------|------|
| 全 PASS | 6 项全 ✅ | `RecommendStart` — 建议立即启动 Phase 7a |
| **阻塞 FAIL** | **C1/C2/C5/C6 任一 FAIL** | **`DescopeOrContinue`** ← 当前 3 FAIL (C1/C2/C5) |
| 条件 3 ✅ + 其他 🟡 | C3 PASS + 其余非阻塞未全 PASS | `Conditional` — ship 剩余前置再决议 |
| 数据缺失 | 任一条件无法判定 | `Abort` — 需人工 --override |

---

## §后续路径 (Subsequent Paths)

| 路径 | 说明 | 估时 |
|------|------|------|
| A: 继续前置 ship | 补足 FAIL 条件: C5 需真实 3 模型 baseline PASS（调度 Sprint 25+）；C1 需 ≥2 个完整 AgentForge agent；C2 需 Solo Dev 容量提升 | Sprint 25+ |
| B: descope | 拆分 Phase 7a（仅 stdio MCP）/ 7b / 7c 降级启动 | 0.5 sprint |
| C: 暂缓 | 维持 ⏸ 顺延状态，Phase 6c 收官（Sprint 28 末）后再决议 | — |

---

## §决议 (Decision)

**single-choice**: **继续前置 ship（路径 A）** — Phase 7 维持 ⏸ 顺延，等待 C1/C2/C5 满足后重跑评估脚本再决议。

> 本决议基于 `scripts/control-plane-eval.py` 自动检测输出（exit 1），经 human review 签字确认（per design D-5 human review only）。下次重评估触发点: Evidence Gate 真实 PASS（Sprint 25+ 真实 baseline 决议后）。