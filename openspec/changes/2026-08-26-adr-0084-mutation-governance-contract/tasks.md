# Tasks: adr-0084-mutation-governance-contract

> **TDD 5 步结构**: 每任务按 Write failing test → Verify fail → Implement → Verify pass → Commit

## Phase 0: 契约类声明（估时 0.5 sprint）

- [ ] **T0.1** Write failing test: `tests/test_mutation_governance.cpp` 骨架（≥ 6 cases 占位，引入 IMutationGovernor + MutationContext 类型）
- [ ] **T0.2** Verify fail: 编译失败（`fatal error: 'agenticdsl/contract/imutation_governance.h' file not found`）
- [ ] **T0.3** Implement minimal: `include/agenticdsl/contract/imutation_governance.h`（`class IMutationGovernor` 抽象类 + 3 虚函数 `propose/commit/revert` + `MutationContext` 值类型）
- [ ] **T0.4** Implement minimal: `include/agenticdsl/types/mutation_record.h`（`struct MutationRecord` + 4 mutation.* 主题 payload）
- [ ] **T0.5** Verify pass: 编译成功，6 个 TEST_CASE 编译通过（运行时仍 FAIL）
- [ ] **T0.6** Commit: `feat(contract): IMutationGovernor + MutationRecord types (T0)`

## Phase 1: ADR-0068 amendment + 4 主题注册（估时 0.25 sprint）

- [ ] **T1.1** 修改 `docs/adr/adr-0068-event-emission-contract.md` 附录 A：注册 4 mutation.* 主题（per ADR-0084 §决策 4 §实施任务）
- [ ] **T1.2** 修改 `tools/adr_implementation_status_gap_analysis.md` §四：14 → 18 主题
- [ ] **T1.3** Verify: `grep "mutation.{proposed,committed,reverted,denied}" docs/adr/adr-0068-event-emission-contract.md` 命中 4 行
- [ ] **T1.4** Commit: `docs(adr-0068): register 4 mutation.* topics (per ADR-0084 §决策 4)`

## Phase 2: V1 MutationGovernor L1-L3 实现（估时 1 sprint）

- [ ] **T2.1** Write failing test: `test_mutation_governance.cpp::l1_prompt_mutation_happy_path`（yolo 模式 + L1 prompt + IEvaluator=Excellent → mutation.committed emit）
- [ ] **T2.2** Write failing test: `test_mutation_governance.cpp::l2_dsl_mutation_plan_only`（yolo + L2 → mutation.denied 含 denial_reason="plan_required"）
- [ ] **T2.3** Write failing test: `test_mutation_governance.cpp::l3_skill_mutation_human_review_required`（agent + L3 → mutation.denied 含 denial_reason="human_review_required"）
- [ ] **T2.4** Write failing test: `test_mutation_governance.cpp::l4_weight_mutation_explicitly_rejected`（任意模式 + L4 → 抛出明确异常，mutation.denied emit）
- [ ] **T2.5** Write failing test: `test_mutation_governance.cpp::audit_events_complete`（propose → commit 触发 4 主题全部发射）
- [ ] **T2.6** Write failing test: `test_mutation_governance.cpp::fail_closed_on_unknown_source`（mutation_context.whitelisted_source_id="" → mutation.denied, default fail-closed）
- [ ] **T2.7** Verify fail: 6 cases 全部 FAIL
- [ ] **T2.8** Implement: `src/common/governance/mutation_governor.cpp`（4 模式 × 4 等级矩阵 + L4 拒绝 + 4 mutation.* emit + IEvaluator 集成）
- [ ] **T2.9** Implement: `src/common/governance/mutation_topics.cpp`（4 主题注册到 EventBuilder）
- [ ] **T2.10** Implement: `src/common/governance/CMakeLists.txt` 注册新源文件
- [ ] **T2.11** Verify pass: 6 cases PASS, ctest 185/185 零回归
- [ ] **T2.12** Commit: `feat(governance): MutationGovernor V1 L1-L3 (T2)`

## Phase 3: 文档同步 + ship + 评审（估时 0.5 sprint）

- [ ] **T3.1** 修改 `docs/adr/adr-0084-mutation-governance-contract.md` 头部 `## 状态` 章节: `🔍 Proposed` → `✅ Approved (ship 2026-XX-XX)`
-  [ ] **T3.2** 修改 `docs/architecture/capability-application-map-2026-08.md` §二 G11 行: `🔍 Proposed` → `✅ Closed` + §八.3 R 轨 T19/T20/T22 任务前置条件移除"待 G11 ADR Approved"
- [ ] **T3.3** 修改 `docs/architecture/self-evolution-architecture-2026-08.md` §五 变异治理行 + §七"需要继续形成的架构决议"项 #1 (ADR-0084) 标记完成
- [ ] **T3.4** 修改 `docs/active-status.md` §一 OpenSpec active 计数 + G11 跟踪段更新（移除"issue #14 保持 OPEN"声明，标记为 G11 Closed）
- [ ] **T3.5** GitHub issue #14 关闭（body 内留 audit trail）
- [ ] **T3.6** `python3 tools/adr_lint.py` + `python3 tools/docs_drift_audit.py` 全部通过
- [ ] **T3.7** ctest 全量 185/185 PASS 零回归
- [ ] **T3.8** `git commit -m "feat(governance): ship IMutationGovernor + 4 mutation.* topics (closes G11)"`
- [ ] **T3.9** `openspec archive 2026-08-26-adr-0084-mutation-governance-contract`

## 总估时

- Phase 0: 0.5 sprint
- Phase 1: 0.25 sprint
- Phase 2: 1 sprint
- Phase 3: 0.5 sprint
- **总估时: ~2.25 sprint**

## 启动条件

- T17 SkillCompiler ship（依赖 TaskCompiler L3 变异对象）
- OpenSpec `2026-08-26-ship-ievaluator-reward-contract` ship（依赖 IEvaluator 评估门）