# P0 改进提案优先级排序决策 (2026-10-14)

**生成**: 2026-10-14 via 主会话 (用户原话: "请对未完成或部分完成的改进进行优先级排序，列出优先级高的提案，然后对这些提案进行深入分析，并再确认最高优先级的提案和如何处理这些提案")
**决策人**: 用户（Single-Dev）
**会话**: ses_ec7b0fe3fffe8g9GCNiGs2a3Zw
**最终验证**: 2026-10-14 via rdd-arch Phase 1 setup

## 0. ⚠️ 重要发现 (本会话推进时)

用户在 2026-10-14 决策启动 Phase A 后，本会话推进中通过 `rdd-arch Phase 1 setup` + git log + openspec/changes/archive/ 验证发现：

**Phase A 3 个提案已全部 ship**（在用户的决策日期前数周完成）：

| 提案 | 实际 ship 日期 | Archive 路径 | Ship 证据 |
|------|----------------|-------------|---------|
| fix-tool-registry-signal-handler-shutdown | 2026-08-08 | `openspec/changes/archive/2026-08-08-fix-tool-registry-signal-handler-shutdown/` | `commit 29a7a06` (route SIGINT/SIGTERM via atomic flag) + `commit 4a0c994` (archive) + `test_signal_shutdown` 9/9 PASS |
| fix-loop-agent-bypass | 2026-08-03 | `openspec/changes/archive/2026-08-03-fix-loop-agent-bypass/` | `commit a1ef463` (ship) + `commit 5ab2174` (archive) |
| adr-0069-tool-coordinator-hooks | 2026-08-04 | `openspec/changes/archive/2026-08-04-adr-0069-tool-coordinator-hooks/` | `merge commit 386fac9` |

**`.rddf/improvements/fix-tool-registry-signal-handler-shutdown.md` 等 3 文件仍存在于目录**, 但对应 OpenSpec change 已 archived。这是 AGENTS.md Pattern #10 hygiene 的"已 ship 但未清理"实例 - 文件保留作为历史 traceability, 但本提案状态应标 ✅。

**主要代码证据**:
- `examples/pdk_chat_demo/main.cpp:70-88` 已实施 atomic flag + signal_handler 只 set flag，禁止 std::exit() (commit `29a7a06`)
- `examples/pdk_chat_demo/tests/test_signal_shutdown.cpp` 9/9 assertions PASS

**`.rddf/improvements/` 文件保留决策**: 保留作为历史 traceability (per AGENTS.md Pattern #10 + 用户决策 3 "暂不处理，留作 .rddf/improvements/ 历史")。

因此本登记文件的"Phase A 实施路线图"实际已被**过去 2 个月前**完成, 本次决策登记仅记录用户决策意图与事后追溯.



## 1. 当前状态盘点

`.rddf/improvements/` 共 **74 个改进提案**：

- ✅ DONE: 24 (32%)
- 🟡 PARTIAL: 10 (14%)
- 🔴 OPEN: 33 (45%)
- ⏸ GATED: 6 (8%, Phase 7a 不启动)
- ⛔ DESCOPED: 1 (1%, Phase 7c descoped)

**核心结论**: 50 个 (68%) 仍需工作。

## 2. 最高优先级排名（5 维评分，满分 10）

| 排名 | 提案 | 评分 | 类型 |
|------|------|------|------|
| 1 | fix-tool-registry-signal-handler-shutdown.md | 9.5 | 🔴 真生产 SIGSEGV |
| 2 | fix-loop-agent-bypass.md | 9.0 | 🔴 Dead code 设计与代码脱节 |
| 3 | harness-mutation-commit-real-llm.md | 8.5 | 🔴 真实 LLM 闭环盲点 |
| 4 | adr-0069-tool-coordinator-hooks.md | 8.0 | 🟡 Partial ADR 收口 |
| 5 | adr-0070-declare-command.md | 8.0 | 🟡 Partial ADR 收口 |
| 6 | adr-0068-event-emission-contract.md | 7.5 | 🟡 emission 代码路径补全 |
| 7 | parser-string-template-args.md | 7.5 | 第 3 潜伏 gap |
| 8 | chat-async-io-cancellation-chain.md | 7.5 | Phase B stop_token 链路 |

## 3. 用户决策 (per multi-choice)

### 决策 1: 整体策略 → **A 选项**
> 立即启动 Phase A 方案 — P0-1 (signal handler) + P0-2 (loop bypass) 串行 ship

### 决策 2: Ship 模式 → **单 atomic commit + 24h cooling-off**
> per AGENTS.md default, 不走 HARD pause override, 标准治理

### 决策 3: PARTIAL 处理 → **暂不处理, 留作 .rddf/improvements/ 历史**
> 不增加额外治理层级 (用户选 3-a 不增加 closure 跟踪表/不 absorb 进路线图)

## 4. 实施路线图 (3 阶段)

### Phase A — 立即启动 (本周)

| 序号 | OpenSpec Change | 工时 | 依赖 |
|------|---------------|------|------|
| **A1** | fix-tool-registry-signal-handler-shutdown | 4-8h | 无前置 |
| **A2** | fix-loop-agent-bypass | 8-12h | 无前置 (与 A1 串行) |
| **A3** | adr-0069 ADR 翻牌 sub-task (1 test fix) | 4-6h | 无前置 |

**Phase A ship gate**:
- 3 个 OpenSpec changes 顺序启动, 单 atomic commit + 24h cooling-off
- 全 ship 后: adr-0068 loop.* 主题真实发射解锁, chat-async-io Phase B 测试可用
- drop_ratio = 0% (mock-identical, 仅修复不重构)

**Phase A cooling-off 时序**:
- T+0:    A1 立项 (rdd-arch → rdd-planner → rdd-builder P0+P1 → P2)
- T+24h:  A1 ship + A2 立项 (按 A2 cooling-off 时序 = A1 ship 时)
- T+48h:  A2 ship + A3 立项
- T+72h:  A3 ship + 所有 Phase A 闭环

### Phase B — Phase A 期间并行立项 (下周)

| 序号 | OpenSpec Change | 工时 | 备注 |
|------|---------------|------|------|
| B1 | chat-async-io-queue-infra (Phase A) | 4-6h | 无前置 (与 A 并行 ship gate) |
| B2 | context-compactor | 4-8h | 前置 100% ship, 实施门槛最低 |
| B3 | parser-string-template-args | 1-2h | 1 文件扩展 + regression test |

### Phase C — 真正立项触发 (Phase A+B 完成)

| 序号 | OpenSpec Change | 工时 | 备注 |
|------|---------------|------|------|
| C1 | harness-mutation-commit-real-llm | 1-2 天 | 🔴 P0 真实 LLM 闭环补全 |
| C2 | chat-async-io-cancellation-chain (Phase B) | 2-3 天 | 7 步 wiring — 必须 ship 全链路 |
| C3 | adr-0070 4 outstanding sub-tasks | 1-2 天 | 分 4 commits ship, 转 ✅ Approved |

## 5. Pending 立项触发条件 (C1 / harness-mutation-commit-real-llm)

任一满足即升级 P0 → 立即立项:
1. 用户要求 "Harness 真实 LLM 端到端" 验证
2. 自进化 v1.5 9 段闭环 §R3 元指标需要实证
3. Wave 3 Phase 2 D4 (LoRA 训练管线) 启动需要 harness 真实 LLM 闭环数据

## 6. PARTIAL 治理豁免 (per 用户决策)

10 个 PARTIAL 提案**不**额外治理 (不增 status log, 不 absorb 进路线图):

- chat-streaming-slash-tui.md
- fork_join_dsl_migration.md (Future Change Tracker 维持)
- from-roadmap-phase-6c-execution-envbackend.md
- h-d-m-transition-guard.md
- l2-release-metrics-real-llm-drop-ratio.md
- pdk-chat-demo-evolution-reference-example.md
- pdk-chat-demo-followups.md
- session-tree-commands.md
- session-tree-tui.md
- wave-3-finetune-base-model.md

**理由** (用户决策 3): 不增加额外治理负担, 保留 .rddf/improvements/ 历史 traceability。

## 7. GATED/DESCOPED 处理 (无变化)

6 GATED + 1 DESCOPED = 7 提案**不**进 Phase A-C, 维持现状:

- 6 GATED: Phase 7a 不启动 (per 2026-09-02 评估, 3/6 conditions not met)
- 1 DESCOPED: Phase 7c descoped (per 2026-09-02 决议)

## 8. 后续 rdd-builder 路径

当用户或后续 session 启动 rdd-arch for 上述 P0 提案:

1. `rdd-arch` ADR 起草 (仅 #4-#6 需新 ADR, #1-#3 已 ADR-0069/0068/0069 关联)
2. `rdd-planner` improvement review approve (本会话已完成部分分析)
3. `rdd-builder P0`: OpenSpec change `openspec/changes/2026-XX-<name>/` 创建 (6 文件完整性)
4. `rdd-builder P1`: 5-segment tasks.md TDD 5-step
5. `rdd-builder P2`: worktree + execute
6. `rdd-builder P2.5`: Oracle post-impl SHIP-with-fixes review (per AGENTS.md Pattern #4)
7. `rdd-builder P3`: archive + verifier (4-file integrity per Day-5 lesson)

## 9. 24h Cooling-Off 记录

per AGENTS.md default + 用户决策 2:

| Change | T 立 | T ship | T+24h 触发 |
|--------|------|--------|-----------|
| A1 (signal-handler-shutdown) | T+0 | T+24h | A2 立项时 |
| A2 (loop-bypass) | T+24h | T+48h | A3 立项时 |
| A3 (adr-0069 test fix) | T+48h | T+72h | Phase A 闭环 |

注: 用户未启动立项工作, 本登记仅记录决策意图。
