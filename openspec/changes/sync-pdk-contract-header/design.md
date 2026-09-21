# Sync PDK Contract Header — Design

## Context

C4 DB1 (commit f7f0fe3) 扩展了 `include/agenticdsl/contract/itool_registry.h`（+`unregister_tool_function` 纯虚）。该头是 PDK 消费面（chat_session.h:104 include 它）。ADR-0021 §7 Dual-Repo Policy 要求 monorepo 变更同步到 standalone `hydraforge-pdk` 仓库。但 sync-pdk.sh 现状（2026-09-21 双 agent 事实核查）：

- 只同步 `include/agenticdsl/pdk/` 下的 `tool_macros.h` + `safe_exec.h` + 2 个生成 stub（agent_macros.h + pdk.h）
- **无 contract 目录同步** — grep 确认零 `contract` cp 逻辑
- **`chat_session.h` 不在同步范围**（grep "chat_session" = 0）——standalone 仓库当前没有 chat_session.h
- **已 ship 的悬空 include**: `tool_macros.h:23-24` include `common/policy/execution_policy.h` + `agenticdsl/tools/schema_generation.h`（非 contract 非 pdk 头）——**standalone 的 tool_macros.h 当前就编译不过**

Oracle bg_5db13fe0 确认 contract 头是"唯一未闭环的 PDK 生态影响"，但审查后范围修正为"预置 contract 消费面"（chat_session.h 同步牵连 DSLEngine 依赖树，超出本 change）。

## Goals / Non-Goals

**Goals**:
- sync-pdk.sh 携带实测的 PDK contract 依赖头到 standalone 仓库
- 路径与 monorepo 一致 (`include/agenticdsl/contract/`)，`#include <agenticdsl/contract/...>` 解析
- DRY_RUN 模式**离线**可验证 contract 清单完整性（不 clone、不 push）

**Non-Goals**:
- **不修 tool_macros.h 的悬空 include**（common/policy/execution_policy.h + schema_generation.h 传递闭包）——登记为独立 follow-up
- **不同步 chat_session.h 本身**（牵连 DSLEngine + SessionManager 依赖树，超出本 change 范围）
- 不重构 sync-pdk.sh 整体架构（保持现有 preflight/copy/commit 结构）
- 不引入 git submodule 或 FetchContent 替代 vendored 方案（ADR-0021 已定 Option C）
- 不推送 standalone 仓库（push 需要真实 SSH 凭据，CI/手动验证后由用户触发）

## Decisions

### D1: 依赖清单 = 实测硬编码清单 + drift-guard 测试（非运行时扫描）

**决策**: 硬编码 contract 依赖清单 `PDK_CONTRACT_DEPS` = **实测 11 个头**（2026-09-21 grep 实测，非假设）:
- chat_session.h 依赖 (6): ilogger.h / iinput_source.h / iinteraction_bus.h / itool_registry.h / resume_token.h / timer_service.h
- cross_cutting 依赖 (5): ievaluator.h / itool_hook_registry.h / iagent_hook_registry.h / iagent_registry.h / i_llm_provider_decorator.h

**Rationale**: 实测全集是可枚举的有限集合。硬编码清单 + **drift-guard 测试**（测试侧 grep 扫描 pdk 头 contract include 集合 ⊆ PDK_CONTRACT_DEPS，不在 sync script 内做动态扫描）——化解 D1 拒绝动态扫描的理由（扫描只在测试里跑，不污染生产脚本）。

**Alternatives**:
- (a) `grep -oP '#include <agenticdsl/contract/.*>'` 动态扫描于生产脚本 → bash 正则解析 include 脆弱，PR 评审噪声大，**否决**
- (b) 全量同步 `include/agenticdsl/contract/` 全部头 → 过度（standalone 不需要 iparser/ischeduler 等引擎侧头），**否决**
- (c) 只同步 4 头（早期版本清单）→ **创建时就是错的**（漏 iinput_source/resume_token + cross_cutting 5 头），Oracle Critical-3.1，已修正

### D2: 路径映射

**决策**: monorepo `include/agenticdsl/contract/itool_registry.h` → standalone `include/agenticdsl/contract/itool_registry.h`（路径一致）。

**Rationale**: `#include <agenticdsl/contract/...>` 依赖路径一致性，CMake target_include_directories 已指向 `include/`。

### D3: 验证 = DRY_RUN 离线化（不 clone）

**决策**: 新增 `PDK_SYNC_DRY_RUN=1` 离线分支——**跳过 `prepare_workdir` 的 git clone**，改用本地临时目录（`mktemp -d`）作为 standalone 替身，执行 contract 复制 + 输出清单；测试断言清单包含 itool_registry.h + 文件存在。

**Rationale**: 现状 `main()` 无条件先走 `prepare_workdir`（git clone SSH），DRY_RUN 只跳过 commit/push——**sandbox/CI 无 SSH 凭据时 dry-run 必失败**（Oracle Critical-3.3，同模式 #8 C2 chmod-000 判例）。离线分支让测试在 CI 可跑。

**Alternatives**:
- (a) 保留 clone 但用 HTTPS + token → CI 仍需 secret 配置，沙箱仍不可跑，**否决**
- (b) 测试直接 grep sync-pdk.sh 源码断言清单 → 不验证实际复制行为，弱，**否决**

## Risks / Trade-offs

- [硬编码清单与 pdk 头未来 include 漂移] → drift-guard 测试（扫描 ⊆ 清单）锁定 + DRY_RUN 输出人工核对
- [standalone CMakeLists 未 include contract 路径] → 需确认 standalone 的 target_include_directories 已含 `include/`（contract 头在 include/agenticdsl/contract/ 下自动覆盖）——tasks 1.2 验证
- [push 凭据缺失无法端到端验证] → change 只保证离线 dry-run + 清单，push 由用户在有凭据环境执行
- [standalone tool_macros.h 悬空 include 未修] → 登记 follow-up（非本 change 范围），DRY_RUN 只验证 contract 头落位不验证编译

## Migration Plan

1. 修改 sync-pdk.sh 加 contract 段（实测 11 头清单 + cp + DRY_RUN 离线分支）
2. `PDK_SYNC_DRY_RUN=1` 离线运行验证（不 clone/push，输出清单 + 工作目录文件存在）
3. 新增 test（bash 断言: DRY_RUN 输出含 itool_registry.h + 文件存在）+ CMake add_test 注册
4. 回滚: 单 commit revert

## Open Questions

- (无 — 范围修正已明确；standalone tool_macros.h 编译性 = 独立 follow-up 跟踪)
