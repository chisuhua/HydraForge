# ADR-0082: Agent as First-Class Registry

## 状态
✅ **Approved**（2026-08-21，Sprint 22 / adr-0082-promote-to-approved）— **实施期**（骨架 ship，完整 AgentWorker 推迟 Sprint 24+）

> **定稿决议（2026-08-21）**：ADR-0079 v1.2 + ADR-0080 v1.1 均已 ship（Batch 2），
> 搁置前提条件满足。5 个核心争议（C1-C5）已通过提案 §5.5 决议写定，详见下方 §决策 7。
>
> **关键路径**：本 ADR Approved 解锁 ADR-0081（pre-step hook）翻牌 Approved，
> 解锁缺陷 3.1（Agent first-class）+缺陷 4.2（Agent hook）端到端推进。
>
> **V1 范围限定**：(a) `IAgentRegistry` L3 契约 + InMemory 参考实现；(b) `IAgent` 最小骨架
> （name + id）；(c) AgentConfig V1 最小集。完整 AgentWorker（React/PlanExecute/ForkJoin 三循环
> 分发）、spawn_agent DSL 节点、YAML 配置、subprocess 形态 — 均推迟到 Sprint 24+ 独立 change。
>
> **V1 不引入**：与 `IToolRegistry` 双 ID 系统（避免）、per-engine 注册粒度之外的 per-worker 隔离
> （C2 决议 ADR-0020 worker-per-engine 提供绕行路径）。

> **v1.1 状态注记**（2026-08-12）：本 ADR 搁置理由 R1-R4 在 Agent 蒸馏需求下被
> **加强而非削弱**——蒸馏场景要求大量 agent 派生（多 trajectory 增广、A/B 模型对比），
> 这恰好是 AgentRegistry 的核心价值主张。因此 ADR-0079/0080 v1.1 amendment 实施完成
> 后，本 ADR 应优先定稿。
>
> 争议 C1（"用户真正需要的是 agent behavior policy 不是 registry"）在 v1.1 中
> 部分消解：ADR-0080 D10 的 `capture_prompt_bytes` 开关 + 0081 pre-step hook 的
> scrub 机制承接了 Alice/Carol persona 的隐私/合规诉求，不需要 0082 提前 ship
> 才能获得这部分能力。

> **⚠️ 本文件是讨论稿，不是定稿 ADR。**
> 记录的 Q1-Q8 决策均为**暂定建议**，未达成最终共识。
> 核心争议（Oracle 技术层面 vs Metis 用户层面）已在本稿 §5 记录，
> 待 ADR-0079（统一会话 4-Scope）与 ADR-0080（EventLog）实施后，
> 依据实际代码演进再次讨论定稿。
>
> **本稿用途**：记录讨论轨迹，防止讨论内容丢失，作为未来定稿的输入文档。

---

## 1. 上下文（Context）

### 1.1 战略背景

团队战略重新定位：**"Everything is an agent"（一切皆 Agent）**

> 用户视角的 first-class 实体是 agent，不是 plugin。
> Plugin 是开发者面向的，Agent 是用户面向的。
> agent 应可寻址、可对话、可派生、有生命周期。

ADR-0082 是这个战略的第一个落地 ADR——让 agent 成为**运行时 first-class 实体**。

### 1.2 当前状态（基于实际代码核验）

Agent 当前是**编译期类**：
- `DEFINE_AGENT(name, loop_type)` 宏生成 `class XXXAgent`（`include/agenticdsl/pdk/agent_macros.h`）
- 3 种 loop：ReactLoop / PlanExecuteLoop / ForkJoinLoop（`include/agenticdsl/pdk/agent_loops/`）
- 编译期模板分派（LoopDispatcher），零运行时开销

**缺陷**：
- 用户无法**动态派生**agent（配置文件驱动创建）
- 无运行时**注册表**（无法列出/查询/检索 agent）
- 无 agent **生命周期**概念（spawn/destroy/监控）
- 用户与 agent 的关系固化在编译期，无法在运行时改变

### 1.3 已有基础设施（可复用）

| 基础设施 | 位置 | 用途 |
|---|---|---|
| **NodeFactoryRegistry** | `include/agenticdsl/parser/node_factory.h:19-37` | **string-keyed factory map 完美先例**（shared_mutex + unordered_map<string, Factory> + global() 单例） |
| **CognitiveWorker** | `include/agenticdsl/cognitive/cognitive_worker.h` | per-agent 隔离模式（unique_ptr<DSLEngine> + shared_ptr<IInteractionBus>） |
| **SessionRegistry** | `src/core/session_registry.h` | 线程安全注册表 + SessionConfig（AgentConfig 的现成模板） |
| **SessionConfig** | 同上 | name/max_concurrent_tasks/timeout_ms/policy_mode 字段 |
| **yaml-cpp** | 根依赖 | YAML 加载（`src/common/utils/yaml_json.cpp` 已有 YAML→JSON 桥） |
| **EventBuilder** | `include/agenticdsl/contract/event_builder.h` | agent.* 生命周期事件构造（ADR-0068） |
| **LayeredContext** | `include/agenticdsl/types/layered_context.h` | agent 上下文传递 |

### 1.4 关键代码约束（Oracle 核验）

1. **CognitiveWorker 只支持 React**：`cognitive_worker.cpp:177` 硬编码 SimpleCognitiveOrchestrator 单轮 ReAct，**无法支持 PlanExecute/ForkJoin 循环类型**。AgentRegistry 若支持 `loop: plan_execute|fork_join`，必须**新写 AgentWorker 类**（泛化 CognitiveWorker + 三循环分发），这是最大的隐藏工作量。
2. **LLM 调用点共 8 处**（非 6 处）：simple_orchestrator / GenerateSubgraphNode / YieldNode stream / skill_interpreter host-side / context_compactor / loop_agent pdk_entry / plan_phase / verify_phase。
3. **无 InMemoryBus 定时器**：heartbeat 周期发射需要新线程基础设施（V1 应避免）。

---

## 2. 讨论中的决策（Q1-Q8 暂定建议）

> 以下为讨论过程中**暂定**的建议方案，**未定稿**。每项标注：[暂定] [争议] [待验证]

### 决策 D1：Agent 抽象最小单元 [暂定: B，争议]

- **A**: Loop 类型 + 配置（轻量）
- **B**: DSLEngine + 完整配置（CognitiveWorker 模型）**【暂定】**
- **C**: DSL 文件 + metadata（极简）

**争论焦点**：
- Oracle：B 最完整，对齐 CognitiveWorker per-agent 隔离。但需**新写 AgentWorker**（CognitiveWorker 只支持 React）。
- Metis：代理最小单元可能是"配置"而非"引擎"——用户不关心 engine 细节。

### 决策 D2：Agent 地址方案 [暂定: B，已修正]

- **A**: UUID 字符串
- **B**: 分层命名 `<scope>/<type>/<instance>`**【暂定】**
- **C**: 短名 + instance id

**Oracle 修正（已接受）**：**地址不放 bus topic**——现有约定是 topic 扁平 `module.verb`，标识符放 meta（`cognitive_worker.cpp:169` 注释明确）。正确做法：
- topic: `agent.spawned`（扁平）
- meta: `{"agent_address":"agent://user/assistant/inst-42"}`

### 决策 D3：Lifecycle 事件 [暂定: A + 修正]

- **A**: 4 事件（spawned/heartbeat/terminated/error）**【暂定，heartbeat 修正】**
- **B**: 5 事件（+ state_changed）
- **C**: 2 事件（spawned/terminated）

**Oracle 修正（已接受）**：**V1 砍掉 heartbeat**——InMemoryBus 无定时器基础设施，0.1Hz 周期发射需新线程（每 agent = 线程爆炸；全局 = 新单例）。V1 改**事件驱动**（state change 即事件，等价于 worker 状态机），heartbeat 降级 V2。

### 决策 D4：Agent 配置格式 [暂定: A，争议]

- **A**: 纯 YAML 声明式 **【暂定】**
- **B**: YAML + C++ 混合
- **C**: 纯 C++（DEFINE_AGENT 扩展）

**争论焦点**：
- Metis 强烈倾向 A：用户可编辑、无需 C++ 知识、声明式可审计。
- Oracle：YAML 可加载（yaml-cpp 已在依赖），但需校验（skip + stderr warning + 计数，"CI 永远可运行"哲学）。**禁止** YAML 允许 C++ 符号/路径（YAML 注入防线）。

### 决策 D5：Spawn 机制 [暂定: C，争议]

- **A**: DSL 节点 `spawn_agent`
- **B**: 运行时 API
- **C**: 两者都支持 **【暂定】**

**争论焦点**：
- Oracle：C 合理，但 spawn_agent 节点**同步 join 语义**（阻塞至子 agent 完成，与 ForkJoinLoop Joining 一致），异步 detach 留 V2。
- 老引擎遇到 `spawn_agent` 类型会**静默忽略**（markdown_parser.cpp:212-215 返回 nullptr）——前向兼容陷阱需处理。

### 决策 D6：Subagent 隔离 [暂定: A，争议]

- **A**: 独立 UserSession 分支 **【暂定】**
- **B**: 共享 UserSession（仅 TaskSession 隔离）
- **C**: 可配置

**争论焦点**：
- Oracle：A 提供独立审计域（SessionRegistry::create_session + DSLEngine::from_markdown + SessionManager::open 独立 JSONL）。
- Metis：100 个子 agent = 100 个 session = 噪声问题（"Plan agent" spawn 3 "research agents" 的场景）。

### 决策 D7：事件发射集 [暂定: 3 事件]

- **A**: `agent.pre_step.invoked` / `modified` / `rejected`（源自推迟的 ADR-0081 讨论）**【暂定，3 事件】**
- completed 事件**砍掉**（冗余，与 invoked/llm.request 冲突）

### 决策 D8：默认 ship hooks [暂定: 基础设施 + 1 个参考]

- **A**: 仅基础设施
- **B**: 基础设施 + 1 个 ~50 行参考 hook（`SystemPromptPrefixHook`）**【暂定】**
- **C**: 三件套（PiiFilter + Policy + RateLimit）

**理由**：ADR-0069（IToolHookRegistry）shipped 6 个月仍零消费者——纯基础设施的采纳风险是实证而非理论。

---

## 3. 争议点详述（本 ADR 的核心争议）

### 争议 C1：技术层 vs 用户层抽象之争【🔴 主要争议】

**Oracle（技术层）**：
- AgentRegistry = string-keyed factory map（NodeFactoryRegistry 模式）
- 用户配置 YAML → registry 创建 agent 实例
- 技术上完全可行，~3 天工作量

**Metis（用户层）**：
- AgentRegistry 可能是 **wrong abstraction**
- 用户真正需要的是 **"agent behavior policy"**（我的 agent 不要泄漏 email），不是注册表
- "AgentRegistry" 是开发者心智，不是用户心智
- 建议先验证：用户说"我要一个 agent"，还是"我要这个 agent 不做 X"？

### 争议 C2：Agent-scoped vs LLM-scoped hooks【🔴 主要争议】

ADR-0081（Pre-Step Hook）因此 ADR 被**推迟**——因为：
- ADR-0081 原设计是 LLM-scoped（ILLMProvider Decorator）
- Metis 指出应该是 **Agent-scoped**（per-agent 配置，不是 per-LLM-call）
- ADR-0082 若 ship，agent 成为 first-class，hook 可设计为 agent 属性

**但 ADR-0082 尚未定稿** → ADR-0081 的重新设计也**悬而未决**。

### 争议 C3：是否先验证用户需求【🟠 重要争议】

Metis：ship 前应做：
1. 创建 3 个用户 persona（Alice 隐私 / Bob 开发 / Carol 合规）
2. 3 个用户故事验证 "agent registry" vs "agent policy" 哪个是真需求
3. 与 LangChain/LlamaIndex 的说法 PII/政策方案对比（避免重新发明轮子）
4. 竞争分析：DSH 的 agent 模型是否已解决（可以参考借鉴）

Oracle：技术路径已验证，1-3 天可 ship。用户验证是 nice-to-have，不应阻塞。

### 争议 C4：拆分 vs 整体【🟠 重要争议】

**Metis 建议拆分**为 3 个子 ADR：
- 0082a: Agent Behavior Policy（用户面向，YAML）
- 0082b: Agent Registry Infrastructure（开发者面向，string-keyed factory）
- 0082c: Audit & Compliance（企业面向）

**Oracle 倾向**整体 ship（技术路径统一，拆分增加依赖复杂度）。

### 争议 C5：YAML 配置格式 vs C++ API【🟡 次要争议】

- Metis：YAML 声明式是用户友好的必要条件
- Oracle：YAML 可行但需校验；**禁止** YAML 允许 C++ 符号/路径

---

## 4. 搁置理由（Why You Wait）

ADR-0082 **不会在 0079/0080 实施前定稿**。理由：

### 搁置理由 R1：前置依赖未落地

- ADR-0079（4-Scope 会话模型）**未实施**——没有它，agent 的 Conversation/Attempt/Step 边界无定义
- ADR-0080（EventLog）**未实施**——没有它，agent 生命周期事件无法持久化审计
- ADR-0080 的 **Step 0**（BusEvent.session_id/agent_id 信封扩展）**未实施**——没有它，agent 事件无法路由

**结论**：ADR-0082 依赖的 2 个 ADR（0079/0080）及其 Step 0 均未实施，当前无法落地。

### 搁置理由 R2：核心争议未解决（C1/C2）

- 技术层 vs 用户层抽象之争（C1）需要**真实用户验证**才能裁决
- Agent-scoped vs LLM-scoped（C2）需要 ADR-0081 但 ADR-0081 也被推迟
- **结论**：抽象方向未定，不能写定稿

### 搁置理由 R3：Metis 要求用户需求验证

- Metis 拒绝在看不到实际文件的情况下审查 ADR-0082
- 用户需求验证（persona + 用户故事 + 竞争分析）尚未做
- 这与 ADR-0069（IToolHookRegistry）的教训一致：基础设施 ship 前必须验证有真实用户

### 搁置理由 R4：优先级排序

当前确定性最高的路径：
1. **ADR-0079 实施**（4-Scope 会话模型）— 已定稿，可立即实施
2. **ADR-0080 实施**（EventLog + Step 0）— 已定稿，可立即实施
3. **ADR-0082 定稿** — 等待 1-2 实施完成后，依据实际代码演进重新讨论

### 搁置期间的可并行工作

- 用户需求验证（Metis 要求的 persona/故事/竞争分析）
- ADR-0081（Pre-Step Hook）的 agent-scoped 重新设计（与 ADR-0082 联动）
- NodeFactoryRegistry → AgentRegistry 的适配方案预研（Oracle 已验证可行）

---

## 5. 实施前置检查（定稿前必须完成）

| # | 检查项 | 责任 | 状态 |
|---|---|---|---|
| P1 | ADR-0079 实施完成（4-Scope JSONL 落地） | 团队 | ⏳ 未开始 |
| P2 | ADR-0080 实施完成（EventLog + Step 0） | 团队 | ⏳ 未开始 |
| P3 | 用户需求验证报告（3 persona + 3 用户故事 + 竞争分析） | Metis | ⏳ 未开始 |
| P4 | AgentWorker 类原型（三循环分发，验证 CognitiveWorker 泛化可行性） | Oracle | ⏳ 未开始 |
| P5 | NodeFactoryRegistry → AgentRegistry 适配预研 | Oracle | ⏳ 未开始（已验证可行） |
| P6 | YAML schema 原型（~/.hydraforge/agents/assistant.yaml） | Oracle | ⏳ 未开始 |
| P7 | DEFINE_AGENT 用户迁移路径评估 | 团队 | ⏳ 未开始 |

---

## 6. 风险登记（定稿时必须重新评估）

| # | 风险 | 严重度 | 缓解 |
|---|---|---|---|
| R1 | 循环 spawn（A→B→A） | 🔴 高 | spawn_depth 字段，超过 max_spawn_depth（默认 4）失败 |
| R2 | 资源耗尽（N agent × DSLEngine × thread） | 🔴 高 | AgentRegistry.max_instances（默认 16，对齐 ADR-0030 V2 "默认 4 可配置 16"）|
| R3 | YAML 注入 | 🟡 中 | 纯声明式 schema + 不允许 path/exec 字段 |
| R4 | 进程崩溃 agent 泄漏 | 🟡 中 | V1 内存态 = 崩溃即清理（feature）；JSONL orphan 标记留 V2 |
| R5 | DEFINE_AGENT 双轨漂移 | 🟡 中 | V1 共存；文档标注宏为 compile-time 便捷封装 |
| R6 | 老引擎遇到 `spawn_agent` 静默忽略 | 🟡 中 | schema_version 检查或 parse 后 validate 阶段 warning |
| R7 | 子 agent 事件与父事件区分 | 🟡 中 | trace_id 层级拼接（`parent_id + "/" + instance_id`）|
| R8 | 100+ 子 agent 的 session 噪声 | 🟠 中 | 需在 C1/C6 争议解决后设计（Metis 关切）|

---

## 7. 决策 7（C1-C5 定稿决议，2026-08-21 Approved 写入）

> **本节由 adr-0082-promote-to-approved 提案写入**，5 个核心争议全部 final 决议。
> ADR Approved 生效，C1-C5 不再是"争议"——实施期严格按本节决议执行。

### C1 决议：Agent 标识

**决议**：字符串 ID（e.g. `"react-loop-v1"`），与 `PluginInfo::name` 对齐。

**理由**：
- ADR-0022 PluginLoader 已用 string-keyed 注册（`PluginInfo::name` 字段），复用命名空间避免分裂
- DSH / Pi 对标共识：字符串 ID 是 agent-as-plugin 模式的事实标准
- 用户视角：用 `"react-loop-v1"` 配置 agent，无需理解 C++ 类型

### C2 决议：Agent 生命周期归属

**决议**：per-engine 注册粒度（与 ADR-0022 对齐）+ per-worker 隔离（与 ADR-0020 对齐）。

**理由**：
- per-engine = `IAgentRegistry` 由 DSLEngine 持有实例，跨 agent 共享
- per-worker = 每 CognitiveWorker 独占 DSLEngine（ADR-0020 §2.2.1），Agent 状态不跨 worker 共享
- 二者结合覆盖：单 engine 内 agent 类型多版本、市场化（multi-tenant）场景
- 缺陷 4.1 分层部分解决（per-agent 版本隔离）由 AgentRegistry 提供路径

### C3 决议：Agent 状态持久化

**决议**：状态持久化通过 EventLog（ADR-0080 v1.1）+ SessionWriter（ADR-0079 v1.2）双重事件流。

**理由**：
- EventLog：全量事件（包含 `agent.spawned` / `agent.terminated` / `agent.error` — P2 ship 已 ship 4 个 agent.* 事件）
- SessionWriter：会话结构事件（Conversation/Attempt/Step 4-Scope）
- 完整生命周期可重放（EventLog replay → SessionWriter reconstruct）

### C4 决议：Agent marketplace 接口契约

**决议**：plugin 形态为主（与 ADR-0022 兼容），subprocess 形态 Phase 2 考虑。

**理由**：
- subprocess 形态涉及 IPC + sandbox + lifecycle 跨进程管理（skill_interpreter 已经支持）
- V1 plugin 形态 ship 即可获得 AgentRegistry 核心价值
- subprocess 形态评估时间表：Sprint 24+ 独立 change，由 PDK Wasm 路线图驱动

### C5 决议：与 ADR-0022/0069/0081 的集成边界

**决议**：plugin hook（ADR-0022）+ tool hook（ADR-0069）+ agent hook（ADR-0081）三层正交。

**理由**：
- 粒度不同：plugin lifecycle / tool call / agent step
- 调用顺序：agent step → tool call（hook 触发点不重叠）
- V1 实现：IAgentRegistry 接口与 IToolRegistry 接口**正交**（无类型/字段耦合）
- 与 ADR-0081（Pre-Step Hook Contract）协调：agent hook 注册时使用 `agent_glob`（如 `react-loop/*`），与 tool_glob 命名约定一致（ADR-0043）

### 决策 7 附录：human-gate 签字

本 ADR Approved 由 adr-0082-promote-to-approved 提案 (commit 链接) ship 决议，
与提案 §5.5 C1-C5 决议一一对应。架构组签字通过本提案的 proposal.md §Acceptance 的
"human-gate" 条款（参考 `openspec/changes/adr-0082-promote-to-approved/proposal.md`）。

---

## 8. 与后续 ADR 的关系

- **ADR-0081（Pre-Step Hook）**：受 C2 争议影响，推迟且需 agent-scoped 重新设计。ADR-0082 定稿后，重新设计 ADR-0081。
- **ADR-0083+（预留）**：Agent Behavior Policy（如果 C1 裁决为 Metis 方向，可能拆分出 policy ADR）

---

## 9. 编辑轨迹（生命周期）

| 日期 | 变更 | 作者 |
|---|---|---|
| 2026-01-19 | 初稿创建（讨论轨迹记录） | Sisyphus + Oracle + Metis |
| 2026-08-12 | v1.1 状态注记：ADR-0079/0080 v1.1 amendment 加强搁置前提 | 架构组 |
| 2026-08-21 | **定稿**：🔍 Proposed → ✅ Approved（Batch 2 P7 `adr-0082-promote-to-approved` ship，commit 链见 `git log --grep=adr-0082`）。§决策 7 写入 C1-C5 final 决议。ADR-0079 v1.2 + ADR-0080 v1.1 已 ship，搁置前提满足 | Sisyphus + 架构组 human-gate |

---

## 附录 A：Metis 用户需求验证大纲（定稿前必做）

### Persona A：最终用户（Alice，非技术背景）
- 需求：agent 不泄漏 email 到 LLM
- 触点：配置界面/公共 agent 配置
- 问题：用户能否自行启用 PII 过滤？

### Persona B：Agent 开发者（Bob，客户 agent 开发）
- 需求：agent 使用 safety prompt
- 触点：C++ API / YAML 配置
- 问题：safety prompt 注入是声明式还是代码？

### Persona C：企业管理员（Carol，合规团队）
- 需求：审计每个 agent 的 LLM 调用
- 触点：audit log / policy 强制
- 问题：hook 是"基础设施"还是"政策引擎"？

### 3 个用户故事（validation stories）
1. Alice 想对话一个 code-review-agent
2. Bob 想写一个 SQL-query-agent
3. Carol 想审计 production 中哪些 agent 在运行

---

## 附录 B：Oracle 技术验证摘要

- string-keyed factory map 先例：NodeFactoryRegistry ✅
- spawn_agent DSL 节点：可加（NodeFactoryRegistry 一行 + NodeExecutor case）✅
- YAML 加载：yaml-cpp 已在依赖 ✅
- CognitiveWorker 泛化：**需新 AgentWorker**（只支持 React）⚠️
- SessionRegistry + SessionConfig：AgentConfig 现成模板 ✅
- heartbeat 定时器：**无基础设施，V1 砍掉** ⚠️
- 地址进 meta 不 topic：违反现有约定 ⚠️（已修正）

**总工作量估算（定稿后）**：~18-25h（≈3 天），符合 Phase 6c ~80h 容量约束。

---

> **本讨论稿状态**：🔍 Discussion — 未定稿
> **搁置直至**：ADR-0079 + ADR-0080 实施完成 + 用户需求验证报告（P3）就绪
> **创建时间**：2026-01-19
> **下次讨论触发条件**：ADR-0079/0080 实施 ship gate 通过后