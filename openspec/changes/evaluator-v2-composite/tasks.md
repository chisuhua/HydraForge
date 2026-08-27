# Tasks: evaluator-v2-composite

> **TDD 5 步结构**: 每任务按 Write failing test → Verify fail → Implement → Verify pass → Commit
> **关键不变量**: IEvaluator 接口**零修改**（仅新增子类实现）。V1 评估器 + 测试**零回归**。
> **设计依据**: ADR-0083 §决策 5 V2 评估器明确 out of scope → 本 change 实施。

## Phase 0: BehavioralEquivalenceEvaluator 契约 (估时 0.3 sprint)

- [x] **T0.1** Write failing test: `tests/test_evaluator.cpp` 扩展 ≥ 3 cases:
  - `behavioral_equivalence_compare_pass_pair`（相似 fingerprint + Hotelling Pass → compare 返回 0）
  - `behavioral_equivalence_compare_fail_pair`（差异 fingerprint + Hotelling Fail → compare 返回 +1/-1）
  - `behavioral_equivalence_evaluate_single_returns_acceptable`（V1: 单 trace evaluate 返回 Acceptable）
- [x] **T0.2** Verify fail: 编译失败（`fatal error: 'behavioral_equivalence_evaluator.h' file not found`）
- [x] **T0.3** Implement: `include/agenticdsl/cognitive/behavioral_equivalence_evaluator.h`:
  ```cpp
  namespace agenticdsl {
  class BehavioralEquivalenceEvaluator : public IEvaluator {
  public:
      BehavioralEquivalenceEvaluator();
      RewardSignal evaluate(const ExecutionTrace& trace) const override;  // V1: Acceptable
      int compare(const ExecutionTrace& a, const ExecutionTrace& b) const override;
  private:
      // 复用 agenticdsl::testing::BehavioralRegressionGate
  };
  }
  ```
- [x] **T0.4** Implement: `src/modules/cognitive/behavioral_equivalence_evaluator.cpp`
  - `evaluate(trace)`: 返回 `RewardSignal::acceptable(0.5)`（V1 单 trace 不评估）
  - `compare(a, b)`:
    - 调用 `BehavioralRegressionGate::compute_fingerprint(a.final_result.data)` 与 b
    - 调用 `BehavioralRegressionGate::hotelling_t2_test(fp_a, fp_b, budget)` (budget=0.05 默认)
    - `Verdict::Pass` → 0；`Verdict::Inconclusive` → 0；`Verdict::Fail` → 比较 reward scalar (a - b) → +1/-1/0
- [x] **T0.5** Verify pass: 3 cases PASS + V1 12 cases 零回归
- [x] **T0.6** Commit: `feat(cognitive): BehavioralEquivalenceEvaluator V2 - T14 integration (T0)`

## Phase 1: CompositeEvaluator 契约 (估时 0.3 sprint)

- [x] **T1.1** Write failing test: `tests/test_evaluator.cpp` 扩展 ≥ 3 cases:
  - `composite_aggregate_two_evaluators`（2 evaluators + weights → 加权 scalar 平均）
  - `composite_aggregate_empty_evaluators_throws`（空 vector → `std::invalid_argument`）
  - `composite_weights_mismatch_throws`（evaluators 数量 ≠ weights 数量 → `std::invalid_argument`）
- [x] **T1.2** Verify fail: 编译失败（无实现）
- [x] **T1.3** Implement: `include/agenticdsl/cognitive/composite_evaluator.h`:
  ```cpp
  namespace agenticdsl {
  class CompositeEvaluator : public IEvaluator {
  public:
      CompositeEvaluator(std::vector<std::shared_ptr<IEvaluator>> evaluators,
                        std::vector<double> weights);
      RewardSignal evaluate(const ExecutionTrace& trace) const override;
      int compare(const ExecutionTrace& a, const ExecutionTrace& b) const override;
  private:
      std::vector<std::shared_ptr<IEvaluator>> evaluators_;
      std::vector<double> weights_;  // normalized to sum=1
  };
  }
  ```
- [x] **T1.4** Implement: `src/modules/cognitive/composite_evaluator.cpp`
  - 构造校验: `evaluators_.size() == weights_.size()` 且非空，否则 throw `std::invalid_argument`
  - weights 归一化（除以 sum）
  - `evaluate(trace)`:
    - 对每个 evaluator 调用 evaluate(trace)
    - scalar 加权平均（Σ scalar * weight）
    - quality 取众数（Excellent > Acceptable > Poor 平局取较高）
    - confidence 取 min
  - `compare(a, b)`:
    - 对每个 evaluator 调用 compare(a, b)
    - 加权求和 → +1/-1/0（threshold ±0.1 决定胜负）
- [x] **T1.5** 注册 `src/modules/cognitive/CMakeLists.txt` 新源（**仅追加，不修改既有规则**）
- [x] **T1.6** Verify pass: 3 cases PASS + 全部既有测试零回归
- [x] **T1.7** Commit: `feat(cognitive): CompositeEvaluator V2 - multi-evaluator aggregation (T1)`

## Phase 2: 集成测试 (估时 0.2 sprint)

- [x] **T2.1** Write failing test: `v1_v2_coexistence`（TaskSuccessEvaluator + BehavioralEquivalenceEvaluator 同时注入到 CognitiveWorker 互不干扰）
- [x] **T2.2** Write failing test: `composite_with_v1_inside`（CompositeEvaluator 包装 TaskSuccessEvaluator → 验证 V1 在 V2 内可工作）
- [x] **T2.3** Verify fail: 集成测试 FAIL（需要 worker 集成 mock）
- [x] **T2.4** Implement: 测试内部使用现有 `set_evaluator(shared_ptr<IEvaluator>)` 注入多 evaluator（不修改 CognitiveWorker）
- [x] **T2.5** Verify pass: 2 cases PASS
- [x] **T2.6** Commit: `test(evaluator): V1+V2 coexistence + composite integration (T2)`

## Phase 3: 文档同步 + ship 验证 (估时 0.2 sprint)

- [x] **T3.1** 修改 `docs/adr/adr-0083-evaluator-reward-contract.md` 头部 `##状态`:
  - 追加 V2 ship 证据段: `evaluator-v2-composite` change, BehavioralEquivalence + Composite 实施, 6+ cases PASS
- [x] **T3.2** 修改 `docs/architecture/capability-application-map-2026-08.md`:
  - 头部 v1.9 → v2.0 + 最后验证 2026-08-27
  - §一 +1（新能力 #26 IEvaluator V2）
  - §八 评估信号行更新 V2 ship
  - §七 changelog 新增 v2.0 条目
- [x] **T3.3** 修改 `docs/active-status.md` §一 G10 跟踪段：标注 V2 ship
- [x] **T3.4** 验证: `python3 tools/adr_lint.py` 通过（≥82 ADR）
- [x] **T3.5** 验证: `python3 tools/docs_drift_audit.py` 通过（无新增 CRITICAL）
- [x] **T3.6** 验证: `ctest --output-on-failure` 全量 0 回归（动态基线，禁止硬编码 189）
- [x] **T3.7** Commit: `feat(evaluator): ship IEvaluator V2 (BehavioralEquivalence + Composite) - V2 evaluation layer`
- [x] **T3.8** `openspec archive evaluator-v2-composite`

## 总估时

- Phase 0: 0.3 sprint
- Phase 1: 0.3 sprint
- Phase 2: 0.2 sprint
- Phase 3: 0.2 sprint
- **总估时: ~1.0 sprint**（符合 Layer 2 估时范围）

## 明确 out of scope (V3+ 延后)

- TrajectoryFidelityEvaluator（依赖 T15 Trajectory IR V2 完整 schema）
- ParetoEvaluator（多目标优化，需 CompositeEvaluator 先稳定）
- RLHF 训练数据导出（依赖 V2 + V3 评估器组合）
- 评估器配置 DSL（YAML/JSON 配置 CompositeEvaluator 拓扑）
- 跨 evaluator 缓存与共享 fingerprint
- Worker setter 强制 V2 类型（保持 V1 接口完全兼容）

## 关键不变量（强制遵守）

- ❌ IEvaluator 接口**任何修改**（V2 仅子类实现）
- ❌ 修改 `include/agenticdsl/contract/ievaluator.h`
- ❌ 修改 `src/modules/cognitive/cognitive_worker.{h,cpp}` 或 `domain_worker_pool.{h,cpp}`
- ❌ 修改 V1 TaskSuccessEvaluator 实现
- ❌ 修改 V1 测试用例（12 cases 必须保持 PASS）
- ❌ 在测试失败时强行 commit
- ❌ 硬编码 ctest 数字