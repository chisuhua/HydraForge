# ADR-0010: 记忆系统标准接口

## 状态

**已批准** (2026-05-13)

## 背景

HydraForge Phase 1 通过 ADR-8 定义了 LayeredContext 结构（L1-L5），但仅提供底层 `state.read`/`state.write` 工具原语。L4 Working Layer 需要更完整的记忆操作接口，支持：

- **层级分离**：working (活跃状态) ↔ archive (历史压缩)
- **向量检索**：语义搜索能力
- **多作用域**：agent_id + user_id 复合键

**参考系统**：
- Mem0：基于 facts 的记忆组织，semantic search + metadata filtering
- MemGPT/Letta：层级记忆（Core/Archival/Recall）
- LangChain：MessageHistory + CondensedMemoryChain

---

## 决策

### 1. 记忆组织模型：层级 + 作用域

```
memory.state.{agent_id}.{user_id}.{key}
         │          │         │
         │          │         └─ 状态路径（如 preferences.language）
         │          └─ 用户标识
         └─ Agent 标识
```

**设计原则**：
- 按 `agent_id + user_id` 复合键隔离
- 支持嵌套路径（如 `preferences.language`）
- L4 Working 层为活跃状态，L3 Archive 为压缩历史

### 2. 子图定义

#### `/lib/memory/state/set@v1`

```yaml
AgenticDSL `/lib/memory/state/set@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: true
    - name: user_id
      type: string
      required: true
    - name: key
      type: string
      required: true
      description: "状态路径，如 'preferences.language'"
    - name: value
      type: any
      required: true
    - name: ttl_seconds
      type: integer
      default: 0
      description: "TTL 秒数，0 = 永不过期"
  outputs:
    - name: success
      type: boolean
version: "1.0"
stability: stable
permissions:
  - memory: state_write
```

#### `/lib/memory/state/get@v1`

```yaml
AgenticDSL `/lib/memory/state/get@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: true
    - name: user_id
      type: string
      required: true
    - name: key
      type: string
      required: true
  outputs:
    - name: value
      type: any
      required: false
    - name: exists
      type: boolean
version: "1.0"
stability: stable
permissions:
  - memory: state_read
```

#### `/lib/memory/state/delete@v1`

```yaml
AgenticDSL `/lib/memory/state/delete@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: true
    - name: user_id
      type: string
      required: true
    - name: key
      type: string
      required: true
  outputs:
    - name: success
      type: boolean
version: "1.0"
stability: stable
permissions:
  - memory: state_delete
```

#### `/lib/memory/state/search@v1`（新增：向量语义检索）

```yaml
AgenticDSL `/lib/memory/state/search@v1`
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
    - name: filter_keys
      type: array
      required: false
      description: "限定搜索的 key 前缀"
  outputs:
    - name: results
      type: array
      items:
        type: object
        properties:
          key: { type: string }
          value: { type: any }
          score: { type: number }
          snippet: { type: string }
version: "1.0"
stability: stable
permissions:
  - memory: vector_search
```

#### `/lib/memory/state/query@v1`（新增：键模式查询）

```yaml
AgenticDSL `/lib/memory/state/query@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: true
    - name: user_id
      type: string
      required: true
    - name: key_pattern
      type: string
      required: true
      description: "支持通配符，如 'preferences.*'"
  outputs:
    - name: entries
      type: array
      items:
        type: object
        properties:
          key: { type: string }
          value: { type: any }
version: "1.0"
stability: stable
permissions:
  - memory: state_query
```

#### `/lib/memory/state/batch@v1`（新增：批量操作）

```yaml
AgenticDSL `/lib/memory/state/batch@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: true
    - name: user_id
      type: string
      required: true
    - name: operations
      type: array
      items:
        type: object
        properties:
          op: { type: string, enum: [set, delete] }
          key: { type: string }
          value: { type: any }
  outputs:
    - name: success
      type: boolean
    - name: failed_ops
      type: array
version: "1.0"
stability: stable
permissions:
  - memory: state_write
```

### 3. 权限模型

| 权限声明 | 说明 |
|----------|------|
| `memory: state_write` | 写入 `memory.state.*` |
| `memory: state_read` | 读取 `memory.state.*` |
| `memory: state_delete` | 删除 `memory.state.*` |
| `memory: state_query` | 查询键模式 |
| `memory: vector_search` | 向量语义检索 |

### 4. 与 LayeredContext L1-L5 的集成

```
L4 Working (memory.state.{agent_id}.{user_id}.*)
  ├─ set/get/delete/query/search/batch
  └─ 工具通过 state.write 写入

L2 Recent (recent_turns)
  └─ 由 ADR-7 ContextCompressor 管理

L3 Archive (archive)
  └─ search@v1 可跨越 L4 + L3 搜索
```

---

## 后果

### 正面

- L4 Working 层有完整的记忆操作接口
- 向量检索支持语义搜索能力
- 作用域隔离支持多用户/多 Agent

### 负面

- 增加了 ToolRegistry 中需要注册的内存工具数量
- 向量检索需要外部向量库（Qdrant/FAISS）支持

### 待决策

- 向量库选型（Qdrant vs FAISS vs ChromaDB）

---

## 参考

- [Mem0: Memory system for AI agents](https://github.com/mem0ai/mem0)
- [MemGPT/Letta: Memory management for LLMs](https://github.com/MemGPT/MemGPT)
- [ADR-0008: 结构化 Context](./adr-0008-structured-context.md)
- [ADR-0007: 上下文压缩](./adr-0007-context-compression.md)