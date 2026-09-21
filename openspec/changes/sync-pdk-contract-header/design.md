# Sync PDK Contract Header — Design

## Context

C4 DB1 (commit f7f0fe3) 扩展了 `include/agenticdsl/contract/itool_registry.h`（+`unregister_tool_function` 纯虚）。该头是 PDK 消费面（chat_session.h:104 include 它）。ADR-0021 §7 Dual-Repo Policy 要求 monorepo 变更同步到 standalone `hydraforge-pdk` 仓库。但 sync-pdk.sh 现状：

- 只同步 `include/agenticdsl/pdk/` 下的 `tool_macros.h` + `safe_exec.h` + `agent_macros.h` + `pdk.h` (及 agent_loops/ 等)
- **无 contract 目录同步** — grep 确认零 `contract` cp 逻辑
- standalone 仓库若被外部消费，`chat_session.h` include `<agenticdsl/contract/itool_registry.h>` 将编译失败（头不存在）

Oracle bg_5db13fe0 确认这是唯一未闭环的 PDK 生态影响。

## Goals / Non-Goals

**Goals**:
- sync-pdk.sh 携带 chat_session.h 依赖的全部 contract 头到 standalone 仓库
- 路径与 monorepo 一致 (`include/agenticdsl/contract/`)，`#include <agenticdsl/contract/...>` 解析
- DRY_RUN 模式可验证 contract 清单完整性

**Non-Goals**:
- 不重构 sync-pdk.sh 整体架构（保持现有 preflight/copy/commit 结构）
- 不引入 git submodule 或 FetchContent 替代 vendored 方案（ADR-0021 已定 Option C）
- 不推送 standalone 仓库（push 需要真实 SSH 凭据，CI/手动验证后由用户触发）

## Decisions

### D1: 依赖清单 = 静态 include 扫描 vs 硬编码清单

**决策**: 硬编码 contract 依赖清单 `PDK_CONTRACT_DEPS`（itool_registry.h + chat_session.h include 的其他 contract 头），不做运行时 include 扫描。

**Rationale**: chat_session.h include 的 contract 头是**有限集合**（itool_registry.h / iinteraction_bus.h / timer_service.h / ilogger.h 等，grep 可枚举）。硬编码清单 + DRY_RUN 输出对比 = 简单可靠。运行时扫描（sed/grep 解析 include）在 bash 中脆弱（条件 include、注释干扰）。

**Alternatives**:
- (a) `grep -oP '#include <agenticdsl/contract/.*>'` 动态扫描 → bash 正则解析 include 脆弱，PR 评审噪声大
- (b) 全量同步 `include/agenticdsl/contract/` 全部头 → 过度（standalone 不需要 iparser/ischeduler 等引擎侧头）

### D2: 路径映射

**决策**: monorepo `include/agenticdsl/contract/itool_registry.h` → standalone `include/agenticdsl/contract/itool_registry.h`（路径一致）。

**Rationale**: `#include <agenticdsl/contract/...>` 依赖路径一致性，CMake target_include_directories 已指向 `include/`。

### D3: 验证 = DRY_RUN 清单断言

**决策**: 新增 `PDK_SYNC_DRY_RUN=1` 下输出 contract 头清单 + standalone 结构树；测试断言清单包含 itool_registry.h。

**Rationale**: 不依赖真实 SSH push（CI 无凭据）。dry-run 输出 + 结构树 = 可验证的契约完整性证据。

## Risks / Trade-offs

- [硬编码清单与 chat_session.h 未来 include 漂移] → 测试断言锁定当前集合 + DRY_RUN 输出人工核对
- [standalone CMakeLists 未 include contract 路径] → 需确认 standalone 的 target_include_directories 已含 `include/`（contract 头在 include/agenticdsl/contract/ 下自动覆盖）
- [push 凭据缺失无法端到端验证] → change 只保证 dry-run + 清单，push 由用户在有凭据环境执行

## Migration Plan

1. 修改 sync-pdk.sh 加 contract 段（清单 + cp + dry-run 输出）
2. `PDK_SYNC_DRY_RUN=1` 运行验证（无需 clone/push）
3. 新增 test（bash 断言: dry-run 输出含 itool_registry.h + 文件存在）
4. 回滚: 单 commit revert

## Open Questions

- (无 — 范围明确，push 凭据为外部依赖)
