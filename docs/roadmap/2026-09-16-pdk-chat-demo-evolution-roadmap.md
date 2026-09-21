# Master Plan: pdk-chat-demo Evolution & Self-Evolution Agent Framework

> **驱动诊断**: pdk_chat_demo 跑真实 LLM 模式返回 0 步 + 空 Assistant
> **驱动愿景**: 在 HydraForge "DSL 执行引擎" 核心使命内搭建 Harness-RSI 闭环骨架
> **覆盖**: Sprint 34-36 (3 Sprints, ~3-4 周)
> **生成日期**: 2026-09-16
> **最后验证**: 2026-09-20（C3 follow-up `ig-genome-registry-walk-ancestors` SHIPPED + 10 atomic commits `a40e9e1`→`231cd8d` + Oracle dual-agent pre-impl review (`bg_7984922b` Metis C1/C2/C3 + `bg_dd35a52d` Oracle O-1/O-2/C4, 4 Critical fixes) + Oracle post-impl SHIP-with-fixes verdict 80/100 (`bg_f6190442`) + Oracle 2nd review **APPROVE 95/100** (`bg_86a511e0`) — judge_data_freshness lineage integration tests GREEN + canonical spec sync 3 处 + ADR-0088 status 翻牌 + D5/D6/D9 ship 标记 + `agenticdsl_evolution` 静态库扩展 transition_guard.cpp + `test_genome_walk_ancestors` 10/10 cases / 55 assertions PASS）
> **作者**: Architecture Working Group + Oracle 评审 (`task_id=ses_f55f307f6ffeRJ9SIny8iUbZ8Y`)
> **状态**: 🔍 Proposed (Master Plan 草案，待 24h cooling-off + GitHub Issue self-review)

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

---

## 一、Baseline (项目当前状态)

### 1.1 Phase 进度
- **Phase 6c** (2026-08-19 ~ 09-09, ~80h) — 🟢 实质 ship 完成 (11/13 任务)
- **Sprint 33+** (Day 1-5 2026-09-16) — real-LLM 验证 + 收盘，7 changes ship + archived
- **Phase 7** — ⏸ Gated (3/6 启动条件 FAIL，结构性不满足 Solo Dev ~27h/周)
- **Phase 8a/b** — ⏸ Gated by Phase 7a ship ≥3 月

### 1.2 Active vs Archive
- **openspec/changes/** 当前 active: **7** (per `ctest -N` 实测 2026-09-20):
  - `harness-rsi-pilot` (C4 — 🟡 Ready, unblocked)
  - `fix-generate-subgraph-static-next` (latent gap #1 fix)
  - `intent-classification-router` (P1)
  - `chat-real-llm-coverage-phase-h` (real-LLM E2E 6 cases follow-up)
  - `fix-flatten-layers-comment-drift` (drift cleanup)
  - `provider-llm-tool-empty-passthrough` (provider bug)
  - `adr-0068-appendix-a-evolution-themes` (D8 主题注册, 唯一 Sprint 34+ 仍 active follow-up)
- **最近 archive** (2026-09-19 → 2026-09-20): 3 new
  - `2026-09-19-2026-09-16-genome-registry` (C2)
  - `2026-09-20-2026-09-16-h-d-m-transition-guard` (C3)
  - `2026-09-20-2026-09-20-ig-genome-registry-walk-ancestors` (C3 follow-up — D5/D6/D9 ship)

### 1.3 关键 Baseline 数据
- **ctest baseline**: **251/251 PASS** (实测 `ctest -N` 2026-09-20; 248/248 baseline + test_credit_assignment + test_transition_guard + test_genome_walk_ancestors; per `docs/active-status.md`)
- **adr_lint.py**: 0 errors
- **docs_drift_audit**: 0 DRIFT items
- **openspec validate**: clean

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

---

## 二、Dependency Graph

```
[Wave 1 - 修 chat demo, P0 必要]
═══════════════════════════════════════════════════════════════
   C0 (fix-loop-run-return-contract) ⊥ C1 (loop-agent-tools)
              │                            │
              └─────────┬──────────────────┘
                        ▼
                 [Sprint 34 ship gate]
                        │
                        ▼
[Wave 2 - 自进化骨架, P1 中期]
═══════════════════════════════════════════════════════════════
              C2 (genome-registry) ──── hard dep on C0+C1
                        │
                        ▼
              C3 (h-d-m-transition-guard) ──── hard dep on C2
                        │
                        ▼
                 [Sprint 35 ship gate]
                        │
                        ▼
[Wave 2.5 - Pilot 实验, P2 紧跟]
═══════════════════════════════════════════════════════════════
              C4 (harness-rsi-pilot) ──── hard dep on C3
                        │
                        ▼
              [Go / No-Go Decision Gate]
                        │
            ┌───────────┴───────────┐
            ▼                       ▼
         [Go]                  [No-Go]
    扩展 Model-RSI 方向       归档 Wave 2 skeleton
    (立项 ADR-0078 pilot)    等待需求驱动
```

### 依赖关系详细

| 关系 | 类型 | 含义 | 调度 |
|------|------|------|------|
| C0 → C1 | **parallel (none)** | 两者独立 | Sprint 34 同 Sprint 并行 |
| C0 + C1 → C2 | **hard** | C2 需要 chat demo 能跑通才能采集真实事件流 | Sprint 35 起 |
| C2 → C3 | **hard** | C3 状态机需要 Genome 版本号才能判断"过期数据" | Sprint 35 内 |
| C3 → C4 | **hard** | Pilot 需要 H→D→M 守卫 | Sprint 36 |
| P0 → P1 | **hard** | P1 classify→execute 链路需要 P0 让 Autonomous 模式跑通（已 ship ✅） | Sprint 36+ deferred |
| P1 → (无 hard dep) | none | P1 纯 DSL 实现，可独立 ship 不阻塞 C2/C3/C4 | 可与 C4 并行 |
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
| **C4** | `harness-rsi-pilot` — **🟡 Ready** (C3 + walk_ancestors follow-up + judge_data_freshness 完整实装 + lineage tests GREEN 全部 ship 2026-09-20 → unblocked). 仍需 D8 主题正式注册 (`2026-09-20-adr-0068-appendix-a-evolution-themes` follow-up 未启动) 后开始 Go/No-Go 门. | ~~hard-placeholder~~ | 1-2 周 | C3 ✅ (含 D5/D6/D9 follow-up) | 🟡 Ready (unblocked) | 36 |
| **F1** | `fix-react-decide-empty-response` (Wave 34.5 follow-up) — react agent loop 在 `decide` 节点真实 LLM 端到端 `Missing 'response' argument` 修复。✅ **SHIPPED** (2026-09-18, commits `a96842e` + `9dc3ac8`, archived `2026-09-18-fix-react-decide-empty-response`) — 根因 = think 节点 LLM 空 text silent 穿透, fix = `node_executor.cpp` main + stream 双路径 fail-fast 空校验 +21 行. Real-LLM 6 cases 降级为 1 skip-guarded skeleton (`tests/test_react_loop_real_llm.cpp`) + 移交 chat-real-llm-coverage Phase H follow-up. | hard-placeholder | **5h → DONE** | `0b0da50` (bridge fix) + `chat-real-llm-coverage` helper | ✅ SHIPPED | **34.5** (post-Wave-1) |

**总估时**: 4-5 周 + F1 5h ≈ **5 周**（基线 C0+C1+P0 已 ship + C2+C3 Sprint 35 + C4 Sprint 36 + P1 deferred → Sprint 36+ 候选与 C4 并行 + generate_subgraph fix 项独立 1-2 sprint + F1 Wave 34.5 follow-up）
**P1 实施方案**: 方案 A'' (Oracle `ses_f4fd88215ffeUWSe2StAWtFPSQ` 推荐)，2 次平级 loop/run + 复用 `loop/execute_plan` 模式
**P1 worker pool routing**: 不需要单独决策（Oracle `ses_f505f99fdffefgE5Q9oFA2t2AD`）
**P1 intent schema**: 4 字段 `intent_type/complexity/suggested_loop/requires_subgraph`，不加 `worker_pool`

(原 "总估时: 3-4 周" Oracle `ses_f55f307f6ffeRJ9SIny8iUbZ8Y` 评审版已被 P1 补登记后的 4-5 周 supersede — 详 §十一 Adjustment Log)

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
1. `pdk/genome/spec/genome-v1.yaml` — Genome CRD schema (apiVersion, metadata{name,version,parent,created_by,capture_mode}, spec{harness{system_prompt,loop_type,workflow}, tools[], budget, model_routing, prompt_cache_prefix})
2. `include/agenticdsl/genome/registry.h` — `IGenomeRegistry` 接口 (load(name@version) / commit(genome) / fork(name, parent, mutations) / list_versions(name) / diff(v1, v2))
3. `src/core/genome/registry.cpp` — 文件系统后端 (`~/.hydraforge/genomes/<name>/<version>/genome.yaml`)
4. 谱系追踪：commit 强制 `parent` 字段必填
5. 完整性校验：HMAC 签名 + schema validation
6. 8 个 test case (load/commit/fork/diff/version_listing/parent_validation/HMAC/invalid_schema)

**Out of Scope**:
- Git-LFS 后端（filesystem 优先）
- Genome-aware ChatSession/DSLEngine 构造参数（避免 BREAKING，留作 follow-up）

**Verification**: Genome commit/fork/rollback 往返 + 版本 diff 确定性测试

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

---

### Sprint 35 (Wave 2: 自进化骨架, ~1.5 周, P1 中期)

| Change | 估时 | Type | 任务 |
|--------|------|------|------|
| **C2** `genome-registry` | 1 周 | hard | Genome CRD + IGenomeRegistry + filesystem 后端 |
| **C3** `h-d-m-transition-guard` | 2-3 天 | hard | ~200 行状态机 + can_transition H→D→M 强制 |

**并行**: C2 和 C3 顺序（C3 依赖 C2 的 Genome 版本号接口）

**Ship Gate**:
- [ ] Genome commit/fork/rollback 往返测试通过
- [ ] can_transition(H→M) 编译期+运行期双重断言通过
- [ ] 12 个 transition guard test case 全部 PASS
- [ ] ctest 零回归
- [ ] dual-agent review (Metis + Oracle) 通过

---

### Sprint 36 (Wave 2.5: Pilot 实验, ~1-2 周, P2 紧跟)

| Change | 估时 | Type | 任务 |
|--------|------|------|------|
| **C4** `harness-rsi-pilot` | 1-2 周 | hard | IHarnessRSI 首个实现 + 端到端 mock 闭环 + 真实 LLM 1 turn |

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
- [ ] 写 failing test (3 cases: ctx bridge / type / empty arg)
- [ ] 实施最小修复 (1 file + ~10 行 per AGENTS.md "fix minimally")
- [ ] 加 real LLM test binary `tests/test_react_loop_real_llm.cpp` (3 react + 2 plan_execute + 1 fork_join cases)
- [ ] Metis + Oracle dual-agent review + 应用所有 Critical/Major 修正
- [ ] ctest 全量 245/245 零回归
- [ ] adr_lint 0 errors
- [ ] docs_drift_audit 0 DRIFT
- [ ] openspec validate clean
- [ ] 真实 DeepSeek LLM 端到端验证 (手动 `HYDRAFORGE_SKIP_REAL_LLM=0 ctest -R test_react_loop_real_llm --output-on-failure`)
- [ ] openspec archive + 更新 §一.4 Bug 3 残量风险为 ✅ FIXED

**变更依据**: 本 master plan + OpenSpec change `fix-react-decide-empty-response/`.

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
- **ADR-0086** Credit Assignment Contract: `docs/adr/adr-0086-credit-assignment-contract.md` (🔍 Proposed)
- **ADR-0080** AppendOnlyEventLog: `docs/adr/adr-0080-append-only-event-log.md`
- **ADR-0061-13** Distillation Output Format: `docs/adr/skill/adr-0061-13-distillation-output-format.md`
- **ADR-0078** Fine-tune Base Model: `docs/adr/adr-0078-finetune-base-model.md` (🔍 Proposed, Model-RSI 依赖)

### 8.2 关键架构文档
- `docs/architecture/self-evolution-architecture-2026-08.md` (🔍 Proposed, 边界定义)
- `docs/architecture/agent-evolution-pipeline.md` (✅ Approved, 4 阶段管线)
- `docs/architecture/axis6-chain-workflow-architecture-2026-08.md` (🔍 Proposed v1.1, 7 缺口)
- `docs/architecture/capability-application-map-2026-08.md` (✅ Active v2.6.0, 33 项能力 — `#32` Credit Assignment Contract v1.0+v1.1 + `#33` H→D→M Transition Guard v1.0 ship 2026-09-20)

### 8.3 关键既有 OpenSpec archive (pattern 参照)
- `openspec/changes/archive/2026-07-20-loop-agent-dsl-execution/` (loop_agent DSL 实施, Change 2 模板)
- `openspec/changes/archive/2026-09-15-pdk-chat-session-shim-cleanup/` (PDK shim 清理, Change 1/2 模板)
- `openspec/changes/archive/2026-08-03-promote-event-builder-fulltoolresult-support/` (EventBuilder V2, 契约模式)

### 8.3.1 活跃 OpenSpec changes (PLACEHOLDER 状态)
- `openspec/changes/2026-09-16-genome-registry/` (C2, hard-placeholder)
- `openspec/changes/2026-09-16-h-d-m-transition-guard/` (C3, hard-placeholder)
- `openspec/changes/2026-09-16-harness-rsi-pilot/` (C4, hard-placeholder Go/No-Go)
- `openspec/changes/2026-09-17-intent-classification-router/` (P1, hard-placeholder 方案 A'')
- `openspec/changes/2026-09-17-fix-generate-subgraph-static-next/` (latent gap fix, hard-placeholder)
- `openspec/changes/fix-react-decide-empty-response/` (F1, hard-placeholder, 2026-09-18 立项)

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
| 2026-09-17 | 34 | **C0 ship-with-fixes**: Oracle review (session `ses_f54ef2010ffeLK90Y0OQp1DuxJ`) 发现 4 项 (Critical commit 顺序, Major spec R3/R2 producer-correction, Minor test 名 + spec text alignment). 全部修正 ship. | 应用 Critical/Major/Minor; 3 atomic commits `f84dbb3` + `d21ac6f` + `f3fbb9d`; archive 5 文件完整 | `f84dbb3` + `d21ac6f` + `f3fbb9d` |
| 2026-09-17 | 34 | **C1 ship-with-fixes**: Metis dual-agent review (session `ses_f53731302ffegMN8KTsrzqrPOB`) 发现 4 Major (M1 merge_patch 语义未定义, M2 子图 registry 隔离 NOTE 缺失, M3 thread_local per-thread 声明缺失, M4 E2E 自动化缺失). 全部修正 ship. | design.md D2/D4 加 NOTE, tasks.md §6.1 改自动化 E2E; deep agent `bg_9a5f7c89` 实施 22 case PASS; atomic commit `f4766be`; archive 5 文件完整 (路径 `2026-09-17-2026-09-16-loop-agent-tools` 因当前日期 9-17 + 创建日期 9-16 双前缀) | `f4766be` |
| 2026-09-17 | 34 | **P0 (fix-dsl-call-pause-autonomous-mode) ship-with-fixes**: Oracle+Metis dual-agent review (sessions `ses_f530341e6ffeCdMvLYU9pITBoI` + `ses_f53731302ffegMN8KTsrzqrPOB`) 发现 7 项 (Oracle C1 steps, C2 SchedulerConfig.execution_flags 透传路径, M1 catch 双 guard, M2 has_autonomous_flag 包装, M3 enum class 单 flag; Metis M1 loop/execute_plan Autonomous, M2 D1 DSL-only 实现, M3 worker pool 调研). 全部修正 ship. | 基础设施 11 文件 +209/-3 (`016497e`) + D4 wiring 4 文件 +308/-8 (`23e8403`); 7 test_loop_agent_autonomous + 7 test_e2e_mock 全 PASS (真实 DeepSeek LLM "Hello" 验证); Option A mock_fallback 修回归 (Metis M3 衍生) | `016497e` + `23e8403` |
| 2026-09-17 | - | **Master Plan 补登记 Wave 2 P1**: P0 proposal L182 提及"P1 (intent 分类 + loop_type 路由) — 独立 change" 但 master plan 未登记. Metis+Oracle dual-agent review 确认 P1 真实存在, 完全可通过纯 DSL 图节点实现 (`lib/loop/intent_classify.agent.md` + switch + loop/run_subgraph + 可选 generate_subgraph). | §三表格添加 P1 行 (deferred → Sprint 36+), §二 Dependency Graph 添加 P0→P1 hard + P1→(none), 顶部 ⚠️ 更新日志记录本次同步, §十一 Adjustment Log 添加 P1 row. 估时修订 3-5 天 → 2-3 天. | (本 commit) |
| 2026-09-17 | - | **Master Plan 补登记 Worker Pool 调研结论**: Oracle 调研 (session `ses_f505f99fdffefgE5Q9oFA2t2AD`) 确认 chat 路径无 worker pool 选择 (CognitiveWorker 生产零使用, DomainWorkerPool 唯一生产消费者是 C++ ForkJoinLoop 但 chat 走 DSL TopoScheduler Taskflow). | P1 intent schema 不加 `worker_pool` 字段. 顶部更新日志记录. | (本 commit) |
| 2026-09-17 | - | **DAG 动态组合完整流程调研完成**: Oracle session `ses_f4fd88215ffeUWSe2StAWtFPSQ` (20m 36s). **新发现 2 个 Latent Gap** (此前未记录): (1) 静态 `next: "/dynamic/..."` 在 `parse_node_wait_for_deps` (topo_scheduler.cpp:88-95) 抛 "Next node not found", 无 `/dynamic/` 豁免; dsl.md §423/§438/§1114 与实现矛盾. (2) generate→register→execute 全链路无任何端到端测试; 现有 2 个 "E2E" 名义测试实为 prompt smoke, 真实 LLM 对 `execute_generate_subgraph` 覆盖 = 零. **关键架构事实**: plan_execute.agent.md 已 ship 的"LLM 生成子图→执行"走 `loop/execute_plan` 工具 (独立子引擎), **不走 generate_subgraph 节点**. | §三 P1 Row 修订为方案 A'' (推荐): ChatSession 两次平级 loop/run + 复用 `loop/execute_plan` 模式 + 不使用 generate_subgraph 节点. §十一 Adjustment Log 加 P1 方案 A'' + generate_subgraph 节点 deferred 行. | (本 commit) |
| 2026-09-18 | 34.5 | **F1 `fix-react-decide-empty-response` SHIPPED** (Oracle 3 sessions 累计审计). **根因修正 (per `ses_f4d05cdb0`)**: 初判 (flatten_layers 嵌套) 错; 正确根因 = think 节点 LLM 空 text silent 穿透 → inja 静默渲染 "" → decide_react "Missing 'response' argument". **最小修复**: `node_executor.cpp:194-205` main path + `:147-157` stream path 双路径 fail-fast 空校验 +21 行 (per AGENTS.md 模式 #1). **测试**: 5 cases / 13 assertions NodeExecutor 级 + 1 skip-guarded real-LLM skeleton. focused ctest 9/9 PASS 0 regression. 全量 ctest 16 known pre-existing failures (Oracle audit 已确认非本 change regression). openspec validate --strict → "Change is valid". **Metis waived**: change 收敛为 minimal fix, Oracle 3 sessions 累计覆盖, docs drift 由 Oracle session 3 (`ses_f4c6e14f`) 直接发现并修正 (3 处 spec drift + design 重复节删除). Single-Dev 模式自审决议合法. archived `2026-09-18-fix-react-decide-empty-response` (6 files verified, Day 5 lesson 避免). §一.4 Bug3 残量风险句更新 ✅ FIXED + 双 commit hash 引用 + §四 F1 子节 9 TODO 全勾选 + §十一 Adjustment Log 新增 4 行 (ship / §5 降级 / spec drift / ctest 247). | `a96842e` + `9dc3ac8` |

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

---

## 十二、Strategic Pivots

| 日期 | Pivot | 触发 | 影响 |
|------|-------|------|------|
| (空) | - | - | - |

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
│   └── 2026-09-20-2026-09-20-ig-genome-registry-walk-ancestors/  # C3 follow-up ✅ shipped (10 atomic commits `a40e9e1`→`231cd8d`)
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
└── 2026-09-20-adr-0068-appendix-a-evolution-themes/  # D8 主题注册 (Sprint 34+ follow-up, 仅 active 项 per C3 Oracle NEEDS_FIX)
    ├── proposal.md
    ├── tasks.md
    ├── specs/adr-0068-appendix-a/spec.md
    └── .openspec.yaml

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
- D8 主题注册 (evolution.transition.denied / evolution.readiness.denied → ADR-0068 Appendix A) — `2026-09-20-adr-0068-appendix-a-evolution-themes` registered, 未启动. C4 harness-rsi-pilot Go/No-Go 门仍需此项 ship.
- ADR-0086 v1.2 candidate: 扩展 judge_data_freshness 签名为 Result<AttributionVerdict, JudgeResult> 含 verdict + HarnessChangeRecord (当前 verdict-only 签名限制已知 per Oracle bg_f6190442)
- C3 harness-rsi-pilot (C4) — **🟡 Ready unblocked** (D5/D6/D9 全 ship, 但 D8 主题注册仍 pending)

---

**Last Updated**: 2026-09-20 (C3 h-d-m-transition-guard ✅ SHIPPED + C3 follow-up `ig-genome-registry-walk-ancestors` ✅ SHIPPED — 10 atomic commits `a40e9e1`→`231cd8d`. Oracle dual-agent pre-impl review (bg_7984922b Metis C1/C2/C3 + bg_dd35a52d Oracle O-1/O-2/C4, 4 Critical fixes applied with C1 double-convergence 最高置信度) + Oracle post-impl SHIP-with-fixes verdict 80/100 (bg_f6190442) + Oracle 2nd review **APPROVE 95/100** (bg_86a511e0). test_transition_guard 13/13 + test_genome_walk_ancestors 10/10 + test_credit_assignment 12/12 ALL GREEN. ctest 248→251. Active OpenSpec 7 (含 adr-0068-appendix-a D8 主题注册仍 pending). §三 Overview C3 ✅ SHIPPED + §四 C3 子节 7 TODO 全 ✅ + §十 Drift Log +2 行 + §十一 Adjustment Log +3 行 + 附录 B.3 C3 + follow-up 实施路径. ADR-0088 v1.0 + ADR-0086 v1.1 翻牌 + Canonical spec sync 3 处 (genome-registry 6 methods / transition-guard self-inclusive / self-evolution §七 #6a). + 治理债显式化 (D8 主题注册 + ADR-0086 v1.2 verdict-only 签名限制 noted))
**Next Review**: C4 harness-rsi-pilot 启动前 (D8 主题注册 ship 后)
**Maintainer**: Architecture Working Group + Solo Dev
