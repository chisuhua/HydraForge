# Sync PDK Contract Header Spec

## ADDED Requirements

### Requirement: sync-pdk.sh 同步 contract 依赖头

`scripts/sync-pdk.sh` MUST 将 `PDK_CONTRACT_DEPS` 清单（**实测 11 个头**，2026-09-21 grep 枚举）中的 `agenticdsl/contract/` 头文件同步到 standalone 仓库的 `include/agenticdsl/contract/` 路径。

`PDK_CONTRACT_DEPS` 清单（当前实测值）:
- chat_session.h 依赖 (6): ilogger.h / iinput_source.h / iinteraction_bus.h / itool_registry.h / resume_token.h / timer_service.h
- cross_cutting 依赖 (5): ievaluator.h / itool_hook_registry.h / iagent_hook_registry.h / iagent_registry.h / i_llm_provider_decorator.h

#### Scenario: contract 头被复制
- **WHEN** `PDK_SYNC_DRY_RUN=1` 离线运行 `scripts/sync-pdk.sh`（跳过 git clone，用本地临时目录）
- **THEN** 输出中列出的 contract 清单含 itool_registry.h
- **AND** 临时工作目录 `include/agenticdsl/contract/itool_registry.h` 存在
- **AND** 清单覆盖全部 11 头（drift-guard: pdk 头实际 contract include 集合 ⊆ PDK_CONTRACT_DEPS）

#### Scenario: DRY_RUN 离线（不 clone）
- **WHEN** 在无 SSH 凭据/无网络的 sandbox/CI 环境运行 `PDK_SYNC_DRY_RUN=1 scripts/sync-pdk.sh`
- **THEN** 退出码为 0（不因 git clone 失败而失败）
- **AND** contract 复制 + 清单输出正常完成

### Requirement: DRY_RUN 清单断言

`PDK_SYNC_DRY_RUN=1` 离线模式 MUST 输出 contract 头清单（每个被同步的 contract 头一行），供人工核对完整性。测试 MUST 断言清单包含 itool_registry.h。

#### Scenario: dry-run 输出清单
- **WHEN** `PDK_SYNC_DRY_RUN=1 scripts/sync-pdk.sh` 离线执行成功
- **THEN** 退出码为 0
- **AND** 输出含 "itool_registry.h"

#### Scenario: drift-guard 锁定清单
- **WHEN** `include/agenticdsl/pdk/` 下任一头文件新增 `<agenticdsl/contract/...>` include 且不在 PDK_CONTRACT_DEPS
- **THEN** drift-guard 测试 FAIL（提醒开发者更新清单）

### Requirement: 不推送 standalone 仓库

sync-pdk.sh 的 contract 同步逻辑 MUST NOT 在 DRY_RUN 模式下执行 git clone / commit / push。真实 clone+commit+push 仅在非 DRY_RUN 且存在 SSH 凭据时执行（保持现有行为）。

#### Scenario: dry-run 无 push
- **WHEN** `PDK_SYNC_DRY_RUN=1 scripts/sync-pdk.sh`
- **THEN** 无 git clone / commit / push 执行（无网络写操作）
- **AND** 本地临时工作目录可检查（mktemp -d 替身）