## Why

`chat-session-pdk-lift Change 1` (commit `597796a`) 把 `ChatSession` 与 `CancellationRegistry` 提升到 PDK `namespace hydraforge::pdk`（设计 §D6.1）。为避免 16+ 个 includer 一次断链，lift 同步引入 **4 处 1-Sprint 兼容 shim**：

| Shim | 文件 | 内容 |
|---|---|---|
| 1 | `examples/pdk_chat_demo/chat_session.h` | `#include <agenticdsl/pdk/chat_session.h>` + `using hydraforge::pdk::{AgentConfig, ChatConfig, ChatResult, ChatSession, InputMessage, ObservabilityConfig, PluginConfig, QueueKind, SessionConfig};` 内部 namespace |
| 2 | `examples/pdk_chat_demo/cancellation_registry.h` | `#include <agenticdsl/pdk/cancellation_registry.h>` + 全局 `using hydraforge::pdk::CancellationRegistry;` |
| 3 | `examples/pdk_chat_demo/commands/command_globals.h` | `using ChatSession = hydraforge::pdk::ChatSession;`（前置声明改为 alias） |
| 4 | `examples/pdk_chat_demo/commands/cancellation_globals.h` | `using CancellationRegistry = hydraforge::pdk::CancellationRegistry;`（前置声明改为 alias）|

**到期条件**（`docs/superpowers/specs/2026-09-11-chat-session-pdk-lift-design.md` §6.1）已满足：
- ✅ Change 1 (commit `597796a`) 全部 ship + 232/232 ctest PASS
- ✅ Change 2 (commit `c9b1a42`) 全部 ship + 234/234 ctest PASS + TSan 零新增 race
- ✅ 实战 1 Sprint (2026-09-11 → 2026-09-14) shim 无生产问题

**继续保留 shim 的代价**：
1. **跨树耦合隐藏**: `pdk/loop_agent/src/pdk_entry.cpp` 通过 `${PROJECT_SOURCE_DIR}/examples/pdk_chat_demo` include 路径访问 shim — 违反 "PDK 不应反向依赖 examples/"（ADR-0021 §3.5 精神）
2. **API 双重名空间**: 同一类存在 `pdk_chat_demo::ChatSession` 和 `hydraforge::pdk::ChatSession` 两个名字（一个 alias 一个真名）— IDE 跳转/重命名工具会随机选 alias
3. **技术债累积**: 后续添加方法/类型时必须同步更新两套名字，错误倾向

**Oracle 优先级**: `MEDIUM`（不修不阻塞功能，但累积技术债 + 隐藏架构问题）。

## What Changes

### 1. 删除 2 个 shim 头文件
- `git rm examples/pdk_chat_demo/chat_session.h`
- `git rm examples/pdk_chat_demo/cancellation_registry.h`

### 2. 移动 `cancellation_globals.h` 到 PDK（消除跨树耦合）
- `git mv examples/pdk_chat_demo/commands/cancellation_globals.h pdk/chat_session/include/agenticdsl/pdk/cancellation_globals.h`
- 删除原 `using CancellationRegistry = ...;` (改用 `agenticdsl::pdk::CancellationRegistry` 直接 include)
- `pdk/chat_session/CMakeLists.txt` 添加头文件导出（公共 include 目录 + INTERFACE 库传播）

### 3. 18 个 includer 迁移

#### A. `examples/pdk_chat_demo/main.cpp`
- 替换 `#include "chat_session.h"` → `#include <agenticdsl/pdk/chat_session.h>`
- 替换 `#include "commands/cancellation_globals.h"` → `#include <agenticdsl/pdk/cancellation_globals.h>`
- 替换 `pdk_chat_demo::ChatSession` → `hydraforge::pdk::ChatSession` (1 处)
- 替换 `pdk_chat_demo::CancellationRegistry` → `hydraforge::pdk::CancellationRegistry` (1 处)
- 替换 `pdk_chat_demo::g_cancellation_registry` → `hydraforge::pdk::g_cancellation_registry` (2 处) — **注**: `g_cancellation_registry` 当前位置在 `examples/.../commands/cancellation_globals.cpp`，迁移后属 `hydraforge::pdk` 命名空间

#### B. `examples/pdk_chat_demo/commands/{cancel,model}_command.cpp` (2 个 commands)
- 替换 `#include "chat_session.h"` → `#include <agenticdsl/pdk/chat_session.h>`
- 替换 `#include "commands/cancellation_globals.h"` → `#include <agenticdsl/pdk/cancellation_globals.h>`

#### C. `examples/pdk_chat_demo/commands/command_globals.{h,cpp}` (1 个 file pair)
- `command_globals.h` 替换 `using ChatSession = hydraforge::pdk::ChatSession;` → `using ChatSession = ::hydraforge::pdk::ChatSession;` (前导 `::` 显式全局解析)
- `command_globals.cpp` 替换 `ChatSession*` → `::hydraforge::pdk::ChatSession*` (或经 using)

#### D. 14 个 examples tests (test_*.cpp in examples/pdk_chat_demo/tests/)
- 替换 `#include "chat_session.h"` → `#include <agenticdsl/pdk/chat_session.h>`
- 替换 `#include "cancellation_registry.h"` → `#include <agenticdsl/pdk/cancellation_registry.h>`
- 替换 `using namespace pdk_chat_demo;` → `using namespace hydraforge::pdk;` (13 个文件)
- 替换 `ChatSession session(...);` → 不变 (alias 解析为新 namespace)
- 替换 `test_cancellation_registry.cpp` 内的 `CancellationRegistry reg;` → `hydraforge::pdk::CancellationRegistry reg;` (仅此文件未 `using namespace`)

#### E. `pdk/loop_agent/src/pdk_entry.cpp`
- 替换 `#include "cancellation_registry.h"` → `#include <agenticdsl/pdk/cancellation_registry.h>`
- 替换 `#include "commands/cancellation_globals.h"` → `#include <agenticdsl/pdk/cancellation_globals.h>`
- 替换 `pdk_chat_demo::g_cancellation_registry` → `hydraforge::pdk::g_cancellation_registry` (3 处)

#### F. `examples/pdk_chat_demo/tests/CMakeLists.txt`
- 移除 `target_include_directories(... ${CMAKE_CURRENT_SOURCE_DIR}/..)` 中对 examples 目录的依赖（`#include "chat_session.h"` 全部替换为 `<agenticdsl/pdk/chat_session.h>` 后不再需要相对路径）

### 4. 不修改的（保持稳定）
- `pdk/chat_session/src/*.cpp` (PDK 实现已 ship, 不动)
- `include/agenticdsl/pdk/{chat_session, cancellation_registry}.h` (PDK 头不动)
- `tests/test_pdk_chat_session*.cpp` (core 树测试已在 Change 1/2 ship, 使用 `using namespace hydraforge::pdk`)

### Non-goals

- **不** 改 `pdk/loop_agent` 内部 `g_loop_registry` 逻辑（与本 change 无关）
- **不** 动 `pdk/chat_session` 任何实现文件
- **不** 改 `pdk_chat_demo` namespace 内其它符号 (`pdk_chat_demo::ChatConfig::from_json` 仍存在 — 因为 `ChatConfig` 类已迁到 PDK 命名空间, 此引用变成 namespace 不存在)
  - 等等, 这是个问题: 现有 `using namespace pdk_chat_demo;` 在 tests 中, 实际使用 `ChatConfig`/`AgentConfig` 等的类型名。shim 删除后, 这些类型名**不再** in `pdk_chat_demo`. Tests 需 `using namespace hydraforge::pdk` 才能使用 `ChatConfig` 等.
  - **修正**: D 节 14 tests 的 `using namespace pdk_chat_demo;` → `using namespace hydraforge::pdk;` 包含 `ChatConfig` 等所有类型名

## 影响面

| 类型 | 范围 |
|---|---|
| 删除文件 | 2 (`chat_session.h` shim + `cancellation_registry.h` shim) |
| 移动文件 | 1 (`commands/cancellation_globals.h` → pdk/chat_session/include/...) |
| 新文件 | 1 (`pdk/chat_session/include/agenticdsl/pdk/cancellation_globals.h` 在原移动) |
| 修改文件 | ~22 (main + 2 commands + 2 globals + 14 tests + 1 loop_agent + 2 CMakeLists) |
| 公开 API | **`pdk_chat_demo::ChatSession` 等 9 个别名消失** (硬性 breaking) — 但已在 change 文档明确 |
| ABI | `g_cancellation_registry` 全局变量**位置**从 examples 迁到 pdk (符号命名空间变化) — 影响链接, 但仅 main.cpp / loop_agent 引用 |
| 现有测试 | **全部**需修改 (using namespace 替换) |

## 验收

- **A1** `grep -rn "pdk_chat_demo::ChatSession\|pdk_chat_demo::CancellationRegistry" --include="*.h" --include="*.cpp" examples/ pdk/loop_agent/ tests/ src/` **返回 0 行** (除 changelog 注释)
- **A2** `grep -rn "using namespace pdk_chat_demo" --include="*.h" --include="*.cpp" examples/ pdk/` **返回 0 行**
- **A3** `ls examples/pdk_chat_demo/chat_session.h examples/pdk_chat_demo/cancellation_registry.h` **文件不存在** (2>/dev/null 静默)
- **A4** `cmake --build build -j$(nproc)` **0 error** (全 targets: agenticdsl_core, agenticdsl_modules_*, pdk_chat_session, pdk_chat_demo, pdk_chat_demo_obj, LoopAgent.so, examples tests)
- **A5** `ctest --test-dir build -j4 --timeout 180` **234 tests, 233 PASS** (允许 test_skill_interpreter pre-existing flaky)
- **A6** `ctest --test-dir build -R chat_session` **新 baseline** (新加入的 test_chat_session* 仍 PASS)
- **A7** LoopAgent.so 链接 `hydraforge::pdk::g_cancellation_registry` 成功 (`nm` 验证符号存在)
- **A8** TSan gate: `ctest -R '^test_pdk_chat_session(_recovery|_static_logger)?$' --test-dir build-tsan` 3/3 PASS 0 warnings

## 风险

| 风险 | 等级 | 缓解 |
|---|---|---|
| `g_cancellation_registry` 符号从 examples 迁到 pdk → 旧二进制 (.so) 不再能找到符号 | 低 | LoopAgent.so 由 CMake 重新链接; main 同一 process 启动期 set, 符号可见 |
| 8 个 commands 中 `pdk_chat_demo::CommandRegistry` 等其它类型不存在 → 这些不属于 shim 范围, 仍 in `pdk_chat_demo` | 低 | commands 命名空间用 `pdk_chat_demo::` 是设计意图, 不动 |
| 14 个 tests 同时修改 using namespace → 编译失败风险 | 中 | 14 个文件**同样改动** — 用 `sed` 一次性 batch 替换, 然后逐个验证编译 |
| 某 test 用 `pdk_chat_demo::Xxx` 限定名 (Xxx 非 shim 提供) → 编译失败 | 中 | grep A1 + A2 范围外 (shim 类型) 的 `pdk_chat_demo::` 引用, 单独评估 |
