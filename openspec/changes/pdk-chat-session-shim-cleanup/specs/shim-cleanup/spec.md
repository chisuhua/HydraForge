# shim-cleanup Spec Deltas

**Capability**: `shim-cleanup` (新增)
**关联代码**: `pdk/chat_session/{include,src}/` + `examples/pdk_chat_demo/` (consumer) + `pdk/loop_agent/` (cross-tree consumer)
**前置**: chat-session-pdk-lift Change 1 (commit `597796a`) + Change 2 (commit `c9b1a42`) ship
**1-Sprint 兼容期**: 2026-09-11 → 2026-09-15 已到期

## ADDED Requirements

### Requirement: cancellation-globals-in-pdk

`hydraforge::pdk::g_cancellation_registry` 全局 MUST 定义于 `pdk/chat_session/src/cancellation_globals.cpp`，通过 `pdk_chat_session` STATIC 库 PUBLIC 链接接口传播到 `LoopAgent.so` 与 `pdk_chat_demo` 主可执行。`examples/pdk_chat_demo/commands/cancellation_globals.{h,cpp}` MUST NOT 存在（已迁移到 PDK）。

#### Scenario: global symbol 在 PDK namespace

- **WHEN** `nm build/pdk/loop_agent/libLoopAgent.so` | grep cancellation_registry
- **THEN** 含 `hydraforge::pdk::g_cancellation_registry` (BSS weak definition)

#### Scenario: examples cancellation_globals 已删除

- **WHEN** `ls examples/pdk_chat_demo/commands/cancellation_globals.{h,cpp}`
- **THEN** No such file or directory

### Requirement: chat-session-shim-headers-deleted

`examples/pdk_chat_demo/chat_session.h` 与 `examples/pdk_chat_demo/cancellation_registry.h` 两个 1-Sprint 兼容 shim MUST NOT 存在。原别名 `pdk_chat_demo::{ChatSession, CancellationRegistry, QueueKind, AgentConfig, ChatConfig, ...}` MUST 全部解析为 `hydraforge::pdk::*` (迁移到 PDK) 或 examples-app-specific 命名空间（保留 `pdk_chat_demo::`）。

#### Scenario: shim headers 已删

- **WHEN** `ls examples/pdk_chat_demo/chat_session.h examples/pdk_chat_demo/cancellation_registry.h`
- **THEN** No such file or directory

#### Scenario: PDK 迁移类型全限定

- **GIVEN** 所有 examples + pdk/loop_agent consumers
- **WHEN** grep `pdk_chat_demo::ChatSession\|pdk_chat_demo::CancellationRegistry\|pdk_chat_demo::QueueKind\|pdk_chat_demo::AgentConfig\|pdk_chat_demo::ChatConfig\|pdk_chat_demo::SessionConfig\|pdk_chat_demo::PluginConfig\|pdk_chat_demo::InputMessage\|pdk_chat_demo::ChatResult\|pdk_chat_demo::ObservabilityConfig`
- **THEN** 返回 0 行

#### Scenario: examples-app types 保留 pdk_chat_demo namespace

- **GIVEN** examples 仍引用 app-specific 类型如 `DslValidator`/`EventHandler`/`parse_cli_args`/`make_*_command_spec`
- **WHEN** 这些类型被引用
- **THEN** 仍在 `pdk_chat_demo::` namespace 解析（未迁到 PDK）

### Requirement: pdk-loop-agent-no-examples-dependency

`pdk/loop_agent/` MUST NOT 通过 `${PROJECT_SOURCE_DIR}/examples/...` include 路径访问 examples 树代码。所有跨树依赖 MUST 通过 PDK public link 接口（`pdk_chat_session` 等）解决。

#### Scenario: loop_agent CMakeLists 无 examples 路径

- **WHEN** `grep "examples" pdk/loop_agent/CMakeLists.txt`
- **THEN** 返回 0 行

#### Scenario: loop_agent 链接 pdk_chat_session

- **WHEN** 检查 `pdk/loop_agent/CMakeLists.txt` target_link_libraries
- **THEN** 含 `pdk_chat_session`

### Requirement: in-memory-input-source-wip

MUST establish the IInputSource 5-method contract (`read_line`/`has_input`/`at_eof`/`wake`/`close`) in `include/agenticdsl/contract/iinput_source.h` plus StdinInputSource self-pipe+`poll(2)` implementation in `src/common/io/stdin_input_source.{h,cpp}` plus in-memory test double in `tests/test_helpers/in_memory_input_source.h`. (Code change recorded in commit `1c758ae` — chat-session-pdk-lift Change 1 WIP carry-forward.) This is WIP from the original Change 1 work, finalized as part of the shim-cleanup's audit trail. Future changes (chat-session-static-logger-injection, pdk-chat-session-shim-cleanup tests) depend on this contract.

#### Scenario: IInputSource 5-method contract

- **GIVEN** `include/agenticdsl/contract/iinput_source.h`
- **WHEN** grep `virtual.*\b(read_line|has_input|at_eof|wake|close)\b`
- **THEN** 返回 5 个虚方法

#### Scenario: StdinInputSource self-pipe

- **GIVEN** `src/common/io/stdin_input_source.cpp`
- **WHEN** grep `pipe2\|poll(2)`
- **THEN** 含 `pipe2(O_CLOEXEC|O_NONBLOCK)` + `poll([STDIN_FILENO, pipe_read_fd_], ...)`

## REMOVED Requirements

### Requirement: pdk-chat-demo-chat-session-alias-shim

REMOVED in commit `711b00f` (Commit 2 of this change).

`examples/pdk_chat_demo/chat_session.h` (3-line alias shim mapping `hydraforge::pdk::*` → `pdk_chat_demo::*`) was the 1-Sprint compat layer for chat-session-pdk-lift Change 1. The compat period (2026-09-11 → 2026-09-15) has expired; all consumers now use `hydraforge::pdk::` direct references.

#### Scenario: REMOVED shim 不可用

- **WHEN** 编译 `examples/pdk_chat_demo/main.cpp` 引用 `pdk_chat_demo::ChatSession`
- **THEN** 编译失败 `'pdk_chat_demo::ChatSession' was not declared` (because shim is deleted + no alias remains)
- **AND** MUST 使用 `hydraforge::pdk::ChatSession`

### Requirement: pdk-chat-demo-cancellation-registry-alias-shim

REMOVED in commit `711b00f` (Commit 2 of this change).

`examples/pdk_chat_demo/cancellation_registry.h` (global-scope `using hydraforge::pdk::CancellationRegistry;`) was the 1-Sprint compat layer. Same expiry as chat_session.h.

#### Scenario: REMOVED global alias 不可用

- **WHEN** 编译引用全局 `CancellationRegistry`（无 namespace）
- **THEN** 编译失败 (no such type in global namespace)
- **AND** MUST 使用 `hydraforge::pdk::CancellationRegistry`

## UNCHANGED Requirements

`ChatSession`/`CancellationRegistry`/`ResumeToken`/`IInputSource`/`ILogger`/`ITimerService`/`SafeExec` 等 PDK 公开 API 签名不变。`pdk_chat_demo` namespace 内 examples-app-specific 类型（`DslValidator`/`EventHandler`/`parse_cli_args`/`make_*_command_spec`/`ChatConfig::from_json`/`kExitCommand`/`kCommandExitSentinel`）继续保留 — 这些不是 PDK 通用类型，未在本 change 范围。

## 已知偏差（spec 实施期间发现）

`openspec/changes/pdk-chat-session-shim-cleanup/design.md` 的 A2 验收要求"`using namespace pdk_chat_demo;` 0 行"，但实施发现 4 个 tests 同时使用 examples-app 类型 + PDK 类型：

- `test_event_handler_rendering.cpp` (使用 `EventHandler` examples-app)
- `test_dsl_validation.cpp` (使用 `DslValidator` examples-app)
- `test_dsl_validator_yaml.cpp` (使用 `DslValidator` examples-app)
- `test_e2e_mock.cpp` (使用 `EventHandler` examples-app + `ChatSession`/`AgentConfig` PDK)

proposal 的 sed 设计缺陷（`using namespace pdk_chat_demo;` → `using namespace hydraforge::pdk;` 一刀切）破坏了这 4 个文件。修复方式：保留 `pdk_chat_demo::` namespace 与 examples-app 类型共存，**显式添加 `using namespace hydraforge::pdk;` 用于 PDK 类型**。每文件 2 行 using。

`pdk_chat_demo::testing` 子命名空间（`test_chat_session_cancellation.cpp` 用）未迁移，保持原样 — 这是 examples-app testing 命名空间，不来自 shim。

`tests/CMakeLists.txt` 的 `${CMAKE_CURRENT_SOURCE_DIR}/..` include 路径**保留**（T2.9 skipped）— 4 个 test 仍 `#include "commands/..."` 相对路径，需 `/..` 解析。这些命令 headers 仍 in examples（未在 PDK），无需迁移。Optional cleanup，留后续 sprint。
