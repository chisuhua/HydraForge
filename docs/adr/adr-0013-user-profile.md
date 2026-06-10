# ADR-0013: 用户画像管理
> ⛔ **已废弃 (2026-06-09)** — 代码侧 0 命中,仅作设计历史保留。详见 OpenSpec change `tech-debt-and-doc-cleanup`
## 状态

**❌ 未实施** (2026-05-13, 2026-06-09 标注废弃)

## 背景

HydraForge Phase 2 需要支持用户画像持久化，使 Agent 能够跨会话记忆用户信息，提供个性化服务。典型的用户画像包括：

- **基本信息**：姓名、邮箱、公司、角色
- **偏好设置**：语言偏好、交互风格、通知设置
- **上下文**：当前项目、关注领域、常用工具

**参考系统**：
- Mem0：用户画像管理
- LangChain：profile_update / profile_get

---

## 决策

### 1. 命名空间设计

```
memory.profile.{agent_id}.{user_id}.*
         │          │         │
         │          │         └─ 用户私有画像
         │          └─ Agent 级别画像（Agent 配置）
         └─ 画像命名空间
```

### 2. 子图定义

#### `/lib/memory/profile/update@v1`

```yaml
AgenticDSL `/lib/memory/profile/update@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: true
    - name: user_id
      type: string
      required: true
    - name: attributes
      type: object
      required: true
      description: "要更新的属性键值对"
  outputs:
    - name: success
      type: boolean
    - name: updated_fields
      type: array
version: "1.0"
stability: stable
permissions:
  - memory: profile_update
```

#### `/lib/memory/profile/get@v1`

```yaml
AgenticDSL `/lib/memory/profile/get@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: true
    - name: user_id
      type: string
      required: true
    - name: fields
      type: array
      required: false
      description: "指定要获取的字段（为空返回全部）"
  outputs:
    - name: profile
      type: object
version: "1.0"
stability: stable
permissions:
  - memory: profile_read
```

#### `/lib/memory/profile/merge@v1`

```yaml
AgenticDSL `/lib/memory/profile/merge@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: true
    - name: user_id
      type: string
      required: true
    - name: source_profile
      type: object
      required: true
      description: "要合并的来源偏好"
    - name: merge_strategy
      type: string
      enum: [overwrite, prefer_new, prefer_old]
      default: prefer_new
      description: |
        overwrite: 完全覆盖
        prefer_new: 新值优先
        prefer_old: 旧值优先
  outputs:
    - name: success
      type: boolean
    - name: merged_profile
      type: object
version: "1.0"
stability: experimental
permissions:
  - memory: profile_update
```

### 3. 权限模型

| 权限声明 | 说明 |
|----------|------|
| `memory: profile_update` | 更新用户画像 |
| `memory: profile_read` | 读取用户画像 |

### 4. 与 LayeredContext 的集成

```
L4 Working: memory.state.*              (键值状态，ADR-0010)
            memory.profile.*           (用户画像，本 ADR)
            memory.kg.*                (知识图谱，ADR-0011)
            memory.vector.*            (向量存储，ADR-0012)
```

### 5. 工具注册要求

执行器必须预注册以下工具：

| 工具名 | 输入 | 输出 | 说明 |
|--------|------|------|------|
| `profile_update` | `{user_id, attributes}` | `{success, fields}` | 更新画像 |
| `profile_get` | `{user_id, fields}` | `{profile}` | 读取画像 |
| `profile_merge` | `{user_id, source, strategy}` | `{success, profile}` | 合并画像 |

---

## 后果

### 正面

- 支持跨会话用户偏好持久化
- 简单的键值模型，易于实现
- 与其他记忆系统独立

### 负面

- 无语义检索能力（需要向量库）
- 缺乏复杂偏好推理

---

## 参考

- [Mem0: Memory system for AI](https://github.com/mem0ai/mem0)
- [ADR-0010: 记忆系统标准接口](./adr-0010-memory-system.md)