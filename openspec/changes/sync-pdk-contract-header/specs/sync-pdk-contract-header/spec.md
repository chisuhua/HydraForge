# Sync PDK Contract Header Spec

## ADDED Requirements

### Requirement: sync-pdk.sh 同步 contract 依赖头

`scripts/sync-pdk.sh` MUST 将 `include/agenticdsl/pdk/` 下头文件依赖的全部 `agenticdsl/contract/` 头文件同步到 standalone 仓库的 `include/agenticdsl/contract/` 路径。依赖集合由 `PDK_CONTRACT_DEPS` 清单定义（当前: itool_registry.h + iinteraction_bus.h + timer_service.h + ilogger.h，按 chat_session.h include 枚举）。

#### Scenario: contract 头被复制
- **WHEN** 运行 `PDK_SYNC_DRY_RUN=1 scripts/sync-pdk.sh`
- **THEN** 输出中列出 itool_registry.h
- **AND** dry-run 工作目录 `include/agenticdsl/contract/itool_registry.h` 存在

#### Scenario: 路径一致
- **WHEN** standalone 仓库的 chat_session.h 执行 `#include <agenticdsl/contract/itool_registry.h>`
- **THEN** 头文件可解析（路径与 monorepo 一致，CMake target_include_directories 指向 include/）

### Requirement: DRY_RUN 清单断言

`PDK_SYNC_DRY_RUN=1` 模式 MUST 输出 contract 头清单（每个被同步的 contract 头一行），供人工核对完整性。测试 MUST 断言清单包含 itool_registry.h。

#### Scenario: dry-run 输出清单
- **WHEN** `PDK_SYNC_DRY_RUN=1 scripts/sync-pdk.sh` 执行成功
- **THEN** 退出码为 0
- **AND** 输出含 "itool_registry.h"

### Requirement: 不推送 standalone 仓库

sync-pdk.sh 的 contract 同步逻辑 MUST NOT 在 DRY_RUN 模式下执行 git push。真实 push 仅在非 DRY_RUN 且存在 SSH 凭据时执行（保持现有行为）。

#### Scenario: dry-run 无 push
- **WHEN** `PDK_SYNC_DRY_RUN=1 scripts/sync-pdk.sh`
- **THEN** 无 git push 执行（无远程写操作）
- **AND** 本地临时工作目录可检查
