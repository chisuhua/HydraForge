# 架构缺陷真相表（2026-08 v1.1）

**生成日期**: 2026-08-20
**最后验证**: 2026-08-20（v1.1 修订——Oracle + Metis 独立评审后；验证命令见 §八）
**作者**: Architecture Working Group
**状态**: ✅ Active — 架构缺陷分析的**唯一事实源**

**关联文档**:
- `docs/architecture/adr-implementation-status-gap-analysis.md` — ADR 实施状态基线（ADR 状态引用必须以此为准）
- `docs/architecture/defect-fix-roadmap-2026-08.md` — 本表对应的 rdd-workflow 修复路线图（12 个提案）
- `docs/adr/adr-0079-unified-session-4scope.md` — 缺陷 1.1~1.5 主参考 ADR
- `docs/adr/adr-0080-append-only-event-log.md` — 缺陷 2.1/2.2 主参考 ADR
- `docs/adr/adr-0057-agent-lifecycle.md` — 缺陷 3.2 主参考 ADR
- `docs/adr/adr-0060-agent-composition.md` — 缺陷 3.3 主参考 ADR
- `docs/adr/adr-0069-tool-coordinator-hooks.md` — 缺陷 4.2 Tool hooks 部分主参考 ADR
- `docs/adr/adr-0081-pre-step-hook-contract.md` — 缺陷 4.2 Agent hooks 主参考 ADR
- `docs/adr/adr-0082-agent-first-class-registry.md` — 缺陷 3.1 主参考 ADR

**审计方法**: 三 agent（explore × 2 + Metis + oracle）独立核查 — (1) 14 项缺陷的代码真实性；(2) `docs/adr/` 与 `docs/archive/adr/` 全量 ADR 文档的方案检索；(3) 综合评审一致性验证。

**核心发现**（v1.1 校正后）:

1. **11 项真实架构缺陷** + 1 项工程债（缺陷 6.1）+ 2 项**分层部分解决**（缺陷 4.1/5.1），合计 14 项覆盖
2. **已批准但实施待补的 ADR**（5 个）：ADR-0057（零事件契约）、ADR-0069（partial）、ADR-0079（v1.1 已批，SessionWriter 待 ship）、ADR-0080（v1.1 已批，query API 待补）、ADR-0060（4/6 模式待 ship）
3. **搁置待解锁的 ADR**（2 个）：ADR-0081（待 ADR-0082）、ADR-0082（待 ADR-0079/0080 ship）
4. **新发现 3 个盲点**：错误传播断层、compact 幻影事件、OTel 零代码（均归 §六）

---

## 一、缺陷索引（速查表）

> ADR 状态以 `adr-implementation-status-gap-analysis.md` 为准；docs/README、ADR 头部之间的状态不一致本文不维护副本。

| # | 缺陷 | 真实? | 主参考 ADR | 现状 | 优先级 |
|---|------|------|-----------|------|--------|
| 1.1 | 4 套会话存储并存 | ✅ | ADR-0079 v1.1 ✅ Approved | SessionWriter 实施 0% | **P0** |
| 1.2 | message-index 寻址（脆性） | ✅ | ADR-0079 v1.2 修订 | 含于 v1.2 计划 | P1 |
| 1.3 | 缺 branch cursor 语义 formalization | ✅ | ADR-0079 v1.2 修订 | 含于 v1.2 计划 | P1 |
| 1.4 | compact 是破坏性重写 | ✅ | ADR-0079 §不变量 2 | **有意例外**（待 v1.2 决策） | P3 |
| 1.5 | 缺 path-extraction fork | ✅ | ADR-0079 D4 | fork() 存在，实现细节未对齐 | P1 |
| 2.1 | EventLog 不完整（query 缺失） | ✅ | ADR-0080 v1.1 ✅ Approved | ✅ **已 ship**（P4 `event-log-query-api`：member read() + query() with glob + perf 基准 10k<100ms） | **P0 → ✅** |
| 3.1 | Agent 非 first-class | ✅ | ADR-0082 🔍 Proposed | 搁置待 ADR-0079/0080 ship | P1 |
| 3.2 | Agent 生命周期**事件契约**缺位 | ✅ | ADR-0057 ✅ Approved + ADR-0068 附录 A | ✅ **已 ship**（P1 `adr-0057-amend` §决策 6 + ADR-0068 附录 A 注册 4 主题：agent.spawned/heartbeat/terminated/error） | **P0 → ✅** |
| 3.3 | Agent↔Agent 协议缺失 | ✅ | ADR-0060 ✅ Approved | 2/6 模式 ship（ADR-0060 决策 4 表格的 ✅ 列是 scope 声明而非实施声明） | P2 |
| **4.1** | **Plugin scoped 注册（per-agent）** | **🟡 分层部分解决** | ADR-0022 ✅ ship | **engine 级已解决**（per-engine 注册）；**agent 级版本隔离未解**（同 engine 共享 ToolRegistry）；marketplace/多租户场景为 open gap | 非阻塞 open gap |
| 4.2 | 缺 pre-step hook（Agent 级拦截） | ✅ | ADR-0069 🟡 + ADR-0081 🔍 | Tool hooks ✅ ship；Agent hooks 待 ADR-0081 → Approved | **P0**（但需 ADR-0082 → Approved 解锁） |
| **5.1** | **Cancellation scope tree（per-agent）** | **🟡 分层部分解决** | ADR-0020 ✅ ship | **协作式取消 + RAII 已 ship**（jthread + stop_token）；**统一 scope tree/父子级联未 ship**；术语"structured concurrency"误用 | 非阻塞 open gap |
| 6.1 | MockBus 重复（9 处） | ✅（工程债） | 无 ADR | ✅ **已 ship**（P12 `mock-bus-canonical-extract`：canonical fixture + 9 处迁移 + 12 cases/38 assertions PASS） | P3 → ✅ |

**索引校正说明**：
- 缺陷 4.1/5.1 从"已解决"**降级**为"分层部分解决"——per-engine 与 per-agent 是不同粒度，per-agent 版本/作用域隔离未 ship；ADR-0020 worker-per-engine 提供绕行路径但无强制
- 缺陷 3.2 修订：ADR-0057 是已批准状态机但**未定义生命周期发射事件**；"零实施纯增量，无需新设计" 的 v1.0 措辞被修正（见 §六修订说明）
- 缺陷 2.2 修订：v:1 schema 字段**已落盘**（非"待验证"）；ADR-0068 状态 ✅ **Approved**（非 Proposed）
- 缺陷 6.1 修订：MockBus 计数 **9 处**（非 7 处）

---

## 二、缺陷全景（按层分组）

### Layer A: 会话层（5 项）

#### 缺陷 1.1：4 套会话存储并存

**背景**

HydraForge 当前存在 4 套独立的会话存储子系统（ADR-0079 v1.1 §现行 defect 声明识别）：

| 子系统 | 位置（**v1.1 校正**） | 语义 | 持久化 | Fork 粒度 |
|---|---|---|---|---|
| `SessionManager` | `src/core/session_manager.{h,cpp}` | JSONL 树 + 分支索引 | ✅ | 节点级 |
| `SessionStore`（pdk session_agent） | `pdk/session_agent/src/session_store.cpp`（声明在 `pdk/session_agent/include/session_agent.h`） | 全局单例 + JSONL 消息流 | ✅ | message-index |
| `SessionRegistry` | **`src/core/types/session_registry.{h,cpp}`**（v1.1 校正：独立类，非"engine.cpp 内存态"） | DSLEngine 内存态 | ❌ | — |
| `g3_state.h SessionStore` | **`pdk/g3_knowledge_base/src/g3_state.h`**（v1.1 校正：`src/g3/state.h` 不存在） | g3 知识库内部 | ❌ | — |

**为什么是缺陷**

1. **四套真相源**：同一概念"会话"有 4 个不同真相源；调用方必须知道使用哪一套（`pdk_session_agent::SessionStore::instance()` 与 `SessionManager::fork()` 看到不同的 session）
3. **历史 bug**：v1.1 修复了 `SessionStore::trunc` 全量重写 bug（`session_store.cpp:163`），但**4 套并存的结构性问题未解决**
4. **语义冲突未深入分析**：SessionRegistry（执行层级）与 SessionManager（持久化对话树）对"会话"建模根本不同；二者与 pdk SessionStore 的 session_id 命名空间是否碰撞？`pdk_chat_demo` 同时走哪几条路径？——这些是"4 套并存"的真正危险点，**v1.1 仍未覆盖**（见 §六盲点）

**参考内容**

- **ADR-0079 v1.1**：`docs/adr/adr-0079-unified-session-4scope.md` ✅ Approved
  - D1：单一 JSONL 流（每 conversation 一个文件）
  - D2：4-Scope 模型（Conversation/Attempt/Step/Execution）
  - D5：SessionWriter 职责边界（与 EventLog 分离）
  - §现行 defect 声明（line 207-222）：明确指出 4 套并存 + v1.1 修 trunc bug + Phase 2 裁决
- **ADR-0033**：`docs/adr/adr-0033-session-hierarchy.md` ✅ Approved — 定义内存三层模型（UserSession/TaskSession/SubtaskSession），不涉及持久化统一

**当前状态**：ADR-0079 v1.1 已批准 + SessionWriter 基础设施**未启动**（Phase 1 待 ship）

---

#### 缺陷 1.2：message-index 寻址（脆性设计）

**背景**

`SessionStore::branch(src_session_id, size_t message_index)` 使用**消息位置**作为分叉地址（`pdk/session_agent/src/session_store.cpp:204-217`）：

```cpp
s.messages.assign(src.messages.begin(), src.messages.begin() + message_index + 1);
```

**为什么是缺陷**

1. **位置漂移**：compact 后消息被截断或 placeholder 替换，位置指向错误消息
2. **跨 session 引用失效**：分支在新 session 中无法引用"父 session 第 N 条消息"
3. **并发 append 竞争**：消息位置在 append 期间变化
4. **不可调试**：错误日志无法定位具体消息

**参考内容**

- **ADR-0079 v1.1**：未明确处理 message-index 寻址机制（当前 SessionManager 实际使用 `node_id` 寻址）
- **ADR-0080 v1.1**：EventLog 使用 `event_id` 寻址（ADR-0080 D2）— 这是正确的**范式**

**当前状态**：SessionStore 单例未被 ADR-0079 显式裁决 message-index 替代方案；需要在 v1.2 修订中明确

---

#### 缺陷 1.3：缺 branch cursor 语义 formalization

> v1.1 修订：原措辞"leaf pointer"是 Oracle 评审后**改用**的术语——`get_branch_leaf()` 是查询分支当前叶子；本文档描述的"持久化可回退游标"是 cursor 语义，不是 leaf 语义。

**背景**

`SessionManager` 有 `get_branch_leaf_node(branch_id)` 方法（`session_manager.h:206` / 243）能查询分支叶子节点，但**没有任何"branch cursor"作为可持久化的位置状态**。`current_branch_`（line 284）是分支级追踪，非位置追踪。

**为什么是缺陷**

1. **无法在分支内回退**：用户走到分支的第 10 步想回到第 5 步继续 — 当前 API 只能切分支，不能在分支内"回退游标"
2. **无法做 Pi 式 branch**：Pi 的 leaf pointer 移动产生新"分支"，零数据拷贝
3. **无法做 DSH 式 boundary**：DSH `fork(source, boundary)` 在任意事件处建分支 — 需要稳定的 boundary 概念

**参考内容**

- **ADR-0079 v1.1**：未 formalize "branch cursor" 概念
- **ADR-0080**：EventLog 用 `(causal_time, event_id)` 做稳定排序键（ADR-0080 D2）— 类似概念但用于事件

**当前状态**：SessionManager 的 `get_branch_leaf()` API 是 branch cursor 概念的雏形，但未在 ADR 层定义为位置状态

---

#### 缺陷 1.4：compact 是破坏性重写（**有意例外**，待 v1.2 决策）

> v1.1 修订：原措辞缺一个时代错位说明 — 当前 `SessionManager::compact` **先于** ADR-0079 存在；"ADR-0079 §不变量 2 的有意例外"是**追溯性**对未来 SessionWriter 的声明，不是原始设计决策。

**背景**

`SessionManager::compact()`（`session_manager.cpp:408-467`）通过备份+原子 rename 重写文件，**不保留 append-only 不变量**：

```cpp
// 步骤 1: 备份原文件
std::filesystem::copy(current_path_, current_path_.string() + ".backup", ...);
// 步骤 3: 写临时文件
const auto temp_path = current_path_.string() + ".tmp";
int fd = ::open(temp_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
// 步骤 4: atomic rename
std::filesystem::rename(temp_path_, current_path_, ec);
```

**为什么是缺陷（但被有意保留）**

1. **破坏审计**：压缩前的消息物理消失，无法回放完整对话
2. **与 append-only 矛盾**：声称 append-only 但 compact 例外（参见 ADR-0079 §不变量 2 line 150-152："compact 除外"）
3. **破坏并发**：压缩期间文件被替换，正在 append 的写入可能冲突

**参考内容**

- **ADR-0079 v1.1 §不变量 2**（line 150-152）："JSONL 文件只追加，不修改已写入的行（compact 除外）"——追溯性背书
- **ADR-0080 D10.6**：audit 防线 — tool args 不落盘，但 prompt/response 落盘后**也不应被压缩破坏**

**评估**：此缺陷属于**有意设计的例外**（ADR-0079 显式声明）。是否升级为"绝对 append-only"需要在 ADR-0079 v1.2 中决策 — 接受 Pi 模式（compaction 作为 entry）还是保留当前 in-place 重写。

---

#### 缺陷 1.5：缺 path-extraction fork

**背景**

`SessionStore::branch(src, msg_index)`（`session_store.cpp:204-217`）做"消息前缀拷贝"，不是 Pi 风格的"路径提取到新文件"。

**为什么是缺陷**

1. **不可分享**：无法把"探索路径"作为 artifact 分享给其他 agent
2. **不可跨进程接管**：一个进程 fork 的 session 另一个进程无法接管
3. **数据冗余**：分叉时复制消息前缀，大型会话占用存储线性增长

**参考内容**

- **ADR-0079 D4**：`docs/adr/adr-0079-unified-session-4scope.md` line 109-117 — 已定义 `fork_session(parent)` 创建 `<parent>-fork-<n>.v1.jsonl`
- **ADR-0079 v1.1 §附录 D**（line 298-307）：列出 `fork_session(parent)` 为新 API（已在 v1 实现范围内）

**当前状态**：ADR-0079 v1.1 已批准 + SessionManager::fork 存在（实现细节与 ADR 不完全对齐）

---

### Layer B: 事件/可观测层（2 项）

#### 缺陷 2.1：EventLog 不完整（query API 缺失）

> v1.1 修订：原措辞"EventBus 非真相源"在 v1.0 是部分过时。实际 `EventLogWriter` 已 ship 182 行实现（`event_log.cpp`），含 bus 订阅、批量 flush、rotation、`enable_event_log()` 集成（`engine.cpp:195`）、4 个测试（`test_event_log_capture.cpp`）、v:1 schema 落盘（line 136）。**真正缺失的是 read/query API（Phase 3）和 SessionWriter 集成（Phase 4）**。

**背景**

`InMemoryBus`（`include/agenticdsl/contract/inmemory_bus.h:60`）仅持有 `std::queue<BusEvent> queue_`，**纯内存**，重启后事件全部丢失：

```cpp
std::queue<BusEvent> queue_;  // 纯内存
```

`EventLogWriter`（`src/core/event_log.{h,cpp}`）是**单独的 subscriber**，订阅 bus 写 JSONL，但 bus 本身无持久化逻辑。

**为什么是缺陷（v1.1 校正））**

1. **EventLogWriter::read() 是 stub**（`event_log.cpp:178` 注释"占位实现：不在本 ADR 范围"）——ADR-0080 D5 定义了 query API，但**未实现**
2. **SessionWriter 集成未 ship**（ADR-0080 Phase 4）——ADR-0079 SessionWriter 与 ADR-0080 EventLog 并行订阅 bus，职责清晰；**集成测试缺失**
3. **离线分析闭环断裂**：EventLog 能写但不能读，无法做跨会话事件聚合（如"本月 tool 执行统计"）
4. **Agent 学习无历史**：无事件可分析

**参考内容**

- **ADR-0080 v1.1**：`docs/adr/adr-0080-append-only-event-log.md` ✅ Approved
  - D1：单目录单文件 per agent
  - D3：系统级 EventLog + SessionWriter 子集并行
  - D4：批量写入 + rotation（**已 ship**）
  - D5：API（emit ✅ / flush_sync ✅ / read ❌ stub / query ❌ stub）
  - D7：Step 0 BusEvent 信封扩展（**已 ship**）
  - D12：opt-in + fail-closed（**已 ship**，`engine.cpp:195 enable_event_log()`）
- **ADR-0019**：`docs/adr/adr-0019-iinteraction-bus-mvp.md` 🟡 Partial — MVP ship 但缺位事件发射契约
- **ADR-0068**：`docs/adr/adr-0068-event-emission-contract.md` ✅ **Approved** (2026-08-03) — Canonical Topic Registry（v1.1 校正：原 v1.0 误写 🔍 Proposed）

**当前状态**（v1.1 校正）：ADR-0080 Step 0+1+2 大部分 ship（envelope/writer/rotation/engine opt-in/v:1 落盘/4 测试已 ship）；缺 query API（Phase 3）+ SessionWriter 集成（Phase 4）

---

#### 缺陷 2.2：事件 schema 无版本化（实际接近已闭环）

> v1.1 修订：原措辞"落盘时是否实际写 `v:1` 字段**待验证**"已被代码否定（`event_log.cpp:136` `j["v"] = 1`）。本缺陷实际接近已闭环，但 Canonical Topic Registry 的 payload schema 字段校验仍**未自动化**（v1.1 维持 P1）。

**背景**

`BusEvent`（`include/agenticdsl/contract/bus_event.h:22-28`）payload 是 `ToolResult`，**无 schema 版本字段**：

```cpp
struct BusEvent {
    std::string topic;
    ToolResult payload;  // 无版本字段
    std::chrono::steady_clock::time_point timestamp;
    uint64_t causal_time{0};  // Reserved
    EventPriority priority{EventPriority::Normal};  // Reserved
};
```

**为什么是缺陷**

1. **字段演进风险**：未来重命名/删除字段会破坏旧事件读
2. **多版本并行**：同 session 包含新旧格式事件无法区分
3. **客户端 SDK 演进无保障**：SDK 升级后无法读旧事件
4. **缺契约**：消费者无法验证事件合法性

**参考内容**

- **ADR-0080 v1.1 D2**：定义 `v:1` schema 版本字段 + `causal_time` + `ts_wall` + `event_id`
- **ADR-0068 附录 A**：Canonical Topic Registry 定义 payload 必填字段
- **ADR-0080 §解析原则**："未知字段忽略"（forward-compatible）

**当前状态**（v1.1 校正）：v:1 字段**已落盘**（`event_log.cpp:136`）；ADR-0068 ✅ **Approved**（v1.0 误写 Proposed）。Canonical Topic Registry payload 校验自动化未 ship。

---

### Layer C: Agent 抽象层（3 项）

#### 缺陷 3.1：Agent 非 first-class

**背景**

Agent 必须通过 `DEFINE_AGENT` 宏（`include/agenticdsl/pdk/agent_macros.h:136-149`）生成 compile-time 类，且通过 PluginLoader 加载 `.so` 注册。无运行时字符串 ID 创建能力：

```cpp
#define DEFINE_AGENT(name, loop_type) \
    class name##Agent { ... Loop loop_; ... };
```

**为什么是缺陷**

1. **不可动态创建**：`AgentFactory::create("react-loop-v1")`（字符串 ID）不可行
2. **不可运行时扩展**：用户从 marketplace 装 Agent 必须重新编译
3. **不可热替换**：运行时换 Agent 版本需要重启
4. **DSH/Pi 对标**：两者都把 Agent 作为运行时 first-class 实体

**参考内容**

- **ADR-0082**：`docs/adr/adr-0082-agent-first-class-registry.md` 🔍 Proposed（讨论稿，未定稿）
  - 设计：AgentWorker + YAML 配置 + 生命周期
  - 搁置理由 R1-R4：前置 ADR-0079/0080 未实施 + 5 个核心争议未解
  - **v1.1 状态注记（2026-08-12）**：蒸馏需求加强搁置

**当前状态**：ADR-0082 讨论稿搁置中，需要等 ADR-0079/0080 实施后定稿

---

#### 缺陷 3.2：Agent 生命周期**事件契约**缺位

> v1.1 修订：原 v1.0 措辞"ADR-0057 定义了 6 状态机 + 4 个生命周期事件（spawned/heartbeat/terminated/error）"——**经 Oracle 评审核查为虚构**。ADR-0057 全文 215 行，**无任何生命周期发射事件定义**。v1.0 P"0 理由"零实施无需新设计"也错——需 ADR-0057 amendment + ADR-0068 附录 A 注册 + emit 点实现。

**背景**

ADR-0057 ✅ Approved 但**只定义** 5 个决策：

1. 6 状态机（LOADED→initialized→registered→active→inactive→unloaded）
2. **Lazy loading via `activation_events`**——这是**懒加载触发器**（如 `onTool:`/`onAgent:` 等），**不是生命周期发射事件**
3. 冷切换（热更新 defer Phase 2）
4. 依赖检查
5. 与 ADR-0051 ToolCoordinator RAII 正交

代码中**没有任何 emit 点**实现生命周期事件（grep 证实 `agent.spawned/heartbeat/terminated/error` 在 `src/`、`include/`、`pdk/` 零命中）。

**为什么是缺陷**

1. **无 agent lifecycle 审计**：无法追溯"Agent 何时 spawn / 何时死"
2. **EventLog 缺 agent.* 主题**：ADR-0068 ✅ Approved 附录 A（v1.1 ~28 主题注册）**无任何 `agent.*` 主题**
3. **无 marketplace 可观测性**：Agent 从 marketplace 装/卸载无审计记录
4. **ADR-0082 搁置的部分原因**：没有 lifecycle 事件层，Agent first-class 缺乏审计基础

**参考内容**

- **ADR-0057**：`docs/adr/adr-0057-agent-lifecycle.md` ✅ Approved (2026-07-16)
  - 决策 1-5 如上，**未定义生命周期发射事件**
- **ADR-0068 附录 A**：28 主题注册清单，**无 `agent.*`**
- **ADR-0082 line 92**：唯一一处 `agent.spawned` 提及，Proposed 状态未 ship

**P0 理由（v1.1 校正）**：保持 P0 但**修正 rationale**：

> 缺陷真实 + 解锁 ADR-0082 + 解锁 marketplace 审计 + 但**需要先补事件定义**（ADR-0057 amendment + ADR-0068 附录 A 注册 `agent.*` 主题），是 ≈1 sprint 的最小增量。**"无需新设计"措辞不成立**——事件 topic 命名、payload schema 全部需要新设计（且必须过 ADR-0068 Canonical Topic Registry）。

---

#### 缺陷 3.3：Agent↔Agent 通信协议缺失

**背景**

`archive_subtask_result`（`src/core/types/session.cpp:41-55`）做内存合并：

```cpp
void TaskSession::archive_subtask_result(const SubtaskSession& subtask) {
    for (auto& existing : subtask_sessions_) {
        if (existing.branch_path == subtask.branch_path && existing.status == "running") {
            existing = subtask;  // In-memory merge only
        }
    }
}
```

**为什么是缺陷**

1. **无跨树通信**：Agent A 想给 Agent B 发消息（B 不在 A 的 subtask 树中）不可行
2. **无 streaming**：partial result 流回不可行
3. **无 negotiation 协议**：offer/counter-offer 模式不可行
4. **仅 in-memory**：跨进程/重启不保留通信上下文

**参考内容**

- **ADR-0060**：`docs/adr/adr-0060-agent-composition.md` ✅ Approved (2026-07-16)
  - 定义 6 种模式：① call ② call_async ③ emit ④ delegate ⑤ parallel ⑥ stream
  - **实施状态真伪辨析**（v1.1 校正）：
    - ADR-0060 决策 4 表格中模式 ①②③④⑤ 的 ✅ 列 = scope 声明（"ADR 范围包含"），**不是实施声明**
    - 代码侧 grep `call_async`/`delegate` 在非测试代码零命中
    - 实际 ship：parallel + pub/sub（emit）= 2/6 模式
  - Stream 模式 defer Phase 2

**当前状态**：ADR-0060 已批准 + 6 模式中 4 模式待 ship；ADR 表与代码真相之间存在 ADR 自身的"scope 标记 vs 实施标记"混淆问题

---

### Layer D: 生命周期/作用域层（2 项）

#### 缺陷 4.1：Plugin scoped 注册（per-agent）— **分层部分解决**

> v1.1 修订：原 v1.0 措辞"完全解决"被**降级**为"分层部分解决"。ADR-0022 ship 了 **per-engine** 注册（每 DSLEngine 独立 ToolRegistry），但**per-agent** 版本隔离在同一 engine 内**未 ship**——同 engine 共享 ToolRegistry，重名注册被拒绝。

**背景**

PluginLoader 维护 `std::vector<LoadedPlugin> loaded_`（`plugin_loader.h:206`）。注册符号 `pdk_register_tools(IToolRegistry& registry)`（ADR-0022 line 72）将 plugin 注册到调用方传入的 registry——粒度为 engine 级。

**为什么前版被识别为缺陷**

1. **per-agent 隔离困难**：同 engine 内 Agent A 加载 plugin v1，Agent B 加载 plugin v2 → 不可行
2. **多租户场景**：无法 version-per-agent

**分层解决现状**

| 层级 | 状态 | 证据 |
|---|---|---|
| **Engine 级隔离** | ✅ **已 ship** | ADR-0022 per-engine 注册（2026-06-24 commit `968937f`） |
| **Agent 级版本隔离（per-engine 内）** | ❌ **未 ship** | 同 engine 共享 ToolRegistry；ADR-0020 worker-per-engine 提供绕行路径（每 CognitiveWorker 独占 DSLEngine），但**无强制** |
| **PluginLoader 实例** | process-scope | `loaded_` 是单实例 vector；PluginLoader 之间无隔离 |

**ADR-0022 已 ship 范围**：RTLD_LOCAL 符号隔离 + 路径白名单 + Dual ABI support（V1/V2）+ `pdk_plugin_init`/`fini` 钩子（ADR-0041 扩展）+ 依赖图

**结论**（v1.1 校正）：缺陷 4.1 **engine 级已解决**，**agent 级版本/作用域隔离未解**——marketplace/多租户需求出现时再立项（ADR-0082 完整 ship 后自然解决）

**参考内容**

- **ADR-0022**：`docs/adr/adr-0022-plugin-loading.md` ✅ Approved + **shipped** (2026-06-24)
- **ADR-0041**：`docs/adr/adr-0041-pluginloader-lifecycle-extension.md` ✅ Approved + shipped (2026-07-10) — 扩展 `pdk_plugin_init`/`fini` 钩子
- **ADR-0073**：`docs/adr/adr-0073-tool-json-schema-contract.md` 🟡 Partial — Tool JSON Schema 契约

---

#### 缺陷 4.2：缺 pre-step hook（Agent 级拦截点）

**背景**

`NodeExecutor::dispatch_to_tool`（`src/modules/executor/node_executor.cpp:350-389`）无 agent step 拦截点，仅 tool 级别有 ToolCoordinator 入口检查：

```cpp
if (tool_coordinator_) {
    tool_result = tool_coordinator_->execute(meta, tool_ctx, args, token);
} else if (approval_handler_) {
    // ...直接调用 tool_registry_.call_tool(tool_name, args)
} else {
    nlohmann::json raw_result = tool_registry_.call_tool(tool_name, args);
}
```

**为什么是缺陷**

1. **无 PII 脱敏拦截点**：agent step 进入 LLM 前无法 scrub 敏感数据
2. **无 budget 检查拦截点**：agent step 调用 LLM 前无法扣费
3. **无 capability 校验拦截点**：agent step 决策前无法校验
4. **无 audit + replay 拦截点**：agent step 进入前无法记录

**参考内容**

- **ADR-0069**：`docs/adr/adr-0069-tool-coordinator-hooks.md` 🟡 Partial (2026-08-04)
  - Tool 级别 pre/post hook 已 ship
  - 实施状态：`IToolHookRegistry` 契约 + ToolCoordinator middleware + 5 类测试已 ship
- **ADR-0081**：`docs/adr/adr-0081-pre-step-hook-contract.md` 🔍 Proposed (2026-08-12)
  - Agent 级拦截点定义
  - 推迟到 ADR-0082 定稿（两种设计：Agent-scoped vs LLM-scoped）

**当前状态**：Tool hooks ✅（ADR-0069 已 ship）；Agent hooks 🔍 Proposed（ADR-0081 搁置待 ADR-0082）。**硬依赖链：ADR-0081 → ADR-0082 → { ADR-0079, ADR-0080 } ship**

---

### Layer E: 并发层（1 项）

#### 缺陷 5.1：Cancellation scope tree（per-agent）— **分层部分解决**

> v1.1 修订：原 v1.0 措辞"完全解决"被**降级**为"分层部分解决"。ADR-0020 ship 的是 **jthread RAII + 协作式取消（std::stop_token）+ 全局锁顺序**——这是 cooperative cancellation，**不是** true structured concurrency（parent scope 保证 children 先完成的 scope tree，如 std::execution nursery）。术语"structured concurrency"被误用。

**背景**

3 处独立并发原语：`std::jthread` (CognitiveWorker) + CV wait (ForkJoinLoop) + `shared_mutex` (DomainWorkerPool)，**无统一 cancellation scope tree**。

**分层解决现状**

| 层级 | 状态 | 证据 |
|---|---|---|
| **RAII 线程生命周期** | ✅ ship | `std::jthread` 构造启动 + 析构 stop+join（cognitive_worker.h:267） |
| **协作式取消** | ✅ ship | `std::stop_token` 协议（ADR-0020 line 313 原文） |
| **统一锁顺序** | ✅ ship | StateStore > ToolRegistry > CognitiveWorker::queue_lock > InMemoryBus（ADR-0020 §6） |
| **父-子 scope tree / 父子级联取消保证** | ❌ **未 ship** | 无 nursery / scope guard / 形式化取消传播；ForkJoinLoop 取消时级联子 worker 无形式化保证 |
| **C++26 `std::execution`** | ⏸ 推迟 | 观望 |

**ADR-0020 ship 范围**（Sprint 2/3, 2026-06-24）：

- CognitiveWorker：`std::jthread` + `std::stop_token` 协作式取消
- DomainWorkerPool：`std::jthread` + `std::condition_variable` 多消费者模式 + 显式状态机
- 死锁避免：全局锁顺序
- 错误码 bridge：SimpleCognitiveOrchestrator 9 处 legacy string → ErrorCode enum

**结论**（v1.1 校正）：缺陷 5.1 **协作式取消 + RAII 已 ship**，**统一 scope tree / 父子级联未 ship**——形式化 cancellation 传播 /marketplace 多租户场景需要时再立项（与 ADR-0082 的 agent scope 概念自然耦合）

**参考内容**

- **ADR-0020**：`docs/adr/adr-0020-thread-model-isolation.md` ✅ Approved + **shipped** (2026-06-24)
- **ADR-0030**：`docs/adr/adr-0030-async-runtime-v2.md` 🟡 Partial — AsyncRuntime V2 异步运行时（Phase 2 协程迁移）

---

### Layer F: 测试基础设施层（1 项）

#### 缺陷 6.1：MockBus 重复（工程债，**非架构缺陷**）

> v1.1 修订：原 v1.0 计数 **7 处**，实际 **9 处**（v1.1 实测，验证命令见 §八）

**背景**

`tests/` 和 `examples/` 下 9 个 MockBus 实现：

1. `tests/test_skill_interpreter.cpp:59`
2. `tests/test_budget_agent_hooks.cpp:26`
3. `tests/test_context_compactor.cpp:122`
4. `tests/test_escalation_triggers.cpp:25`
5. **`tests/test_tool_coordinator.cpp:54`**（v1.1 新增）
6. **`tests/test_tool_coordinator_hooks.cpp:80`**（v1.1 新增）
7. `examples/pdk_chat_demo/tests/test_session_persistence.cpp:30`
8. `examples/pdk_chat_demo/tests/test_e2e_mock.cpp:69`
9. `examples/pdk_chat_demo/tests/test_budget_alert.cpp:33`

**为什么是工程债而非架构缺陷**

1. **生产代码无 MockBus**：唯一 InMemoryBus 在 `src/common/contract/inmemory_bus.cpp`
2. **测试 fixture**：仅是测试代码重复，不构成"架构缺陷"
3. **无 ADR 方案**：不需要 ADR，提取 `tests/test_helpers/mock_bus.h` 即可

**参考内容**

- **ADR-0019**：`docs/adr/adr-0019-iinteraction-bus-mvp.md` 🟡 Partial — IInteractionBus 接口

**当前状态**：工程债。提取 canonical MockBus 是 **5h 工程任务**，不需要 ADR。

---

## 三、修正后的优先级（**单一权威表**）

> v1.1 修订：原 v1.0 §一索引表与 §三优先级表对同一缺陷标了不同优先级（1.1/2.1 在索引表 P0，优先级表 P1），自相矛盾。**v1.1 合并为单一权威表**：

### 🔥 P0 — 立即可启动（无前置）

| # | 缺陷 | 主参考 ADR | 备注 |
|---|------|-----------|------|
| **3.2** | Agent 生命周期事件契约缺位 | ADR-0057 + ADR-0068 附录 A | **需先补事件定义**（amendment）+ 主题注册 + emit 实现。≈1 sprint 最小增量。修正 v1.0 "无需新设计"措辞 |
| **4.2** | Agent pre-step hook | ADR-0081 → Approved | Tool hooks（ADR-0069）已 ship。但需 ADR-0082 定稿解锁——**实际启动需 ADR-0082 先动** |

### 🟠 P1 — 前置 P0 启动后立即推进

| # | 缺陷 | 主参考 ADR | 备注 |
|---|------|-----------|------|
| **1.1** | 4 套会话存储 | ADR-0079 v1.1 实施 | 已批准，SessionWriter Phase 1 待 ship |
| **2.1** | EventLog 不完整（query 缺失） | ADR-0080 v1.1 Phase 3+4 | Step 0+1+2 大部分 ship，缺 query API + SessionWriter 集成 |
| **2.2** | 事件 schema 版本化 | ADR-0080 D2 + ADR-0068 | v:1 字段已落盘；payload 校验自动化未 ship |
| **1.2** | message-index 寻址 | ADR-0079 v1.2 修订 | 与 ADR-0079 实施一起解决 |
| **1.3** | branch cursor 语义 | ADR-0079 v1.2 修订 | 与 ADR-0079 实施一起解决 |
| **1.5** | path-extraction fork | ADR-0079 D4 | 已批准，实现细节需修 |
| **3.1** | Agent 非 first-class | ADR-0082（推动定稿） | 需 ADR-0079/0080 实施后解锁搁置 |

### 🟡 P2 — 业务需求驱动

| # | 缺陷 | 主参考 ADR | 备注 |
|---|------|-----------|------|
| **3.3** | Agent↔Agent 协议 | ADR-0060 | 已批准，4/6 模式待 ship |

### 🟢 P3 — 工程改进

| # | 缺陷 | 主参考 ADR | 备注 |
|---|------|-----------|------|
| **1.4** | compact 破坏性 | ADR-0079 §不变量 2 例外 | 有意设计的例外，是否升级需 v1.2 决策 |
| **6.1** | MockBus 重复（9 处） | 无 ADR | 工程债，5h 任务 |

### 🟡 分层部分解决（open gap，非阻塞）

| # | 缺陷 | 已 ship 部分 | 未 ship 部分 |
|---|------|--------------|--------------|
| **4.1** | Plugin scoped 注册 | per-engine ✅（ADR-0022） | per-agent 版本/作用域隔离；marketplace 多租户 |
| **5.1** | Cancellation scope tree | RAII + 协作取消 ✅（ADR-0020） | 统一 scope tree / 父子级联；形式化传播 |

---

## 四、实施路线图

详见独立文档 **`docs/architecture/defect-fix-roadmap-2026-08.md`**，以 rdd-workflow 提案为粒度，共 12 个提案：

| Phase | 提案 | 估时 | 前置 |
|-------|-----|------|------|
| **A** | `adr-0057-amend-lifecycle-events` | 0.5 sprint | 无 |
| **A** | `emit-agent-lifecycle-events` | 1 sprint | 0057 amend |
| **A** | `adr-0081-promote-to-approved` | 0.5 sprint | ADR-0082 解决核心争议 |
| **B** | `event-log-query-api` | 1 sprint | 无 |
| **B** | `session-writer-bridge` | 1 sprint | 0080 Phase 3 |
| **B** | `adr-0079-v1-2-amend` | 1 sprint | 无 |
| **C** | `adr-0082-promote-to-approved` | 1 sprint | 0079 + 0080 ship |
| **C** | `adr-0060-p2-p3-patterns` | 2-3 sprint | 业务需求 |
| **D** | `error-taxonomy-execution-boundary` | 1 sprint | 无（盲点） |
| **D** | `compact-events-emit` | 0.5 sprint | ADR-0068 主题注册 |
| **D** | `otel-exporter-skeleton` | 1-2 sprint | 无（盲点） |
| **E** | `mock-bus-canonical-extract` | 0.1 sprint | 无（工程债） |

**总估时**：约 5-6 个月（Phase A-D）+ 1 周（Phase E）

---

## 五、与已有 ADR 的关系图

```
                      缺陷 4.1/5.1 (分层部分解决)
                            ↓
                ┌───────────┴───────────┐
                ↓                       ↓
        ADR-0022 (ship)            ADR-0020 (ship)
        Plugin (per-engine)        Thread (协作取消)
                │                       │
                └──→ 绕行路径：         │
                     worker-per-engine  │
                     隔离（ADR-0020）    │
                                        ↓
缺陷 3.2 ────────→  ADR-0057 (ship 但零事件契约)
   (需补事件)            │
                        ↓ 需 amendment
                  ADR-0068 附录 A
                  (✅ 但无 agent.* 主题)
                        │
                        ↓ 需主题注册
                  缺陷 3.2 真正修复

                        ↓
              缺陷 1.1/1.2/1.3/1.5 ──→ ADR-0079 v1.1/v1.2
              缺陷 2.1/2.2 ──────→ ADR-0080 v1.1 (大部分 ship)
                                         │
                                         ↓
缺陷 3.1 (搁置) ──→  ADR-0082 (Proposed, 5 个争议未解)
                       搁置条件：ADR-0079 + ADR-0080 ship
                                         ↓
缺陷 4.2 ────────→  ADR-0081 (Proposed, 推迟至 ADR-0082 定稿)
                       推迟条件：ADR-0082 定稿

缺陷 3.3 ────────→  ADR-0060 (Approved, 2/6 模式 ship)
```

**关键路径**：

```
ADR-0057 amendment + ADR-0068 主题注册 (Phase A)
                ↓
      缺陷 3.2 修复 + ADR-0082 搁置解锁
                ↓
   ADR-0079 v1.2 ship + ADR-0080 Phase 3+4 ship (Phase B)
                ↓
       ADR-0082 定稿 (Phase C)
                ↓
   ADR-0081 → Approved → 缺陷 4.2 修复 (Phase A.3)
```

---

## 六、修订说明（v1.0 → v1.1）

### 6.1 关键修订项

| # | v1.0 措辞 | v1.1 措辞 | 修订原因 |
|---|----------|-----------|---------|
| 1 | "ADR-0057 定义了 4 个生命周期事件（spawned/heartbeat/terminated/error）" | "ADR-0057 **未定义任何生命周期发射事件**；`activation_events` 是懒加载触发器而非发射事件" | **Oracle 评审核查为虚构**（ADR-0057 全文 215 行零事件定义） |
| 2 | 缺陷 3.2 P0 理由："零实施的纯增量，无需新设计" | "需 ADR-0057 amendment + ADR-0068 附录 A 注册 + emit 实现，≈1 sprint 最小增量" | 修正"无需新设计"的错误措辞；事件 topic 命名、payload schema 全部需要新设计 |
| 3 | 缺陷 4.1 结论："已解决" | "**分层部分解决**：engine 级 ✅ ship（ADR-0022），agent 级版本隔离未 ship" | per-engine ≠ per-agent（同一 DSLEngine 内 Agent A/B 共享 ToolRegistry）；删/留都是错的，**保留+重写**才对 |
| 4 | 缺陷 5.1 结论："完全解决" | "**分层部分解决**：协作式取消 + RAII 已 ship（ADR-0020），scope tree 未 ship；术语误用" | jthread RAII 是 cooperative cancellation，**不是** true structured concurrency |
| 5 | ADR-0068 状态：🔍 Proposed | ✅ **Approved** (2026-08-03) | 读 ADR 头部确认（v1.0 索引表 line 34 写 Approved 但 §七写 Proposed，自相矛盾） |
| 6 | 缺陷 2.1 状态："EventLogWriter 代码存在但完整功能未 ship（Step 0-4 待启动）" | "Step 0+1+2 大部分 ship（envelope/writer/rotation/engine opt-in/v:1 落盘/4 测试）；缺 query API（Phase 3）+ SessionWriter 集成（Phase 4）" | 读 `event_log.cpp` 182 行确认（v1.0 进度低估约 2 个 Phase） |
| 7 | 缺陷 2.2 状态："落盘 `v:1` 待验证" | "v:1 字段**已落盘**（`event_log.cpp:136`）" | grep 实证 |
| 8 | 缺陷 1.1 SessionRegistry 路径："engine.cpp 内存态" | "`src/core/types/session_registry.{h,cpp}`（独立类）" | `grep` 实证独立类存在 |
| 9 | 缺陷 1.1 g3_state.h 路径："`src/g3/state.h`" | "`pdk/g3_knowledge_base/src/g3_state.h`" | `src/g3/` 目录不存在，**v1.0 路径是幽灵路径** |
| 10 | 缺陷 6.1 MockBus 计数："7 处" | "**9 处**" | grep 命中 v1.0 漏掉的 `test_tool_coordinator.cpp:54` 和 `test_tool_coordinator_hooks.cpp:80` |
| 11 | §一 核心发现段："剩余 5 项缺陷（4.1、4.2-tool hooks 部分、5.1）已被...解决" | "2 项分层部分解决（4.1/5.1）；ADR-0069 tool hooks ✅，agent hooks 搁置待 ADR-0082" | 列出来只有 3 项，不是 5 项（算术错 + 归类错） |
| 12 | §一 算术："14 项中 12 项真实；2 项已解决；1 项工程债" | "11 项真实 + 2 项分层部分解决 + 1 项工程债 = 14" | "12"是写错了，实际是 11 |
| 13 | 缺陷 1.3 用词："leaf pointer" | "branch cursor" | Oracle 指出"leaf"是查询，"cursor"才是位置状态 |
| 14 | 缺陷 1.4 处理 | 增加"时代错位说明"（compact 先于 ADR-0079 存在，"有意例外"是追溯性背书） | Metis 评审发现 |
| 15 | 缺陷 3.3 ADR-0060 状态真伪辨析 | 增加 ADR-0060 决策 4 表格的 ✅ 是 scope 声明而非实施声明的辨析 | Oracle 发现 ADR 自身 dishonest（scope vs implemented） |

### 6.2 新增章节（v1.1 独有）

- **§一 索引校正说明**：明确 ADR 状态源规则
- **§六 修订说明**：v1.0 → v1.1 全部修订项
- **§七 新发现盲点**：3 个新缺陷（错误传播断层、compact 幻影事件、OTel 零代码）
- **§八 验证命令附录**：MockBus grep、EventLogWriter ctest、Session 4-store ls 命令（符合 architecture/README §二"计数数据可复现"强制规范）

### 6.3 已知局限

1. **4 套 session 存储的语义冲突未深入分析**：本文档只列了 4 套的存在，未分析 SessionRegistry 与 SessionManager 命名空间碰撞机制——这是"4 套并存"的真正危险点，留 ADR-0079 v1.2 修订处理
2. **`gap-analysis` 事实源不覆盖 ADR-0079/0080/0081/0082**：grep 证实该文档（2026-07-30）零命中这些 ADR——真相表引用最多的 4 个 ADR 在其宣称的事实源中**不存在**。本文档以 ADR 头部状态为准并标注 README drift
3. **ADR-0060 决策 4 表格的 scope vs implemented 混淆**：本文档明确戳破该 ADR 的 dishonesty——`✅` 列是 scope 声明而非实施声明，真实实施率为 2/6。ADR-0060 应发 amendment 澄清

### 6.4 后续追踪

- **下一修订触发**：(1) ADR-0079 v1.2 写定后；(2) ADR-0080 v1.1 Phase 3+4 ship 后；(3) ADR-0082 定稿后
- **定期审计**：每 Sprint 收官同步（`scripts/sprint-closeout.sh` Step 8 加本表交叉检查）

---

## 七、新发现盲点（v1.1 追加）

> Oracle + Metis 评审发现的本文档**未覆盖**的真实架构缺陷。**仅追加不重新编号**（避免外部引用失效）。

### 盲点 7.1：错误传播断层（ExecutionResult 错误分类不连续）

**背景**

代码中存在**两个同名 `ExecutionResult` 结构体**：

- `src/core/types/budget.h:116`
- `src/core/types/session.h`（或 `execution_session.h:89`）

两者都使用 `std::string message` 承载错误，**无 ErrorCode 枚举字段**。

设计意图（ADR-0033 §D10 + ADR-0023）：执行层使用 ErrorCode 体系（强类型），编排层应自动转换。但 `record_failure`（`session.cpp:62-67`）注释自承"ExecutionResult 没有 error_code 字段…默认失败总是递增（保守策略）"——设计是"仅可重试错误递增"，实现是"全部递增"。

**为什么是缺陷**

1. **错误分类断层**：执行层 ErrorCode → 编排层 stringly-typed → 上层分类逻辑失真
2. **retry 策略保守**：因无法区分错误类型，全部按可重试处理，可能进入 retry 循环
3. **DSH 对标**：DSH `Result<T, E>` 是强类型枚举

**参考内容**：ADR-0023（ToolResult 标准化）+ ADR-0033 §D10（失败模式判定）

**优先级**：P1（与缺陷 1.1/2.1 同步推进）

---

### 盲点 7.2：`context.compact.before/after` 事件幻影

**背景**

ADR-0007（context compaction）✅ Approved + ship 2026-08-13。ADR-0068 附录 A Canonical Topic Registry 中 `context.compact.before/after` 原标 👻——compaction 事件**未发射**（已由代码 emit，文档落后于代码）。

**为什么是缺陷**

1. **compaction 不可观测**：Context 压缩时丢消息，无法审计
2. **与缺陷 2.x 同族**：属于事件契约缺位
3. **ADR-0068 附录 A 需 amendment**：注册 `context.compact.*` 主题

**参考内容**：ADR-0007 + ADR-0068 附录 A + ADR-0080 D2

**优先级**：P1（与缺陷 2.1/2.2 同步推进）

**当前状态（2026-08-20 update）**：✅ **已 ship**（P10 `compact-events-emit`：
- 代码 `src/core/context_compactor.cpp:45-51, 81-89` 已通过 EventBuilder emit `context.compact.before/after`
- ADR-0068 附录 A 👻→✅ + payload 字段对齐（stale `before_tokens`/`after_tokens`/`summary_ref` → 实际代码 `session_id`/`tokens_before`/`tokens_after`/`compression_ratio`）
- `tests/test_context_compact_events_payload.cpp` 4 cases / 29 assertions PASS（before/after payload schema + 配对 + null bus 静默跳过）
- 文档与代码语义已对齐）

---

### 盲点 7.3：ADR-0063 OTel exporter 零代码

**背景**

ADR-0063 ✅ Approved（OpenTelemetry tracing）但**实施 0%**——可观测性出口完全缺失。

**为什么是缺陷**

1. **外部可观测性盲区**：EventLog 是项目内 JSONL，OTel 是工业标准（Jaeger/Prometheus/Tempo）
2. **分布式追踪缺失**：跨进程/跨主机的 agent 追踪无标准出口
3. **ADR Approved 但零实施**：与 ADR-0057 同模式

**参考内容**：ADR-0063 ✅ Approved

**优先级**：P2（业务需求驱动）

---

## 八、验证命令附录

> 符合 `docs/architecture/README.md` §二 "Last-Verified 规则"——文档中所有**计数类数据**必须可用命令复现。

### 8.1 MockBus 计数（验证缺陷 6.1 = 9 处）

```bash
grep -rn "class Mock.*Bus" /workspace/project/HydraForge/tests/ /workspace/project/HydraForge/examples/
```

**预期输出**：9 处命中

### 8.2 EventLogWriter 实施进度（验证缺陷 2.1 = Step 0+1+2 ship，缺 Phase 3+4）

```bash
# 检查 event_log.cpp 行数
wc -l /workspace/project/HydraForge/src/core/event_log.cpp
# 预期：~182 行

# 检查 v:1 字段实际落盘
grep -n '"v".*=.*1' /workspace/project/HydraForge/src/core/event_log.cpp
# 预期：line 136 命中 j["v"] = 1

# 检查 enable_event_log() 集成
grep -n "enable_event_log" /workspace/project/HydraForge/src/core/engine.cpp
# 预期：line 195 命中

# 检查 query API 是否 stub
grep -n "EventLog.*query\|read.*placeholder\|占位" /workspace/project/HydraForge/src/core/event_log.cpp
# 预期：line 178 "占位实现" 命中
```

### 8.3 Session 4-store 路径验证（验证缺陷 1.1 路径正确）

```bash
# SessionManager
ls -la /workspace/project/HydraForge/src/core/session_manager.{h,cpp}

# SessionStore（pdk session_agent）
ls -la /workspace/project/HydraForge/pdk/session_agent/src/session_store.cpp

# SessionRegistry
ls -la /workspace/project/HydraForge/src/core/types/session_registry.{h,cpp}

# g3_state.h（v1.0 幽灵路径 src/g3/state.h 不存在）
ls -la /workspace/project/HydraForge/pdk/g3_knowledge_base/src/g3_state.h
ls -la /workspace/project/HydraForge/src/g3/state.h
# 预期：第一个存在，第二个 NOT FOUND
```

### 8.4 ADR-0057 无事件定义验证（验证缺陷 3.2 = ADR-0057 未定义发射事件）

```bash
grep -n "spawned\|heartbeat\|terminated" /workspace/project/HydraForge/docs/adr/adr-0057-agent-lifecycle.md
# 预期：零命中（仅 `activation_events` 懒加载触发器，非发射事件）

grep -n "agent.spawned" /workspace/project/HydraForge/src/ /workspace/project/HydraForge/include/ /workspace/project/HydraForge/pdk/ -r
# 预期：零命中（无 emit 实现）
```

### 8.5 ADR-0068 状态（验证 = ✅ Approved 2026-08-03）

```bash
head -20 /workspace/project/HydraForge/docs/adr/adr-0068-event-emission-contract.md | grep "状态"
# 预期：✅ Approved
```

### 8.6 ctest 全量验证（验证 EventLogWriter 集成测试通过）

```bash
cd /workspace/project/HydraForge/build
ctest --output-on-failure -R "event_log\|test_tool_coordinator"
# 预期：test_event_log_capture 4 cases + test_tool_coordinator_hooks 8 cases 通过
```

---

## 九、参考内容汇编

### 已批准 + 已实施的 ADR（无需动作）

| ADR | 标题 | 实施证据 |
|-----|----|---------|
| ADR-0020 | 多智能体线程模型与隔离策略 | Sprint 2/3 ship, CognitiveWorker + DomainWorkerPool + 30+ ctest |
| ADR-0022 | 插件加载机制 | Sprint 5 ship (commit `968937f`), PluginLoader + PluginInfo V2 |
| ADR-0033 | 会话层次结构 | Sprint 15 C5 ship, UserSession/TaskSession/SubtaskSession |
| ADR-0041 | PluginLoader 生命周期扩展 | C16 ship, `pdk_plugin_init`/`fini` 钩子 |

### 已批准 + 部分实施的 ADR（待推进）

| ADR | 标题 | 当前实施率 | 待完成 |
|-----|----|----------|--------|
| ADR-0057 | Agent 生命周期管理 | 0%（**无事件契约**——v1.1 校正） | state machine 实施 + **事件定义 amendment** + emit 实现 |
| ADR-0068 | 事件发射契约 | Wave 1 ✅ + 28 主题注册 | agent.* 主题注册（盲点 7.2）+ context.compact.* 注册 |
| ADR-0069 | ToolCoordinator Hook 注入点 | partial (2026-08-04) | §决策 7 条件 5 ctest 零回归 |
| ADR-0079 | 统一会话模型与 4-Scope 存储 | v1.1 ✅ / 实施 0% | SessionWriter Phase 1-5 |
| ADR-0080 | AppendOnlyEventLog as Core | v1.1 ✅ / 实施 ~70%（v1.1 校正） | query API（Phase 3）+ SessionWriter 集成（Phase 4） |
| ADR-0060 | Agent 协作协议 | 2/6 模式 ship（**ADR 表 ✅ 列是 scope 声明**，不是实施声明——v1.1 校正） | call/call_async/delegate/stream |
| ADR-0063 | OTel tracing | 0%（盲点 7.3） | exporter skeleton |

### 🔍 提案状态 ADR（待定稿/解锁）

| ADR | 标题 | 搁置/解锁条件 |
|-----|----|------------|
| ADR-0070 | PDK Plugin 命令注册 | 独立立项，与本表无直接关联 |
| ADR-0071 | LLM-native AgenticDSL 架构 | 顶层方向 ADR，本表缺陷均可受益 |
| ADR-0073 | Tool JSON Schema 契约 | partial，独立 |
| ADR-0074 | Prompt Evidence Gate | Wave 2 Phase 2.2，独立 |
| ADR-0075 | EnvBackend 多环境执行 | ✅ 已 ship (Wave 3-A) |
| ADR-0076 | DSL Engine as MCP Server | Wave 3 末，独立 |
| ADR-0077 | gRPC Data Plane | Wave 4 docs-only，独立 |
| ADR-0078 | Fine-tune 基模与训练管线 | Wave 5+ docs-only，独立 |
| ADR-0081 | Pre-Step Hook Contract | 推迟到 ADR-0082 定稿 |
| ADR-0082 | Agent as First-Class Registry | 搁置至 ADR-0079/0080 实施后 |

---

**审批与维护**：
- v1.0 提议：2026-08-20（本会话）
- v1.1 修订：2026-08-20（Oracle + Metis 独立评审后）
- 维护者：架构组
- 审查频率：每 Sprint 收官
- 与 `docs/architecture/adr-implementation-status-gap-analysis.md` 交叉验证（注意：该文件未覆盖 ADR-0079/0080/0081/0082，本文档以 ADR 头部状态为准）
- 关联修复路线图：`docs/architecture/defect-fix-roadmap-2026-08.md`（12 个 rdd-workflow 提案）