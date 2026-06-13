# DSL 内存记忆规范 v3.10

> 合并自 `docs/specs/memory.md` (MEP-001 v3.2 Draft, 2025-10-31) 和 `docs/specs/dsl.md` §10.3 (v3.10, 2026-05-13)，
> 合并日期 2026-06-12，作为 `project-organization` 计划 Stage 2 / Task 9 的产出。
>
> `docs/archive/specs/phase2-standard-library-v1.0.md` 的 memory.* 部分（23 个子图）作为历史规划保留。

## 版本

v3.10 (2026-05-13 → 2026-06-12 unified)

## 概述

DSL 内存记忆子图提供 `/lib/memory/**` 命名空间下的标准化记忆操作，覆盖：

- **结构化状态**（键值）— `state/set`, `state/get_latest`
- **知识图谱**（KG 写入/查询）— `kg/write_subgraph`, `kg/query_subgraph`
- **向量语义**（向量存储/检索）— `vector/store`, `vector/recall`
- **用户画像**（profile 更新/读取）— `profile/update`, `profile/get`

> 历史 ADR 关联（已归档）：ADR-0010（结构化状态）、ADR-0011（KG）、ADR-0012（向量）、ADR-0013（profile）。见 `docs/archive/adr/`。

## 设计原则

| 原则 | 实现方式 |
|------|---------|
| **契约驱动** | 所有 `/lib/memory/**` 子图必须声明 `signature` |
| **最小权限** | 显式声明 `permissions`（如 `memory: state_write`） |
| **向后兼容** | 不修改现有执行原语，仅扩展标准库 |
| **可终止 & 可观测** | 每个操作生成结构化 Trace，含 `memory_op_type` |

## 子图完整清单（8 个，v3.10 当前规范）

---

### 1. `/lib/memory/state/set@v1`

> 结构化状态写入（中期记忆）。

```markdown
### AgenticDSL `/lib/memory/state/set@v1`
```yaml
signature:
  inputs:
    - name: key
      type: string
      description: "状态路径，如 'travel.departure_date'"
      required: true
    - name: value
      type: any
      required: true
  outputs:
    - name: success
      type: boolean
      required: true
  version: "1.0"
  stability: stable
permissions:
  - memory: state_write
type: assign
assign:
  expr: "{{ $.value }}"
  path: "memory.state.{{ $.key }}"
context_merge_policy:
  "memory.state.{{ $.key }}": last_write_wins
```
```

- **权限**：`memory: state_write`
- **对应后端**：任何 KV 存储（实现者自选；最小实现为 Context 内存）
- **来源**：`memory.md` §12.1.1

---

### 2. `/lib/memory/state/get_latest@v1`

> 结构化状态读取最新值（中期记忆）。

```markdown
### AgenticDSL `/lib/memory/state/get_latest@v1`
```yaml
signature:
  inputs:
    - name: key
      type: string
      required: true
  outputs:
    - name: value
      type: any
      required: false  # 可能为空
  version: "1.0"
type: assign
assign:
  expr: "{{ $.memory.state[key] | default(null) }}"
  path: "result.value"
```
```

- **权限**：`memory: state_read`（隐式，源自 `set` 的对偶）
- **对应后端**：与 `state/set` 一致
- **来源**：`memory.md` §12.1.1

---

### 3. `/lib/memory/kg/query_subgraph@v1`

> 知识图谱子图查询（中期+长期记忆）。
> 注：v3.10 重命名为 `query_subgraph`（原 v3.2 Draft 为 `kg/query_latest`），更精确表达"子图范围查询"语义。

```markdown
### AgenticDSL `/lib/memory/kg/query_subgraph@v1`
```yaml
signature:
  inputs:
    - name: head
      type: string
      required: true
    - name: relation
      type: string
      required: true
  outputs:
    - name: tail
      type: any
    - name: timestamp
      type: string
  version: "1.0"
permissions:
  - kg: subgraph_query
type: tool_call
tool: kg_query_subgraph
arguments:
  head: "{{ $.head }}"
  relation: "{{ $.relation }}"
output_mapping:
  tail: "result.tail"
  timestamp: "result.timestamp"
```
```

- **权限**：`kg: subgraph_query`
- **对应后端**：Graphiti（首选）、Neo4j Cypher、Cognee Adapter
- **来源**：`dsl.md` §10.3 + 详细 YAML 适配自 `memory.md` §12.1.2（`query_latest` → `query_subgraph`）

---

### 4. `/lib/memory/kg/write_subgraph@v1`

> 知识图谱子图写入（中期+长期记忆）。
> 注：v3.10 重命名为 `write_subgraph`（原 v3.2 Draft 为 `kg/write_fact`），从"单一事实"扩展为"子图批量写入"。

```markdown
### AgenticDSL `/lib/memory/kg/write_subgraph@v1`
```yaml
signature:
  inputs:
    - name: head
      type: string
      required: true
    - name: relation
      type: string
      required: true
    - name: tail
      type: any
      required: true
    - name: timestamp
      type: string
      format: "ISO8601"
      required: false  # 默认为 $.now
  outputs:
    - name: fact_id
      type: string
  version: "1.0"
permissions:
  - kg: subgraph_write
type: tool_call
tool: kg_write_subgraph
arguments:
  head: "{{ $.head }}"
  relation: "{{ $.relation }}"
  tail: "{{ $.tail }}"
  timestamp: "{{ $.timestamp or $.now }}"
output_mapping:
  fact_id: "result.fact_id"
```
```

- **权限**：`kg: subgraph_write`
- **对应后端**：与 `query_subgraph` 一致
- **来源**：`dsl.md` §10.3 + 详细 YAML 适配自 `memory.md` §12.1.2（`write_fact` → `write_subgraph`）

---

### 5. `/lib/memory/vector/store@v1`

> 向量语义存储（长期记忆）。

```markdown
### AgenticDSL `/lib/memory/vector/store@v1`
```yaml
signature:
  inputs:
    - name: text
      type: string
      required: true
    - name: metadata
      type: object
      required: false
      schema: { type: object }
  outputs:
    - name: success
      type: boolean
  version: "1.0"
permissions:
  - vector: store
type: tool_call
tool: vector_store
arguments:
  text: "{{ $.text }}"
  metadata:
    user_id: "{{ $.user.id }}"
    timestamp: "{{ $.now }}"
    task_id: "{{ $.task.id }}"
    extra: "{{ $.metadata | default({}) }}"
output_mapping:
  success: "result.success"
```
```

- **权限**：`vector: store`（自动附加 `user_id`，最小权限范围：当前用户）
- **工具注册**：`vector_store(text: string, metadata: object) -> success`
- **参考实现**：Pinecone / Qdrant / LightRAG + FAISS
- **来源**：`memory.md` §12.1.3

---

### 6. `/lib/memory/vector/recall@v1`

> 向量语义检索（长期记忆）。

```markdown
### AgenticDSL `/lib/memory/vector/recall@v1`
```yaml
signature:
  inputs:
    - name: query
      type: string
      required: true
    - name: top_k
      type: integer
      default: 3
  outputs:
    - name: memories
      type: array
      schema:
        type: array
        items:
          type: object
          properties:
            text: { type: string }
            score: { type: number }
            metadata: { type: object }
  version: "1.0"
permissions:
  - vector: recall
type: tool_call
tool: vector_recall
arguments:
  query: "{{ $.query }}"
  top_k: "{{ $.top_k }}"
  filter:
    user_id: "{{ $.user.id }}"
output_mapping:
  memories: "result.memories"
```
```

- **权限**：`vector: recall`（自动过滤 `user_id`，最小权限范围：当前用户）
- **工具注册**：`vector_recall(query: string, top_k: int, filter: object?) -> memories[]`
- **参考实现**：Pinecone / Qdrant / LightRAG Retriever
- **来源**：`memory.md` §12.1.3

---

### 7. `/lib/memory/profile/update@v1`

> 用户画像更新（长期记忆）。

```markdown
### AgenticDSL `/lib/memory/profile/update@v1`
```yaml
signature:
  inputs:
    - name: attributes
      type: object
      required: true
      schema: { type: object }
  outputs:
    - name: success
      type: boolean
  version: "1.0"
permissions:
  - profile: update
type: tool_call
tool: profile_update
arguments:
  user_id: "{{ $.user.id }}"
  attributes: "{{ $.attributes }}"
output_mapping:
  success: "result.success"
```
```

- **权限**：`profile: update`（最小权限范围：仅当前用户）
- **工具注册**：`profile_update(user_id: string, attributes: object) -> success`
- **参考实现**：Mem0 API Wrapper / Redis / MongoDB
- **来源**：`memory.md` §12.1.4

---

### 8. `/lib/memory/profile/get@v1`

> 用户画像读取（长期记忆）。

```markdown
### AgenticDSL `/lib/memory/profile/get@v1`
```yaml
signature:
  inputs: []
  outputs:
    - name: profile
      type: object
      schema: { type: object }
  version: "1.0"
permissions:
  - profile: read
type: tool_call
tool: profile_get
arguments:
  user_id: "{{ $.user.id }}"
output_mapping:
  profile: "result.profile"
```
```

- **权限**：`profile: read`（最小权限范围：仅当前用户）
- **工具注册**：`profile_get(user_id: string) -> profile`
- **参考实现**：Mem0 API Wrapper / Redis / MongoDB
- **来源**：`memory.md` §12.1.4

---

## 权限模型汇总

| 子图 | 权限 | 最小权限范围 |
|------|------|------------|
| `/lib/memory/state/set@v1` | `memory: state_write` | 仅限 Context 写入 |
| `/lib/memory/state/get_latest@v1` | `memory: state_read` | 仅限 Context 读取 |
| `/lib/memory/kg/query_subgraph@v1` | `kg: subgraph_query` | 仅限当前用户图谱 |
| `/lib/memory/kg/write_subgraph@v1` | `kg: subgraph_write` | 仅限当前用户图谱 |
| `/lib/memory/vector/store@v1` | `vector: store` | 自动附加 `user_id` |
| `/lib/memory/vector/recall@v1` | `vector: recall` | 自动过滤 `user_id` |
| `/lib/memory/profile/update@v1` | `profile: update` | 仅限当前用户 |
| `/lib/memory/profile/get@v1` | `profile: read` | 仅限当前用户 |

> ✅ 执行器必须在调度前验证权限，未授权 → 跳转 `on_error`。

## 工具注册要求

为支持上述子图，执行器必须预注册以下工具（由开发者实现）：

| 工具名 | 输入 | 输出 | 参考实现 |
|-------|------|------|----------|
| `kg_query_subgraph` | `{head, relation}` | `{tail, timestamp}` | Graphiti / Neo4j Cypher / Cognee |
| `kg_write_subgraph` | `{head, relation, tail, timestamp}` | `{fact_id}` | Graphiti / Cognee Adapter |
| `vector_store` | `{text, metadata}` | `{success}` | Pinecone / Qdrant / LightRAG + FAISS |
| `vector_recall` | `{query, top_k, filter}` | `{memories[]}` | Pinecone / Qdrant / LightRAG Retriever |
| `profile_update` | `{user_id, attributes}` | `{success}` | Mem0 API Wrapper |
| `profile_get` | `{user_id}` | `{profile}` | Mem0 API Wrapper |

> 🔧 工具实现**不要求**纳入规范，但**接口契约必须一致**。

## 可观测性（Trace Schema 扩展）

所有记忆操作 Trace 必须包含：

```json
{
  "memory_op_type": "state_set | kg_write | kg_query | vector_store | vector_recall | profile_update | profile_get",
  "memory_key": "travel.departure_date",
  "backend_used": "context | graphiti | qdrant | mem0",
  "latency_ms": 12,
  "user_id": "user_123"
}
```

## 命名变更记录（v3.2 Draft → v3.10）

| 旧名 (v3.2 Draft, memory.md) | 新名 (v3.10, dsl.md §10.3) | 变更原因 |
|------------------------------|----------------------------|---------|
| `kg/write_fact@v1` | `kg/write_subgraph@v1` | 从"单一事实"扩展为"子图批量写入" |
| `kg/query_latest@v1` | `kg/query_subgraph@v1` | 命名更精确，反映子图范围查询语义 |
| `kg/extract@v1` | (不包含在 v3.10) | 实验性功能，由 `docs/proposals/` 演进 |
| 权限 `kg: temporal_fact_insert` | `kg: subgraph_write` | 概念从"时间事实"提升为"子图" |
| 权限 `kg: temporal_fact_read` | `kg: subgraph_query` | 同上 |

## 对话上下文（不属本规范）

对话协议 (`/lib/conversation/**`) 见 [`docs/specs/dsl.md`](dsl.md) §10.4，**不属于** 内存记忆规范。

## 演进路线

- **v3.10**（当前）：8 个核心子图（`state.{set,get_latest}` / `kg.{write,query}_subgraph` / `vector.{store,recall}` / `profile.{update,get}`）
- **v3.11+**（实验性）：
  - `/lib/memory/orchestrator/hybrid_recall@v1`（融合结构化+语义）
  - 支持记忆 TTL（`assign` + `$.now` + 过期策略）

## 示例：订票助手使用标准记忆接口

```yaml
### AgenticDSL '/main/booking'
type: assign
assign:
  expr: "2025-11-20"
  path: "user_input.date"
next: "/lib/memory/state/set@v1?key=travel.departure_date&value={{ $.user_input.date }}"

### AgenticDSL '/main/confirm'
type: assign
assign:
  expr: "已记录您的出发日期为 {{ $.memory.state.travel.departure_date }}"
  path: "response.text"
next: "/end"
```

> ✅ 应用层无需关心记忆后端，仅依赖标准接口。

## 与现有系统的映射

| AgenticDSL 接口 | 推荐后端实现 |
|----------------|------------|
| `/lib/memory/state/**` | Context（内存） |
| `/lib/memory/kg/**` | Graphiti（首选）、Cognee |
| `/lib/memory/vector/**` | LightRAG + Qdrant / FAISS / Pinecone |
| `/lib/memory/profile/**` | Mem0 / Redis / MongoDB |

> **AgenticDSL 不应定义"如何存储记忆"，而应定义"如何调用记忆"**。

## 参见

- [`docs/specs/dsl.md`](dsl.md) §10.3 — 内存记忆原语 (v3.10)
- [`docs/specs/memory.md`](memory.md) (MEP-001, 2025-10-31, Draft) — **已删除**, 合并到本文件
- [`docs/specs/stdlib-v3.10.md`](stdlib-v3.10.md) — DSL 标准库 v3.10
- [`docs/archive/specs/phase2-standard-library-v1.0.md`](../archive/specs/phase2-standard-library-v1.0.md) — 历史 Phase 2 规划 (23 个 memory 子图)
- [`docs/archive/adr/`](../archive/adr/README.md) — 相关 ADR (0010, 0011, 0012, 0013) 已归档

---

*文档版本: v3.10 (2026-06-12 unified)*
