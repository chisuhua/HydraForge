# ADR-0018: 图引导假设生成

## 状态

**已批准** (2026-05-13)

## 背景

HydraForge Phase 2 需要图引导假设生成能力，结合知识图谱进行推理。当 Agent 需要回答需要多跳推理的问题时：

- **问题**：需要回答的问题
- **KG 上下文**：从知识图谱获取的上下文信息
- **假设生成**：基于 KG 生成多个可能的答案假设

**应用场景**：
- 复杂问答：需要多跳推理的问题
- 证据推理：基于图谱证据生成置信答案
- 假设验证：生成多个假设并验证真伪

---

## 决策

### 1. 子图定义

#### `/lib/reasoning/graph_guided_hypothesize@v1`

```yaml
AgenticDSL `/lib/reasoning/graph_guided_hypothesize@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: true
    - name: user_id
      type: string
      required: true
    - name: question
      type: string
      required: true
      description: "需要回答的问题"
    - name: kg_context
      type: object
      required: true
      description: "知识图谱上下文"
      properties:
        start_entities:
          type: array
          items: { type: string }
          minItems: 1
          description: "起始实体列表"
        query_path:
          type: string
          description: "路径查询模式"
        max_hops:
          type: integer
          default: 3
          maximum: 5
    - name: max_hypotheses
      type: integer
      default: 3
      minimum: 1
      maximum: 10
      description: "最大假设数量"
  outputs:
    - name: hypotheses
      type: array
      required: true
      items:
        type: object
        properties:
          text:
            type: string
            description: "假设文本"
          evidence_path:
            type: array
            description: "支持该假设的证据路径"
            items:
              type: object
              properties:
                head: { type: string }
                relation: { type: string }
                tail: { type: string }
          confidence:
            type: number
            minimum: 0
            maximum: 1
version: "1.0"
stability: experimental
permissions:
  - kg: subgraph_query
  - reasoning: llm_generate
```

### 2. 与 KG 系统的集成

- 依赖 `/lib/memory/kg/query_subgraph@v1` 获取 KG 上下文
- 假设生成基于 KG 证据路径
- 证据路径格式与 ADR-0011 一致

---

## 参考

- [ADR-0011: 知识图谱与 Meta-KG 导航](./adr-0011-knowledge-graph.md)