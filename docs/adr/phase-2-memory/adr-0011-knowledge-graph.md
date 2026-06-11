# ADR-0011: 知识图谱系统与 Meta-KG 导航
> 📋 **Phase 2 规划: 记忆系统** (规划于 2026-05/06, 2026-06-09 整理归档) — 见 `implementation-roadmap.md`
## 状态

**❌ 未实施** (2026-05-13, 2026-06-09 标注废弃)

## 背景

HydraForge Phase 2 需要支持结构化知识存储与检索。ADR-0010 定义了键值记忆接口，但缺乏图结构知识的表达能力。Agent 需要：

- **多跳推理查询**（multi-hop reasoning）
- **证据路径返回**（explainable reasoning）
- **混合查询模式**（自然语言 + Cypher）
- **知识导航**（navigate related concepts）
- **混合作用域**（per-user + per-agent + global）

**参考系统**：
- Graphiti：Temporal KG with entity extraction
- Cognee：Multi-modal KG
- MemGPT：Hierarchical memory with recall

---

## 决策

### 1. 作用域层次设计

```
global KG (memory.kg.global.*)      ← 全局共享知识
        ↑
agent KG (memory.kg.{agent_id}.*)   ← Agent 级别知识
        ↑
user KG (memory.kg.{agent_id}.{user_id}.*)  ← 用户私有知识
```

**查询解析顺序（默认）**：user → agent → global（可配置 `scope` 参数）

**写入传播选项**：
- `write_scope: user` — 只写入用户 KG
- `write_scope: agent` — 写入 agent KG
- `write_scope: global` — 写入全局 KG（需特殊权限）
- `propagate: true` — 写入 user KG 后同步到 agent/global（根据权限）

### 2. Meta-KG 设计

**位置**：`memory.kg.meta.*`（独立于主 KG）

**目的**：导航层，用于发现知识，而非仅存储知识

**内容结构**：
```
memory.kg.meta.topics.*        # Topic 节点（概念/主题）
memory.kg.meta.relations.*    # Topic 间关系
memory.kg.meta.popularity.*   # 热度统计
memory.kg.meta.recency.*      # 最近访问
```

**自动维护机制**：
- 实体写入时：自动提取实体类型 → 更新 Topic
- 查询时：更新 recency 统计
- 定期同步：从主 KG 汇总到 Meta-KG

### 3. 子图定义

#### `/lib/memory/kg/query_subgraph@v1`

```yaml
AgenticDSL `/lib/memory/kg/query_subgraph@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: false
      description: "限定查询范围到特定 Agent（为空则跨所有 Agent）"
    - name: user_id
      type: string
      required: false
      description: "限定查询范围到特定用户（为空则跨所有用户）"
    - name: scope
      type: string
      enum: [user, agent, global, all]
      default: all
    - name: query
      type: string
      required: true
      description: "自然语言查询 或 Cypher 查询"
    - name: query_mode
      type: string
      enum: [nl, cypher]
      default: nl
    - name: max_hops
      type: integer
      default: 3
      maximum: 5
    - name: evidence_required
      type: boolean
      default: true
  outputs:
    - name: subgraph
      type: object
      properties:
        nodes: { type: array }
        edges: { type: array }
    - name: explanation_paths
      type: array
      description: "推理路径 [(head, relation, tail), ...]"
    - name: sources
      type: array
      description: "结果来源 [user, agent, global]"
version: "1.0"
stability: stable
permissions:
  - kg: subgraph_query
```

#### `/lib/memory/kg/write_subgraph@v1`

```yaml
AgenticDSL `/lib/memory/kg/write_subgraph@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: true
    - name: user_id
      type: string
      required: true
    - name: nodes
      type: array
      required: true
      description: "实体列表 [{id, label, type, properties}]"
    - name: edges
      type: array
      required: true
      description: "关系列表 [{source, target, relation, properties}]"
    - name: write_scope
      type: string
      enum: [user, agent, global]
      default: user
    - name: propagate
      type: boolean
      default: true
      description: "是否向上层传播"
    - name: source
      type: string
      default: "user_provided"
  outputs:
    - name: subgraph_id
      type: string
    - name: nodes_written
      type: integer
    - name: edges_written
      type: integer
    - name: propagated_to
      type: array
version: "1.0"
stability: stable
permissions:
  - kg: subgraph_write
```

#### `/lib/memory/kg/extract@v1`

```yaml
AgenticDSL `/lib/memory/kg/extract@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: true
    - name: user_id
      type: string
      required: true
    - name: text
      type: string
      required: true
      description: "待抽取的自然语言文本"
    - name: ontology_id
      type: string
      required: false
    - name: target_scope
      type: string
      enum: [user, agent, global]
      default: user
  outputs:
    - name: subgraph_id
      type: string
    - name: extracted_entities
      type: array
    - name: extracted_relations
      type: array
version: "1.0"
stability: experimental
permissions:
  - kg: subgraph_write
  - reasoning: llm_generate
```

#### `/lib/memory/kg/search@v1`

```yaml
AgenticDSL `/lib/memory/kg/search@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: false
    - name: user_id
      type: string
      required: false
    - name: scope
      type: string
      enum: [user, agent, global, all]
      default: all
    - name: query
      type: string
      required: true
    - name: top_k
      type: integer
      default: 5
      maximum: 20
    - name: search_mode
      type: string
      enum: [vector, graph, hybrid]
      default: hybrid
  outputs:
    - name: results
      type: array
      items:
        type: object
        properties:
          node: { type: object }
          score: { type: number }
          path: { type: array }
          source: { type: string }
version: "1.0"
stability: stable
permissions:
  - kg: subgraph_query
```

#### `/lib/memory/kg/navigate@v1`（Meta-KG 导航）

```yaml
AgenticDSL `/lib/memory/kg/navigate@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: true
    - name: user_id
      type: string
      required: true
    - name: current_topic
      type: string
      required: true
    - name: navigation_mode
      type: string
      enum: [explore, narrow, expand]
      default: expand
      description: "explore=探索相关, narrow=精化缩小, expand=扩展视野"
    - name: max_hops
      type: integer
      default: 2
    - name: include_global
      type: boolean
      default: true
  outputs:
    - name: related_topics
      type: array
      items:
        type: object
        properties:
          topic: { type: string }
          relevance: { type: number }
          path: { type: array }
          source: { type: string }
    - name: suggested_queries
      type: array
version: "1.0"
stability: experimental
permissions:
  - kg: navigate
```

#### `/lib/memory/kg/get_topic@v1`

```yaml
AgenticDSL `/lib/memory/kg/get_topic@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: false
    - name: user_id
      type: string
      required: false
    - name: topic
      type: string
      required: true
    - name: include_global
      type: boolean
      default: true
  outputs:
    - name: topic_info
      type: object
      properties:
        name: { type: string }
        aliases: { type: array }
        category: { type: string }
        connections: { type: array }
        recent_entities: { type: array }
        source: { type: string }
version: "1.0"
stability: stable
permissions:
  - kg: read_ontology
```

#### `/lib/memory/kg/get_ontology@v1`

```yaml
AgenticDSL `/lib/memory/kg/get_ontology@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: false
    - name: user_id
      type: string
      required: false
    - name: ontology_id
      type: string
      required: false
  outputs:
    - name: ontology
      type: object
version: "1.0"
stability: stable
permissions:
  - kg: read_ontology
```

#### `/lib/memory/kg/register_ontology@v1`

```yaml
AgenticDSL `/lib/memory/kg/register_ontology@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: false
    - name: user_id
      type: string
      required: false
    - name: ontology
      type: object
      required: true
    - name: scope
      type: string
      enum: [user, agent, global]
      default: user
  outputs:
    - name: ontology_id
      type: string
version: "1.0"
stability: stable
permissions:
  - kg: write_ontology
```

### 4. 权限模型

| 权限声明 | 说明 |
|----------|------|
| `kg: subgraph_query` | 查询图谱 |
| `kg: subgraph_write` | 写入图谱 |
| `kg: read_ontology` | 读取 Ontology |
| `kg: write_ontology` | 注册/更新 Ontology |
| `kg: navigate` | Meta-KG 导航 |
| `kg: write: global` | 写入全局 KG（需特殊权限） |

### 5. 与 LayeredContext 的集成

```
L4 Working: memory.state.* (键值状态)
L3 Archive: memory.kg.* + memory.kg.meta.* (主 KG + Meta-KG)
L2 Recent:  最近访问的知识（用于热路径优化）
L1 System:  Agent 提示词 + 工具定义
```

### 6. 工具注册要求

执行器必须预注册以下 KG 相关工具：

| 工具名 | 输入 | 输出 | 说明 |
|--------|------|------|------|
| `kg_query` | `{scope, query, mode}` | `{subgraph, paths}` | 图谱查询 |
| `kg_write` | `{nodes, edges, scope}` | `{id, counts}` | 图谱写入 |
| `kg_extract` | `{text, ontology}` | `{entities, relations}` | LLM 抽取 |
| `kg_search` | `{query, mode, top_k}` | `{results}` | 混合搜索 |
| `kg_navigate` | `{topic, mode}` | `{related, suggestions}` | 导航 |
| `kg_ontology` | `{operation, ontology}` | `{ontology_id}` | Ontology 操作 |

---

## 后果

### 正面

- 支持多跳推理查询和可解释推理路径
- 混合查询模式（自然语言 + Cypher）
- Meta-KG 导航使 Agent 能发现性探索知识
- 混合作用域支持灵活的知识共享

### 负面

- 增加了 KG 工具注册数量
- 需要外部 KG 后端（Graphiti/Cognee）支持
- Meta-KG 自动维护增加了系统复杂度

### 待决策

- KG 后端选型（Graphiti vs Cognee vs Neo4j）
- 向量库选型（与 ADR-0010 统一）

---

## 参考

- [Graphiti: Temporal Knowledge Graph](https://github.com/mem0ai/graphiti)
- [Cognee: Multi-modal Knowledge Graphs](https://github.com/mem0ai/cognee)
- [MemGPT: Memory management for LLMs](https://github.com/MemGPT/MemGPT)
- [ADR-0010: 记忆系统标准接口](./adr-0010-memory-system.md)