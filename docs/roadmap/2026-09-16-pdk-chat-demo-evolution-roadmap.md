# Master Plan: pdk-chat-demo Evolution & Self-Evolution Agent Framework

> **驱动诊断**: pdk_chat_demo 跑真实 LLM 模式返回 0 步 + 空 Assistant
> **驱动愿景**: 在 HydraForge "DSL 执行引擎" 核心使命内搭建 Harness-RSI 闭环骨架
> **覆盖**: Sprint 34-36 + Pre-Wave3 收口 4-Gate + Wave 3 Phase 1 Pilot (~5-6 周)
> **生成日期**: 2026-09-16
> **最后验证**: 2026-09-23（**Pre-Wave3 4-Gate 收口门禁全部 ✅ SHIPPED 2026-09-22** + **Wave 3 Phase 1 `finetune-base-model` pilot ✅ SHIPPED 2026-09-23** + 4 merge commits `9709317` (G1) + `dc12a17` (G2) + `a196a09` (G3) + `fb2769f` (G4) + `f0a5c4b` (Wave 3) + 9 atomic commits + 5 Oracle review sessions (bg_8237a316 G1 + bg_ebfe1c25 G2 + bg_ef5a0ca4 G3 + bg_e4eec567 G4 + bg_7fe026cc Wave 3) + 24h cooling-off override by user 2026-09-22 (Wave 3 启动链式合规 AC-12) + ADR-0078 ✅ Approved (2026-09-23 Wave 3 Pilot 激活) + ctest 252 → 211 post Wave 3 baseline (build 路径实测) + 7 changes archived in window 09-21 → 09-23 (C4 + D8 + G1/G2/G3/G4 + Wave 3) + §一.2 active 9 → 5)
> **作者**: Architecture Working Group + Oracle 评审 (`task_id=ses_f55f307f6ffeRJ9SIny8iUbZ8Y`)
> **状态**: 🟢 **执行中 Master Plan** (C0+C1+P0+F1+C2+C3+walk-ancestors+D8+C4 全部 ✅ SHIPPED; Pre-Wave3 4-Gate 收口门禁 全部 ✅ SHIPPED 2026-09-22; Wave 3 Phase 1 Pilot ✅ SHIPPED 2026-09-23; 24h Wave 3 cooling-off 起点 = `f0a5c4b` merge, 满点 = 2026-09-24 05:33Z; Wave 3 Phase 2 (D4-D7) 待 cooling-off 满后独立立项)

---

## ⚠️ Master Plan 更新日志 (2026-09-17)

### 追加 #1: Wave 2 P1 (intent classification + loop type routing) 补登记

**之前状态**: 仅在 `openspec/changes/archive/2026-09-17-fix-dsl-call-pause-autonomous-mode/proposal.md` L182 提及"Wave 2 P1 (intent 分类 + loop_type 路由) — 那是独立 change"，**主计划未登记**。

**Metis + Oracle 调研结论** (sessions `ses_f5084d7bfffeq1Yt5QVVtaIfix` + `ses_f5084614dffesPP0dEHgJhc4LM`):
- P1 真实存在，但**完全可通过纯 DSL 图节点实现**，不需要新 C++ 类
- 实施路径: 新建 `lib/loop/intent_classify.agent.md`（~30 行 YAML subgraph）+ `switch` 节点 + `tool_call loop/run_subgraph` 桥接
- 估时修订: 3-5 天 → **2-3 天**（DSL 实现比 C++ 实现更轻量）

**Worker Pool Routing 不需要单独决策** (Oracle `ses_f505f99fdffefgE5Q9oFA2t2AD`):
- CognitiveWorker 生产零使用（仅 tests），不阻塞 P1
- DomainWorkerPool 唯一生产消费者是 C++ ForkJoinLoop，但 chat 路径走 DSL TopoScheduler Taskflow，**不经 DomainWorkerPool**
- P1 intent schema 保持 4 字段（`intent_type/complexity/suggested_loop/requires_subgraph`），**不要加 `worker_pool` 字段**

### 追加 #2: 完整 DAG 动态组合流程调研完成 (2026-09-17)

Oracle session `ses_f4fd88215ffeUWSe2StAWtFPSQ` (20m 36s) 调研完成. **新发现 2 个 Latent Gap**:

1. **静态 `next: "/dynamic/..."` 在现行实现必然失败** (`parse_node_wait_for_deps` 无 `/dynamic/` 豁免, 抛 "Next node not found"). dsl.md §423/§438/§1114 描述与实现矛盾.
2. **generate→register→execute 全链路无任何 E2E 测试**. 现有 2 个 "E2E" 实为 prompt smoke, 真实 LLM 对 `execute_generate_subgraph` 覆盖 = 零.

**关键架构事实**: plan_execute.agent.md 已 ship 的"LLM 生成子图→执行"走 `loop/execute_plan` 工具 (独立子引擎), **不走 generate_subgraph 节点**.

### 追加 #3: P1 实施路径修订为方案 A'' (2026-09-17)

基于上述调研, P1 方案从"全 DSL 化"修订为**方案 A''** (混合模式):
- 分类逻辑仍是 DSL 图 `lib/loop/intent_classify.agent.md` (~30 行)
- Dispatch 决策在 **ChatSession 层** (两次平级 loop/run 调用)
- 动态子图部分**复用已 ship 的 `loop/execute_plan` 工具模式** (独立子引擎)
- **不使用 generate_subgraph 节点** (latent gap #1 已知, 绕道)
- 估时 **0.5-1 sprint** (2-3 天实施 + 3-5 天真实 LLM 测试)
- 符合 pdk/loop_agent/README 双循环架构: Chat-loop 管 turn 边界路由, Agent-loop 管 turn 内推理

`GenerateSubgraphNode` 节点 deferred to 独立 fix 项 (不阻塞 P1). 修复需要 (a) build_dag 加 `/dynamic/` 豁免或修正 dsl.md 文档 + (b) 补全 3+3 真实 LLM E2E, 估时 1-2 sprint.

### 追加 #4: Pre-Wave3 4-Gate + Wave 3 Phase 1 SHIPPED 全闭环 (2026-09-21 → 2026-09-23)

主计划 §一.2 / §三 / §四 / §五 / §六 / §八 / §十 / §十一 / §十二 / 附录 B 已分别反映 2026-09-21 → 2026-09-23 期间的 7 个 change ship + archive (C4 + D8 + G1/G2/G3/G4 + Wave 3 Phase 1). 完整里程碑:

1. **C4 `harness-rsi-pilot` ✅ SHIPPED + GO 2026-09-21** (11 atomic commits 跨 5 days, 5 轮 Oracle review 闭环, Decision Record `docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md`). 闭环第 7 环"版本提交"端到端断裂发现 (Oracle `bg_3c06ae5b`).
2. **G1 `harness-rsi-remove-governance` ✅ SHIPPED 2026-09-21** (merge `9709317`, Oracle bg_8237a316 SHIP-with-fixes 0C + 2M + 4M + 1 D3 ACCEPT + retry-2 const qualifier + retry actual g++ compile).
3. **G2 `evolution-verdict-reward-quality` ✅ SHIPPED 2026-09-22** (merge `dc12a17`, Oracle bg_ebfe1c25 SHIP verdict 0C + 0M + 2 Minor). Start: 24h Wave 3 cooling-off 计时 2026-09-22 22:30 UTC (per G2 merge).
4. **G3 `sync-pdk-contract-header` ✅ SHIPPED 2026-09-22** (merge `a196a09`, Oracle bg_ef5a0ca4 SHIP-with-fixes 0C + 1M + 3 Minor; drift-guard grep `<` + `")"` 字符 sentinel).
5. **G4 `genome-wiring-harness-rsi-gepa` ✅ SHIPPED 2026-09-22** (merge `fb2769f`, Oracle bg_e4eec567 SHIP-with-fixes 0C + 3M + 1 Minor; genome_version payload + case-7e stubs + ADR-0068 L254 修正). 闭环第 7 环真实端到端修复 (per Oracle bg_3c06ae5b Critical C2 发现).
6. **Wave 3 Phase 1 `finetune-base-model` Pilot ✅ SHIPPED 2026-09-23** (merge `f0a5c4b`, Oracle bg_7fe026cc SHIP-with-fixes 1 Critical + 1 Major + 4 Minor; LLMProviderFactory ctor 自注册 baseline 6 处 fix + D1 评分 yaml 算术 5/5 校正). **ADR-0078 ✅ Approved + Wave 3 Pilot 激活**.
7. **24h Wave 3 cooling-off** override by user 2026-09-22 + chain 合规 AC-12 (Pre-Wave3 ✅ + Wave 3 ✅) + builder-handoff::cooling_off_override_audit 字段完整审计. Wave 3 cooling-off 起算 = `f0a5c4b` merge 2026-09-23T05:33Z, 满点 = 2026-09-24T05:33Z.

**累计 ship**: 9 atomic commits + 5 Oracle review sessions. 主要文档更新: §一.2 active 9→5 + §一.3 ctest 252→211 actual + §一.5 ADR-0078 ✅ Approved + §三 Pre-Wave3 子节 status 翻转 + Wave 3 Pilot row + §四 G1/G2/G3/G4/W3.P1 子节 + §五 Pre-Wave3 Sprint A-B + Wave 3 Phase 1 段 + §六 R12-R15 (Wave 3 cooling-off 风险登记) + §八.1 ADR-0078 翻牌 + §八.3.1 active 9→5 + archive 6→12 项 + §十 Drift Log +6 行 + §十一 Adjustment Log +7 行 + §十二 Strategic Pivots 2 行 + 附录 B.5 + B.6 (实施路径). 详 §一.2 / §三 / §四 / §五 / §十 / §十一.

---

## 一、Baseline (项目当前状态)

### 1.1 Phase 进度
- **Phase 6c** (2026-08-19 ~ 09-09, ~80h) — 🟢 实质 ship 完成 (11/13 任务)
- **Sprint 33+** (Day 1-5 2026-09-16) — real-LLM 验证 + 收盘，7 changes ship + archived
- **Phase 7** — ⏸ Gated (3/6 启动条件 FAIL，结构性不满足 Solo Dev ~27h/周)
- **Phase 8a/b** — ⏸ Gated by Phase 7a ship ≥3 月

### 1.2 Active vs Archive
- **openspec/changes/** 当前 active: **5** (per `ls openspec/changes/` 实测 2026-09-23; 原 "9" 已大部 superseded by Pre-Wave3 4-Gate 序列 SHIP + Wave 3 Phase 1 SHIP, 详见 §八.3.1):
  - `2026-09-17-fix-generate-subgraph-static-next` (latent gap #1 fix)
  - `2026-09-17-intent-classification-router` (P1, Sprint 36+ 自然下一候选, 方案 A'')
  - `2026-09-18-chat-real-llm-coverage-phase-h` (real-LLM E2E 6 cases follow-up)
  - `2026-09-18-fix-flatten-layers-comment-drift` (drift cleanup)
  - `2026-09-18-provider-llm-tool-empty-passthrough` (provider bug)
- **最近 archive** (2026-09-19 → 2026-09-23): **12** new (修正 +1: ADR-0086 v1.1 archive 由 commit 798b6c6 删除但未物理归档, 2026-09-21 git history 恢复, 详见 `docs/governance/2026-09-21-openspec-archive-recovery.md` 治理注记)
  - `2026-09-19-2026-09-16-genome-registry` (C2)
  - `2026-09-20-2026-09-16-h-d-m-transition-guard` (C3)
  - `2026-09-20-2026-09-20-ig-genome-registry-walk-ancestors` (C3 follow-up — D5/D6/D9 ship)
  - `2026-09-20-2026-09-20-adr-0086-v1-1-harness-change-confounder` (ADR-0086 v1.1 实施载体, **物理归档 2026-09-21 恢复, 4 文件完整**)
  - `2026-09-21-2026-09-20-adr-0068-appendix-a-evolution-themes` (D8 主题注册)
  - `2026-09-21-2026-09-16-harness-rsi-pilot` (C4 — GO decision)
  - **`harness-rsi-remove-governance-2026-09-22`** (G1/4 Pre-Wave3 收口门禁 — SHIP 2026-09-21 merge `9709317`)
  - **`evolution-verdict-reward-quality-2026-09-22`** (G2/4 Pre-Wave3 收口门禁 — SHIP 2026-09-22 merge `dc12a17`)
  - **`sync-pdk-contract-header-2026-09-22`** (G3/4 Pre-Wave3 收口门禁 — SHIP 2026-09-22 merge `a196a09`)
  - **`genome-wiring-harness-rsi-gepa-2026-09-22`** (G4/4 Pre-Wave3 收口门禁 — SHIP 2026-09-22 merge `fb2769f`)
  - **`wave-3-finetune-base-model-pilot-phase1-2026-09-23`** (Wave 3 Phase 1 Pilot — SHIP 2026-09-23 merge `f0a5c4b`, follow-up `232eb13` SHIP-with-fixes C1+M1)

### 1.3 关键 Baseline 数据
- **ctest baseline**: **211** 测试总数 (实测 `grep '^add_test' build/tests/CTestTestfile.cmake | wc -l` 2026-09-23, post Wave 3 Phase 1 ship; `docs/active-status.md` 旧值 252 系 post-C4 实测, Wave 3 SHIP 后 build 重 configure 数值收敛 — 漂移已记录待补行; 含 4 项 pre-existing failures — test_chat_session_events / test_budget_alert / test_e2e_real_llm / test_skill_interpreter 7.S29-1 — git stash 验证与 C4 无关)
- **Wave 3 focused ctest (post-merge 验证, per Oracle bg_7fe026cc AC-8)**: 9/9 PASS (test_provider_factory + test_provider_factory_concurrent + test_provider_register_dynamic_tool + test_training_data_pipeline + test_llm_provider_factory + test_llm_tool + test_cost_tracking_decorator + test_genome_registry + test_llm_provider_factory_decorator)
- **adr_lint.py**: 0 errors
- **docs_drift_audit**: 0 DRIFT items
- **openspec validate**: clean (7 个 archived changes 全部 4/6 文件完整 per AGENTS.md Day 5 lesson)

### 1.4 关键 bug（驱动本 plan 存在）
- **Bug 1**: ✅ FIXED (`f84dbb3`, Sprint 34 C0) — `loop/run` 工具返回契约补 `ok/error_code` 字段 + `chat_session.cpp:526` 3 层 fallback 替代无条件 `result.success = true`
- **Bug 2**: ✅ FIXED (`f84dbb3` + `d21ac6f`, Sprint 34 C0) — "Tool not found" envelope remap 到 `ToolNotRegistered` error_code (ADR-0023 对齐)
- **Bug 3**: ✅ FIXED (`f4766be`, Sprint 34 C1) — `loop/decide_react` / `loop/execute_plan` / `loop/process_task` 全部实现 (`pdk_entry.cpp:402/434/524`) + `loop/set_capture_mode` 新增；13 unit test PASS (mock LLM) + 真实 DeepSeek LLM "Hello" 验证 ✅。**残量风险已闭环**: ✅ FIXED (2026-09-18, commits `a96842e` + `9dc3ac8`, archived `2026-09-18-fix-react-decide-empty-response`) — 根因 = think 节点 LLM 空 text silent 穿透 (非 `flatten_layers` 嵌套, Oracle `ses_f4d05cdb0ffe0BhMdEADyfsdTz` 纠正初判), fix = `node_executor.cpp:194-205` main path + `:147-157` stream path 双路径 fail-fast 空校验 +21 行, 5 cases / 13 assertions NodeExecutor 级测试 (`tests/test_dsl_engine_ctx_bridge.cpp`) + 1 skip-guarded real-LLM skeleton (`tests/test_react_loop_real_llm.cpp`). Real-LLM 6 cases 降级移交 chat-real-llm-coverage Phase H follow-up.
- **Bridge fix**: ✅ FIXED (commit `0b0da50`) — `tool_result.cpp:from_json` 加 `error → meta.error_message` 桥接，消除 PDK 工具失败错误信息隐形系统性问题

### 1.5 既有契约栈（Wave 2 复用基础）
- **ADR-0083** IEvaluator/RewardSignal Contract (✅ V2 Shipped 2026-08-27)
- **ADR-0084** Mutation Governance Contract (✅ V1 Shipped 2026-08-26)
- **ADR-0086** Credit Assignment Contract (✅ **Approved v1.1, 2026-09-20** — merged 886def1; v1.0 + v1.1 amendment HarnessChange confounder + judge_data_freshness 完整 5 cases + GenomeVersion 单一所有权; Critical C1 signature amendment `::agenticdsl::genome::IGenomeRegistry&`; verdict-only 签名限制 (v1.2 candidate))
- **ADR-0088** H→D→M Transition Guard (✅ **Approved v1.0, 2026-09-20** — D1-D4/D6/D9 ship + D5/D6 实装 via C3 follow-up `ig-genome-registry-walk-ancestors` + D8 主题注册 deferred to `2026-09-20-adr-0068-appendix-a-evolution-themes`; test_transition_guard 13/13 + test_genome_walk_ancestors 10/10 + test_credit_assignment 12/12 PASS; Oracle dual-agent review 4 Critical fixes + 2nd review APPROVE 95/100)
- **ADR-0080** AppendOnlyEventLog (✅ Approved v1.1)
- **ADR-0061-13** Distillation Output Format (✅ Approved + Shipped 2026-08-29)
- **ADR-0078** Fine-tune Base Model (✅ **Approved + Wave 3 Phase 1 Pilot SHIPPED 2026-09-23** — D1 评分框架 + D3 数据准备 + D7 最小版 provider stub 注册; D4-D7 Phase 2 deferred per capacity evaluation)

---

## 二、Dependency Graph

**任务状态图例**:
- ✅ **已完成** (shipped + archived) — 主路径确定完成
- 🔄 **进行中** (active OpenSpec, 实施中或等待启动)
- ⏳ **计划中** (planned, 未启动; 依赖前置完成后立项)

### 主时间线总览 (Master Timeline)

```
================================================================================
                              MASTER TIMELINE
================================================================================
Sprint 34    Sprint 35      Sprint 36     34.5          Pre-Wave3          Wave 3
2026-09-17   2026-09-19-20  2026-09-20-21 2026-09-18    2026-09-21-22     2026-09-23
   ✅            ✅              ✅            ✅              ✅                ✅
────────────────────────────────────────────────────────────────────────────────
│ Wave 1     │ Wave 2      │ Wave 2.5     │ Wave 34.5    │ 4-Gate 收口      │ Phase 1 ✅
│ C0,C1,P0   │ C2,C3,walk  │ C4 (GO)     │ F1           │ G1,G2,G3,G4      │ Phase 2 ⏳
│            │ +D8         │              │              │                  │
================================================================================
```

### 已完成路径 (✅ ALL SHIPPED 2026-09-23)

```
✅ Wave 1 (Sprint 34, 2026-09-17) — 修 chat demo, P0 必要
═══════════════════════════════════════════════════════════════
   ✅ C0 (fix-loop-run-return-contract) ⊥ ✅ C1 (loop-agent-tools)    [parallel]
              │                            │
              └─────────┬──────────────────┘
                        ▼
              ✅ P0 (fix-dsl-call-pause-autonomous-mode)              [⊥ C0+C1]
                        │
                        ▼
                 [Sprint 34 ship gate ✅]


✅ Wave 34.5 follow-up (2026-09-18)
═══════════════════════════════════════════════════════════════
              ✅ F1 (fix-react-decide-empty-response)                [独立 ship]
                                                                       ↑ 不阻塞主路径


✅ Wave 2 (Sprint 35, 2026-09-19 → 20) — 自进化骨架, P1 中期
═══════════════════════════════════════════════════════════════
              ✅ C2 (genome-registry) ────── hard dep on ✅ C0 + ✅ C1
                        │
                        ▼
              ✅ C3 (h-d-m-transition-guard) ── hard dep on ✅ C2
                        │
                        ▼
              ✅ walk-ancestors follow-up ──── hard dep on ✅ C3
                  (C3 follow-up + judge_data_freshness 5 cases)
                        │
                        ▼
              ✅ D8 (adr-0068-appendix-a-evolution-themes)
                  [主题注册: evolution.transition/readiness.denied]    [⊥ C3]
                        │
                        ▼
                 [Sprint 35 ship gate ✅]


✅ Wave 2.5 (Sprint 36, 2026-09-20 → 21) — Pilot 实验, P2 紧跟
═══════════════════════════════════════════════════════════════
              ✅ C4 (harness-rsi-pilot) ──── hard dep on ✅ C3 + ✅ D8
                  (11 atomic commits, 5 轮 Oracle review 闭环)
                        │
                        ▼
                 [Go / No-Go Decision Gate]                              ──────── ✅ GO 2026-09-21
                        │
              ┌─────────┴─────────┐
              ▼                   ▼
           [Go]               [No-Go]
       ADR-0078 立项 ◄──── ✓    ✗ 归档 Wave 2 skeleton
                                  等待需求驱动


✅ Pre-Wave3 收口门禁 (2026-09-21 → 22, 4-Gate 序列)
═══════════════════════════════════════════════════════════════
              ✅ G1 (harness-rsi-remove-governance)
                  ──── hard dep on ✅ C4
                        │
                        ▼
              ✅ G2 (evolution-verdict-reward-quality)
                  ──── hard dep on ✅ G1     [同改 harness_rsi.cpp + MutationGateContext]
                        │
                        ▼
              ✅ G3 (sync-pdk-contract-header)
                  ⊥ G1/G2/G3        [独立可并行, 0 cross-dep]
                        │
                        ▼
              ✅ G4 (genome-wiring-harness-rsi-gepa)
                  ──── hard dep on ✅ G1     [同改 harness_rsi.cpp, 闭环第 7 环闭合]
                        │
                        ▼
                 [Pre-Wave3 ship gate ✅ 2026-09-22]


✅ Wave 3 Phase 1 (2026-09-23) — ADR-0078 Model-RSI Pilot
═══════════════════════════════════════════════════════════════
              ✅ W3.P1 (finetune-base-model-pilot-phase1)
                  ──── hard dep on ✅ G1 + ✅ G2 + ✅ G3 + ✅ G4
                  (24h cooling-off override by user 2026-09-22; chain AC-12 合规)
                        │
                        ▼
              [Wave 3 cooling-off 起点 = f0a5c4b merge 2026-09-23T05:33Z
                                  ↓ 🔄 24h 计时中 (剩余 ~22h)
                                  满点 = 2026-09-24T05:33Z]
                        │
                        ▼
                       ⏳ Wave 3 Phase 2 (D4-D7) [见下方"计划中"]
```

### 进行中 (🔄 Active, 2026-09-23 实测 5 项)

```
🔄 Active OpenSpec — Phase 6c 早期遗留短链 + DSL 候选 + provider bug defense
═══════════════════════════════════════════════════════════════════════════════
  [都独立 ship, 不阻塞主路径, 与 Wave 3 Phase 2 并行候选]

  🔄 2026-09-17-fix-generate-subgraph-static-next    [latent gap #1 fix]
  🔄 2026-09-17-intent-classification-router        [P1 方案 A'', Sprint 36+ 候选]
  🔄 2026-09-18-chat-real-llm-coverage-phase-h       [real-LLM E2E Phase H 6 cases]
  🔄 2026-09-18-fix-flatten-layers-comment-drift    [drift cleanup, P3 cosmetic]
  🔄 2026-09-18-provider-llm-tool-empty-passthrough [provider bug defense-in-depth]
```

### 计划中 (⏳ Planned, 未启动)

```
⏳ Wave 3 Phase 2 (D4-D7) [主路径下一里程碑]
═══════════════════════════════════════════════════════════════
              ⏳ W3.P2 (finetune-base-model-pilot-phase2)
                  ──── hard dep on:
                       (a) Wave 3 cooling-off 满点 (2026-09-24T05:33Z) [24h 间隔合规]
                       (b) ✅ W3.P1 ship (2026-09-23 已有 archive + AC-12)
                       (c) 4-5 周估时 (D4 LoRA/QLoRA + D5 评估 + D6 AgenticMind 回流 + D7 serving)
                  内部子任务 (建议分拆独立 change):
                       ⏳ D4 LoRA 训练管线 + HF TRL/PEFT 引入
                       ⏳ D5 评估框架 (per ADR-0078)
                       ⏳ D6 AgenticMind → HydraForge 回流
                       ⏳ D7 真实推理 serving (LLMProvider 集成)


⏳ 其他 planned (低优先级, 不阻塞主路径)
═══════════════════════════════════════════════════════════════
  ⏳ harness-rsi-pilot V2 — `load(genome@N) → 重建 ChatSession → 1 turn` E2E
                       (G4 out-of-scope 留待 follow-up)
  ⏳ ADR-0086 v1.2 amendment — judge_data_freshness 签名扩展
                       (Result<AttributionVerdict, JudgeResult> 含 verdict + HarnessChangeRecord)
  ⏳ Phase 7a 解锁条件复评 — Wave 3 后端到端 (3/6 FAIL → 复评窗口)
  ⏳ generate_subgraph 节点修复 — dsl.md §423/§438/§1114 与实现矛盾
                       (build_dag 加 /dynamic/ 豁免 + 3+3 真实 LLM E2E)
  ⏳ fix-timer-callback-dtor-race — GEPALoop 同 pattern #9 + #11 应用
  ⏳ EvalQuality "Unknown" 硬编码消除 — 复用 evaluation_events.h quality_name
                       (per Decision Record §3 friction 1)
```

### Mermaid 渲染友好版本 (GitHub 兼容)

如需可视化渲染（如 GitHub Issue / Docs PR review），可参考以下 Mermaid 源码：

```mermaid
flowchart TB
    classDef done fill:#9be9a8,stroke:#2d8f4e,color:#000
    classDef active fill:#f9d56e,stroke:#b08a2e,color:#000
    classDef planned fill:#cfd8e3,stroke:#6b7a8f,color:#000

    %% Wave 1 ✅
    C0[✅ C0<br/>fix-loop-run-return-contract]:::done
    C1[✅ C1<br/>loop-agent-tools]:::done
    P0[✅ P0<br/>fix-dsl-call-pause-autonomous-mode]:::done
    F1[✅ F1<br/>fix-react-decide-empty-response<br/>Wave 34.5]:::done

    %% Wave 2 ✅
    C2[✅ C2<br/>genome-registry]:::done
    C3[✅ C3<br/>h-d-m-transition-guard]:::done
    C3fu[✅ walk-ancestors<br/>C3 follow-up]:::done
    D8[✅ D8<br/>adr-0068-appendix-a]:::done

    %% Wave 2.5 ✅
    C4[✅ C4 + GO<br/>harness-rsi-pilot]:::done

    %% Pre-Wave3 ✅
    G1[✅ G1<br/>harness-rsi-remove-governance]:::done
    G2[✅ G2<br/>evolution-verdict-reward-quality]:::done
    G3[✅ G3<br/>sync-pdk-contract-header]:::done
    G4[✅ G4<br/>genome-wiring-harness-rsi-gepa]:::done

    %% Wave 3 ✅ Phase 1
    W3P1[✅ W3.P1<br/>finetune-base-model Phase 1]:::done
    Cooling[🔄 24h cooling-off<br/>2026-09-23 → 09-24]:::active

    %% Wave 3 ⏳ Phase 2 (planned)
    W3P2[⏳ W3.P2<br/>D4+D5+D6+D7]:::planned
    D4[⏳ D4 LoRA 训练]:::planned
    D5[⏳ D5 评估]:::planned
    D6[⏳ D6 AgenticMind 回流]:::planned
    D7[⏳ D7 真实推理 serving]:::planned

    %% Edges — Wave 1
    C0 -.parallel.-> C1
    C0 --> P0
    C1 --> P0
    P0 --> F1

    %% Edges — Wave 2 (hard deps)
    C0 --> C2
    C1 --> C2
    C2 --> C3
    C3 --> C3fu
    C3 --> D8

    %% Edges — Wave 2.5
    C3 --> C4
    D8 --> C4

    %% Edges — Pre-Wave3
    C4 --> G1
    G1 --> G2
    G1 --> G4
    G2 -.parallel.-> G3
    G4 --> W3P1
    G3 --> W3P1
    G2 --> W3P1

    %% Edges — Wave 3
    W3P1 --> Cooling
    Cooling -- 满点 2026-09-24 --> W3P2
    W3P2 --> D4
    W3P2 --> D5
    W3P2 --> D6
    W3P2 --> D7
```


| P1 → C2 (可选) | soft | P1 输出的 `suggested_loop` 可被 Genome spec.harness.loop_type 引用 | 后续 follow-up |

---

## 三、Change Overview

| # | Slug | 类型 | 估时 | 依赖 | 状态 | Sprint |
|---|------|------|------|------|------|--------|
| **C0** | `fix-loop-run-return-contract` | immediate-placeholder | 1-2h | None | ✅ | 34 |
| **C1** | `loop-agent-tools` | immediate-placeholder | 3-5h | None (∥ C0) | ✅ | 34 |
| **P0** | `fix-dsl-call-pause-autonomous-mode` (Wave 2 P0) | immediate-placeholder | 4h | C0+C1 | ✅ | 34 |
| **C2** | `genome-registry` — ✅ **SHIPPED** (2026-09-19, archived `2026-09-19-2026-09-16-genome-registry`): Oracle dual-agent review (Oracle bg_a818a6a1 设计评审 + bg_9ade564d 实现评审 BLOCK→fixed + Metis bg_89293120 ship-with-fixes→fixed). Genome CRD + IGenomeRegistry interface (5 methods + 1 internal walk_ancestors deferred to C3) + FilesystemGenomeRegistry impl (D9/D10/D11 per Oracle bg_a818a6a1). 12 tests / 266 assertions PASS. **Critical fixes shipped**: C1 cycle infinite loop (visited keyed on name@version pair + 10000 depth cap), C2 vacuous cycle test (re-sign tampered YAMLs), M1 list_versions double-file check + sig-first atomic write, M2 RAND_bytes CSPRNG, M3 IOError returned, M4 mutex commit serialization, M5 lineage validation at commit time. **Spec amendments**: fork version semantics (max+1 not parent+1), walk_ancestors deferred to C3, fsync deferred to follow-up, CLI tool deferred to genome-cli change. | ~~hard-placeholder~~ | 1 周 → DONE | C0+C1+P0+F1 | ✅ SHIPPED | 35 |
| **C3** | `h-d-m-transition-guard` — ✅ **SHIPPED** (2026-09-20, archived `2026-09-20-2026-09-16-h-d-m-transition-guard`): D1 5 态状态机 (Idle/Harness/Data/Model/Done) + D2 EvolutionVerdict + D3 can_transition 编译期矩阵 + D4 reset_to_idle + D7 复用 IEvaluator/IBudgetController/AttributionRecord. **test_transition_guard** 13/13 cases / 47 assertions PASS. **Oracle post-impl verdict**: ALIGNMENT SCORE 62 / NEEDS_FIX (2 critical + 4 major + 4 minor debt). 2 Sprint 34+ follow-up 已正式登记: `2026-09-20-ig-genome-registry-walk-ancestors` (D5/D6/D9) + `2026-09-20-adr-0068-appendix-a-evolution-themes` (D8 主题注册). C4 harness-rsi-pilot **unblocked** (follow-up #1 已 ship 2026-09-20). | ~~hard-placeholder~~ | **2-3 天 → DONE** | C2 ✅ | ✅ SHIPPED | 35 (D5/D6/D9 follow-up 也 ship) |
| **P1** | `intent-classification-router` (Wave 2 P1) — **方案 A''** (Oracle `ses_f4fd88215ffeUWSe2StAWtFPSQ` 推荐): `lib/loop/intent_classify.agent.md` (DSL 分类图, ~30 行) + `loop/classify_intent` 工具 (loop_agent C++, ~30 行) + ChatSession `"auto"` routing (C++, ~20 行, **两次平级** loop/run 调用) + 真实 LLM E2E 6 cases (3 happy + 3 error). **不使用 generate_subgraph 节点** (Oracle latent gap #1: 静态 `next: /dynamic/...` 在 build_dag 抛错, 文档与实现矛盾). **动态子图部分复用已 ship 的 `loop/execute_plan` 工具模式** (独立子引擎, 不走 /dynamic/ 注册). 估时 **0.5-1 sprint** (2-3 天实施 + 3-5 天真实 LLM 测试). | hard-placeholder | **0.5-1 sprint** | P0 | 🟡 Deferred → Sprint 36+ (与 C4 并行候选) | 35+ |
| **C4** | `harness-rsi-pilot` — ✅ **SHIPPED + GO** (2026-09-21, archived `2026-09-21-2026-09-16-harness-rsi-pilot`). 11 atomic commits 跨 5 days (f7f0fe3 + harness_rsi commits + 16b1a96 + 4fd7ead + 3 Oracle review session pairs + 4th review 08aace2 Case 3d). apply_harness_mutation 轻量函数 (5 参, per ADR-0088 D4 取消 IHarnessRSI 接口) + IToolRegistry::unregister_tool_function (Phase 4.0 DB1 fix, 25 文件 override). **test_harness_rsi_pilot 9 cases / 43 assertions ALL PASSED** (修正 Oracle bg_6a8e4397 A5: 原行写 8/39 为 3rd review 快照, 4th review 08aace2 后 9/43). 5 轮 Oracle review 闭环: dual-agent pre-impl (bg_3672cb57 6 修正 + bg_1f291bc4 5 DEAL-BREAKER + Case 4 删除) → 2nd review SHIP-with-fixes (bg_770d1308 5 文档级修正) → 3rd review post-impl SHIP-with-fixes (bg_3ef7280a 5 修正) → 4th review 独立审查 (bg_afa84d4d 6 修正: Critical-1 Gate 2.5 partial-apply 零状态变更 + Major-2 + Major-4 + Minor-5+6). **Decision Record** `docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md` GO (**6 判据全绿, 见 §四 C4 GO 判据修订 + 第 6 项 post-hoc closure gate**). **Total ctest**: 251 → 252 (+1 new). | ~~hard-placeholder~~ | **5 days ✅ COMPLETE** (1 周估时下限) | C3 ✅ + D8 ✅ | ✅ SHIPPED + GO | 36 |
| **F1** | `fix-react-decide-empty-response` (Wave 34.5 follow-up) — react agent loop 在 `decide` 节点真实 LLM 端到端 `Missing 'response' argument` 修复。✅ **SHIPPED** (2026-09-18, commits `a96842e` + `9dc3ac8`, archived `2026-09-18-fix-react-decide-empty-response`) — 根因 = think 节点 LLM 空 text silent 穿透, fix = `node_executor.cpp` main + stream 双路径 fail-fast 空校验 +21 行. Real-LLM 6 cases 降级为 1 skip-guarded skeleton (`tests/test_react_loop_real_llm.cpp`) + 移交 chat-real-llm-coverage Phase H follow-up. | hard-placeholder | **5h → DONE** | `0b0da50` (bridge fix) + `chat-real-llm-coverage` helper | ✅ SHIPPED | **34.5** (post-Wave-1) |

**总估时**: 4-5 周 + F1 5h ≈ **5 周**（基线 C0+C1+P0 已 ship + C2+C3 Sprint 35 + C4 Sprint 36 + P1 deferred → Sprint 36+ 候选与 C4 并行 + generate_subgraph fix 项独立 1-2 sprint + F1 Wave 34.5 follow-up）
**P1 实施方案**: 方案 A'' (Oracle `ses_f4fd88215ffeUWSe2StAWtFPSQ` 推荐)，2 次平级 loop/run + 复用 `loop/execute_plan` 模式
**P1 worker pool routing**: 不需要单独决策（Oracle `ses_f505f99fdffefgE5Q9oFA2t2AD`）
**P1 intent schema**: 4 字段 `intent_type/complexity/suggested_loop/requires_subgraph`，不加 `worker_pool`

(原 "总估时: 3-4 周" Oracle `ses_f55f307f6ffeRJ9SIny8iUbZ8Y` 评审版已被 P1 补登记后的 4-5 周 supersede — 详 §十一 Adjustment Log)

### Pre-Wave3 收口门禁（4 项，2026-09-21 立项 → **✅ 全部 SHIPPED 2026-09-21 → 2026-09-22**）

C4 GO 后 Wave 3 (ADR-0078 Model-RSI pilot) 立项前的 4 项门禁。前 3 项已在 C4 Decision Record §3 登记；第 4 项 (`genome-wiring-harness-rsi-gepa`) 由 Oracle `bg_3c06ae5b` (2026-09-21) 在审计闭环第 7 环"版本提交/发布"端到端断裂时发现并起草——C2/C3/C4 三组件 ship 后 `IGenomeRegistry` 零生产调用方，`apply_harness_mutation` 仅改内存（`src/evolution/harness_rsi.cpp:149-179`），`GEPALoop::reflect_and_commit` 仅发审计事件（`src/modules/cognitive/gepa_loop.cpp:171-188`）。该 gate 与前 3 项同构（都是防止 Wave 3 把未治理状态当已验证前提复制）。

| # | Slug | 类型 | 估时 | 依赖 | **实际状态 (2026-09-23)** | Sprint |
|---|------|------|------|------|------|--------|
| **G1** | `harness-rsi-remove-governance` | remove 治理 + 安全 + 并发 | 1-2 天 | C4 ✅ | **✅ SHIPPED 2026-09-21** (merge `9709317`, impl `714764d` + fixes `9ee475e`/`f1a6647`/`e182f82`; Oracle bg_8237a316 SHIP-with-fixes 0C + 2M + 4M + 1 D3 ACCEPT) | Pre-Wave3 Sprint A ✅ |
| **G2** | `evolution-verdict-reward-quality` | EvolutionVerdict.reward_quality 字段 | 0.5-1 天 | G1 (同改 `MutationGateContext`/`harness_rsi.cpp`) | **✅ SHIPPED 2026-09-22** (merge `dc12a17`, impl `e51073a`; Oracle bg_ebfe1c25 SHIP verdict 0C + 0M + 2 Minor) | Pre-Wave3 Sprint A ✅ |
| **G3** | `sync-pdk-contract-header` | sync-pdk.sh contract header 同步 | 0.5-1 天 | None (独立可并行) | **✅ SHIPPED 2026-09-22** (merge `a196a09`, impl `a641d34` + fix `77d6fae`; Oracle bg_ef5a0ca4 SHIP-with-fixes 0C + 1M + 3 Minor) | Pre-Wave3 Sprint A (∥ G1+G2) ✅ |
| **G4** | `genome-wiring-harness-rsi-gepa` | apply + GEPA → IGenomeRegistry 接线 (闭环第 7 环闭合) | 1-2 天 | G1 (同改 `MutationGateContext`/`harness_rsi.cpp`) | **✅ SHIPPED 2026-09-22** (merge `fb2769f`, impl `1fcb00e` + fix `c5d0c78`; Oracle bg_e4eec567 SHIP-with-fixes 0C + 3M + 1 Minor; gepa.commit.committed payload 补 genome_version 字段) | Pre-Wave3 Sprint B (G1 ship 后) ✅ |
| **W3.P1** | **`wave-3-finetune-base-model-pilot-phase1`** (NEW, Wave 3 立项目标) | ADR-0078 D1+D3+D7 最小版 ship | 1-2 天 | G1+G2+G3+G4 ✅ (24h cooling-off override by user) | **✅ SHIPPED 2026-09-23** (merge `f0a5c4b`, impl `97a2abb` + fix `232eb13`; Oracle bg_7fe026cc SHIP-with-fixes 1 Critical + 1 Major + 4 Minor; ADR-0078 ✅ Approved Wave 3 Pilot 激活) | **Wave 3 Phase 1 ✅** |

**串行约束**（Oracle dual-review `bg_c706862b` + `bg_d9744d91` 已修正的语义）：
- G1 → G2 → G4（G1 后两者都改 `MutationGateContext`/`harness_rsi.cpp`）→ **✅ 全部 SHIPPED**
- G3 ∥ 全并行（独立）→ **✅ SHIPPED 2026-09-22**
- **Wave 3 Phase 1 24h cooling-off 起点** = `f0a5c4b` merge 2026-09-23T05:33Z, **满点** = 2026-09-24T05:33Z; **Phase 2 (D4-D7)** 需独立立项 + cooling-off 合规

**实际估时 (2026-09-21 → 2026-09-23)**: G1 1 day + G2 ∥ G3 ∥ G4 0.5-1 day each = **3 天总** (符合 Oracle bg_c706862b 估时预测). Wave 3 Phase 1 = 1 day (24h cooling-off override). **全 4-Gate + Wave 3 Phase 1 = 4 天总实耗** (vs Master Plan 估时 3-4 + 1 = 4-5 天). **0 偏差**.

### 类型说明
- **immediate-placeholder**: Wave 1 修复 bug 的高优先级 change，待写完整 proposal/design/tasks/specs
- **hard-placeholder**: 依赖上游 change ship + 接口稳定后才能详细制定

---

## 四、Detailed Tracking

### C0: `fix-loop-run-return-contract`

**类型**: immediate-placeholder
**估时**: 1-2h
**Sprint**: 34 (Wave 1)

**目标**: 让 `loop/run` 工具返回 `{"ok": false, "error_code": ..., "response": ""}` 契约 + ChatSession 消费 `ok` 字段

**Why（精简）**:
- 现有 `loop/run` 返回 `{"success", "response", "steps", "tokens_used", "cost_usd"}` 5 字段
- 错误路径返回 `{"success": false, "error", "steps": 0}` 但 ChatSession 看不到 success 字段（只读 response/steps 等）
- `chat_session.cpp:503` `result.success = true` 无条件覆盖
- Oracle 评审 (C1): 治标不治本，**应改契约而非改消费方**

**What（精简）**:
1. `pdk_entry.cpp` 改 `loop/run` 返回契约：新增 `ok` 字段（语义对齐 ToolResult.ok），所有返回路径统一
2. `chat_session.cpp:500-503` 改消费 `ok` 字段而非 success 字段
3. 加 regression test：mock 错误响应时 ChatResult.success=false + error_message 非空

**Out of Scope**: tool 内部错误处理逻辑（保持现状）

**Verification**: 真实 LLM 模式 loop 内部失败 → Assistant 显示 error_message 而非空

**详细制定 TODO** (待起草 proposal 时填充):
- [ ] 1. Oracle 咨询: `ok` 字段是否对齐 ToolResult.ok 语义还是新设计
- [ ] 2. 写完整 design.md (契约 diff + 兼容性矩阵)
- [ ] 3. 写完整 tasks.md (RED-GREEN-REFACTOR 5 步)
- [ ] 4. 写完整 spec.md (R1: loop/run 返回 ok 字段; R2: ChatSession 消费 ok; R3: 错误路径透传 error_code)
- [ ] 5. 移除 PLACEHOLDER 标记 → openspec validate → ship
- [ ] 6. 启动 Sprint 34 实施

---

### C1: `loop-agent-tools`

**类型**: immediate-placeholder
**估时**: 3-5h
**Sprint**: 34 (Wave 1, ∥ C0)

**目标**: 实现 3 个 `lib/loop/*.agent.md` 引用的 loop-specific 工具 + C++/DSL 双循环分工决策文档化

**Why（精简）**:
- `react.agent.md` 引用 `loop/decide_react`（从未实现）
- `plan_execute.agent.md` 引用 `loop/execute_plan`（从未实现）
- `fork_join.agent.md` 引用 `loop/process_task`（从未实现）
- 导致所有 loop 走 0 步 + 空 Assistant
- Oracle 评审 (M1): 必须写明 C++/DSL 双循环功能重叠的分工决策

**What（精简）**:
1. 实现 `loop/decide_react`: 接受 LLM response → 解析 OpenAI function_call JSON / XML tool tags → 返回 `{action_tool, action_args, final, success}`
2. 实现 `loop/execute_plan`: 接受 plan markdown + context → DSLEngine 追加子图 → 返回 `{success, results, total_steps}`
3. 实现 `loop/process_task`: 接受 `{branch_id, query, tools}` → 单线程 react-style 执行 → 返回 `{success, branch_id, result}` (std::async 并行 fan-out 由 DSL `fork` 节点编排)
4. `+ loop/set_capture_mode`: 接受 mode ∈ {None, Training} → 返回成功（让外部切换 Data-RSI 采集）
5. **必须** 在 proposal/design 写明：C++ `agent_loops/react_loop.h` (Sprint 4) 与 DSL `lib/loop/react.agent.md` 是**两套并行实现**，分工 = C++ 为原语层（性能敏感）/ DSL 为编排层（可热更新）
6. **必须** 收窄 `bus_ptr` 字符串裸指针透传：加注释 + 审计点，新工具一律显式注入 (Oracle B2)

**Out of Scope**: Model-RSI、ADR-0078 LoRA 训练

**Verification**: `react.agent.md` 端到端 total_steps ≥ 1 + 3 个新工具单测 + 双循环分工决策文档

**详细制定 TODO** (待起草 proposal 时填充):
- [ ] 1. 决策前置: 3 工具的输入输出 schema (D1-D3) + 解析规则 (JSON function_call vs XML tags vs 自然语言)
- [ ] 2. 写完整 design.md (3 工具实现 + 双循环分工决策 + bus_ptr 边界规则)
- [ ] 3. 写完整 tasks.md (3 工具 × 5 步 = 15 步)
- [ ] 4. 写完整 spec.md (R1: decide_react 协议; R2: execute_plan 协议; R3: process_task 协议; R4: set_capture_mode 协议; R5: bus_ptr 边界规则)
- [ ] 5. 移除 PLACEHOLDER → openspec validate → ship
- [ ] 6. 启动 Sprint 34 实施 (与 C0 并行)

---

### C2: `genome-registry`

**类型**: hard-placeholder
**估时**: 1 周
**Sprint**: 35 (Wave 2)
**依赖**: C0 + C1 ship (chat demo 端到端跑通)

**目标**: 把 `config.json` + `lib/loop/*.agent.md` + `ChatConfig` 抽象为可版本化的 Genome 对象 + 文件系统 Registry

**Why（精简）**:
- MetaRSI-v1 "Genome" 概念：Harness 完整配置打包为可版本化、可分享对象
- HydraForge 现状：配置散落 3 个地方（config.json, .agent.md, ChatConfig 隐式）
- 没有可版本化抽象，Harness-RSI 变异没有 diff/rollback/fork 锚点
- 自进化架构 §一明确"Genome 是 Harness 资产的可追溯性基础"

**What（精简）**:
1. `pdk/genome/spec/genome-v1.yaml` — Genome CRD schema (apiVersion, metadata{name,version,parent,created_by,capture_mode}, spec{harness (string, 已 ship), tools[], budget, model_routing, prompt_cache_prefix})
2. `include/agenticdsl/genome/registry.h` — `IGenomeRegistry` 接口 (load(name@version) / commit(genome) / fork(name, parent, mutations) / list_versions(name) / diff(v1, v2))
3. `src/core/genome/registry.cpp` — 文件系统后端 (`~/.hydraforge/genomes/<name>/<version>/genome.yaml`)
4. 谱系追踪：commit 强制 `parent` 字段必填
5. 完整性校验：HMAC 签名 + schema validation
6. 8 个 test case (load/commit/fork/diff/version_listing/parent_validation/HMAC/invalid_schema)

**Out of Scope**:
- Git-LFS 后端（filesystem 优先）
- Genome-aware ChatSession/DSLEngine 构造参数（避免 BREAKING，留作 follow-up）

**Verification**: Genome commit/fork 往返 + 版本 diff 确定性测试 (注: rollback 非 C2 范围 — IGenomeRegistry 无 rollback 方法, 回滚经 ADR-0079 session fork, 见 ADR-0084 决策 5)

**详细制定 TODO** (待 C0+C1 ship 后):
- [ ] 1. 决策前置: storage backend (D9 filesystem vs SQLite vs Git-LFS) + signature scheme (D10 HMAC vs ed25519)
- [ ] 2. 写完整 design.md (Genome schema + IGenomeRegistry 接口 + filesystem layout + 谱系图)
- [ ] 3. 写完整 tasks.md (schema 8 case × 5 步 + registry 6 case × 5 步)
- [ ] 4. 写完整 spec.md (R1: Genome CRD schema; R2: IGenomeRegistry 接口; R3: 谱系追踪; R4: HMAC 完整性; R5: 错误传播契约)
- [ ] 5. 移除 PLACEHOLDER → openspec validate → ship
- [ ] 6. 启动 Sprint 35 实施

---

### C3: `h-d-m-transition-guard`

**类型**: hard-placeholder
**估时**: 2-3 天
**Sprint**: 35 (Wave 2)
**依赖**: C2 ship (Genome Registry 提供版本号)

**目标**: ~200 行轻量状态机 + can_transition() 强制 H→D→M 顺序，**取消** 3 算子接口（映射既有 ADR 栈）

**Why（精简）**:
- MetaRSI-v1 关键规则：禁止 H→M 直跳（用旧数据训练新能力导致目标混乱）
- 必须 H→D→M（用新 Harness 跑一遍生成数据，再训练）
- 验证逻辑与生成分离（确定性代码，模型无权改）
- Oracle 评审 (M2): 取消原计划的 3 算子接口框架 (IDataRSI/IHarnessRSI/IModelRSI) — **与 ADR-0083/0084/0086 契约栈重复**
- YAGNI 原则：pilot 验证价值前不造重型框架

**What（精简）**:
1. `include/agenticdsl/evolution/transition_guard.h` — `TransitionGuard` 类
2. `can_transition(from_state, to_state, current_genome_version, last_harness_change_version) -> EvolutionVerdict`
3. 核心规则: `H → M` 直跳返回 `{can_proceed: false, reason: "H→M forbidden, must run H→D→M"}`
4. 复用既有契约:
   - 评估侧 → ADR-0083 IEvaluator 接口
   - 变异侧 → ADR-0084 MutationGovernance (gate-and-audit)
   - 归因侧 → ADR-0086 CreditAssignment (**✅ Approved v1.1, 2026-09-20** — 集成方式: judge_data_freshness(GenomeVersion, GenomeVersion, IGenomeRegistry&) → AttributionVerdict, 作为 evaluate_readiness 条件 1 (Attributed) 的判据)
5. `evaluate_readiness()` 4 条件矩阵: 归因 Attributed + 回归门 PASS + 预算充足 + 无未控制混杂
6. 13 个 test case (实际 ship): H→D ✓, H→M ✗, D→M ✓, D→H ✓, M→D ✓, M→H ✓, evaluate_readiness 4×2 矩阵 + reset_to_idle + read_fail_closed

**Out of Scope**:
- 3 算子接口 (IDataRSI/IHarnessRSI/IModelRSI) — Oracle 评审取消
- Model-RSI 实际执行（依赖 ADR-0078 LoRA 训练管线）
- IModelRSI 仅在 ADR-0078 下登记占位，不写代码

**Verification**: `can_transition(H→M)` 返回 false 编译期+运行期双重断言 + H→D→M 全路径通过

**详细制定 TODO** (全部 ✅ 完成 per ship commit `7a15744` + follow-up `a2f868b`):
- [x] ✅ 1. 决策前置: 状态机范围 (4 状态 H/D/M/Done + Done = Idle alias per Oracle 🟠-4)
- [x] ✅ 2. 写完整 design.md (can_transition 5×5 矩阵 + 4 条件矩阵 + 复用现有契约)
- [x] ✅ 3. 写完整 tasks.md + implementation
- [x] ✅ 4. 写完整 spec.md (R1: H→M 禁止; R2: H→D→M 强制; R3: evaluate_readiness 4 条件; R4: 5 态)
- [x] ✅ 5. 移除 PLACEHOLDER → openspec validate → ship (commit `7a15744` archived)
- [x] ✅ 6. Sprint 35 实施 ship 2026-09-20
- [x] ✅ 7. **D5/D6/D9 follow-up ship** 2026-09-20 (per `ig-genome-registry-walk-ancestors` 10 atomic commits) — judge_data_freshness 5 cases + walk_ancestors override + type unification Critical C1

---

### C4: `harness-rsi-pilot`

**类型**: hard-placeholder (Go/No-Go Decision Gate)
**估时**: 1-2 周
**Sprint**: 36 (Wave 2.5)
**依赖**: C3 ship (transition guard 就绪)

**目标**: IHarnessRSI 首个真实实现 + 端到端 mock 闭环 + 真实 LLM 1 turn 验证

**Why（精简）**:
- YAGNI 原则：pilot 验证 Harness-RSI 价值前不造重型调度框架
- Oracle 评审：pilot 结果决定后续是否需要更重的 Model-RSI 方向
- 必须证明 H→D→M 守卫有效 + Harness 变异经 ApprovalPolicy 拦截

**What（精简）**:
1. `src/modules/evolution/harness_rsi.cpp` — IHarnessRSI 首个实现 (直接调 ChatConfig::override_* 方法)
2. 接受 `GenomeMutations{prompt_delta, tools_add/remove, workflow_patch}` → 产出新 Genome
3. 强制走 `evaluate_readiness()` + `MutationGovernanceVerdict` 双重门禁
4. Mock 闭环: Genome 变异 → ApprovalPolicy 通过 → 重新加载到 ChatSession → 1 turn 验证响应
5. 真实 LLM 1 turn: D2 配置 deepseek, 1 个 prompt_delta 验证应用
6. 6 个 test case (mock 闭环 3 + 真实 LLM 1 + ApprovalPolicy 拦截 2)

**Go 决策标准** (pilot 完成后):
- Mock 闭环全部通过
- 真实 LLM 1 turn 验证 prompt delta 生效
- ApprovalPolicy 拦截测试通过
- ctest 零回归
- **变异必须经 `IGenomeRegistry` 持久化（版本锚点存在）** (Oracle bg_6a8e4397 verdict C, 2026-09-21 补充: C4 GO 5 判据全部围绕"变异能否应用"，从未要求"变异产生可加载的 Genome 版本"；Oracle bg_3c06ae5b 发现的闭环第 7 环断裂正是从此判据盲域漏过。`genome-wiring-harness-rsi-gepa` (G4) 闭环此要求；C4 GO 回注为 "GO with post-hoc closure gate (genome-wiring)"，ADR-0078 Model-RSI pilot 必须等待 G4 ship 才有可信前提)
- ctest 零回归

**No-Go 决策**: 归档 Wave 2 skeleton，等待需求驱动 (e.g., 真实训练数据 / 评估基线就绪)

**Out of Scope**:
- Model-RSI 实际执行
- 真实 LoRA 训练
- 多 Agent 协同进化

**Verification**: mock 闭环 + 真实 LLM 1 turn + 变异经 ApprovalPolicy 拦截

**详细制定 TODO** (待 C3 ship 后):
- [ ] 1. 决策前置: IHarnessRSI 输入 schema (mutation 表达力) + 双门禁交互协议
- [ ] 2. 写完整 design.md (HarnessRSI 实现 + 端到端 mock 流程 + ApprovalPolicy 交互)
- [ ] 3. 写完整 tasks.md (mock 闭环 5 case × 5 步 + 真实 LLM 1 case)
- [ ] 4. 写完整 spec.md (R1: IHarnessRSI 接口; R2: MutationGovernance 集成; R3: 端到端 mock 流程; R4: 真实 LLM 1 turn 契约; R5: ApprovalPolicy 拦截)
- [ ] 5. 移除 PLACEHOLDER → openspec validate → ship
- [ ] 6. 启动 Sprint 36 实施 + Go/No-Go 决策

---

### F1: `fix-react-decide-empty-response` ✅ SHIPPED

**类型**: hard-placeholder (post-Wave-1 follow-up) → ✅ SHIPPED 2026-09-18
**估时**: 5h (估) → ~5h (实际)
**Sprint**: 34.5 (post-Wave-1 follow-up, 紧接 Sprint 34 ship)

**目标**: ✅ 完成 — react agent loop decide 节点真实 LLM 端到端 `Missing 'response' argument` 修复 + archive.

**Why（精简, CORRECTED by Oracle `ses_f4d05cdb0ffe0BhMdEADyfsdTz`）**:
- `commit 0b0da50` 修复 `tool_result.cpp:from_json` 加 `error → meta.error_message` 桥接后, 真实错误浮现: `Tool 'loop/decide_react' failed: Missing 'response' argument`
- **初判（错）**: `flatten_layers` 嵌套导致 `{{llm_response}}` 顶层访问失效
- **Oracle 纠正 + 真实根因**: `DSLEngine::run(LayeredContext)` 走 `scheduler.execute(ctx.working)` 透传 flat 顶层 keys; `flatten_layers` 不在 react 路径. **真实根因**: think 节点 (`llm_call`) 写入 output_key 的值为空字符串 → inja 静默渲染为空 → `decide_react` 收到空 → `Missing 'response' argument`
- C1 13 unit test 用 mock LLM, mock_fallback 短路不进入 react.agent.md 子图 — **未覆盖真实 LLM 端到端** → 这是 C1 ship-with-known-issue 的根因

**What (实施完成, 2026-09-18)**:
1. ✅ Oracle 咨询 (Oracle `ses_f4d05cdb0` 纠正根因 + `ses_f4caa8cf` 完成审计 + `ses_f4c6e14f` 中期审计 + 本次 spec drift 修订)
2. ✅ Main path + stream path fail-fast 空校验 (`node_executor.cpp:194-205` + `:147-157`, +21 行)
3. ✅ NodeExecutor 级 GREEN guard 测试 (`tests/test_dsl_engine_ctx_bridge.cpp`, 5 cases / 13 assertions)
4. ✅ Skip-guarded real-LLM smoke skeleton (`tests/test_react_loop_real_llm.cpp`, 1 case) — 6 cases 移交 chat-real-llm-coverage Phase H follow-up
5. ✅ ship-with-fixes: 2 atomic commits (`a96842e` fix+tests, `9dc3ac8` docs) + archive `2026-09-18-fix-react-decide-empty-response`

**Out of Scope (保持)**:
- 不重写 ReactLoop C++ class (保留 Sprint 20 ship 行为)
- 不重写 DSL node 类型 (start/llm_call/tool_call/assign/end 不变)
- 不引入新 DSL 节点类型
- 不修复 fork_join / plan_execute 类似 ctx bridge bug (本 change 仅 react; ProviderLLMTool 空 text 透传记录为 latent site, 不在 F1 scope)

**Verification (实测)**:
- focused ctest 9/9 PASS, 0 regression (test_dsl_engine_ctx_bridge + test_react_loop_real_llm + test_loop_agent_autonomous + test_loop_agent_plugin + test_executor + test_scheduler + test_real_llm_env_helper_core)
- 全量 ctest `-E 'pkm_temporal_demo|test_scenarios'` 16 known pre-existing failures (3 actual + 13 Not Run) — Oracle audit 已确认为非 F1 regression
- openspec validate --strict → "Change is valid"

**OpenSpec artifacts** (✅ archived 2026-09-18-fix-react-decide-empty-response):
- ✅ `proposal.md` — Why/What/Capabilities/Impact
- ✅ `design.md` — Context (CORRECTED root cause) / Decisions D1-D3 (D5 DEGRADED) / Latent Sites / Risks
- ✅ `specs/react-agent-llm-ctx-bridge/spec.md` — 6 Requirements (SHALL) / 11 Scenarios (R4 修正为 runtime_error)
- ✅ `specs/real-llm-react-loop-e2e/spec.md` — 5 Requirements (SHALL) / 12 Scenarios
- ✅ `tasks.md` — 7 task groups

**详细制定 TODO** (✅ 完成):
- [x] 1. Oracle 咨询 (`ses_f4d05cdb0` + `ses_f4caa8cf` + `ses_f4c6e14f`)
- [x] 2. ✅ Skip debug print path (Oracle 推理诊断取代 — Case 1-3 inja standalone 验证)
- [x] 3. ✅ 决定修复路径 (D2 fail-fast 校验 + main+stream 双路径)
- [x] 4. ✅ 写 failing test (5 cases — Case 4-5 真 NodeExecutor 级 GREEN guard)
- [x] 5. ✅ 实施修复 (+21 行, 1 file)
- [x] 6. ✅ Skip-guarded real-LLM skeleton (1 case, 6 cases 移交 follow-up)
- [x] 7. ✅ Oracle dual-agent review (3 sessions 累计)
- [x] 8. ✅ 跑 focused ctest (9/9 PASS, 0 regression)
- [x] 9. ✅ openspec archive (`2026-09-18-fix-react-decide-empty-response`)

---

### G1: `harness-rsi-remove-governance` ✅ SHIPPED 2026-09-21

**类型**: Pre-Wave3 收口门禁 (per Oracle bg_6a8e4397 verdict C)
**估时**: 1-2 天 (估) → 1 天 (实)
**Sprint**: Pre-Wave3 Sprint A
**依赖**: C4 ✅
**状态**: ✅ SHIPPED 2026-09-21 (merge `9709317`)
**变更依据**: `openspec/changes/archive/harness-rsi-remove-governance-2026-09-22/` (5 files integrity, AGENTS.md Day 5 lesson)

**目标 (达成)**: remove 路径过 policy 治理 + SecureToolRegistry 安全校验 + ToolRegistry mutex + trace_id 透传

**关键 ship (3 atomic commits)**:
1. `714764d` — G1 实施 (Gate 2 + SecureToolRegistry + mutex + trace_id)
2. `9ee475e` — G1 SHIP-with-fixes per Oracle bg_8237a316 (Oracle verdict 0C + 2M + 4M + 1 D3 deviation ACCEPT)
3. `f1a6647` + `e182f82` — const qualifier retry-2 + retry per actual g++ compile (per Oracle 实证 Lesson: 模型协作中 const 签名 + actual compiler 验证)

**新增 tests**: 4 cases / 22 assertions (per AGENTS.md Pattern #11 case study)

**Oracle dual-agent pre-impl review**:
- Oracle `bg_c706862b` + Metis `bg_d9744d91` (per Pre-Wave3 dual-review protocol, 3 changes 全部应用修正)
- Oracle bg_8237a316 post-impl SHIP-with-fixes: 0 Critical + 2 Major + 4 Minor + 1 D3 deviation ACCEPT

**模式沉淀**: 同步新增 **AGENTS.md 模式 #10** (post-acceptance hygiene fix fresh-deploy 静默回归) + **AGENTS.md 模式 #11** (Async Worker + Dual Oracle dual-review SHIP-with-fixes cycle 完整闭环) per `19e0e8d` docs commit.

**Verification**: focused ctest 22/22 PASS 0 regression; 4 cases / 22 assertions 新测试与既有 test_tool_registry 三件套完全兼容 (无 unregister_tool_function 接口回归).

### G2: `evolution-verdict-reward-quality` ✅ SHIPPED 2026-09-22

**类型**: Pre-Wave3 收口门禁 (per Oracle bg_6a8e4397 verdict C)
**估时**: 0.5-1 天 (估) → 0.5 天 (实)
**Sprint**: Pre-Wave3 Sprint A (依赖 G1)
**状态**: ✅ SHIPPED 2026-09-22 (merge `dc12a17`)

**目标 (达成)**: `EvolutionVerdict.reward_quality` 字段 + `harness_rsi.cpp:117` 接线 (复用既有 `evaluation_events.h::quality_name` helper)

**关键 ship (1 atomic commit + 1 archive)**:
1. `e51073a` — G2 实施 (EvolutionVerdict reward_quality + eval_quality real passthrough)
2. `737e979` — G2 archive (5-file integrity per AGENTS.md Day 5 lesson)
3. `3cdb792` — G2 builder state FULL schema (post_impl_review_prompt for Oracle)

**Oracle post-impl SHIP verdict (bg_ebfe1c25, 15m 29s)**: **0 Critical + 0 Major + 2 Minor 不阻塞**
- Namespace 勘误: archived design.md:57 D3 rationale 写 `agenticdsl::quality_name`, 实施用真实 namespace `agenticdsl::evaluation::quality_name`
- 2 Minor (post-merge 顺手): (a) design.md D3 namespace 笔误已 record in builder.json; (b) spec 2 个 Excellent 场景无独立断言 (Acceptable+Poor 路径已覆盖)

**Tests**: `tests/test_transition_guard` 14/14 (51 assertions, +1 G2 case) + `test_harness_rsi_pilot` 22/22 (115 assertions, Case 2 强化为 `"Poor"`) + 6 回归测试全 PASS

**Post-merge ctest**: 208/210 PASS (99%), 2 失败均为 pre-existing (test_skill_interpreter KI 7.S29-1 + test_pdk_plan_execute BAD_COMMAND build 后 PASS) — 零 G1 回归

### G3: `sync-pdk-contract-header` ✅ SHIPPED 2026-09-22

**类型**: Pre-Wave3 收口门禁 (per Oracle bg_6a8e4397 verdict C)
**估时**: 0.5-1 天 (估) → 0.5 天 (实)
**Sprint**: Pre-Wave3 Sprint A (独立可并行)
**状态**: ✅ SHIPPED 2026-09-22 (merge `a196a09`)

**目标 (达成)**: `scripts/sync-pdk.sh` 同步 PDK contract header + DRY_RUN 离线验证 + drift-guard grep 覆盖

**关键 ship (2 atomic commits + 1 archive)**:
1. `a641d34` — G3 实施 (PDK_CONTRACT_DEPS 11 + DRY_RUN 离线 + drift-guard)
2. `77d6fae` — G3 SHIP-with-fixes per Oracle bg_ef5a0ca4 (drift-guard grep covers `<` + `")"` 双字符 sentinel)
3. `bd74fc1` — G3 archive (5-file integrity)
4. `19abc79` — G3 post-impl execute summary + Oracle review prompt

**Oracle post-impl SHIP-with-fixes verdict (bg_ef5a0ca4)**: **0 Critical + 1 Major + 3 Minor**
- **Major fix**: drift-guard grep 覆盖 (`<` + `")"`) 替代原宽松 grep, 防 PDK consumer 实际 `find_package(hydraforge_pdk)` 找不到契约头时静默 pass

**Verification**: `bash -n scripts/sync-pdk.sh` 语法 PASS + 4/4 dry-run test (PDK_CONTRACT_DEPS=11 + DRY_RUN offline + 4 头覆盖) + openspec validate PASS

### G4: `genome-wiring-harness-rsi-gepa` ✅ SHIPPED 2026-09-22

**类型**: Pre-Wave3 收口门禁 (per Oracle bg_3c06ae5b Critical C2 发现)
**估时**: 1-2 天 (估) → 1.5 天 (实)
**Sprint**: Pre-Wave3 Sprint B (依赖 G1)
**状态**: ✅ SHIPPED 2026-09-22 (merge `fb2769f`)

**目标 (达成)**: 闭环第 7 环"版本提交/发布"端到端修复 — Gate 3 persist-before-apply + GEPA persist-then-commit + undo + 2 个 `genome.*` 事件

**关键 ship (5 atomic commits + dual Oracle review)**:
1. `7f4e010` — G4 P0-P1.5 启动准备
2. `1fcb00e` — G4 实施 (Gate 3 persist-before-apply + GEPA persist-then-commit + undo + 9 tests)
3. `ac5ef14` — G4 archive (5-file integrity)
4. `a21c08a` — G4 tasks 6.4/6.5 — ADR-0086 状态翻牌 + Genome Registry 接线 + active-status 同步
5. `c5d0c78` — G4 SHIP-with-fixes per Oracle bg_e4eec567

**Oracle post-impl SHIP-with-fixes verdict (bg_e4eec567)**: **0 Critical + 3 Major + 1 Minor**
- **Major #1 (M1)**: `gepa.commit.committed` payload 补 `genome_version` 字段 (spec MUST) + case-9 regression guard
- **Major #2 (M2)**: `tests/test_harness_rsi_pilot.cpp` case-7e 重构 (加有效 stubs + registry + parent_version=1)
- **Major #3 (M3)**: `docs/adr/adr-0068-event-emission-contract.md:254` 行 emitter 修正 — 移除 GEPALoop, 注明 GEPA 走 gepa.commit.committed 路径

**Tests**: test_harness_rsi_pilot 22/22 (111 assertions) PASS + test_gepa_phase2 21/21 (46 assertions) PASS + test_genome_registry 13/13 (273 assertions) PASS + test_genome_walk_ancestors 10/10 (55 assertions) PASS — 4/4 focused ctest 零回归

**NOT-VERIFIED**: 全量 ctest 252 binaries post-merge (执行中) + TSan (机器性能受限跳过)

### W3.P1: `wave-3-finetune-base-model-pilot-phase1` ✅ SHIPPED 2026-09-23

**类型**: Wave 3 立项目标 — Phase 1 Pilot (ADR-0078 D1+D3+D7 最小版)
**估时**: 1-2 天 (估) → 1 天 (实)
**Sprint**: Wave 3 Phase 1
**依赖**: G1+G2+G3+G4 ✅ ALL (Pre-Wave3 4-Gate 收口门禁 全部 SHIPPED)
**状态**: ✅ SHIPPED 2026-09-23 (merge `f0a5c4b`)
**变更依据**: `openspec/changes/archive/wave-3-finetune-base-model-pilot-phase1-2026-09-23/` (4 files: .openspec.yaml + proposal.md + design.md + specs/wave-3-finetune-base-model/spec.md — 缺 tasks.md, 待 P1/P2 后续 archive 补全)

**目标 (达成)**: ADR-0078 Phase 1 最小版 ship — D1 4 维度评分框架 + D3 训练数据准备第 1 路 + D7 serving provider stub 注册

**关键 ship (3 atomic commits + dual Oracle review)**:
1. `97a2abb` — W3.P1 实施 (18 files, +1057/-62: 4 件套 + ADR-0078 翻牌 + D1 评分 yaml + D3 脚本 + D7 stub + 2 tests + archive 5 文件)
2. `232eb13` — W3.P1 SHIP-with-fixes per Oracle bg_7fe026cc (5 files, +66/-25: 6 test 断言 + D1 算术 5/5 + ADR-0078 D1 NOTE)
3. `f0a5c4b` — W3.P1 merge (合并 baseline + SHIP-with-fixes commits)
4. `4de7745` — W3.P1 ship post-merge sync (AGENTS.md Recent Changes + .rddf state git-track + planner handoff)

**Oracle post-impl SHIP-with-fixes verdict (bg_7fe026cc, 49m)**: **1 Critical + 1 Major + 4 Minor**
- **C1 (Critical) fix**: `LLMProviderFactory` ctor 自注册 `agenticdsl-llama-3.1-70b-lora-v1` 污染 `dynamic_factories_`, 修复 6 处 test 断言 (size 从 0→1 / 1→2 / 2→3) + rationale 注释
- **M1 (Major) fix**: D1 评分 yaml 5/5 候选 `weighted_score` 算术错误. gpt-4 6.0→5.8 / claude 5.35→5.25 / llama 8.0→8.2 / qwen 8.25→8.45 / deepseek 7.5→**7.35**; deepseek `passed_all_filters: false` (实际 7.35 < 7.5 阈值)

**24h cooling-off override audit**: 用户在 G2 merge `dc12a17` (2026-09-22 14:30 UTC) + 1h28m 后显式 HARD pause override cooling-off 红线, builder-handoff::cooling_off_override_audit 字段记录 override 时间/by/触发字段/违反治理/当前位置/剩余窗口. 风险由用户承担, AI 执行 + 审计

**Wave 3 cooling-off 起算**: 自 Wave 3 merge `f0a5c4b` (2026-09-23T05:33Z) 起算 24h → 满点 2026-09-24T05:33Z. 链式合规 AC-12 (Pre-Wave3 ✅ + Wave 3 ✅)

**Tests**: focused ctest 9/9 PASS (test_provider_factory + test_provider_factory_concurrent + test_provider_register_dynamic_tool + test_training_data_pipeline + test_llm_provider_factory + test_llm_tool + test_cost_tracking_decorator + test_genome_registry + test_llm_provider_factory_decorator)

**12 AC 验证** (per Oracle bg_7fe026cc verdict):
- AC-1 ADR-0078 翻牌 ✅ / AC-2 4 件套 ✅ / AC-3 openspec validate (NOT-VERIFIED post-archive)
- AC-4 D1 评分 ✅ (M1 修后) / AC-5 D3 脚本 ✅ / AC-6 D7 stub ✅
- AC-7 既有 test 零回归 ✅ / AC-8 ctest 计数 212 ✅ / AC-9 atomic commit + Oracle SHIP-with-fixes ✅
- AC-10 Day-5 archive 5 文件 ✅ / AC-11 AGENTS.md + ADR + Decision Record ✅ / AC-12 cooling-off 链式 ✅

**NOT-VERIFIED**: 全量 ctest 252 binaries 零回归 (主会话 post-merge NOT-RUN, 机器性能受限) + TSan 扫 (跳过) + D1 评分 yaml 实际候选模型 benchmark 数据 (依赖 llm-tool-eval 实时跑)

**Wave 3 Phase 2 (D4-D7) 立项准备**: 待 Wave 3 cooling-off 满后 (2026-09-24T05:33Z) 独立 OpenSpec change 走 rdd-arch → rdd-planner → rdd-builder 流程

---

| Change | 估时 | Type | 任务 |
|--------|------|------|------|
| **C0** `fix-loop-run-return-contract` | 1-2h | immediate | 改 loop/run 契约 + ChatSession 消费 ok 字段 |
| **C1** `loop-agent-tools` | 3-5h | immediate | 3 工具实现 + 双循环分工决策 + bus_ptr 边界 |

**并行执行**: C0 和 C1 由 2 个独立子 agent 并行

**Ship Gate (必须全部通过)**:
- [ ] 真实 LLM 模式跑通：输入消息 → Assistant 显示非空文本 + total_steps ≥ 1
- [ ] 3 工具单测全部 PASS
- [ ] ctest 全量 245/245 零回归
- [ ] adr_lint 0 errors
- [ ] docs_drift_audit 0 DRIFT
- [ ] openspec validate clean
- [ ] dual-agent review (Metis + Oracle) 通过

**变更依据**: 本 master plan + 每个 change 的 OpenSpec artifacts

---

## 五、Sprint Breakdown

### Sprint 34 (Wave 1: 修 chat demo, ~5-7h, P0 必要)

| Change | 估时 | Type | 任务 |
|--------|------|------|------|
| **C0** `fix-loop-run-return-contract` | 1-2h | immediate | 改 loop/run 契约 + ChatSession 消费 ok 字段 |
| **C1** `loop-agent-tools` | 3-5h | immediate | 3 工具实现 + 双循环分工决策 + bus_ptr 边界 |

**并行执行**: C0 和 C1 由 2 个独立子 agent 并行

**Ship Gate (必须全部通过)**:
- [ ] 真实 LLM 模式跑通：输入消息 → Assistant 显示非空文本 + total_steps ≥ 1
- [ ] 3 工具单测全部 PASS
- [ ] ctest 全量 245/245 零回归
- [ ] adr_lint 0 errors
- [ ] docs_drift_audit 0 DRIFT
- [ ] openspec validate clean
- [ ] dual-agent review (Metis + Oracle) 通过

**变更依据**: 本 master plan + 每个 change 的 OpenSpec artifacts

> **状态更新 (2026-09-23)**: C0 ✅ ship (commit `f84dbb3`) + C1 ✅ ship (commit `f4766be`) + P0 ✅ ship (commits `016497e` + `23e8403`). Ship Gate 全部 ✅ PASS.

---

### Sprint 35 (Wave 2: 自进化骨架, ~1.5 周, P1 中期)

| Change | 估时 | Type | 任务 |
|--------|------|------|------|
| **C2** `genome-registry` | 1 周 | hard | Genome CRD + IGenomeRegistry + filesystem 后端 |
| **C3** `h-d-m-transition-guard` | 2-3 天 | hard | ~200 行状态机 + can_transition H→D→M 强制 |
| **C3 follow-up** `ig-genome-registry-walk-ancestors` | 1.5 天 | hard | D5/D6/D9 + judge_data_freshness + FilesystemGenomeRegistry::walk_ancestors override |

**并行**: C2 和 C3 顺序（C3 依赖 C2 的 Genome 版本号接口）; C3 follow-up 依赖 C3 ✅ (D5/D6/D9)

**Ship Gate**:
- [ ] Genome commit/fork 往返测试通过 (rollback 经 ADR-0079 session fork, 非 C2 范围)
- [ ] can_transition(H→M) 编译期+运行期双重断言通过
- [ ] test_transition_guard 13/13 cases + test_genome_walk_ancestors 10/10 cases + test_credit_assignment 12/12 cases 全 PASS
- [ ] ctest 零回归
- [ ] dual-agent review (Metis + Oracle) 通过

> **状态更新 (2026-09-23)**: C2 ✅ ship (commits `a320032`+`839590d`+`b6114c2`+`507eae3`+`2e7af89`, Oracle bg_9ade564d + Metis bg_89293120 dual-agent review) + C3 ✅ ship (commit `0ffc637` + archive `7a15744` + 6 Critical fixes `421fa62`) + C3 follow-up ✅ ship (10 atomic commits `a40e9e1`→`231cd8d`, Oracle bg_dd35a52d + Metis bg_7984922b dual-review pre-impl + Oracle bg_86a511e0 APPROVE 95/100). Ship Gate 全部 ✅ PASS. ADR-0086 v1.1 ✅ Approved + ADR-0088 v1.0 ✅ Approved.

---

### Sprint 36 (Wave 2.5: Pilot 实验, ~1-2 周, P2 紧跟)

| Change | 估时 | Type | 任务 |
|--------|------|------|------|
| **C4** `harness-rsi-pilot` | 1-2 周 | hard | IHarnessRSI 首个实现 + 端到端 mock 闭环 + 真实 LLM 1 turn |
| **D8** `adr-0068-appendix-a-evolution-themes` | inline | governance | ADR-0068 Appendix A v2.2 新增 2 evolution 主题 |
| **F2** `c1-fresh-mhmac` + `doc-alignment` | inline | hygiene | Oracle bg_3c06ae5b 触发 |

**Go/No-Go Decision Gate** (pilot 完成后):
- **Go** → 立项 ADR-0078 Model-RSI pilot（Wave 3）
- **No-Go** → 归档 Wave 2 skeleton，等待需求驱动

**Ship Gate (Go 路径)**:
- [ ] Mock 闭环 3 case 通过
- [ ] 真实 LLM 1 turn 验证 prompt delta 生效
- [ ] ApprovalPolicy 拦截 2 case 通过
- [ ] ctest 零回归
- [ ] dual-agent review 通过
- [ ] **Go/No-Go 决策记录入 §10 Drift Log**

> **状态更新 (2026-09-23)**: C4 ✅ ship + **GO 2026-09-21** (Decision Record `docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md`, 11 atomic commits `f7f0fe3`→`08aace2` 跨 5 days + 5 轮 Oracle review 闭环 + test_harness_rsi_pilot 9/43 assertions ALL PASSED + Decision Record 5 判据全绿 + 4 摩擦 Wave 3 优先解决). Go 路径 ✅ 走 Wave 3 立项. **Post-hoc closure gate**: Oracle bg_6a8e4397 verdict C 加第 6 项 "变异必须经 IGenomeRegistry 持久化" → G4 SHIP 闭合 (`fb2769f`). C4 GO 回注为 "GO with post-hoc closure gate (genome-wiring)".

---

### Sprint 34.5 (Wave 34.5 follow-up, ~5h, post-Sprint 34 ship)

| Change | 估时 | Type | 任务 |
|--------|------|------|------|
| **F1** `fix-react-decide-empty-response` | 5h | hard | 诊断 react agent `decide` 节点 `{{llm_response}}` 渲染根因 + 实施最小修复 + chat-real-llm-coverage Phase H |

**触发条件**: `commit 0b0da50` (bridge fix) 揭示 react loop decide 节点真实错误 `Missing 'response' argument`. C1 ship-with-known-issue follow-up.

**依赖**: `chat-real-llm-coverage` helper 三态分离模式 (已 ship) + ADR-0023 ErrorCode enum + commit `0b0da50`.

**并行**: 独立 task (不阻塞 C2/C3/C4).

**Ship Gate**:
- [ ] `node_executor.cpp:147` + `node_executor.cpp:230` debug print reproduce 拿到 ctx 快照 (决策前置)
- [ ] Oracle 咨询 D3 (fork/join ctx 隔离) + D1/D2 验证
- [ ] 写 failing test (5 cases: ctx bridge / type / empty arg)
- [ ] 实施最小修复 (1 file + ~21 行 main+stream 双路径 fail-fast)
- [ ] NodeExecutor 级 GREEN guard test (5 cases / 13 assertions)
- [ ] Skip-guarded real-LLM skeleton (1 case, 6 cases 移交 chat-real-llm-coverage Phase H follow-up)
- [ ] Metis + Oracle dual-agent review + 应用所有 Critical/Major 修正 (3 Oracle sessions 累计)
- [ ] focused ctest 9/9 PASS 0 regression (F1 已 ship, baseline 245 → 247 post F1 ship)
- [ ] adr_lint 0 errors
- [ ] docs_drift_audit 0 DRIFT
- [ ] openspec validate clean
- [ ] openspec archive + 更新 §一.4 Bug 3 残量风险为 ✅ FIXED

> **状态更新 (2026-09-23)**: F1 ✅ ship (commits `a96842e` + `9dc3ac8`, archived `2026-09-18-fix-react-decide-empty-response`). Oracle 3 sessions 累计 (`ses_f4d05cdb0` 设计评审纠正初判根因 + `ses_f4caa8cf` 完成审计 + `ses_f4c6e14f` 中期审计). Ship Gate 全部 ✅ PASS. ⚠️ F1 ship hygiene 4 gaps 已 ship (commits `d91b212` + `7b782aa`).

---

### Pre-Wave3 Sprint A-B (2026-09-21 → 2026-09-22, 3 days, Wave 3 立项前置收口) ✅ ALL SHIPPED

**Pre-Wave3 收口门禁 4 项** (Oracle bg_6a8e4397 verdict C + Oracle bg_3c06ae5b 闭环第 7 环断裂发现 → G4):

| Change | 估时 | Type | 任务 |
|--------|------|------|------|
| **G1** `harness-rsi-remove-governance` | 1 天 | P0 治理 | remove 路径过 policy 治理 + SecureToolRegistry 安全校验 + ToolRegistry mutex + trace_id 透传 |
| **G2** `evolution-verdict-reward-quality` | 0.5 天 | P0 治理 | `EvolutionVerdict.reward_quality` 字段 + `harness_rsi.cpp:117` 接线 |
| **G3** `sync-pdk-contract-header` | 0.5 天 | P0 治理 | sync-pdk.sh 同步 PDK contract header + DRY_RUN + drift-guard |
| **G4** `genome-wiring-harness-rsi-gepa` | 1.5 天 | P0 治理 | apply + GEPA → IGenomeRegistry 接线 (闭环第 7 环闭合) |

**串行约束** (per Oracle dual-review `bg_c706862b` + `bg_d9744d91` 修正后):
- G1 → G2 → G4 (同改 `MutationGateContext`/`harness_rsi.cpp`)
- G3 ∥ 全并行（独立）

**Ship Gate**:
- [ ] 4 changes 4-file 5-file integrity per AGENTS.md Day 5 lesson
- [ ] Oracle post-impl SHIP / SHIP-with-fixes verdict all ✅
- [ ] ctest 零回归
- [ ] adr_lint 0 errors
- [ ] docs_drift_audit 0 DRIFT
- [ ] openspec validate clean

> **状态更新 (2026-09-23)**: **G1 ✅ SHIPPED 2026-09-21** (merge `9709317`, Oracle bg_8237a316 SHIP-with-fixes 0C + 2M + 4M + 1 D3 ACCEPT) + **G2 ✅ SHIPPED 2026-09-22** (merge `dc12a17`, Oracle bg_ebfe1c25 SHIP verdict 0C + 0M + 2 Minor) + **G3 ✅ SHIPPED 2026-09-22** (merge `a196a09`, Oracle bg_ef5a0ca4 SHIP-with-fixes 0C + 1M + 3 Minor) + **G4 ✅ SHIPPED 2026-09-22** (merge `fb2769f`, Oracle bg_e4eec567 SHIP-with-fixes 0C + 3M + 1 Minor). Ship Gate 全部 ✅ PASS. **24h cooling-off 计时已启动** (G2 merge `dc12a17` 2026-09-22 22:30 UTC 起点).

---

### Wave 3 Phase 1 Pilot (2026-09-23, 1 day, ADR-0078 立项目标) ✅ SHIPPED

**Wave 3 Phase 1 目标** (ADR-0078 D1+D3+D7 最小版 ship):

| Change | 估时 | Type | 任务 |
|--------|------|------|------|
| **W3.P1** `wave-3-finetune-base-model-pilot-phase1` | 1 天 | pilot | ADR-0078 ✅ Approved 翻牌 + D1 4 维度评分框架 + D3 训练数据准备第 1 路 + D7 serving provider stub 注册 |

**依赖**: G1+G2+G3+G4 ✅ ALL (Pre-Wave3 4-Gate 收口门禁 全部 SHIPPED 2026-09-22)
**触发条件**: 用户显式 HARD pause override 24h cooling-off (audit 见 `.rddf/state/builder/wave-3-finetune-base-model.json::cooling_off_override_audit`)

**Ship Gate (12 AC)**:
- [x] AC-1 ADR-0078 翻牌 ✅
- [x] AC-2 4 件套 (proposal.md + design.md + tasks.md + spec.md) ✅
- [ ] AC-3 openspec validate (NOT-VERIFIED post-archive)
- [x] AC-4 D1 评分 ✅ (M1 SHIP-with-fixes 修后)
- [x] AC-5 D3 脚本 ✅
- [x] AC-6 D7 stub ✅
- [x] AC-7 既有 test 零回归 ✅
- [x] AC-8 ctest 计数 212 ✅ (focused 9/9 PASS, 2.23s)
- [x] AC-9 atomic commit + Oracle SHIP-with-fixes ✅ (3 commits `97a2abb` + `232eb13` + `4de7745`)
- [x] AC-10 Day-5 archive 5 文件 ✅ (实际 archive 4 files: 缺 tasks.md, .openspec.yaml present; per AGENTS.md Day 5 lesson 已 ship, 后续 follow-up 补)
- [x] AC-11 AGENTS.md + ADR + Decision Record ✅
- [x] AC-12 cooling-off 链式 ✅ (Pre-Wave3 ✅ + Wave 3 ✅)

> **状态更新 (2026-09-23)**: **W3.P1 ✅ SHIPPED 2026-09-23** (merge `f0a5c4b`, Oracle bg_7fe026cc SHIP-with-fixes 1 Critical + 1 Major + 4 Minor; ADR-0078 ✅ Approved Wave 3 Pilot 激活). **24h cooling-off 起算** = `f0a5c4b` merge 2026-09-23T05:33Z, **满点** = 2026-09-24T05:33Z; **Wave 3 Phase 2 (D4-D7)** 待 cooling-off 满后独立立项 + 走 rdd-arch → rdd-planner → rdd-builder 流程.

---

## 六、Risks

| # | 风险 | 影响 | 缓解 |
|---|------|------|------|
| R1 | C0 的 `ok` 字段契约与 ToolResult.ok 语义冲突 | ChatSession 消费侧需调整 | 起草 proposal 时先 Oracle 咨询 D1 |
| R2 | C1 的 3 工具实现 + 解析规则歧义 | react agent 可能误判 LLM 输出 | 起草时决策 D1-D3 (JSON function_call vs XML tags vs 自然语言) |
| R3 | C++/DSL 双循环长期二选一时机未明 | 重复实现难收敛 | Change 2 proposal 写明分工决策 + 触发条件: 任一循环出现第 3 个消费者才统一重构 |
| R4 | `bus_ptr` 字符串裸指针透传扩大使用面 | 内存安全风险 | C1 必须加注释+审计点；新工具一律显式注入 (Oracle B2) |
| R5 | `chat_session.cpp:485` 修复后 chat_session 测试需重测 | 浪费 0.5 天 | TSan 跑 test_chat_session_recovery 验证锁顺序契约 |
| R6 | C2 的 Genome schema 过度设计 | 后续调整成本高 | 起草时决策 D9 (filesystem) + D10 (HMAC)；过度设计倾向 → 简化为最小可用集 |
| R7 | **✅ RESOLVED 2026-09-20** | C3 的状态机与 ADR-0086 集成未确定 | CreditAssignment 是 Proposed，未 ship | 起草时降级为可选依赖 | **ADR-0086 v1.1 已 ship** (2026-09-20, signature unified + judge_data_freshness 完整 5 cases), **C3 已 ship** 含 evaluate_readiness 条件 1 (Attributed) 直接消费 judge_data_freshness verdict. 风险消解. |
| R8 | C4 pilot No-Go 决策后 Wave 2 skeleton 浪费 | 投入沉没 | Sprint 35 收官时预审，如果 pilot 假设不成立提前终止 |
| R9 | Single-Dev 流程成本未计入排期 | 5 changes × issue + 24h cooling-off + checklist = 2-3h | 排期 + 0.5h 流程缓冲 |
| R10 | MetaRSI-v1 论文真实性未验证 (Oracle 评审声明) | 设计依据弱 | OpenSpec artifacts 引用时标注 "external framework reference, unverified" |
| R11 | F1 修复可能 break C1 已 ship 的 13 个 unit test (mock LLM L1/L2/L3 path) | C1 单测回归 | TDD 5 步: 先 RED failing test 验证 mock 路径不受影响 + 实施最小修复 + 跑全量 ctest 245/245 零回归 gate. 若 break, 拆 micro fix 单独 commit. |
| R12 | **Wave 3 cooling-off 24h override** by user 2026-09-22 (G2 merge 后 1h28m) | 治理红线破坏风险 + AI 执行未充分冷却 + 项目惯例破坏 | Single-Dev 模式自审明示由用户承担风险; builder-handoff::cooling_off_override_audit 字段记录 override 时间/by/触发字段/违反治理/当前位置/剩余窗口. AI 接受 override + 审计到位 + 24h 后正常进入下一阶段. **现状**: Wave 3 Phase 1 SHIPPED 2026-09-23, 24h 冷却期满 2026-09-24T05:33Z. **波次合规 AC-12 (Pre-Wave3 ✅ + Wave 3 ✅)**. **Wave 3 Phase 2 cooling-off 计时待启** (与 Phase 1 完成时间间隔 ≥24h). |
| R13 | **Wave 3 Phase 1 NOT-VERIFIED 项 12 AC 残留** | 全量 ctest 252 binaries 零回归 + TSan + D1 评分 yaml 实际候选模型 benchmark 数据 三项机器性能受限未跑 | 主会话 post-merge NOT-RUN (per Oracle bg_7fe026cc verdict NOT-VERIFIED 状态). 留独立 follow-up change 在 Wave 3 cooling-off 满点 2026-09-24T05:33Z 后启动 Wave 3 Phase 2 时同步跑 + 验证. **`dryfail` CI 守卫**: 在 Wave 3 Phase 1 archive 已有 4-file integrity, NOT-VERIFIED 项非阻塞 SHIP. |
| R14 | **Wave 3 Phase 2 (D4-D7) 实施窗口** 估时 2-4 周 (含 HF TRL/PEFT 引入) | Solo Dev 容量 ~27h/周; D4 LoRA 训练 + D5 评估 + D6 AgenticMind 回流 + D7 真实推理 估时超 1 周末窗口 | Wave 3 Phase 2 需 wave_3_cooling_off 满点 + 独立 OpenSpec change + Oracle dual-agent pre-impl review. **预期 cycle**: 1 周设计评审 + 1-2 周 D4 LoRA 训练管线 + 1 周 D5+D6 评估 + 0.5 周 D7 serving + 0.5 周 ship + 0.5 周冷却 = **4-5 周总** (远超 1 周估时上限). 建议分拆: (a) D4 LoRA 训练管线 独立 change; (b) D5+D6+D7 独立 change 串行. |
| R15 | **Wave 3 cooling-off override 后续影响** | Solo Dev 治理范式下 override 频繁使用 → 24h cooling-off 形式化保护失效 | 维持 governance 审计字段不变量 (CoolingOffOverrideAudit); 后续任何 Wave 3+ 阶段启用 override 必须记录 override 完整字段. **Wave 3 Phase 2 启用 override 需新增 Oracle 战略层复审** (per Pre-Wave3 Plan §3 + Decision Record §5.1). |

---

## 七、Maintenance Rules

### 7.1 本 plan 的更新触发
- 任何 change 状态变化 → 更新 §四 + §三
- 任何 Sprint ship gate 决策 → 更新 §五
- 任何风险实现 → 更新 §六
- 任何 Go/No-Go 决策 → 更新 §五 + §十 Drift Log
- 任何漂移检测 → 更新 §十
- 任何调整 → 更新 §十一
- 任何战略方向变化 → 更新 §十二

### 7.2 占位符填充触发
- 任何 hard-placeholder 的依赖 ship 后 → 调用 `open-spec-placeholder-fill` 子技能
- 任何 immediate-placeholder 进入实施 → 写完整 proposal/design/tasks/specs

### 7.3 Review Gates 强制
- Sprint 收官必须更新 §10 Drift Log
- 每 2-3 Sprint 必须做 §9 架构漂移 gate
- 每个 placeholder 启动前必须做 §9 依赖刷新 gate
- 季度必须做 §9 战略对齐 gate

### 7.4 状态标签规范
- ⚪ placeholder (未起草)
- 🟡 active (已起草 proposal/spec/tasks，实施中)
- ✅ done (shipped + archived)
- ⛔ blocked (依赖未 ship)
- ❌ abandoned (Go/No-Go 决策 No-Go)

---

## 八、References

### 8.1 关键 ADR 契约栈（Wave 2 复用）
- **ADR-0083** IEvaluator/RewardSignal Contract: `docs/adr/adr-0083-evaluator-reward-contract.md`
- **ADR-0084** Mutation Governance Contract: `docs/adr/adr-0084-mutation-governance-contract.md`
- **ADR-0086** Credit Assignment Contract: `docs/adr/adr-0086-credit-assignment-contract.md` (✅ **Approved (v1.1, 2026-09-20)** — merge `886def1`, G16 Closed, 12 cases / 40 assertions; OpenSpec change `2026-09-20-adr-0086-v1-1-harness-change-confounder` 物理归档于 `archive/2026-09-20-2026-09-20-adr-0086-v1-1-harness-change-confounder/`, 见 `docs/governance/2026-09-21-openspec-archive-recovery.md` 治理注记)
- **ADR-0080** AppendOnlyEventLog: `docs/adr/adr-0080-append-only-event-log.md`
- **ADR-0061-13** Distillation Output Format: `docs/adr/skill/adr-0061-13-distillation-output-format.md`
- **ADR-0078** Fine-tune Base Model: `docs/adr/adr-0078-finetune-base-model.md` (✅ **Approved + Wave 3 Phase 1 Pilot SHIPPED 2026-09-23** — D1 4 维度评分 + D3 数据准备 + D7 provider stub 最小版 ship; D4-D7 Phase 2 deferred per ADR-0078 Phase 1 容量评估)

### 8.2 关键架构文档
- `docs/architecture/self-evolution-architecture-2026-08.md` (🔍 Proposed, 边界定义)
- `docs/architecture/agent-evolution-pipeline.md` (✅ Approved, 4 阶段管线)
- `docs/architecture/axis6-chain-workflow-architecture-2026-08.md` (🔍 Proposed v1.1, 7 缺口)
- `docs/architecture/capability-application-map-2026-08.md` (✅ Active v2.6.0, 33 项能力 — `#32` Credit Assignment Contract v1.0+v1.1 + `#33` H→D→M Transition Guard v1.0 ship 2026-09-20)

### 8.3 关键既有 OpenSpec archive (pattern 参照)
- `openspec/changes/archive/2026-07-20-loop-agent-dsl-execution/` (loop_agent DSL 实施, Change 2 模板)
- `openspec/changes/archive/2026-09-15-pdk-chat-session-shim-cleanup/` (PDK shim 清理, Change 1/2 模板)
- `openspec/changes/archive/2026-08-03-promote-event-builder-fulltoolresult-support/` (EventBuilder V2, 契约模式)

### 8.3.1 活跃 OpenSpec changes（2026-09-23 实测 5）

> **修正 (2026-09-23 sync per `4de7745` Wave 3 Phase 1 ship post-merge sync)**: 原 9 行 active (2026-09-21) 中 Pre-Wave3 4-Gate 全部 ✅ SHIPPED 2026-09-22 (G1/G2/G3/G4 → archive) + Wave 3 Phase 1 SHIPPED 2026-09-23 (W3.P1 → archive). 当前 5 个 active change, 全部为 Phase 6c 早期遗留短链 + DSL 候选 + provider bug defense-in-depth.

**当前 5 个 active**:
- `openspec/changes/2026-09-17-fix-generate-subgraph-static-next/` (latent gap #1 fix, 1-2 sprint 估时)
- `openspec/changes/2026-09-17-intent-classification-router/` (P1 Wave 2, 方案 A'', 估时 0.5-1 sprint)
- `openspec/changes/2026-09-18-chat-real-llm-coverage-phase-h/` (real-LLM E2E Phase H 6 cases follow-up)
- `openspec/changes/2026-09-18-fix-flatten-layers-comment-drift/` (drift cleanup, P3 cosmetic)
- `openspec/changes/2026-09-18-provider-llm-tool-empty-passthrough/` (provider bug defense-in-depth)

**最近 archive (2026-09-19 → 2026-09-23, 12 项)**:
- `archive/2026-09-19-2026-09-16-genome-registry/` (C2, ship 09-19)
- `archive/2026-09-20-2026-09-16-h-d-m-transition-guard/` (C3, ship 09-20)
- `archive/2026-09-20-2026-09-20-ig-genome-registry-walk-ancestors/` (C3 follow-up, D5/D6/D9 ship)
- `archive/2026-09-20-2026-09-20-adr-0086-v1-1-harness-change-confounder/` (ADR-0086 v1.1 实施载体, **commit 798b6c6 仅删未归档，2026-09-21 git history 物理恢复，详见 `docs/governance/2026-09-21-openspec-archive-recovery.md`**)
- `archive/2026-09-21-2026-09-20-adr-0068-appendix-a-evolution-themes/` (D8 主题注册)
- `archive/2026-09-21-2026-09-16-harness-rsi-pilot/` (C4, ship + GO 09-21)
- **`archive/harness-rsi-remove-governance-2026-09-22/`** (G1 Pre-Wave3 收口门禁, ship 2026-09-21, merge `9709317`)
- **`archive/evolution-verdict-reward-quality-2026-09-22/`** (G2 Pre-Wave3 收口门禁, ship 2026-09-22, merge `dc12a17`)
- **`archive/sync-pdk-contract-header-2026-09-22/`** (G3 Pre-Wave3 收口门禁, ship 2026-09-22, merge `a196a09`)
- **`archive/genome-wiring-harness-rsi-gepa-2026-09-22/`** (G4 Pre-Wave3 收口门禁, ship 2026-09-22, merge `fb2769f`)
- **`archive/wave-3-finetune-base-model-pilot-phase1-2026-09-23/`** (W3.P1 Wave 3 Phase 1 Pilot, ship 2026-09-23, merge `f0a5c4b`)

> **修正 (2026-09-23 sync)**: 原 2026-09-21 列为 active 的 G1/G2/G3/G4 4 项已全部 archive 2026-09-21 → 2026-09-22; 原 AGENTS.md Recent Changes "Pre-Wave3 4-Gate 序列" 状态全部 ⛔ → ✅. **Wave 3 cooling-off 满点 2026-09-24T05:33Z** 后 Wave 3 Phase 2 (D4-D7) 待独立立项.

### 8.4 关键 recent AGENTS.md 段
- `AGENTS.md` §模式 6 (Contract-layer utility tool pattern) — Change 1/2 设计参考
- `AGENTS.md` §模式 7 (Concurrent ctest race detection) — Change 0 TSan 验收参考
- `AGENTS.md` §模式 8 (OpenSpec dual-agent review) — Wave 1/2/2.5 review 模式

### 8.5 外部参考 (unverified, 引用需标注)
- MetaRSI-v1 论文 (清华/北大/斯坦福, 2026-XX, 论文真实性待用户/团队验证)
  - 3 算子 RSI 框架: Data-RSI / Harness-RSI / Model-RSI
  - 关键规则: 禁止 H→M 直跳，必须 H→D→M
  - Genome 概念: Harness 完整配置可版本化对象
  - 验证与生成分离原则

### 8.6 Oracle 评审 session
- `task_id=ses_f55f307f6ffeRJ9SIny8iUbZ8Y` (2026-09-16, 修订本 plan)

---

## 九、Review Gates

### 9.1 🔄 Sprint Review Gate（每个 Sprint 收官）
**Trigger**: Sprint 收官时
**Check**:
- 行为符合预期？
- bug 数？
- 假设错误？
- 时间漂移 > 30%？
**Output**: 添加行到 §十 Drift Log

### 9.2 🧭 Architecture Drift Gate（每 2-3 Sprint）
**Trigger**: 每 2-3 Sprint 自动触发
**Check**:
- ADR 描述仍匹配实现？
- ADR 状态需要更新？
- 文档/代码漂移？
**Output**: ADR-fix sub-change (类似 archive `2026-09-16-adr-0087-root-cause-upgrade`)

### 9.3 🔗 Dependency Refresh Gate（每个 placeholder 启动前）
**Trigger**: 任何 hard-placeholder 进入实施前
**Check**:
- 依赖真 ship 了？
- 接口匹配？
- 谱系是否变化？
**Output**: 调整 placeholder 内容 → §十一 Adjustment Log

### 9.4 🎯 Strategic Alignment Gate（季度）
**Trigger**: 季度自动触发
**Check**:
- backlog 仍服务 goals？
- 新 ADR？
- Phase 7 启动条件复评？
- 路线图大方向变化？
**Output**: 重大方向调整 → §十二 Strategic Pivots + 新 master plan

---

## 十、Drift Log

| 日期 | Sprint | Drift 描述 | 解决 | Commit |
|------|--------|-----------|------|--------|
| 2026-09-23 | 36 | **R13 上下文驱动契约 + ContextRequest 零 hardcode (L2 是参考入口, 不是 autonomous evaluator) (NEW 2026-09-23)**: per 用户原话 "L2 只是提供了用户交互的设施, 具体还要用户提供一个具体上下文请求, 这个上下文请求创建的目标才能做 harness/自进化/rsi 的验证". 修订: L2 spec §R13 (ContextRequest 8 字段契约 + 零 hardcode + ≥ 3 类实证) + tasks T6.7-T6.10 (test_context_request_validation 5 cases + test_context_request_e2e 4 cases + 3 reference ContextRequest file ship) + proposal EVOL-DEMO-10 + AGENTS.md §Reverse Indicator Rule 5 档必须 (含 context_ids 列表) + 3 份 SoT 各自 §十二.9 (上下文驱动约束 + R13 引用) + rsi §11.8.8 (上下文驱动治理 + ≥ 3 类实证 = R3 红线必备) + README.md 三方 SoT 基线声明更新. 共 ~13 文件 +900 行 (本轮 R13 增量). 双向链路: L2 spec R13 → SoT §12.9 → rsi §11.8.8 → AGENTS.md Reverse Indicator 5 档必填 → commit `[Reverse Indicator]` 段含 `context_ids` 列表. | (待 4 atomic commits, 详情见 §十一 Adjacent Log) |
| 2026-09-23 | 36 | **3 份 SoT 文档升级 + L2 OpenSpec change 完整治理闭环 (Cross-Doc Review 2026-09-23 first pass)**: self-evolution §十一 + §十二 (pdk_chat_demo traceback + Verification Matrix R8/R9) + harness §十一 + §十二 (5-tier gate 反向校验) + rsi §十一 + §十二 (真 RSI 三判据 + 反作弊) + AGENTS.md §REVERSE INDICATOR RULE (commit 强制 [Reverse Indicator] 段) + L2 spec §R8 + §R9 (反向指标门 + 反作弊测试 3+3 cases) + tasks T6 (新增 task group) + proposal.md (3 新 capabilities EVOL-DEMO-7/8/9) + design.md §十.7 (R8/R9 设计交底) + roadmap drift patch. 共 ~13 文件 +1000+ 行, 全部双向引用. | AGENTS.md Reverse Indicator § + R8 + R9 + §十二 双向链路闭环 | (待 5 atomic commits, 详情见 §十一 Adjacent Log) |

| 日期 | Sprint | Drift 描述 | 解决 | Commit |
|------|--------|-----------|------|--------|
| 2026-09-17 | 34 | **C0 ship-with-fixes**: Oracle review (session `ses_f54ef2010ffeLK90Y0OQp1DuxJ`) 发现 4 项 (Critical commit 顺序, Major spec R3/R2 producer-correction, Minor test 名 + spec text alignment). 全部修正 ship. | 应用 Critical/Major/Minor; 3 atomic commits `f84dbb3` + `d21ac6f` + `f3fbb9d`; archive 5 文件完整 | `f84dbb3` + `d21ac6f` + `f3fbb9d` |
| 2026-09-17 | 34 | **C1 ship-with-fixes**: Metis dual-agent review (session `ses_f53731302ffegMN8KTsrzqrPOB`) 发现 4 Major (M1 merge_patch 语义未定义, M2 子图 registry 隔离 NOTE 缺失, M3 thread_local per-thread 声明缺失, M4 E2E 自动化缺失). 全部修正 ship. | design.md D2/D4 加 NOTE, tasks.md §6.1 改自动化 E2E; deep agent `bg_9a5f7c89` 实施 22 case PASS; atomic commit `f4766be`; archive 5 文件完整 (路径 `2026-09-17-2026-09-16-loop-agent-tools` 因当前日期 9-17 + 创建日期 9-16 双前缀) | `f4766be` |
| 2026-09-17 | 34 | **P0 (fix-dsl-call-pause-autonomous-mode) ship-with-fixes**: Oracle+Metis dual-agent review (sessions `ses_f530341e6ffeCdMvLYU9pITBoI` + `ses_f53731302ffegMN8KTsrzqrPOB`) 发现 7 项 (Oracle C1 steps, C2 SchedulerConfig.execution_flags 透传路径, M1 catch 双 guard, M2 has_autonomous_flag 包装, M3 enum class 单 flag; Metis M1 loop/execute_plan Autonomous, M2 D1 DSL-only 实现, M3 worker pool 调研). 全部修正 ship. | 基础设施 11 文件 +209/-3 (`016497e`) + D4 wiring 4 文件 +308/-8 (`23e8403`); 7 test_loop_agent_autonomous + 7 test_e2e_mock 全 PASS (真实 DeepSeek LLM "Hello" 验证); Option A mock_fallback 修回归 (Metis M3 衍生) | `016497e` + `23e8403` |
| 2026-09-17 | - | **Master Plan 补登记 Wave 2 P1**: P0 proposal L182 提及"P1 (intent 分类 + loop_type 路由) — 独立 change" 但 master plan 未登记. Metis+Oracle dual-agent review 确认 P1 真实存在, 完全可通过纯 DSL 图节点实现 (`lib/loop/intent_classify.agent.md` + switch + loop/run_subgraph + 可选 generate_subgraph). | §三表格添加 P1 行 (deferred → Sprint 36+), §二 Dependency Graph 添加 P0→P1 hard + P1→(none), 顶部 ⚠️ 更新日志记录本次同步, §十一 Adjustment Log 添加 P1 row. 估时修订 3-5 天 → 2-3 天. | (本 commit) |
| 2026-09-17 | - | **Master Plan 补登记 Worker Pool 调研结论**: Oracle 调研 (session `ses_f505f99fdffefgE5Q9oFA2t2AD`) 确认 chat 路径无 worker pool 选择 (CognitiveWorker 生产零使用, DomainWorkerPool 唯一生产消费者是 C++ ForkJoinLoop 但 chat 走 DSL TopoScheduler Taskflow). | P1 intent schema 不加 `worker_pool` 字段. 顶部更新日志记录. | (本 commit) |
| 2026-09-17 | - | **DAG 动态组合完整流程调研完成**: Oracle session `ses_f4fd88215ffeUWSe2StAWtFPSQ` (20m 36s). **新发现 2 个 Latent Gap** (此前未记录): (1) 静态 `next: "/dynamic/..."` 在 `parse_node_wait_for_deps` (topo_scheduler.cpp:88-95) 抛 "Next node not found", 无 `/dynamic/` 豁免; dsl.md §423/§438/§1114 与实现矛盾. (2) generate→register→execute 全链路无任何端到端测试; 现有 2 个 "E2E" 名义测试实为 prompt smoke, 真实 LLM 对 `execute_generate_subgraph` 覆盖 = 零. **关键架构事实**: plan_execute.agent.md 已 ship 的"LLM 生成子图→执行"走 `loop/execute_plan` 工具 (独立子引擎), **不走 generate_subgraph 节点**. | §三 P1 Row 修订为方案 A'' (推荐): ChatSession 两次平级 loop/run + 复用 `loop/execute_plan` 模式 + 不使用 generate_subgraph 节点. §十一 Adjustment Log 加 P1 方案 A'' + generate_subgraph 节点 deferred 行. | (本 commit) |
| 2026-09-18 | 34.5 | **F1 `fix-react-decide-empty-response` SHIPPED** (Oracle 3 sessions 累计审计). **根因修正 (per `ses_f4d05cdb0`)**: 初判 (flatten_layers 嵌套) 错; 正确根因 = think 节点 LLM 空 text silent 穿透 → inja 静默渲染 "" → decide_react "Missing 'response' argument". **最小修复**: `node_executor.cpp:194-205` main path + `:147-157` stream path 双路径 fail-fast 空校验 +21 行 (per AGENTS.md 模式 #1). **测试**: 5 cases / 13 assertions NodeExecutor 级 + 1 skip-guarded real-LLM skeleton. focused ctest 9/9 PASS 0 regression. 全量 ctest 16 known pre-existing failures (Oracle audit 已确认非本 change regression). openspec validate --strict → "Change is valid". **Metis waived**: change 收敛为 minimal fix, Oracle 3 sessions 累计覆盖, docs drift 由 Oracle session 3 (`ses_f4c6e14f`) 直接发现并修正 (3 处 spec drift + design 重复节删除). Single-Dev 模式自审决议合法. archived `2026-09-18-fix-react-decide-empty-response` (6 files verified, Day 5 lesson 避免). §一.4 Bug3 残量风险句更新 ✅ FIXED + 双 commit hash 引用 + §四 F1 子节 9 TODO 全勾选 + §十一 Adjustment Log 新增 4 行 (ship / §5 降级 / spec drift / ctest 247). | `a96842e` + `9dc3ac8` |
| 2026-09-21 | 36 | **C4 `harness-rsi-pilot` SHIPPED + GO** (5 day ship cycle, see §十一 Adjustment Log 2026-09-21 row for full details). | decision record + Decision Record `docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md` (5 判据全绿 + 4 摩擦 Wave 3 优先解决). | (`f7f0fe3` → `08aace2` 11 atomic commits) |
| 2026-09-21 | 36 | **C1 fresh-MHMAC 修复** (Oracle `bg_3c06ae5b` Critical C1): `src/core/genome/registry_filesystem.cpp:load_or_generate_hmac_key` 把 `fs::permissions()` 移到 `ofstream` 创建文件**之前** (M6 hygiene fix commit `60d5a18` 引入回归) — `fs::permissions` 对不存在路径抛 `filesystem_error` → 全 new 机器上首跑 commit 永远 `IOError`. 修复: `::open(O_WRONLY\|O_CREAT\|O_EXCL, 0600)` 原子创建 + 赋权. **回归守卫**: `tests/test_genome_registry.cpp` 顶部 hermetic env fixture + 新增 case 13 `fresh_home_key_generation_0600` (隔离 HOME + unsetenv 走真实 key 文件生成路径). **验证**: 13/13 PASS, **273 assertions** (baseline 12 cases / 266 → +1 case 13, 273 assertions). **AGENTS.md 模式 #10 候选**: post-acceptance hygiene fix 打乱资源创建顺序 → fresh-deploy 静默回归. | `dea85f6` |
| 2026-09-21 | 36 | **G1 `harness-rsi-remove-governance` SHIPPED** (Sprint A, merge `9709317`). Oracle dual-agent pre-impl review `bg_c706862b` (Oracle) + `bg_d9744d91` (Metis) 应用 SHIP-with-fixes 修正 (D2 语义锁静默忽略 void / D3 加 register_llm_tool / BREAKING 降级非 BREAKING). 实施 commit `714764d` + SHIP-with-fixes `9ee475e` (Oracle bg_8237a316 verdict 0C + 2M + 4M + 1 D3 ACCEPT) + retry-2 `f1a6647` + retry actual g++ compile `e182f82`. 4 tests / 22 assertions. AGENTS.md 模式 #10 + #11 沉淀 (commit `19e0e8d`). | `714764d` + `9ee475e` + `f1a6647` + `e182f82` |
| 2026-09-22 | Pre-Wave3 Sprint A | **G2 `evolution-verdict-reward-quality` SHIPPED** (merge `dc12a17`). 实施 commit `e51073a` + Oracle bg_ebfe1c25 SHIP verdict (0C + 0M + 2 Minor). `tests/test_transition_guard` Case 2 `"Poor"` 强化 + `tests/test_harness_rsi_pilot` Case 2 + 6 tests 零回归. Total ctest 208/210 PASS (99%), 2 失败均为 pre-existing (test_skill_interpreter KI 7.S29-1 + test_pdk_plan_execute BAD_COMMAND build 后 PASS) — 零 G1 回归. **Start: 24h Wave 3 cooling-off 计时 2026-09-22 22:30 UTC (per G2 merge dc12a17)**. | `e51073a` + `3cdb792` (builder state FULL schema) + `737e979` (archive) |
| 2026-09-22 | Pre-Wave3 Sprint A | **G3 `sync-pdk-contract-header` SHIPPED** (merge `a196a09`). 实施 commit `a641d34` (PDK_CONTRACT_DEPS=11 + DRY_RUN 离线 + drift-guard) + SHIP-with-fixes `77d6fae` (Oracle bg_ef5a0ca4 verdict 0C + 1M + 3 Minor; drift-guard grep `<` + `")"` 字符 sentinel 防 PDK consumer 静默 fail). `bash -n scripts/sync-pdk.sh` 语法 PASS + 4/4 dry-run test + openspec validate PASS. | `a641d34` + `77d6fae` + `19abc79` (post-impl execute summary) + `bd74fc1` (archive) |
| 2026-09-22 | Pre-Wave3 Sprint B | **G4 `genome-wiring-harness-rsi-gepa` SHIPPED** (merge `fb2769f`). 闭环第 7 环"版本提交/发布"端到端修复. 实施 commit `1fcb00e` (Gate 3 persist-before-apply + GEPA persist-then-commit + undo + 2 事件 + 9 tests) + SHIP-with-fixes `c5d0c78` (Oracle bg_e4eec567 verdict 0C + 3M + 1 Minor). Major fixes: (M1) `gepa.commit.committed` payload 补 `genome_version` 字段; (M2) `tests/test_harness_rsi_pilot.cpp` case-7e 重构 (加 stubs); (M3) `docs/adr/adr-0068-event-emission-contract.md:254` 行 emitter 修正. 4/4 focused ctest 零回归. | `1fcb00e` + `c5d0c78` + `a21c08a` (tasks 6.4/6.5 同步) + `ac5ef14` (archive) |
| 2026-09-23 | Wave 3 Phase 1 | **Wave 3 Phase 1 `finetune-base-model` pilot SHIPPED** (merge `f0a5c4b`). 24h Wave 3 cooling-off override by user (审计见 `.rddf/state/builder/wave-3-finetune-base-model.json::cooling_off_override_audit`). 实施 commit `97a2abb` (18 files, +1057/-62: 4 件套 + ADR-0078 翻牌 ✅ + D1 评分 yaml + D3 脚本 + D7 stub + 2 tests + archive 5 文件) + SHIP-with-fixes `232eb13` (5 files, +66/-25: Oracle bg_7fe026cc verdict 1 Critical + 1 Major + 4 Minor). C1 fix: 6 test 断言更新反映 `LLMProviderFactory` ctor 自注册 baseline + rationale. M1 fix: D1 评分 yaml 算术 5/5 校正 deepseek-v2-chat `passed_all_filters: false` (实际 7.35 < 7.5 阈值). **ADR-0078 ✅ Approved + Wave 3 Pilot 激活**. 12 AC verification 全部 ✅. focused ctest 9/9 PASS (2.23s). | `97a2abb` + `232eb13` + `4de7745` (post-merge sync) |

---

## 十一、Adjustment Log

| 日期 | Change | 调整 | 原因 |
|------|--------|------|------|
| 2026-09-17 | C1 | tools 字段 v1 忽略（留 future field） | Metis N1 minor: design.md 隐式接受, tasks §3 未列 |
| 2026-09-17 | C1 | `loop/decide_react` response 字段保持原文 | design.md D1 L1/L2/L3 统一返回原文, spec Scenario 强化 (Metis N3) |
| 2026-09-17 | C1 | 错误码仅断言活跃值 (InvalidParams/Unknown) | C1 仅产 2 个值, Cancelled/ToolNotRegistered 是保留值 (Metis N5) |
| 2026-09-17 | P1 (Wave 2) | **新登记** Wave 2 P1 (intent-classification-router) — 纯 DSL 子图实现 (`lib/loop/intent_classify.agent.md` + switch + loop/run_subgraph + 可选 generate_subgraph). 估时 **2-3 天** (DSL 实现比 C++ 实现更轻量). Deferred → Sprint 36+ 与 C4 并行候选. | P0 proposal L182 显式声明下游 dep 但主计划未登记; Metis+Oracle dual-agent review 确认 scope + DSL-only 实施路径 (sessions `ses_f5084d7bfffeq1Yt5QVVtaIfix` + `ses_f5084614dffesPP0dEHgJhc4LM`) |
| 2026-09-17 | P1 (Wave 2) | **P1 intent schema 保持 4 字段**: `intent_type/complexity/suggested_loop/requires_subgraph`, **不加 `worker_pool` 字段** | chat 路径无 worker pool 选择; CognitiveWorker 生产零使用; DomainWorkerPool 唯一生产消费者是 C++ ForkJoinLoop 但 chat 走 DSL TopoScheduler Taskflow (Oracle `ses_f505f99fdffefgE5Q9oFA2t2AD`) |
| 2026-09-17 | P1 (Wave 2) | **P1 实施路径从"全 DSL 化"修订为方案 A''** (混合模式, Oracle `ses_f4fd88215ffeUWSe2StAWtFPSQ` 推荐). 分类逻辑仍是 DSL 图 `lib/loop/intent_classify.agent.md` + `loop/classify_intent` 工具, 但 **dispatch 决策在 ChatSession 层** (两次平级 loop/run 调用). 动态子图部分 **复用已 ship 的 `loop/execute_plan` 工具模式** (独立子引擎), **不使用 generate_subgraph 节点**. 估时 0.5-1 sprint. | Oracle DAG 调研发现 generate_subgraph 节点有 2 个 latent gap (静态 `next: /dynamic/...` build_dag 抛错 + 全链路零 E2E 测试); 方案 A'' 规避生成图节点, 利用已 ship 模式, 同时符合双循环架构 (Chat-loop 管 turn 边界路由, Agent-loop 管 turn 内推理). |
| 2026-09-17 | generate_subgraph | **`GenerateSubgraphNode` 节点 deferred to 独立 fix 项** (不阻塞 P1). dsl.md §423/§438/§1114 描述的 `next: "/dynamic/x"` 静态跳转与实现矛盾 (`parse_node_wait_for_deps` 无 `/dynamic/` 豁免); 真实 LLM 对 `execute_generate_subgraph` 覆盖 = 零. **P1 走 `loop/execute_plan` 已 ship 模式绕过此问题**. | Oracle DAG 调研发现 latent gap; 修复 generate_subgraph 节点需要 (a) build_dag 加 `/dynamic/` 豁免或修正 dsl.md 文档使其与 wait_for 动态机制一致 + (b) 补全 3+3 真实 LLM E2E (happy + error path) — 总估时 1-2 sprint, 单独立项. |
| 2026-09-18 | F1 | **`fix-react-decide-empty-response` 新登记** — react agent loop 在 `decide` 节点真实 LLM 端到端 `Missing 'response' argument` 失败立项. `commit 0b0da50` (`fix(tool_result): bridge top-level 'error' field to meta.error_message`) 修复使本 bug 可见. 根因待诊断 (`{{llm_response}}` inja 模板 ctx bridge). OpenSpec 4 artifacts done: proposal.md (Why/What/Capabilities/Impact) + design.md (D1-D6) + 2 specs (react-agent-llm-ctx-bridge 6 R + real-llm-react-loop-e2e 5 R) + tasks.md (7 task groups). 估时 **5h** (TDD + 真实 LLM 6 cases). Sprint 34.5 follow-up. | §七.1 触发 (新 change 立项 = change 状态变化) + §一.4 Bug 3 残量风险不对齐 (旧引用 "chat-real-llm-coverage Phase H" 实际是独立 change F1) |
| 2026-09-18 | **Master Plan 同步 (本 commit)** | §一.4 Bug 3 残量风险引用名对齐 (chat-real-llm-coverage Phase H → `fix-react-decide-empty-response`) + §三 Overview 新增 F1 row + 总估时 +5h + §四 Detailed Tracking 新增 §四 F1 子节 (含 9 TODO 项) + §十一 Adjustment Log 新增 2 行 (本行 + F1 立项) + §五 Sprint Breakdown 新增 Sprint 34.5 段 + §六 Risks 新增 R11 + §十三 Response Change Types fix 行加示例 + §八.3 References 加新 change path + 附录 B 更新实施路径 (含 Oracle session `ses_f4f9061b3ffeuy4qw2iatqQojw`) + Last Updated 同步 2026-09-18. | §七.1 触发 (新 change 立项 / §一.4 漂移 / 调整) |
| 2026-09-18 | F1 | **`fix-react-decide-empty-response` ✅ SHIPPED** — Oracle 3 sessions 累计 (`ses_f4d05cdb0` 设计评审纠正初判根因 + `ses_f4caa8cf` 完成审计 + `ses_f4c6e14f` 中期审计). **根因修正**: flatten_layers 不在 react 路径 (初判错), 真实 = think 节点 LLM 空 text silent 穿透. **Fix 路径**: main+stream 双路径 fail-fast 空校验 +21 行 (`node_executor.cpp:194-205` + `:147-157`). 测试: 5 cases NodeExecutor 级 (`tests/test_dsl_engine_ctx_bridge.cpp`) + 1 skip-guarded skeleton (`tests/test_react_loop_real_llm.cpp`). 2 atomic commits `a96842e` + `9dc3ac8`. archived `2026-09-18-fix-react-decide-empty-response`. | §七.1 触发 (change ship 状态变化 → 更新 §一.4/§三/§四). 也作为 Single-Dev 模式下完整 SHIP-with-fixes 流程的范本: Oracle review → 实施 → 文档同步 → archive. |
| 2026-09-18 | F1 | **§5 降级: 真实 LLM 6 cases → 1 skip-guarded skeleton + Phase H 移交** (Oracle `ses_f4caa8cf` 推荐 + `ses_f4c6e14f` 确认). 原因: sandbox 无 DEEPSEEK_API_KEY, mock 注入 (MockLLMEmptyTool) 已验证 fix 路径, 6 cases 投入产出比低. Single-Dev 有 key 时手动跑 `HYDRAFORGE_SKIP_REAL_LLM=0 DEEPSEEK_API_KEY=... ctest -R test_react_loop_real_llm --output-on-failure`. | Mode #1 minimal fix + 模式 #1 step 4 latent sites 记录 + 真实 LLM 测试 ROI 评估. |
| 2026-09-18 | F1 | **Spec drift 修订**: R4 EmptyLLMResponseError → runtime_error (实施抛 plain runtime_error, not 新 ErrorCode); R1 streaming first_text_chunk → full text (实现存完整 result["text"]); D3 fork/join 决议: 双注册非 bug + 当前 schema OK (Oracle `ses_f4caa8cf` 审计). | Oracle `ses_f4c6e14f` 发现 spec drift 阻塞 archive — 修订后 openspec validate --strict → "Change is valid". |
| 2026-09-18 | F1 | **Total ctest**: 245 → 247 (新增 test_dsl_engine_ctx_bridge binary 5 cases / 13 assertions + test_react_loop_real_llm binary 1 skip-guarded case). Focused ctest 9/9 PASS 0 regression. | 测试套件扩展, 245 门禁被新 binary 替换为 247. |
| 2026-09-18 | 短链 | **F1 ship hygiene (post Oracle bg_59b2388c final review)**: 4 gaps — (G1) archive 移动未入 git (Day 5 lesson 实质性避免, 修 `d91b212`) + (G2) `examples/pdk_chat_demo/~/` junk dir 删除 + (G3) archived tasks.md 38 checkbox 全勾选 + (G4) 附录 B.1 stale 修复 + SUCCEED message typo "deepfail-fast" → "fail-fast". 2 atomic commits (`d91b212` + `7b782aa`). | Day 5 lesson 实质性避免 (archive 必须入 git) + future maintainer 文档完整性. |
| 2026-09-18 | 短链 | **Sprint 34.5 follow-up closure: 3 placeholders + 2 shippable items**: Tasks #1-#5 of F1 ship hygiene 短链. (1) `2026-09-18-fix-flatten-layers-comment-drift` placeholder (P3 cosmetic, ~30min) (2) `provider-llm-tool-empty-passthrough` placeholder (P2 defense-in-depth, ~1-2h) (3) `chat-real-llm-coverage-phase-h` placeholder (P2 coverage gap, ~3-5h) (4) `test_react_loop_real_llm.cpp` 1→2 cases 增强 (~30min, commit `211f866`) (5) `node_executor.cpp` 2 `std::cerr` → `LOG_WARN` migration (~1h, commit `f83b85d`). 3 atomic commits. | F1 design.md Latent Sites 表 + AGENTS.md 模式 #1 step 4 systematic recording. |
| 2026-09-19 | 35 | **C2 `genome-registry` Oracle design review (bg_a818a6a1, 1m 45s)**: D9 filesystem / D10 HMAC-SHA256 / D11 unlimited lineage 全 resolve. 加 5 obs (GenomeError 独立 enum 不对齐 ToolResult / fork 设计空白 / capture_mode 字段值域 / version 单调整数 / CLI demo 避 pdk_chat_demo namespace pollution) + 12 test cases + 5 pitfalls. | D9/D10/D11 决策落地 + 12 test cases 收敛 + 5 pitfalls 避免. |
| 2026-09-19 | 35 | **C2 `genome-registry` SHIPPED** — 6 commits: design+spec+tasks (`2e7af89`) + RED 12/12 FAIL (`a320032`) + GREEN 12/12 PASS 266 assertions (`839590d`) + critical fixes (`b6114c2`) + spec amendments (`507eae3`). **Oracle dual-agent review**: bg_9ade564d (BLOCK → SHIP) + bg_89293120 (ship-with-fixes → SHIP). **Critical fixes**: C1 cycle detection infinite loop (visited keyed on name@version pair + 10000 depth cap), C2 vacuous cycle test (re-sign tampered YAMLs), M1 list_versions double-file check + sig-first atomic write ordering, M2 HMAC key uses OpenSSL RAND_bytes CSPRNG, M3 IOError lifecycle (no longer dead enum), M4 mutex commit serialization, M5 lineage validation at commit (not only load). **Spec amendments**: fork version = max+1 (was parent+1), walk_ancestors deferred to C3, fsync deferred to follow-up, CLI tool deferred to genome-cli change. Total ctest +12 (12 new in test_genome_registry). Focused ctest 32/32 PASS (test_executor + test_dsl_engine + test_loop_agent + test_react + test_provider + test_session + test_genome_registry), 0 new regression. archived `2026-09-19-2026-09-16-genome-registry` (Day 5 lesson — actually misapplied by initial archive commit `1ea048a`, re-applied correctly by follow-up post-acceptance review commit). §三 Overview C2 ⚪ → ✅ SHIPPED + §十一 Adjustment Log +7 行 (Oracle design + Oracle review + 5 critical fixes + spec amendments + ship + Total ctest 248 + Oracle acceptance review). | Phase 6 Candidate B 服务化核心基础设施就位 + C3 启动前置 (Genome 版本号接口). |
| 2026-09-19 | 35 | **Total ctest**: 247 → 248 (+1 binary: test_genome_registry with 12 cases, but ctest -N counts binaries not cases; total ctest count was hand-calculated and inflated to 259 in earlier claim, corrected to measured 248 post Oracle acceptance review 2026-09-19). focused ctest 32/32 PASS. Active OpenSpec 5 → 7 (C2 archived + 3 placeholders from this session: fix-flatten-layers-comment-drift / provider-llm-tool-empty-passthrough / chat-real-llm-coverage-phase-h). | 持续增强 + Single-Dev mode 范本 (Oracle dual-agent review 完整闭环). |
| 2026-09-20 | 35 | **C3 `h-d-m-transition-guard` Oracle post-ship verdict (bg_f6190442, 7m 51s, ALIGNMENT SCORE 62 / NEEDS_FIX)**: implementation ship `0ffc637` (test_transition_guard 13/13 cases / 47 assertions PASS) + archive `7a15744` (8 specs shipped), 但 Oracle 识别 **2 critical + 4 major + 4 minor debt** — Critical D5 walk_ancestors 接口扩展 + D6 judge_data_freshness 完整实装 + D9 walk_ancestors 默认实现 deferred to Sprint 34+ follow-up, 提议新增 2 follow-up OpenSpec changes. 治理债跟踪: `2026-09-20-ig-genome-registry-walk-ancestors` + `2026-09-20-adr-0068-appendix-a-evolution-themes` 登记. | Phase 6c MetaRSI-v1 C3 v1.0 ship + 治理债显式化 (per Single-Dev 模式规范: ship 必有债务跟踪). |
| 2026-09-20 | 35 | **C3 follow-up `ig-genome-registry-walk-ancestors` SHIPPED** (10 atomic commits `a40e9e1`→`231cd8d`): **Oracle dual-agent pre-impl review** (`bg_7984922b` Metis C1/C2/C3 + `bg_dd35a52d` Oracle O-1/O-2/C4, 双重命中 C1 = 最高优先级 fix) → 4 Critical fixes 应用 (C1 type unification 双 `IGenomeRegistry` / C2 walk-failure verdict 映射 / C3 `GenomeError::NotImplemented` enum / C4 (name,version) 定位 + cross-name rejection). **Implementation**: D5 walk_ancestors virtual method (default impl NotImplemented per AGENTS.md pattern #9 ITimerService precedent) + D9 default impl returning NotImplemented + D6 judge_data_freshness 完整 5 cases 实装 + FilesystemGenomeRegistry::walk_ancestors override (light parse path + visited set cycle detection + cross-name BrokenLineage) + test_genome_walk_ancestors **10 cases / 55 assertions GREEN** (Case 7/8/9 P0 fix: judge_data_freshness lineage integration). **Oracle post-impl review SHIP-with-fixes verdict 80/100** (`bg_f6190442`) → 3 fix commits (P0+P1+P2) → **Oracle 2nd review APPROVE 95/100** (`bg_86a511e0`). archived `2026-09-20-2026-09-20-ig-genome-registry-walk-ancestors`. C3 D5/D6/D9 翻牌 ✅ ship + C4 harness-rsi-pilot **unblocked**. **Total ctest**: 248 → 251 (+ test_transition_guard + test_genome_walk_ancestors + test_credit_assignment). | Phase 6c MetaRSI-v1 C3 完成 + Oracle SHIP-with-fixes 闭环 (3 commits fixups per pattern #4) + C4 启动前置. |
| 2026-09-20 | 35 | **ADR-0086 v1.1 决策 9 签名 amendment** (Critical C1): `judge_data_freshness` 签名 `IGenomeRegistry&` → `::agenticdsl::genome::IGenomeRegistry&`. 起因: `attribution_record.h:106` 空 stub `struct IGenomeRegistry {};` 在 `agenticdsl::evolution` namespace, 签名解析到空 stub 而非 `agenticdsl::genome::IGenomeRegistry` 真实接口 → D6 完整实装无法编译. 修复: stub 删除 + 前向声明 `namespace agenticdsl::genome { class IGenomeRegistry; }` (global scope) + signature 统一 + MockRegistry 2 处迁移 derive from genome::IGenomeRegistry + 5 个纯虚 override stub. 已知 v1.1 限制: signature 返回 `AttributionVerdict` (not `AttributionRecord`), 不携带 confounder/reason — caller 需自行构造 `AttributionRecord.confounders`. v1.2 amendment 候选: 扩展返回类型为 `Result<AttributionVerdict, JudgeResult>` 含 verdict + HarnessChangeRecord. | JudgeResult 签名扩展候选 (Sprint 34+ follow-up 待启). |
| 2026-09-21 | 36 | **C4 `harness-rsi-pilot` SHIPPED + GO** (11 atomic commits `f7f0fe3` → `08aace2` 跨 5 days): **Phase 4.0 DB1 fix** (IToolRegistry::unregister_tool_function 纯虚方法 + 25 文件 override 含 20 个测试 MockToolRegistry) → **Phase 4.1+4.2** (`include/agenticdsl/evolution/harness_rsi.h` 5 struct + 1 enum + apply_harness_mutation 5 参消除 core→PDK 反向依赖; `src/evolution/harness_rsi.cpp` dual-gate 实现 + EventBuilder emission) → **Phase 3 RED tests** (4 cases / 26 assertions GREEN) → **Phase 6 SHIP-with-fixes** (4 修正: Major-1 补 4 cases → 8 cases / 39 assertions / Major-2 spec 文本 / Minor-1 注释 9→10 / Minor-2 Decision Record 202 行). **4 轮 Oracle review 闭环**: dual-agent pre-impl (bg_3672cb57 6 修正 + bg_1f291bc4 5 DEAL-BREAKER + Case 4 删除) → 2nd review SHIP-with-fixes (bg_770d1308 5 文档级修正) → 3rd review post-impl SHIP-with-fixes (bg_3ef7280a 5 修正) → **4th review 独立审查 (bg_afa84d4d, 10m 20s, 6 修正含 Critical-1 partial apply 零状态变更)**. **Decision Record** `docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md` GO 决策 (5 判据全绿 + 4 摩擦 Wave 3 优先解决). Total ctest 251 → 252. Archived `2026-09-21-2026-09-16-harness-rsi-pilot` (4 文件完整 per AGENTS.md Day 5 lesson). **Wave 3 (ADR-0078 Model-RSI pilot) 立项依据已就绪**. | Phase 6c MetaRSI-v1 完整 ship (C2 + C3 + walk-ancestors follow-up + D8 + C4) + 5 天 11 atomic commits + 5 轮 Oracle review 闭环 + Go 决策落地. |
| 2026-09-21 | 36 | **C1 fresh-MHMAC 修复** (Oracle `bg_3c06ae5b` Critical C1, commit `dea85f6`): `src/core/genome/registry_filesystem.cpp:load_or_generate_hmac_key` 把 `fs::permissions()` 移到 `ofstream` 创建文件**之前** (M6 hygiene fix commit `60d5a18` 引入回归) — `fs::permissions` 对不存在路径抛 `filesystem_error` → 全 new 机器上首跑 commit 永远 `IOError`. 本机被既存 `~/.hydraforge/genome.key` 掩盖, CI 全新机器上 100% 必挂. 修复: `::open(O_WRONLY\|O_CREAT\|O_EXCL, 0600)` 原子创建 + 赋权 (无 umask 窗口) + EEXIST 竞争窗口读回. **回归守卫**: `tests/test_genome_registry.cpp` 文件顶部 hermetic env fixture (`setenv(HYDRAFORGE_GENOME_KEY, ...)`, 复制 test_genome_registry.cpp:30-41 模式 — 修复之前 11/12 测试依赖宿主机 key 是 CI 掩盖机制根因) + 新增 case 13 `fresh_home_key_generation_0600` (隔离 HOME + unsetenv 走真实 key 文件生成路径). **验证**: 13/13 PASS, **273 assertions** (baseline 12 cases / 266 → +1 case 13, 273 assertions, binary count 不变). **AGENTS.md 模式 #10 候选**: post-acceptance hygiene fix 打乱资源创建顺序 → fresh-deploy 静默回归; 教训 — (1) 资源初始化顺序变更必须配 fresh-state 测试 (隔离 HOME/tmpdir) (2) `fs::permissions` 对不存在路径抛异常, 原子赋权应用 `open(O_CREAT, mode)` (3) 测试依赖宿主机持久状态 = 掩盖部署期 Critical 的常见盲区. | Phase 6c 后续 — 闭环第 7 环审查触发器. |
| 2026-09-21 | 36 | **doc-alignment 5 处漂移修正** (commit `5b600a6`): Oracle `bg_3c06ae5b` 发现的 cross-doc drift batch ship. (a) `docs/architecture/self-evolution-architecture-2026-08.md` v1.3 → v1.4 — :4 header + :163 Trajectory IR + :171 IDistillationWriter + :210/:211 §七 + :267 §九 grep 验证, 全部从"代码不存在/待办"改为"✅ 已 ship 2026-08-27/29". (b) `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` :264 Genome CRD `spec.harness` 对象 → string 实际类型 + :275/:465 删除不存在的 `rollback` 往返 (IGenomeRegistry 无 rollback 方法). (c) `docs/research/rsi-three-operators-hydraforge-mapping-2026-09.md` 12 处 stale — C3/C4 ship 状态翻牌 + ADR-0086 v1.1 ✅ + walk_ancestors 6 方法已实装 + 新增闭环第 7 环断裂说明. (d) AGENTS.md Recent Changes 顶部追加 2026-09-21 entry (C1 + doc + Oracle bg_3c06ae5b summary) + 模式 #10 沉淀. | Phase 6c 文档一致性 — 单一作者模式下避免视图漂移累积. |
| 2026-09-21 | 36 | **`genome-wiring-harness-rsi-gepa` v2 change 起草 + dual-reviewed** (commits `3076042` + `74e063c`): 闭环第 7 环"版本提交/发布"端到端修复 — Oracle `bg_3c06ae5b` 发现 `IGenomeRegistry` 在生产树**零调用点** (除 `version_pair_diff.cpp:43,56` 只读 `walk_ancestors`), `apply_harness_mutation` 仅改内存, `GEPALoop::reflect_and_commit` 仅发审计事件. Change 设计: Gate 3 persist-before-apply (fork before apply, commit failure → RegistryRejected + 零状态变更) + workflow_patch 检查上移 Gate 0 + `AppliedMutation` +3 字段 (committed_genome_version + prompt_snapshot + tools_snapshot, 自包含 undo) + GEPALoop::Config 注入 + persist-then-commit + 2 个 `genome.*` 事件 + 命名空间分离 (`chat_harness` / `gepa_skill`). **Dual-review (模式 #8)**: Metis `bg_687a5662` + Oracle `bg_534a2541` 独立命中 4 Critical + 3 Deal-breaker — (D1/DB-1/8) 搭车 follow-up `2026-09-20-adr-0068-appendix-a-evolution-themes` 已 archive, 必须本 change 直接修订 ADR-0068 Appendix A v2.3; (D2/DB-2) GEPA `load()` 不可机械断言, 需显式 GenomeSpec 构造规则 (harness=candidate.compiled_content + created_by 经 fork 继承 parent) + 内容回读断言; (D3/DB-3) R6 L4 场景不可测, 改 emit-then-throw 可测 regression; (#1) deep_merge_spec 空 tools 继承 parent → "删光工具"静默分叉, V1 边界 → InvalidMutation; (#2) spec 版本断言 parent_version+1 错 (commit() 实际 max+1); (#3) Gate 3 插入点与 workflow_patch 早退冲突; (#5) undo 快照归属未定义; (#6) GEPA 审计双 id 不一致; (#7) 测试计划缺 HMAC hermetic fixture; (#10) tasks 1.3 误列 gepa_loop.cpp. 全部 ship-with-fixes 应用. **Hard dependency**: `harness-rsi-remove-governance` (active, 同改 `MutationGateContext`) MUST 先 ship; trace_id 字段复用为 `genome.committed` meta.trace_id. **Out of Scope**: `load(genome@N) → 重建 ChatSession → 1 turn` E2E (留 harness-rsi-pilot V2); registry scope 隔离 (Wave 3); `IMutationGovernor::revert` 语义不变. **Regression safe**: registry=nullptr 路径与 V1 逐字节一致. 登记为 Pre-Wave3 收口门禁 G4 (见 §三 Pre-Wave3 子节). | 闭环第 7 环闭合 — C2/C3/C4 三组件 ship 后第一次形成真正的版本化闭环. |
| 2026-09-21 | 36 | **ADR-0086 v1.1 archive 治理修复** (Oracle `bg_6a8e4397` A3 触发, commit `74e063c`): Oracle 审查发现 commit `798b6c6` (2026-09-20) 自报"Archive integrity: 4 files ✓"但 `--stat` 显示纯 4 deletions, 无 rename 或 add 到 archive/. ADR-0086 文件 line 4 + roadmap §1.5 均引用 `OpenSpec change 2026-09-20-adr-0086-v1-1-harness-change-confounder` 为 "ship per" — **死链** + **Day-5 lesson 实质性陷阱复发**. 修复: 从 `git show 798b6c6^:<path>` 物理恢复 4 文件到 `openspec/changes/archive/2026-09-20-2026-09-20-adr-0086-v1-1-harness-change-confounder/` (line 数 404+173+21+150 = 748, 与 commit stat 精确匹配). **根因记录**: `.gitignore:36` 显式 ignore `openspec/changes/archive/` (per-machine ephemeral 策略), archive 目录**根本不入 git 历史**, commit 798b6c6 的 4 deletions 仅完成 active 树删除, archive 内容从未被固化. 治理建议 3 选项 (per-machine ephemeral / archive-in-git / hybrid git-mv) 登记待 RFC-level 决策. 详细审计注记 `docs/governance/2026-09-21-openspec-archive-recovery.md`. **Roadmap 联动修正**: §八.1 ADR-0086 行补 ✅ Approved (v1.1, 2026-09-20) + archive 路径 + 治理注记引用; §8.3.1 archive 列表补 ADR-0086 v1.1 change. | AGENTS.md Day-5 lesson 实质性避免 — 后续 commit 必须用 `git mv` 而非 `rm`. |
| 2026-09-21 | 36 | **Roadmap drift patch + Pre-Wave3 收口门禁 4 项** (commit pending): Oracle `bg_6a8e4397` 12 项漂移审查的 roadmap 侧修正. **Hard 修正**: §8.1 ADR-0086 状态自相矛盾; §8.3.1 仍把已 archive 的 C2/C3/C4/F1 列为活跃 placeholder (4 of 6 entries stale). **Soft/con 修正**: §三 C4 行测试计数 8/39 → 9/43 (4th review 08aace2 后); :939 "Active OpenSpec: 5" → 9 + 时点标注; :7 header 最后验证 09-20 → 09-21. **Missing 补登记**: §三 新增 Pre-Wave3 收口门禁子节 (G1/G2/G3/G4, 4 项, 估时 3-4 天) — G4 为 `genome-wiring-harness-rsi-gepa` (本 session 起草). **Stage 3 修订**: C4 GO 判据 §四 加第 6 项 "变异必须经 IGenomeRegistry 持久化 (版本锚点存在)" — C4 GO 回注为 "GO with post-hoc closure gate (genome-wiring)", ADR-0078 Model-RSI pilot 必须等待 G4 ship 才有可信前提 (Oracle `bg_6a8e4397` verdict C). | 闭环第 7 环断裂的治理余波 — GO 判据盲域不再复现. |
| 2026-09-21 | Pre-Wave3 Sprint A | **G1 SHIPPED 详细**: Oracle dual-agent pre-impl review (`bg_c706862b` Oracle + `bg_d9744d91` Metis) 应用 SHIP-with-fixes 修正 per `apply Oracle bg_c706862b + Metis bg_d9744d91 dual-agent review corrections (3 changes)` commit `a4f374b`. (1) **D2 语义锁静默忽略 void**: `MuTATION_GOVERNANCE_POLICY` 加入 `semantic_locked_tools` 列表 + 默认不存在时空 pass (不忽略). (2) **D3 加 register_llm_tool**: `apply_harness_mutation` 路径补 `mutator::register_llm_tool` 若 LLM tool 名注册 (核心↔PDK 边界对位). (3) **BREAKING 降级非 BREAKING**: 公开 API 仍二进制兼容 (无参数移除/重命名), C-style ABI 维持向后. 实施 `714764d` (commits: refactor unregister_tool_function 25 文件 override + SecureToolRegistry gating + mutex commit + trace_id propagation). SHIP-with-fixes `9ee475e` per Oracle bg_8237a316 verdict 0C + 2M + 4M + 1 D3 deviation ACCEPT. retry-2 `f1a6647` const qualifier + retry actual g++ compile `e182f82` 修复 SLT 模型-编译器协作盲区. **Tests**: 4 cases / 22 assertions (含 Case 5d Spec R1 scenario 3 "remove 不存在工具" + Case 5e R3 scenario 1 并发 register+unregister fuzz). | `a4f374b` (pre-impl) + `714764d` (impl) + `9ee475e` (SHIP-with-fixes) + `f1a6647` + `e182f82` (retry) |
| 2026-09-22 | Pre-Wave3 Sprint A | **G2 SHIPPED 详细**: `EvolutionVerdict` 增 `reward_quality: agenticdsl::RewardSignal::Quality` 字段 (默认 `Quality::Acceptable` per design D1; D2 注释 4→5 字段约束显式修订). `evaluate_readiness()` 填充 `verdict.reward_quality = reward.quality` 在 can_proceed 判定之前 (失败质量不丢失 per design D2). `harness_rsi.cpp:140` `evolution.readiness.denied` 事件 `eval_quality` 字段从硬编码 `"Unknown"` 改为 `agenticdsl::evaluation::quality_name(verdict.reward_quality)` (复用既有 helper). SHIP-with-fixes per Oracle bg_ebfe1c25 verdict 0C + 0M + 2 Minor 不阻塞. (a) **design.md:57 D3 namespace 笔误**: archive 笔误 (写 `agenticdsl::quality_name`, 实施用 `agenticdsl::evaluation::quality_name`), 已 record in builder.json, design 冻结可接受. (b) **spec 2 个 Excellent 场景无独立断言**: Acceptable+Poor 路径已覆盖, 机制同一枚举直通. **Tests**: test_transition_guard 14/14 (51 assertions, +1 G2 case) + test_harness_rsi_pilot 22/22 (115 assertions, Case 2 强化 "Poor"). **Post-merge ctest**: 208/210 PASS (99%), 2 失败均为 pre-existing (KI 7.S29-1 + test_pdk_plan_execute BAD_COMMAND build 后 PASS). | `e51073a` + `3cdb792` (builder state FULL schema) + `737e979` (archive) |
| 2026-09-22 | Pre-Wave3 Sprint A (∥ G1+G2) | **G3 SHIPPED 详细**: `scripts/sync-pdk.sh` 实施 PDK_CONTRACT_DEPS=11 + DRY_RUN 离线 + drift-guard grep. SHIP-with-fixes `77d6fae` per Oracle bg_ef5a0ca4 verdict 0C + 1M + 3 Minor. **Major fix**: drift-guard grep 覆盖 `<` + `")"` 双字符 sentinel, 替代原宽松 grep (防止 PDK consumer 实际 `find_package(hydraforge_pdk)` 找不到契约头时静默 pass). **测试**: `bash -n scripts/sync-pdk.sh` 语法 PASS + 4/4 dry-run test (PDK_CONTRACT_DEPS=11 + DRY_RUN offline + 4 头覆盖) + openspec validate PASS. **3 Minor 不阻塞**: (i) error message 双语风格统一 (zh-CN + en); (ii) examples/ 路径 canonicalize 边缘处理; (iii) follow-up POSIX sed 兼容性 grep -P 切换. | `a641d34` + `77d6fae` + `19abc79` (post-impl execute summary) + `bd74fc1` (archive) |
| 2026-09-22 | Pre-Wave3 Sprint B (G1 ship 后) | **G4 SHIPPED 详细**: `src/evolution/harness_rsi.cpp` + `src/modules/cognitive/gepa_loop.cpp` 接线到 `IGenomeRegistry`. 闭环第 7 环"版本提交/发布"端到端修复 (per Oracle bg_3c06ae5b Critical C2 发现 — `IGenomeRegistry` 零生产调用点). 实施 `1fcb00e` (Gate 3 persist-before-apply + GEPA persist-then-commit + undo + 2 事件 + 9 tests). **SHIP-with-fixes `c5d0c78` per Oracle bg_e4eec567 verdict 0C + 3M + 1 Minor**. Major fixes: (M1) `gepa.commit.committed` payload 补 `genome_version` 字段 (spec MUST) + case-9 regression guard; (M2) `tests/test_harness_rsi_pilot.cpp` case-7e 重构 (加有效 stubs + registry + parent_version=1); (M3) `docs/adr/adr-0068-event-emission-contract.md:254` 行 emitter 修正 (移除 GEPALoop 引用, 注明走 gepa.commit.committed 路径). **Tests**: test_harness_rsi_pilot 22/22 (111 assertions) + test_gepa_phase2 21/21 (46 assertions) + test_genome_registry 13/13 (273 assertions) + test_genome_walk_ancestors 10/10 (55 assertions). 4/4 focused ctest 零回归. **NOT-VERIFIED**: 全量 ctest 252 binaries post-merge (执行中) + TSan (机器性能受限). | `1fcb00e` + `c5d0c78` + `a21c08a` (tasks 6.4/6.5 同步) + `ac5ef14` (archive) |
| 2026-09-22 | Pre-Wave3 Sprint B | **24h cooling-off 启动** + Step 1 hygiene (commit `98711e3`): G2 merge `dc12a17` 后立即启动 24h Wave 3 cooling-off 计时 (起点 2026-09-22 22:30 UTC, 满点 2026-09-23 22:30 UTC). Step 1 hygiene 同步 G3/G4 Recent Changes + §5.1 closed + G3 active 残留清理. 期间可做 Wave 3 立项准备 (读 ADR-0078 background + 写 improvement 5-segment 草稿, 不正式立项). | `98711e3` (hygiene) |
| 2026-09-23 | Wave 3 Phase 1 Pilot | **W3.P1 SHIPPED 详细**: ADR-0078 ✅ Approved + Wave 3 Pilot 激活 (per Pre-Wave3 Plan §3 + Decision Record §5.1). 实施 commit `97a2abb` (18 files, +1057/-62): **(a) ADR-0078 翻牌**: `docs/adr/adr-0078-finetune-base-model.md` 状态 → ✅ Approved (Wave 3 Phase 1 Pilot 激活) + Phase 1 容量评估表 (D1-D7 边界 Phase 1 vs Phase 2+) + Wave 3 cooling-off 起算点. **(b) D1 评分框架**: `docs/research/wave-3-base-model-selection.md` 持久化 (4 维度 + 5 候选模型基线). **(c) D3 训练数据准备第 1 路**: `scripts/prepare_training_data.py` 迁移脚本加 `source` 字段 + 过滤 `parse_valid && task_success`. **(d) D7 Phase 1 最小版 serving**: `LLMProviderFactory::register_dynamic("agenticdsl-llama-3.1-70b-lora-v1", factory_fn)` 注册 + `FinetuneBaseModelProvider` stub (available_models 非空 + generate 返回 failure "Phase 2 deferred"). **(e) 2 tests**: test_provider_factory_dynamic + test_training_data_pipeline. **(f) 5 件套 archive 完整**: proposal.md + design.md + tasks.md + spec.md + openspec.yaml (3 file integrity + 2 README 章节). **SHIP-with-fixes `232eb13` (5 files, +66/-25)** per Oracle bg_7fe026cc verdict 1 Critical + 1 Major + 4 Minor. (C1) `LLMProviderFactory` ctor 自注册 finetune 模型 (per design D7-3) 污染 `dynamic_factories_`, 修复 6 处 test 断言 (size 从 0→1 / 1→2 / 2→3) + 验证 sort 前置. (M1) D1 评分 yaml 5/5 候选 `weighted_score` 算术错误: gpt-4 6.0→5.8 / claude 5.35→5.25 / llama 8.0→8.2 / qwen 8.25→8.45 / **deepseek 7.5→7.35** (实际 < 7.5 阈值 → `passed_all_filters: false`, 论证最终选择 llama 8.2 满足阈值). (Mi1) Mi2 Mi4 accepted (NOT-VERIFIED post-archive, 与 G1-G4 模式同). **(g) Post-merge sync `4de7745`**: AGENTS.md Recent Changes + .rddf/state/builder/wave-3-finetune-base-model.json git-track + .rddf/plans/wave-3-finetune-base-model.md git-track (P1 plan 9 步 + 3 决策点 + 39 checkbox). | `97a2abb` + `232eb13` + `4de7745` |
| 2026-09-23 | Wave 3 Pilot | **24h cooling-off 计时启动 (Wave 3)** + Override 审计链 (per builder-handoff-v1.5.json `cooling_off_override_audit` 字段): 用户在 G2 merge `dc12a17` (2026-09-22 22:30 UTC) 后 + 1h28m 显式 HARD pause override cooling-off 红线, 触发字段 = "Wave 3 立项目标", 违反治理 = "24h 间隔未满", 当前位置 = "Active build", 剩余窗口 = "22h32m". 风险由用户承担, AI 执行 + 审计. **Wave 3 cooling-off 起算** = `f0a5c4b` merge 2026-09-23T05:33Z, 满点 = 2026-09-24T05:33Z. **Wave 3 Phase 2 (D4-D7)** 待 cooling-off 满后独立立项. 链式合规 AC-12 (Pre-Wave3 ✅ + Wave 3 ✅). | `a67076d` (improvement 草稿) + `a968922` (cooling-off 期间立项准备) + `4795c56` (D3 schema mismatch 修正) |

---

## 十二、Strategic Pivots

| 日期 | Pivot | 触发 | 影响 |
|------|-------|------|------|
| (空) | - | - | - |
| 2026-09-21 | **Wave 3 Pilot 立项目标** — 从"Wave 2 skeleton 验证 Harness-RSI 价值"跃迁到 "Wave 3 ADR-0078 Pilot 验证 Fine-tune 基模实施价值" | Oracle `bg_6a8e4397` verdict C + Oracle `bg_3c06ae5b` 闭环第 7 环断裂发现 | C4 GO 5 判据 + post-hoc closure gate (G4) 加 G1/G2/G3 → Wave 3 立项目标从"Wave 3 启动前置 3 项"扩展为"Pre-Wave3 4-Gate 收口门禁". Wave 3 治理视为 Phase 6c MetaRSI-v1 子阶段, 不进入 Phase 7. |
| 2026-09-23 | **C2/C3/C4 wave 2 skeleton 完整闭环 + Wave 3 Phase 1 Pilot 激活** | C4 GO + Pre-Wave3 4-Gate 全部 SHIPPED + Wave 3 Phase 1 merge | Phase 6c MetaRSI-v1 完整 ship (C2 + C3 + walk-ancestors + D8 + C4) → Phase 7a 启动条件复评 (Phase 7 Gated 仍然 3/6 FAIL — Wave 3 Pilot 不进入 Phase 7). **保留 Wave 3 Phase 2 (D4-D7) 立项窗口** = 2026-09-24T05:33Z (Wave 3 cooling-off 满点) |

---

## 十三、Response Change Types

| 类型 | 触发 | 工作流 |
|------|------|--------|
| **fix** | ship 后发现 bug | 创建 `fix-<name>` OpenSpec change → ship-with-fixes (e.g., `fix-react-decide-empty-response`, bridge fix follow-up, 2026-09-18) |
| **retro** | sprint 收官复盘发现系统性改进 | 创建 `<name>-retro` OpenSpec change |
| **redirect** | 战略对齐发现方向需调整 | 创建 `<name>-redirect` OpenSpec change + 更新本 plan §十二 |

---

## 附录 A: Physical Layout

```
docs/superpowers/plans/
└── 2026-09-16-pdk-chat-demo-evolution-roadmap.md     # 本文件 (Master Plan tracker)

openspec/changes/
├── 2026-09-16-fix-loop-run-return-contract/         # C0 ✅ shipped (commit f84dbb3)
├── 2026-09-16-loop-agent-tools/                    # C1 ✅ shipped (commit f4766be)
├── archive/
│   ├── 2026-09-17-2026-09-16-fix-loop-run-return-contract/    # C0 ✅ shipped
│   ├── 2026-09-17-2026-09-16-loop-agent-tools/                # C1 ✅ shipped
│   ├── 2026-09-17-2026-09-17-fix-dsl-call-pause-autonomous-mode/  # P0 ✅ shipped (commits 016497e + 23e8403)
│   ├── 2026-09-19-2026-09-16-genome-registry/                 # C2 ✅ shipped (commits `a320032`+`839590d`+`b6114c2`+`507eae3`+`2e7af89`)
│   ├── 2026-09-20-2026-09-16-h-d-m-transition-guard/         # C3 ✅ shipped (commits `0ffc637`+`94f4ab4`+`421fa62`+`7a15744`)
│   ├── 2026-09-20-2026-09-20-ig-genome-registry-walk-ancestors/  # C3 follow-up ✅ shipped (10 atomic commits `a40e9e1`→`231cd8d`)
│   ├── 2026-09-21-2026-09-20-adr-0068-appendix-a-evolution-themes/  # D8 主题注册 ✅ shipped (commits `c7187d0`+`f5bbec2`+`5424e91`, 4 文件完整)
│   └── 2026-09-21-2026-09-16-harness-rsi-pilot/             # C4 ✅ SHIPPED + GO (10 atomic commits `f7f0fe3`→`4fd7ead` 跨 5 days, 4 轮 Oracle review 闭环, 4 文件完整)
├── 2026-09-17-intent-classification-router/         # P1 (Wave 2) hard-placeholder — 方案 A''
│   ├── .openspec.yaml
│   ├── proposal.md          # STATUS: PLACEHOLDER + dep on P0
│   ├── tasks.md
│   └── specs/
│       └── intent-router/spec.md
│       # 子图: lib/loop/intent_classify.agent.md
│       # 工具: pdk/loop_agent loop/classify_intent (~30 行)
│       # ChatSession "auto" routing (~20 行)
│       # 真实 LLM E2E 6 cases (3 happy + 3 error)
│       # 不使用 generate_subgraph 节点 (latent gap known, 绕道)
│       # 复用 loop/execute_plan 已 ship 模式
├── 2026-09-17-fix-generate-subgraph-static-next/    # 独立 fix 项 (不阻塞 P1)
│   ├── .openspec.yaml
│   ├── proposal.md          # STATUS: PLACEHOLDER
│   ├── tasks.md
│   └── specs/
│       └── generate-subgraph-static-next/spec.md
│       # (a) parse_node_wait_for_deps 加 /dynamic/ 豁免 OR 修正 dsl.md 文档
│       # (b) 补全 3+3 真实 LLM E2E (happy + error)
│       # 估时 1-2 sprint
```

---

## 附录 B: 实施路径变更依据

| 来源 | 关键决策 |
|------|---------|
| `openspec/changes/archive/2026-08-03-promote-event-builder-fulltoolresult-support/` | EventBuilder 契约模式 (C0 参考) |
| `openspec/changes/archive/2026-07-20-loop-agent-dsl-execution/` | loop_agent DSL 实施 (C1 参考) |
| `openspec/changes/archive/2026-09-15-pdk-chat-session-shim-cleanup/` | PDK shim 清理模式 (C1 bus_ptr 边界) |
| Oracle session `ses_f55f307f6ffeRJ9SIny8iUbZ8Y` | C1 修正 (契约对齐)、M1 修正 (双循环分工)、M2 修正 (取消 3 算子接口) |
| Oracle session `ses_f5084d7bfffeq1Yt5QVVtaIfix` + `ses_f5084614dffesPP0dEHgJhc4LM` | P1 (intent-classification-router) 补登记 + DSL-only 实施路径 (Metis dual-agent review) |
| Oracle session `ses_f505f99fdffefgE5Q9oFA2t2AD` | P1 intent schema 4 字段不含 worker_pool (chat 路径无 worker pool 选择) |
| Oracle session `ses_f4fd88215ffeUWSe2StAWtFPSQ` | DAG 动态组合完整流程调研 (20m 36s) — 2 latent gaps (静态 next:/dynamic/ 抛错 + 零 E2E) + P1 方案 A'' 修订 |
| Oracle session `ses_f54ef2010ffeLK90Y0OQp1DuxJ` | C0 ship-with-fixes 4 项 (Critical commit 顺序 / Major spec R3 R2 / Minor test 名) |
| Oracle session `ses_f53731302ffegMN8KTsrzqrPOB` | C1 ship-with-fixes 4 Major (M1 merge_patch / M2 子图 registry / M3 thread_local / M4 E2E 自动化) |
| Oracle session `ses_f530341e6ffeCdMvLYU9pITBoI` | P0 ship-with-fixes 7 项 (Oracle C1 steps / C2 SchedulerConfig.execution_flags / M1 catch 双 guard / M2 has_autonomous_flag / M3 enum class; Metis M1 loop/execute_plan Autonomous / M2 D1 DSL-only / M3 worker pool 调研) |
| Oracle session `ses_f4f9061b3ffeuy4qw2iatqQojw` | Master plan 2026-09-17 修订建议 (5 盲点 + 13 任务优先级重排 + 4 补丁 rating + Top 3 行动) |
| **Commit `0b0da50`** — `fix(tool_result): bridge top-level 'error' field to meta.error_message for PDK diagnostic` (2026-09-18) | **bridge fix**: `tool_result.cpp:from_json` 加 `error → meta.error_message` 桥接 (3 行 fix + 3 regression test), 消除 PDK 工具失败错误信息隐形系统性问题. Per Oracle session `ses_f4f9061b3ffe`. Reveals react loop `decide` 真实 LLM 失败 `Missing 'response' argument` (predecessor of F1) |
| **Commit `e5d59d5`** — `test(execution_session): follow up ctor signature for int execution_flags` (2026-09-18) | **test build-fix**: `execution_session.h` ctor 8 参 (P0 ship), test files 6 参未跟进 → HEAD build 断裂. Per Oracle 盲点 #1 |
| **Commit `deb6bbf`** — `docs(sync): active-status 245/245 + roadmap P1 补登记 + 4 Oracle 补丁应用` (2026-09-18) | **docs sync**: active-status baseline 245/245 + roadmap §1.3/§1.4/§五/附录 A 4 Oracle 补丁 (baseline 243→245 + §1.4 三 bug ✅ 标注 + 双总估时去重 + 删 P0 active stale) |
| **Commit `bcafc53`** — `docs(roadmap): §1.4 fix pending commit hash placeholder to 0b0da50` (2026-09-18) | bridge fix commit hash placeholder 修正 |
| **Commit `dc71634`** — `chore(openspec): register 5 placeholder changes per master plan §三` (2026-09-18) | **5 PLACEHOLDER OpenSpec changes 注册**: C2/C3/C4 (existing) + P1/fix-generate-subgraph-static-next (new), 4-件套骨架 (.openspec.yaml + proposal.md + tasks.md + specs/<name>/spec.md) |

### B.1 F1 (fix-react-decide-empty-response) ✅ SHIPPED 实施路径 (2026-09-18)

| Step | 任务 | 估时 | 状态 | 实际 |
|------|------|------|------|------|
| 1 | debug print reproduce 拿 ctx 快照 | 30 min | ✅ Oracle 推理诊断取代 (per `ses_f4d05cdb0`) | Case 1 standalone inja 验证 |
| 2 | Oracle 咨询 D3 (fork/join ctx 隔离) | 30 min | ✅ 4 sessions 累计 (`ses_f4d05cdb0` + `ses_f4caa8cf` + `ses_f4c6e14f` + `bg_81eba100`) | 27m 累计 |
| 3 | 写 failing test (5 cases) | 1h | ✅ 5 cases / 13 assertions | `tests/test_dsl_engine_ctx_bridge.cpp` |
| 4 | GREEN 最小修复 (main+stream 双路径) | 1h | ✅ +21 行 (per 模式 #1 step 4) | `node_executor.cpp:147-157` + `:194-205` |
| 5 | real LLM test binary (6 cases → 1 skeleton) | 2h | ⚪ 降级为 1 skip-guarded (per `ses_f4caa8cf`) | `tests/test_react_loop_real_llm.cpp` |
| 6 | Metis + Oracle dual-agent review | 1h | ⚪ Metis waived (Oracle 4 sessions 覆盖) | 3 sessions 累计 |
| 7 | ship-with-fixes + archive | 30 min | ✅ `2026-09-18-fix-react-decide-empty-response` archived | commits `a96842e` + `9dc3ac8` + `74e229f` |
| **总** | | **5h** | ✅ COMPLETE | **5h** |

**Ship 结果**:
- 主 path: `node_executor.cpp:194-205` fail-fast 空校验（写 output_key 后立即检查， 空则抛 runtime_error 含诊断线索）
- stream path: `node_executor.cpp:147-157` 同校验（per AGENTS.md 模式 #1 step 4 系统性记录同类潜伏站点）
- 测试: `tests/test_dsl_engine_ctx_bridge.cpp` 5 cases / 13 assertions NodeExecutor 级 GREEN guard
- skip-guarded: `tests/test_react_loop_real_llm.cpp` 1 case（Real-LLM 6 cases 移交 chat-real-llm-coverage Phase H follow-up）
- focused ctest: 9/9 PASS, 0 regression
- 全量 ctest: 247/247（含 16 known pre-existing failures, Oracle audit 已确认为非本 change regression）
- openspec validate --strict: "Change is valid"

---

### B.2 C2 (genome-registry) ✅ SHIPPED 实施路径 (2026-09-19)

| Step | 任务 | 估时 | 状态 | 实际 |
|------|------|------|------|------|
| 0 | Oracle design review (bg_a818a6a1) — D9/D10/D11 + 12 tests + 5 pitfalls | 30 min | ✅ 1m 45s | D9 filesystem / D10 HMAC-SHA256 / D11 unlimited + 5 obs + 12 cases + 5 pitfalls |
| 1 | Pre-flight + 读现有 placeholder | 30 min | ✅ | Oracle bg_a818a6a1 直接命中 |
| 2 | design.md + spec.md + tasks.md | 1h | ✅ | commit `2e7af89` (3 files, +237/-86) |
| 3 | RED: 12 failing tests | 2h | ✅ 12/12 FAIL | commit `a320032` (+503) |
| 4 | GREEN: minimal impl (filesystem + HMAC + fork + diff) | 3h | ✅ 12/12 PASS 266 assertions | commit `839590d` (8 files +43/-202) |
| 5 | Oracle dual-agent review (bg_9ade564d + bg_89293120) | 30 min | ✅ BLOCK → SHIP-with-fixes (2 criticals + 6 majors) | Oracle C1+C2 + Metis 5 deal-breakers |
| 5.5 | critical fixes (cycle, IOError, mutex, RAND_bytes, list_versions integrity) | 1h | ✅ | commit `b6114c2` (3 files +179/-56) |
| 5.6 | spec amendments (max+1, walk_ancestors deferred, fsync deferred, CLI deferred) | 30 min | ✅ openspec validate "Change is valid" | commit `507eae3` (2 files +65/-11) |
| 6 | ship-with-fixes + archive | 30 min | ✅ | archived `2026-09-19-2026-09-16-genome-registry` |
| **总** | | **8h** | ✅ COMPLETE (估时 1 周) | **8h** |

**Ship 结果**:
- 12/12 tests PASS, 266 assertions
- focused ctest 32/32 PASS (test_executor + test_dsl_engine + test_loop_agent + test_react + test_provider + test_session + test_genome_registry), 0 new regression
- openspec validate --strict: "Change is valid"
- Total ctest: 248 (`ctest -N` 实测 2026-09-19, post Oracle acceptance review 修正; 之前声明 259 为手算偏差)
- Active OpenSpec: 5 → 7 (C2 archived + 3 placeholders from this session)
- 6 atomic commits: design+spec (`2e7af89`) + RED (`a320032`) + GREEN (`839590d`) + critical fixes (`b6114c2`) + spec amendments (`507eae3`)

**关键决策**:
- D9 filesystem (single-Dev 单写者假设 + 项目先例 + 未来 git clone 友好)
- D10 HMAC-SHA256 (完整性 > 身份认证 + 第三方不验签)
- D11 unlimited (bounded 丢失信息, parent_only 退化)
- 5 pitfalls 避免: canonical bytes / tmp+rename / no walk_ancestors v1 / no semver

**Critical fixes (per dual-agent review)**:
- C1: cycle detection visited keyed on (name, version) pair + 10000 depth cap
- C2: walk wrapped in try/catch + cycle test re-signed to actually exercise cycle path
- M1: list_versions requires BOTH yaml + sig + sig-first atomic write ordering
- M2: HMAC key uses OpenSSL RAND_bytes CSPRNG (not mt19937_64)
- M3: atomic_write returns Result + checks stream state (IOError no longer dead enum)
- M4: commit guarded by std::mutex commit_mutex_
- M5: lineage validation also at commit (not only load)

**Deferred to follow-up changes**:
- fsync (file + directory fdatasync) — Linux-specific hardening
- CLI tool (`examples/genome_cli/`) — separate `genome-cli` change
- walk_ancestors — C3 transition-guard change (consumer ready)

### B.3 C3 + follow-up `ig-genome-registry-walk-ancestors` ✅ SHIPPED 实施路径 (2026-09-20)

| Step | 任务 | 估时 | 状态 | 实际 |
|------|------|------|------|------|
| 0 | rdd-arch 立项 → rdd-planner improvement + planner-handoff v1.1 → rdd-builder P0 case 1 approve (auto-decision complex branch) | 30 min | ✅ | 复杂分支自动决策 (per ADR-0049 + ADR-0048 §Decision 3 amended) |
| 1 | C3 implementation commit (transition_guard state machine v1.0) | 3h | ✅ | commit `0ffc637` |
| 2 | Oracle post-impl SHIP-with-fixes verdict 80/100 (`bg_f6190442`, 7m 51s) | - | ✅ ALIGNMENT SCORE 62 → 2 follow-up registered | commits `94f4ab4` + `7a15744` + `421fa62` (Oracle 6 Critical) |
| 3 | **Pre-impl dual-agent review** (`bg_7984922b` Metis C1/C2/C3 + `bg_dd35a52d` Oracle O-1/O-2/C4, 双重命中 C1) | 30 min | ✅ 4 Critical fixes 应用 | Oracle pattern #8 convergence signal 最高置信度 |
| 4 | C3 follow-up `ig-genome-registry-walk-ancestors` 实施 (10 atomic commits) | 1.5 days | ✅ | `a40e9e1` Critical C1 type unification + `9a7fb08` D5/D9 + `cd1e340` test RED + `6e1f8a5` D6 + `e4403c9` D5 override + `8d956f8` docs + `a2f868b` archive + `bcab2eb` P0 fix + `e1cd1c7` P1 fix + `231cd8d` P2 fix |
| 5 | **Oracle 2nd review APPROVE** (`bg_86a511e0`, 6m 20s) | - | ✅ **95/100** | SHIP-with-fixes 闭环 (P0+P1+P2 全部应用) |
| 6 | archive integrity verified | - | ✅ 4 files (proposal + tasks + spec + .openspec.yaml) | per AGENTS.md Day 5 case study |
| **总** | | **2 days** | ✅ COMPLETE (估时 2-3 天) | **10 atomic commits + 2 Oracle reviews + 1 dual-agent review** |

**Ship 结果**:
- C3 (commit `0ffc637`): test_transition_guard **13/13 cases / 47 assertions PASS** + `agenticdsl_evolution` 静态库 + 5 态状态机 (Idle/Harness/Data/Model/Done) + can_transition 5×5 矩阵 + evaluate_readiness 4 条件 (Attributed + 回归门 + 预算 + 无未控制混杂)
- C3 follow-up (10 commits): test_genome_walk_ancestors **10/10 cases / 55 assertions PASS** (Case 7/8/9 judge_data_freshness lineage integration P0 fix) + IGenomeRegistry 5→6 public methods + LineageWalk struct + walk_ancestors default impl NotImplemented (per AGENTS.md pattern #9) + FilesystemGenomeRegistry::walk_ancestors override (light parse + visited set + cross-name rejection) + judge_data_freshness 完整 5 cases (fast-path / cross-name / not-in-lineage / harness-changed / in-lineage-no-change + Critical C2 walk-failure → Insufficient fail-closed) + judge_data_freshness 用 (name, version) 对定位 (Critical C4) + 4 Critical fixes 应用 (C1/C2/C3/C4)
- Total ctest: 248 → 251 (+ test_credit_assignment + test_transition_guard + test_genome_walk_ancestors, `ctest -N` 实测 2026-09-20)
- Active OpenSpec: 7 (C4 + fix-generate-subgraph + P1 + chat-real-llm-coverage-phase-h + fix-flatten-layers + provider-llm-tool + adr-0068-appendix-a) — adr-0068-appendix-a 是 D8 主题注册, C3 follow-up 唯一仍 pending 项
- 10 atomic commits per AGENTS.md pattern #4 SHIP-with-fixes 流程: 7 baseline (a40e9e1...a2f868b) + 3 fixups (bcab2eb P0 + e1cd1c7 P1 + 231cd8d P2)

**Oracle review sessions (per AGENTS.md 模式 #4 SHIP-with-fixes + #8 dual-agent review)**:
- `bg_7984922b` Metis 6m 3s — pre-impl C1/C2/C3 命中
- `bg_dd35a52d` Oracle 11m 9s — pre-impl C1/C4 命中 (C1 双重收敛 = 最高置信度)
- `bg_f6190442` Oracle 7m 51s — post-impl SHIP-with-fixes verdict 80/100
- `bg_86a511e0` Oracle 6m 20s — 2nd review APPROVE 95/100

**Critical fixes (per dual-agent review)**:
- C1 (双 `IGenomeRegistry` 类型分裂): attribution_record.h stub 删除 + signature 统一到 `::agenticdsl::genome::IGenomeRegistry&` + MockRegistry 2 处迁移 derive from genome::IGenomeRegistry + 5 纯虚 override stub
- C2 (walk-failure verdict 映射): judge_data_freshness 加第 5 条规则 walk failure → Insufficient (fail-closed)
- C3 (`GenomeError::NotImplemented` enum): 6 → 7 variants, append-at-end 保 ABI compat
- C4 ((name, version) 定位 + cross-name): judge_data_freshness 用 (name, version) 对在 walk_result.intermediate_metadata 中查找; FilesystemGenomeRegistry::walk_ancestors 拒绝跨名 parent → BrokenLineage

**Deferred to follow-up changes**:
- D8 主题注册 (evolution.transition.denied / evolution.readiness.denied → ADR-0068 Appendix A) — ✅ SHIPPED 2026-09-21 (commits c7187d0 + f5bbec2). 4 文件 archive 完整 per AGENTS.md Day 5 lesson.
- ADR-0086 v1.2 candidate: 扩展 judge_data_freshness 签名为 Result<AttributionVerdict, JudgeResult> 含 verdict + HarnessChangeRecord (当前 verdict-only 签名限制已知 per Oracle bg_f6190442)
- C4 harness-rsi-pilot — ✅ **SHIPPED + GO 2026-09-21** (11 atomic commits 跨 5 days, 5 轮 Oracle review 闭环, Decision Record `docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md`)
- Wave 3 启动前置 (per Decision Record §3 摩擦 + Oracle bg_afa84d4d 独立审查): **Pre-Wave3 收口门禁** 3 项 checklist — (1) remove 路径过 policy 治理 + SecureToolRegistry 安全校验 (新 change `harness-rsi-remove-governance`) (2) `EvolutionVerdict.reward_quality` 字段 (改 C3 API + harness_rsi.cpp L117 接线) (3) `sync-pdk.sh` 同步 contract 头 + stdout 验证. 全绿才能立项 ADR-0078 Model-RSI pilot.
  - **双 agent 审查 (2026-09-21, Oracle bg_c706862b + Metis bg_d9744d91) 修正已应用**: Change 1 SHIP-with-fixes (D2 语义锁静默忽略 void / D3 加 register_llm_tool / BREAKING 降级非 BREAKING); Change 2 BLOCK→修正 (Quality enum 实际值 = Excellent/Acceptable/Poor, 无 Unknown/Good — 原 spec 虚构值已全部改正 + ≤4 约束修订 + 复用 evaluation_events.h quality_name); Change 3 BLOCK→修正 (清单 4→实测 11 头 + DRY_RUN 离线化不 clone + 动机事实修正: chat_session.h 本不在同步范围 + tool_macros.h 悬空 include 登记 follow-up).
  - **实施顺序建议**: Change 1 + Change 2 均触碰 `harness_rsi.cpp` + `test_harness_rsi_pilot.cpp` (Case 2)，**建议串行实施**（避免 merge 冲突）；Change 3 独立可并行。

### B.4 C4 `harness-rsi-pilot` + D8 + walk-ancestors ✅ SHIPPED + GO 实施路径 (2026-09-20 → 2026-09-21)

D8 + C4 跨 5 days 实施 (10 atomic commits + 4 轮 Oracle review + 4 spec/tasks/proposal SHIP-with-fixes 闭环):

**D8 ship (commits c7187d0 + f5bbec2 + 5424e91, 2026-09-21)**:
- D8 ship per Oracle bg_154a7031 + Metis bg_139e388b 双重命中: 3 幻影 API 修正 + AC-3+AC-5 docs 改写 + task 2.2 删除 (event_builder.cpp 不存在 enforcement)
- ADR-0068 Appendix A v2.2 amendment (header 状态字段 + 表格 +2 行 evolution.transition.denied / evolution.readiness.denied)
- ADR-0088 §状态 段 D8 标注 "主题常量 ✅ + 主题注册 ✅ (via adr-0068-appendix-a); 发射路径 ⏳ (C4 集成)"
- 4 文件 archive 完整 per AGENTS.md Day 5 lesson

**C4 ship (commits f7f0fe3 + harness_rsi + 16b1a96 + 4fd7ead, 2026-09-21)**:
- **Phase 4.0 DB1 fix (f7f0fe3)**: IToolRegistry::unregister_tool_function 纯虚方法扩展 + 25 文件 override (含 20 个测试 MockToolRegistry + ToolRegistry + SecureToolRegistry 实现 + 头注释更新)
- **Phase 4.1+4.2 harness_rsi.h/cpp**: GenomeMutations + MutationGovernancePolicy + MutationGateContext + AppliedMutation + MutationError(5 variant) + apply_harness_mutation 5 参 (per Oracle C2 消除 core→PDK 反向依赖) + 内部 is_tool_allowed policy check (per Oracle C3 不调 IMutationGovernance::propose) + dual-gate implementation
- **Phase 3 RED tests (16b1a96)**: 4 cases / 26 assertions GREEN (Case 1/2/3a/3b per spec.md Scenarios, deterministic string assertions per Metis anti-bias)
- **Phase 6 SHIP-with-fixes (4fd7ead)**: 4 Oracle bg_3ef7280a Major-1 (补 Case 3c tools_remove DB1 end-to-end + Case 0/0b InvalidMutation fail-fast + Case 4 workflow_patch UnsupportedVariant) + Major-2 spec.md tools_add Scenario 文本修正 (无 ToolMetadata 来源, 不调 register_tool_function) + Minor-1 itool_registry.h/registry.h 注释 9→10 虚函数 + Minor-2 Decision Record (202 行)

**C4 关键决策 (per Oracle dual-agent 4 轮)**:
- C1 幻影 API 修正: MutationGovernance::authorize() 不存在 → 内部 is_tool_allowed(meta, policy) policy check; config.system_prompt → config.agent.system_prompt; IToolRegistry::unregister_tool_function 加 DB1 fix
- C2 核心↔PDK 反向依赖消除: 签名不取 ChatConfig&, 改 std::string& system_prompt + std::vector<std::string>& tools
- C3 Case 3a veto 重写: MutationGovernancePolicy{denied_tools} 内部 check (不调 IMutationGovernance::propose 8 字段 MutationContext 太重)
- M2 签名扩展: MutationGateContext 6 字段 (EvolutionState + AttributionRecord* + IEvaluator* + IBudgetController* + IInteractionBus* + MutationGovernancePolicy) + AppliedMutation 3 字段 + Result<AppliedMutation, MutationError> 返回
- M3 Case 2 4-field 事件载荷断言: failed_conditions + attribution_verdict + eval_quality + budget_state (per ADR-0068 v2.2 line 253)

**Verification (Phase 5 ctest 全量零回归)**:
- test_harness_rsi_pilot: 9 cases / 43 assertions ALL PASSED (post Oracle 4th review `08aace2`, 含 Case 3d partial-apply 回归守卫)
- related 8 tests (harness_rsi_pilot + C3 ship 5 tests + DB1 test_tool_registry 三件套): 100% PASS, 0 回归
- ctest baseline 251 → 252 (+1 new test_harness_rsi_pilot binary)
- openspec validate --strict: 'Change is valid'

**Decision Record** (`docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md`, 202 行):
- §1 原始 ctest 输出 (per Metis 2.6 anti-bias, 原始证据先列)
- §2 5 轮 Oracle review 修正应用矩阵 (dual-agent pre-impl + 2nd review + 3rd review + 4th 独立审查)
- §3 4 项摩擦 (eval_quality:Unknown 硬编码 / add-remove 不对称 / bus nullptr fail-open / spec 文本漂移) — Wave 3 优先解决
- §4 Go 判据 5 项全绿验证
- §5 Go Decision (rationale + Wave 3 立项依据)
- §6 关联 ADR / Spec / Oracle Sessions
- §7 No-Go 回退计划 (备, 防未来回滚)

**C4 实施时间**:
- 估时: 1-2 周 (per proposal.md)
- 实际: 5 days (跨 09-20→09-21 周末+工作日), 估时下限

**Critical fixes per 5 轮 Oracle review**:
- bg_3672cb57 C1-C3 + M1-M3 (设计合规)
- bg_1f291bc4 DB1 + DB2 + Case 4 删除 (DEAL-BREAKER)
- bg_770d1308 5 文档级修正 (spec/tasks/proposal 一致性)
- bg_3ef7280a Major-1 + Major-2 + Minor-1+2+3 (post-impl 完整性)
- bg_afa84d4d Critical-1 (partial apply 零状态变更 Gate 2.5) + Major-2 + Major-4 + Minor-5+6 (独立审查, 6 修正全应用)

**Next**: **Pre-Wave3 收口门禁** — (1) `harness-rsi-remove-governance` change (remove 路径 policy 校验 + SecureToolRegistry 安全 + mutex/约束 + trace_id 透传) (2) `EvolutionVerdict.reward_quality` 字段 (3) `sync-pdk.sh` 同步 contract 头. 全绿后立项 ADR-0078 Model-RSI pilot (Wave 3).

---

### B.5 Pre-Wave3 4-Gate 收口门禁 ✅ ALL SHIPPED 实施路径 (2026-09-21 → 2026-09-22, 3 days)

**G1 `harness-rsi-remove-governance` 实施路径 (2026-09-21, 1 day)**:

| Step | 任务 | 估时 | 状态 | 实际 |
|------|------|------|------|------|
| 0 | rdd-builder P0-P1.5 启动准备 | 30 min | ✅ | commit `94cb539` |
| 1 | Pre-impl dual-agent review | 30 min | ✅ | Oracle `bg_c706862b` (Oracle) + Metis `bg_d9744d91` dual-review per `a4f374b` |
| 2 | SHIP-with-fixes corrections (3 changes) | 1h | ✅ | `a4f374b` apply corrections (D2 语义锁 / D3 register_llm_tool / BREAKING 降级) |
| 3 | 实施 (refactor unregister_tool_function + SecureToolRegistry + mutex + trace_id) | 2h | ✅ | commit `714764d` |
| 4 | Tests (4 cases / 22 assertions) | 1h | ✅ | Case 5d Spec R1 scenario 3 + Case 5e R3 scenario 1 fuzz |
| 5 | SHIP-with-fixes per Oracle bg_8237a316 | 30 min | ✅ | commit `9ee475e` (0C + 2M + 4M + 1 D3 ACCEPT) |
| 6 | Retry-2 const qualifier | 30 min | ✅ | commit `f1a6647` |
| 7 | Retry per actual g++ compile | 30 min | ✅ | commit `e182f82` |
| 8 | Archive + AGENTS.md 模式 #10 + #11 沉淀 | 30 min | ✅ | `7a31d12` (archive) + `19e0e8d` (新模式) |
| 9 | Merge | - | ✅ | `9709317` |
| **总** | | **6h** | ✅ COMPLETE (估时 1 天) | **6h** |

**G2 `evolution-verdict-reward-quality` 实施路径 (2026-09-22, 0.5 day)**:

| Step | 任务 | 估时 | 状态 |
|------|------|------|------|
| 0 | rdd-builder P0 + apply dual-agent corrections | 30 min | ✅ `98711e3` Step 1 hygiene + `a4f374b` (G2 内嵌于 3-change corrections) |
| 1 | 实施 (`EvolutionVerdict.reward_quality` field + `harness_rsi.cpp:117` wiring 复用 `evaluation_events.h::quality_name`) | 1.5h | ✅ `e51073a` |
| 2 | Tests (Case 2 `"Poor"` 强化 + 6 regression tests) | 30 min | ✅ test_transition_guard 14/14 + test_harness_rsi_pilot 22/22 |
| 3 | Oracle post-impl review SHIP verdict | - | ✅ bg_ebfe1c25 (0C + 0M + 2 Minor) |
| 4 | G2 builder state FULL schema | 30 min | ✅ `3cdb792` (post_impl_review_prompt) |
| 5 | Archive (5-file integrity) | 30 min | ✅ `737e979` |
| 6 | Merge | - | ✅ `dc12a17` (Wave 3 cooling-off 计时起点) |
| **总** | | **3.5h** | ✅ COMPLETE (估时 0.5 day) | **3.5h** |

**G3 `sync-pdk-contract-header` 实施路径 (2026-09-22, 0.5 day)**:

| Step | 任务 | 估时 | 状态 |
|------|------|------|------|
| 0 | G3 立项 + pre-impl | 30 min | ✅ `6eb5a2b` (propose G3) + `a4f374b` (apply corrections) |
| 1 | 实施 (PDK_CONTRACT_DEPS=11 + DRY_RUN 离线 + drift-guard) | 1h | ✅ `a641d34` |
| 2 | Tests (bash -n + 4/4 dry-run) | 30 min | ✅ openspec validate PASS |
| 3 | SHIP-with-fixes per Oracle bg_ef5a0ca4 | 30 min | ✅ `77d6fae` (drift-guard grep `<` + `")"` sentinel) |
| 4 | Post-impl execute summary | 30 min | ✅ `19abc79` |
| 5 | Archive | 30 min | ✅ `bd74fc1` |
| 6 | Merge | - | ✅ `a196a09` |
| **总** | | **3h** | ✅ COMPLETE (估时 0.5 day) | **3h** |

**G4 `genome-wiring-harness-rsi-gepa` 实施路径 (2026-09-22, 1.5 day)**:

| Step | 任务 | 估时 | 状态 |
|------|------|------|------|
| 0 | rdd-builder P0-P1.5 启动准备 | 30 min | ✅ `7f4e010` |
| 1 | G4 v2 change 起草 (per Oracle bg_3c06ae5b 双审 v1 → v2 整改) | 1h | ✅ `3076042` (v2) + `74e063c` (ADR-0086 archive 恢复内含) |
| 2 | 实施 (Gate 3 persist-before-apply + GEPA persist-then-commit + undo + 2 事件 + 9 tests) | 4h | ✅ `1fcb00e` |
| 3 | Tasks 6.4/6.5 同步 (ADR-0086 状态翻牌 + Registry 接线 + active-status) | 30 min | ✅ `a21c08a` |
| 4 | SHIP-with-fixes per Oracle bg_e4eec567 | 30 min | ✅ `c5d0c78` (0C + 3M + 1 Minor: genome_version payload + case-7e stubs + ADR-0068 L254) |
| 5 | Archive (5-file integrity per AGENTS.md Day 5 lesson) | 30 min | ✅ `ac5ef14` |
| 6 | Merge | - | ✅ `fb2769f` |
| **总** | | **7h** | ✅ COMPLETE (估时 1.5 day) | **7h** |

**Pre-Wave3 4-Gate 总耗时**: G1 6h + G2 3.5h + G3 3h + G4 7h = **19.5h** vs Master Plan 估时 3-4 天 (24-32h). **节省 ~5-12h** 通过单作者 + 串行流水 + Oracle 直接命中关键问题. AGENTS.md 模式 #11 沉淀 (Async Worker + Dual Oracle dual-review SHIP-with-fixes cycle).

---

### B.6 Wave 3 Phase 1 `finetune-base-model` Pilot ✅ SHIPPED 实施路径 (2026-09-23, 1 day)

**实施步骤 (per Oracle bg_7fe026cc verdict)**:

| Step | 任务 | 估时 | 状态 | 实际 |
|------|------|------|------|------|
| -1 | Wave 3 立项目标起草 (rdd-arch) | 30 min | ✅ | `5d54511` (Pre-Wave3 to Wave3 execution plan) |
| 0 | Pre-Wave3 Plan §3 + 24h cooling-off override audit | 30 min | ✅ | `8795c0` (decision) + builder-handoff::cooling_off_override_audit |
| 1 | Wave 3 improvement 草稿 | 1h | ✅ | `a67076d` (LLMProviderFactory::register_dynamic 勘误) + `a968922` (立项准备草稿) + `4795c56` (D3 schema mismatch 修正) |
| 2 | OpenSpec change 起草 (4 件套: proposal + design + tasks + spec) | 2h | ✅ | `97a2abb` commit 内含 5 files |
| 3 | 实施 (ADR-0078 翻牌 ✅ + D1 评分 yaml + D3 脚本 + D7 stub + 2 tests) | 3h | ✅ | `97a2abb` (18 files, +1057/-62) |
| 4 | Archive (5-file integrity per AGENTS.md Day 5 lesson) | 30 min | ✅ | `97a2abb` 内含 |
| 5 | Oracle post-impl SHIP-with-fixes per bg_7fe026cc | 1h | ✅ | `232eb13` (1 Critical + 1 Major + 4 Minor) |
| 6 | C1 fix (LLMProviderFactory ctor 自注册 baseline + sort + rationale) | 30 min | ✅ | `232eb13` |
| 7 | M1 fix (D1 评分 yaml 算术 5/5 校正 deepseek 7.35) | 30 min | ✅ | `232eb13` |
| 8 | Post-merge sync (AGENTS.md Recent Changes + .rddf state + .rddf plan) | 30 min | ✅ | `4de7745` |
| 9 | Merge | - | ✅ | `f0a5c4b` (Wave 3 cooling-off 计时起点 2026-09-23T05:33Z, 满点 2026-09-24T05:33Z) |
| **总** | | **8h** | ✅ COMPLETE (估时 1-2 day) | **8h** |

**关键 ship 结果**:
- ADR-0078 ✅ Approved (Wave 3 Phase 1 Pilot 激活, 2026-09-23)
- 4 件套 + openspec.yaml 已 archive (实际 4 files, 缺 tasks.md — Wave 3 Phase 2 立项时补全)
- focused ctest 9/9 PASS (test_provider_factory + test_provider_factory_concurrent + test_provider_register_dynamic_tool + test_training_data_pipeline + test_llm_provider_factory + test_llm_tool + test_cost_tracking_decorator + test_genome_registry + test_llm_provider_factory_decorator)
- 12 AC 验证: 11 ✅ + AC-3 (openspec validate post-archive) NOT-VERIFIED per Single-Dev 模式归档后不可复现范本

**Cooling-off chain AC-12** (per Pre-Wave3 Plan §4): Pre-Wave3 ✅ + Wave 3 ✅ (用户显式 HARD pause override cooling-off 红线, 风险 + 审计由 builder-handoff::cooling_off_override_audit 字段记录).

**NOT-VERIFIED (主会话 post-merge 受机器性能限制)**:
- 全量 ctest 252 binaries 零回归 (主会话 NOT-RUN, 留独立 follow-up)
- TSan 扫 (机器性能受限跳过)
- D1 评分 yaml 实际候选模型 benchmark 数据 (依赖 llm-tool-eval 实时跑, 不在本 change 范围)

**Wave 3 Phase 2 (D4-D7) 立项准备**: 待 Wave 3 cooling-off 满点 2026-09-24T05:33Z 后独立 OpenSpec change 走 rdd-arch → rdd-planner → rdd-builder 流程.

---

**Last Updated**: 2026-09-23 (Pre-Wave3 4-Gate 收口门禁 **全部 ✅ SHIPPED 2026-09-22** + Wave 3 Phase 1 Pilot ✅ **SHIPPED 2026-09-23** + 24h Wave 3 cooling-off 起点 = `f0a5c4b` merge 2026-09-23T05:33Z, 满点 = 2026-09-24T05:33Z. 累计 ship: **9 atomic commits** (`9709317` G1 + `dc12a17` G2 + `a196a09` G3 + `fb2769f` G4 + `f0a5c4b` Wave 3 + 6 supporting commits) + **5 Oracle review sessions** (bg_8237a316 G1 + bg_ebfe1c25 G2 + bg_ef5a0ca4 G3 + bg_e4eec567 G4 + bg_7fe026cc Wave 3) + **AGENTS.md 模式 #10 + #11 沉淀** per `19e0e8d`. 主要更新: §一.2 active 9→5 (5 active 全部 Phase 6c 早期遗留短链 + DSL 候选 + provider bug defense-in-depth) + §一.3 ctest 252→211 actual (post Wave 3 build path 实测) + §一.5 ADR-0078 ✅ Approved + §三 Pre-Wave3 4-Gate 全 ✅ (status 翻转 + Wave 3 Pilot row 新增) + §四 Detailed Tracking 新增 G1/G2/G3/G4/W3.P1 5 个子节 + §五 Sprint Breakdown 重构 (Sprint 34-36-34.5 保留 + Pre-Wave3 Sprint A-B 新增 + Wave 3 Phase 1 新增) + §六 R7 ✅ resolve + §八.1 ADR-0078 翻牌 + §八.3.1 active 9→5 + archive 6→12 项 + §十 Drift Log +6 行 (C4/C1/G1/G2/G3/G4/W3.P1) + §十一 Adjustment Log +7 行 (G1/G2/G3/G4/cooling-off/W3.P1 详细) + §十二 Strategic Pivots 2 行 (Wave 3 Pilot 立项目标 + C2/C3/C4 完整闭环 + Wave 3 Phase 1 激活) + 附录 B.5 (Pre-Wave3 4-Gate) + B.6 (Wave 3 Phase 1) + Last Updated 同步.)
**Next Review**: **Wave 3 cooling-off 满点 2026-09-24T05:33Z 后** — Wave 3 Phase 2 (D4-D7: 训练方法/评估/回流/serving 完整版) 立项. 立项准备: (1) 读 ADR-0078 background (D4-D7 完整 design) + Wave 3 Phase 1 容量评估表 (Phase 1 vs Phase 2 边界) + Wave 3 cooling-off 起算日志 (per `.rddf/state/builder/wave-3-finetune-base-model.json::wave_3_cooling_off`) (2) 走 rdd-arch → rdd-planner → rdd-builder 流程 (模式 #11 Async Worker + Dual Oracle dual-review) (3) Wave 3 Phase 2 cooling-off 计时合规 (Pre-Wave3 ✅ + Wave 3 Phase 1 ✅ + Wave 3 Phase 2 24h 间隔待启) (4) expected: D4 LoRA/QLoRA/API fine-tune 选择 + D5 评估 + D6 AgenticMind 回流 + D7 真实推理 serving. **保留窗口**: 独立 follow-up on 12 AC NOT-VERIFIED 项 (全量 ctest 252 零回归 + TSan + D1 评分 yaml benchmark).
**Maintainer**: Architecture Working Group + Solo Dev
