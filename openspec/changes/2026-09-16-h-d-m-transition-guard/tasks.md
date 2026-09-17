# Tasks: H→D→M Transition Guard

> **STATUS: PLACEHOLDER** — depends on C2 ship

## 1. Pre-flight
- [ ] TBD: 状态机范围决策 (4 状态 vs Ready/NotReady/Blocked)
- [ ] TBD: 与 ADR-0086 集成方式 (可选依赖 vs 强制)
- [ ] TBD: 编译期+运行期双重断言机制 (constexpr can_transition?)

## 2. Tests (RED) - 12 case
- [ ] TBD: can_transition(H, D) → true
- [ ] TBD: can_transition(H, M) → false (核心规则)
- [ ] TBD: can_transition(D, M) → true
- [ ] TBD: can_transition(D, H) → true (允许 re-Harness)
- [ ] TBD: can_transition(M, D) → true (允许再 D)
- [ ] TBD: can_transition(M, H) → true (允许 re-Harness)
- [ ] TBD: can_transition(current, current) → true (no-op)
- [ ] TBD: can_transition(InvalidState, X) → false
- [ ] TBD: evaluate_readiness 4 条件全满足 → Ready
- [ ] TBD: evaluate_readiness 任一条件不满足 → NotReady(reason)
- [ ] TBD: evaluate_readiness 硬门禁触发 → Blocked
- [ ] TBD: 编译期 constexpr can_transition 验证 (sanity check)

## 3. Implementation (GREEN)
- [ ] TBD: include/agenticdsl/evolution/transition_guard.h (~200 行)
- [ ] TBD: include/agenticdsl/evolution/state.h (4 状态 enum)
- [ ] TBD: include/agenticdsl/evolution/verdict.h (EvolutionVerdict struct)
- [ ] TBD: src/modules/evolution/transition_guard.cpp
- [ ] TBD: 集成点: ADR-0083 IEvaluator::evaluate() 输入
- [ ] TBD: 集成点: ADR-0084 MutationGovernance::authorize() 门禁
- [ ] TBD: 集成点: ADR-0086 CreditAssignment::attribute() (可选)

## 4. Event emission
- [ ] TBD: 守卫失败时 emit `evolution.scheduler.denied` 事件
- [ ] TBD: 使用 EventBuilder (per ADR-0068 规范)

## 5. Verification
- [ ] TBD: 12 个 test case 全部 PASS
- [ ] TBD: 编译期 constexpr 验证通过
- [ ] TBD: ctest 零回归

## 6. Ship gate
- [ ] TBD: ctest 零回归
- [ ] TBD: adr_lint 0 errors
- [ ] TBD: docs_drift_audit 0 DRIFT
- [ ] TBD: openspec validate
- [ ] TBD: dual-agent review (Metis + Oracle)

## 7. Archive
- [ ] TBD: openspec archive
- [ ] TBD: 更新 master plan §四 C3 状态

## 8. Out-of-scope
- [ ] TBD: 不新建 3 算子接口
- [ ] TBD: 不实现 Model-RSI 实际执行
- [ ] TBD: 不实现 IModelRSI

## 9. References
- [ ] TBD: MetaRSI-v1 论文 H→D→M 规则 (unverified)
- [ ] TBD: ADR-0083/0084/0086 契约栈
- [ ] TBD: self-evolution-architecture §三
- [ ] TBD: Oracle session `ses_f55f307f6ffeRJ9SIny8iUbZ8Y` §4 Change 3
