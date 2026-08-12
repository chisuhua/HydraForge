# fix-tool-registry-signal-handler-shutdown

**优先级**: P0 | **来源**: docs/audits/2026-08-08-chat-async-io-steering-pre-approval.md Finding 2
**阶段**: chat-async-io Phase 0 (precondition) | **分类**: bugfix-shutdown
**类型**: bugfix

## 架构依据

`pdk_chat_demo` mock 模式下，YAML 校验失败路径或 SIGINT/SIGTERM 触发 `signal_handler` 直接调用 `unload_all_plugins()` → `std::exit(0)`，跳过 `DSLEngine` / `ToolRegistry` 析构。

历史 commit `c7a95d7` 已修复正常 shutdown 路径与 `StartupCleanupGuard` 早退路径，但 signal handler 路径仍存在 use-after-unload：plugin `.so` 被 `dlclose()` 后，`ToolRegistry::tools_` 中 `std::function` 回调隐式析构 → SIGSEGV。

直接阻塞 `chat-async-io-steering` Phase B 的 E2E 测试（mock 模式崩溃导致 steering 测试不可靠）。

## 范围
- **In Scope**: `signal_handler` 改为设置 atomic shutdown flag；main loop 检测 flag 后走正常有序清理路径；新增 SIGINT 子进程回归测试 + YAML 校验失败回归测试。
- **Out Scope**: ToolRegistry 插件所有权改造（独立 follow-up）；新增 shutdown 抽象层；signal_handler async-signal-safe 全面审计。

## 关键场景
- GIVEN demo 启动中，WHEN YAML 校验失败，THEN `StartupCleanupGuard` 已走有序路径（已 ship，不变）。
- GIVEN demo 交互中，WHEN 用户按下 Ctrl+C (SIGINT)，THEN signal handler 仅设置 `g_shutdown_requested = true`，main loop 检测后走 `engine.reset()` → `unload_all_plugins()` → 退出，零 SIGSEGV。
- GIVEN demo 加载 plugin 后启动失败，WHEN 任何退出路径触发，THEN 销毁顺序严格保证 `ToolRegistry` → `PluginLoader`。

## 技术约束
- MUST 销毁顺序保持 `DSLEngine::~DSLEngine()` 先于 `PluginLoader::unload_all_plugins()`（`engine.h:199-205` 已声明该顺序）。
- MUST signal handler 不调用任何非 async-signal-safe 操作。
- MUST 不修改 `ToolRegistry` 公开 API。
- SHOULD ASan / TSan 全量零回归。

## 验收标准
- mock 模式 + YAML 校验失败子进程测试 PASS（无 SIGSEGV，exit code = validation error code）。
- SIGINT 子进程测试 PASS（无 SIGSEGV，干净退出）。
- ctest 全量零回归（pre-existing 失败保持不变）。
- ASan 全量 0 新增 error。