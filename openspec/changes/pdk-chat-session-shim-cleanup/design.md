# Design — pdk-chat-session-shim-cleanup

## 迁移映射

| Before (shim-mediated) | After (PDK-direct) |
|---|---|
| `#include "chat_session.h"` (resolves to `examples/.../chat_session.h`) | `#include <agenticdsl/pdk/chat_session.h>` |
| `#include "cancellation_registry.h"` (resolves to `examples/.../cancellation_registry.h`) | `#include <agenticdsl/pdk/cancellation_registry.h>` |
| `#include "commands/cancellation_globals.h"` (examples/.../commands/) | `#include <agenticdsl/pdk/cancellation_globals.h>` (moved to pdk/chat_session/include/) |
| `using namespace pdk_chat_demo;` (tests) | `using namespace hydraforge::pdk;` |
| `pdk_chat_demo::ChatSession` (commands/command_globals.h) | `::hydraforge::pdk::ChatSession` (with leading `::` to force global resolution) |
| `pdk_chat_demo::CancellationRegistry` (main.cpp) | `hydraforge::pdk::CancellationRegistry` |
| `pdk_chat_demo::g_cancellation_registry` (main + loop_agent) | `hydraforge::pdk::g_cancellation_registry` |

## 关键设计决策

### D1: `commands/cancellation_globals.h` 移动到 PDK

**Why**: 现行结构要求 `pdk/loop_agent/src/pdk_entry.cpp` 通过 `${PROJECT_SOURCE_DIR}/examples/pdk_chat_demo` 与 `${PROJECT_SOURCE_DIR}/examples/pdk_chat_demo/commands` 两个 include 路径访问 examples tree — **PDK plugin 反向依赖 examples**，违反 ADR-0021 §3.5 (PDK 头文件应只依赖 `agenticdsl/contract/*.h`)。

**移动位置**: `pdk/chat_session/include/agenticdsl/pdk/cancellation_globals.h`

**理由选 pdk/chat_session 而非新建 pdk/common**:
- `cancellation_globals` 与 ChatSession 一起 ship（change 2 引入）
- 维持"按 change 物理就近"原则（避免散落多个 PDK 子目录）
- 公开 `g_cancellation_registry` 是 ChatSession 跨进程取消链状态

**导出方式**: `pdk_chat_session` 静态库 (PUBLIC include dir) 传播 — `agenticdsl/pdk/cancellation_globals.h` 自动可见

**实施细节**:
```cpp
// pdk/chat_session/include/agenticdsl/pdk/cancellation_globals.h
#pragma once
#include <memory>
#include <agenticdsl/pdk/cancellation_registry.h>

namespace hydraforge::pdk {
// pdk_chat_demo 兼容别名: 1 Sprint 后已过期, 移除
extern std::shared_ptr<CancellationRegistry> g_cancellation_registry;
}
```

注意: `g_cancellation_registry` **变量定义** 仍需保留在某个 .cpp 中 (extern 声明必须有定义)。从 `examples/pdk_chat_demo/commands/cancellation_globals.cpp` 迁到 `pdk/chat_session/src/cancellation_globals.cpp`:

```cpp
// pdk/chat_session/src/cancellation_globals.cpp
#include "agenticdsl/pdk/cancellation_globals.h"
namespace hydraforge::pdk {
std::shared_ptr<CancellationRegistry> g_cancellation_registry = nullptr;
}
```

更新 `pdk/chat_session/CMakeLists.txt` 添加 `src/cancellation_globals.cpp` 到 SOURCES。

**副作用**: 
- `pdk/chat_session` (STATIC 库) 多了 1 个 TU, 链接到 `LoopAgent.so` / `pdk_chat_demo_obj` 后符号正确可见
- `pdk_chat_demo` (examples 主可执行) 链接 `pdk_chat_session` 公开接口, 符号仍可见

### D2: 14 个 tests 的 `using namespace` 替换

**挑战**: 14 个 test 文件用 `using namespace pdk_chat_demo;` 让 `ChatSession`/`ChatConfig`/etc. 无前缀可见。

**方案**:
- 替换为 `using namespace hydraforge::pdk;` — 包含 9 个迁移到 PDK 的类型名
- 同一文件内若还引用 `pdk_chat_demo::CommandRegistry`/`pdk_chat_demo::EventHandler` 等**未迁移**的类型, 显式加 `using pdk_chat_demo::CommandRegistry;`

**Verification**: `grep -rn "pdk_chat_demo::" --include="*.cpp" examples/pdk_chat_demo/tests/` 在 shim cleanup 后应只显示**未迁移**类型 (CommandRegistry, EventHandler, ChatConfig, ...) → 这些应 in `pdk_chat_demo` namespace (examples-app-specific) 或已迁到 `hydraforge::pdk` 而 tests 需用新 namespace.

注意: 实际 `ChatConfig`, `EventHandler`, `CommandRegistry`, `DslValidator`, `parse_cli_args` 等都**仍** in `pdk_chat_demo` namespace (它们是 examples-app-specific 业务类型, 不是 PDK 通用类型). 只有 `ChatSession` + `CancellationRegistry` + 配置结构体 + `QueueKind` 等迁到了 PDK.

**详细迁移示例** (`test_chat_session_queues.cpp`):
```cpp
// Before
#include "chat_session.h"
using namespace pdk_chat_demo;
TEST_CASE(...) {
  ChatSession session(nullptr, nullptr, nullptr, {}, {});
  REQUIRE(session.queue_size(QueueKind::Steering) == 0);
}

// After
#include <agenticdsl/pdk/chat_session.h>
using namespace hydraforge::pdk;
// 同时: pdk_chat_demo::Xxx (未迁移类型) 仍需 `pdk_chat_demo::Xxx` 限定
```

**注**: 实际 `test_chat_session_queues.cpp` 不引用 pdk_chat_demo::CommandRegistry 之类, 纯 `using namespace` 替换即可。

### D3: `command_globals.h` 内的 `using` 别名处理

`commands/command_globals.h` 当前:
```cpp
namespace agenticdsl { class ToolCoordinator; class CommandRegistry; class SessionManager; }
namespace hydraforge::pdk { class ChatSession; }

namespace pdk_chat_demo {
  using ChatSession = hydraforge::pdk::ChatSession;
  ...
}
```

`using ChatSession = ::hydraforge::pdk::ChatSession;` 在 `pdk_chat_demo` 内部提供 `pdk_chat_demo::ChatSession` 别名.

shim cleanup 后, 该 `using` 应**移除** (不再需要别名), 所有引用 `pdk_chat_demo::ChatSession` 的代码应改为 `hydraforge::pdk::ChatSession` 或 `using hydraforge::pdk::ChatSession;` 后简写.

**具体改动**:
```cpp
// command_globals.h Before
namespace hydraforge::pdk { class ChatSession; }
namespace pdk_chat_demo {
using ChatSession = hydraforge::pdk::ChatSession;
extern agenticdsl::ToolCoordinator* g_command_coordinator;
extern hydraforge::pdk::ChatSession* g_command_session;  // <-- 用 using
}

// command_globals.h After
#include <agenticdsl/pdk/chat_session.h>
namespace pdk_chat_demo {
extern agenticdsl::ToolCoordinator* g_command_coordinator;
extern hydraforge::pdk::ChatSession* g_command_session;  // <-- 直接用全名
}
```

**注**: `agenticdsl::SessionManager` 前置声明仍保留 (它是 examples-app state, 未迁到 PDK).

### D4: `cancellation_globals.h` 内的别名处理

类似 D3: `using CancellationRegistry = ...` 移除, 改用全名 `hydraforge::pdk::CancellationRegistry`.

## 实施步骤（高层）

```
1. 移动 cancellation_globals.h 到 PDK
   git mv examples/pdk_chat_demo/commands/cancellation_globals.h pdk/chat_session/include/agenticdsl/pdk/cancellation_globals.h
   git mv examples/pdk_chat_demo/commands/cancellation_globals.cpp pdk/chat_session/src/cancellation_globals.cpp
   修改 namespace pdk_chat_demo → namespace hydraforge::pdk
   pdk/chat_session/CMakeLists.txt: 添加 src/cancellation_globals.cpp 到 SOURCES

2. 批量替换 tests/ (14 个文件)
   sed -i 's|#include "chat_session.h"|#include <agenticdsl/pdk/chat_session.h>|g' examples/pdk_chat_demo/tests/*.cpp
   sed -i 's|#include "cancellation_registry.h"|#include <agenticdsl/pdk/cancellation_registry.h>|g' examples/pdk_chat_demo/tests/*.cpp
   sed -i 's|using namespace pdk_chat_demo;|using namespace hydraforge::pdk;|g' examples/pdk_chat_demo/tests/*.cpp
   # test_cancellation_registry.cpp 不 using namespace, 替换 CancellationRegistry 为 hydraforge::pdk::CancellationRegistry

3. 修改 commands/ (4 files)
   cancel_command.cpp, model_command.cpp: 替换 include
   command_globals.h/cpp: 替换 using alias
   cancellation_globals.h/cpp: 已删除 (移到 PDK)

4. 修改 main.cpp (1 file)
   替换 include + pdk_chat_demo::Xxx → hydraforge::pdk::Xxx

5. 修改 loop_agent pdk_entry.cpp (1 file)
   替换 include + pdk_chat_demo::g_cancellation_registry → hydraforge::pdk::

6. 删除 shim headers
   git rm examples/pdk_chat_demo/chat_session.h
   git rm examples/pdk_chat_demo/cancellation_registry.h

7. 修改 examples CMakeLists.txt + tests CMakeLists.txt
   移除 examples test 对 examples include dir 的依赖
   (test_x.cpp #include "chat_session.h" 改为 <agenticdsl/pdk/chat_session.h> 后, 不再需要 相对 include)
```

## 验收命令

```bash
# A1: 无 pdk_chat_demo::ChatSession / CancellationRegistry 引用 (shim 类型)
grep -rn "pdk_chat_demo::ChatSession\|pdk_chat_demo::CancellationRegistry" --include="*.h" --include="*.cpp" examples/ pdk/ tests/ src/

# A2: 无 using namespace pdk_chat_demo (tests/commands)
grep -rn "using namespace pdk_chat_demo" --include="*.h" --include="*.cpp" examples/ pdk/

# A3: shim headers 已删
ls examples/pdk_chat_demo/chat_session.h examples/pdk_chat_demo/cancellation_registry.h 2>/dev/null

# A4: 全量构建
cmake --build build -j$(nproc)

# A5: functional ctest
ctest --test-dir build -j4 --timeout 180

# A7: LoopAgent 符号验证
nm -D build/pdk/loop_agent/libLoopAgent.so 2>/dev/null | grep g_cancellation_registry
# 或
nm build/pdk/loop_agent/libLoopAgent.so 2>/dev/null | grep cancellation_registry

# A8: TSan 门禁
ctest --test-dir build-tsan -R '^test_pdk_chat_session(_recovery|_static_logger)?$' --output-on-failure
```

## 备选方案

| 方案 | 评估 | 决策 |
|---|---|---|
| **A. 继续保留 shim 至 2 Sprint** | 延后债 | ❌ (本期解决) |
| **B. 拆为 2 个 change (PDK 移动 vs includer 迁移)** | 复杂度高 | ❌ (合并) |
| **C. 保留 cancellation_globals.h 在 examples** (loop_agent 继续 include examples) | 跨树耦合 | ❌ (D1) |
| **D. 用 `using namespace hydraforge::pdk` 改名为 `using pdk` 等** | 减小 namespace 名长 | ❌ (不需要, 单字 `using namespace` 足够) |

**最终方案**: 1 change 包含全部迁移 + shim 删除 + cancellation_globals.h 移到 PDK。
