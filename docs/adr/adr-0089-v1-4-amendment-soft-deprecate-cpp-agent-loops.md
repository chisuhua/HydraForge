# ADR-0089 v1.4 amendment: Soft-Deprecate ReactLoop / PlanExecuteLoop / DEFINE_AGENT (Single-Dev Path)

**日期**: 2026-10-10
**状态**: ✅ **Approved**（ship commits `7dc83a7` + `87aadc4`，Oracle dual-review 通过）
**父 ADR**: [adr-0021-pdk-design.md](adr-0021-pdk-design.md) v0.2 §3.2
**前置 v1.3**: [adr-0089-v1-3-amendment-loop-phase-shared-helpers.md](adr-0089-v1-3-amendment-loop-phase-shared-helpers.md)（2026-10-09 ship，5 commits：`1cfe82a` / `04273ad` / `bed7995` / `b06cf60` / `fbda908`）

> **主文档引用**: 本 amendment 修订 ADR-0021 §3.2 (Agent Loop 模板)。ADR-0021 主文档 §3.2 已加指回 ADR-0089 v1.3 的指针；本 v1.4 在 v1.3 之上叠加 soft-deprecate 指令，公开 API 不强制变更（D9.inv.soft-deprecate-not-delete）。

---

## 状态

✅ **Approved** (2026-10-10 ship: Phase 1 docs `7dc83a7` + Phase 2 code `87aadc4`)

**Ship 验证** (Phase 2, commit `87aadc4`):
- `cmake --build build` 全量 exit 0，零 error；deprecation warnings 按 Oracle 预期出现在消费者（agent_macros.h 内部引用 + test_plan_execute_loop_integration + test_pdk_macros + g1_coding_assistant）
- `ctest --test-dir build -LE must_realllm` **264/264 PASS，零回归**
- M3 g1_coding_assistant 编译成功（warnings 指向 agent_macros.h:60/65/88/96/150 内部引用点，编译期依赖保留）
- must_realllm 测试 binary (test_react_loop_real_llm / test_plan_execute_realllm) 重编成功（编译零 error，mtime 17:09）
- Oracle verdict `bg_f3dad0b2`: **SHIP-with-fixes** — Critical-1 (-Werror 排查: 全项目 0 命中 ✅) / Major-1 (message 统一含 ADR 引用 ✅) / Major-2 (DEFINE_AGENT 生成 class 标注 ✅) / Minor-1 (forward decl 排查: 0 命中 ✅) / Minor-2 (本状态更新 ✅)
- drop_ratio = 0%（纯 [[deprecated]] 属性，零行为变化）

**前置文档**:
- ADR-0089 v1.3 (5 commits ship 2026-10-09) — 本 amendment 基础
- ADR-0021 v0.2 §3.2 (Agent Loop 模板 — 本 amendment 修订对象)
- ADR-0067 (L2/L3/L4 分层架构)
- ADR-0041 (PluginLoader 生命周期 + ABI)
- ADR-0087 (Cloud Adapter Threading Model — ForkJoin 3.3× speedup benchmark, 2026-09-16 commit `13ac53f`)
- Metis session `ses_edbe24cc1ffeqFnkc2v9e56nqt` (deprecation 意图分析)
- Oracle session `ses_edbe211c0ffemuq7g2KEC5qglC` (deprecation 技术审查)
- AGENTS.md Pattern #12 (`consolidate-loop-phases-to-shared-helpers` 立项沉淀)

**反向指标** (per AGENTS.md Reverse Indicator Rule — 5 字段强制):
- `+ new_up`: 公开 API canonical surface 收敛到 `lib/loop/*.agent.md` + `loop/run` 工具; 减少 new contributor 认知负担 (C++/DSL/DEFINE_AGENT 三选一困惑)
- `- old_down`: drop_ratio = 0% (软 deprecated = 零行为变化, drop 仅是文档 + 编译 warning)
- `failure_traces`: Metis C4 (测试/e2e coverage 双轨 风险) + Oracle C3 (DEFINE_AGENT [[deprecated]] 路径与 LoopDispatcher specialization 分裂)
- `ablation`: mock-identical 3-segment (纯 refactor, no LLM dependency change)
- `context_ids`: N/A (本 ADR 不改 pipeline schema)

---

## 背景

### 用户原始动机

用户提的问题："**C++ 版本的 Agent Loop 可以停用，只暴露 DSL 路径吗？**"。经 Metis + Oracle dual-agent review 评估 + 用户三轮澄清，最终决定 **Z-1 路径**（本期仅 React/PlanExecute 软停用，ForkJoin 现状保留）。

### D4 历史决策重新审查（2026-10-10）

ADR-0089 v1.3 (2026-10-09 ship) 写明的 D4 决策原话：
> "ForkJoinLoop 内部实现 (DomainWorkerPool 4-worker + InMemoryBus 事件同步) **零代码改动**。DSL 继续走 fork node + loop/process_task 路径，**不合并**。ADR-0087 benchmark 4-worker 3.3× 加速承载对象不变。"

#### 重新审查结论

| 维度 | v1.3 D4 当时解读 | 现在重新评估 | 影响 |
|---|---|---|---|
| **D4 真实性质** | v1.3 change 的范围守卫（scope guard）— 限定 consolidate change 不动 ForkJoinLoop | **不是永久政策禁令**。是 narrow change 的局部约束 | v1.3 ship 后即可重新审视 |
| **保护对象** | DomainWorkerPool 4-worker 行为（ADR-0087 benchmark） | ✓ 仍然有效 — 但绑定到**原语**而非 class 形态 | ForkJoinLoop::run() 内部逻辑可迁出到 DSL 工具，但**原语本身必须保留** |
| **隐含假设** | "DEFINE_AGENT 是 PDK 公开 API，有外部消费者" | **已证伪** — 用户本轮确认无外部消费者 | 该 hidden rationale 失效 |
| **真实生产消费者** | 设计文档假设 g1_coding_assistant 用 DEFINE_AGENT 全部 3 类 | 实测：`pdk/g1_coding_assistant/src/g1_agent.cpp:132` DEFINE_AGENT(**React**)，**不涉及 ForkJoin** | ForkJoinLoop 实际零 production consumer |
| **3.3× speedup** | ADR-0087 benchmark 4-worker 实证 | ✓ 仍是 ADR-0087 资产。但解耦: DomainWorkerPool 是 ADR-0020 §3.2 独立模块，**不绑定到 ForkJoinLoop class** | ForkJoinLoop class 可软停用，DomainWorkerPool 4-worker 行为迁入 pdk_entry.cpp::loop/process_task 或新工具 |

#### D4 在 v1.4 中的地位

本 v1.4 决定 D4 升级为 D8（**确认继续生效**）。ForkJoinLoop class + DomainWorkerPool 4-worker 原语**两者均不动**。ForkJoin DSL 路径迁移是**独立 future change**（D9.future-change-tracker），不在本 v1.4 scope。

### 三个收敛点 (Metis `ses_edbe24cc` + Oracle `ses_edbe211c`)

| 收敛点 | Metis | Oracle |
|---|---|---|
| 推荐软停用而非硬删除 | ✅ 推荐 B (软停用) | ✅ 推荐 B' (OpenSpec 流程) 用户改 B (no OpenSpec) |
| ForkJoinLoop 必须保留 | ✅ D4 + 3.3× | ✅ ADR-0087 benchmark |
| 影响 `hydraforge-pdk` 外部消费者 | ✅ ADR-0021 §7 sync-pdk | ✅ standalone 治理 |

三处独立命中 = Pattern #8 dual-agent 最高置信度修正优先级。用户进一步提供新证据"**无实际外部消费者**"，使 A 选项（硬删除）部分阻力消除，但 ForkJoin 性能 + Phase 1 真实复杂度评估后，最终选择 Z-1（仅软停用 React/PlanExecute）。

### Phase 1 真实复杂度评估（关键发现）

读 `fork_join_loop.h:138-340` 后量化：ForkJoinLoop 不是"可移动的 lambda"，是 **4 个紧耦合组件的胶水**（DomainWorkerPool 4 jthread + InMemoryBus 事件订阅 domain.task.* + mutex+cv 同步 + fail-fast/分支合并）。要把这些行为迁移到 DSL 工具 = **重建 ForkJoinLoop 内部逻辑的镜像实现** + DomainWorkerPool 跨 ABI 暴露面（违反 ADR-0021 §3.5 "PDK 头文件仅依赖 contract/*.h"） + ADR-0087 benchmark 在 DSL 路径上重跑 = **1 个完整 Sprint 工作**。

**Z-1 决策**: 本 v1.4 不做 ForkJoin DSL 化，立独立 future change tracker。

---

## 决策

### 决策 D6 — Soft-deprecate ReactLoop + PlanExecuteLoop class

`include/agenticdsl/pdk/agent_loops/{react_loop.h, plan_execute_loop.h}` 公开 API 不变（G1 + 5 个测试 binary 不动），但在 class 声明前增加 `[[deprecated("Use lib/loop/react.agent.md / plan_execute.agent.md + loop/run tool. See ADR-0089 v1.4.")]]` 属性：

```cpp
namespace hydraforge::pdk {
  /**
   * @brief ReactLoop — PDK Agent 单轮循环 (ADR-0089 v1.3 薄壳化, ADR-0089 v1.4 soft-deprecated)
   * @deprecated 自 ADR-0089 v1.4 (2026-10-10) 起, 推荐使用
   *             `lib/loop/react.agent.md` DSL 路径 + `loop/run` 工具。
   *             类本身保留可编译使用（含 `g1_coding_assistant` M3 编译期依赖），
   *             但将随 v1.4 之后的观察期（≥ 1-2 月）择机删除。
   *             删除前必须完成 must_realllm 守卫镜像到 DSL 路径。
   *             详见 ADR-0089 v1.4 决策 D6/D9。
   */
  class [[deprecated("Use lib/loop/react.agent.md + loop/run tool, see ADR-0089 v1.4")]] ReactLoop {
    // ... 公开 API 零变化
  };
}
```

**关键不变量**:
- **D6.inv.api**: ReactLoop / PlanExecuteLoop 公开 API 零变化（G1 + 5 个测试 binary 零修改）
- **D6.inv.body**: class 内部实现不修改（v1.3 已 ship 薄壳化委托 helper，本 amendment 仅加 deprecation 标签）
- **D6.inv.warning-not-error**: [[deprecated]] 仅产生 compiler warning，不阻止编译（`-Wno-deprecated-declarations` 可关闭）
- **D6.inv.forkjoin**: ForkJoinLoop **不在**本 deprecation 范围（D8 维持）

### 决策 D7 — Soft-deprecate DEFINE_AGENT macro

`include/agenticdsl/pdk/agent_macros.h` 的 `DEFINE_AGENT(name, loop_type)` 宏加 `[[deprecated]]` 属性：

```cpp
#define DEFINE_AGENT(name, loop_type)                                              \
  [[deprecated("Use lib/loop/<loop_type>.agent.md + loop/run tool, see ADR-0089 v1.4")]] \
  class name##Agent {                                                             \
    using Loop = typename ::hydraforge::pdk::LoopDispatcher<loop_type>::Type;     \
    /* ... 其余宏展开不变 */                                                      \
  };
```

**关键不变量**:
- **D7.inv.api**: 宏展开后 `LoopDispatcher` 3 specialization 保留（React/PlanExecute/ForkJoin），**不被删除**
- **D7.inv.compile-only-warning**: 现有 `test_pdk_macros.cpp` 实例化 `DEFINE_AGENT(test_agent, AgentLoopType::React)` 等编译期测试**仅获 deprecation warning**（不报错）
- **D7.inv.g1**: `pdk/g1_coding_assistant/src/g1_agent.cpp:132` 现有 `DEFINE_AGENT(CodingAssistant, AgentLoopType::React)` **不修改**（M3 编译期依赖 — 测试零行为变化）
- **D7.inv.must_realllm**: `tests/test_react_loop_real_llm.cpp` (7/7) + `tests/test_plan_execute_realllm.cpp` (3/3) **不修改**（编译 warning 而已，PASS 不变）

### 决策 D8 — ForkJoinLoop 维持现状 (Reaffirm v1.3 D4)

继承 v1.3 D4 决策，**逐字重申**:

> `ForkJoinLoop` 内部实现 (DomainWorkerPool 4-worker + InMemoryBus 事件同步 + fail-fast) **零代码改动**。DSL `lib/loop/fork_join.agent.md` 继续走 fork node + `loop/process_task` 路径。ADR-0087 benchmark 4-worker 3.3× 加速承载对象不变。

**关键不变量**:
- **D8.inv.forkjoin**: ForkJoinLoop 内部 `DomainWorkerPool` 4-worker 语义不变，ADR-0087 benchmark 不回归
- **D8.inv.unchanged-class**: ForkJoinLoop class **不加** `[[deprecated]]`（与 D6/D7 范围不同）
- **D8.inv.unchanged-handler**: ForkJoinLoop `process_task` handler 注册路径 `register_domain_handler("branch", ...)` 保留
- **D8.inv.archive-original**: ADR-0089 v1.4 ship commit message 含 "fork_join_loop.h 当前实现作为 future-change reference" 注解

### 决策 D9 — Future change tracker: `fork_join_dsl_migration`

本 v1.4 不实施 ForkJoinLoop → DSL 工具的迁移（**1 个完整 Sprint 工作量，已超出本次会话范围**）。但留独立 **future change tracker** 文件：

新建 `.rddf/improvements/fork_join_dsl_migration.md`（5-segment draft 格式），明确指出：
1. **Why**: ADR-0089 v1.4 D9 — ForkJoin 4-worker 不绑定到 class 形态, 可迁出到 DSL 工具
2. **What Changes**:
   - 新增 `loop/run_fork_join(branches[], context)` 工具注册（pdk_entry.cpp, similar to `loop/run_plan` v1.3 D5）
   - 修改 `lib/loop/fork_join.agent.md` 使用新工具替代 fork node + 多次 `loop/process_task` 调用
   - DomainWorkerPool 跨 ABI 暴露面评估（违反 ADR-0021 §3.5 时需 amendment）
   - 复跑 ADR-0087 benchmark 在 DSL 路径上验证 3.3× speedup 复现
3. **Acceptance**: ctest 全量 + ADR-0087 benchmark 复跑 ≥ 3× 三次稳定 + ADR-0021 §3.5 合规
4. **Capabilities**: DSL fork_join 路径获得 4-worker 并行能力 + ForkJoinLoop class 退役前置
5. **Impact**: ADR-0089 v1.5 candidate (本 amendment 后下一个 amendment)

**跟踪依赖**: 本 tracker 状态由 `.rddf/improvements/improvement-suggestions.md` 维护。立项触发条件: 用户明确"启动 fork-join-migration change" 或 ADR-0089 v1.4 观察期满（≥ 1-2 月）。

**关键不变量**:
- **D9.inv.tracker-not-implementation**: 本 v1.4 ship 时, `.rddf/improvements/fork_join_dsl_migration.md` 仅作为 proposal tracker 存在，**不进入实施周期**
- **D9.inv.depends-on-v1.4-observation**: implementation 启动前提 v1.4 soft-deprecation 观察期满 + 无新增硬依赖 FORK join 路径

---

## 实施

### Phase 1 — 文档同步 (commit 1)

新增 / 修改文件（**零行为变化**）:
1. 新建 `docs/adr/adr-0089-v1-4-amendment-soft-deprecate-cpp-agent-loops.md` (本文件)
2. 新建 `.rddf/improvements/fork_join_dsl_migration.md` (5-segment future change tracker)
3. 更新 `AGENTS.md` Pattern #12 section: 提及 v1.4 ship 后 dual-track 治理升级为"单源 + 入口软停用"
4. 在 `include/agenticdsl/pdk/agent_loops/fork_join_loop.h` 头部添加 ADR 戳记注释一行: `// ADR-0089 v1.4 D8: ForkJoinLoop 维持不变, 4-worker 原语承载对象不动`

**ship gate**: 无 ctest 影响（仅文档与单行注释）。

### Phase 2 — Soft deprecation 代码标注 (commit 2)

修改文件（**纯 [[deprecated]] 属性, 零行为变化**）:
1. `include/agenticdsl/pdk/agent_loops/react_loop.h` — class 加 `[[deprecated]]` (D6)
2. `include/agenticdsl/pdk/agent_loops/plan_execute_loop.h` — class 加 `[[deprecated]]` (D6)
3. `include/agenticdsl/pdk/agent_macros.h` — `DEFINE_AGENT` 宏加 `[[deprecated]]` (D7)
4. 更新 `pdk/loop_agent/README.md` "双循环架构" 段: 新增 "Soft-deprecated C++ Agent Loop, 引导 DSL path" 段 + ForkJoinLoop 现状保留注记

**ship gate**:
- 编译: `cmake --build build --target agenticdsl_core` 零 error
- warning 数量: 现有 5 个测试 binary + g1_coding_assistant 编译期 deprecation warning 计数 = 已知数 (≤ 5)
- 行为: ctest 全量零回归（`ctest --test-dir build -LE must_realllm` 100% PASS）
- must_realllm: `test_react_loop_real_llm` 7/7 + `test_plan_execute_realllm` 3/3 全部 PASS（仅获 deprecation warning 不影响用例）
- 链接: g1_coding_assistant libG1CodingAssistant.so symbols (`nm | grep -E "DEFINE_AGENT|ReactLoop"`) 仍命中（M3 编译期保留）
- ctest 计数: 与 v1.3 ship 后 baseline 等同（仅 warning 非 error）

### Phase 3 (FUTURE — 1-2 月观察期后, 不在本 amendment scope)

完整删除 ReactLoop/PlanExecuteLoop class + DEFINE_AGENT 宏 + must_realllm 守卫（镜像到 DSL 路径测试）。启动独立 OpenSpec change `delete-deprecated-cpp-loops`，引用本 v1.4 观察期记录。

---

## 不变量

### 继承 v1.3 不变量（保留）

- **D1.inv.model**: `req.params.model.clear()` 在 `loop_phases::run_plan_phase` / `run_verify_phase` 内保留，注释解释 (Oracle M1 修复链延续)
- **D2.inv.api**: 3 个 C++ 类公开 API 零变化（G1 + 6 个测试 binary 零修改）— **D6/D7 加 [[deprecated]] 不破坏**
- **D2.inv.retry**: retry 编排 (`while(true)` + `retries_used`) 保留在 `PlanExecuteLoop::run()` 内, helper 无 retry 语义
- **D2.inv.engine**: `engine_` 成员保留在类内，跨 retry 累积
- **D4.inv.forkjoin** → **D8.inv.forkjoin**（本 v1.4 升级 D4 → D8）: ForkJoinLoop 4-worker 语义不变
- **D5.inv.optional** (v1.3): `loop/run_plan` / `loop/run_verify` 工具可选，DSL 文件不强制改写

### 本 v1.4 新增不变量

- **D6.inv.api**: ReactLoop / PlanExecuteLoop 公开 API 零变化（class 内部实现不修改, 仅加 [[deprecated]] 属性）
- **D6.inv.body**: class 内部实现不修改（v1.3 已 ship 薄壳化, 本 amendment 仅标记 deprecation）
- **D6.inv.warning-not-error**: [[deprecated]] 仅产生 compiler warning，不阻止编译
- **D6.inv.forkjoin**: ForkJoinLoop **不在**本 deprecation 范围
- **D7.inv.api**: 宏展开后 `LoopDispatcher` 3 specialization 保留
- **D7.inv.compile-only-warning**: 现有实例化 DEFINE_AGENT 的测试仅获 warning, 不报错
- **D7.inv.g1**: g1_coding_assistant M3 编译期依赖零变化
- **D7.inv.must_realllm**: must_realllm 真实 LLM 守卫的 ReactLoop / PlanExecuteLoop 测试零修改
- **D8.inv.forkjoin**: ForkJoinLoop 内部 `DomainWorkerPool` 4-worker 语义不变, ADR-0087 benchmark 不回归（升级自 v1.3 D4）
- **D8.inv.unchanged-class**: ForkJoinLoop class 不加 `[[deprecated]]`
- **D8.inv.unchanged-handler**: ForkJoinLoop `register_domain_handler("branch", ...)` 路径保留
- **D9.inv.tracker-not-implementation**: `.rddf/improvements/fork_join_dsl_migration.md` 仅作 proposal tracker, 不进入实施周期
- **D9.inv.depends-on-v1.4-observation**: implementation 启动前提 v1.4 soft-deprecation 观察期满 + 无新增硬依赖 fork join 路径

---

## 影响范围

| 类型 | 文件 | 操作 | Phase |
|---|---|---|---|
| 新增 | `docs/adr/adr-0089-v1-4-amendment-soft-deprecate-cpp-agent-loops.md` | 新建（本文件） | 1 |
| 新增 | `.rddf/improvements/fork_join_dsl_migration.md` | 新建 (5-segment tracker) | 1 |
| 修改 | `AGENTS.md` Pattern #12 段 | 追加 v1.4 描述 (dual-track → soft-deprecate 升级) | 1 |
| 修改 | `include/agenticdsl/pdk/agent_loops/fork_join_loop.h` | 顶部单行 ADR 戳记注释 (D8 INVARIANT 标记) | 1 |
| 修改 | `include/agenticdsl/pdk/agent_loops/react_loop.h` | class 加 `[[deprecated]]` 属性 (D6) | 2 |
| 修改 | `include/agenticdsl/pdk/agent_loops/plan_execute_loop.h` | class 加 `[[deprecated]]` 属性 (D6) | 2 |
| 修改 | `include/agenticdsl/pdk/agent_macros.h` | `DEFINE_AGENT` 宏加 `[[deprecated]]` 属性 (D7) | 2 |
| 修改 | `pdk/loop_agent/README.md` | "双循环架构" 段改写 (新增 soft-deprecate 段 + ForkJoinLoop 保留注记) | 2 |
| **零修改** | `include/agenticdsl/pdk/agent_loops/loop_phases.h` | D1 不变量保留, helper 无变化 | — |
| **零修改** | `include/agenticdsl/pdk/agent_loops/fork_join_loop.h` 内部 | D8 不变量保留, 4-worker 行为不变 | — |
| **零修改** | `tests/test_pdk_*.cpp` (5 个) | 编译期获 warning, 测试零行为变化 | — |
| **零修改** | `tests/test_plan_execute_realllm.cpp` (must_realllm 3 cases) | must_realllm real LLM 验证零影响 | — |
| **零修改** | `pdk/g1_coding_assistant/src/g1_agent.cpp` | M3 编译期依赖零修改 | — |
| **零修改** | `examples/pdk_chat_demo/tests/test_*_loop_integration.cpp` (2 个) | 集成测试零影响 | — |

**总修改量**: 4 新（1 ADR + 1 tracker + AGENTS.md + pdk_entry.cpp 不修改 ✓）+ 4 改（fork_join_loop.h header 戳记 + 3 class 注释 + README）= **~8 文件**, Medium 复杂度, **1 个会话可完成**。

---

## 风险登记

| # | 风险 | 严重度 | 缓解 |
|---|---|---|---|
| R1 | [[deprecated]] warning 数量影响 CI log 噪音 | 🟢 低 | `-Wno-deprecated-declarations` 可关闭；warning 数量 ≤ 5（5 个测试 binary + g1）已知可控 |
| R2 | 用户在 v1.4 ship 后立即启动 fork-join-migration | 🟡 中 | D9.inv.tracker-not-implementation 明确 separation; .rddf/improvements/fork_join_dsl_migration.md 显式 5-segment draft 需独立 OpenSpec 流程 |
| R3 | ForkJoinLoop class 长期保留造成"双轨认知负担延续" | 🟡 中 | D9.future-change-tracker 显式 follow-up; 1-2 月观察期后必须决定是否启动 migration |
| R4 | v1.3 D4 → v1.4 D8 决策继承链条歧义 | 🟢 低 | D8 显式声明"逐字重申 v1.3 D4"，commit message + ADR 双向交叉引用 |
| R5 | LoopDispatcher specialization 未来被删 | 🟢 低 | D7.inv.api 明确"3 specialization 保留, 不被删除"; 仅 DEFINE_AGENT macro 加 deprecation 标签 |
| R6 | must_realllm 测试因 deprecation 编译失败 | 🟢 低 | D7.inv.must_realllm 显式约束; ship gate 必跑 test_react_loop_real_llm + test_plan_execute_realllm 验证 |
| R7 | R 模式 #10: post-acceptance hygiene 风险（参 2026-09-22 G1） | 🟢 低 | 本 amendment 仅加 [[deprecated]], 无 lifecycle / 资源顺序变更; 文档优先于代码 |

---

## 治理流程（按用户决定 "B: 直接 deprecation + 文档同步, 不走 OpenSpec"）

按 AGENTS.md Single-Dev 模式 + 用户本轮明确决策：

1. **Oracle dual-review** (本会话): Metis `ses_edbe24cc` + Oracle `ses_edbe211c` 已完成，编入本 amendment Background 段
2. **用户显式决策** (2026-10-10): Z-1 路径, B 治理（不走 OpenSpec）, 本会话可完
3. **rdd-arch** (本 stage): ADR-0089 v1.4 ✅ Proposed (本文档)
4. **rdd-planner** (无独立 activation): Z-1 直接 phase 1/2 实施
5. **Phase 1 文档 commit**: 1 atomic commit + [Reverse Indicator] 5 字段
6. **Phase 2 代码 commit**: 1 atomic commit + Oracle post-impl SHIP-with-fixes review (per Pattern #4 + #11)
7. **archive**: 本 amendment 状态翻牌 → ✅ Approved; AGENTS.md Recent Changes 登记
8. **Follow-up**: `.rddf/improvements/fork_join_dsl_migration.md` 观察 1-2 月后由用户决定是否启动

---

## 替代方案 (排除)

### 方案 A — 直接删除 ReactLoop / PlanExecuteLoop / DEFINE_AGENT (硬删除)

**排除理由** (per Oracle C1 + M3 + Metis DB-1):
- ABI breaking（即使无外部消费者，monorepo 内仍有 4 个测试 binary + g1_coding_assistant 编译期依赖）
- must_realllm 守卫（test_react_loop_real_llm 7/7 + test_plan_execute_realllm 3/3）需镜像到 DSL 路径，超本会话 scope
- 软停用 vs 硬删除 = 同等行为约束 + 软退路（用户明示破坏更小）

### 方案 B — ForkJoin 全 DSL 迁移同期完成 (合并方案)

**排除理由** (per Phase 1 真实复杂度量化):
- ForkJoinLoop 内部依赖 4 个紧耦合组件（DomainWorkerPool + InMemoryBus + mutex/cv + handler 注册）
- DomainWorkerPool 跨 ABI 暴露 = 违反 ADR-0021 §3.5
- ADR-0087 benchmark 复跑需稳定 3×
- 至少 1 个完整 Sprint 工作量，超过本会话可用时间

### 方案 C — 仅文档同步不动代码 (Phase 1 only)

**排除理由**:
- 文档引导效果有限（C++ 类仍可调用，新 contributor 看 deprecated 仍困惑）
- 用户原始动机（停用 C++ Loop）未充分满足
- 仅有 Phase 1 等于"未 ship 的 decision" → Pattern #10 hygiene 风险（post-acceptance 未兑现）

### 方案 D — ADR-0089 v1.3 amendment 升级为 v2.0 (新 ADR 替代 v1.3)

**排除理由**:
- v1.3 D4 决策仍可继承，无矛盾
- v1.4 amendment 形式符合项目惯例（per ADR-0080 v1.2 / ADR-0086 v1.1 / ADR-0079 v1.1 已有先例）
- v2.0 = 强行分离父子关系，不利于线性追溯

---

## 参考资料

- [ADR-0021 §3.2 Agent Loop 模板](adr-0021-pdk-design.md#32-agent-loop-模板) — 修订对象
- [ADR-0089 v1.3 — Loop Phase Consolidation](adr-0089-v1-3-amendment-loop-phase-shared-helpers.md) — 5 commits ship 2026-10-09 (`1cfe82a` / `04273ad` / `bed7995` / `b06cf60` / `fbda908`)
- [ADR-0067 L2/L3/L4 分层架构](adr-0067-layered-plugin-architecture-split.md) — PDK 头依赖约束 P3
- [ADR-0041 PluginLoader 生命周期扩展](adr-0041-pluginloader-lifecycle-extension.md) — ABI 边界
- [ADR-0087 Cloud Adapter Threading Model](adr-0087-cloud-adapter-threading-model.md) — ForkJoinLoop 4-worker benchmark (2026-09-16 commit `13ac53f` 2834ms → 858ms)
- [ADR-0020 Thread Model Isolation](adr-0020-thread-model-isolation.md) — DomainWorkerPool 模式
- [ADR-0082 Agent as First-Class Registry](adr-0082-agent-first-class-registry.md) — L4 编排层（与本 L3 区分）
- Metis session `ses_edbe24cc1ffeqFnkc2v9e56nqt` — Deprecation 意图分析 + ForkJoin 范围 + AI failure modes
- Oracle session `ses_edbe211c0ffemuq7g2KEC5qglC` — Soft deprecation 架构 + 物理可行性 + ABI
- AGENTS.md §Reverse Indicator Rule (5 字段强制)
- AGENTS.md Pattern #4 (SHIP-with-fixes + Oracle 介入)
- AGENTS.md Pattern #8 (OpenSpec Change pre-implementation dual-agent review)
- AGENTS.md Pattern #11 (Async Worker + Dual Oracle dual-review SHIP-with-fixes cycle)
- AGENTS.md Pattern #12 (consolidate-loop-phases-to-shared-helpers v1.3 沉淀)
- AGENTS.md Pattern #10 (post-acceptance hygiene 风险 — 必走 fresh-state 验证)

---

## 维护规则

| 触发 | 更新 |
|---|---|
| 本 amendment 翻牌 ✅ Approved (post-ship) | 更新本文档 ## 状态 字段；AGENTS.md Recent Changes 登记 |
| 用户在 1-2 月观察期后启动 fork-join-migration | 立独立 OpenSpec change `fork-join-dsl-migration`，引用本文档 D9 + 本 v1.4 观察期记录 |
| ForkJoinLoop 4-worker benchmark 在未来 regression | 立即触发 fork-join-dsl-migration change 评估（DomainWorkerPool 原语保护） |
| DEFINE_AGENT macro 后续若被外部 repo 引用（虽本会话确认无） | 撤销 D6/D7 deprecation，恢复原始公开 API，向 PDK Dual-Repo 用户发出通知 |
| Pattern #10 fresh-deploy regression 检测 (未来 ship 后审计) | 走 AGENTS.md §verification-before-completion skill + 隔离 HOME / tmpdir fixture |

---

**Ship gate checklist** (Phase 1 + Phase 2 合并 commit 前必跑):

- [x] Oracle dual-review verdict 收集（本会话已通过）
- [x] 用户 Z-1 路径决策记录在本 amendment "决策" 段（✅ 已记录）
- [x] Phase 1 commit 必含: ADR-0089 v1.4 amendment + .rddf/improvements tracker + AGENTS.md Pattern #12 更新 + fork_join_loop.h 戳记（commit `7dc83a7` ✅）
- [x] Phase 2 commit 必含: ReactLoop/PlanExecuteLoop/DEFINE_AGENT [[deprecated]] + pdk/loop_agent/README.md 改写（commit `87aadc4` ✅）
- [x] Oracle post-impl SHIP-with-fixes 审查（bg_f3dad0b2: SHIP-with-fixes, Critical-1/Major-1/Major-2/Minor-1 全部应用 ✅）
- [x] 编译: `cmake --build build` 零 error（deprecation warnings 按预期出现在消费者, 零 error ✅）
- [x] 测试: `ctest --test-dir build -LE must_realllm` 100% PASS（264/264 ✅）
- [ ] must_realllm: `test_react_loop_real_llm` 7/7 + `test_plan_execute_realllm` 3/3 PASS — ⚠️ **NOT-RUN**（本环境无 DEEPSEEK_API_KEY, must_realllm skip 时强制 FAIL 是既有语义, 非本改动回归；两个 binary 编译零 error 已验证, 真 LLM 跑验留待有 key 环境）
- [x] 链接: g1_coding_assistant libhydraforge_g1_coding_assistant.so 重编成功（warnings 指向 agent_macros.h:60/65/88/96/150 内部引用点, M3 编译期依赖保留 ✅）
- [x] 反向指标: drop_ratio = 0%（本 amendment 软 deprecated, 实测 0% drop）
