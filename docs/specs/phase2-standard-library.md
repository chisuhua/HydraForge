# HydraForge Phase 2 标准库规划

> **文档版本**: v1.0
> **日期**: 2026-05-13
> **状态**: 已批准
> **范围**: Phase 2 ADR-0010 ~ ADR-0018

---

## 一、概述

### 1.1 目标

Phase 2 扩展 HydraForge 标准库，提供完整的记忆、推理和对话能力，支持：

- **多层次记忆**：键值、图谱、向量、用户画像
- **结构化推理**：IPER 闭环、异常处理、假设生成
- **多角色对话**：话题隔离、会议协作

### 1.2 设计原则

1. **接口抽象**：不绑定具体后端实现（向量库、KG 系统）
2. **作用域分层**：`global` → `agent` → `user` 三层隔离
3. **权限最小化**：每个子图必须声明所需权限
4. **可扩展性**：experimental 轨道允许快速迭代

---

## 二、命名空间结构

```
memory.
├── state.{agent_id}.{user_id}.*    # 键值状态 (ADR-0010)
├── kg.{agent_id}.{user_id}.*       # 知识图谱 (ADR-0011)
│   ├── global.*                    # 全局 KG
│   └── meta.*                      # Meta-KG 导航
├── vector.{agent_id}.{user_id}.*    # 向量存储 (ADR-0012)
├── profile.{agent_id}.{user_id}.*   # 用户画像 (ADR-0013)
└── conversation.{agent_id}.{user_id}.*
    ├── topics.*                    # 话题上下文
    ├── roles.*                     # 角色上下文
    └── meetings.*                  # 会议上下文

lib/
├── reasoning/
│   ├── iper_loop@v1               # IPER 闭环 (ADR-0015)
│   ├── try_catch@v1               # 异常处理 (ADR-0016)
│   ├── counterfactual_compare@v1   # 反事实推理 (ADR-0017, experimental)
│   └── graph_guided_hypothesize@v1 # 图引导假设 (ADR-0018, experimental)
```

---

## 三、子图完整清单

### 3.1 memory.state.* — 键值状态 (ADR-0010)

| 子图 | 版本 | 稳定性 | 说明 |
|------|------|--------|------|
| `set@v1` | 1.0 | stable | 写入状态 |
| `get@v1` | 1.0 | stable | 读取状态 |
| `delete@v1` | 1.0 | stable | 删除状态 |
| `search@v1` | 1.0 | stable | 向量语义检索 |
| `query@v1` | 1.0 | stable | 键模式查询 |
| `batch@v1` | 1.0 | stable | 批量操作 |

### 3.2 memory.kg.* — 知识图谱 (ADR-0011)

| 子图 | 版本 | 稳定性 | 说明 |
|------|------|--------|------|
| `query_subgraph@v1` | 1.0 | stable | 图谱查询（NL/Cypher 双模式）|
| `write_subgraph@v1` | 1.0 | stable | 图谱写入 |
| `extract@v1` | 1.0 | experimental | 自然语言抽取 |
| `get_ontology@v1` | 1.0 | stable | 获取 Ontology |
| `register_ontology@v1` | 1.0 | stable | 注册 Ontology |
| `search@v1` | 1.0 | stable | 混合搜索 |
| `navigate@v1` | 1.0 | experimental | Meta-KG 导航 |
| `get_topic@v1` | 1.0 | stable | 获取 Topic 详情 |

### 3.3 memory.vector.* — 向量语义 (ADR-0012)

| 子图 | 版本 | 稳定性 | 说明 |
|------|------|--------|------|
| `store@v1` | 1.0 | stable | 存储向量 |
| `recall@v1` | 1.0 | stable | 向量检索 |
| `delete@v1` | 1.0 | stable | 删除向量 |
| `update_metadata@v1` | 1.0 | stable | 更新元数据 |
| `deduplicate@v1` | 1.0 | experimental | 语义去重 |

### 3.4 memory.profile.* — 用户画像 (ADR-0013)

| 子图 | 版本 | 稳定性 | 说明 |
|------|------|--------|------|
| `update@v1` | 1.0 | stable | 更新画像 |
| `get@v1` | 1.0 | stable | 读取画像 |
| `merge@v1` | 1.0 | experimental | 合并画像 |

### 3.5 conversation.* — 对话上下文 (ADR-0014)

| 子图 | 版本 | 稳定性 | 说明 |
|------|------|--------|------|
| `start_topic@v1` | 1.0 | stable | 开启话题 |
| `switch_role@v1` | 1.0 | stable | 切换角色 |
| `meeting@v1` | 1.0 | stable | 多角色会议 |
| `get_current@v1` | 1.0 | stable | 获取当前状态 |

### 3.6 reasoning.* — 推理原语 (ADR-0015 ~ ADR-0018)

| 子图 | 版本 | 稳定性 | 说明 |
|------|------|--------|------|
| `iper_loop@v1` | 1.0 | stable | IPER 闭环 |
| `try_catch@v1` | 1.0 | stable | 异常处理 |
| `counterfactual_compare@v1` | 1.0 | experimental | 反事实推理 |
| `graph_guided_hypothesize@v1` | 1.0 | experimental | 图引导假设 |

---

## 四、实现优先级

### Phase 2A — 核心记忆系统（高优先级）

| # | 子图 | ADR | 依赖 |
|---|------|-----|------|
| 1 | `memory.state.*` | ADR-0010 | 无 |
| 2 | `memory.kg.query/write` | ADR-0011 | 无 |
| 3 | `memory.vector.*` | ADR-0012 | 向量库 |
| 4 | `memory.profile.*` | ADR-0013 | 无 |

### Phase 2B — 对话与推理（中优先级）

| # | 子图 | ADR | 依赖 |
|---|------|-----|------|
| 5 | `conversation.*` | ADR-0014 | Phase 2A |
| 6 | `reasoning.iper_loop` | ADR-0015 | `generate_subgraph` |
| 7 | `reasoning.try_catch` | ADR-0016 | `memory.state` 快照 |
| 8 | `memory.kg.navigate` | ADR-0011 | KG + Meta-KG |

### Phase 2C — 实验性功能（experimental）

| # | 子图 | ADR | 依赖 |
|---|------|-----|------|
| 9 | `memory.kg.extract` | ADR-0011 | LLM + KG |
| 10 | `memory.kg.search` (hybrid) | ADR-0011 | 向量库 |
| 11 | `memory.vector.deduplicate` | ADR-0012 | 向量库 |
| 12 | `memory.profile.merge` | ADR-0013 | 无 |
| 13 | `reasoning.counterfactual` | ADR-0017 | LLM |
| 14 | `reasoning.graph_guided` | ADR-0018 | KG |

---

## 五、工具注册清单

执行器必须预注册以下工具：

### 5.1 State Tools

| 工具名 | 输入 | 输出 | 说明 |
|--------|------|------|------|
| `state_set` | `{key, value, ttl?}` | `{success}` | 写入状态 |
| `state_get` | `{key}` | `{value, exists}` | 读取状态 |
| `state_delete` | `{key}` | `{success}` | 删除状态 |
| `state_search` | `{query, top_k}` | `{results}` | 语义搜索 |
| `state_query` | `{key_pattern}` | `{entries}` | 键模式查询 |
| `state_batch` | `{operations}` | `{success, failed}` | 批量操作 |

### 5.2 KG Tools

| 工具名 | 输入 | 输出 | 说明 |
|--------|------|------|------|
| `kg_query` | `{scope, query, mode}` | `{subgraph, paths}` | 图谱查询 |
| `kg_write` | `{nodes, edges, scope}` | `{id, counts}` | 图谱写入 |
| `kg_extract` | `{text, ontology}` | `{entities, relations}` | LLM 抽取 |
| `kg_ontology` | `{operation, ontology}` | `{ontology_id}` | Ontology 操作 |
| `kg_search` | `{query, mode, top_k}` | `{results}` | 混合搜索 |
| `kg_navigate` | `{topic, mode}` | `{related, suggestions}` | Meta-KG 导航 |
| `kg_get_topic` | `{topic}` | `{topic_info}` | Topic 详情 |

### 5.3 Vector Tools

| 工具名 | 输入 | 输出 | 说明 |
|--------|------|------|------|
| `vector_store` | `{texts, metadata, model?}` | `{success, ids}` | 存储向量 |
| `vector_recall` | `{query, top_k, filter?}` | `{results}` | 检索向量 |
| `vector_delete` | `{ids, filter?}` | `{success, count}` | 删除向量 |
| `vector_update` | `{ids, filter?, metadata}` | `{success, count}` | 更新元数据 |
| `vector_deduplicate` | `{text, threshold}` | `{is_dup, match}` | 语义去重 |

### 5.4 Profile Tools

| 工具名 | 输入 | 输出 | 说明 |
|--------|------|------|------|
| `profile_update` | `{user_id, attributes}` | `{success, fields}` | 更新画像 |
| `profile_get` | `{user_id, fields?}` | `{profile}` | 读取画像 |
| `profile_merge` | `{user_id, source, strategy}` | `{success, profile}` | 合并画像 |

### 5.5 Conversation Tools

| 工具名 | 输入 | 输出 | 说明 |
|--------|------|------|------|
| `conv_start_topic` | `{topic_id, context?}` | `{context_path}` | 开启话题 |
| `conv_switch_role` | `{role_id}` | `{context_path}` | 切换角色 |
| `conv_meeting` | `{meeting_id, participants, mode}` | `{summary}` | 会议 |
| `conv_get_current` | `{}` | `{topic, role, messages}` | 当前状态 |

### 5.6 Reasoning Tools

| 工具名 | 输入 | 输出 | 说明 |
|--------|------|------|------|
| `iper_execute` | `{intent, planner, max_reflections}` | `{result, status}` | IPER 执行 |
| `try_catch_execute` | `{try_path, catch_path, snapshot}` | `{success, path}` | Try-Catch |
| `counterfactual_compare` | `{base, variants, evaluator}` | `{comparison}` | 反事实推理 |
| `graph_hypothesize` | `{question, kg_context, max}` | `{hypotheses}` | 图引导假设 |

---

## 六、权限模型

### 6.1 Memory Permissions

| 权限声明 | 说明 |
|----------|------|
| `memory: state_write` | 写入 `memory.state.*` |
| `memory: state_read` | 读取 `memory.state.*` |
| `memory: state_delete` | 删除 `memory.state.*` |
| `memory: state_query` | 查询键模式 |
| `memory: vector_search` | 向量语义检索 |
| `memory: profile_update` | 更新用户画像 |
| `memory: profile_read` | 读取用户画像 |

### 6.2 KG Permissions

| 权限声明 | 说明 |
|----------|------|
| `kg: subgraph_query` | 查询图谱 |
| `kg: subgraph_write` | 写入图谱 |
| `kg: read_ontology` | 读取 Ontology |
| `kg: write_ontology` | 注册 Ontology |
| `kg: navigate` | Meta-KG 导航 |
| `kg: write: global` | 写入全局 KG（需特殊权限）|

### 6.3 Reasoning Permissions

| 权限声明 | 说明 |
|----------|------|
| `generate_subgraph: { max_depth: N }` | 生成子图深度限制 |
| `reasoning: llm_generate` | LLM 生成能力 |
| `reasoning: structured_generate` | 结构化输出 |
| `reasoning: stream_output` | 流式输出 |

---

## 七、LayeredContext 集成

```
┌─────────────────────────────────────────────────────────────┐
│ L1 System: agent_prompt, tool_definitions                  │
│   └─ 永不压缩/丢弃                                          │
├─────────────────────────────────────────────────────────────┤
│ L2 Recent: recent_turns                                     │
│   └─ 由 ADR-007 ContextCompressor 管理                       │
├─────────────────────────────────────────────────────────────┤
│ L3 Archive: archive, memory.kg.*, memory.vector.*            │
│   └─ 压缩归档 + 知识图谱 + 向量存储                           │
├─────────────────────────────────────────────────────────────┤
│ L4 Working: working.data, memory.state.*, memory.profile.*  │
│   └─ 活跃状态 + 键值记忆 + 用户画像                           │
├─────────────────────────────────────────────────────────────┤
│ L5 Meta: schema_version, task_id, total_turns               │
│   └─ 元数据（只读）                                          │
└─────────────────────────────────────────────────────────────┘
```

---

## 八、ADR 索引

| ADR | 标题 | 状态 |
|-----|------|------|
| ADR-0010 | 记忆系统标准接口 | ✅ 已批准 |
| ADR-0011 | 知识图谱与 Meta-KG 导航 | ✅ 已批准 |
| ADR-0012 | 向量语义记忆 | ✅ 已批准 |
| ADR-0013 | 用户画像管理 | ✅ 已批准 |
| ADR-0014 | 对话上下文隔离 | ✅ 已批准 |
| ADR-0015 | IPER 闭环推理 | ✅ 已批准 |
| ADR-0016 | 异常自动快照回溯 | ✅ 已批准 |
| ADR-0017 | 反事实推理 | ✅ 已批准 |
| ADR-0018 | 图引导假设生成 | ✅ 已批准 |

---

## 九、待决策事项

1. **向量库选型**：Qdrant vs FAISS vs ChromaDB
2. **KG 后端选型**：Graphiti vs Cognee vs Neo4j
3. **Embedding 模型**：默认模型选型
4. **Meta-KG 自动维护频率**：定期同步间隔

---

*最后更新: 2026-05-13*