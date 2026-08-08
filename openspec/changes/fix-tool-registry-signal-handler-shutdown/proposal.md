# fix-tool-registry-signal-handler-shutdown

**STATUS**: 🔍 Proposed
**日期**: 2026-08-08
**来源**: docs/audits/2026-08-08-chat-async-io-steering-pre-approval.md Finding 2
**类型**: bugfix (shutdown ordering)
**优先级**: P0 (阻塞 chat-async-io Phase B E2E 测试)
**关联改进**: improvements/fix-tool-registry-signal-handler-shutdown.md

## Why

`pdk_chat_demo` 在 mock 模式下，SIGINT/SIGTERM 信号触发 `signal_handler` (`examples/pdk_chat_demo/main.cpp:71-79`) 直接调用 `unload_all_plugins(*g_loader)` → `std::exit(0)`，跳过 `DSLEngine` / `ToolRegistry` 析构。Plugin `.so` 被 `dlclose()` 后，`ToolRegistry::tools_` 中 `std::function` 回调隐式析构 → **SIGSEGV**。

历史 commit `c7a95d7`（"plugins unloaded before DSLEngine destroying dangling function ptrs"）已修复正常 shutdown 路径与 `StartupCleanupGuard` 早退路径，但 `signal_handler` 路径仍是 use-after-unload。

**直接阻塞性**：
- 阻塞 `chat-async-io-cancellation-chain` (Phase B) 的 mid-loop cancel E2E 测试（mock 模式崩溃导致测试不可靠）
- 阻塞任何依赖 mock 模式 + SIGINT 的 steering 验证

**问题严重度**：🔴 HIGH — 用户操作可达的崩溃路径（Ctrl+C 触发）

## What Changes

### 核心改动

- **`signal_handler` 改为设置 atomic shutdown flag**：移除直接 `unload_all_plugins()` 调用，仅置 `g_shutdown_requested.store(true)`
- **main 交互循环检测 flag 后走正常有序清理路径**：`engine.reset()` 先于 `unload_all_plugins(loader)`，保持 `engine.h:199-205` 成员声明顺序的反向析构保证
- **新增 `g_shutdown_requested` atomic<bool> 全局变量**（signal-safe 读写）
- **新增 SIGINT 子进程回归测试**：启动 demo → 等待加载完成 → 发送 SIGTERM → 断言无 SIGSEGV，exit code 正常
- **新增 YAML 校验失败子进程回归测试**：触发 validation error → 断言走 `StartupCleanupGuard` 已 ship 的有序路径，无 SIGSEGV

### 不修改范围 (Non-goals)

- 不修改 `ToolRegistry` 公开 API 或析构语义
- 不引入 plugin 所有权追踪（独立 follow-up）
- 不全面 audit signal_handler 的 async-signal-safe 合规性（仅修复本具体路径）
- 不修复 pre-existing ASan/TSan 失败（`test_cost_tracking_decorator` 等）

## Capabilities

### New Capabilities

- `shutdown-signal-routing`: signal handler 委派给 main 线程的有序清理路径，定义 SIGINT/SIGTERM 经 atomic flag 传播至 main 循环的契约

### Modified Capabilities

无 — 本改动不修改任何已有 spec 的 REQUIREMENTS（仅修复实现层 bug）

## Impact

### 受影响代码

- `examples/pdk_chat_demo/main.cpp:71-79` — `signal_handler` 实现
- `examples/pdk_chat_demo/main.cpp:466-522` — main 交互循环（增加 flag 检测 + 跳出条件）
- `examples/pdk_chat_demo/main.cpp` — 新增 `g_shutdown_requested` atomic 全局
- `examples/pdk_chat_demo/tests/` — 新增 2 个子进程回归测试

### API 影响

- 无 public API 变更
- 无 ABI 变更
- 无依赖变更

### 依赖系统

- `pdk_chat_demo` 启动路径（`main.cpp:145-205` engine + loader 初始化）
- `StartupCleanupGuard` (`main.cpp:82-98` 早退清理)
- PluginLoader (`src/modules/plugin/plugin_loader.cpp:382-408` `unload_all_plugins`)
- DSLEngine (`src/core/engine.h:199-205` 成员声明顺序保证)

### 测试影响

- 新增测试：YAML 校验失败 SIGSEGV 回归 + SIGINT 干净退出回归
- 不修改已有测试
- 期望 ctest 增量：+2 测试，pre-existing 5 失败不变

### 风险评估

- **风险等级**: 🟢 LOW
- **理由**: 改动局限于 `signal_handler` 单函数 + main 循环 1 个分支；正常路径与 YAML 路径已 ship（不回归）；最小 delta 修复（不引入新抽象）
- **回退方案**: 若新测试失败，回滚至原 `signal_handler` 即可（无 schema 变更）