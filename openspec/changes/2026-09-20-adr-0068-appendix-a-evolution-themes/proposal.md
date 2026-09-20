# Proposal: ADR-0068 Appendix A 主题注册 — evolution.themes (Sprint 34+ follow-up)

> **STATUS**: DRAFT (follow-up registered per Oracle NEEDS_FIX verdict 2026-09-20)
> **Type**: follow-up to ADR-0088 v1.0 ship (commit `0ffc637` + `7a15744`)
> **优先级**: P0 (C4 harness-rsi-pilot readiness gate 真实事件发射前置; 当前 D8 字符串常量无法发射,EventBuilder 强制注册 REJECT)
> **估时**: 0.5-1 天 (纯主题注册,无业务逻辑)

## Why (背景)

ADR-0088 v1.0 ship 包含 D8 决策: 发射 `evolution.transition.denied` + `evolution.readiness.denied` 事件 (修正 Metis Q1 DEAL-BREAKER `evolution.scheduler.denied` 幻影主题)。

**但** Oracle 审查发现:
- transition_guard.cpp:18-19 仅 ship 了字符串常量 `kEventTransitionDenied = "evolution.transition.denied"`, 无实际发射路径
- ADR-0068 Appendix A **未注册** 2 个新主题 — 按 EventBuilder 强制主题注册契约, 真发射会被 REJECT
- 幻影主题风险: 事件主题常量存在但 ADR-0068 不认, 实际发射 = silent failure

**没有这个 ship 的后果**:
- C4 harness-rsi-pilot 的 `evaluate_readiness()` 三条件门控任意 fail 时, 想发射 `evolution.readiness.denied` 事件但 EventBuilder 拒绝 (未知主题)
- D8 决策在 ADR-0088 治理上"已 ship"但实际是死代码
- 与 ADR-0068 v1.2 Appendix A 治理脱钩

## What Changes

### 模块边界

**修改**:
1. `docs/adr/adr-0068-event-emission-contract.md` (~30 行增): v1.9 amendment 注册 evolution.transition.denied + evolution.readiness.denied 主题, 含 payload schema
2. (可选) `src/common/contract/event_builder.cpp`: 主题白名单检查更新 (如已存在)

**复用** (per ADR-0088 D8):
- evolution.transition.denied payload schema (per transition_guard.cpp:18 + spec §evolution-transition-denied-event)
- evolution.readiness.denied payload schema (per spec §evolution-readiness-denied-event)

## Acceptance (验收标准)

- [ ] **AC-1**: ADR-0068 Appendix A 注册 evolution.transition.denied 主题 (含 payload 字段: from / to / reason / current_genome / last_harness_change)
- [ ] **AC-2**: ADR-0068 Appendix A 注册 evolution.readiness.denied 主题 (含 payload 字段: failed_conditions / attribution_verdict / eval_quality / budget_state)
- [ ] **AC-3**: EventBuilder enforcement 不再 REJECT 2 个新主题发射 (test_event_emission_contract 验证)
- [ ] **AC-4**: ADR-0088 D8 治理声明: "字符串常量 + 主题注册 + 发射路径" 全部 ship
- [ ] **AC-5**: ctest test_event_emission_contract 零回归 (现有主题发射不受影响)
- [ ] **AC-6**: ctest 全量 248+ tests 零回归

## Capabilities (MUST / MUST NOT)

### MUST

- **MUST** 复用 ADR-0088 D8 主题名 (evolution.transition.denied + evolution.readiness.denied)
- **MUST** payload schema 与 openspec/specs/transition-guard/spec.md §evolution-{transition,readiness}-denied-event Requirement 保持一致
- **MUST** ADR-0068 v1.9 amendment 同步 docs/adr/adr-0068-event-emission-contract.md 头部状态字段

### MUST NOT

- **MUST NOT** 引入 3 个以上新主题 (ADR-0088 governance 已约束 ≤2 个新主题)
- **MUST NOT** 修改 ADR-0068 既有主题注册 (向后兼容)
- **MUST NOT** 修改 EventBuilder 强制注册检查逻辑 (本 change 仅注册主题, 不改 infrastructure)

## Impact (影响范围)

| 文件 | 变更类型 | 行数估计 |
|------|---------|---------|
| `docs/adr/adr-0068-event-emission-contract.md` | 修改 | +30 (v1.9 amendment) |
| (可选) `src/common/contract/event_builder.cpp` | 修改 | +5 (如有强制注册检查) |

**总估计**: +35 行 / -0 行

## 关联 ADR

- **ADR-0068** (✅ Approved v1.2.1) — 本 change 是 v1.9 amendment
- **ADR-0088** (✅ Approved, ship 2026-09-20) — D8 决策本 change 实装

## 阻塞关系

- **前置**: `2026-09-20-ig-genome-registry-walk-ancestors` (D5/D6) — judge_data_freshness 完整实装 + walk_ancestors override 才能触发条件 1 fail 路径发射 readiness.denied
- **下游**: C4 harness-rsi-pilot — 真实事件发射 + 主题注册后才能验证 readiness gate 拒绝路径