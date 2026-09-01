# ADR-0079: 统一会话模型与 4-Scope 存储

## 状态
✅ Approved（v1.1 amendment 2026-08-12，v1.2 amendment 2026-08-20，原始 v1 文本 2026-01-19）

> **修订记录**：v1.1 amendment 针对 Agent 蒸馏需求修订附录 A（step schema 加 `event_ref`），
> 在 §上下文加现行 defect 声明（`SessionStore::trunc` bug 已修复）。
> **v1.2 amendment 2026-08-20**：§决策 D7-D10（node-id 稳定寻址 / branch cursor 持久化 / path-extraction fork / 4 套存储命名空间分配）。
> **v1 决策不变**——4-Scope 模型、ConvergenceEntry、文件布局全部保留。

## 上下文

### 当前会话存储的碎片化

SessionManager 当前维护两套独立存储：
1. **JSONL 格式**（`~/.hydraforge/sessions/<session_id>.jsonl`）— 存储 conversation 消息流
2. **JSON 格式**（`session_meta.json`）— 存储 session 元数据（创建时间、分支树、配置）

**问题**：
- 两套存储无法原子更新（一个写成功、另一个失败 → 不一致）
- 恢复依赖两个文件都存在
- 事件流（attempt / branch / phase）与消息流割裂
- 无统一的"会话发生了什么"视图

### 4 个语义层级混杂

当前代码中 4 个作用域概念混杂：
- **Conversation**（对话）：用户与 agent 的完整交互历史
- **Attempt**（尝试）：一次完整的 LLM planning → execution → verification 循环
- **Step**（步骤）：单次 LLM 推理 + 工具执行
- **Execution**（执行）：单张 DSL 图的一次调度执行

缺乏统一的层级定义和存储格式。

### ADR-0020/0008 遗留架构债

- ADR-0020（线程模型）提到 CognitiveWorker 独立会话，但未定义会话边界
- ADR-0008（LayeredContext）5 层结构与会话层级无映射关系
- Session 类只有 TaskSession/SubtaskSession，无 Conversation/Attempt 层

## 决策

### 决策 D1：统一存储为单一 JSONL 流

**采用**：Single JSONL file per conversation，所有层级事件和元数据写入同一文件。

**格式**：
```jsonl
{"v":1, "type":"session_meta", "session_id":"...", "created_at":"...", "config":{...}}
{"v":1, "type":"conversation", "role":"user", "content":"...", "ts":...}
{"v":1, "type":"attempt", "attempt_id":"...", "branch_id":"...", "started_at":...}
{"v":1, "type":"step", "step_id":"...", "llm_request":{...}, "tool_results":[...]}
{"v":1, "type":"execution", "graph_execution_id":"...", "node_count":5, "result":{...}}
{"v":1, "type":"convergence", "selected_attempt":"...", "reason":"...", "alternatives":[...]}
```

**优势**：
- 原子性：单文件写入，要么全成功要么全失败
- 时序一致：所有事件按发生顺序追加
- 恢复简单：从头读 JSONL 即可重建完整状态
- 支持流式读取：tail -f 实时观察

### 决策 D2：4-Scope 层级模型

定义 4 个语义层级，从外到内：

#### Scope 1: Conversation（对话）
- **定义**：用户与 agent 的完整交互会话，包含所有 user/assistant 消息、分支、重试
- **标识符**：`session_id`（现有）
- **边界**：用户显式创建/关闭，或超时（默认 24h 无活动）
- **JSONL 记录**：`{"type":"conversation", "role":"user"|"assistant", "content":"..."}`
- **分支语义**：fork 创建新 `session_id`，继承父会话上下文

#### Scope 2: Attempt（尝试）
- **定义**：单次"规划 → 执行 → 验证"完整循环（对应 PlanExecuteLoop 一轮，或 ReactLoop 一轮）
- **标识符**：`attempt_id`（新增，格式：`attempt-<timestamp>-<counter>`）
- **边界**：attempt.started → attempt.ended 事件对
- **JSONL 记录**：`{"type":"attempt", "attempt_id":"...", "branch_id":"...", "phase":"plan"|"execute"|"verify"}`
- **并发语义**：ForkJoinLoop 可同时产生多个 attempt（共享 conversation，独立 attempt_id）
- **收敛语义**：ConvergenceEntry 选择最佳 attempt，其余标记 discarded

#### Scope 3: Step（步骤）
- **定义**：单次 LLM 推理 + 立即跟随的工具调用序列（不可分割）
- **标识符**：`step_id`（新增，格式：`step-<attempt_id>-<counter>`）
- **边界**：LLM generate() 调用开始 → 所有 tool call 返回
- **JSONL 记录**：`{"type":"step", "step_id":"...", "llm_request":{...}, "tool_results":[...]}`
- **不可 fork**：Step 是原子单元，不能在 step 中途分支

#### Scope 4: Execution（执行）
- **定义**：单张 DSL 图的一次 TopoScheduler::execute() 调用
- **标识符**：`graph_execution_id`（新增，格式：`gexec-<timestamp>-<counter>`）
- **边界**：execute() 入口 → 所有节点完成
- **JSONL 记录**：`{"type":"execution", "graph_execution_id":"...", "node_count":N, "result":{...}}`
- **嵌套语义**：GenerateSubgraphNode 内部触发新 execution，带 `parent_graph_execution_id`

### 决策 D3：ConvergenceEntry 选择机制

在 Attempt 层引入显式收敛决策：

```jsonl
{"type":"convergence", "selected_attempt":"attempt-123", "reason":"highest_quality_score", "alternatives":["attempt-124", "attempt-125"], "scores":{"attempt-123":0.95, "attempt-124":0.82, "attempt-125":0.73}}
```

**语义**：
- ForkJoinLoop 或 retry 场景产生多个 attempt
- 系统选择一个 attempt 作为 canonical result
- 其他 attempt 标记 `discarded: true`
- ConvergenceEntry 记录选择依据（quality / speed / cost）

### 决策 D4：Session 文件布局

```
~/.hydraforge/sessions/
├── <session_id>.v1.jsonl       # 主会话文件（包含所有 4 scope）
├── <session_id>-fork-<n>.v1.jsonl  # 分支会话（继承父会话 context）
└── _index.jsonl                # 全局索引（可选，加速 list_sessions）
```

**版本化**：
- 文件名包含 `.v1.`，未来格式变更时递增版本号
- 加载时按版本号选择解析器（v1/v2/v3）

### 决策 D5：SessionWriter 职责边界

SessionWriter 独立于 EventLog（ADR-0080），职责：
- **写入**：append conversation/attempt/step/execution/convergence 记录到 JSONL
- **读取**：build_context_from_session() 重建 LayeredContext
- **分支**：fork_session() 创建新文件，记录 parent_session_id
- **压缩**：compact_session() 合并冗余记录（保留语义）

EventLog（ADR-0080）订阅全局 bus "*"，SessionWriter 只写当前会话相关事件（精选子集）。

### 决策 D6：Event 到 JSONL 的映射

| Event Topic | JSONL type | 触发时机 |
|---|---|---|
| `conversation.user_message` | `conversation` (role=user) | 用户输入 |
| `conversation.assistant_message` | `conversation` (role=assistant) | Agent 响应 |
| `attempt.started` | `attempt` | PlanExecuteLoop/ReactLoop 开始 |
| `attempt.ended` | `attempt` (带 result) | 循环结束 |
| `phase.completed` | `attempt` (phase 字段更新) | plan/execute/verify 完成 |
| `branch.created` | `attempt` (带 branch_id) | 分支创建 |
| `llm.request` / `llm.response` | `step` | LLM 调用 |
| `tool.execution.start/end` | `step` (tool_results 数组) | 工具执行 |
| `dsl.call.started/completed` | `execution` | 图执行 |
| `attempt.converged` | `convergence` | 收敛决策 |

### 决策 D7：node-id 稳定寻址（v1.2 amendment, 2026-08-20）

**问题**：当前 `SessionStore::branch(src, message_index)` 使用 `size_t message_index`（位置索引）—— 当 Session 经历 `append` / `compact` / `extract` 等操作后，消息位置漂移，导致旧 branch 引用错位（缺陷 1.2 / 1.5）。

**决策**：引入 `node_id` 作为唯一稳定寻址符（替代 `message_index`）：

```
node_id = "<file_id>:<seq>"
其中 file_id = "sreg:<uuid>" | "sm:<uuid>"（D10 命名空间）
      seq      = per-file 自增序号（compact/append 后单调递增，保证稳定寻址）
```

**规则**：
- 每次 `append()` 自动分配下一个 `seq`（从 1 开始）
- `branch(src, node_id)` 替代 `branch(src, message_index)`
- `extract(node_id)` 返回新 file_id（路径提取）
- `checkout(node_id)` 回退到该 node（branch cursor 语义，决策 D8）

**不变量**：`node_id` 在该 file 生命周期内**永不重用**；compact 仅影响物理行位置，不影响 seq 分配。

### 决策 D8：branch cursor 持久化（v1.2 amendment）

**问题**：当前 Session 无 "当前分支游标" 概念——多次 `branch()` 后无法定位 "我在哪个分支"。

**决策**：在 Session 结构新增 `current_branch_node_id_` 字段（持久化字段）：

| 字段 | 类型 | 含义 |
|------|------|------|
| `current_branch_node_id_` | `std::string` | 当前分支游标（默认 = session 创建时首个 node_id） |
| `branch_history_` | `std::vector<std::string>` | 分支历史栈（可选，v2 引入） |

**规则**：
- `append()` 后 `current_branch_node_id_` 自动推进到新 node
- `extract(node_id)` 后 `current_branch_node_id_` = node_id（在新 file 中）
- `checkout(node_id)` 回退游标（不删除后续 append 的 node，仅标记 "当前位置"）

### 决策 D9：path-extraction fork（v1.2 amendment）

**问题**：当前 `branch(src, msg_index)` 拷贝消息前缀（部分拷贝）—— Pi 风格需要 "路径提取"（创建新 file 包含 leaf→node 路径）。

**决策**：新增 `extract(node_id)` 方法，行为：
1. 给定任意 `node_id`（不限 leaf）
2. 创建新 file，header 含 `parent_file_id` + `branch_at_node_id`
3. 内容 = leaf→node 路径的线性消息序列

**签名**（提案）：
```cpp
std::string SessionStore::extract(const std::string& node_id);
```

**返回**：新 file_id（如 `sm:<uuid>`）

**与 `branch()` 区别**：
- `branch()`：拷贝消息前缀到同一 file（同一命名空间）
- `extract()`：创建新 file（跨命名空间），header 记录 lineage

### 决策 D10：4 套存储命名空间分配（v1.2 amendment）

**问题**：4 套 session 子系统并存（`SessionManager` / `SessionStore` / `SessionRegistry` / `g3_state.h::SessionStore`），命名空间无明确分配 → 可能 ID 碰撞。

**决策**：明确分配命名空间前缀：

| 子系统 | 命名空间前缀 | 示例 |
|--------|-------------|------|
| `SessionRegistry`（engine 内存态） | `sreg:` | `sreg:abc-123` |
| `SessionManager`（core JSONL 树） | `sm:` | `sm:def-456` |
| `SessionStore`（pdk session_agent） | `sst:` | `sst:ghi-789` |
| `g3_state.h::SessionStore` | `g3st:` | `g3st:jkl-012` |

**规则**：
- 各子系统内部生成 ID 时强制使用对应前缀
- 跨子系统引用时（如 SessionStore 引用 SessionManager 的 file_id），通过前缀区分
- pdk_chat_demo 同时使用 `sreg:` 和 `sm:` 路径时无冲突（实测已 ship）

**收敛方案**：长期 4 套→1 套的收敛留待 Phase 2 独立 change 裁决（本 ADR 不裁决收敛，避免 6 缺陷一起处理）。

## 不变量

1. **单一数据源**：每个 conversation 的真相只有一份 — `.v1.jsonl` 文件
2. **追加only**：JSONL 文件只追加，不修改已写入的行（compact 除外）
3. **原子写入**：每条记录一次 write() + fsync()，保证崩溃安全
4. **层级完整性**：
   - 每个 attempt 属于一个 conversation
   - 每个 step 属于一个 attempt
   - 每个 execution 可跨越多个 step（图内多次 LLM 调用）
5. **收敛唯一性**：每组 parallel attempts 最多一个 ConvergenceEntry
6. **分支继承**：fork 的 session 继承父 session 的 context，但有独立 session_id

## 后果

### 正面后果
- ✅ 原子性保证：单文件写入，避免两文件不一致
- ✅ 完整时序：所有事件按发生顺序，易于回放/调试
- ✅ 层级清晰：4 scope 明确定义，消除 session/task/subtask 混乱
- ✅ 支持并发 attempt：ForkJoinLoop 天然支持
- ✅ 支持分支：fork_session 创建新文件，保持父子关系
- ✅ 可审计：ConvergenceEntry 记录选择依据，透明化决策

### 负面后果
- ⚠️ 文件体积：长会话单文件可能数 MB（需 compact 机制）
- ⚠️ 解析开销：恢复 context 需读整个 JSONL（可增量加载优化）
- ⚠️ 并发写入：多线程写同一文件需互斥锁（SessionWriter 内部处理）

## 迁移路径

### Phase 0: 向后兼容层（shipment gate）
- SessionManager 保留 `load_legacy()` 方法读旧格式（JSON + JSONL）
- 新写入全部用 v1.jsonl 格式
- 旧文件首次加载时自动迁移到新格式

### Phase 1: SessionWriter 基础设施（2-3 天）
- 实现 `SessionWriter` 类（append / fsync / build_context）
- 实现 4 scope JSONL schema
- 单元测试：写入 / 读取 / 崩溃恢复

### Phase 2: Event → JSONL 桥接（1-2 天）
- SessionWriter 订阅 bus 事件（表中 11 个 topic）
- 映射到 JSONL 记录
- 测试：模拟 PlanExecuteLoop / ForkJoinLoop，验证 JSONL 正确性

### Phase 3: ConvergenceEntry（1 天）
- 实现收敛决策逻辑（quality/speed/cost 评分）
- ForkJoinLoop 集成（选择最佳 attempt）
- 测试：3 个 attempt，验证 selected/discarded 标记

### Phase 4: SessionManager 迁移（1-2 天）
- 移除旧 `session_meta.json` 写入
- fork_session / compact_session 适配新格式
- 集成测试：完整会话生命周期

### Phase 5: Loop 改造（2-3 天）
- ReactLoop / PlanExecuteLoop / ForkJoinLoop 发射 attempt.* 事件
- 测试：每个 loop 的 JSONL 输出验证

**总估时**：8-12 天（1.5-2 Sprint）

## 现行 defect 声明（v1.1 修订）

`pdk/session_agent/src/session_store.cpp:163` 的 `SessionStore::persist()` 使用 `std::ios::trunc`
**全量重写** session 文件——每次 persist 销毁历史消息。该实现与本 ADR 的 append-only
不变量（不变量 2）**直接冲突**。

**v1.1 已通过独立 commit 修复**：
- `Session` 结构新增 `size_t persisted_count_{0}` 字段（已落盘游标）
- `persist()` 改用 `std::ios::app` + 仅写 `messages[persisted_count_:]`
- 配套测试 `tests/test_session_store_append.cpp` 验证 append 两次后文件行数正确累积

**长期 SessionStore→SessionManager 收敛**留待 Phase 2 独立 change 裁决：
- `SessionManager` 是 core/SessionManager（JSONL 树）
- `SessionStore`（pdk session_agent）是另一个全量重写实现
- `SessionRegistry`（engine.cpp 内存态）是第三个 session 子系统（**实际有 4 套**——含 g3_state.h 中的 SessionStore）
- v1.1 不裁决收敛方案，避免 6 缺陷一起处理；先止血 trunc bug

## 与其他 ADR 的关系

- **ADR-0008**（LayeredContext）：build_context_from_session() 填充 5 层结构
- **ADR-0020**（线程模型）：CognitiveWorker 持有独立 SessionWriter 实例
- **ADR-0080**（EventLog）：SessionWriter 与 EventLog 并行订阅 bus，职责不同
- **ADR-0082**（Agent Registry）：每个 agent 可有独立 session_id，支持 subagent 隔离

## 风险登记

| 风险 | 概率 | 影响 | 缓解 |
|---|---|---|---|
| 文件体积失控 | 中 | 中 | compact_session() 定期合并；rotation 策略 |
| 并发写入冲突 | 低 | 高 | SessionWriter 内部互斥锁；每个 session 单线程写 |
| 旧格式迁移失败 | 中 | 中 | load_legacy() 容错；保留旧文件作为备份 |
| ConvergenceEntry 评分不合理 | 中 | 低 | 可配置评分权重；v1 先 ship 简单规则 |
| 跨 session 引用丢失 | 低 | 中 | parent_session_id 字段维护继承链 |

## 附录 A：JSONL Schema 示例

```jsonl
{"v":1,"type":"session_meta","session_id":"ses_abc123","created_at":"2026-01-19T10:30:00Z","config":{"max_turns":50}}
{"v":1,"type":"conversation","role":"user","content":"帮我写个排序算法","ts":1737281400}
{"v":1,"type":"attempt","attempt_id":"attempt-1737281400-001","branch_id":"main","started_at":1737281401}
{"v":1,"type":"step","step_id":"step-attempt-1737281400-001-1","llm_request":{"model":"deepseek-chat","prompt_hash":"abc123","event_ref":"evt-1737281400-001"},"tool_results":[{"tool":"file.write","ok":true,"args_ref":"evt-1737281402-003","result_ref":"evt-1737281403-004"}]}
{"v":1,"type":"execution","graph_execution_id":"gexec-1737281402-001","node_count":5,"result":{"ok":true}}
{"v":1,"type":"attempt","attempt_id":"attempt-1737281400-001","ended_at":1737281405,"result":{"ok":true}}
{"v":1,"type":"conversation","role":"assistant","content":"已完成排序算法实现","ts":1737281405}
{"v":1,"type":"convergence","selected_attempt":"attempt-1737281400-001","reason":"only_attempt","alternatives":[]}
```

**v1.1 修订**：step 记录新增 `event_ref` / `args_ref` / `result_ref` 三个**指针字段**，
指向 ADR-0080 EventLog 中的 `event_id`。

### 附录 A.1：职责边界（v1.1 原则声明）

**Session JSONL 存骨架 + 指针；EventLog 存字节。** 两者职责严格分离：

| 文件 | 存什么 | 不存什么 |
|---|---|---|
| Session JSONL（0079）| 4-Scope 结构、ConvergenceEntry、决策摘要 | prompt 文本、response 文本、tool args 值 |
| EventLog（0080）| 完整 prompt_text、response_text、params、tool args 值 | 4-Scope 结构、Convergence 决策 |

**蒸馏场景**：导出器 join 两个文件——从 Session JSONL 拿到 ReAct 骨架（think→tool_call→observe→think），从 EventLog 按 `event_ref` 拿到 prompt/response/args 字节，**完整 ReAct 轨迹**。

**零数据冗余**：同一事实不写两遍。Session 不复制 EventLog 数据，EventLog 不复制 Session 结构。

**保留 ADR-0079 D5 与 ADR-0080 附录 C 的职责划分**：SessionWriter 订阅 bus 写会话结构事件子集，EventLog 订阅全部事件落审计字节。

## 附录 B：ConvergenceEntry 评分规则（v1）

```cpp
struct ConvergenceScore {
  double quality;   // 0-1, 基于验证结果
  double speed;     // 0-1, 归一化耗时（越快越高）
  double cost;      // 0-1, 归一化成本（越低越高）
  
  double total() const {
    return 0.5 * quality + 0.3 * speed + 0.2 * cost;  // 可配置权重
  }
};
```

## 附录 C：文件格式版本演进策略

- **v1**（当前）：JSONL 追加，无压缩
- **v2**（未来）：引入增量快照（每 N 条记录一个 checkpoint）
- **v3**（未来）：引入列式存储（Parquet）用于大规模会话分析

加载器按文件名版本号分发：
```cpp
if (filename.contains(".v1.")) return load_v1(path);
if (filename.contains(".v2.")) return load_v2(path);
```

## 附录 D：与 SessionManager 现有 API 的兼容性

| 现有 API | v1 实现 | 变更 |
|---|---|---|
| `open_session(id)` | 加载 `<id>.v1.jsonl` | 返回类型不变 |
| `create_session()` | 创建 `.v1.jsonl` + 写 session_meta | 返回类型不变 |
| `fork_session(parent)` | 创建 `<parent>-fork-<n>.v1.jsonl` | 新 API |
| `compact_session(id)` | 合并冗余记录 | 新 API |
| `build_context(id)` | 从 JSONL 重建 LayeredContext | 签名变更（新增 scope 参数）|

---

**审批记录**：
- 提议：2026-01-19
- 审批：2026-01-19
- 实施：待 Phase 1 启动
