# Tasks: adr-0084-mutation-governance-contract

> **TDD 5 步结构**: 每任务按 Write failing test → Verify fail → Implement → Verify pass → Commit
>
> **范围前提**: V1 = gate-and-audit contract only。无 subject 版本存储 / 无 subject 恢复 / 无 24h 保留窗口 / 无可操作 revert recovery。
> **启动顺序**: OpenSpec `2026-08-26-ship-ievaluator-reward-contract`（IEvaluator）**必须先 ship**，本 change 才能启动实施（构造时强制注入非空 IEvaluator）。
> **设计文档**: 见本目录 `design.md`（D1-D8 决策与门禁链顺序定义）。

## Phase 0: 契约类声明（估时 0.5 sprint）

- [x] **T0.1** Write failing test: `tests/test_mutation_governance.cpp` 骨架（≥ 8 cases 占位，引入 IMutationGovernor + MutationContext 类型；断言全部面向可客观验证的 topic/payload/顺序/异常类型）
- [x] **T0.2** Verify fail: 编译失败（`fatal error: 'agenticdsl/contract/imutation_governance.h' file not found`）
- [x] **T0.3** Implement minimal: `include/agenticdsl/contract/imutation_governance.h`（`class IMutationGovernor` 抽象类 + 3 虚函数 `propose/commit/revert` + `MutationContext` 值类型含 `source_id` / `mutation_kind` / `subject_ref` / `evaluation_refs`（不透明 evaluation_id 字符串数组，来自 IEvaluator 契约层））
- [x] **T0.4** Implement minimal: `include/agenticdsl/types/mutation_record.h`（`struct MutationRecord` + 4 mutation.* 主题 payload 值类型；所有标识字段为不透明字符串，无 subject 内容/版本存储）
- [x] **T0.5** Verify pass: 编译成功，≥ 8 个 TEST_CASE 编译通过（运行时仍 FAIL）
- [x] **T0.6** Commit: `feat(contract): IMutationGovernor + MutationRecord types (T0)`

## Phase 1: ADR-0068 amendment 文档注册（估时 0.25 sprint）

> **注意**: EventBuilder 仅为 payload builder，无 `register_topic` / `TopicRegistry` API（全库 grep 0 命中）。主题为**文档注册 only**，无运行时代码注册，**不存在 `mutation_topics.cpp`**。

- [x] **T1.1** 修改 `docs/adr/adr-0068-event-emission-contract.md` 附录 A：登记 4 mutation.* 主题 payload schema（per ADR-0084 §决策 4 §实施任务）
- [x] **T1.2** 修改 `docs/architecture/adr-implementation-status-gap-analysis.md` §四：14 → 18 主题（文档计数同步）
- [x] **T1.3** Verify: `grep -c "mutation\.\(proposed\|committed\|reverted\|denied\)" docs/adr/adr-0068-event-emission-contract.md` ≥ 4
- [x] **T1.4** Commit: `docs(adr-0068): register 4 mutation.* topics in Appendix A (per ADR-0084 §决策 4)`

## Phase 2: V1 MutationGovernor gate-and-audit 实现（估时 1 sprint）

门禁链顺序（实现与测试必须一致，见 design.md D6）:
白名单 fail-closed → L4 emit-then-throw → 模式×等级矩阵（plan+L3=`plan_insufficient`）→ agent+L3 IApprovalHandler 人类复核 → IEvaluator 评估门 → 行为回归门 → commit（evaluation_refs 非空校验）

- [x] **T2.1** Write failing test: `l1_prompt_mutation_happy_path`（yolo + L1 + IEvaluator=Excellent + 回归 Pass → 顺序 emit mutation.proposed → mutation.committed，committed payload 原样透传 evaluation_refs）
- [x] **T2.2** Write failing test: `l2_dsl_mutation_plan_only`（yolo + L2 → 返回拒绝，emit mutation.denied 含 denial_reason="plan_required" + failed_step="authorization"，不 emit proposed）
- [x] **T2.3** Write failing test: `l3_skill_mutation_plan_insufficient`（plan + L3 → emit denied 含 denial_reason="plan_insufficient"，不调用 IApprovalHandler）
- [x] **T2.4** Write failing test: `l3_skill_mutation_agent_requires_approval`（agent + L3 → IApprovalHandler::process_request 恰好调用一次；true → 继续 emit proposed；false → emit denied 含 denial_reason="approval_denied" + failed_step="human_review"）
- [x] **T2.5** Write failing test: `l3_skill_mutation_agent_handler_missing_fail_closed`（agent + L3 + handler=nullptr → emit denied 含 denial_reason="approval_handler_unavailable"）
- [x] **T2.6** Write failing test: `l4_weight_mutation_emit_then_throw`（任意模式 + L4 → 先捕获到 mutation.denied 含 denial_reason="l4_forbidden_v1"，随后 REQUIRE_THROWS_AS std::runtime_error，验证事件严格先于异常）
- [x] **T2.7** Write failing test: `audit_event_ordering_and_evaluation_refs`（propose→commit 全序事件断言 + committed 含非空 evaluation_refs + 同一 attempt 无第 3 个 mutation.* 事件）
- [x] **T2.8** Write failing test: `fail_closed_on_unknown_source`（source_id="" 或未注册 → emit denied 含 denial_reason="non_whitelisted_source" + failed_step="source_whitelist"，且不执行后续任何门禁；默认空白名单 = 全部拒绝）
- [x] **T2.9** Write failing test: `commit_missing_evaluation_refs_fail_closed`（propose 通过但 evaluation_refs 为空 → commit emit denied 含 denial_reason="missing_evaluation_refs"）
- [x] **T2.10** Write failing test: `constructor_null_evaluator_throws`（shared_ptr\<IEvaluator\>(nullptr) 构造 → REQUIRE_THROWS_AS std::invalid_argument）
- [x] **T2.11** Write failing test: `revert_is_audit_only`（committed 后调用 revert(target_version, rollback_reason) → emit mutation.reverted 含两字段；governor 无任何 subject 状态可恢复——以接口无可读回状态断言）
- [x] **T2.12** Verify fail: ≥ 10 cases 全部 FAIL
- [x] **T2.13** Implement: `src/common/governance/mutation_governor.cpp`（构造注入非空 IEvaluator + 不可变白名单 + 可空 IApprovalHandler*；按 design.md D6 门禁链顺序实现；4 mutation.* 经 EventBuilder emit）
- [x] **T2.14** Implement: `src/common/governance/CMakeLists.txt` 注册 `mutation_governor.cpp`（**无 mutation_topics.cpp**）
- [x] **T2.15** Verify pass: 全部 cases PASS；ctest 全量零回归（基线 = 本 change 启动时 main 实测 ctest 计数，禁止硬编码数字）
- [x] **T2.16** Commit: `feat(governance): MutationGovernor V1 gate-and-audit L1-L3 (T2)`

## Phase 3: 文档同步 + ship 验证（估时 0.5 sprint）

> **范围外**（本 Phase 明确不做，留待评审 change）：ADR-0084 状态翻转 🔍→✅、cap-map §二 G11 翻 ✅、GitHub issue #14 关闭、active-status.md / self-evolution-architecture-2026-08.md 更新、T17 producer 接线。

- [x] **T3.1** 修改 `docs/adr/adr-0084-mutation-governance-contract.md` `## 状态` 章节：追加 "V1 代码 ship 完成 (2026-XX-XX)" 注记（**保持 🔍 Proposed**，评审翻转为后续步骤）
- [x] **T3.2** `python3 tools/adr_lint.py` + `python3 tools/docs_drift_audit.py` 全部通过
- [x] **T3.3** `openspec validate 2026-08-26-adr-0084-mutation-governance-contract --strict` 通过
- [x] **T3.4** ctest 全量零回归（基线 = 启动时 main 实测计数 + 本 change 净增测试数）
- [x] **T3.5** `git commit -m "feat(governance): ship IMutationGovernor V1 gate-and-audit contract"`
- [x] **T3.6** `openspec archive 2026-08-26-adr-0084-mutation-governance-contract`

## 总估时

- Phase 0: 0.5 sprint
- Phase 1: 0.25 sprint
- Phase 2: 1 sprint
- Phase 3: 0.5 sprint
- **总估时: ~2.25 sprint**

## 启动条件

- ✅ OpenSpec `2026-08-26-ship-ievaluator-reward-contract` ship（**硬前置**：IEvaluator 评估门 + 构造注入）
- ~~T17 SkillCompiler ship~~ **已降级**：非启动条件。T17 为后续 L3 producer 集成依赖（独立 change 接线）；本契约 L3 用例使用合成 MutationContext
