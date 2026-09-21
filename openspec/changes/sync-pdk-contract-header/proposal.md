# Sync PDK Contract Header — Proposal

## Why

C4 `harness-rsi-pilot` 修改了 `include/agenticdsl/contract/itool_registry.h`（新增 `unregister_tool_function` 纯虚方法）。Oracle bg_5db13fe0 审查确认：该头文件是 **PDK 可消费面** —— `include/agenticdsl/pdk/chat_session.h:104` 直接 `#include <agenticdsl/contract/itool_registry.h>`。按 ADR-0021 §7 Dual-Repo Policy，monorepo 的 contract 头变更必须同步到独立 `hydraforge-pdk` 发布仓库。但 `scripts/sync-pdk.sh` **只同步 `include/agenticdsl/pdk/` 下的头文件，完全没有 contract 目录同步逻辑**——standalone 仓库的 chat_session.h 引用 contract 头，但 sync 脚本不携带它。这是已验证的生态闭环断裂（Oracle 确认为"唯一未闭环的 PDK 生态影响"）。

## What Changes

- **`scripts/sync-pdk.sh` 扩展**: 新增 contract 头同步逻辑——将 `include/agenticdsl/contract/itool_registry.h`（以及 chat_session.h 依赖的其他 contract 头）复制到 standalone 仓库对应路径
- **依赖清单驱动**: 扫描 `include/agenticdsl/pdk/` 下所有头文件的 `#include <agenticdsl/contract/...>` 引用，构建 contract 依赖集合，全量同步
- **standalone 仓库结构**: contract 头放到 `include/agenticdsl/contract/`（与 monorepo 路径一致，`#include <agenticdsl/contract/itool_registry.h>` 才能解析）
- **DRY_RUN 验证**: `PDK_SYNC_DRY_RUN=1` 下输出 contract 头清单，人工核对

## Capabilities

### New Capabilities
- `sync-pdk-contract-header`: PDK sync 脚本 contract 头同步 + 依赖扫描 + 验证

### Modified Capabilities
- (none — 这是构建/发布基础设施变更，非运行时行为)

## Impact

- `scripts/sync-pdk.sh` — 新增 contract 同步段（依赖扫描 + cp）
- `pdk/` 或 `include/agenticdsl/pdk/` — 无代码变更（消费面已 include contract 头）
- 外部 hydraforge-pdk 仓库 — contract 头首次携带（需 `PDK_SYNC_DRY_RUN=1` 验证后推送）
- 测试: 新增 sync 脚本 dry-run 断言（contract 头清单包含 itool_registry.h）
