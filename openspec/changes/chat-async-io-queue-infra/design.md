## Context

`pdk_chat_demo` 当前使用 sync `while (std::getline(...))` 循环 (`main.cpp:466`)，Agent turn 执行期间用户无法输入。Wave 3-A 目标是为 pdk_chat_demo 添加 pi-agent 风格的 steering/follow-up 借鉴路径。

本 change (Phase A) 仅添加**队列数据结构 + 输入线程分离**，不引入 stop_token 链路（Phase B 引入）。Phase A 是 Phase B 的前置依赖。

### 现有架构约束

- `ChatSession::Impl` (PIMPL): main.cpp 持有 `ChatSession session(...)` (`main.cpp:395`)
- `ChatSession::chat(user_input)` 同步调用，返回 `ChatResult`
- `std::cin` 由 main 交互循环阻塞读取
- Agent turn 通常调用 `loop_agent.run` plugin 工具
- 现有原子模式: `session.consume_budget_alert()` (`main.cpp:507-510`) — 跨线程通知

## Goals / Non-Goals

**Goals:**
- 在 `ChatSession::Impl` 内引入 `steering_queue_` + `follow_up_queue_` 双有界队列
- 分离 input 线程从 main 交互循环
- 公开 queue 状态查询 API（线程安全 O(1)）
- sync 路径 E2E 测试验证 queue 行为
- 阶段 B 集成点留接口（不实现 turn 中断点注入）

**Non-Goals:**
- 不实现 turn 中断点注入（→ Phase B）
- 不引入 async I/O 框架（如 Boost.Asio）
- 不重写 `ChatSession::chat()` 接口
- 不动 main.cpp `while(getline)` 循环结构（Phase B 改）
- 不实现 `/model` 切换（→ Phase C）
- 不实现 input 优先级（steering 与 follow-up 平等对待）

## Decisions

### Decision 1: 数据结构选择 — `std::queue<std::string>` + mutex

**选择**: `std::queue<std::string>` + `std::mutex` + `std::condition_variable` (per queue)

**理由**:
- **标准库**: 无新依赖，与项目 C++20 约束一致
- **FIFO 语义**: 与队列语义天然匹配
- **有界通过 size check**: 不引入 `std::queue` 之外的容器
- **可移植**: Linux/macOS/Windows 通用

**替代方案考虑**:
- **Boost.Lockfree.Queue**: 引入新依赖，单测同步场景下性能优势不明显
- **moodycamel::ConcurrentQueue**: 第三方库，无 lock-free 优势在 sync 场景下
- **每个 entry 独立 atomic**: 复杂度高，O(1) 查询需要额外索引

### Decision 2: 双 mutex vs 单 mutex

**选择**: 每个队列独立 mutex

**理由**:
- **避免死锁**: 单 mutex 下 enqueue + dequeue 不会死锁但路径长
- **降低 contention**: 2 个 mutex 让 producer/consumer 路径独立
- **deadlock-free**: producer 永远只锁一个 mutex（steering 或 follow-up），consumer 同理

**替代方案考虑**:
- **单 mutex 保护双队列**: 简单但 contention 高
- **lock-free 队列**: 复杂度高，sync 场景下无收益

### Decision 3: 队列容量默认 32

**选择**: `kDefaultQueueCapacity = 32` 通过 `constexpr` 定义

**理由**:
- **典型交互**: 1 input/秒，30 秒内输入 30 条 → 32 略有余量
- **防内存膨胀**: 上限保护避免恶意/异常输入导致 OOM
- **可配置**: 构造函数接受 override，测试用 4 即足够

**替代方案考虑**:
- **无界**: 内存膨胀风险
- **16 → 32 → 64**: 32 是平衡点（整数约 30s 输入窗口 + 2 entries 余量）

### Decision 4: Overflow 策略 — 拒绝新 + log warning

**选择**: 队列满时拒绝新输入 + stderr warning (包括输入长度，不包括内容)

**理由**:
- **不丢老**: 拒绝新策略不会意外丢失已排队消息
- **可观察**: log warning 让用户/操作员知道发生 overflow
- **无内容泄露**: 仅记录长度（避免 secret/key 泄露到 stderr）

**替代方案考虑**:
- **drop oldest**: 丢失 steering 历史 (用户可能依赖序列)
- **drop newest**: 与当前选择相同但名称混淆
- **阻塞 producer**: 会导致 stdin 读取阻塞，破坏 sync 路径

### Decision 5: Input 线程 vs 同步 stdin 读取

**选择**: 新增 `std::thread input_thread_` 在 `ChatSession::Impl` 构造时启动

**理由**:
- **解耦 producer**: input 线程独立运行，不依赖 main 循环节奏
- **RAII**: 析构 join，无 use-after-free
- **Phase B 兼容**: stop_token 可在 input 线程内观察并退出

**替代方案考虑**:
- **保持 main.cpp 同步 getline**: 与 Phase B 集成困难（sync 阻塞调用无法中断）
- **使用现有 atomic flag 模式（Phase 0）**: 类似，但 input 线程是更完整的解耦

### Decision 6: 测试策略 — sync 路径模拟异步

**选择**: 在测试中直接调用 `try_push_queue(QueueKind, item)` 模拟 input 线程 + 验证 `queue_size()` + `try_clear_queue()`

**理由**:
- **单测不依赖真 stdin**: 避免在进程内管理 stdin 注入
- **测试覆盖 boundary**: 容量测试可以构造 33+ entries
- **TDD 友好**: 测试可以直接验证 queue 数据结构

**替代方案考虑**:
- **E2E 真 stdin 注入**: fork + exec + pipe（signal_shutdown 测试模式），复杂
- **集成测试**: 启动 ChatSession + 模拟输入 + 验证 chat() 消费

## Risks / Trade-offs

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Input 线程与 main 线程同时操作 stdin 导致竞争 | 🟡 Medium | 🟡 Medium | input 线程独占 stdin，main 线程不再 getline（Phase B 改） |
| Queue mutex 死锁 | 🟢 Low | 🔴 High | 设计上 producer/consumer 各自只锁一个 mutex；测试覆盖 |
| 析构 join 阻塞导致 ChatSession 销毁慢 | 🟡 Medium | 🟢 Low | input 线程读取快速循环，join 时间 ms 级 |
| Phase B 集成需要重新设计接口 | 🟡 Medium | 🟡 Medium | 现有 API (`queue_size`, `try_clear_queue`) 已足够 Phase B 消费 |
| 测试覆盖不足 | 🟢 Low | 🟡 Medium | 4 个测试场景覆盖入队/出队/overflow/clear |

## Migration Plan

### 部署

无 schema 变更 / 无 ABI 变更 / 无依赖变更 → 零迁移成本

### 回滚

删除 `ChatSession::Impl` 新增字段即可（无 API 破坏，析构跳过 join 即可）

### 兼容性

- 现有 `ChatSession::chat(user_input)` 调用方不受影响（Phase A 不改 chat()）
- Phase B 启动后 main.cpp 交互循环改为观察 queue（而非直接 getline），但 Phase A 期间 main.cpp 保持原状

## Open Questions

1. **Input 线程启动对 ChatSession 构造的影响**: 当前 `ChatSession::chat_session.cpp:175-184` 构造函数不涉及 I/O，input 线程会改变此行为。是否需要在 main.cpp 显式 `session.start()` 异步？**决议**: 不要 — 构造函数即启动，析构 join，保持 RAII 简单性（Phase B 集成时如需要可调整）
2. **QueueKind 枚举位置**: 公开 API 枚举，在 `chat_session.h` 还是单独 namespace？**决议**: `chat_session.h` 内联定义（与 ChatSession 同一头文件，零耦合）

3. **Input 线程与 mock 模式**: mock 模式下 stdin 仍可用，但测试可绕过 input 线程直接 `try_push_queue` 注入。**决议**: 保留 input 线程（标准运行路径），测试用直接 API 注入（可控场景）