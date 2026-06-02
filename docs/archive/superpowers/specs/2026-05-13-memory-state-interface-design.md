# HydraForge L4 Working Memory Interface Design

> **⚠️ 归档说明 (2026-06-03)**：本文档为 ADR-0010 (记忆系统标准接口) 的前置设计稿，已被 ADR-0010 正式批准并取代。**关键内容（3 方案对比）已迁移至 `docs/adr/adr-0010-memory-system.md` "替代方案" 章节**。归档至 `docs/archive/superpowers/specs/`，仅供历史参考。

> **作者**: Oracle  
> **日期**: 2026-05-13  
> **版本**: 1.0  
> **关联 ADR**: ADR-8 (LayeredContext), ADR-9 (DSL Standard Library)  
> **状态**: 设计评审中

---

## 1. 概述

本文档定义 HydraForge `/lib/memory/state/**` 子图接口设计，为 L4 Working 层提供结构化的内存操作能力。

### 1.1 设计目标

| 目标 | 说明 |
|------|------|
| **分层抽象** | 低/中/高三层 API，覆盖从简单 KV 到语义检索的全场景 |
| **行业最佳实践** | 融合 Mem0 画像持久化、MemGPT 分层压缩、LangChain MessageHistory、LlamaIndex 层次索引 |
| **DSL 原生** | 所有接口以子图签名暴露，内部通过 `tool_call` 调用底层工具 |
| **向后兼容** | 保留现有 `state.read`/`state.write`，平滑迁移 |
| **增量实现** | Phase 1 可用（1-2 天），Phase 2 扩展（语义搜索） |

### 1.2 核心洞察

从行业研究中提取的关键洞察：

- **Mem0**: 用户画像（profile）和偏好（preference）需要持久化，且应当有独立命名空间
- **MemGPT**: 记忆应当按作用域分层（working/recent/archive），并支持重要性评估和压缩
- **LangChain**: MessageHistory 需要显式的读写接口，CondensedMemoryChain 需要批量查询能力
- **LlamaIndex**: 层次索引结构意味着内存查询应当支持前缀匹配和范围扫描

---

## 2. 方案对比

| # | 方案 | 优点 | 缺点 | 工作量 |
|---|------|------|------|--------|
| **A** | **Flat KV 扩展** — 仅扩展 `state.read`/`state.write`，增加 `delete` + `batch` | 完全向后兼容；仅需新增 1-2 个工具 | 无组织、无作用域、无语义能力；未利用 MemGPT/LlamaIndex 洞察 | Short (2-4h) |
| **B** | **分层命名空间** — 强制 `user.*`/`task.*`/`agent.*` 路径约定，提供类型化访问 | 清晰的归属关系；作用域隔离；契合 MemGPT 分层思想 | 需要路径验证逻辑；无查询/发现层；DSL 子图实现冗长 | Medium (1-2d) |
| **C** | **混合分层 API** — 低层 KV + 中层类型化命名空间 + 高层语义查询（Phase 2） | 覆盖全场景；渐进式采用；匹配行业最佳实践；向后兼容 | 需要注册更多工具；类型层需要 Schema 定义 | Medium (1-2d) Phase 1; Large (3d+) Phase 2 |

### 2.1 推荐方案: C（混合分层 API）

**理由**:
1. HydraForge L4 Working 是唯一工具可写层，必须同时服务三种场景：通用工具状态、结构化用户数据、语义检索
2. 单一抽象无法同时满足这三种需求（MemGPT 使用多层，LangChain 使用 Chain 组合）
3. 混合方案每层都是可选的——简单工具继续使用 `state.read`/`state.write`，复杂场景使用类型化接口
4. Phase 1 即可实现低层和中层，语义搜索作为 Phase 2 扩展，不阻塞主线开发

---

## 3. L4 Working 内存组织模型

```
working.data:                          # L4 Working — 唯一工具可写区域
├── __meta__/                          # 内存系统元数据
│   ├── schema_version: "1.0"
│   ├── namespaces: ["user", "task", "agent", "temp"]
│   └── indices: {...}                 # Phase 2: 语义索引元数据
│
├── user/                              # 用户作用域（跨会话持久化候选）
│   ├── profile/                       # 用户画像（Mem0 风格）
│   │   ├── name: string
│   │   ├── role: string
│   │   └── expertise: list<string>
│   ├── preferences/                   # 用户偏好（Mem0 风格）
│   │   ├── language: string = "zh-CN"
│   │   ├── style: string = "concise"
│   │   └── format: string = "markdown"
│   └── history/                       # 跨任务历史摘要（由 L3 压缩器写入）
│       └── summaries: list<{
│             task_id: string,
│             summary: string,
│             timestamp: string
│       }>
│
├── task/                              # 任务作用域（当前会话内）
│   ├── state/                         # 当前任务状态
│   ├── checkpoints/                   # 命名检查点
│   │   └── <checkpoint_name>: map<string, any>
│   └── results/                       # 中间结果
│
├── agent/                             # Agent 作用域（per-agent 配置）
│   ├── context/                       # Agent 维护的上下文
│   └── learned/                       # Agent 学习到的模式
│
└── temp/                              # 非结构化临时存储
    └── <任意键>: any
```

**路径约定**:
- `working.data.user.profile.name` → 用户画像字段
- `working.data.task.state.current_step` → 任务状态
- `working.data.agent.context.*` → Agent 上下文
- `working.data.temp.*` → 无类型临时存储

**作用域语义**:
| 命名空间 | 生命周期 | 写入者 | 示例 |
|----------|----------|--------|------|
| `user.*` | 跨会话（持久化候选） | 认证/初始化子图 | `user.profile.name` |
| `task.*` | 单会话 | 工作流子图 | `task.state.step_3_done` |
| `agent.*` | 单会话（Agent 实例级） | 认知子图 | `agent.context.focus_topic` |
| `temp.*` | 任意 | 任意工具 | `temp.calculation_buffer` |

---

## 4. 核心内存操作定义

### 4.1 Phase 1: 基础操作（立即实现）

这些操作通过注册到 `ToolRegistry` 的底层工具实现，再由 DSL 子图封装为签名接口。

#### 4.1.1 底层工具接口（C++ 实现）

| 工具名 | 签名 | 实现说明 |
|--------|------|----------|
| `state.read` | `(path: string, default: any = null) -> value: any` | **已存在**，扩展 `default` 参数 |
| `state.write` | `(path: string, value: any, ttl: int = 0) -> success: bool` | **已存在**，扩展 `ttl` 参数（ttl=0 表示无过期） |
| `memory.delete` | `(path: string) -> success: bool` | **新增**，删除指定路径 |
| `memory.query` | `(prefix: string, limit: int = 50) -> keys: list<string>` | **新增**，按前缀列出键（如 `user.profile.*`） |
| `memory.batch` | `(operations: list<BatchOp>) -> results: list<any>` | **新增**，原子批量操作 |
| `memory.exists` | `(path: string) -> exists: bool` | **新增**，检查路径是否存在 |

**BatchOp Schema**:
```json
{
  "op": "read" | "write" | "delete",
  "path": "string",
  "value": "any"    // 仅 write 需要
}
```

#### 4.1.2 子图签名（DSL 暴露层）

```markdown
### /lib/memory/state/read
graph_type: subgraph
signature: "(path: string, default: any = null) -> value: any"
description: "从 working memory 读取指定路径的值"
permissions: []

### /lib/memory/state/write
graph_type: subgraph
signature: "(path: string, value: any, ttl: int = 0) -> success: bool"
description: "写入值到 working memory，支持 TTL"
permissions: []

### /lib/memory/state/delete
graph_type: subgraph
signature: "(path: string) -> success: bool"
description: "删除指定路径"
permissions: []

### /lib/memory/state/query
graph_type: subgraph
signature: "(prefix: string, limit: int = 50) -> keys: list<string>"
description: "按前缀查询键列表"
permissions: []

### /lib/memory/state/batch
graph_type: subgraph
signature: "(operations: list<BatchOp>) -> results: list<any>"
description: "原子批量操作"
permissions: []

### /lib/memory/state/exists
graph_type: subgraph
signature: "(path: string) -> exists: bool"
description: "检查路径是否存在"
permissions: []
```

### 4.2 Phase 2: 高级操作（语义搜索 + 版本控制）

| 工具名 | 签名 | 说明 |
|--------|------|------|
| `memory.semantic.search` | `(query: string, namespace: string = "*", top_k: int = 5) -> memories: list<MemoryHit>` | 基于嵌入的语义搜索 |
| `memory.semantic.add` | `(content: string, namespace: string = "default", tags: list<string> = []) -> id: string` | 添加到语义记忆 |
| `memory.versioned.write` | `(path: string, value: any, expected_version: int) -> {success: bool, version: int}` | 乐观锁写入 |

**MemoryHit Schema**:
```json
{
  "id": "string",
  "content": "string",
  "namespace": "string",
  "score": "float",
  "metadata": "map<string, any>"
}
```

---

## 5. 类型化内存子图（中层 API）

类型化子图在底层工具之上增加**路径约定和语义封装**，让 DSL 作者以领域语言操作内存。

### 5.1 User Profile (`/lib/memory/profile/*`)

基于 Mem0 用户画像模式：

```markdown
### AgenticDSL `/lib/memory/profile/get`
graph_type: subgraph
signature: "(key: string, default: any = null) -> value: any"
description: "获取用户画像字段（如 name, role, expertise）"
permissions: []

nodes:
  - id: read_profile
    type: tool_call
    tool: state.read
    arguments:
      path: "working.data.user.profile.{{ inputs.key }}"
      default: "{{ inputs.default }}"
    output_keys: ["value"]
    next: ["/end_soft"]

### AgenticDSL `/lib/memory/profile/set`
graph_type: subgraph
signature: "(key: string, value: any) -> success: bool"
description: "设置用户画像字段"
permissions: []

nodes:
  - id: write_profile
    type: tool_call
    tool: state.write
    arguments:
      path: "working.data.user.profile.{{ inputs.key }}"
      value: "{{ inputs.value }}"
    output_keys: ["success"]
    next: ["/end_soft"]

### AgenticDSL `/lib/memory/profile/get_all`
graph_type: subgraph
signature: "() -> profile: map<string, any>"
description: "获取完整用户画像"
permissions: []

nodes:
  - id: query_profile
    type: tool_call
    tool: memory.query
    arguments:
      prefix: "working.data.user.profile."
      limit: 100
    output_keys: ["keys"]
    next: ["/lib/memory/profile/get_all/batch_read"]

  - id: batch_read
    type: tool_call
    tool: memory.batch
    arguments:
      operations: "{{ keys | map(path => {op: 'read', path: path}) }}"
    output_keys: ["profile"]
    next: ["/end_soft"]
```

### 5.2 User Preference (`/lib/memory/preference/*`)

基于 Mem0 偏好持久化模式：

```markdown
### AgenticDSL `/lib/memory/preference/get`
graph_type: subgraph
signature: "(key: string, default: any = null) -> value: any"
description: "获取用户偏好（如 language, style, format）"
permissions: []

### AgenticDSL `/lib/memory/preference/set`
graph_type: subgraph
signature: "(key: string, value: any, persist: bool = true) -> success: bool"
description: |
  设置用户偏好。persist=true 表示跨会话持久化候选，
  由 L5 持久化层在会话结束时处理。
permissions: []

nodes:
  - id: write_pref
    type: tool_call
    tool: state.write
    arguments:
      path: "working.data.user.preferences.{{ inputs.key }}"
      value: "{{ inputs.value }}"
    output_keys: ["success"]
    next: ["/end_soft"]
```

### 5.3 Task State (`/lib/memory/task/*`)

基于 LangChain 任务状态管理模式：

```markdown
### AgenticDSL `/lib/memory/task/save`
graph_type: subgraph
signature: "(name: string, include: list<string> = []) -> checkpoint_id: string"
description: |
  保存当前任务状态为命名检查点。
  include=[] 表示保存 task.state.* 全部；
  include=["step1", "step2"] 表示仅保存指定键。
permissions: []

### AgenticDSL `/lib/memory/task/load`
graph_type: subgraph
signature: "(name: string) -> state: map<string, any>"
description: "从命名检查点恢复任务状态"
permissions: []

### AgenticDSL `/lib/memory/task/clear`
graph_type: subgraph
signature: "() -> success: bool"
description: "清空 task 命名空间（会话结束清理）"
permissions: []
```

---

## 6. L1-L5 层集成点

```
┌─────────────────────────────────────────────────────────────────┐
│  DSL 子图调用内存操作时的层交互                                   │
├─────────────────────────────────────────────────────────────────┤
│  /lib/memory/profile/get                                        │
│    ├─ 读取 ← L4 working.data.user.profile.*  （目标层）          │
│    └─ 只读 ← L1 system.agent_prompt  （上下文参考）               │
│                                                                 │
│  /lib/memory/task/save                                          │
│    ├─ 读取 ← L4 working.data.task.state.*  （源数据）            │
│    ├─ 写入 → L4 working.data.task.checkpoints.*  （目标）        │
│    └─ 读取 ← L5 meta.task_id  （作用域参考）                      │
│                                                                 │
│  /lib/memory/semantic/search (Phase 2)                          │
│    ├─ 查询 ← L4 working.data.user.history.*  （长期记忆）         │
│    ├─ 查询 ← L2 recent_turns  （近期上下文）                      │
│    └─ 查询 ← L3 archive  （压缩历史）                             │
└─────────────────────────────────────────────────────────────────┘
```

### 6.1 集成规则

| 层 | 角色 | 内存操作权限 |
|----|------|-------------|
| **L1 System** | Agent 配置、工具定义 | 内存子图可 **读取** 作为上下文参考，**禁止写入** |
| **L2 Recent** | 最近 5 轮对话 | 内存子图可 **读取** 用于语义查询上下文，**禁止写入** |
| **L3 Archive** | 压缩历史 | 内存子图可 **读取** 用于长期记忆检索，**禁止写入** |
| **L4 Working** | 活跃状态 | **唯一可写层**。`user/`, `task/`, `agent/`, `temp/` 命名空间 |
| **L5 Meta** | 任务 ID、轮次、时间戳 | 内存子图可 **读取** `meta.task_id` 用于作用域隔离 |

### 6.2 L3 压缩器集成

ADR-7 ContextCompressor 应将压缩后的对话摘要写入：
```json
{
  "working.data.user.history.summaries": [
    {
      "task_id": "task_123",
      "summary": "用户要求分析代码库，已完成模块 A 和 B",
      "timestamp": "2026-05-13T10:00:00Z",
      "turn_range": [1, 15]
    }
  ]
}
```

这使 `memory.query(prefix="working.data.user.history.")` 能检索跨任务历史。

---

## 7. 类型化 vs 非类型化：双接口策略

### 7.1 非类型化（逃生舱）

适合通用工具和临时状态：

```yaml
# 通用工具不知道 Schema，直接使用底层路径
tool: state.write
arguments:
  path: "working.data.temp.my_tool_buffer"
  value: {"partial": "result"}
```

### 7.2 类型化（推荐）

适合已知模式，提供文档和验证：

```yaml
# 结构化访问 — 路径约定封装在子图内
tool: memory.profile.get
arguments:
  key: "name"

tool: memory.preference.set
arguments:
  key: "language"
  value: "zh-CN"
  persist: true
```

### 7.3 选择指南

| 场景 | 推荐接口 | 理由 |
|------|----------|------|
| 通用工具写入临时数据 | `state.write` | 无需定义 Schema，即写即用 |
| 用户画像/偏好读写 | `memory.profile.*` / `memory.preference.*` | 路径约定内置，语义清晰 |
| 任务状态保存/恢复 | `memory.task.*` | 封装检查点逻辑，避免路径错误 |
| 批量操作 | `memory.batch` | 原子性，减少工具调用次数 |
| 发现/查询所有键 | `memory.query` | 前缀扫描，类似 LlamaIndex 索引 |

---

## 8. 目录结构与文件清单

```
lib/
└── memory/
    ├── README.md                    # 内存子图使用指南
    │
    ├── state/                       # 底层状态操作（通用 KV）
    │   ├── read.md                  # (path, default?) -> value
    │   ├── write.md                 # (path, value, ttl=0) -> success
    │   ├── delete.md                # (path) -> success
    │   ├── query.md                 # (prefix, limit=50) -> keys[]
    │   ├── batch.md                 # (operations[]) -> results[]
    │   └── exists.md                # (path) -> bool
    │
    ├── profile/                     # 用户画像（Mem0 风格）
    │   ├── get.md                   # (key, default?) -> value
    │   ├── set.md                   # (key, value) -> success
    │   └── get_all.md               # () -> profile: map
    │
    ├── preference/                  # 用户偏好（Mem0 风格）
    │   ├── get.md                   # (key, default?) -> value
    │   └── set.md                   # (key, value, persist=true) -> success
    │
    ├── task/                        # 任务状态（LangChain 风格）
    │   ├── save.md                  # (name, include=[]) -> checkpoint_id
    │   ├── load.md                  # (name) -> state: map
    │   └── clear.md                 # () -> success
    │
    └── semantic/                    # Phase 2: 语义记忆
        ├── search.md                # (query, namespace="*", top_k=5) -> memories[]
        └── add.md                   # (content, namespace, tags=[]) -> id
```

---

---

## 8.5 实现注意事项（代码审查发现）

### 8.5.1 工具参数传递机制

经审查 `NodeExecutor::execute_tool_call` 实现，工具调用参数通过 `InjaTemplateRenderer` 渲染后，以 `std::unordered_map<std::string, std::string>` 形式传递给 `ToolRegistry::call_tool`。

**影响**：
- 简单参数（string, int, bool）可直接传递和解析
- 复杂参数（list, map, object）需要 **JSON 序列化**为字符串传递

**示例**：
```yaml
# DSL 子图中的 batch 调用
tool: memory.batch
arguments:
  operations: '[{"op":"read","path":"user.profile.name"},{"op":"write","path":"user.preferences.lang","value":"zh"}]'
```

**C++ 工具实现示例**：
```cpp
register_tool("memory.batch", [](const auto& args) -> nlohmann::json {
    auto ops_it = args.find("operations");
    if (ops_it == args.end()) {
        return nlohmann::json{{"error", "Missing operations"}};
    }
    
    // 解析 JSON 字符串
    auto operations = nlohmann::json::parse(ops_it->second);
    std::vector<nlohmann::json> results;
    
    for (const auto& op : operations) {
        std::string op_type = op["op"];
        std::string path = op["path"];
        
        if (op_type == "read") {
            results.push_back(state_manager.read(path));
        } else if (op_type == "write") {
            state_manager.write(path, op["value"]);
            results.push_back(true);
        } else if (op_type == "delete") {
            state_manager.del(path);
            results.push_back(true);
        }
    }
    
    return nlohmann::json{{"results", results}};
});
```

### 8.5.2 `default` 参数实现

`state.read` 的 `default` 参数传递的是 JSON 编码的默认值字符串。工具实现时：
1. 尝试读取指定路径
2. 如果路径不存在或值为 null，解析 `default` 参数并返回
3. 如果 `default` 未提供，返回 null

### 8.5.3 `ttl` 参数实现

`state.write` 的 `ttl` 参数为 advisory：
- `ttl=0`: 无过期（默认）
- `ttl>0`: 建议在 N 秒后过期
- 实际清理由会话管理器或后台线程执行
- 过期数据写入时打上 `__expires_at` 时间戳

## 9. 实现路线图

### Phase 1: 基础能力（1-2 天）

1. **注册新工具**（C++ 侧）:
   - `memory.delete` — 删除 working.data 路径
   - `memory.query` — 遍历 working.data 子树，前缀匹配
   - `memory.batch` — 循环执行 read/write/delete，返回结果列表
   - `memory.exists` — 检查 `working.data` 中是否存在路径

2. **创建子图**（DSL 侧）:
   - `/lib/memory/state/*` — 6 个子图
   - `/lib/memory/profile/*` — 3 个子图
   - `/lib/memory/preference/*` — 2 个子图
   - `/lib/memory/task/*` — 3 个子图

3. **扩展现有工具**:
   - `state.read`: 增加 `default` 参数
   - `state.write`: 增加 `ttl` 参数（ advisory，实际驱逐由会话管理器处理）

4. **测试**:
   - 验证路径前缀权限（`user.*` vs `task.*` vs `temp.*`）
   - 验证批量操作原子性
   - 验证 query 前缀匹配

### Phase 2: 语义搜索（3+ 天）

1. **嵌入模型集成**: 在 `LlamaAdapter` 或独立组件中增加 embedding 生成能力
2. **语义索引**: 在 `__meta__.indices` 中维护路径→embedding 映射
3. **语义工具**: 注册 `memory.semantic.search` / `memory.semantic.add`
4. **语义子图**: 创建 `/lib/memory/semantic/*`

### Phase 3: 持久化（可选，依赖 L5 设计）

1. **跨会话持久化**: `user.*` 数据在会话结束时序列化到磁盘
2. **版本控制**: 实现 `memory.versioned.write` 乐观锁
3. **TTL 实现**: 后台线程或惰性清理过期数据

---

## 10. 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| **路径冲突** | 不同子图写入同一路径导致数据覆盖 | 强制命名空间约定；`user/`/`task/`/`agent/` 前缀隔离 |
| **内存膨胀** | L4 Working 无限增长 | TTL 参数；`task.clear` 子图；L3 压缩器定期归档 |
| **权限绕过** | 工具通过 `state.write` 写入只读层 | `state.write` 内部检查路径前缀，拒绝 `system.*`/`recent.*` 等写入 |
| **Schema 漂移** | 类型化子图假设的 Schema 与实际数据不一致 | Phase 1 不强制 Schema；子图使用 `default` 参数优雅降级 |

---

## 11. 开放问题

1. **跨会话持久化**: `user.*` 数据是否应在会话结束后持久化到磁盘？这会影响 `persist` 参数的语义实现。
2. **Phase 1 语义搜索**: 如果无法集成 embedding 模型，是否先用关键词搜索（BM25）作为语义搜索的降级方案？
3. **批量操作原子性**: `memory.batch` 是否需要严格原子性（全部成功或全部回滚），还是部分成功即可？

---

## 附录 A: 与行业系统的映射

| 本设计组件 | Mem0 | MemGPT | LangChain | LlamaIndex |
|-----------|------|--------|-----------|------------|
| `memory.profile.*` | User Profile | Core Memory (Persona) | — | — |
| `memory.preference.*` | Preferences | — | — | — |
| `memory.task.*` | — | Working Memory | MessageHistory | — |
| `memory.query` | — | — | — | Index Query |
| `memory.semantic.*` (Phase 2) | Semantic Search | Recall Memory | VectorStore | VectorIndex |
| L3 压缩器 | — | Compression | CondensedMemoryChain | SummaryIndex |

## 附录 B: 完整子图签名速查

```
/lib/memory/state/read        (path: string, default: any = null) -> value: any
/lib/memory/state/write       (path: string, value: any, ttl: int = 0) -> success: bool
/lib/memory/state/delete      (path: string) -> success: bool
/lib/memory/state/query       (prefix: string, limit: int = 50) -> keys: list<string>
/lib/memory/state/batch       (operations: list<BatchOp>) -> results: list<any>
/lib/memory/state/exists      (path: string) -> exists: bool

/lib/memory/profile/get       (key: string, default: any = null) -> value: any
/lib/memory/profile/set       (key: string, value: any) -> success: bool
/lib/memory/profile/get_all   () -> profile: map<string, any>

/lib/memory/preference/get    (key: string, default: any = null) -> value: any
/lib/memory/preference/set    (key: string, value: any, persist: bool = true) -> success: bool

/lib/memory/task/save         (name: string, include: list<string> = []) -> checkpoint_id: string
/lib/memory/task/load         (name: string) -> state: map<string, any>
/lib/memory/task/clear        () -> success: bool

# Phase 2
/lib/memory/semantic/search   (query: string, namespace: string = "*", top_k: int = 5) -> memories: list<MemoryHit>
/lib/memory/semantic/add      (content: string, namespace: string = "default", tags: list<string> = []) -> id: string
```
