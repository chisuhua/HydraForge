# ADR-0017: 反事实推理
> 📋 **Phase 3 规划: 推理能力** (规划于 2026-05/06, 2026-06-09 整理归档) — 见 `implementation-roadmap.md`
## 状态

**❌ 未实施** (2026-05-13, 2026-06-09 标注废弃)

## 背景

HydraForge Phase 2 需要反事实推理能力，用于对比多场景决策。当 Agent 需要分析"如果...会怎样"时：

- **基准场景**：当前已选择的方案
- **变体场景**：其他可能的替代方案
- **评估器**：判断哪个方案更优

**应用场景**：
- 决策分析：比较不同策略的预期结果
- 风险评估：评估不同行动方案的风险
- What-if 分析：探索不同假设条件下的结果

---

## 决策

### 1. 子图定义

#### `/lib/reasoning/counterfactual_compare@v1`

```yaml
AgenticDSL `/lib/reasoning/counterfactual_compare@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: true
    - name: user_id
      type: string
      required: true
    - name: base_scenario
      type: object
      required: true
      description: "基准场景描述"
    - name: variants
      type: array
      required: true
      description: "变体场景列表"
      items:
        type: object
        properties:
          id: { type: string }
          description: { type: string }
          parameters: { type: object }
    - name: evaluator_path
      type: string
      required: true
      description: "评估器子图路径"
  outputs:
    - name: comparison_result
      type: object
      properties:
        base_score: { type: number }
        variant_scores: { type: array }
        best_variant: { type: string }
        reasoning: { type: string }
version: "1.0"
stability: experimental
permissions:
  - reasoning: llm_generate
```

### 2. 执行逻辑

```
1. 对基准场景调用 evaluator_path → base_score
2. 对每个变体场景并行调用 evaluator_path → variant_scores
3. 比较所有分数，确定最佳方案
4. 返回 comparison_result
```

---

## 参考

- [ADR-0015: IPER 闭环推理](./adr-0015-iper-loop.md)