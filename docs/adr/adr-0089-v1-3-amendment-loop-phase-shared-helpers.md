# ADR-0089 v1.3 amendment: Loop Phase Consolidation to Shared C++ Helpers

**日期**: 2026-10-09
**状态**: 🔍 **Proposed**（24h cooling-off 后由架构组 review 决定）
**父 ADR**: [adr-0021-pdk-design.md](adr-0021-pdk-design.md) v0.2 §3.2

> **主文档引用**: 本 amendment 修订 ADR-0021 §3.2 (Agent Loop 模板), 主文档 §3.2 已加指回本文件的指针。请同步阅读 [ADR-0021 §3.2](adr-0021-pdk-design.md#32-agent-loop-模板)。

---

## 状态

🔍 Proposed (2026-10-09, 立项 + Oracle/Metis dual review 完成)

**前置文档**:
- `pdk/loop_agent/README.md` "双循环架构 (C1 决策矩阵)" — 现状描述
- Metis session `ses_edfe8b94effeLHZtjyKMyWFTXO` (intent + ambiguity + AI failure modes)
- Oracle session `ses_edfe8b7daffeysJHJPj9PaG5SZ` (architecture + physical feasibility + test migration)
- ADR-0021 v0.2 §3.2 (Agent Loop 模板 — 本 amendment 修订对象)
- ADR-0067 (L2/L3/L4 分层架构)
- ADR-0041 (PluginLoader 生命周期 + ABI)

**反向指标** (per AGENTS.md Reverse Indicator Rule):
- `+ new_up`: Loop phase 逻辑从"双轨 C++/DSL 平行实现" 收敛为"phase helper + 薄壳 + DSL 调用",消除测试/e2e coverage gap
- `- old_down`: drop_ratio ≤ 5% (纯 refactor, 零行为变化, OpenSpec change 阶段 1 ctest 必全 PASS)
- `failure_traces`: Oracle Critical #1 (DSL DAG 无环 → retry 不可表达) + Critical #2 (execute_phase engine 归属) + Metis DB-3 (must_realllm 3 cases 断言精确 C++ 字符串, 无 DSL 等价目标)
- `ablation`: mock-identical 3-segment + must_realllm 真 LLM 复跑 `test_plan_execute_realllm` 3 cases PASS + `test_e2e_real_llm` ChatSession case PASS
- `context_ids`: N/A (本 ADR 不改 pipeline schema)

---

## 背景

### 现状 (per `pdk/loop_agent/README.md` "双循环架构" 段)

HydraForge 当前存在"双循环架构 (C1 决策矩阵)":

| 层 | 拥有者 | 加载方式 | 同步语义 |
|---|---|---|---|
| Chat-loop | ChatSession::chat() | user-turn 边界 | 每 user turn 一次 |
| Agent-loop | `lib/loop/<loop_type>.agent.md` | `loop/run` 工具读盘 | lock-step |
| 原语层 | `include/agenticdsl/pdk/agent_loops/*` (C++) | 直接 `#include` 头 + 构造 | 由调用方决定 |

**README 自承**: "统一重构触发条件: 任一循环实现出现第 3 个消费者 (当前: DSL 层消费者 = ChatSession; C++ 层消费者 = tests)"。本次 request 即满足触发条件。

### 双重 Review 关键发现 (Metis `ses_edfe8b94` + Oracle `ses_edfe8b7d`)

**Metis Deal-breaker DB-1**: `lib/loop/plan_execute.agent.md` 当前**单遍 linear** (plan → execute → verify → end), **无 retry 循环**。`plan_execute_loop.h:141-195` C++ 实现有 `while(true)` retry (max_retries=3)。两套并行实现**从未等价** — C++ 有 retry, DSL 无。

**Oracle Critical C1**: DSL 是 DAG (TopoScheduler:586 显式拒绝环), PlanExecuteLoop 的 Verify → Retry → Planning 回边**物理上无法用 DSL 节点表达**。"单源"必须收窄为 phase 函数级, retry 编排必须留在 C++。

**Oracle Critical C2**: `execute_phase` 跨 retry 累积子图到同一 engine (`plan_execute_loop.h:246-257`)。工具化后 engine 所有权需 `run_id` map + RAII。

**Oracle Major M3**: G1 `DEFINE_AGENT(React)` (`pdk/g1_coding_assistant/src/g1_agent.cpp:132`) 是 ReactLoop 真实生产消费者 (编译期依赖 agent_macros.h LoopDispatcher)。ReactLoop **不能删, 只能薄壳化**。

**Oracle Major M1**: `plan_phase` / `verify_phase` 显式 `req.params.model.clear()` (`plan_execute_loop.h:225, 289` — `fix-generation-request-model-default` 修复点) 与 DSL `llm_call` 节点走 ProviderLLMTool (`pdk_entry.cpp:56-58` `req.params.model = avail.front().name`) 语义分歧, 是 must_realllm 回归点。

**Metis DB-3**: `test_plan_execute_realllm.cpp` 3 cases (must_realllm) 断言精确 C++ 字符串 `"PlanExecuteLoop: completed successfully"` + `result.retries_used <= 3`, 无 DSL 等价目标。AGENTS.md §FULL REGRESSION TEST FLOW 阶段 2 强制 10/10 PASS。

### 三个收敛点 (Metis + Oracle 独立交叉验证)

| 收敛点 | Metis | Oracle |
|---|---|---|
| DAG 无环 → retry 不可表达 | DB-1 | C1 |
| model.clear() vs ProviderLLMTool | M-1 (意图层) | M1 (物理层) |
| "单源"意图 vs 状态机所有权 | DB-3 (测试断言无等价目标) | C2 (engine 跨 retry 累积) |

三处独立命中 = Pattern #8 dual-agent 最高置信度修正优先级。

---

## 决策

### 决策 D1 — Phase helper 抽取 (`include/agenticdsl/pdk/agent_loops/loop_phases.h`)

新增公共头文件, 抽取 3 个 Phase 自由函数 (无状态, 不持引擎):

```cpp
namespace hydraforge::pdk::loop_phases {

// Plan 阶段: LLM 生成 DSL 片段
std::optional<std::string> run_plan_phase(
    agenticdsl::ILLMProvider& llm,
    const std::string& goal,
    const agenticdsl::LayeredContext& ctx,
    std::stop_token token = {});

// Execute 阶段: 解析 + 追加生成的 DSL (跨 retry 累积)
bool run_execute_phase(
    agenticdsl::DSLEngine& engine,
    const std::string& generated_dsl,
    std::optional<std::string>& execute_error_out);

// Verify 阶段: LLM 评估 ExecutionResult
bool run_verify_phase(
    agenticdsl::ILLMProvider& llm,
    const std::string& goal,
    const nlohmann::json& result_data,
    std::stop_token token = {});

}  // namespace
```

**关键不变量**:
- `req.params.model.clear()` 保留 + "NOT redundant" 注释从 `plan_execute_loop.h:225/289` 复制到 helper 头 (D1.inv.model)
- helper 抛异常时, 调用方 (C++ 类) 负责状态机转换 (helper 无 retry 语义)

### 决策 D2 — C++ 类变薄壳委托 helper

`PlanExecuteLoop` / `ReactLoop` / `ForkJoinLoop` 公开 API 零变化, 内部委托 helper:

```cpp
class PlanExecuteLoop {
 public:
  LoopResult run(const std::string& goal, const LayeredContext& ctx, std::stop_token token = {}) {
    // ... Plan/Execute/Verify/Retry 状态机编排保留在类内
    while (true) {
      auto plan_output = loop_phases::run_plan_phase(*llm, goal, ctx, token);
      if (!plan_output) return {failed_phase="Planning", ...};

      std::optional<std::string> exec_err;
      if (!loop_phases::run_execute_phase(*engine_, *plan_output, exec_err)) {
        return {failed_phase="Executing", message="..." + *exec_err};
      }

      if (loop_phases::run_verify_phase(*llm, goal, data, token)) {
        return {success=true, message="PlanExecuteLoop: completed successfully"};
      }
      // ... retry logic 保留
    }
  }
};
```

**关键不变量**:
- 公开 API 零变化 (G1 + test_pdk_macros + 6 个测试 binary 不动)
- retry 编排 (`while(true)` + `retries_used` 计数) **保留在类内** — DAG 无环约束
- engine_ 成员保留 (跨 retry 累积 `continue_with_generated_dsl`)
- `result.message` / `state_` / `retries_used` 字段不变

### 决策 D3 — G1 / DEFINE_AGENT 兼容

`DEFINE_AGENT(name, loop_type)` 宏 (在 `include/agenticdsl/pdk/agent_macros.h`) 展开路径不变:
- `using Type = ReactLoop;` 等 specialization 保留
- 实例化 `XXXAgent` 类仍然派生自原 3 个 C++ 类
- G1 `pdk/g1_coding_assistant/src/g1_agent.cpp:132` 编译期依赖保留

**新增 macro 注释**: `agent_macros.h:52` 加 `// ADR-0089 v1.3: 委托 shared phase helpers (per ADR-0021 §3.2 amendment)`

### 决策 D4 — ForkJoinLoop 语义不变 (用户决策, Oracle M4)

C++ ForkJoinLoop 继续使用 `DomainWorkerPool` 4-worker + InMemoryBus 事件同步 (`fork_join_loop.h:176-293`)。DSL `lib/loop/fork_join.agent.md` 继续走 fork node + `loop/process_task` 路径, **不合并**。ADR-0087 benchmark 4-worker 3.3× 加速承载对象不变。

### 决策 D5 — `pdk_entry.cpp` 可选 phase 工具 (新)

新增 `loop/run_plan` / `loop/run_verify` 工具函数注册 (C1 工具先例延续), DSL 可选调用:

```cpp
registry.register_tool_function("loop/run_plan", ..., [](args) {
  return loop_phases::run_plan_phase(*tls_parent_provider, ...);
});
registry.register_tool_function("loop/run_verify", ..., [](args) {
  return loop_phases::run_verify_phase(*tls_parent_provider, ...);
});
```

**关键不变量**:
- **不强制 DSL 改写** — 现有 DSL 文件 (`lib/loop/*.agent.md`) 行为零变化, 新工具只是可选
- engine 生命周期用 `run_id` map + RAII guard (解决 Oracle C2 状态问题)

---

## 实施

### Phase 0 — helper 抽取 (commit 1)

抽取 `loop_phases.h` / `loop_phases.cpp`, 复制 `model.clear()` 注释 + "NOT redundant" 段落。**PlanExecuteLoop 暂时不调用 helper** (逐步迁移)。

### Phase 1 — PlanExecuteLoop 薄壳化 (commit 2)

`plan_execute_loop.cpp` 内部委托 helper, 公开 API 零变化。**5 个测试文件零修改** (mock + restart + realllm + plan_execute_loop_integration + pdk_macros DEFINE_AGENT)。

### Phase 2 — ReactLoop + ForkJoinLoop 薄壳化 (commit 3)

同样模式, ReactLoop 用 `loop_phases::run_plan_phase` (或单轮简化版), ForkJoinLoop 保持 DomainWorkerPool (用户决策 D4)。

### Phase 3 — pdk_entry.cpp 可选工具 (commit 4)

新增 `loop/run_plan` + `loop/run_verify` 工具注册, DSL 可选调用 (不强制改写)。

### Phase 4 — docs 同步 (commit 5)

更新 `pdk/loop_agent/README.md` "双循环架构" 段 (C1 决策矩阵表述修正: DSL 与 C++ 类现在共享 helper, 不再是"双轨"而是"单源 + 双入口")。更新 `docs/specs/architecture.md` L3 契约层 docMap entry。

---

## 不变量

- D1.inv.model: `req.params.model.clear()` 在 helper 内保留, 注释解释 (Oracle M1 修复链延续)
- D2.inv.api: 3 个 C++ 类公开 API 零变化 (G1 + 6 个测试 binary 零修改)
- D2.inv.retry: retry 编排 (`while(true)` + `retries_used`) 保留在 `PlanExecuteLoop::run()` 内, helper 无 retry 语义
- D2.inv.engine: `engine_` 成员保留在类内, 跨 retry 累积 `continue_with_generated_dsl` (Oracle C2)
- D4.inv.forkjoin: ForkJoinLoop `DomainWorkerPool` 4-worker 语义不变, ADR-0087 benchmark 不回归
- D5.inv.optional: 新工具 `loop/run_plan` / `loop/run_verify` 可选, DSL 文件不强制改写

---

## 影响范围

| 类型 | 文件 | 操作 |
|---|---|---|
| 新增 | `include/agenticdsl/pdk/agent_loops/loop_phases.h` | 新建 (Phase 0) |
| 新增 | `src/modules/agent_loops/loop_phases.cpp` | 新建 (Phase 0) |
| 修改 | `include/agenticdsl/pdk/agent_loops/plan_execute_loop.h/.cpp` | 内部委托 helper (Phase 1) |
| 修改 | `include/agenticdsl/pdk/agent_loops/react_loop.h/.cpp` | 同 (Phase 2) |
| 修改 | `include/agenticdsl/pdk/agent_loops/fork_join_loop.h/.cpp` | 不变 (用户决策 D4) |
| 修改 | `include/agenticdsl/pdk/agent_macros.h` | 加 ADR 注释 (D3) |
| 修改 | `pdk/loop_agent/src/pdk_entry.cpp` | 新增 2 工具 (Phase 3) |
| 修改 | `pdk/loop_agent/README.md` | "双循环架构" 段改写 |
| 修改 | `docs/specs/architecture.md` | L3 契约层 docMap 更新 |
| 新增 | `openspec/changes/2026-10-09-consolidate-loop-phases-to-shared-helpers/` | OpenSpec change artifacts |
| **零修改** | `tests/test_pdk_*.cpp` (5 个) | D2.inv.api |
| **零修改** | `tests/test_plan_execute_*.cpp` (3 个, 含 must_realllm) | D2.inv.api |
| **零修改** | `pdk/g1_coding_assistant/src/g1_agent.cpp` | D3 |
| **零修改** | `examples/pdk_chat_demo/tests/test_*_loop_integration.cpp` (2 个) | D2.inv.api |

**总修改量估算**: ~10 文件 (5 新 + 5 改), Medium 复杂度, 1-2 天实施 (per Oracle 估算)。

---

## 风险登记

| # | 风险 | 严重度 | 缓解 |
|---|---|---|---|
| R1 | model.clear() helper 移植遗漏 → must_realllm 回归 | 🔴 高 | D1.inv.model + Phase 0 抽 helper 时显式 grep `req.params.model.clear()` 数量 (1 处) 必须保留 |
| R2 | execute_phase 跨 retry 累积语义改变 → C++ 类行为漂移 | 🟠 中 | D2.inv.engine + 现有 test_pdk_plan_execute 5 cases 自动覆盖 (断言 result.final_context.working["meta"]["plan_appended"] == true) |
| R3 | ForkJoinLoop 行为漂移 | 🟡 低 | D4.inv.forkjoin + ForkJoinLoop 内部不改 + ADR-0087 benchmark 复跑 |
| R4 | G1 编译断 | 🟠 中 | D3 + Phase 2 实施前 local build 验证 |
| R5 | 测试与 e2e 路径不一致 (用户原始动机) | ✅ 已消除 | D1 + D2 + D5 单源, 测试覆盖 = e2e 路径覆盖 |

---

## 治理流程

按 AGENTS.md Pattern #4 SHIP-with-fixes + Pattern #11 dual Oracle review:
1. rdd-arch (本 stage): ADR amendment ✅ Proposed
2. rdd-planner: 创建 `.rddf/improvements/consolidate-loop-phases.md` 5-segment draft
3. rdd-builder P0: 创建 OpenSpec change `2026-10-09-consolidate-loop-phases-to-shared-helpers/`
4. **24h cooling-off** (per Single-Dev 模式): 用户 review
5. rdd-builder P2: 实施 (5 atomic commits per Phase 0-4)
6. Oracle post-impl SHIP-with-fixes review
7. archive OpenSpec change + ADR flip to ✅ Approved
8. AGENTS.md Recent Changes 登记

---

## 替代方案 (排除)

### 方案 A — 直接删除 C++ 类, 全迁 DSL

**排除理由** (per Oracle C1 + M3 + Metis DB-1+DB-2):
- DAG 无环, retry 不可表达 → PlanExecute 行为降级
- G1 + DEFINE_AGENT 编译期依赖破坏
- agent_loops/*.h 公开头 API 删除 = PDK ABI BREAKING

### 方案 B — DSL 引擎扩展循环节点

**排除理由**:
- 超本 amendment scope, 需独立 ADR
- TopoScheduler DAG → 条件边/循环边的语义变化影响范围远超 loop
- 推迟到 ADR-0034 (性能元数据契约) 之后再讨论

### 方案 C — C++ 类不变, 仅文档同步

**排除理由**:
- 用户原始动机 (测试/e2e coverage gap) 未解决
- 仅推迟决策, 保留双轨技术债

---

## 参考资料

- [ADR-0021 §3.2 Agent Loop 模板](adr-0021-pdk-design.md#32-agent-loop-模板) — 修订对象
- [ADR-0067 L2/L3/L4 分层架构](adr-0067-layered-plugin-architecture-split.md) — PDK 头依赖约束 P3
- [ADR-0041 PluginLoader 生命周期扩展](adr-0041-pluginloader-lifecycle-extension.md) — ABI 边界
- [ADR-0087 Cloud Adapter Threading Model](adr-0087-cloud-adapter-threading-model.md) — ForkJoinLoop 4-worker benchmark
- [ADR-0020 Thread Model Isolation](adr-0020-thread-model-isolation.md) — DomainWorkerPool 模式
- [ADR-0082 Agent as First-Class Registry](adr-0082-agent-first-class-registry.md) — L4 编排层 (与本 L3 区分)
- Metis session `ses_edfe8b94effeLHZtjyKMyWFTXO` — Intent + AI failure modes
- Oracle session `ses_edfe8b7daffeysJHJPj9PaG5SZ` — Architecture + test migration + ABI
- `pdk/loop_agent/README.md` "双循环架构 (C1 决策矩阵)" 段 — 现状描述
- `tests/AGENTS.md` §REAL-LLM TEST PATTERNS — must_realllm 测试模板
- AGENTS.md §Reverse Indicator Rule (5 字段强制)
- AGENTS.md Pattern #4 (SHIP-with-fixes + Oracle 介入)
- AGENTS.md Pattern #8 (OpenSpec Change pre-implementation dual-agent review)
- AGENTS.md Pattern #11 (Async Worker + Dual Oracle dual-review SHIP-with-fixes cycle)

---

> **本 amendment 状态**: 🔍 Proposed — 未定稿, 24h cooling-off 后由架构组 review 决定
> **创建时间**: 2026-10-09
> **下次 review 触发条件**: rdd-planner 创建 `.rddf/improvements/consolidate-loop-phases.md` 完成后, 由架构组审批