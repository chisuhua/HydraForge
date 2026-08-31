# ADR-0053 Agent Descriptor Interface 实施范围审计

> **生成时间**: 2026-08-31 (docs_drift_audit Scenario 4 修正)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0053-agent-descriptor-interface.md](adr-0053-agent-descriptor-interface.md)
> **状态**: ✅ Approved (Phase 6a direction ADR, 实施顺位 later)

## 状态

**📋 Audit** (impl-scope-audit 文档)

✅ Approved (Phase 6a 评审 2026-07-16 通过, 12 个 Phase 6 ADR 之一, 全部方向性 ADR, 实施分 Wave 派生子 Change; 当前 0/0 tasks 子 Change 立项)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved (Phase 6 评审), 但 4/4 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `AgentDescriptor` | 📅 Phase 6a Wave 2 待实施 | — | ADR-0082 (IAgentRegistry) 已提供最小骨架, AgentDescriptor 是 Phase 6a Wave 2 增量 |
| `AgentForm` | 📅 Phase 6a Wave 2 待实施 | — | 同上 |
| `CapabilityRegistry` | 📅 Phase 6a Wave 2 待实施 | — | ADR-0054 同 |
| `Hybrid` (Hybrid descriptor mode) | 📅 Phase 6a Wave 2 待实施 | — | 描述 hybrid local+remote agent 模式 |

## 分类详情

### 📅 Deferred — Phase 6a Wave 2

ADR-0053 是 Phase 6a (Agent Manifest / Descriptor) 方向 ADR, 2026-07-15 架构评审通过 (OpenSpec change `from-roadmap-phase-6c-execution-baseline` 立项基础)。当前 0 个子 Change 已立项实施, 子 Change 预计 Phase 6a Wave 2 启动 (依赖 execution-baseline handoff)。

参考: ADR-0052 ~ ADR-0065 系列 14 个 ADR 全部为 Phase 6 方向性, 由 ADR-0071 (LLM-native AgenticDSL) 锚定。当前 cap-map §一 #28-#30 已 ship (#27 GEPALoop, #28 Prompt Evidence Gate, #29 MCTSWorkflowSearch), #31 Distillation Data Plane 2026-08-29 ship。
