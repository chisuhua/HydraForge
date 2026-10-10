# Design: consolidate-loop-phases-to-shared-helpers

**Change ID**: `consolidate-loop-phases-to-shared-helpers`
**Status**: 🔍 Proposed
**关联 ADR**: [ADR-0089 v1.3](../../docs/adr/adr-0089-v1-3-amendment-loop-phase-shared-helpers.md)
**前置文档**: [proposal.md](proposal.md) · [`.rddf/improvements/consolidate-loop-phases-to-shared-helpers.md`](../../.rddf/improvements/consolidate-loop-phases-to-shared-helpers.md)

---

## Context

HydraForge 当前存在"双循环架构 (C1 决策矩阵)"(per `pdk/loop_agent/README.md`):

| 层 | 拥有者 | 加载方式 | 同步语义 |
|---|---|---|---|
| Chat-loop | ChatSession::chat() | user-turn 边界 | 每 user turn 一次 |
| Agent-loop | `lib/loop/<loop_type>.agent.md` | `loop/run` 工具读盘 | lock-step |
| 原语层 | `include/agenticdsl/pdk/agent_loops/*` (C++) | 直接 `#include` 头 + 构造 | 由调用方决定 |

**核心问题**: C++ 类公开 API 零改动 + 内部委托 phase helper, 实现"phase 逻辑单一权威 + DSL/C++ 双入口可调用"的单源架构。彻底消除"测试覆盖 ≠ 生产路径"的双轨风险。

**Oracle Critical (per Oracle `ses_edfe8b7d`)**:
- **C1**: DSL 是 DAG (`topo_scheduler.cpp:586` 显式拒绝环), PlanExecuteLoop 的 Verify → Retry → Planning 回边**物理上无法**用 DSL 节点表达
- **C2**: `execute_phase` 跨 retry 累积子图到同一 engine (`plan_execute_loop.h:246-257`), helper 化后 engine 所有权需保留在 C++ 类内

**Metis Deal-breaker (per Metis `ses_edfe8b94`)**:
- **DB-1**: `lib/loop/plan_execute.agent.md` 当前**单遍 linear** (plan → execute → verify → end, 无 retry), C++ `PlanExecuteLoop` 有 `while(true)` retry
- **DB-2**: `agent_macros.h:52-67` LoopDispatcher 编译期引用 3 个 C++ 类 + G1 是 ReactLoop 真实生产消费者
- **DB-3**: `test_plan_execute_realllm.cpp` 3 cases 断言精确 C++ 字符串, 无 DSL 等价目标

**收敛信号**: 3 个收敛点独立交叉命中（DAG 无环 / model.clear() 分歧 / 状态机所有权）= Pattern #8 dual-agent 最高置信度。

---

## Goals / Non-Goals

**Goals:**
- 抽取 phase 自由函数（`run_plan_phase` / `run_execute_phase` / `run_verify_phase`）作为单源实现
- `PlanExecuteLoop` / `ReactLoop` 变薄壳委托 helper, **公开 API 零变化**
- `loop/run_plan` + `loop/run_verify` 工具函数注册, DSL **可选**调用
- 零行为变化: 6 个测试 binary + 3 个 must_realllm + 2 个 examples integration 测试零修改
- 零 ABI 变化: `agent_loops/*.h` 公开头签名零变化, 无需 `sync-pdk.sh` Dual-Repo 同步
- 零 G1 破坏: `DEFINE_AGENT(React)` 编译期路径不变

**Non-Goals:**
- ❌ 删除 `agent_loops/*.h` 公开头（DEFINE_AGENT + G1 编译期依赖, 物理不可删）
- ❌ 删除 `lib/loop/*.agent.md` DSL 文件（ChatSession 生产路径）
- ❌ 扩展 DSL 引擎支持循环节点（TopoScheduler DAG → 条件边/循环边, 远超 scope, 需独立 ADR）
- ❌ 改造 `lib/loop/plan_execute.agent.md` 实现 retry 循环（DAG 不可达, 物理约束）
- ❌ 修改 `ForkJoinLoop` 内部实现（用户决策 D4: DomainWorkerPool 4-worker 不变）
- ❌ 迁移 must_realllm 测试到 DSL 路径（C++ 类公开 API 不变, 测试零修改）
- ❌ 替换 `g1_coding_assistant` 的 ReactLoop 消费者为 DSL（M3 兼容）

---

## Decisions

### 决策 D1 — Phase helper 抽取 (`include/agenticdsl/pdk/agent_loops/loop_phases.h`)

**新增头文件 + 实现文件, 抽取 3 个 Phase 自由函数**:

```cpp
// include/agenticdsl/pdk/agent_loops/loop_phases.h
#pragma once
#include "agenticdsl/contract/illm_provider.h"
#include "agenticdsl/types/layered_context.h"
#include "common/llm/llm_types.h"
#include "core/engine.h"

#include <nlohmann/json.hpp>
#include <optional>
#include <stop_token>
#include <string>

namespace hydraforge::pdk::loop_phases {

// Plan 阶段: LLM 生成 DSL 片段
std::optional<std::string> run_plan_phase(
    agenticdsl::ILLMProvider& llm,
    const std::string& goal,
    const agenticdsl::LayeredContext& ctx,
    std::stop_token token = {});

// Execute 阶段: 解析 + 追加生成的 DSL (跨 retry 累积, engine_ 必须 caller 持有)
bool run_execute_phase(
    agenticdsl::DSLEngine& engine,
    const std::string& generated_dsl,
    std::optional<std::string>& execute_error_out);

// Verify 阶段: LLM 评估 ExecutionResult (返回 bool: true=通过, false=重试)
bool run_verify_phase(
    agenticdsl::ILLMProvider& llm,
    const std::string& goal,
    const nlohmann::json& result_data,
    std::stop_token token = {});

}  // namespace hydraforge::pdk::loop_phases
```

**关键设计决策**:

1. **`req.params.model.clear()` 必须保留** (从 `plan_execute_loop.h:225/289` 同步), 含"NOT redundant"大段注释:
   ```cpp
   // ⚠️ NOT redundant: LLMParams = LLMConfig 别名, 默认 model = "gpt-4o-mini" (非空).
   // 若不清空, CloudLLMAdapter::build_request_body L164
   // (req.params.model.empty() ? config_.model : req.params.model) 会拿默认
   // "gpt-4o-mini" 遮蔽 adapter 构造时 factory 设置的真实 model
   // (如 deepseek-v4-flash) → deepseek server 拒绝
   // ("you passed gpt-4o-mini"). 站点无 model 概念 (model 由 adapter/factory 持有),
   // 清空让 adapter fallback.
   // 详见 openspec/changes/fix-generation-request-model-default/.
   req.params.model.clear();
   ```

2. **helper 无 retry 语义**: 3 个函数都是原子操作, 不持 while loop, 不计数 retries_used — 这些编排属于 C++ 薄壳类的状态机 (D2.inv.retry)

3. **helper 无状态**: 不持 engine_ / bus_, 接收引用参数 (调用方负责生命周期, D2.inv.engine)

4. **execute_phase 接受 reference 而非 unique_ptr**: caller (PlanExecuteLoop) 拥有 engine_, helper 只调 `continue_with_generated_dsl` 不获取所有权

### 决策 D2 — C++ 类薄壳委托 (`plan_execute_loop.h/.cpp` + `react_loop.h/.cpp`)

**PlanExecuteLoop::run() 内部改造**:

```cpp
LoopResult PlanExecuteLoop::run(const std::string& goal,
                                const LayeredContext& ctx,
                                std::stop_token token) {
  LoopResult result;
  result.final_context = ctx;
  result.final_context.working["data"] = nlohmann::json::object();

  if (!engine_) { /* 零变化 */ }
  agenticdsl::ILLMProvider* llm = engine_->get_llm_provider();
  if (!llm) { /* 零变化 */ }
  if (token.stop_requested()) { /* 零变化 */ }

  while (true) {
    result.total_steps++;

    // === Plan 阶段: 委托 helper ===
    state_ = State::Planning;
    auto plan_output = loop_phases::run_plan_phase(*llm, goal, ctx, token);
    if (!plan_output.has_value()) {
      result.success = false;
      result.message = "PlanExecuteLoop: plan phase failed (empty LLM response)";
      result.failed_phase = "Planning";
      state_ = State::Done;
      return result;
    }

    // === Execute 阶段: 委托 helper, engine_ 仍由本类持有 ===
    state_ = State::Executing;
    std::optional<std::string> exec_err;
    bool exec_ok = loop_phases::run_execute_phase(*engine_, *plan_output, exec_err);
    if (!exec_ok) {
      result.success = false;
      result.message = "PlanExecuteLoop: execute phase failed: " + exec_err.value_or("unknown");
      result.failed_phase = "Executing";
      state_ = State::Done;
      return result;
    }
    result.final_context.working["meta"]["plan_appended"] = true;

    // === Verify 阶段: 委托 helper ===
    state_ = State::Verifying;
    const auto& working = result.final_context.working;
    std::string data_dump = working.is_object() && working.contains("data")
                                ? working["data"].dump()
                                : std::string{"{}"};
    bool verify_ok = loop_phases::run_verify_phase(
        *llm, goal, nlohmann::json::parse(data_dump), token);
    if (verify_ok) {
      result.success = true;
      result.message = "PlanExecuteLoop: completed successfully";  // 字符串零变化 (must_realllm 断言)
      state_ = State::Done;
      return result;
    }

    // === Retry 状态机保留 (D2.inv.retry) ===
    if (result.retries_used >= max_retries_) {
      result.success = false;
      result.message = "PlanExecuteLoop: verify failed after " +
                       std::to_string(result.retries_used) + " retries";
      result.failed_phase = "Verifying";
      state_ = State::Done;
      return result;
    }
    state_ = State::Retry;
    result.retries_used++;
  }
}
```

**关键设计决策**:

1. **公开 API 零变化**: `LoopResult` 字段 (`success` / `message` / `retries_used` / `failed_phase` / `final_context`) + `State` 枚举 + `run()` 签名全部零变化 (D2.inv.api)
2. **`message` 字符串不变**: `"PlanExecuteLoop: completed successfully"` / `"PlanExecuteLoop: plan phase failed (empty LLM response)"` 等字符串字面量零变化, 确保 `test_plan_execute_realllm.cpp` must_realllm 断言不退化
3. **retry 编排保留**: `while(true)` + `retries_used` 计数 + `state_ = State::Retry` 都在类内 (D2.inv.retry, Oracle C1)
4. **engine_ 成员保留**: 跨 retry 累积 `continue_with_generated_dsl` 调用同一 engine_ (D2.inv.engine, Oracle C2)

**ReactLoop 同样模式**: `run_once()` 内部委托 `loop_phases::run_plan_phase` (Plan/Act 简化版), 公开 API 零变化

### 决策 D3 — G1 + DEFINE_AGENT 兼容

`agent_macros.h:52` 加 ADR 注释:

```cpp
namespace hydraforge::pdk {
// ADR-0089 v1.3 amendment (2026-10-09): 委托 shared phase helpers (per ADR-0021 §3.2 amendment)
// LoopDispatcher specializations 保留 ReactLoop/PlanExecuteLoop/ForkJoinLoop 引用,
// 类变薄壳委托 helper (见 loop_phases.h), 公开 API 零变化 (G1 + test_pdk_macros 不动)
template <typename Agent> struct LoopDispatcher { using Type = void; };
template <> struct LoopDispatcher<AgentLoopType::React> { using Type = ReactLoop; };
template <> struct LoopDispatcher<AgentLoopType::PlanExecute> { using Type = PlanExecuteLoop; };
template <> struct LoopDispatcher<AgentLoopType::ForkJoin> { using Type = ForkJoinLoop; };

} // namespace
```

**零代码改动** — 仅加注释。G1 + test_pdk_macros 编译期路径零变化 (Oracle M3).

### 决策 D4 — ForkJoinLoop 语义不变 (用户决策)

`ForkJoinLoop` 内部实现 (DomainWorkerPool 4-worker + InMemoryBus 事件同步 + fail-fast, `fork_join_loop.h:176-293`) **零代码改动**。DSL `lib/loop/fork_join.agent.md` 继续走 fork node + `loop/process_task` 路径, **不合并**。ADR-0087 benchmark 4-worker 3.3× 加速承载对象不变。

`fork_join_loop.h/.cpp` 不在 commit 2/3 改动范围内 (per Phase 1-2 tasks.md 仅动 PlanExecuteLoop + ReactLoop)。

### 决策 D5 — `pdk_entry.cpp` 可选 phase 工具

新增 `loop/run_plan` + `loop/run_verify` 工具函数注册, **DSL 可选调用** (不强制现有 DSL 文件改写):

```cpp
// pdk/loop_agent/src/pdk_entry.cpp
registry.register_tool_function(
    "loop/run_plan",
    ::agenticdsl::ToolMetadata{
        .name = "loop/run_plan",
        .description = "Execute Plan phase: LLM generates plan DSL from cluster context",
        .domain = "loop",
        .category = ::agenticdsl::ToolCategory::Execute,
        .min_layer = ::agenticdsl::LayerProfile::Workflow,
        .approval = ::agenticdsl::ApprovalPolicy{
            .requires_approval_in_plan = false,
            .requires_approval_in_agent = true,
            .requires_approval_in_yolo = false,
            .force_approval_always = false
        },
        .allowed_layers = {::agenticdsl::LayerProfile::Workflow}
    },
    [](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
        std::string goal = str_arg(args, "goal");
        if (goal.empty()) {
            return {{"ok", false}, {"success", false},
                    {"error_code", "InvalidParams"},
                    {"error", "Missing 'goal' argument"}};
        }
        // 注: 调用方负责 engine 生命周期 (Phase 3 可选, 不强制)
        if (!tls_parent_provider) {
            return {{"ok", false}, {"success", false},
                    {"error_code", "Unknown"},
                    {"error", "Parent LLM provider not set. Call loop/set_parent_provider first."}};
        }
        LayeredContext ctx;
        auto plan_output = loop_phases::run_plan_phase(
            *tls_parent_provider, goal, ctx, std::stop_token{});
        if (!plan_output.has_value()) {
            return {{"ok", false}, {"success", false},
                    {"error_code", "Unknown"},
                    {"error", "Plan phase returned empty"}};
        }
        return {{"ok", true}, {"success", true},
                {"error_code", nullptr},
                {"plan", *plan_output}};
    });

// loop/run_verify 类似实现, 调用 loop_phases::run_verify_phase
```

**关键设计决策**:

1. **不接收 bus_ptr**: 遵循 C1 工具规则 (D6 bus_ptr 边界规则), 事件发射由 caller 负责
2. **不接收 engine**: helper 是无状态的, engine 生命周期由 caller (Phase 5 caller 用 `run_id` map + RAII)
3. **不影响现有 DSL**: `lib/loop/*.agent.md` 文件零修改, 新工具只是可选能力

---

## Risks / Trade-offs

### Risk 1: model.clear() 移植遗漏 → must_realllm 回归

**严重度**: 🔴 高 (must_realllm 10/10 必过)

**缓解**:
- D1.inv.model 强制约束: `req.params.model.clear()` 必须从 `plan_execute_loop.h:225/289` 同步到 helper 头, 含完整 "NOT redundant" 注释
- commit 1 (`feat(loop_phases): 抽 helper`) 实施前 `grep "req.params.model.clear()"` 必须命中至少 3 处 (PlanExecuteLoop × 2 + helper × 1)
- Phase 4 验证门: `ctest -L must_realllm` 含 `test_plan_execute_realllm` 3 cases 必过

### Risk 2: execute_phase 跨 retry 累积语义改变

**严重度**: 🟠 中 (C++ 类行为漂移)

**缓解**:
- D2.inv.engine 强制约束: `engine_` 成员保留在类内, helper 接受 reference 而非 unique_ptr
- 现有 `test_pdk_plan_execute.cpp` 5 cases 自动覆盖 (`result.final_context.working["meta"]["plan_appended"] == true` 断言验证累积语义)
- 现有 `test_plan_execute_restart.cpp` 3 cases 自动覆盖 retry 状态机

### Risk 3: ForkJoinLoop 行为漂移 (虽 D4 不动)

**严重度**: 🟡 低 (但 ship gate 必过)

**缓解**:
- D4.inv.forkjoin: `ForkJoinLoop` 内部零代码改动
- Phase 5 验证门: `ctest -R test_adr_0087_step5_1_benchmark` 4-worker 3.3× 加速数据不退化
- `test_pdk_fork_join.cpp` 5 cases 零修改 (D2.inv.api)

### Risk 4: G1 编译期断

**严重度**: 🟠 中 (M3 真实生产消费者)

**缓解**:
- D3: `agent_macros.h` 仅加注释, 零代码改动
- commit 2/3 实施后本地 build `cmake --build build --target g1_coding_assistant` 验证
- `nm build/pdk/g1_coding_assistant/libG1CodingAssistant.so | grep ReactLoop` 应命中符号 (ABI=2 不变)

### Risk 5: Phase 5 新工具的 engine 状态归属

**严重度**: 🟡 低 (Phase 5 是 D5 可选能力, 不强制)

**缓解**:
- Phase 5 caller 用 `run_id` map + RAII guard (per Oracle C2 建议, Sprint 32 self-pipe trick 同模式)
- 当前 Phase 1-4 不强制 Phase 5 实施, 可选

### Trade-off 1: 引入 loop_phases.h vs 内部 inline

**选项 A**: 新增 header (当前方案)
- ✅: 可被 pdk_entry.cpp 工具复用 (D5), 单元测试可独立测
- ❌: 新增文件, 编译时间略增 (~1s)

**选项 B**: helper 内联到 agent_loops/loop_result.h
- ❌: pdk_entry.cpp 工具无法复用, DSL 可选调用失去实现源
- 选 A

### Trade-off 2: 公开 ABI 影响

**当前方案**: 公开 ABI 零变化 (`agent_loops/*.h` 签名零变化, 新增 `loop_phases.h` 是 L3 内部头)
- ✅: 无需 `scripts/sync-pdk.sh` Dual-Repo 同步
- ✅: 现有 `pdk/loop_agent/libLoopAgent.so` ABI version=2 不变
- 接受: `loop_phases.h` 内部头不进 PDK 公开契约 (per ADR-0067 R3 L3 contract-only 规则, `agent_loops/*.h` 仍 include `core/engine.h` 是 pre-existing P3 违规, 不在本 change 修复范围)

---

## Implementation Plan (5 atomic commits)

per ADR-0089 §实施 + tasks.md 详细任务:

1. **commit 1 (Phase 0)**: `feat(loop_phases): 抽 plan/verify/execute helper 自由函数`
   - 新增 `include/agenticdsl/pdk/agent_loops/loop_phases.h` (3 自由函数声明)
   - 新增 `src/modules/agent_loops/loop_phases.cpp` (3 自由函数实现, 移植 `req.params.model.clear()` 注释)
   - PlanExecuteLoop 暂时不调用 helper (仅 helper 抽取, 验证编译通过)

2. **commit 2 (Phase 1)**: `refactor(plan_execute): 内部委托 helper, 公开 API 不变`
   - 修改 `include/agenticdsl/pdk/agent_loops/plan_execute_loop.h/.cpp` 委托 helper
   - `result.message` 字符串字面量零变化
   - `tests/test_pdk_plan_execute.cpp` + `tests/test_plan_execute_restart.cpp` + `tests/test_plan_execute_realllm.cpp` 零修改

3. **commit 3 (Phase 2)**: `refactor(react_loop): 内部委托 helper, 公开 API 不变`
   - 修改 `include/agenticdsl/pdk/agent_loops/react_loop.h/.cpp` 委托 helper (单轮简化版)
   - `agent_macros.h:52` 加 ADR-0089 注释 (D3)
   - `tests/test_pdk_macros.cpp` + `pdk/g1_coding_assistant` 编译验证

4. **commit 4 (Phase 3)**: `feat(pdk_entry): 新增 loop/run_plan + loop/run_verify 工具注册`
   - 修改 `pdk/loop_agent/src/pdk_entry.cpp` 注册 2 新工具
   - `lib/loop/*.agent.md` 文件**零修改** (DSL 可选调用, 不强制改写)

5. **commit 5 (Phase 4)**: `docs: pdk/loop_agent/README.md 双循环架构段改写 + specs/architecture.md L3 契约层 docMap 更新`
   - 改写 `pdk/loop_agent/README.md` "双循环架构" 段为 "phase helper + 薄壳 + DSL 可选调用"
   - 更新 `docs/specs/architecture.md` L3 契约层 docMap entry
   - `docs/adr/adr-0021-pdk-design.md` §3.2 加指回 ADR-0089 指针

---

## References

- [ADR-0089 v1.3 amendment](https://github.com/chisuhua/HydraForge/blob/main/docs/adr/adr-0089-v1-3-amendment-loop-phase-shared-helpers.md) — 本 design 对应 ADR
- [proposal.md](proposal.md) — 本 change proposal
- [tasks.md](tasks.md) — 实施任务清单 (TDD 5-step + 5 atomic commits)
- [specs/loop-phases-shared-helpers/spec.md](specs/loop-phases-shared-helpers/spec.md) — 新 helper spec
- [`.rddf/improvements/consolidate-loop-phases-to-shared-helpers.md`](https://github.com/chisuahua/HydraForge/blob/main/.rddf/improvements/consolidate-loop-phases-to-shared-helpers.md) — 5-segment draft
- Metis session `ses_edfe8b94effeLHZtjyKMyWFTXO` — Intent + ambiguity
- Oracle session `ses_edfe8b7daffeysJHJPj9PaG5SZ` — Architecture + physical feasibility
- AGENTS.md Pattern #4 (SHIP-with-fixes) + Pattern #8 (dual-agent review) + Pattern #11 (Async Worker cycle)
- `topo_scheduler.cpp:586` — DSL DAG 拒绝环 (Oracle C1 物理约束)
- `plan_execute_loop.h:141-195` — C++ retry 主循环 (Oracle C1 retry 在 C++)
- `plan_execute_loop.h:225/289` — `req.params.model.clear()` 修复点 (Oracle M1)
- `plan_execute_loop.h:246-257` — `execute_phase` engine_ 累积 (Oracle C2)
- `agent_macros.h:52-67` — LoopDispatcher 模板 3 specialization (Oracle M3 G1 依赖)
- `pdk/g1_coding_assistant/src/g1_agent.cpp:132` — `DEFINE_AGENT(React)` 真实生产消费者 (M3)
- `fork_join_loop.h:176-293` — DomainWorkerPool 4-worker 并发 (用户决策 D4 不变)
- `pdk/loop_agent/README.md` "双循环架构 (C1 决策矩阵)" 段 — 现状描述
- `tests/test_pdk_plan_execute.cpp` (5 cases, 零修改 - D2.inv.api)
- `tests/test_pdk_fork_join.cpp` (5 cases, 零修改 - D4 不动)
- `tests/test_plan_execute_restart.cpp` (3 cases, 零修改 - D2.inv.api)
- `tests/test_plan_execute_realllm.cpp` (3 cases must_realllm, 零修改 - D2.inv.api + M1)
- `tests/test_pdk_macros.cpp` (DEFINE_AGENT 测试, 零修改 - D3)
- `examples/pdk_chat_demo/tests/test_plan_execute_loop_integration.cpp` (1 case, 零修改)
- `examples/pdk_chat_demo/tests/test_fork_join_loop_integration.cpp` (1 case, 零修改)
- ADR-0021 §3.2 (修订对象) · ADR-0067 (L2/L3/L4 分层) · ADR-0041 (ABI) · ADR-0087 (benchmark)