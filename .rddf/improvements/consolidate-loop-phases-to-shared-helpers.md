# Improvement: consolidate-loop-phases-to-shared-helpers

**日期**: 2026-10-09
**对应 ADR**: [ADR-0089 v1.3](../docs/adr/adr-0089-v1-3-amendment-loop-phase-shared-helpers.md) (🔍 Proposed)
**父 ADR**: [ADR-0021 §3.2](../docs/adr/adr-0021-pdk-design.md#32-agent-loop-模板) v0.2 (Agent Loop 模板)

> **本 improvement 是 OpenSpec change `2026-10-09-consolidate-loop-phases-to-shared-helpers/` 的 5-segment 草稿**。OpenSpec change 由 rdd-builder P0 创建,本文件提供实施前设计依据。

### 为什么需要这个 improvement

`pdk/loop_agent/README.md` 的"双循环架构 (C1 决策矩阵)"段自承存在双轨实现问题:

| 层 | 拥有者 | 加载方式 | 同步语义 |
|---|---|---|---|
| Chat-loop | ChatSession::chat() | user-turn 边界 | 每 user turn 一次 |
| Agent-loop | `lib/loop/<loop_type>.agent.md` | `loop/run` 工具读盘 | lock-step |
| 原语层 | `include/agenticdsl/pdk/agent_loops/*` (C++) | 直接 `#include` 头 + 构造 | 由调用方决定 |

**核心痛点**: C++ 类的消费者是"tests" (README 矩阵明确表述), 但生产路径是 `loop/run` 工具走 DSL。**测试覆盖 ≠ 生产覆盖** —— 6 个 C++ 测试 binary (含 3 个 must_realllm 真实 LLM 测试) 测的是 C++ 类语义, 而生产 ChatSession e2e 走 DSL 路径。测试 PASS 不能保证生产行为正确。

**根因** (经 Metis + Oracle dual-agent review 独立交叉验证):

1. **语义从未等价** (Metis DB-1): `lib/loop/plan_execute.agent.md` 当前**单遍 linear** (plan → execute → verify → end), 无 retry; C++ `PlanExecuteLoop` 有 `while(true)` retry (max_retries=3)。两套并行实现**不是同一个状态机**, 而是不同的东西。
2. **DAG 物理约束** (Oracle C1): DSL 是 DAG (`topo_scheduler.cpp:586` 显式拒绝环), retry 回边**无法**用 DSL 节点表达。
3. **DEFINE_AGENT 编译期依赖** (Metis DB-2 + Oracle M3): `agent_macros.h:52-67` LoopDispatcher 模板硬编码引用 3 个 C++ 类; G1 `pdk/g1_coding_assistant/src/g1_agent.cpp:132` 是 ReactLoop 真实生产消费者。**ReactLoop 不能删**。

**用户动机**: 消除双轨实现的"测试 PASS ≠ 生产正确"风险, 同时不破坏 G1 + must_realllm + 6 个测试文件的现状。

---

## Why

消除"测试覆盖 ≠ 生产覆盖"的双轨风险, 同时:
- 保留 C++ 类的现有测试覆盖 (6 个 binary + 3 个 must_realllm 真实 LLM)
- 保留 G1 + DEFINE_AGENT 编译期依赖
- 保留 `lib/loop/*.agent.md` DSL 文件 (ChatSession 生产路径)
- 不引入 retry 等价的 DSL 扩展 (DAG 物理不可达)

### Why not 删 C++ 类全迁 DSL (排除方案 A)

- DSL DAG 无环 → retry 不可表达 → PlanExecute 行为降级
- DEFINE_AGENT 编译期断 → G1 + test_pdk_macros 编译失败
- `agent_loops/*.h` 公开头删除 = PDK API BREAKING + `scripts/sync-pdk.sh` Dual-Repo 同步违约

### Why not 扩展 DSL 引擎加循环节点 (排除方案 B)

- 超本 improvement scope, 需独立 ADR + OpenSpec change
- TopoScheduler DAG → 条件边/循环边语义变化影响范围远超 loop (影响所有 DSL graph)
- 推迟到 ADR-0034 (性能元数据契约) 之后再讨论

### Why not 仅文档同步不动代码 (排除方案 C)

- 用户原始动机 (测试/e2e coverage gap) 未解决
- 仅推迟决策, 保留双轨技术债

---

## What Changes

**核心改造**: 抽取 phase helper 自由函数, C++ 类变薄壳委托, DSL 可选调用 helper 暴露的工具。

### 5 项决策 (per ADR-0089 v1.3)

| # | 决策 | 改动范围 |
|---|---|---|
| **D1** | 抽 `include/agenticdsl/pdk/agent_loops/loop_phases.h` 自由函数 (`run_plan_phase` / `run_execute_phase` / `run_verify_phase`) | 新增 2 文件 (h + cpp) |
| **D2** | `PlanExecuteLoop` / `ReactLoop` / `ForkJoinLoop` 内部委托 helper, **公开 API 零变化** | 3 文件内部重构 |
| **D3** | G1 `DEFINE_AGENT(React)` + `agent_macros.h` LoopDispatcher 编译期依赖保留, 仅加 ADR 注释 | 1 行注释 |
| **D4** | ForkJoinLoop 语义不变 (DomainWorkerPool 4-worker, 用户决策) — DSL 路径**不合并** | 零代码改动 |
| **D5** | `pdk_entry.cpp` 新增 `loop/run_plan` + `loop/run_verify` 工具函数, DSL **可选调用** (不强制改写) | 1 文件 + 新 2 工具注册 |

### 5 atomic commits 拆分 (per ADR-0089 §实施)

```
commit 1: feat(loop_phases): 抽 plan/verify/execute helper 自由函数 (Phase 0)
commit 2: refactor(plan_execute): 内部委托 helper, 公开 API 不变 (Phase 1)
commit 3: refactor(react_loop): 内部委托 helper, 公开 API 不变 (Phase 2)
commit 4: feat(pdk_entry): 新增 loop/run_plan + loop/run_verify 工具注册 (Phase 3)
commit 5: docs: pdk/loop_agent/README.md 双循环架构段改写 + specs/architecture.md L3 契约层 docMap 更新 (Phase 4)
```

### 关键不变量 (5 项)

- D1.inv.model: `req.params.model.clear()` 保留 + "NOT redundant" 注释 (修复 `fix-generation-request-model-default` 修复链延续)
- D2.inv.api: 3 个 C++ 类公开 API 零变化 (G1 + 6 个测试 binary 零修改)
- D2.inv.retry: retry 编排 (`while(true)` + `retries_used`) 保留在 `PlanExecuteLoop::run()` 内, helper 无 retry 语义
- D2.inv.engine: `engine_` 成员保留在类内, 跨 retry 累积 `continue_with_generated_dsl`
- D4.inv.forkjoin: ForkJoinLoop `DomainWorkerPool` 4-worker 语义不变, ADR-0087 benchmark 不回归

---

## Acceptance

### 必过验收 (Must)

- **核心契约**: `tests/test_pdk_plan_execute.cpp` 5/5 PASS (mock 状态机)
- **核心契约**: `tests/test_pdk_fork_join.cpp` 5/5 PASS (mock 并发)
- **核心契约**: `tests/test_plan_execute_restart.cpp` 3/3 PASS (retry 状态机)
- **must_realllm 回归门**: `tests/test_plan_execute_realllm.cpp` 3/3 PASS (`ctest -L must_realllm --output-on-failure`)
- **must_realllm 回归门**: `examples/pdk_chat_demo/tests/test_e2e_real_llm.cpp` ChatSession case PASS (`ctest --test-dir build/examples/pdk_chat_demo/tests -L must_realllm`)
- **零回归**: core tree `ctest -LE must_realllm --output-on-failure` 263+/263+ PASS (100%)
- **零回归**: examples tree `ctest -LE must_realllm --output-on-failure` 33/33 PASS (100%)
- **G1 兼容**: `pdk/g1_coding_assistant` 编译通过, `DEFINE_AGENT(React)` 展开无变化
- **ADR-0087 benchmark 不退化**: `tests/test_adr_0087_step5_1_benchmark.cpp` 复跑 4-worker 3.3× 加速数据不退化

### 期望验收 (Should)

- DSL 文件可选调用 `loop/run_plan` / `loop/run_verify` 工具 (新功能, 不强制)
- `docs/specs/architecture.md` L3 契约层 docMap 更新反映 helper 抽取

### 验收禁止 (Must Not)

- **不删**: agent_loops/*.h 公开头文件 (DEFINE_AGENT 编译期断)
- **不删**: lib/loop/*.agent.md DSL 文件 (ChatSession 生产路径)
- **不删**: G1 DEFINE_AGENT 调用 (M3 真实生产消费者)
- **不引入**: DSL 循环节点扩展 (超本 improvement scope, 排除方案 B)
- **不声称**: "DSL 完整替代 C++ 状态机"——DAG 无环硬约束, retry 必须在 C++

### 文档漂移检查 (per `docs_drift_audit.py`)

- 引用 `PlanExecuteLoop` / `ForkJoinLoop` / `ReactLoop` 的 docstrings 与 README 在 Phase 4 同步
- `pdk/loop_agent/README.md` "双循环架构" 段改写为"phase helper + 薄壳 + DSL 可选调用"
- `AGENTS.md` Recent Changes 登记 ship 条目 (含 5 字段 Reverse Indicator)

---

## Capabilities

### New Capabilities

- `loop-phases-shared-helpers`: `loop_phases::run_plan_phase` / `run_execute_phase` / `run_verify_phase` 三个 phase 自由函数, 被 C++ 薄壳类与 `pdk_entry` 工具共同调用

### Modified Capabilities

- `loop-execute-only`: `pdk/loop_agent/src/pdk_entry.cpp` 新增 `loop/run_plan` + `loop/run_verify` 工具注册, DSL 可选调用
- `docs/specs/architecture.md §L3 契约层`: docMap 反映 `loop_phases` 共享 helper 抽取

### Unchanged Capabilities (零修改)

- `react-loop-final-decision-tooling` (per ADR-0089 §决策 D3): `DEFINE_AGENT` 编译期路径不变
- `loop-run-dsl-execution` (ChatSession 生产路径): `lib/loop/*.agent.md` 文件零修改
- `fork-join-concurrent-pool` (DomainWorkerPool 4-worker): ForkJoinLoop 内部不变, ADR-0087 benchmark 不退化

---

## Impact

**Production code (~6 files)**:
- `include/agenticdsl/pdk/agent_loops/loop_phases.h` (新增) — 3 自由函数声明
- `src/modules/agent_loops/loop_phases.cpp` (新增) — 3 自由函数实现
- `include/agenticdsl/pdk/agent_loops/plan_execute_loop.h/.cpp` (修改) — 内部委托 `loop_phases::run_*`, 公开 API 不变
- `include/agenticdsl/pdk/agent_loops/react_loop.h/.cpp` (修改) — 同
- `include/agenticdsl/pdk/agent_macros.h` (1 行注释) — ADR-0089 引用注释
- `pdk/loop_agent/src/pdk_entry.cpp` (修改) — 新增 2 工具注册 (`loop/run_plan` + `loop/run_verify`)

**零修改 (D2.inv.api + D4.inv.forkjoin)**:
- `tests/test_pdk_plan_execute.cpp` (5 cases)
- `tests/test_pdk_fork_join.cpp` (5 cases)
- `tests/test_plan_execute_restart.cpp` (3 cases)
- `tests/test_plan_execute_realllm.cpp` (3 cases, must_realllm)
- `tests/test_pdk_macros.cpp` (DEFINE_AGENT)
- `examples/pdk_chat_demo/tests/test_plan_execute_loop_integration.cpp` (1 case)
- `examples/pdk_chat_demo/tests/test_fork_join_loop_integration.cpp` (1 case)
- `pdk/g1_coding_assistant/src/g1_agent.cpp` (DEFINE_AGENT(React))
- `lib/loop/{react,plan_execute,fork_join}.agent.md` (DSL 文件零变化)

**Spec / docs (3 files)**:
- `docs/adr/adr-0021-pdk-design.md` §3.2 加指回 ADR-0089 指针
- `docs/specs/architecture.md` L3 契约层 docMap 更新
- `pdk/loop_agent/README.md` "双循环架构" 段改写

**Test (新增可选)**:
- 若 Phase 3 验证 DSL 调用新工具, 可新增 `tests/test_loop_run_phase_tools.cpp` (regression guard, 非必须)

**总修改量**: ~10 文件 (6 改 + 4 新/改 doc), Medium 复杂度, 1-2 天实施

---

## Non-goals

- ❌ 删除 `agent_loops/*.h` 公开头文件 (DEFINE_AGENT + G1 依赖)
- ❌ 删除 `lib/loop/*.agent.md` DSL 文件 (ChatSession 生产路径)
- ❌ 扩展 DSL 引擎支持循环节点 (TopoScheduler DAG → 条件边/循环边, 远超 scope)
- ❌ 改造 `lib/loop/plan_execute.agent.md` 实现 retry 循环 (DAG 不可达, 物理约束)
- ❌ 修改 `ForkJoinLoop` 内部实现 (用户决策 D4: 语义不变)
- ❌ 迁移 must_realllm 测试到 DSL 路径 (C++ 类公开 API 不变, 测试零修改)
- ❌ 替换 `g1_coding_assistant` 的 ReactLoop 消费者为 DSL (M3 兼容 + 风险最小化)

---

## 优先级

**P1** (消除生产 coverage gap, 但不阻塞现有 ship)

---

## 依赖

### 前置依赖 (已完成)

- ✅ ADR-0021 v0.2 (Agent Loop 模板, 包含 3 个 C++ loop 类) — 2026-06-24 ship
- ✅ `fix-generation-request-model-default` (`plan_phase` / `verify_phase` `req.params.model.clear()` 修复) — 2026-09-08 archive
- ✅ Metis + Oracle dual-agent review (本 improvement 立项前完成, sessions `ses_edfe8b94` + `ses_edfe8b7d`)
- ✅ `pdk/loop_agent/README.md` "双循环架构 (C1 决策矩阵)" 表述 (明确双轨问题)
- ✅ react loop final decision tooling ship (commit pending, 2026-09-30) — 修了 V2 修复链的 act 节点最终决策路径

### 后置依赖 (本 improvement 触发)

- ADR-0021 §3.2 主文档加指回 ADR-0089 指针
- `pdk/loop_agent/README.md` 双循环架构段改写
- `scripts/sync-pdk.sh` DEFINE_AGENT 注释同步 (若 macro 注释变化)

---

## Cooling-Off

按 AGENTS.md Single-Dev 模式:
- **ADR-0089 v1.3 状态**: 🔍 Proposed (本 improvement 立项时)
- **OpenSpec change `2026-10-09-consolidate-loop-phases-to-shared-helpers/` 状态**: 待 rdd-builder P0 创建 + 24h cooling-off
- **cooling-off 起点**: OpenSpec change 创建后开始计算
- **cooling-off 满点**: 24h 后 (约 2026-10-10T12:00Z)
- **cooling-off 期间**: 用户 review + Oracle dual-agent 预审 + 决定是否 ship-with-fixes

---

## Reverse Indicator (per AGENTS.md §Reverse Indicator Rule)

```
+ new_up: Loop phase 逻辑从"双轨 C++/DSL 平行实现" 收敛为"phase helper + 薄壳 + DSL 可选调用",
         消除测试/e2e coverage gap; must_realllm 10/10 PASS (含 test_plan_execute_realllm 3 cases);
         零回归 263+/263+ core + 33/33 examples
- old_down: drop_ratio ≤ 5% (纯 refactor + 薄壳委托, 零行为变化, ctest 必全 PASS);
           ForkJoinLoop 零代码改动 (D4 用户决策); 
           3 个 DSL 文件零代码改动 (ChatSession 生产路径不变)
failure_traces: 
  - Oracle C1: DSL DAG 无环 → retry 不可表达 (consolidate 后 retry 仍在 C++, helper 无 retry 语义)
  - Oracle C2: execute_phase engine 归属 (helper 无状态, engine 仍在 PlanExecuteLoop 类内)
  - Oracle M1: model.clear() 必须保留 (helper 抽取时同步移植注释)
  - Oracle M3: G1 DEFINE_AGENT(React) 编译期依赖 (LoopDispatcher 零修改)
  - Metis DB-3: must_realllm 3 cases 断言精确 C++ 字符串 (零迁移, C++ 测试零修改)
ablation: 
  mock-identical 3-segment (baseline / mutated / rerun 全 mock-identical, refactor 性质);
  must_realllm 实测:
    - test_plan_execute_realllm 3/3 PASS (DeepSeek 真实响应, 字符串断言不变)
    - test_e2e_real_llm ChatSession case PASS (DSL 路径 + 真实 LLM)
  ADR-0087 benchmark 4-worker 复跑数据不退化
context_ids: 
  - 不涉及 ContextRequest schema 变更 (本 improvement 不改 pipeline schema)
  - 不涉及必须 [must_realllm] label 变化 (10 个 must_realllm binary 全部保留原 label + 范围)
```

---

## 推荐 OpenSpec change 元数据

- **OpenSpec change 名**: `2026-10-09-consolidate-loop-phases-to-shared-helpers`
- **OpenSpec change 4 件套**: `proposal.md` (high-level + 5 字段) + `design.md` (含 C1/C2/M1/M3 硬约束) + `tasks.md` (TDD 5 步 + 5 atomic commits) + `specs/<name>/spec.md` (helper Requirements + 薄壳契约修订)
- **rdd-builder P0 推荐决策**: complex 路径 (1-2 天, cross-file, architecture impact)
- **Post-impl Oracle SHIP-with-fixes**: 必走 (AGENTS.md Pattern #4)
- **archive**: ship 后 git mv 到 `archive/2026-10-09-consolidate-loop-phases-to-shared-helpers/`

---

## 状态

🔍 **Proposed** (2026-10-09, 立项 + Oracle/Metis dual review 完成, 待 rdd-builder P0 创建 OpenSpec change)

**预实施日期**: 24h cooling-off 满后 (~2026-10-10T12:00Z)
**预实施路径**: rdd-builder P0 complex → P1 plan → P1.5 deps → P2 execute (5 atomic commits) → P2.5 Oracle SHIP-with-fixes review → P3 archive

---

## 参考资料

- **ADR**:
  - [ADR-0021 §3.2](https://github.com/chisuhua/HydraForge/blob/main/docs/adr/adr-0021-pdk-design.md#32-agent-loop-模板) — 修订对象
  - [ADR-0089 v1.3](../docs/adr/adr-0089-v1-3-amendment-loop-phase-shared-helpers.md) — 本 improvement 对应 amendment (🔍 Proposed)
  - [ADR-0067](https://github.com/chisuhua/HydraForge/blob/main/docs/adr/adr-0067-layered-plugin-architecture-split.md) — L2/L3/L4 分层架构
  - [ADR-0041](https://github.com/chisuhua/HydraForge/blob/main/docs/adr/adr-0041-pluginloader-lifecycle-extension.md) — PluginLoader ABI
  - [ADR-0087](https://github.com/chisuhua/HydraForge/blob/main/docs/adr/adr-0087-cloud-adapter-threading-model.md) — Cloud Adapter Threading Model (ForkJoinLoop benchmark)
  - [ADR-0020](https://github.com/chisuhua/HydraForge/blob/main/docs/adr/adr-0020-thread-model-isolation.md) — DomainWorkerPool 模式
- **Dual-Agent Review**:
  - Metis session `ses_edfe8b94effeLHZtjyKMyWFTXO` — Intent + ambiguity + AI failure modes
  - Oracle session `ses_edfe8b7daffeysJHJPj9PaG5SZ` — Architecture + physical feasibility + test migration + ABI
- **现状描述**:
  - `pdk/loop_agent/README.md` "双循环架构 (C1 决策矩阵)" 段
  - `include/agenticdsl/pdk/agent_macros.h:52-67` (LoopDispatcher 模板 3 specialization)
  - `include/agenticdsl/pdk/agent_loops/plan_execute_loop.h:141-195` (retry 主循环)
  - `include/agenticdsl/pdk/agent_loops/plan_execute_loop.h:225/289` (`req.params.model.clear()` 修复点)
  - `topo_scheduler.cpp:586` (DSL DAG 拒绝环)
  - `pdk/g1_coding_assistant/src/g1_agent.cpp:132` (DEFINE_AGENT(React) 真实生产消费者)
- **AGENTS.md 沉淀模式**:
  - Pattern #4 (SHIP-with-fixes + Oracle 介入)
  - Pattern #8 (OpenSpec Change pre-implementation dual-agent review)
  - Pattern #11 (Async Worker + Dual Oracle dual-review SHIP-with-fixes cycle)
  - §Reverse Indicator Rule (5 字段强制)
  - §FULL REGRESSION TEST FLOW (must_realllm 阶段 2 强制)