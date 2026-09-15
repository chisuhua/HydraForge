# Spec — fix-tsan-residual-2026-09-15

## ADDED Requirements

### R1: EventLogWriter flush_loop 与 flush_sync 串行化 file_ 写入

EventLogWriter 的 `flush_loop` (后台 std::thread, 默认 10ms 周期) 与 `flush_sync` (同步 API) 都对 `std::ofstream file_` 写入。`std::ofstream` 非线程安全 — 并发 `<<` 操作通过 `basic_streambuf::xsputn` (libstdc++ writev) 调用可能 interleaving 损坏文件内容。`buffer_mutex_` 只保护队列访问, 不保护 IO 路径。

#### 实现约束

- 新增 `std::mutex file_mutex_` 成员, 与 `buffer_mutex_` 独立
- `flush_sync`: 获取 `file_lock` **必须在** `snapshot.empty()` 检查**之前** (否则 flush_loop 已抢走本次批次 buffer 时, flush_sync 立即返回但 flush_loop 写入未完成 → race)
- `flush_loop`: 获取 `file_lock` 在 `snapshot.empty()` continue 检查**之后** (无空批次无需锁)
- `rotate_if_needed()` 在 file_lock 作用域内调用 (已经是的)
- 锁获取顺序: `buffer_mutex_` 先 (取 snapshot) → `file_mutex_` 后 (写文件). 不允许反向.

#### SCENARIO R1.1: 并发 flush_loop + flush_sync 无 records 丢失
- **GIVEN** 50 iter × 7 thread 并发调 `on_bus_event` (灌入 buffer) + `flush_sync` (在后台 flush_loop 运行期间)
- **WHEN** 全部 iter 完成后调 `read()`
- **THEN** records 数 ≥ 期望总数 (vs baseline records.size() < 期望 14% race)
- **由** `tests/test_event_log_concurrent.cpp` (新建) 验证

#### SCENARIO R1.2: rotate 在并发下安全
- **GIVEN** 已 set `max_file_size = 1KB` + 灌入超过 1KB 的 events
- **WHEN** 多个 thread 并发 flush
- **THEN** 不 crash, 不损坏 rotation files (rename sequence 完整)

### R2: ChatSession::Impl::~Impl() 与 timer callback 生命周期同步

ChatSession 是 ITimerService 的**外部注入持有方** (per `include/agenticdsl/contract/timer_service.h` line 86: "外部注入 timer 必须自行保证生命周期"). timer callback `[this]` 在 `timer_->cancel()` 返回后仍可能 in-flight 触发, 访问 `this->shutdown_check_pending_` / `this->input_` 时 `~Impl()` 可能已开始销毁 → UB. 同时 `periodic_id_` 在 `~Impl()` 与 `input_thread_main` TimerGuard 析构并发 write-write.

#### 实现约束

- `periodic_id_` 改 `std::atomic<std::uint64_t>` (TimerId = uint64_t, trivially copyable)
- `Impl` 新增成员:
  - `std::atomic<int> in_flight_callbacks_{0}` — timer callback in-flight 计数器
  - `std::mutex in_flight_mutex_` + `std::condition_variable in_flight_cv_` — barrier 同步
- timer callback 用 **RAII guard** 包裹 (避免异常路径漏 decrement): 入口 `fetch_add(1)`, 出口 `fetch_sub(1) + notify_all`
- `~Impl()` 5 步析构顺序 (per AGENTS.md 模式 #5 + Sprint 30 D8):
  - ① cancel timer (atomic exchange 0)
  - **①.5 wait in-flight callback barrier** (关键 — 之前缺失, 导致 callback 在 cancel 后仍跑)
  - ② `timer_ = nullptr`
  - ③ `stop_input_thread_.store(true) + input_->close()`
  - ④ `input_cv_.notify_all() + input_thread_.join()`
  - ⑤ `input_/logger_` auto-destroy (unique_ptr)
- 锁获取顺序: buffer_mutex_ 先 (input queue), file_mutex_ 后 (实际是 timer 等待 in-flight, 与 input 解耦). `in_flight_mutex_` 仅在 barrier wait 时短持.

#### SCENARIO R2.1: ~Impl() 在 callback in-flight 时不 UB
- **GIVEN** 构造 ChatSession (启 input_thread + timer 周期 50ms)
- **WHEN** 在 callback 预期触发时调 `~ChatSession()`
- **THEN** TSan 0 warning, 无 SIGSEGV, 无 race
- **由** `tests/test_chat_session_dtor.cpp` (新建) 验证 (catch2 SECTION × N iter)

#### SCENARIO R2.2: `periodic_id_` atomic 无 data race
- **GIVEN** `enable_input_thread=true` 构造 ChatSession
- **WHEN** `~ChatSession()` 与 `input_thread_main` TimerGuard 并发执行
- **THEN** `periodic_id_` 读写均通过 atomic op, TSan 0 warning

## MODIFIED Requirements

无 (公开 API 不变).

## UNCHANGED Requirements

- ITimerService contract 不变 (cancel 与 callback 不互斥的公开约束保持)
- ChatSession 公开 API 不变
- EventLogWriter 公开 API 不变

## 风险验收

| 风险 | 验收 |
|---|---|
| EventLogWriter file_lock 锁顺序错 (after snapshot.empty() check) | 复用 1456752 SessionWriter fix 同模式 (实战验证) |
| chat_session in_flight counter 死锁 (callback 异常未 decrement) | RAII guard 包裹 callback body, 析构无条件 decrement + notify |
| Callback 在 cancel 返回后访问已销毁成员 | ~Impl() step ①.5 barrier wait 保证 callback 必结束前不进入 step ②-⑤ |