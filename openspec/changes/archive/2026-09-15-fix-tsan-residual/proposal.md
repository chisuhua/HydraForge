## Why

TSan baseline (2026-09-13 Sprint 33 lock-order fix 后) 扫出 2 项生产代码 TSan race，均未登记为 OpenSpec change:

1. **`src/core/event_log.cpp` EventLogWriter streambuf writev race (#200)** — `flush_loop` (后台 std::thread, 默认 10ms 周期) 与 `flush_sync` (同步 API) 都对 `std::ofstream file_` 无 lock 写入。`std::ofstream` 非线程安全 — 并发 `<<` 操作调用 `basic_streambuf::xsputn` 时可能 interleaving 损坏文件内容。**根因**: libstdc++ writev + basic_streambuf::xsputn 而非 ofstream 内部 memcpy。`buffer_mutex_` 只保护队列访问不保护 IO (参 1456752 SessionWriter fix 同模式)。
2. **`pdk/chat_session/src/chat_session.cpp:291/296 ~Impl() dtor race** — `periodic_id_` 在 `~Impl()` (line 295 `periodic_id_ = 0`) 与 `input_thread_main` TimerGuard 析构 (line 879-883 `*id_ptr = 0`) 并发 write-write。**且** timer callback `[this]` capture 在 `timer_->cancel()` 返回后仍可能 in-flight 触发 (per ITimerService contract line 86 "外部注入 timer 必须自行保证生命周期" + "cancel 与 callback 不互斥")，访问 `this->shutdown_check_pending_` / `this->input_` 时 `~Impl()` 可能已开始销毁 → UB。

**触发场景**: 现有 functional ctest 232/232 全绿，但 TSan 构建下触发 (test_chat_session + test_chat_session_queues 触发 dtor race, test_event_log_* 触发 streambuf race)。

## What Changes

- **新增** `src/core/event_log.h::EventLogWriter::file_mutex_` 成员 (参考 1456752 SessionWriter 模式)
- **修改** `src/core/event_log.cpp::EventLogWriter::flush_sync` — 在 `snapshot.empty()` 检查**之前**获取 `file_lock` (per SessionWriter fix lesson "锁获取顺序关键")
- **修改** `src/core/event_log.cpp::EventLogWriter::flush_loop` — 写文件_前获取 `file_lock`
- **修改** `include/agenticdsl/pdk/chat_session.h::ChatSession::Impl` — `periodic_id_` 改 `std::atomic<std::uint64_t>`
- **修改** `pdk/chat_session/src/chat_session.cpp::ChatSession::Impl::Impl` — 新增 `std::atomic<int> in_flight_callbacks_` + `std::mutex in_flight_mutex_` + `std::condition_variable in_flight_cv_` 成员 (per ITimerService contract callback lifetime 约束)
- **修改** `~Impl()` — 在 `timer_ = nullptr` **之前**等待 in-flight callback 完成
- **修改** timer callback lambda — increment/decrement `in_flight_callbacks_` + notify cv (确保 callback lifetime 与 Impl destructor 同步)

### Non-goals

- 不改 ITimerService contract (cancel 与 callback 不互斥的语义是公开 API 约束)
- 不改 SessionWriter (1456752 已 ship)
- 不修 7.S29-1 / KI-1 / KI-2 等 pre-existing KIs
- 不动 EventLogWriter 的 rotate 逻辑 (仅加 file_mutex_)

## 影响面

| 类型 | 范围 |
|---|---|
| 生产代码 | `src/core/event_log.{h,cpp}` +6/-2 + `pdk/chat_session/src/chat_session.{h,cpp}` +18/-4 |
| 公开 API | **无变化** |
| ABI | **无变化** |
| 现有测试 | **0 行为变更** (functional ctest 232/232 PASS); TSan warnings -2 |

## 验收

- **A1** TSan 构建下 `ctest --test-dir build-tsan` 无新增 warning (baseline 排除 pre-existing KIs)
- **A2** functional ctest `-j1` 232/232 PASS 零回归
- **A3** 新增回归守卫测试:
  - `tests/test_event_log_concurrent.cpp` (新建, ~30 行) — 50 iter × 7 thread 并发 flush_loop + flush_sync 验证 records 数 ≥ 期望
  - `tests/test_chat_session_dtor.cpp` (GLOB 自动注册) — 验证 ~Impl() 在 input thread + timer callback 活跃时不触发 TSan warning
- **A4** `ctest -E test_timer_service` 排除 KI-1 后 0 failure
- **A5** AGENTS.md §沉淀源新增 2026-09-15 case study (本 fix 双修复)

## 风险

| 风险 | 等级 | 缓解 |
|---|---|---|
| EventLogWriter file_mutex_ 锁顺序错 (after snapshot.empty() check) | 中 | 直接复用 1456752 SessionWriter 同模式 (经实战验证) |
| chat_session in_flight counter 死锁 (callback 异常未 decrement) | 低 | RAII guard 包裹 callback (Sprint 31 D4 模式) |
| TSan 构建下其他未发现 race 暴露 | 低 | 本 change 仅修已确认 2 项, follow-up 留独立 change |

## 参考模式

- **AGENTS.md 模式 #7 v2** (2026-09-13): `std::queue<T>` 线程安全 ≠ `std::ofstream` 线程安全 — IO 路径必须独立 mutex。1456752 SessionWriter fix 即此模式首次应用。
- **AGENTS.md 模式 #5** (Sprint 31 D4): self-pipe + 析构 5 步顺序
- **ITimerService contract line 86** (timer_service.h): "外部注入 timer 必须自行保证生命周期" — ChatSession 显式需要 in-flight barrier