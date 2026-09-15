# Tasks — fix-tsan-residual-2026-09-15

## Phase 1: 实施 (2 atomic commits)

- [ ] **T1.1** 修复 `src/core/event_log.{h,cpp}` — EventLogWriter file_mutex_ (复用 1456752 模式)
  - `event_log.h`: `std::mutex file_mutex_;` 成员 (放在 `buffer_mutex_` 之后)
  - `event_log.cpp::flush_sync` (L120-138): 在 `if (!file_.is_open() || snapshot.empty()) return;` **之前**添加 `std::lock_guard<std::mutex> file_lock(file_mutex_);`
  - `event_log.cpp::flush_loop` (L155-177): 在 `if (!file_.is_open() || snapshot.empty()) continue;` 之后、循环体前添加 `std::lock_guard<std::mutex> file_lock(file_mutex_);`
  - 验证 `rotate_if_needed()` 调用点 (line 135 + 173) 都在 file_lock 作用域内 (已经是的)
  - 注释 (3 行): 解释锁必须在 snapshot.empty() 之前的理由 (引用 1456752 SessionWriter fix lesson)

- [ ] **T1.2** 修复 `pdk/chat_session/{chat_session.h,chat_session.cpp}` — ~Impl() dtor race
  - `chat_session.h::ChatSession::Impl`: 
    - `agenticdsl::ITimerService::TimerId periodic_id_ = 0;` → `std::atomic<std::atomic<...>>` (TimerId = uint64_t, trivially copyable, 安全)
    - 新增成员: `std::atomic<int> in_flight_callbacks_{0};` + `std::mutex in_flight_mutex_;` + `std::condition_variable in_flight_cv_;`
  - `chat_session.cpp::input_thread_main` (~L850): 
    - timer callback lambda 内: 入口 `in_flight_callbacks_.fetch_add(1, std::memory_order_acq_rel);` + 出口 `in_flight_callbacks_.fetch_sub(1, std::memory_order_acq_rel); in_flight_cv_.notify_all();`
    - **改用 RAII guard** 避免异常路径漏 decrement (模式 #5): `struct CallbackGuard { std::atomic<int>* ctr; std::condition_variable* cv; ~CallbackGuard() { ctr->fetch_sub(1, std::memory_order_acq_rel); cv->notify_all(); } } guard{&in_flight_callbacks_, &in_flight_cv_};`
  - `chat_session.cpp::Impl::~Impl` (L291-311): 
    - line 293 `if (periodic_id_ != 0 && timer_)` 改用 atomic load
    - line 295 `periodic_id_ = 0` → `periodic_id_.exchange(0)` 
    - **新增 step ①.5**: cancel timer 后, 在 `timer_ = nullptr` **之前**等待 in-flight callback: `{ std::unique_lock<std::mutex> lock(in_flight_mutex_); in_flight_cv_.wait(lock, [this]{ return in_flight_callbacks_.load() == 0; }); }`
    - 5 步析构顺序: ① cancel timer → ①.5 wait in-flight → ② timer_=nullptr → ③ stop_input_thread_ → ④ notify cv + join → ⑤ auto-destroy
    - input_thread_main TimerGuard (line 879): `*id_ptr = 0` → `id_ptr->store(0)` (atomic)

- [ ] **T1.3** 新增回归守卫测试
  - `tests/test_event_log_concurrent.cpp` (~30 行): 50 iter × 7 thread 并发 `flush_loop` + `flush_sync` 验证 records 数 ≥ 期望 (参 1456752 fuzz pattern)
  - `tests/test_chat_session_dtor.cpp` (新建, GLOB 自动注册): 构造 ChatSession + 启 input_thread + 立即 ~ChatSession() 验证不触发 TSan warning (catch2 SECTION 多 iter)

- [ ] **T1.4** 构建 + TSan gate
  - functional: `cmake --build build --parallel $(nproc) && ctest --test-dir build -j1 -E test_timer_service` (排除 KI-1)
  - TSan: `cmake --build build-tsan --parallel $(nproc) && ctest --test-dir build-tsan -E test_timer_service -R test_event_log|test_chat_session`
  - 期望: functional 232/232 PASS, TSan 0 新增 warning (vs baseline #191 #192 #199 #200 + pre-existing)

## Phase 2: 提交 (2 atomic commits)

- [ ] **T2.1** Commit 1: `fix(event_log): serialize file writes between flush_loop and flush_sync`
  - 文件: `src/core/event_log.{h,cpp}` + `tests/test_event_log_concurrent.cpp` (新建)
  - 格式: 参考 1456752 commit message 风格

- [ ] **T2.2** Commit 2: `fix(chat_session): serialize ~Impl() dtor with in-flight timer callback`
  - 文件: `include/agenticdsl/pdk/chat_session.h` + `pdk/chat_session/src/chat_session.cpp` + `tests/test_chat_session_dtor.cpp` (新建)
  - 格式: 解释 ITimerService contract line 86 callback lifetime 约束

- [ ] **T2.3** AGENTS.md §沉淀源 新增 2026-09-15 case study (本 fix 双修复)

## 验收清单

| 验收 | 命令 | 期望 |
|---|---|---|
| A1 TSan 无新增 | `ctest --test-dir build-tsan -E test_timer_service` | 0 warning (排除 KI-1 + pre-existing) |
| A2 functional 零回归 | `ctest --test-dir build -j1 -E test_timer_service` | 232/232 PASS |
| A3 回归守卫 | `ctest -R test_event_log_concurrent\|test_chat_session_dtor` | PASS (50 iter fuzz + dtor race guard) |
| A4 KI 排除 | `ctest -E test_timer_service` | 0 failure |
| A5 AGENTS.md 沉淀 | grep "2026-09-15" AGENTS.md | case study 段存在 |

## Out-of-scope (follow-up notes)

- **KI-1** Catch2 v3.7.0 + std::jthread reporter bug — pre-existing, 已 ship-with-known-issue, 独立 fix
- **KI-2** test_llm_provider_propagation parallel flake — pre-existing
- **7.S29-1** Sprint 29 inherent limitation — pre-existing
- **TSan #191 #192 #199** (SessionWriter queue mutex) — 1456752 已 ship
- **TSan #200** EventLogWriter streambuf — **本 change 修复**