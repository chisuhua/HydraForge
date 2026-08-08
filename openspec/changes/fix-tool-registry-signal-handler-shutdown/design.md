## Context

`pdk_chat_demo` 在 mock 模式下，SIGINT/SIGTERM 信号触发 `signal_handler` (`examples/pdk_chat_demo/main.cpp:71-79`) 直接调用 `unload_all_plugins(*g_loader)` → `std::exit(0)`，跳过 `DSLEngine` / `ToolRegistry` 析构。Plugin `.so` 被 `dlclose()` 后，`ToolRegistry::tools_` 中 `std::function` 回调隐式析构 → **SIGSEGV**。

历史 commit `c7a95d7` 已修复正常 shutdown 路径与 `StartupCleanupGuard` 早退路径，但 signal handler 路径仍是 use-after-unload。完整审计见 `docs/audits/2026-08-08-chat-async-io-steering-pre-approval.md` Finding 2。

### 现有架构保证

`src/core/engine.h:199-205` 声明成员顺序：
```cpp
plugin_loader_  // 先声明 → 最后析构
...
tool_registry_  // 后声明 → 先析构（成员反向析构）
```

该声明顺序保证 `DSLEngine::~DSLEngine()` 内部 `tool_registry_` 先于 `plugin_loader_` 析构，确保 `dlclose()` 之前 `ToolRegistry` 已销毁回调目标。

**signal handler 绕过了这个保证**，必须修复。

## Goals / Non-Goals

**Goals:**
- 修复 SIGINT/SIGTERM 触发的 SIGSEGV（用户可触达的崩溃路径）
- 保持现有 `engine.h:199-205` 成员顺序的析构保证
- signal handler 仅做 async-signal-safe 操作（atomic store）
- main 循环观察 flag 后走正常有序清理路径
- 新增 2 个子进程回归测试（YAML 校验失败 + SIGINT）

**Non-Goals:**
- 不修改 `ToolRegistry` 公开 API
- 不引入 plugin 所有权追踪
- 不全面 audit signal_handler async-signal-safe 合规性（仅修复本具体路径）
- 不修改 `StartupCleanupGuard`（已 ship，不回归）
- 不修复 pre-existing ASan/TSan 失败（`test_cost_tracking_decorator` 等）

## Decisions

### Decision 1: Atomic flag 模式 vs self-pipe vs sigaction

**选择**: `std::atomic<bool>` + `std::signal()`

**理由**:
- **简单性**: 一行 flag 即可表达 shutdown 意图
- **零依赖**: 不引入 eventfd / pipe / 自定义 event loop
- **合规性**: `std::atomic<bool>::store(true, release)` 是 async-signal-safe 操作（C++20 标准保证）
- **现有模式**: 项目已用 `session.consume_budget_alert()` atomic flag 模式（main.cpp:507）处理类似 cross-thread 通知

**替代方案考虑**:
- **self-pipe 模式**: 写 pipe + main 端 select/poll — 增加 I/O 多路复用复杂度，且 main 循环当前为 blocking getline，引入 poll 需要重写循环结构
- **sigaction + SA_RESTART**: 仅控制系统调用重启行为，不解决 signal handler 直接 unload 的根本问题
- **eventfd**: Linux-specific，与项目跨平台目标不符

### Decision 2: signal_handler 实现位置

**选择**: 修改现有 `signal_handler` (`main.cpp:71-79`)，新增 `g_shutdown_requested` 全局 atomic

**理由**:
- 改动最小 delta（1 函数 + 1 全局变量）
- 不新增文件/类
- 现有 signal handler 已经是项目定义的入口

**替代方案考虑**:
- **新建 SignalRouter 类**: 过度抽象，对单 flag 单 signal type 是 over-engineering
- **使用 std::stop_source 全局**: 引入 jthread/future 依赖，但本场景不需要线程协调

### Decision 3: main 循环 flag 检查位置

**选择**: 在 `while(std::getline(...))` 循环条件之后、阻塞操作之前检查 flag

**理由**:
- 最小侵入现有循环结构
- `std::getline` 阻塞调用前检查 → 立即退出
- `session.chat()` 后检查 → 等当前 turn 自然完成后退出（Phase B 之后才能 turn 中断）

**具体位置**:
```cpp
while (std::getline(std::cin, input)) {
    if (g_shutdown_requested.load(std::memory_order_acquire)) {
        break;  // 走正常 shutdown 路径
    }
    // ... 现有逻辑
}
```

### Decision 4: 测试策略

**选择**: 子进程 fork + exec 启动 pdk_chat_demo，发送信号/触发 validation 失败，断言退出码和退出原因

**理由**:
- 子进程隔离 signal handler 副作用
- 可捕获 stderr 中的 SIGSEGV 信号（ASan 输出）
- 与现有 `test_pdk_chat_demo_session_tree_cli_flags.cpp` 子进程测试模式一致

**测试 1**: YAML 校验失败回归（mock 模式 + 故意配置 invalid YAML）
- 期望: exit code != 0 (validation error code)，stderr 含 "DSL Schema Validation FAILED"
- 反断言: 不含 "SIGSEGV" / "Segmentation fault" / ASan 错误

**测试 2**: SIGINT 干净退出回归（mock 模式 + 等待 prompt → SIGTERM）
- 期望: exit code 0，stderr 含 "shutdown"
- 反断言: 不含 "SIGSEGV" / "Segmentation fault" / ASan 错误

## Risks / Trade-offs

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| `std::getline` 阻塞时不响应 flag | 🟡 Medium | 🟢 Low (sub-second) | 用户再次 Ctrl+C 触发 EOF，或 `std::getline` 超时（未来 Phase B 处理） |
| flag 初始化早于 signal handler 注册 | 🟢 Low | 🟢 Low | 文档保证 `g_shutdown_requested` 在 `std::signal()` 调用前构造（main.cpp:147 之前） |
| main 线程在 `session.chat()` 中时不响应 signal | 🟡 Medium | 🟢 Low | chat turn 自然完成后退出（毫秒到秒级，UX 可接受） |
| atomic<bool> 平台兼容性 | 🟢 Low | 🟢 Low | C++20 std::atomic 在所有目标平台（Linux x86_64 / arm64）原生支持 |
| 测试 flaky due to timing | 🟡 Medium | 🟡 Medium | 子进程测试使用固定 sleep + 同步等待，非 race-prone 操作 |

## Migration Plan

### 部署

无 schema 变更 / 无 ABI 变更 / 无依赖变更 → 零迁移成本

### 回滚

回滚至 commit `c7a95d7` 之前即可恢复原 `signal_handler` 行为（无数据迁移）

### 兼容性

- `pdk_chat_demo` 用户行为变化：从"Ctrl+C 立即 exit(0)"改为"Ctrl+C 触发有序清理后 exit(0)"（用户可见延迟 ≤ 1 秒）
- 脚本/子进程调用方：`exit code` 语义不变（仍为 0 on 正常 exit）

## Open Questions

无 — 所有决策基于现有审计证据，零外部依赖，零向后不兼容变更。