# ADR-0061-09: GEPA-style 反思循环

**日期**: 2026-07-16
**状态**: ✅ Approved + V1 Shipped 2026-08-27 (T19 GEPALoop) — V2 deferred
**父 ADR**: [../adr-0061-agent-evolution-and-solidification.md](../adr-0061-agent-evolution-and-solidification.md)

---

## 背景

GEPA (ICLR 2026 Oral, arXiv:2507.19457)：Genetic-Pareto 反思式 prompt 进化，比 GRPO 平均 +6%、最多 +20%，用 35× 更少 rollout。

## 决策

### 决策 1 — 反思循环

```cpp
class ReflectionLoop {
    // 1. 收集 trace 数据
    std::vector<TraceRecord> collect_traces();
    
    // 2. LLM 反思（哪些 prompt 修改可能提升效果）
    std::vector<PromptEdit> reflect(const std::vector<TraceRecord>&);
    
    // 3. Pareto 评估
    ParetoFront<ParetoCandidate> evaluate(const std::vector<PromptEdit>&);
    
    // 4. 选择下一个 candidate
    PromptEdit select_next(const ParetoFront<ParetoCandidate>&);
    
    // 5. 替换 prompt + regression 验证
    bool commit(const PromptEdit& edit);
};
```

### 决策 2 — Pareto Front

按 (performance, cost) 二维维护 Pareto front，选择下一个 candidate。

### 决策 3 — 与 behavioral regression 集成

每次反思的 candidate 必须通过 [ADR-0061-02](./adr-0061-02-behavioral-regression.md) 的 behavioral regression suite。

## 实施

- 依赖: [ADR-0061-02-behavioral-regression](./adr-0061-02-behavioral-regression.md), [ADR-0061-03-skill-compiler](./adr-0061-03-skill-compiler.md)
- 工作量: 3 weeks
- 优先级: P2

## 参考

- GEPA: arXiv:2507.19457 (ICLR 2026 Oral)
- Meta-Agent: arXiv:2605.25233