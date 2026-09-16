## Why

WorkflowCallbackChannel timer callback `poll_once()` `[this]` capture 存在与 ChatSession 同根 dtor race (per ITimerService contract line 86 + AGENTS.md 模式 #9 contract drain API):

- `cancel()` 不等待已收集但未执行 callback
- `poll_once` 在 cancel 返回后仍可 in-flight 触发, 访问 `this->backend_` / `this->handlers_` / `this->workflow_id_` 时 channel 可能已开始析构 → UB
- 旧 contract note (workflow_callback_channel.h:47-49) 仅文档化风险, 无 barrier 实现

## What Changes

ship via commit cde7713 (2026-09-16):
- periodic_id_ → std::atomic<TimerId>
- 新增 in_flight_callbacks_ (atomic) + in_flight_mutex_ + in_flight_cv_ barrier 成员
- timer callback 用 RAII guard 包裹 (increment/decrement + notify)
- stop() 5 步析构顺序 + 新增 step ①.5 barrier wait in_flight == 0
- 显式同步消除调用方误用 race, 即使 caller 已遵循推荐顺序

回归守卫 (新建 tests/test_temporal_agent_workflow_callback_dtor_race.cpp):
- 11 assertions / 2 cases PASS (functional)
- TSan 0 warnings
- TEST_CASE 1: dtor blocks until callback completes (SlowBackend 在 consume_signals 阻塞, dtor thread 200ms 内 dtor_done=false, release 后立即完成)
- TEST_CASE 2: stop idempotent (多次调用无 deadlock)

## Out-of-scope (其他 change / 未来)

- 其他持有 timer callback `[this]` 的 PDK consumer (需全 audit 应用同模式 #9)
- WorkflowCallbackChannel default timer 路径 nullptr (已 unique_ptr RAII 安全) 不需改
