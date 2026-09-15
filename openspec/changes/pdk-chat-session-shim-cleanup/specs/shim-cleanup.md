# Spec — pdk-chat-session-shim-cleanup

## REMOVED Requirements

### R-REMOVED-1: pdk_chat_demo::ChatSession 命名空间别名

**shim 清理前** (per `docs/superpowers/plans/2026-09-11-chat-session-pdk-lift-change1.md` §6.1):
```cpp
// examples/pdk_chat_demo/chat_session.h
namespace pdk_chat_demo {
using hydraforge::pdk::AgentConfig;
using hydraforge::pdk::ChatConfig;
using hydraforge::pdk::ChatResult;
using hydraforge::pdk::ChatSession;
using hydraforge::pdk::InputMessage;
using hydraforge::pdk::ObservabilityConfig;
using hydraforge::pdk::PluginConfig;
using hydraforge::pdk::QueueKind;
using hydraforge::pdk::SessionConfig;
}
```

**shim 清理后**:
- 该头文件**整个删除** (`git rm examples/pdk_chat_demo/chat_session.h`)
- 所有引用 `pdk_chat_demo::ChatSession` 等的代码必须改为 `hydraforge::pdk::ChatSession` (或用 `using namespace hydraforge::pdk;`)

**SCENARIO R-REMOVED-1.1**: 编译失败检测
- **GIVEN** 代码 `pdk_chat_demo::ChatSession session(...);`
- **WHEN** 编译
- **THEN** 编译失败 `'pdk_chat_demo::ChatSession' was not declared` — 强制迁移

### R-REMOVED-2: examples/pdk_chat_demo/cancellation_registry.h 全局 alias

**shim 清理前**:
```cpp
// examples/pdk_chat_demo/cancellation_registry.h
using hydraforge::pdk::CancellationRegistry;  // global-scope using
```

**shim 清理后**:
- 头文件**整个删除**
- 代码改用 `#include <agenticdsl/pdk/cancellation_registry.h>` + `hydraforge::pdk::CancellationRegistry`

### R-REMOVED-3: commands/command_globals.h 的 using 别名

**shim 清理前**:
```cpp
namespace pdk_chat_demo {
  using ChatSession = hydraforge::pdk::ChatSession;
  extern hydraforge::pdk::ChatSession* g_command_session;
}
```

**shim 清理后**:
- 删除 `using ChatSession = ...`
- `g_command_session` 类型用全名 `hydraforge::pdk::ChatSession*`

### R-REMOVED-4: commands/cancellation_globals.h 的 using 别名

**shim 清理前**:
```cpp
namespace pdk_chat_demo {
  using CancellationRegistry = hydraforge::pdk::CancellationRegistry;
  extern std::shared_ptr<CancellationRegistry> g_cancellation_registry;
}
```

**shim 清理后**:
- 整个头文件**移动**到 `pdk/chat_session/include/agenticdsl/pdk/cancellation_globals.h`
- 命名空间从 `pdk_chat_demo` 改为 `hydraforge::pdk`
- `g_cancellation_registry` **变量定义** 移到 `pdk/chat_session/src/cancellation_globals.cpp` (namespace `hydraforge::pdk`)
- 所有引用 `pdk_chat_demo::g_cancellation_registry` 改为 `hydraforge::pdk::g_cancellation_registry`

## MODIFIED Requirements

### M1: examples/pdk_chat_demo/tests/CMakeLists.txt 移除 examples include 依赖

**Before**:
```cmake
target_include_directories(test_xxx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..  # for #include "chat_session.h"
)
```

**After** (移除 `..` 因为 `#include <agenticdsl/pdk/...>` 不再需要相对路径):
```cmake
target_include_directories(test_xxx PRIVATE
    ${CATCH_INCLUDE_DIR}
    # 注意: ${PROJECT_SOURCE_DIR}/include 已由 agenticdsl_includes 通过 PUBLIC 链接提供
)
```

**影响**: 14 个 test_*.cpp 仍可 `target_link_libraries(test_xxx PRIVATE agenticdsl_includes)`, 通过 `agenticdsl_includes` 的 PUBLIC include path 拿到 `${PROJECT_SOURCE_DIR}/include`.

### M2: examples/pdk_chat_demo/CMakeLists.txt

无需改动 (SOURCES 不变, pdk_chat_session_obj 已链接; main.cpp 自己引用 `<agenticdsl/pdk/...>` 通过 agenticdsl_includes 解析).

### M3: pdk/loop_agent/CMakeLists.txt

**Before** (3 行 include 路径, 跨树):
```cmake
target_include_directories(LoopAgent PRIVATE
    ...
    ${PROJECT_SOURCE_DIR}/examples/pdk_chat_demo
    ${PROJECT_SOURCE_DIR}/examples/pdk_chat_demo/commands
    ...
)
target_sources(LoopAgent PRIVATE
    ${PROJECT_SOURCE_DIR}/examples/pdk_chat_demo/commands/cancellation_globals.cpp
)
```

**After** (PDK 内部解决, 无跨树):
```cmake
target_include_directories(LoopAgent PRIVATE
    ...
    # 移除 examples/pdk_chat_demo + examples/pdk_chat_demo/commands
    # 改用 pdk_chat_session 公开传播
)
target_sources(LoopAgent PRIVATE
    src/pdk_entry.cpp
    # 移除 examples/pdk_chat_demo/commands/cancellation_globals.cpp (已迁到 pdk_chat_session)
)
target_link_libraries(LoopAgent PRIVATE pdk_chat_session)
```

## ADDED Requirements

### A1: `g_cancellation_registry` 位置迁移 (可见性保证)

**After**: `hydraforge::pdk::g_cancellation_registry` 在 `pdk/chat_session/src/cancellation_globals.cpp` 定义, 通过 `pdk_chat_session` (STATIC 库) 链接到 `LoopAgent.so` 与 `pdk_chat_demo` 可执行.

**SCENARIO A1.1**: 符号可见性
- **GIVEN** `pdk_chat_session` 链接到 `pdk_chat_demo` 与 `LoopAgent`
- **WHEN** `nm build/pdk/loop_agent/libLoopAgent.so | grep g_cancellation_registry`
- **THEN** 输出含 `hydraforge::pdk::g_cancellation_registry` (U 符号)

**SCENARIO A1.2**: main 启动期赋值正常
- **GIVEN** main.cpp 编译时类型为 `hydraforge::pdk::g_cancellation_registry`
- **WHEN** `main.cpp` 启动期 `hydraforge::pdk::g_cancellation_registry = make_shared<...>();`
- **THEN** 赋值成功, `g_cancellation_registry.get()` 后续被 ChatSession 与 loop_agent 共享访问

## UNCHANGED Requirements

- `ChatSession` / `CancellationRegistry` / `ResumeToken` 的所有公开 API 签名**不变**
- `pdk_chat_session` 静态库的 `agenticdsl_common` / `agenticdsl_core` 链接关系**不变**
- `pdk_chat_demo` 主可执行的输入输出**不变** (行为字节级一致)
- examples 树 8 个 commands (help/compact/model/tree/fork/clone/cancel/cancel_command 等) 的**业务行为**不变
- core 树 (`tests/test_pdk_chat_session*.cpp`) **不变** (已在 Change 1/2 用 `hydraforge::pdk`)

## 风险

| 风险 | 等级 | 缓解 |
|---|---|---|
| 14 个 tests 同时修改 → 编译错误级联 | 中 | sed 批量替换后**逐文件**编译验证 (cmake 增量) |
| 某 test 用 `pdk_chat_demo::Xxx` (Xxx 非 ChatSession/配置/CancellationRegistry) → 仍 in `pdk_chat_demo` namespace, 需保 using | 中 | grep `pdk_chat_demo::` 在 tests/ 残留, 单独评估每个符号 |
| `pdk_chat_demo` namespace 整体是否被所有引用清除 | 低 | 大部分 files `using namespace pdk_chat_demo` → `hydraforge::pdk` 即可; 个别 `pdk_chat_demo::Xxx` 限定应**不变** (那是 examples namespace, 不是 shim) |
| cancellation_globals.h 移 PDK 后, examples commands/*.cpp 不再能 include (没在 include 路径) | 低 | 显式 include 路径 `${PROJECT_SOURCE_DIR}/pdk/chat_session/include` 已加 |
| 1 个原子 commit 包含 22 个文件修改, 风险扩散 | 中 | 分 2 commit: (1) PDK 移动 cancellation_globals + shim 删除 (2) 14 tests + 2 commands + main + loop_agent 迁移 |
