# Tasks: t19-gepa-phase2-commit

> **TDD 5 步结构**: 每任务按 Write failing test → Verify fail → Implement → Verify pass → Commit
> **关键不变量**: 既有契约**零修改**（IEvaluator / MutationGovernor / TrajectoryIR / SkillCompiler / BehaviorRegression / CognitiveWorker / DomainWorkerPool）。T19 GEPA 仅作为**编排层**调用现有 API。
> **Phase 2 关键**：实际执行 commit(PromptEdit) 通过 MutationGovernor 授权门（Phase 1 仅只读反思）。

## Phase 0: GEPA Loop 契约 (估时 0.4 sprint)

- [x] **T0.1** Write failing test: `tests/test_gepa_phase2.cpp` 骨架（≥ 8 cases 占位）
  - `gepa_loop_initialization`
  - `gepa_loop_failed_detection` (BehavioralEquivalence 检测失败)
  - `gepa_loop_trajectory_serialization` (T15 TrajectoryIR)
  - `gepa_loop_reflection_generation` (LLM 调用)
  - `gepa_loop_skill_compilation` (T17 SkillCompiler)
  - `gepa_loop_regression_validation` (T14 BehaviorRegression)
  - `gepa_loop_evaluation_gate` (IEvaluator V2 CompositeEvaluator)
  - `gepa_loop_commit_authorization` (MutationGovernor commit)
- [x] **T0.2** Verify fail: 编译失败（`fatal error: 'agenticdsl/cognitive/gepa_loop.h' file not found`）
- [x] **T0.3** Implement: `include/agenticdsl/cognitive/gepa_loop.h`:
  ```cpp
  namespace agenticdsl {
  class GEPALoop {
  public:
      struct Config {
          double reward_threshold = 0.0;        // 失败判定阈值
          double regression_alpha = 0.05;       // Hotelling T² alpha
          int max_iterations = 3;                // 最大反思迭代次数
          std::string source_id = "R_T19_GEPA"; // 白名单 source_id
      };
      struct ReflectionResult {
          bool success;
          std::string failure_mode;
          std::vector<std::string> candidate_skills;
      };
      GEPALoop(std::shared_ptr<IEvaluator> evaluator,
              std::shared_ptr<MutationGovernor> governor,
              std::shared_ptr<ILLMProvider> llm,
              Config config = {});
      ReflectionResult reflect_and_commit(const ExecutionTrace& failed_trace);
  private:
      // 内部调用 SkillCompiler + TrajectoryIR + BehaviorRegression + MutationGovernor
  };
  }
  ```
- [x] **T0.4** Verify pass: 8 cases 编译通过（运行时仍 FAIL，断言占位）
- [x] **T0.5** Commit: `feat(cognitive): GEPALoop contract skeleton (T0)`

## Phase 1: 反思循环核心 (估时 0.6 sprint)

- [ ] **T1.1** Write failing test: `reflection_loop_basic_flow`（失败 trace → 反思 → 修订候选 → 回归 Pass → commit success）
- [ ] **T1.2** Write failing test: `reflection_loop_no_improvement`（3 次迭代仍无改进 → return success=false）
- [ ] **T1.3** Verify fail: 2 cases FAIL（reflect_and_commit 未实现）
- [ ] **T1.4** Implement: `src/modules/cognitive/gepa_loop.cpp`
  - `reflect_and_commit(failed_trace)`:
    1. 调用 `MutationGovernor::propose(MutationContext{source_id, "L1_prompt", subject_ref, evaluation_refs})`
    2. 调用 `TrajectoryIR::from_parsed_graph(failed_trace)` 序列化失败轨迹
    3. 调用 `ILLMProvider::generate()` 注入失败轨迹，生成 prompt 修订候选
    4. 调用 `SkillCompiler::compile(candidate_md)` 生成新 skill
    5. 调用 `BehavioralRegressionGate::compute_fingerprint` + `hotelling_t2_test` 验证修订
    6. 调用 `IEvaluator::evaluate(new_trace)` 通过 CompositeEvaluator 加权评估
    7. 若新 reward > failed reward + threshold → 调用 `MutationGovernor::commit()` + emit `gepa.commit.committed`
    8. 否则 → emit `gepa.commit.denied` + 迭代下一轮
- [ ] **T1.5** Verify pass: 2 cases PASS + Phase 0 编译通过
- [ ] **T1.6** Commit: `feat(cognitive): GEPALoop reflection + commit core flow (T1)`

## Phase 2: 事件发射 + ADR-0068 集成 (估时 0.3 sprint)

- [ ] **T2.1** Write failing test: `gepa_event_emission`（成功 commit 后 emit 6 个事件）
- [ ] **T2.2** Verify fail: 事件未发射）
- [ ] **T2.3** Implement: 在 `gepa_loop.cpp` 中添加 EventBuilder emit 调用：
  - 反思开始 → emit `gepa.reflection.started`
  - 反思完成 → emit `gepa.reflection.completed` 或 `gepa.reflection.failed`
  - 变异提议 → emit `gepa.commit.proposed`
  - 变异提交 → emit `gepa.commit.committed` 或 `gepa.commit.denied`
- [ ] **T2.4** Modify: `docs/adr/adr-0068-event-emission-contract.md` 附录 A v1.2.2 → v1.3：
  - 注册 6 个 `gepa.*` 主题（payload schema: reflection_id / candidate_skill / regression_verdict / commit_id / evaluation_refs）
- [ ] **T2.5** Verify pass: 1 case PASS + T1 + T0 全过
- [ ] **T2.6** Commit: `feat(cognitive): GEPA event emission + ADR-0068 v1.3 (T2)`

## Phase 3: E2E 集成测试 (估时 0.3 sprint)

- [ ] **T3.1** Write failing test: `gepa_e2e_with_real_evaluator_v2`（注入真实 IEvaluator V2 CompositeEvaluator）
- [ ] **T3.2** Write failing test: `gepa_e2e_with_real_mutation_governor`（注入真实 MutationGovernor + 真实 IApprovalHandler mock）
- [ ] **T3.3** Write failing test: `gepa_e2e_regression_decline_aborts`（修订导致回归 → abort，不 commit）
- [ ] **T3.4** Verify fail: 3 cases FAIL（集成场景）
- [ ] **T3.5** Implement: E2E 测试 mock：
  - Mock ILLMProvider（返回固定 prompt 修订候选）
  - Mock IApprovalHandler（agent 模式返回 true）
  - 真实 IEvaluator V2 + MutationGovernor + BehavioralRegressionGate
- [ ] **T3.6** Verify pass: 3 cases PASS
- [ ] **T3.7** Commit: `test(gepa): E2E integration with real V2 evaluator + G11 governor (T3)`

## Phase 4: 文档同步 + ship 验证 (估时 0.2 sprint)

- [ ] **T4.1** Modify `docs/adr/adr-0071-llm-native-agenticdsl-architecture.md` §决策 5 / §实施: 追加 GEPA MVP V1 ship 注记
- [ ] **T4.2** Modify `docs/architecture/capability-application-map-2026-08.md`:
  - 头部 v2.0 → v2.1 + 最后验证 2026-08-27
  - §一 +1（能力 #27 GEPA MVP V1）
  - §三 B7 行 → ✅ Completed
  - §八 T19 行 Phase 1 只读反思约束行更新（移除 "Phase 2 commit 已解锁" 加 "Phase 2 committed 2026-08-27"）
  - §七 changelog v2.1 条目
- [ ] **T4.3** Modify `docs/architecture/self-evolution-architecture-2026-08.md` §四: 评估/奖励/变异治理行追加 "GEPA MVP V1 ship 2026-08-27"
- [ ] **T4.4** Modify `docs/active-status.md` §一 T19 跟踪段 + G11 跟踪段同步
- [ ] **T4.5** Verify: `python3 tools/adr_lint.py` + `docs_drift_audit.py` 全通过
- [ ] **T4.6** Verify: `openspec validate --changes --strict` PASS
- [ ] **T4.7** Verify: `ctest --output-on-failure` 全量 0 回归（动态基线）
- [ ] **T4.8** Commit: `feat(gepa): ship T19 Phase 2 commit MVP - B7 自进化基础应用解锁`
- [ ] **T4.9** `openspec archive t19-gepa-phase2-commit`

## 总估时

- Phase 0: 0.4 sprint
- Phase 1: 0.6 sprint
- Phase 2: 0.3 sprint
- Phase 3: 0.3 sprint
- Phase 4: 0.2 sprint
- **总估时: ~1.8 sprint**（中等复杂度，符合 T19 估时范围）

## 明确 out of scope (V2+ deferred)

- 多 agent 协同反思循环（V2）
- 异步 commit 路径（V1 同步）
- 跨 session 经验积累（V2 + SessionManager V2）
- 在线权重微调（L4 权重，V2 + G11 L4 解锁）
- Pareto 多目标评估（依赖 IEvaluator V3+）
- TrajectoryFidelity 评估（依赖 T15 V2 schema）
- LLM 修订 prompt 生成（V1 使用 Mock ILLMProvider）

## 关键不变量（强制遵守）

- ❌ CognitiveWorker / DomainWorkerPool setter **零修改**
- ❌ IEvaluator / IEvaluator V2 / MutationGovernor / TrajectoryIR / SkillCompiler / BehaviorRegression 公开 API **零修改**
- ❌ L4 权重变异（V1 G11 显式禁止）
- ❌ 修改任何既有 ADR 状态
- ❌ 在测试失败时强行 commit
- ❌ 硬编码 ctest 数字
- ❌ 真实触发外部 LLM 调用（仅 Mock ILLMProvider）

## V1 简化策略

- **单 agent 同步循环**：失败 → 反思 → 修订 → 回归 → 评估 → commit（全同步）
- **Mock ILLMProvider**：返回固定 prompt 修订候选，避免外部依赖
- **白名单 source_id 默认 "R_T19_GEPA"**：确保 MutationGovernor gate 通过
- **max_iterations 默认 3**：避免无限循环
- **reward_threshold 默认 0.0**：任何正向改进即 commit

## Mock ILLMProvider 设计

```cpp
class MockLLMProvider : public ILLMProvider {
public:
    std::string generate(const std::string& prompt) override {
        // 返回固定修订候选：插入 "Reflection note:" + 失败模式描述
        return "Reflection note: Add error handling for " + last_failure_;
    }
    void set_failure(const std::string& f) { last_failure_ = f; }
private:
    std::string last_failure_;
};
```

E2E 测试使用此 mock 注入 GEPALoop，确保循环可重现且零外部依赖。