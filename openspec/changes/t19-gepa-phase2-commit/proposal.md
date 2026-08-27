# t19-gepa-phase2-commit

## Why

T19 GEPA MVP Phase 1 (只读反思约束) 已通过 G11 闭锁限制（"不执行 commit(PromptEdit) 直至 G11 ADR Approved"）。**所有前置已 ship 2026-08-26/27**：
- ✅ G11 ✅ Closed（ADR-0084 Approved + V1 gate-and-audit ship + issue #14 Closed）
- ✅ IEvaluator V1 ship（12 cases / 31 assertions）
- ✅ IEvaluator V2 ship（BehavioralEquivalence + Composite）
- ✅ T17 SkillCompiler ship（15 cases / 61 assertions，L3 变异对象生成器）
- ✅ T15 Trajectory IR ship（9 cases / 55 assertions，独立序列化视图）
- ✅ T14 行为回归 ship（BehavioralRegressionGate + Hotelling T²）
- ✅ ADR-0071 ✅ Approved (方向 ADR)

**Phase 1 → Phase 2 转换条件**（per cap-map §八 T19 + ADR-0071 §决策 + Oracle 会话决议）：
- 仅当 G11 ✅ Closed 时才解锁 commit 路径
- 当前 G11 已 ✅ Closed（commit `314561e` 关闭 issue #14）
- Phase 1 只读反思约束解除，Phase 2 commit 启动

**审计依据**：
- cap-map §三 B7 行明确："T19 Phase 2 commit 已解锁 — G11 ✅ Closed 2026-08-26"
- cap-map §八 T19 行："Phase 2 commit 已解锁 (G11 ✅ Closed 2026-08-26)"
- ADR-0071 (LLM-native AgenticDSL 架构) ✅ Approved
- ADR-0083 (IEvaluator/RewardSignal) ✅ Approved + V1/V2 ship
- ADR-0084 (Mutation Governance) ✅ Approved + V1 ship + G11 ✅ Closed
- ADR-0061-02 (BehavioralRegression) ✅ Approved + T14 ship

**T19 范围定位**（per ADR-0071 §D5 + cap-map §八）：
- Phase 2 commit：**实际执行 commit(PromptEdit)** 通过 MutationGovernor 授权门
- 失败→反思→修订 prompt 循环
- 行为回归门禁（修订前后等价性验证）
- 评估信号驱动（IEvaluator V2 BehavioralEquivalence 比较）
- V1 简化：单 agent 反思循环（V2 多 agent 协同 deferred）

**前置依赖**（全部已满足）：
- ✅ G11 变异治理 ship（commit `a2b2d52` + `314561e`）
- ✅ IEvaluator V1+V2 ship（commit `21dd622` + `314561e`）
- ✅ T17 SkillCompiler ship（commit `21dd622`，L3 变异对象生成）
- ✅ T15 Trajectory IR ship（commit `9c7c6da`，失败轨迹序列化）
- ✅ T14 行为回归 ship（commit `9c7c6da` 之前）

## What Changes

**新增 GEPA 反思循环引擎** (`src/modules/cognitive/gepa_loop.cpp` + `gepa_loop.h`):

1. **失败检测**: 通过 `BehavioralEquivalenceEvaluator` 检测任务执行失败（compare V1 vs baseline reward < threshold）
2. **轨迹序列化**: 使用 T15 TrajectoryIR::from_parsed_graph() + to_sft_data() 捕获失败轨迹
3. **反思生成**: 调用 LLM（通过 ILLMProvider）生成 prompt 修订候选
4. **修订编译**: 使用 T17 SkillCompiler 生成新 skill 候选（含 trajectory_ir_hash）
5. **回归验证**: 通过 T14 BehavioralRegressionGate::compute_fingerprint + hotelling_t2_test 验证修订未引入退化
6. **评估门禁**: 使用 IEvaluator V2 CompositeEvaluator（V1 TaskSuccess + V2 BehavioralEquivalence 加权聚合）
7. **变异授权**: 调用 MutationGovernor::propose() → L1/L2 门禁 → commit()（**实际执行 commit**）
8. **审计事件**: emit `gepa.reflection.{started,completed,failed}` + `gepa.commit.{proposed,committed,denied}` 6 个新主题

**新增事件主题**（per ADR-0068 amendment）：
- `gepa.reflection.started` (CognitiveWorker/DomainWorkerPool 触发 GEPA)
- `gepa.reflection.completed` (成功生成修订候选)
- `gepa.reflection.failed` (反思失败，无可用修订)
- `gepa.commit.proposed` (变异提议通过门禁)
- `gepa.commit.committed` (变异提交成功)
- `gepa.commit.denied` (变异被拒绝)

**新增测试** (`tests/test_gepa_phase2.cpp`):
- ≥ 8 cases 覆盖完整 GEPA 循环
- Mock ILLMProvider（修订候选生成）
- Mock T14 BehavioralRegressionGate（指纹计算）
- 真实 IEvaluator V2 注入（CompositeEvaluator）
- 真实 MutationGovernor 注入（commit 授权门）
- 失败→反思→修订→回归→commit 完整 E2E

**集成**：
- CognitiveWorker::set_evaluator + MutationGovernor 注入（已 ship，零修改）
- ILLMProvider 注入（DSLEngine 现有接口，零修改）
- EventBuilder 使用（ADR-0068 已 ship，零修改）

## Impact

**影响范围**：
- CognitiveWorker / DomainWorkerPool **零修改**（依赖既有 setter 注入）
- DSLEngine **零修改**（依赖既有 ILLMProvider）
- MutationGovernor **零修改**（依赖既有 propose/commit API）
- IEvaluator / TrajectoryIR / SkillCompiler **零修改**（作为依赖使用）

**下游解锁**：
- B7 自进化基础应用 MVP（Agent 失败→反思→修订 prompt 自动化）
- ADR-0074 Prompt Evidence Gate 集成 GEPA 评估
- T20 AFlow MCTS 工作流搜索（前置为 GEPA 单 agent MVP）
- T22 Fine-tune 事件驱动训练路径（AgenticMind 回流触发 GEPA）

**V1 边界**（per ADR-0071 §D5 + cap-map §八 T19）：
- ✅ 单 agent 反思循环（V1 简化）
- ✅ 同步 commit 路径（V1 简化，V2 异步 deferred）
- ⏸ 多 agent 协同反思（V2 deferred）
- ⏸ 在线权重微调（V2 deferred，需 L4 权重支持）
- ⏸ 跨 session 经验积累（V2 deferred，需 SessionManager V2 集成）

**Breaking Changes**：无（仅新增模块与事件主题，不修改既有契约）

## ship gate 验证

- `python3 tools/adr_lint.py` 通过（≥82 ADR）
- `python3 tools/docs_drift_audit.py` 通过（无新增 CRITICAL drift）
- `openspec validate --changes --strict` PASS
- `ctest --output-on-failure` 全量 0 回归（动态基线，188→189/190 净增）
- `ctest -R test_gepa_phase2` ≥ 8 cases / ≥ 20 assertions PASS
- `grep "class GEPALoop" include/agenticdsl/cognitive/` 命中
- ADR-0068 附录 A 新增 6 个 `gepa.*` 主题
- cap-map §三 B7 更新 ✅ Completed
- cap-map §一 +1（新能力 #27 GEPA MVP）
- T19 Phase 1 只读反思约束解除（cap-map §八 + active-status §一）

## 关联文档

- `docs/adr/adr-0071-llm-native-agenticdsl-architecture.md` ✅ Approved
- `docs/adr/adr-0083-evaluator-reward-contract.md` ✅ Approved + V1+V2 ship
- `docs/adr/adr-0084-mutation-governance-contract.md` ✅ Approved + V1 ship + G11 ✅ Closed
- `docs/adr/adr-0061-02-behavioral-regression.md` ✅ Approved + T14 ship
- `docs/adr/adr-0068-event-emission-contract.md` ✅ Approved（需新增 6 个主题）
- `include/agenticdsl/contract/ievaluator.h`（V2 ship）
- `include/agenticdsl/contract/imutation_governance.h`（G11 ship）
- `include/agenticdsl/ir/trajectory_ir.h`（T15 ship）
- `include/agenticdsl/cognitive/skill_compiler.h`（T17 ship）
- `include/agenticdsl/testing/behavioral_regression.h`（T14 ship）
- `docs/architecture/capability-application-map-2026-08.md` §三 B7 + §八 T19
- `docs/architecture/self-evolution-architecture-2026-08.md` §四
- `docs/active-status.md` §一 G11 + T19 跟踪段
- Oracle sessions: `ses_fc41537bbffeC35NKqgvzn4m1c` + `ses_fc3e070c0ffeIVgAhsgx2pNXFa` + `ses_fc3090b49ffe7yJwXhx1MoNz5N`