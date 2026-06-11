# ADR-0012: 向量语义记忆
> ⛔ **已废弃 (2026-06-09)** — 代码侧 0 命中,仅作设计历史保留。详见 OpenSpec change `tech-debt-and-doc-cleanup`
## 状态

**❌ 未实施** (2026-05-13, 2026-06-09 标注废弃)

## 背景

HydraForge Phase 2 需要支持基于 embedding 的语义检索能力。不同于 KG 的结构化图查询，向量检索专注于"语义相似度"匹配，适用于：

- **历史问题匹配**：找到与当前问题相似的历史问题
- **知识去重**：判断新知识是否与已有知识重复
- **概念发现**：找到包含类似概念的文档

**设计约束**：
- 仅定义接口，不绑定具体向量库（Qdrant/FAISS/ChromaDB 等）
- 纯向量检索，不做混合检索
- 与 KG 系统独立

---

## 决策

### 1. 命名空间设计

```
memory.vector.{agent_id}.{user_id}.*
        │          │         │
        │          │         └─ 用户私有向量存储
        │          └─ Agent 级别向量存储
        └─ 向量存储命名空间（独立于 KG）
```

### 2. 子图定义

#### `/lib/memory/vector/store@v1`

```yaml
AgenticDSL `/lib/memory/vector/store@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: true
    - name: user_id
      type: string
      required: true
    - name: texts
      type: array
      required: true
      description: "待存储的文本列表"
      items:
        type: string
    - name: metadata
      type: object
      required: false
      description: "元数据（如 source, timestamp, tags）"
    - name: embedding_model
      type: string
      required: false
      default: "default"
      description: "Embedding 模型（为空使用默认）"
  outputs:
    - name: success
      type: boolean
    - name: stored_count
      type: integer
    - name: vector_ids
      type: array
      description: "存储后的向量 ID 列表"
version: "1.0"
stability: stable
permissions:
  - memory: vector_store
```

#### `/lib/memory/vector/recall@v1`

```yaml
AgenticDSL `/lib/memory/vector/recall@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: true
    - name: user_id
      type: string
      required: true
    - name: query
      type: string
      required: true
      description: "自然语言查询"
    - name: top_k
      type: integer
      default: 5
      maximum: 20
    - name: filter_metadata
      type: object
      required: false
      description: "基于 metadata 的过滤条件"
    - name: embedding_model
      type: string
      required: false
      default: "default"
  outputs:
    - name: results
      type: array
      description: "检索结果"
      items:
        type: object
        properties:
          text: { type: string }
          score: { type: number }
          metadata: { type: object }
          vector_id: { type: string }
version: "1.0"
stability: stable
permissions:
  - memory: vector_recall
```

#### `/lib/memory/vector/delete@v1`

```yaml
AgenticDSL `/lib/memory/vector/delete@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: true
    - name: user_id
      type: string
      required: true
    - name: vector_ids
      type: array
      required: false
      description: "要删除的向量 ID 列表"
    - name: filter_metadata
      type: object
      required: false
      description: "基于 metadata 删除（如删除某用户的全部）"
  outputs:
    - name: success
      type: boolean
    - name: deleted_count
      type: integer
version: "1.0"
stability: stable
permissions:
  - memory: vector_delete
```

#### `/lib/memory/vector/update_metadata@v1`

```yaml
AgenticDSL `/lib/memory/vector/update_metadata@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: true
    - name: user_id
      type: string
      required: true
    - name: vector_ids
      type: array
      required: false
    - name: filter_metadata
      type: object
      required: false
    - name: new_metadata
      type: object
      required: true
  outputs:
    - name: success
      type: boolean
    - name: updated_count
      type: integer
version: "1.0"
stability: stable
permissions:
  - memory: vector_update
```

#### `/lib/memory/vector/deduplicate@v1`

```yaml
AgenticDSL `/lib/memory/vector/deduplicate@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: true
    - name: user_id
      type: string
      required: true
    - name: new_text
      type: string
      required: true
      description: "待检查的新文本"
    - name: similarity_threshold
      type: number
      default: 0.95
      description: "相似度阈值（0-1），超过则认为重复"
  outputs:
    - name: is_duplicate
      type: boolean
    - name: duplicate_of
      type: object
      required: false
      properties:
        vector_id: { type: string }
        text: { type: string }
        score: { type: number }
version: "1.0"
stability: experimental
permissions:
  - memory: vector_recall
```

### 3. 权限模型

| 权限声明 | 说明 |
|----------|------|
| `memory: vector_store` | 存储向量 |
| `memory: vector_recall` | 检索向量 |
| `memory: vector_delete` | 删除向量 |
| `memory: vector_update` | 更新元数据 |

### 4. 与 LayeredContext 的集成

```
L4 Working: memory.state.*              (键值状态，ADR-0010)
L3 Archive: memory.kg.*                 (知识图谱，ADR-0011)
            memory.vector.*            (向量存储，独立)
L2 Recent:  最近访问的知识
L1 System:  Agent 提示词 + 工具定义
```

### 5. 工具注册要求

执行器必须预注册以下向量工具：

| 工具名 | 输入 | 输出 | 说明 |
|--------|------|------|------|
| `vector_store` | `{texts, metadata, model}` | `{success, ids}` | 存储向量 |
| `vector_recall` | `{query, top_k, filter}` | `{results}` | 检索向量 |
| `vector_delete` | `{ids, filter}` | `{success, count}` | 删除向量 |
| `vector_update` | `{ids, filter, metadata}` | `{success, count}` | 更新元数据 |
| `vector_deduplicate` | `{text, threshold}` | `{is_dup, match}` | 语义去重 |

---

## 后果

### 正面

- 提供纯语义相似度搜索能力
- 接口抽象，不绑定向量库
- 与 KG、状态存储独立，易于扩展

### 负面

- 需要外部向量库支持
- Embedding 模型需要额外配置

### 待决策

- 向量库选型（Qdrant vs FAISS vs ChromaDB）
- Embedding 模型选型

---

## 参考

- [LightRAG: Simple and Fast RAG](https://github.com/netease-youdao/LightRAG)
- [Mem0: Memory system for AI](https://github.com/mem0ai/mem0)
- [ADR-0010: 记忆系统标准接口](./archive/adr-0010-memory-system.md)