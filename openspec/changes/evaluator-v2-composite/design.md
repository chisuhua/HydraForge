# Design: evaluator-v2-composite

## Context

ADR-0083 V1 (IEvaluator + RewardSignal + ExecutionTrace + TaskSuccessEvaluator) 已 ship 2026-08-26。本 change 实施 V2 评估器（BehavioralEquivalenceEvaluator + CompositeEvaluator），为 GEPA 反思循环 + AFlow MCTS 工作流搜索 + B7 自进化基础应用提供更精细的评估信号。

V2 评估器仅作为 IEvaluator 接口的子类实现，IEvaluator 公开 API 零修改，确保与 V1 + Worker setter 完全兼容。

## Scope Boundaries

### 范围 IN
- BehavioralEquivalenceEvaluator 子类（基于 T14 BehavioralRegressionGate）
- CompositeEvaluator 子类（多评估器加权聚合）
- ≥ 6 新增测试 cases（BehavioralEquivalence ≥ 3 + Composite ≥ 3 + 集成 ≥ 2）
- CMake 注册（`src/modules/cognitive/CMakeLists.txt` 仅追加）
- 文档同步（ADR-0083 V2 ship 注记 + cap-map §一 +1）

### 范围 OUT
- IEvaluator 接口修改（强制不变量）
- V1 TaskSuccessEvaluator 修改（向后兼容）
- CognitiveWorker/DomainWorkerPool setter 修改（共享 V1 接口）
- TrajectoryFidelityEvaluator（依赖 T15 V2 schema）
- ParetoEvaluator（依赖 CompositeEvaluator 先稳定）
- RLHF 训练数据导出（V3+）
- 评估器配置 DSL（YAML/JSON）

## Design Decisions

### D1 — V2 仅子类，接口零修改

IEvaluator 接口（2 虚函数 evaluate + compare）已 ship 且稳定，V2 评估器仅作为 `class BehavioralEquivalenceEvaluator : public IEvaluator` 和 `class CompositeEvaluator : public IEvaluator` 实现。

理由：
- CognitiveWorker/DomainWorkerPool setter 接受 `shared_ptr<IEvaluator>`，自动兼容 V2
- V1 测试 12 cases 必须保持 PASS，零回归
- 避免接口演化引入 breaking change

### D2 — BehavioralEquivalenceEvaluator V1 简化

`evaluate(trace)` V1 返回 `RewardSignal::acceptable(0.5)`（单 trace 无法评估等价性），`compare(a, b)` 复用 T14 BehavioralRegressionGate：

```cpp
int compare(const ExecutionTrace& a, const ExecutionTrace& b) const override {
    auto fp_a = BehavioralRegressionGate::compute_fingerprint(a.final_result.data);
    auto fp_b = BehavioralRegressionGate::compute_fingerprint(b.final_result.data);
    auto verdict = BehavioralRegressionGate::hotelling_t2_test(fp_a, fp_b, 0.05);
    if (verdict == Verdict::Pass || verdict == Verdict::Inconclusive) return 0;
    return a.final_result.data.contains("reward") ? 
           (a.final_result.data["reward"].get<double>() > b... : -1) : 0;
}
```

理由：等价性评估需成对输入，evaluate 单 trace V1 仅返回 Acceptable 占位。

### D3 — CompositeEvaluator 加权聚合策略

```cpp
RewardSignal evaluate(const ExecutionTrace& trace) const override {
    std::vector<RewardSignal> signals;
    for (auto& e : evaluators_) signals.push_back(e->evaluate(trace));
    
    double weighted_scalar = 0.0;
    for (size_t i = 0; i < signals.size(); ++i) {
        weighted_scalar += signals[i].scalar * weights_[i];
    }
    
    // quality 取众数（Excellent > Acceptable > Poor）
    auto quality = std::max_element(...);
    double confidence = std::min_element(signals.confidence);
    
    return RewardSignal{quality, weighted_scalar, confidence};
}
```

compare 类似加权聚合比较结果（threshold ±0.1 决定胜负）。

### D4 — 构造函数严格校验

```cpp
CompositeEvaluator(vector<shared_ptr<IEvaluator>> evaluators, vector<double> weights) {
    if (evaluators.empty()) throw std::invalid_argument("evaluators must be non-empty");
    if (evaluators.size() != weights.size()) throw std::invalid_argument("size mismatch");
    // weights 归一化（除以 sum）
    double sum = std::accumulate(weights.begin(), weights.end(), 0.0);
    for (auto& w : weights_) w /= sum;
}
```

理由：fail-fast 防止运行时静默错误。

### D5 — CMake 仅追加

`src/modules/cognitive/CMakeLists.txt` 仅添加 `behavioral_equivalence_evaluator.cpp` + `composite_evaluator.cpp` 到既有 sources 列表，不修改任何既有规则。

## Risks

| 风险 | 缓解 |
|---|---|
| BehavioralEquivalence compare 复杂度高 | T14 已有成熟实现（6 cases PASS），V2 仅复用 |
| CompositeEvaluator 权重归一化浮点精度 | V1 简化：仅默认权重即可，V2 增强精度 |
| V1 测试回归 | Phase 0/1 末尾显式 `ctest -R test_evaluator` 验证 |
| ctest 数字硬编码 | tasks.md 禁止 + 动态基线 |
| docs_drift_audit 引入新 drift | Phase 3 验证 + Scenario 6 pre-existing |

## Verification Gates

- ≥ 6 新增 cases test_evaluator PASS (总 ≥ 18 cases)
- V1 12 cases 零回归
- ctest 全量 0 回归（动态计数）
- adr_lint 82 ADR PASS
- docs_drift_audit 0 NEW drift
- IEvaluator 接口 0 diff

## Dependencies

### 满足
- ✅ IEvaluator V1 ship (commit `21dd622` 已 archive)
- ✅ T14 BehavioralRegressionGate ship (Hotelling T² + fingerprint)
- ✅ T17 SkillCompiler ship (L3 评估对象生成)
- ✅ T15 Trajectory IR ship (独立序列化)
- ✅ G11 变异治理 ship (评估门集成)

### 不依赖
- V3+ 评估器
- Worker 代码修改

## Out of Scope (V3+ deferred)

- TrajectoryFidelityEvaluator
- ParetoEvaluator
- RLHF 训练数据导出
- 评估器配置 DSL
- 跨 evaluator 缓存

## Success Criteria

- IEvaluator V2 ✅ Shipped
- IEvaluator 接口 0 diff
- V1 12 cases 零回归
- 6+ 新增 cases PASS
- ctest 189+ 全量零回归
- OpenSpec archive 完成
- ADR-0083 V2 ship 注记
- cap-map §一 +1 (能力 #26)