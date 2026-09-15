# ChatSession PDK 化设计文档

**日期**: 2026-09-11
**状态**: 🔄 Draft — 等待用户 review
**作者**: Sisyphus (brainstorming skill)
**Oracle 咨询 session**: `ses_f6f416e91ffeoYWS5S8eXWq8nj` (2026-09-11)

---

## 1. 背景与目标

`examples/pdk_chat_demo/chat_session.{h,cpp}`(**实测 218 + 897 = 1115 行**, 2026-09-11 校对; 原 629 行估算错)是 PDK Chat Demo 应用的核心,已 ship 28 个测试二进制(其中 1 个 `[realllm]` 真实 LLM 测试)。但 ChatSession 留在 examples 树,**不可被外部 PDK 消费者复用**,违背 ADR-0021 §3.5 "PDK 头文件可独立分发"的契约。

**目标**:把 ChatSession 提取到 PDK,作为 PDK 原语,外部消费者可通过 `find_package(hydraforge_pdk)` 链接复用。同时**抽象 I/O 边界**(解 std::cin TTY 死锁, Pattern 5 fail-safe 默认值),**复用既有 contract 层**(`IAgentComposition`/`IInteractionBus`/`SessionManager`),**不引入新的抽象类型**。

**非目标**:
- 不新增 `IChatChannel` 抽象(直接调 `IAgentComposition::call()`)
- 不实现多租户 session 池(由调用方持有)
- 不引入 TokenBucket 限流(已有 ship 的有界队列 + overflow-reject 已够)
- 不做 AgentMailboxInputSource(后续 Sprint)

---

## 2. 决策(基于 Oracle 裁决)

### D1. ChatSession 实例 = 独占单会话(Oracle A 节)

**证据**: `chat_session.h:150-202` ChatSession 持有强 per-session 状态(steering_queue/follow_up_queue/next_model_/current_cancellation_id_/budget_alert_flag_)。多 tenant 共用一个实例 = 队列交叉污染 + cancellation token 误杀他人 turn。

**裁决**:
- 不进 PDK 多租户抽象
- 调用方(`main.cpp` 或 service 层)持有 `unordered_map<tenant_id, unique_ptr<ChatSession>>` 池
- `chat(input)` 签名**不加** sender_id/channel_id

### D2. Agent → Agent messaging 走 IAgentComposition(Oracle A 节)

**证据**: `include/agenticdsl/contract/iagent_composition.h:44-68` 已提供 `call/call_async/delegate/stream` 四种 Agent→Agent 调用模式(ADR-0082 系)。**"Agent 给 Agent 发消息"已被抽象,禁止重新发明。**

**裁决**:
- 不引入 `IChatChannel` 薄包装
- Agent 若要"注入人类侧输入",由目标 Agent 自己的 IInputSource 实现(如 `AgentMailboxInputSource`)从 composition 层拉取 — IInputSource 的**一个实现**,不是新抽象

### D3. 丝滑交互 = 现有双队列已够(Oracle B 节)

**证据**: 双队列 + single-reader 已 ship(`chat_session.h:150-173`):steering 优先 pop、try_peek_input 支持 /cancel 不丢消息、有界 capacity=32 + overflow 拒绝、stop_token 全链路(Wave 3-A 5 步)。

**裁决**:
- 不引入 TokenBucket
- 在 PDK 化契约中把**优先级规则写死为规范**(steering 恒优先于 follow_up + request_stop 不经队列)
- 防止实现漂移

### D4. 断线恢复 = 重放 messages,不持久化 provider/stop_token(Oracle C 节)

**证据**: `src/core/session_manager.h:177` `flush_append` 每行 write+fsync(崩溃安全,等价 WAL);`build_context_entries(leaf)` 叶到根重建上下文;`open()` + `load_jsonl()` + legacy 迁移已齐备。LLM provider 无状态(HTTP 请求级),stop_token 是易失运行时对象。

**裁决**:
- 恢复 = 重放 messages,**不**恢复 provider/stop_token
- **每条消息立即 persist**(现状已是,fsync 每行开销可接受)
- ResumeToken `{session_id, leaf_node_id, model, budget_used}` 作为构造参数

### D5. chat() 保持单 user-turn(Oracle LoopAgent 裁决, 2026-09-11 session `ses_f6f4030c1ffeTwMmy88166EeEG`)

**证据**:
- `chat_session.cpp:377-487` — `chat()` 已通过 `registry->call_tool("loop/run", loop_args)` 委托给 loop_agent 插件
- `pdk/loop_agent/src/pdk_entry.cpp:242-296` — loop_agent 不直接用 ReactLoop C++ 类,而是 `DSLEngine::from_markdown + child->run(ctx)`,turn 内循环由 `.agent.md` DSL 承载
- ReactLoop `unique_ptr<DSLEngine>` 所有权(`react_loop.h:59`)与 ChatSession 共享 engine 冲突

**裁决**:
- `chat()` = "1 次 user 输入 → 1 次 agent 执行",turn 内循环归 DSL,边界外循环归调用方
- 不引入 `IChatLoopAdapter`(YAGNI)
- 不在 chat() 内嵌 ReactLoop(避免双重控制流)
- 若未来 C++ loop 类需在 chat turn 内可用,正确的挂载点是 **loop_agent 插件内部**(`loop_type` → ReactLoop/PlanExecuteLoop/ForkJoin 工厂),而非 chat_session

**升级触发条件**:
- ≥2 个调用方各自手写"chat + loop-class"胶水
- `.agent.md` DSL 无法表达所需循环(如动态 branch 数 ForkJoin)

### D6. 命名空间与 ADR 编号治理(Oracle 2026-09-11 二轮审查)

**D6.1 命名空间表述**:
- ChatSession 公开类型 → **`namespace hydraforge::pdk`**(扁平,无 `::chat` 子命名空间)
- 契约头文件 → 位于 `include/agenticdsl/contract/`(目录名),**`namespace agenticdsl`**(扁平,与既有 IAgentComposition/IInteractionBus 一致)
- **不**为契约新引入 `::contract` 子命名空间(避免破坏 ADR-0021 既有契约层一致性)

**D6.2 ADR 编号治理**:
- 本设计派生 ADR = **ADR-0088**(先 `python3 tools/adr_lint.py` 确认,2026-09-11 实测最大编号 0087 in-flight)
- ADR-0081/0087 已被占用(`adr-0081-pre-step-hook-contract` / `adr-0087-cloud-adapter-threading-root-cause`)
- 同步 `docs/active-status.md` 视图层(README §ADR 状态唯一事实源声明)

### D7. ResumeToken 与 ADR-0079 4-Scope 对齐(Oracle 二轮审查)

**ADR-0079 已 Approved**(2026-08-12 v1.1 amendment),统一会话模型 = Conversation / Attempt / Step / Execution 四 Scope。

**ResumeToken `{session_id, leaf_node_id}` 映射**:
| ResumeToken 字段 | ADR-0079 对应 Scope | 关系 |
|---|---|---|
| `session_id` | **Conversation** Scope | 全局唯一标识,跨 Attempt |
| `leaf_node_id` | **Attempt** Scope(分支叶节点) | 树状重建 `build_context_entries(leaf)` 入口 |
| `model` | **Step** Scope | provider 一致性验证(per-step) |
| `budget_used` | **Execution** Scope | 当前 Attempt 累计值 |

**关键澄清**:ChatSession 持有 `leaf_node_id` 是 **Attempt 入口引用**,不实现装配策略本身(那是 LayeredContext / ContextCompactor / ADR-0083 IEvaluator 的职责)。ChatSession 仅承担**装配入口**,不二次实现装配逻辑,避免 ADR-0079 v1.2 amendment 时返工。

### D8. SessionWriter 序列化锁交互契约(Oracle 二轮审查,2026-09-13 SessionWriter 真实 race 修复后)

**证据**:`src/core/session_writer.{h,cpp}` 2026-09-13 ship 真实 race fix(`file_mutex_` + `flush_sync` 锁获取顺序必须在 `snapshot.empty()` 检查之前)。ChatSession 的"每消息 fsync"路径与 SessionWriter 存在**每消息级竞争**。

**锁顺序契约(写死为规范)**:
```
ChatSession::chat() 入口锁顺序:
  1. steering_queue_.mtx / follow_up_queue_.mtx (双队列锁)
  2. messages_.mtx (消息历史锁)
  3. session_writer.file_mutex_ (持久化锁, 通过 append_to_branch)
  ↓ 禁止反向持有
```

**违反后果**:SessionWriter 的 file_mutex_ 被反向持有时,`flush_loop` 后台写线程会死锁。

**验收标准**:CMake TSan preset (`-DAGENTICDSL_BUILD_TSAN=ON`) 下 `test_chat_session_recovery.cpp` PASS,零 data race 警告。`Dockerfile.tsan` 已 ship,启用成本极低。

### D9. 最小核心 API(YAGNI 边界)(Oracle D 节)

> **2026-09-11 Momus 修订**: `try_push_*_for_test` 从"丢弃"列**回移 P0**(test-only helper 保留)。原因:`InMemoryInputSource::push_for_test` 经异步 input thread 消费,overflow 时序不可控,无法确定性测试有界队列的 capacity=32 拒绝行为。原 2 个既有测试(`test_chat_session_consumer.cpp` + `test_chat_session_queues.cpp`)+ Change 1 Task 13 overflow 测试必须保留直接 push shim。

| P0(必须 lift, 14 个方法) | P1(后续 Sprint, 5 个) | 丢弃(4 个) |
|---|---|---|
| 构造签名(7 参数) | `request_model_switch` / `next_model` | `cleanup_stale`(挪运维工具) |
| `chat(input, stop_token)` | `queue_size` / `try_clear_queue` | `~/.hydraforge/` 默认值(改必填) |
| `request_stop` | `load_from_disk` / `save_to_disk` | `list_sessions`(改 SessionManager::list_sessions) |
| `session_id` | `consume_budget_alert` | `provider_switch_stub` tool |
| `history` | `try_pop_input` 系列 | |
| `is_input_thread_shutdown` | | |
| `load_from_disk`(归 ResumeToken 后再删) | | |
| **`try_push_steering_for_test`**(test-only,[[deprecated]]) | | |
| **`try_push_follow_up_for_test`**(test-only,[[deprecated]]) | | |

---

## 3. 架构总览

```
                    ┌─────────────────────────────────────────────┐
                    │  PDK Layer (新,本设计)                       │
                    │                                              │
   用户 / 其他 Agent │   ┌──────────────────┐    ┌──────────────┐  │
        ───────────►│   │ IInputSource      │    │ ILogger      │  │
                    │   │  (read_line,      │    │  (info/warn/ │  │
                    │   │   has_input)      │    │   error)     │  │
                    │   └────────┬──────────┘    └──────┬───────┘  │
                    │            │                       │          │
                    │            ▼                       ▼          │
                    │   ┌──────────────────────────────────────┐   │
                    │   │  ChatSession  (lifted, namespace     │   │
                    │   │   hydraforge::pdk)                   │   │
                    │   │  - 双队列 (steering / follow-up)     │   │
                    │   │  - request_stop() (CT 直接)          │   │
                    │   │  - chat(input, stop_token)           │   │
                    │   └────────┬─────────────────────────────┘   │
                    │            │                                  │
                    │            ▼                                  │
                    │   ┌──────────────────────────────────────┐   │
                    │   │  CancellationRegistry (提升到 PDK)    │   │
                    │   │  + g_chat_session 全局                │   │
                    │   └────────┬─────────────────────────────┘   │
                    └────────────┼─────────────────────────────────┘
                                 │
                                 ▼
                    ┌─────────────────────────────────────────────┐
                    │  既有 Contract Layer (复用,不重写)          │
                    │  ┌──────────────┐  ┌──────────────────┐   │
                    │  │ IInteraction │  │ IAgentComposition│   │
                    │  │ Bus (chat.*  │  │ (Agent → Agent    │   │
                    │  │ topic 新增)  │  │  messaging)       │   │
                    │  └──────────────┘  └──────────────────┘   │
                    │  ┌──────────────────────────────────────┐   │
                    │  │  SessionManager (持久化,WAL per fsync)│   │
                    │  └──────────────────────────────────────┘   │
                    │  ┌──────────────────────────────────────┐   │
                    │  │  CrossCuttingOrchestrator            │   │
                    │  │  (decorator/hook/composition/bus)    │   │
                    │  └──────────────────────────────────────┘   │
                    └─────────────────────────────────────────────┘
```

---

## 4. 组件清单(2-change 拆分)

### Change 1: I/O 抽象 + ChatSession 重命名迁移(估时 1-2d, ≤ 500 行 diff)

| 新增/修改 | 路径 | 内容 |
|-----------|------|------|
| **新增** | `include/agenticdsl/contract/iinput_source.h` | `IInputSource` 接口(见 §6.1) |
| **新增** | `include/agenticdsl/contract/ilogger.h` | `ILogger` 接口(仅供测试注入,见 §6.1 A1) |
| **新增** | `src/common/io/stdin_input_source.h/.cpp` | 默认实现:**保留 self-pipe + `poll(2)` 多 fd 架构**(Sprint 31 死锁修复不能回退,见 §6.1 A3) |
| **新增** | `src/common/io/stderr_logger.h/.cpp` | 默认实现:桥接到 `agenticdsl::log::emit(Level, std::string)`(`log.h:52` 单一入口,无 log::info/warn/error/debug 自由函数) |
| **新增** | `tests/test_helpers/in_memory_input_source.h` | **A2 修订**:测试 double 放 tests/test_helpers/(Pattern #5 + http_mock_server.h 惯例),**不放 `src/common/io/`** |
| **新增** | `tests/test_helpers/capturing_logger.h` | 同上,捕获 log 调用 |
| **保留** | `examples/pdk_chat_demo/tests/test_helpers/real_llm_env.h` | Frozen 副本不动(Pattern #5 双维护策略) |
| **迁移** | `include/agenticdsl/pdk/cancellation_registry.h` | 从 `examples/pdk_chat_demo/cancellation_registry.h` 提升,namespace `hydraforge::pdk` |
| **迁移** | `include/agenticdsl/pdk/chat_session.h` | 重命名 + 提取 `examples/pdk_chat_demo/chat_session.h`,namespace `hydraforge::pdk`,构造签名加 `unique_ptr<IInputSource>` + `unique_ptr<ILogger>` |
| **迁移** | `pdk/chat_session/src/chat_session.cpp` | 实现移过来,移除 `std::cin`/`std::cerr` 直接调用,改用 IInputSource/ILogger |
| **新增** | `pdk/chat_session/CMakeLists.txt` | OBJECT 库 + add_catch_test 注册 |
| **修改** | `pdk/CMakeLists.txt` | `add_subdirectory(chat_session)` |
| **修改** | `include/agenticdsl/pdk/pdk.h` | 添加 `#include <agenticdsl/pdk/chat_session.h>` |
| **修改** | `examples/pdk_chat_demo/main.cpp` | 构造时显式传 `make_unique<StdinInputSource>()` + `make_unique<StderrLogger>()` |
| **修改** | `examples/pdk_chat_demo/commands/command_globals.{h,cpp}` | `g_command_session` → `hydraforge::pdk::g_chat_session`(兼容 1 Sprint,加 `[[deprecated]]`) |
| **新增** | `tests/test_pdk_chat_session.cpp`(**2026-09-11 Momus 复核 Item 9 修订**: 加 `pdk_` 前缀避免与 examples 同名 target 冲突) | L1 mock-first 测试(10 个 case,见 §7.2) |

### Change 2: 横切集成 + 断线恢复(估时 0.5-1d, ≤ 400 行 diff)

| 新增/修改 | 路径 | 内容 |
|-----------|------|------|
| **新增** | `include/agenticdsl/contract/resume_token.h` | `ResumeToken{session_id, leaf_node_id, model, budget_used}` |
| **修改** | `include/agenticdsl/pdk/chat_session.h` | 构造签名加 `agenticdsl::SessionManager* session_manager = nullptr`(Change 2 Task 0) + `optional<ResumeToken> resume = nullopt` |
| **修改** | `pdk/chat_session/src/chat_session.cpp` | `chat()` 完成后自动调 `SessionManager::flush_append(SessionNode)` + 写 `trace_id/model` 到 `SessionNode::content`(SessionNode 字段为 `id`/`parent_id`/`branch_id`/`content`,见 session_manager.h:46-60) |
| **修改** | `docs/adr/adr-0068-event-emission-contract.md` Appendix A | **A4 修订**:追加 6 个新 chat.* topic 到 canonical registry(**Appendix A v2.1**),**不新建** `event_topic_registry.h` 平行 registry |
| **修改** | `chat_session.cpp` | 在 `chat()` 入口 emit `chat.turn.start`,出口 emit `chat.turn.end`(**EventBuilder 模式,见 §11 A4 + Step 5.4 grep 验收**) |
| **新增** | `tests/test_pdk_chat_session_recovery.cpp`(**2026-09-11 Momus 复核 Item 9 修订**: 加 `pdk_` 前缀) | L1 E2E 测试(3 个 case,见 §7.3) |

---

## 5. 数据流(用户输入 → 路由 → 多轮调用)

```
┌──────────┐    optional<string> read_line(500ms)
│ 用户键盘 ├──────────────────────────────────────────► IInputSource
└──────────┘                                              │
                                                          ▼
                                              ┌─────────────────────┐
                                              │  ChatSession::Impl   │
                                              │  input_thread_main   │
                                              │  (single-reader)     │
                                              └─────────┬───────────┘
                                                        │
                                          ┌─────────────┴─────────────┐
                                          │ 优先级分类                  │
                                          │  '/' 开头 → steering_queue │
                                          │  其他 → follow_up_queue    │
                                          └─────────────┬─────────────┘
                                                        │
                                                        ▼
                                              ┌─────────────────────┐
                                              │ pop_next_input       │
                                              │ (steering > followup)│
                                              └─────────┬───────────┘
                                                        │
                                ┌───────────────────────┴──────────────────────┐
                                │                                              │
                    ┌───────────▼──────────┐                    ┌──────────────▼────────┐
                    │ CommandRegistry       │                    │ ChatSession::chat()   │
                    │ resolve_command("/xxx")│                    │ (单 turn ReAct)        │
                    │ → /model /cancel /help│                    │ ┌──────────────────┐   │
                    └───────────┬──────────┘                    │ │ loop_agent       │   │
                                │                                │ │ (ReactLoop)      │   │
                                │                                │ │ ↓                │   │
                                │                                │ │ SimpleCognitive  │   │
                                │                                │ │ Orchestrator     │   │
                                │                                │ │ ↓                │   │
                                │                                │ │ ILLMProvider     │   │
                                │                                │ │ (mock 或 真实)   │   │
                                │                                │ └──────────────────┘   │
                                │                                └──────────────┬────────┘
                                │                                              │
                                └──────────────────┬───────────────────────────┘
                                                   ▼
                                         ┌──────────────────────┐
                                         │ IInteractionBus      │
                                         │ emit:                │
                                         │  - chat.turn.start   │
                                         │  - chat.turn.end     │
                                         │  - llm.response      │
                                         │  - tool.completed    │
                                         │ SessionManager:      │
                                         │  - append_to_branch  │
                                         │  - fsync per line    │
                                         └──────────────────────┘
```

**关键不变式**:
- input thread 是 `stdin` 唯一 reader → 杜绝双读 race(2026-09-07 已修)
- `steering_queue` 优先级恒高于 `follow_up_queue`(契约强制)
- `request_stop()` 不经队列,直达 `stop_source`(cancel 优先级 > steering)
- 每条消息立即 `append_to_branch` + `fsync`(等价 WAL)

### A5 边界声明:pdk/chat_session vs pdk/session_agent

| | `pdk/chat_session` (本设计新增) | `pdk/session_agent` (已存在) |
|---|---|---|
| **定位** | 交互循环原语(应用侧持有) | DSL 侧 session 工具面(引擎内调用) |
| **持有者** | main / server / test harness | DSLEngine 内 `loop_agent` plugin |
| **API 形态** | 同步 `chat(input, stop_token)` 返回 ChatResult | tool call `session/{fork,clone,compact}` |
| **状态归属** | 应用进程级,多 session 由调用方池管理 | DSLEngine 单例,per-engine state |
| **命名互不替代** | "User-facing chat loop" | "DSL-callable session operations" |

二者不重叠,合并债为零。后续若需"session_agent 调用 chat_session",通过 `IAgentComposition::call()` 显式桥接,**不**做隐式聚合。

### A5.5 异步测试准则:事件驱动同步(Oracle 二轮审查,2026-09-11)

**历史教训**: `test_chat_session_consumer` 的 100ms 超时此前已放宽至 500ms(并发 ctest contention buffer,AGENTS.md 模式 #7),证明"放宽超时"路线已被实践否定。

**新准则**:**用事件驱动同步替代 sleep/timeout**——以本设计 Change 2 新增的 6 个 chat.* topic 作为测试同步点。

| 旧用法(禁) | 新用法(推荐) |
|---|---|
| `std::this_thread::sleep_for(100ms)` 后断言 | `bus_->subscribe("chat.turn.end", callback)` + cv.wait |
| `REQUIRE(elapsed < 100ms)` 硬时间断言 | `REQUIRE(callback_count == 1)` 事件计数 |
| 强制 timeout 大于 LLM 响应时间 | `std::optional<GenerationResult> result` future + cv |

**验收标准**:测试用例不再有 `sleep_for` / 硬时间断言(除非明确测量 LLM 端到端延迟)。CI `script -qec "ctest"` TTY 环境 + 计时稳定性由事件驱动保证。

### A5.6 Topic 持久化分级(对齐 ADR-0080 D6 opt-in,Oracle 二轮审查)

**6 个新 chat.* topic 持久化策略**:

| topic | AppendOnlyEventLog 持久化 | 理由 |
|---|---|---|
| `chat.turn.start` | ✅ 是 | 每次 turn 边界必记录,供审计/重放 |
| `chat.turn.end` | ✅ 是 | 同上 |
| `chat.steering.enqueued` | ❌ 否(仅 IInteractionBus 内存) | **高频小事件**,全量落盘放大 fsync 次数 |
| `chat.followup.enqueued` | ❌ 否 | 同上 |
| `session.resumed` | ✅ 是 | 关键生命周期节点 |
| `session.disconnected` | ✅ 是 | 同上 |

**实现约束**:`ChatSession::Impl` 内部根据 topic 名走 fast-path(仅 `bus_->emit`)/slow-path(`EventBuilder.build()` + 走 AppendOnlyEventLog capture)。EventBuilder 模式强制(ADR-0068 §5.11),grep `bus_->emit(BusEvent{` 0 处。

**性能基线要求**:Change 2 ship 后,跑 `test_chat_session_consumer` 在 1000 turn 流式输入下,AppendOnlyEventLog 写盘次数 ≤ 600(每 turn 1-2 个持久化事件 × 平均 0.6 持久化率)。**禁止**全量 6000 次 fsync。

### A5.7 六层模型覆盖度分析(Oracle 二轮审查附录)

**Harness 六层模型在 HydraForge 的最佳实现**(ChatSession 视角):

```
第1层 动作空间    ░░░░░ ChatSession 不参与(正确,走 loop_agent)
第2层 上下文装配  ███░░ ResumeToken + build_context_entries(装配入口,不实现策略)
第3层 反馈通道    ████░ 6 个 chat.* topic(A5.6 分级,本 Change 2 主要贡献)
第4层 控制循环    █████ 双队列 + 单turn + stop_token(主场,D1-D5 已成熟)
第5层 推理参数    ██░░░ budget_alert 已有,**Prompt Cache 前缀稳定性是空白(A5.8)**
第6层 信任边界    ░░░░░ ChatSession 不参与(归 ADR-0031/0075/0081)
```

**关键边界**:
- ChatSession **不**实现上下文装配策略本身(那是 LayeredContext / ContextCompactor / ADR-0083 IEvaluator 的职责)
- ChatSession **不**实现工具权限/审批(归 ADR-0031 ToolCoordinator)
- ChatSession **不**实现 prompt cache 策略(**新增 A5.8 空白**)
- ChatSession 仅承担**第 4 层的标准原语 + 第 2/3 层的接入点**

### A5.8 Prompt Cache 前缀稳定性约束(Oracle 二轮审查揭示的关键空白)

**问题**:LayeredContext 的层级布局若不保证**系统提示 + 工具定义前缀字节级稳定**,Prompt Cache (ADR-0035 llama.cpp plugin prefix_cache) 全部 miss,成本优化反而变成成本放大。

**缺失责任**:ChatSession 的 chat() 输出 prompt 是 LayeredContext 序列化结果,若 LayeredContext 中层顺序/字段名变化,前缀缓存击穿。

**建议(本 spec 范围外,P2 立项)**:
- 为 LayeredContext 引入"**前缀冻结区**"约束(系统提示 + 工具定义 + agent_cfg 不可压缩)
- ContextCompactor(ADR-0007)压缩只动尾部,**禁止**触碰冻结区
- 验收: 1000 turn 同一 chat_session,llama.cpp plugin prefix_cache hit rate ≥ 70%

**本 spec 推迟理由**:Prompt Cache 是 ADR-0035 已有能力,本 spec 仅 ship chat_session PDK lift 不实现 cache 策略;但**显式记录此空白**,避免未来 chat_session PDK ship 后,cache 击穿归咎于 chat_session 设计。

---

## 6. API 设计

### 6.1 IInputSource / ILogger 接口契约

**A1 修订(基于 Oracle 2026-09-11 审计)**: `ILogger` 不新建 LogLevel/LogSourceLocation/自由函数 — `src/common/log/log.h` 已有 `agenticdsl::log::Level` 全局门面。`ILogger` 仅保留接口用于测试注入,生产实现桥接到既有门面。

```cpp
// include/agenticdsl/contract/iinput_source.h
namespace agenticdsl {

class IInputSource {
 public:
  virtual ~IInputSource() = default;

  // 阻塞读一行,timeout 后返回 nullopt;EOF 返回 nullopt
  virtual std::optional<std::string> read_line(
      std::chrono::milliseconds timeout) = 0;

  // 非阻塞检查是否有可用输入
  virtual bool has_input() const = 0;

  // 关闭输入源(用于优雅退出 / signal handler)
  virtual void close() = 0;
};

}  // namespace agenticdsl

// include/agenticdsl/contract/ilogger.h (仅用于测试注入)
namespace agenticdsl {

enum class LogLevel { kDebug, kInfo, kWarn, kError };

class ILogger {
 public:
  virtual ~ILogger() = default;
  virtual void log(LogLevel level, std::string_view message) = 0;
};

}  // namespace agenticdsl
```

**`StderrLogger` 实现桥接既有门面**(`src/common/log/log.h`):
```cpp
class StderrLogger : public ILogger {
 public:
  void log(LogLevel level, std::string_view message) override {
    // **A1 修订**: `agenticdsl::log` 命名空间只暴露 `emit(Level, std::string)`
    // 单一函数 + `LOG_INFO/WARN/ERROR/DEBUG` 四个**宏**(见 `src/common/log/log.h:52,88-103`)。
    // `log::info/warn/error/debug` 自由函数不存在——必须用 `emit`。
    auto emit = [&](agenticdsl::log::Level lvl) {
      agenticdsl::log::emit(lvl, std::string("[chat] ") + std::string(message));
    };
    switch (level) {
      case LogLevel::kDebug: emit(agenticdsl::log::Level::kDebug); break;
      case LogLevel::kInfo:  emit(agenticdsl::log::Level::kInfo);  break;
      case LogLevel::kWarn:  emit(agenticdsl::log::Level::kWarn);  break;
      case LogLevel::kError: emit(agenticdsl::log::Level::kError); break;
    }
  }
};
```

**A3 修订**: StdinInputSource 实现必须保留 self-pipe + `poll([STDIN_FILENO, pipe_read_fd_], timeout)` 语义,不得回退为裸 `std::getline`。参考现有 `chat_session.cpp:719+` 的 Sprint 31 死锁修复架构(AGENTS.md 模式 #6 闭环)。

### 6.2 ChatSession 构造签名(lift 后)

```cpp
namespace hydraforge::pdk {

class ChatSession {
 public:
  // **2026-09-11 Momus 复核 Item 5 修订**: AgentConfig / SessionConfig / ChatResult / QueueKind / InputMessage
  // 5 个公开类型在 PDK 命名空间 `hydraforge::pdk` (D6.1 落实), 不在 `agenticdsl`。
  // Change 1 + Change 2 完整签名(Change 2 Task 0 加 SessionManager* 观察者指针):
  ChatSession(agenticdsl::DSLEngine* engine,
              std::shared_ptr<agenticdsl::IInteractionBus> bus,
              agenticdsl::IToolRegistry* registry,
              agenticdsl::SessionManager* session_manager,                  // **Change 2 Task 0**, 默认 nullptr = 不持久化
              AgentConfig agent_cfg,
              SessionConfig session_cfg,
              std::shared_ptr<CancellationRegistry> cancellation_registry = nullptr,
              std::unique_ptr<agenticdsl::IInputSource> input = nullptr,  // 默认:nullptr (fail-safe,不启动 input thread)
              std::unique_ptr<agenticdsl::ILogger> logger = nullptr,      // 默认:nullptr (fail-safe,不输出)
              std::optional<ResumeToken> resume = std::nullopt);

  ~ChatSession();

  // P0 API(lift):
  ChatResult chat(std::string_view input,
                  std::stop_token token = {});
  void request_stop();
  const std::string& session_id() const;
  std::vector<nlohmann::json> history() const;
  bool is_input_thread_shutdown() const;

  // P1 API(lift,后续 Sprint 完善测试):
  bool request_model_switch(const std::string& provider_name);
  std::string next_model() const;
  size_t queue_size(QueueKind kind) const;
  size_t try_clear_queue(QueueKind kind);
  bool consume_budget_alert();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

extern ChatSession* g_chat_session;  // 替换 pdk_chat_demo::g_command_session

}  // namespace hydraforge::pdk
```

### 6.3 ResumeToken 结构

```cpp
// include/agenticdsl/contract/resume_token.h
namespace agenticdsl {

struct ResumeToken {
  std::string session_id;
  std::string leaf_node_id;  // SessionManager::build_context_entries(leaf) 的入参
  std::string model;          // 用于 provider 恢复后验证一致性
  double budget_used = 0.0;   // 用于 budget 累计值恢复
};

}  // namespace agenticdsl
```

### 6.4 IInteractionBus 新增 topic(Change 2)

```cpp
// A4 修订:6 个新 topic 追加到 ADR-0068 Appendix A(已有 canonical topic registry),
// 不新建 event_topic_registry.h 平行 registry
constexpr const char* kChatTurnStart       = "chat.turn.start";
constexpr const char* kChatTurnEnd         = "chat.turn.end";
constexpr const char* kChatSteeringEnqueued = "chat.steering.enqueued";
constexpr const char* kChatFollowupEnqueued = "chat.followup.enqueued";
constexpr const char* kSessionResumed       = "session.resumed";
constexpr const char* kSessionDisconnected = "session.disconnected";
```

---

## 7. 测试策略(mock-first,scripted mock)

### 7.1 测试分层(Pattern 1-7 全部应用)

```
┌────────────────────────────────────────────────────────────────┐
│ L1 契约层(mock,必须 100% PASS)                                  │
│  - RecordingLLMProvider 记录 req.params.model / generate_calls  │
│  - ScriptedMockLLMProvider 按 enqueue 顺序返回                  │
│  - InMemoryInputSource 预填测试输入                             │
│  - CapturingLogger 捕获 info/warn/error 调用                   │
└────────────────────────────────────────────────────────────────┘
┌────────────────────────────────────────────────────────────────┐
│ L2 集成层(mock + 真实 LLM opt-in,后续独立 change)              │
│  - 测试_e2e_real_llm_chat_session                              │
│  - [realllm] tag + pdk_chat_demo::testing::require_real_llm_env│
└────────────────────────────────────────────────────────────────┘
```

### 7.2 L1 测试矩阵(Change 1,估时 0.5d)

| 测试名 | 标签 | 验证内容 |
|--------|------|---------|
| `ChatSession ctor default input=nullptr safe no-op` | `[pdk][chat_session][ctor]` | 验证默认不启动输入线程(Pattern 5 fail-safe) |
| `ChatSession ctor explicit InMemoryInputSource consumes queue` | `[pdk][chat_session][ctor]` | 验证 IInputSource 注入 + steering > follow-up 优先级 |
| `ChatSession steering /cancel triggers request_stop` | `[pdk][chat_session][steering]` | 验证优先级规则写死 |
| `ChatSession overflow rejects at capacity 32` | `[pdk][chat_session][queue]` | 验证有界队列 + overflow-reject(已有 ship 行为) |
| `ChatSession ChatResult captures LLM turn response` | `[pdk][chat_session][chat]` | ScriptedMockLLMProvider enqueue `{tool:echo, args:{message:hi}}` → 验证返回 `data.echoed == "hi"` |
| `ChatSession 5 sequential turns consistent` | `[pdk][chat_session][concurrency]` | 5 轮 scripted responses → 验证 history() 累积正确 |
| `ChatSession request_stop mid-turn with MockBlockingProvider` | `[pdk][chat_session][cancel]` | 复用已有 MockBlockingProvider(100ms 内取消契约) |
| `RecordingLLMProvider last_model empty for chat_session turn` | `[pdk][chat_session][realllm-guard]` | 验证 `req.params.model.clear()` 被调(Pattern 2 + 5) |
| `CapturingLogger emits info on chat.turn.start and warn on overflow` | `[pdk][chat_session][logger]` | 验证 ILogger 注入 + chat.* event 发射 |
| `ChatSession with nullptr CancellationRegistry graceful fallback` | `[pdk][chat_session][fallback]` | 验证可空构造 + request_stop 抛 logic_error 而不是 abort |

### 7.3 L1 E2E 测试矩阵(Change 2 断线恢复,估时 0.3d)

| 测试名 | 标签 | 验证内容 |
|--------|------|---------|
| `Resume replay 3 turns then kill -9 same session_id history intact` | `[pdk][chat_session][recovery]` | E2E-1:SessionManager JSONL 持久化 → 重启同 id → history 完整 |
| `Truncated last JSONL line discarded silently on resume` | `[pdk][chat_session][recovery]` | E2E-2:模拟崩溃 → open 成功,丢失不完整行 |
| `Fork branch A 2 turns kill resume then switch branch B` | `[pdk][chat_session][recovery]` | E2E-3:分支隔离 + 上下文互不污染 |

### 7.4 真实 LLM 测试(后续独立 change,本设计范围外)

- `test_e2e_real_llm_chat_session` — `[realllm][chat]`,走 `pdk_chat_demo::testing::real_llm_provider()`,验证 DeepSeek 真实响应路径
- 3-5 个 case,模式同 `tests/test_cognitive_worker.cpp:466`

---

## 8. 实施风险与回滚

| 风险 | 缓解 |
|------|------|
| PR 1 超 500 行 | Oracle 已建议拆分;若仍超,进一步拆 IInputSource 与 ILogger 为独立 change |
| `try_push_*_for_test` 删除破坏既有测试(实测 2 个文件: `test_chat_session_consumer.cpp` + `test_chat_session_queues.cpp`,非 4 个) | **2026-09-11 Momus 修订**: 保留 `try_push_steering_for_test` + `try_push_follow_up_for_test` 作为 lift 后的 test-only helper (标 `[[deprecated]]`), 1 Sprint 内迁 InMemoryInputSource; overflow 测试需直接 push shim 才能确定性验证 capacity=32 拒绝行为 |
| `std::cin` 硬编码 → TTY 死锁风险 | IInputSource 默认 nullptr = 不启动输入线程(Pattern 5 fail-safe);main.cpp 显式 opt-in |
| CancellationRegistry 提升 PDK 破坏已有全局变量 | 全局名 `hydraforge::pdk::g_cancellation_registry`,保留 `examples/pdk_chat_demo::g_cancellation_registry` 为 type alias 兼容期 1 个 Sprint |
| ChatSession 构造签名 BREAKING | Change 1 加 `[[deprecated]]` 重载 + 编译期 warning;Change 2 移除旧签名 |
| SessionManager 写盘性能 | 每行 fsync 已是 ship 行为,无回归;trace_id/model 字段小,JSON 序列化开销 < 5% |

---

## 9. 推迟到后续 Sprint(明确)

| 项目 | 推迟原因 | 后续 Sprint |
|------|----------|-------------|
| 多租户 session 池 | 当前应用层无需求;等 service-ification / ADR-0050 Candidate B | Phase 6 Sprint 30+ |
| AgentMailboxInputSource | 需 IAgentComposition 集成测试先成熟 | Phase 6 Sprint 30+ |
| TokenBucket 限流 | 32 容量有界队列 + overflow-reject 已够;需真实过载数据再做 | 观察期 1-2 Sprint |
| 流式 token 渲染时序测试 | 属 TUI 层,非 PDK 校验范围 | TUI 测试 change |
| 真实 LLM `[realllm]` 测试 | 本设计 mock-first 100% ship 后独立补 | Change 3 |
| 7 个 slash 命令迁移 PDK | 用户选定"保持现状",handler 留 examples | 不在 scope |

---

## 9.5 后续任务优先级与依赖(Oracle 审计 2026-09-11,含二轮审查修订)

### 任务排序(4-6 周,二轮审查修订后;2026-09-11 Momus review 后 P0-3 提前到 P0-0)

| 优先级 | Change | 理由 |
|---|---|---|
| **P0-0** | chat-session-pdk-lift **ADR-0088 立卷** | **2026-09-11 Momus 修订**: D1-D6 决策先于代码, 符合 "OpenSpec Change as Decision Log" 治理惯例(避免 implementation 跑在未批准的 design 上)。先 `python3 tools/adr_lint.py` 确认 ADR 编号 (实测最大 0087), 分配 ADR-0088; 同步 `docs/active-status.md` 视图层 |
| **P0-1** | chat-session-pdk-lift **Change 1** (I/O 抽象 + lift) | 一切的地基;与 `chat-real-llm-coverage` 有 tests 目录协调点 |
| **P0-2** | chat-session-pdk-lift **Change 2** (ResumeToken + SessionManager) | 紧接 Change 1;**新增 TSan acceptance**(§11 Change 2 #5);ResumeToken 4-Scope 对齐 ADR-0079(D7) |
| **P0-3** | **TSan 锁顺序契约** | D8 锁顺序规范落 ShipGate:`-DAGENTICDSL_BUILD_TSAN=ON` 下跑 test_chat_session_recovery,零 race |
| **P1** | chat-session-pdk-lift **Change 3** (真实 LLM) | 排在 `cloud-adapter-threading-root-cause` (adr-0087) ship 之后;复用 `skill-interpreter-ipc-realllm` 模式 |
| **P2-1** | AgentMailboxInputSource | IAgentComposition 仅 V1 骨架,集成测试不成熟 |
| **P2-2** | 多租户 session 池 | 推迟至 ADR-0050 Candidate B service-ification 真正启动 |
| **P2-3** | **Prompt Cache 前缀稳定性约束**(A5.8) | LayeredContext "冻结区"约束 + ContextCompactor 不动 cache 前缀;ADR-0035 prefix_cache hit rate ≥ 70% 验收;六层模型揭示的当前计划最大空白 |
| **P2-4** | 异步测试准则落地(A5.5) | 事件驱动同步(`chat.turn.*` / CausalClock)替代 sleep/timeout,跨项目推广 |
| **P2-5** | pdk_chat_demo 重构为标准示例 | 展示 3 种 IInputSource 形态(Stdin / InMemory / nullptr fail-safe),作为 PDK 消费者文档 |
| **P2-6** | 分支可视化工具 | 输出对齐 ADR-0061-06 v1.1 Trajectory IR;服务蒸馏/评估管线 |
| — | `adr-0087` root-cause 修复 | 非本 spec 范围,但 Change 3 前置,sprint plan 需给它留位 |

### 等待依赖

1. **chat-real-llm-coverage (in-flight) → Change 1**: 该 change 声明不改 production,但 Change 1 要迁 `try_push_*_for_test` 的 4 个测试。**策略:proceed-with-risk** — Change 1 保留旧 API 为 `[[deprecated]]` shim 1 个 Sprint,不动对方新增文件
2. **cloud-adapter-threading-root-cause / adr-0087 (in-flight) → Change 3**: SerializingDecorator 移除后并发行为变化,real LLM 断言需放宽。**策略:block** — Change 3 在其 ship 前不启动
3. Change 2 无硬依赖

### 风险预警

1. **SessionWriter 锁反向持有**(D8 警示):ChatSession 写入路径与 SessionWriter `file_mutex_` 反向持有时,`flush_loop` 后台写线程会死锁。已通过 D8 锁顺序契约 + TSan acceptance 锚定
2. **Prompt Cache 前缀击穿**(A5.8 空白):本 spec 不实现 cache 策略,但显式记录避免未来归咎;P2-3 立项
3. **ADR 编号冲突**:D6.2 已通过 `adr_lint.py` 实测确认,分配 ADR-0088
4. **Topic 持久化放大 fsync**(A5.6):已通过 4 选 2 持久化分级 + 性能基线规避
5. **双日志门面漂移**:ILogger 与 `log.h` 并存,已通过 §6.1 A1 桥接规避
6. **self-pipe 回退**:lift 重写输入线程时丢弃 poll 架构,Sprint 31 死锁修复静默回退(已通过 §11 A3 acceptance 锚定)
7. **测试目录三方并发**:本 Change 1 + chat-real-llm-coverage + 既有 28 binary 同改 examples tests,rebase 冲突 + helper 归属争议(已通过 proceed-with-risk 协调)

### YAGNI(绝对不要做)

1. 不要新建平行 topic registry(ADR-0068 Appendix A 追加即可,A4 已应用)
2. 不要在 Change 1/2 顺手迁移 7 个 slash commands(§9 已确认,勿范围蔓延)
3. 不要为 ILogger 加 `LogSourceLocation`/结构化字段(A1 已应用,既有门面无此能力)
4. 不要为契约新引入 `::contract` 子命名空间(D6.1 已写死,保持 ADR-0021 一致性)
5. 不要在 Change 1 范围外扩展 ChatSession 公开 API(避免 §2 D9 YAGNI 边界破坏)
6. 不要在 spec 范围外实现 Prompt Cache 策略(A5.8 已显式 P2 立项推迟)

---

## 10. 文件清单与依赖

> **A2/A4 修订后(2026-09-11 Momus review)**: 删除 §10 中两条与 §4/§11 矛盾的行——
> (1) `src/common/contract/event_topic_registry.h` 与 A4 "不新建平行 registry" 冲突,删除;
> (2) `src/common/io/{in_memory_input_source,capturing_logger}.{h,cpp}` 与 A2 "测试 double 在 tests/test_helpers/" 冲突,改为 `tests/test_helpers/`。

### 新增文件(11 个)

```
# Contract 层(2)
include/agenticdsl/contract/iinput_source.h
include/agenticdsl/contract/ilogger.h
include/agenticdsl/contract/resume_token.h                        (Change 2)

# PDK 头(2,从 examples 迁)
include/agenticdsl/pdk/cancellation_registry.h
include/agenticdsl/pdk/chat_session.h

# Production 默认实现(2)
src/common/io/stdin_input_source.h/.cpp
src/common/io/stderr_logger.h/.cpp

# 测试 double(2,**A2 修订后放 tests/test_helpers/**)
tests/test_helpers/in_memory_input_source.h
tests/test_helpers/capturing_logger.h

# PDK 编译目标(2)
pdk/chat_session/CMakeLists.txt
pdk/chat_session/src/chat_session.cpp                             (从 examples 迁,包含 cancellation_registry.cpp 内联实现,**保证有 CMake target 编译**,非 orphan)

# 测试(2,**避免与 examples 测试同名 target 冲突**)
tests/test_pdk_chat_session.cpp                                   (10 cases, 替代原 test_chat_session.cpp 命名)
tests/test_pdk_chat_session_recovery.cpp                          (3 cases, Change 2)
```

### 修改文件(7 个)

```
include/agenticdsl/pdk/pdk.h                                      # 加 chat_session.h include
pdk/CMakeLists.txt                                                # add_subdirectory(chat_session)
examples/pdk_chat_demo/main.cpp                                   # 显式传 StdinInputSource + StderrLogger
examples/pdk_chat_demo/commands/command_globals.{h,cpp}           # type alias 兼容(1 Sprint shim)
# 兼容期 shim(新增):examples/pdk_chat_demo/chat_session.h → 3 行转发到 hydraforge::pdk
examples/pdk_chat_demo/chat_session.h                             # **保留为 3 行 shim**,内部 include 新 PDK 头 + namespace alias,避免 17 个旧 includer 一次断链
```

### 删除文件(1 个)

```
examples/pdk_chat_demo/cancellation_registry.h                   # 已迁到 PDK(被 shim include 间接提供)
examples/pdk_chat_demo/cancellation_registry.cpp                 # 已迁到 PDK(内联到 pdk/chat_session/src/chat_session.cpp)
examples/pdk_chat_demo/chat_session.cpp                           # 主体迁到 PDK;旧 chat_session.h 保留为 shim 1 Sprint
```

---

## 11. Acceptance Criteria(可验证)

### Change 1

1. ✅ `include/agenticdsl/pdk/chat_session.h` namespace `hydraforge::pdk` 编译通过
2. ✅ `tests/test_pdk_chat_session.cpp` 10 个 case 全部 PASS,核心 0 回归(测试命名带 `pdk_` 前缀避免与 `examples/pdk_chat_demo/tests/test_chat_session.cpp` 冲突;`tests/CMakeLists.txt` 自动 file(GLOB) 已注册到 `test_chat_session` target → 新文件**必须**改名)
3. ✅ `examples/pdk_chat_demo` 既有 28 个测试二进制全部 PASS,零回归
4. ✅ `pdk/CMakeLists.txt` 加入 `chat_session` 子目录,`hydraforge_pdk` INTERFACE lib 链接成功
5. ✅ `script -qec "ctest" /dev/null` TTY 环境下所有测试 PASS(Pattern 5 fail-safe 验证)
6. ✅ `clangd --check` 关键文件 0 errors(LSP discipline per `scripts/check-lsp-discipline.sh`)
7. ✅ **A3 acceptance**(修订后): `StdinInputSource::read_line()` 实现保留 self-pipe + `poll([STDIN_FILENO, pipe_read_fd_], timeout)` 语义。`pipe2(O_CLOEXEC | O_NONBLOCK)` + poll + callback 写 wake-up byte 的核心代码取自 `examples/pdk_chat_demo/chat_session.cpp` 的 `input_thread_main` 块(**实际行号 817-853**,非 §6.1 误标的 719+)。同时**删除** `ChatSession::Impl` 中的 `pipe2/pipe_read_fd_/pipe_write_fd_` 成员 + timer callback 写字节逻辑(Sprint 31 死锁修复的 wake-up byte 路径改为 `timer_->` 回调调 `input_->close()`,由 StdinInputSource 自有 pipe 接收)——避免 lift 后双 self-pipe 所有权混乱导致 timer 唤醒字节进死 fd。`script -qec "ctest --test-dir build" /dev/null` 232/232 PASS,无 SIGTERM-then-`std::terminate` 死锁回归
8. ✅ **A2 acceptance**: 测试 double `InMemoryInputSource` / `CapturingLogger` 路径在 `tests/test_helpers/`,不在 `src/common/io/`(Pattern #5)
9. ✅ **A1 acceptance**(修订后): `StderrLogger::log()` 内部**仅**委托 `agenticdsl::log::emit(Level, std::string)`(`src/common/log/log.h:52`),不调用任何 `log::info/warn/error/debug` 自由函数(后者在 `agenticdsl::log` 中**不存在**——只有 `LOG_*` 宏,见 log.h:88-103)。验收 grep: `grep -rn "log::info\|log::warn\|log::error\|log::debug" src/common/io/stderr_logger.cpp` 0 行(`log.h` 自身定义 4 个宏,全库 grep 必 ≥4,不能用作验收)

### Change 2

1. ✅ 6 个新 chat.* topic **追加到 ADR-0068 Appendix A** (canonical registry), **不** 新建 `event_topic_registry.h` 平行 registry。所有 bus 发射走 EventBuilder 模式 (`bus_->emit(EventBuilder(...).build())`), 禁止 raw `BusEvent` 字面量。验收 grep (修订后, named-variable 写法会绕过 `BusEvent{` 关键字): `grep -rn "bus_->emit" pdk/chat_session/src/ | grep -v "EventBuilder"` 0 行 (任何 bus 发射都必须经 EventBuilder 构造, named-variable BusEvent 也算违规)
2. ✅ `tests/test_pdk_chat_session_recovery.cpp` 3 个 E2E **真实实现全部 PASS**(修订后: 禁止 `REQUIRE(true)` 占位; E2E 必须用真实 `SessionManager` (`src/core/session_manager.{h,cpp}`, 已 ship API: `open/load_jsonl/build_context_entries/flush_append`), 共享 persist_dir 跨两次 ChatSession 构造模拟 kill+restart; 截断行/分支隔离/1000-turn baseline 全部用真数据驱动)
3. ✅ 现有 chat_session 28 测试二进制 PASS,零回归
4. ✅ ResumeToken 在 SessionManager JSONL 中可解析,断线后 leaf_node_id 正确恢复
5. ✅ **D8 acceptance**: CMake TSan preset (`-DAGENTICDSL_BUILD_TSAN=ON`) 下 `test_chat_session_recovery` PASS,零 data race 警告 — 锁顺序契约(steering/follow_up_queue.mtx → messages.mtx → session_writer.file_mutex_,禁止反向持有)验证
6. ✅ **D7 acceptance**: ResumeToken `{session_id, leaf_node_id, model, budget_used}` 与 ADR-0079 4-Scope 映射表(Conversation / Attempt / Step / Execution)在 §D7 已显式记录
7. ✅ **A5.6 acceptance**: 1000 turn `test_chat_session_consumer` 流式输入下,AppendOnlyEventLog 写盘次数 ≤ 600(每 turn 1-2 个持久化事件 × 平均 0.6 持久化率,4/6 topic 走 fast-path 仅 IInteractionBus 内存)

---

## 12. 开放问题(留待用户 review)

1. **是否同意 Change 1 同时迁移 CancellationRegistry 到 PDK?** (建议:是)
2. **是否同意 IInputSource 默认 nullptr = fail-safe,不启动输入线程?** (建议:是,Pattern 5)
3. **是否同意 7 个 slash 命令 handler 保留在 examples 不进 PDK?** (已确认)
4. **是否同意真实 LLM 测试作为 Change 3 独立后续?** (已确认)
5. **是否同意 ChatSession::chat() 默认 1-turn,多轮由调用方循环驱动?** (建议:是,YAGNI)

---

**待用户确认后**:进入 writing-plans skill,生成 2 个独立 OpenSpec change 的实施计划。
