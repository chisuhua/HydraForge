# Sync PDK Contract Header — Proposal

## Why

C4 `harness-rsi-pilot` 修改了 `include/agenticdsl/contract/itool_registry.h`（新增 `unregister_tool_function` 纯虚方法）。Oracle bg_5db13fe0 审查确认：该头文件是 **PDK 可消费面** —— `include/agenticdsl/pdk/chat_session.h:104` 直接 `#include <agenticdsl/contract/itool_registry.h>`。按 ADR-0021 §7 Dual-Repo Policy，monorepo 的 contract 头变更必须同步到独立 `hydraforge-pdk` 发布仓库。

**事实核查（2026-09-21 双 agent 审查）**:
- `scripts/sync-pdk.sh` **只同步 `include/agenticdsl/pdk/` 下的 2 个显式头**（tool_macros.h + safe_exec.h）+ 2 个生成 stub（agent_macros.h + pdk.h），**完全没有 contract 目录同步逻辑**（grep 零命中）
- **`chat_session.h` 本身不在同步范围**（sync-pdk.sh grep "chat_session" = 0）——"standalone 仓库的 chat_session.h 引用断裂"叙述**不准确**，实际是: standalone 仓库当前**根本没有** chat_session.h
- **已存在悬空 include**: 已同步的 `tool_macros.h:23-24` include `common/policy/execution_policy.h` + `agenticdsl/tools/schema_generation.h`（非 contract 非 pdk 头）——**standalone 的 tool_macros.h 当前就编译不过**，生态断裂比"contract 头缺失"更早、更深
- **准确动机**: 为未来同步 chat_session.h / agent_loops/ / cross_cutting/ 预置 contract 消费面 + 修复已 ship 的悬空 include（ADR-0021 §7 的契约完整性要求）

## What Changes

- **`scripts/sync-pdk.sh` 扩展**: 新增 contract 头同步段——将实测的 contract 依赖清单（见 design D1）复制到 standalone 仓库 `include/agenticdsl/contract/`
- **实测依赖清单**: 枚举 `include/agenticdsl/pdk/` 下全部头文件的 `<agenticdsl/contract/...>` 引用（**实测 11 个**：iinteraction_bus / ilogger / iinput_source / itool_registry / resume_token / timer_service + cross_cutting 5 个），非假设
- **standalone 仓库结构**: contract 头放到 `include/agenticdsl/contract/`（与 monorepo 路径一致，`#include <agenticdsl/contract/itool_registry.h>` 才能解析）
- **DRY_RUN 离线化**: `PDK_SYNC_DRY_RUN=1` 下**跳过 git clone**（sandbox/CI 无 SSH 凭据），改用本地临时目录验证 contract 清单

## Capabilities

### New Capabilities
- `sync-pdk-contract-header`: PDK sync 脚本 contract 头同步 + 依赖清单 + DRY_RUN 离线验证

### Modified Capabilities
- (none — 这是构建/发布基础设施变更，非运行时行为)

## Impact

- `scripts/sync-pdk.sh` — 新增 contract 同步段（实测清单 + cp + DRY_RUN 离线分支）
- `include/agenticdsl/pdk/` — 无代码变更（消费面已 include contract 头）
- 外部 hydraforge-pdk 仓库 — contract 头首次携带（DRY_RUN 验证后由有凭据环境推送）
- 测试: 新增 sync 脚本 DRY_RUN 离线断言（contract 清单含 itool_registry.h）
- **Non-Goals**: 本 change **不**修复 tool_macros.h 的悬空 include（common/policy + schema_generation 传递闭包，登记为 follow-up）；**不**同步 chat_session.h 本身（牵连 DSLEngine 依赖树，超出本 change 范围）
