# evaluator-v2-composite

## Why

ADR-0083 (IEvaluator/RewardSignal Contract) V1 已 ship (2026-08-26, OpenSpec change `2026-08-26-ship-ievaluator-reward-contract` archived, 12 cases / 31 assertions PASS)。V1 仅实现 `TaskSuccessEvaluator`，V2 评估器（`BehavioralEquivalenceEvaluator` + `CompositeEvaluator`）明确留待 follow-up change。

**Oracle 评审关键发现** (cap-map §八 + ADR-0083 §决策 5):
- V1 仅支持 `ToolResult.ok` 简单映射，不足以支撑 GEPA 反思循环 + AFlow MCTS 工作流搜索
- GEPA/AFlow/fine-tune/行为克隆需要**行为等价性评估**（BehavioralEquivalenceEvaluator）+ **多评估器聚合**（CompositeEvaluator）
- IEvaluator 接口已 ship 且稳定（纯虚接口 + 2 虚函数），V2 评估器**仅作为新增子类**，无接口变更

**审计依据**:
- ADR-0083 ✅ Approved + ship 2026-08-26
- `include/agenticdsl/contract/ievaluator.h` 已 ship（纯虚接口 evaluate + compare）
- `include/agenticdsl/testing/behavioral_regression.h` 已 ship（Hotelling T² + fingerprint）
- V1 评估器: `src/modules/cognitive/task_success_evaluator.cpp` (3 行 ok → quality 映射)
- 当前 `tests/test_evaluator.cpp` 12 cases / 31 assertions

**前置依赖**（全部已满足）:
- ✅ IEvaluator V1 contract ship
- ✅ T14 行为回归套件 ship (BehavioralRegressionGate + fingerprint)
- ✅ T17 SkillCompiler ship (L3 变异对象生成器)
- ✅ T15 Trajectory IR ship (独立序列化视图)
- ✅ G11 变异治理 ✅ Approved + ship (评估门 + 回归门可接入)
- ✅ ADR-0061-02 BehavioralRegression ✅ Approved (T14 ship)

## What Changes

**新增 V2 评估器**（仅子类实现，IEvaluator 接口不变）:

1. **`BehavioralEquivalenceEvaluator`** — 基于 T14 行为回归的等价性评估
   - 输入: 两个 ExecutionTrace (a, b)
   - 输出: RewardSignal 基于 Verdict (Pass → Excellent, Fail → Poor)
   - 实现: 复用 `agenticdsl::testing::BehavioralRegressionGate::compute_fingerprint` + `hotelling_t2_test`
   - `evaluate(trace)` V1: 返回 Acceptable (行为等价性需成对输入)
   - `compare(a, b)` V1: 完整实现（fingerprint + Hotelling T² Verdict → +1/0/-1）

2. **`CompositeEvaluator`** — 多评估器加权聚合
   - 构造: `CompositeEvaluator(vector<shared_ptr<IEvaluator>> evaluators, vector<double> weights)`
   - `evaluate(trace)`: 加权聚合 RewardSignal（quality 取众数/均值，scalar 加权平均，confidence min）
   - `compare(a, b)`: 加权聚合比较结果
   - 边界: 空 evaluators → throw；weights 数量不匹配 → throw

**注册 CMake**:
- `src/modules/cognitive/CMakeLists.txt` 注册 `behavioral_equivalence_evaluator.cpp` + `composite_evaluator.cpp`

**新增测试** (`tests/test_evaluator.cpp` 扩展):
- BehavioralEquivalenceEvaluator ≥ 3 cases: ok pair vs fail pair / single trace V1 / compare V1
- CompositeEvaluator ≥ 3 cases: 2-evaluator 加权 / 空 evaluators throw / weights mismatch throw
- 与现有 IEvaluator 兼容性测试（V1 + V2 共存）

## Impact

**影响范围**:
- IEvaluator 接口**零修改**（仅新增子类）
- CognitiveWorker/DomainWorkerPool setter 集成**无需改动**（setter 接受 `shared_ptr<IEvaluator>`，V2 评估器自动兼容）
- 现有 12 cases 测试**零回归**

**V2 边界**（per ADR-0083 §决策 5）:
- ✅ 仅 2 个 V2 评估器（BehavioralEquivalence + Composite）
- ⏸ V3+ 评估器延后（TrajectoryFidelityEvaluator / ParetoEvaluator 等）
- ⏸ RLHF 训练数据导出延后（依赖 V2 + V3 评估器组合）

**下游解锁**:
- T19 GEPA Phase 2 commit（失败→反思→修订 prompt，可基于 BehavioralEquivalence 比较修订前后）
- T20 AFlow MCTS（工作流改写搜索空间需 CompositeEvaluator 加权打分）
- B7 自进化基础应用评估层升级
- ADR-0074 Prompt Evidence Gate（CompositeEvaluator 集成 few-shot + golden 评分）

**Breaking Changes**: 无（仅新增子类，IEvaluator 接口零修改）

## ship gate 验证

- `python3 tools/adr_lint.py` 通过（≥82 ADR）
- `python3 tools/docs_drift_audit.py` 通过（无新增 CRITICAL drift）
- `openspec validate --changes --strict` PASS
- `ctest --output-on-failure` 全量 0 回归（动态基线，188→189/190 净增）
- `ctest -R test_evaluator` ≥ 6 cases 新增 PASS（BehavioralEquivalence ≥ 3 + Composite ≥ 3）
- `grep "class BehavioralEquivalenceEvaluator\|class CompositeEvaluator" include/agenticdsl/cognitive/` 命中
- ADR-0083 状态追加 V2 ship 注记
- cap-map §一 +1 (新能力 #26) 或 §八 评估信号行更新

## 关联文档

- `docs/adr/adr-0083-evaluator-reward-contract.md` (V2 ship 注记)
- `include/agenticdsl/contract/ievaluator.h` (V1 interface, 零修改)
- `include/agenticdsl/testing/behavioral_regression.h` (T14 复用基座)
- `src/modules/cognitive/task_success_evaluator.cpp` (V1 参考实现)
- `tests/test_evaluator.cpp` (V1 12 cases + V2 新增)
- OpenSpec change `2026-08-26-ship-ievaluator-reward-contract` (V1 archived)
- ADR-0061-02 BehavioralRegression (T14 已 ship)
- `docs/architecture/capability-application-map-2026-08.md` §二 G10 + §八 评估信号
- `docs/active-status.md` §一 G10 跟踪段