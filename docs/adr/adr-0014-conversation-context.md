# ADR-0014: 对话上下文隔离

## 状态

**已批准** (2026-05-13)

## 背景

HydraForge Phase 2 需要支持多角色/多话题的结构化上下文隔离机制。在多 Agent 协作场景中：

- **多角色会议**：Agent + User + 其他角色同时参与
- **多话题隔离**：用户同时进行多个独立任务
- **角色上下文切换**：Agent 需要根据角色切换上下文

**参考系统**：
- LangChain：`start_topic`, `switch_role`, `meeting`
- AgenticOS：Social Orchestration Layer（Layer 4.5）

---

## 决策

### 1. 设计原则

- **话题隔离**：每个话题有独立的上下文路径
- **角色上下文**：不同角色看到不同的上下文视图
- **会议共享**：会议中有共享上下文 + 私有上下文

### 2. 子图定义

#### `/lib/conversation/start_topic@v1`

```yaml
AgenticDSL `/lib/conversation/start_topic@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: true
    - name: user_id
      type: string
      required: true
    - name: topic_id
      type: string
      required: true
      description: "话题唯一标识"
    - name: initial_context
      type: object
      required: false
  outputs:
    - name: context_path
      type: string
      description: "话题上下文路径，如 /topics/booking/context"
version: "1.0"
stability: stable
permissions:
  - memory: state_write
```

#### `/lib/conversation/switch_role@v1`

```yaml
AgenticDSL `/lib/conversation/switch_role@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: true
    - name: user_id
      type: string
      required: true
    - name: role_id
      type: string
      required: true
  outputs:
    - name: context_path
      type: string
version: "1.0"
stability: stable
permissions:
  - memory: state_write
```

#### `/lib/conversation/meeting@v1`

```yaml
AgenticDSL `/lib/conversation/meeting@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: true
    - name: user_id
      type: string
      required: true
    - name: meeting_id
      type: string
      required: true
    - name: participants
      type: array
      required: true
      description: "参与者角色列表"
    - name: interaction_mode
      type: string
      enum: [round_robin, free_discussion, qa_session]
      default: free_discussion
      description: |
        round_robin: 轮询模式
        free_discussion: 自由讨论
        qa_session: 问答模式
  outputs:
    - name: meeting_summary
      type: object
version: "1.0"
stability: stable
permissions:
  - memory: state_write
  - kg: subgraph_write
```

#### `/lib/conversation/get_current@v1`

```yaml
AgenticDSL `/lib/conversation/get_current@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: true
    - name: user_id
      type: string
      required: true
  outputs:
    - name: active_topic
      type: string
    - name: active_role
      type: string
    - name: recent_messages
      type: array
version: "1.0"
stability: stable
permissions:
  - memory: state_read
```

### 3. 上下文组织模型

```
memory.conversation.{agent_id}.{user_id}.topics.{topic_id}  # 话题上下文
memory.conversation.{agent_id}.{user_id}.roles.{role_id}   # 角色上下文
memory.conversation.{agent_id}.{user_id}.meetings.{meeting_id}  # 会议上下文
```

### 4. 权限模型

| 权限声明 | 说明 |
|----------|------|
| `memory: state_write` | 写入对话上下文 |
| `memory: state_read` | 读取对话上下文 |
| `kg: subgraph_write` | 会议记录写入 KG |

---

## 参考

- [ADR-0010: 记忆系统标准接口](./adr-0010-memory-system.md)