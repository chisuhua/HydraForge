# consolidate-loop-phases-to-shared-helpers

**Change ID**: `consolidate-loop-phases-to-shared-helpers`
**Status**: 🔍 Proposed（OpenSpec change 立项；24h cooling-off 起点 = 立项完成时）
**优先级**: P1（消除生产 coverage gap, 但不阻塞现有 ship）
**关联 ADR**: [ADR-0089 v1.3](../../docs/adr/adr-0089-v1-3-amendment-loop-phase-shared-helpers.md)（🔍 Proposed）修订 [ADR-0021 §3.2](../../docs/adr/adr-0021-pdk-design.md#32-agent-loop-模板) v0.2
**关联 improvement**: [`.rddf/improvements/consolidate-loop-phases-to-shared-helpers.md`](../../.rddf/improvements/consolidate-loop-phases-to-shared-helpers.md)（5-segment draft, 238 行）
**Dual-Agent Review**: Metis `ses_edfe8b94effeLHZtjyKMyWFTXO` + Oracle `ses_edfe8b7daffeysJHJPj9PaG5SZ`（Pattern #8 dual-agent review 完成）

---

## Why

`pdk/loop_agent/README.md` "双循环架构 (C1 决策矩阵)" 自承存在"测试覆盖 ≠ 生产路径"的双轨风险：

- **C++ 层消费者** = 6 个 test binary（含 3 个 must_realllm 真实 LLM 测试）
- **DSL 层消费者** = ChatSession::chat() 生产路径（走 `loop/run` + `lib/loop/*.agent.md`）
- **两个层从未等价**：DSL `plan_execute.agent.md` 当前是单遍 linear（无 retry），C++ `PlanExecuteLoop` 有 `while(true)` retry（max_retries=3）

用户原始动机："C++ 实现有完整测试，但是实际 e2e 使用的是 DSL 版本"——**测试 PASS 不能保证生产行为正确**。

经 Metis + Oracle 独立交叉验证的 3 个收敛点：
1. **DAG 无环 → retry 不可表达**（`topo_scheduler.cpp:586` 拒绝环）
2. **`model.clear()` vs ProviderLLMTool 语义分歧**（`fix-generation-request-model-default` 修复链延续）
3. **"单源"意图 vs 状态机所有权**（execute_phase 跨 retry 累积子图到 engine_）

---

## What Changes

### 5 项决策（per ADR-0089 v1.3 D1-D5）

| # | 决策 | 改动范围 | 风险 |
|---|---|---|---|
| **D1** | 抽 `include/agenticdsl/pdk/agent_loops/loop_phases.h` 自由函数（`run_plan_phase` / `run_execute_phase` / `run_verify_phase`） | 新增 2 文件 (h + cpp) | 🟠 M1 model.clear() 移植遗漏 → must_realllm 回归 |
| **D2** | `PlanExecuteLoop` / `ReactLoop` 内部委托 helper, **公开 API 零变化** | 2 文件内部重构 | 🟡 薄壳实现引入回归 |
| **D3** | G1 `DEFINE_AGENT(React)` + `agent_macros.h` LoopDispatcher 编译期依赖保留, 仅加 ADR 注释 | 1 行注释 | 🟢 风险极低 |
| **D4** | **ForkJoinLoop 语义不变**（用户决策 D4, DomainWorkerPool 4-worker 不动） | 零代码改动 | 🟢 已锁 |
| **D5** | `pdk_entry.cpp` 新增 `loop/run_plan` + `loop/run_verify` 工具函数注册, **DSL 可选调用**（不强制改写） | 1 文件 + 新 2 工具 | 🟡 新工具 spec 同步 |

### 关键不变量

- **D1.inv.model**: `req.params.model.clear()` 必须保留 + "NOT redundant" 注释从 `plan_execute_loop.h:225/289` 同步到 helper 头
- **D2.inv.api**: 3 个 C++ 类公开 API 零变化（G1 + 6 个测试 binary + examples integration 2 个测试零修改）
- **D2.inv.retry**: retry 编排（`while(true)` + `retries_used`）保留在 `PlanExecuteLoop::run()` 内, helper 无 retry 语义
- **D2.inv.engine**: `engine_` 成员保留在类内, 跨 retry 累积 `continue_with_generated_dsl`（Oracle C2 状态归属）
- **D4.inv.forkjoin**: ForkJoinLoop `DomainWorkerPool` 4-worker 语义不变, ADR-0087 benchmark 不退化

### 不在 scope（明确边界）

- ❌ 删除 `agent_loops/*.h` 公开头（DEFINE_AGENT + G1 编译期断）
- ❌ 删除 `lib/loop/*.agent.md` DSL 文件（ChatSession 生产路径）
- ❌ 扩展 DSL 引擎支持循环节点（TopoScheduler DAG → 条件边/循环边, 远超 scope）
- ❌ 改造 `lib/loop/plan_execute.agent.md` 实现 retry 循环（DAG 不可达, 物理约束）
- ❌ 修改 `ForkJoinLoop` 内部实现（用户决策 D4: 语义不变）
- ❌ 迁移 must_realllm 测试到 DSL 路径（C++ 类公开 API 不变, 测试零修改）
- ❌ 替换 `g1_coding_assistant` 的 ReactLoop 消费者为 DSL（M3 兼容 + 风险最小化）

---

## Capabilities

### New Capabilities

- **`loop-phases-shared-helpers`** — `loop_phases::run_plan_phase` / `run_execute_phase` / `run_verify_phase` 三个 phase 自由函数, 被 C++ 薄壳类与 `pdk_entry` 工具共同调用, 是单一权威实现

### Modified Capabilities

- **`loop-execute-only`** — `pdk/loop_agent/src/pdk_entry.cpp` 新增 `loop/run_plan` + `loop/run_verify` 工具注册, DSL 可选调用（不影响现有 DSL 文件）
- **`react-loop-final-decision-tooling`** (per ADR-0089 §决策 D3) — `DEFINE_AGENT` 编译期路径不变, 仅 `agent_macros.h:52` 加 ADR 注释
- **`loop-run-dsl-execution`** (ChatSession 生产路径) — `lib/loop/*.agent.md` 文件零修改

---

## Impact

### 改动行数估算

| 类别 | 文件数 | 新增行 | 修改行 | 删除行 |
|---|---|---|---|---|
| 新增 header | 1 (`loop_phases.h`) | ~80 (3 自由函数声明 + 注释) | 0 | 0 |
| 新增实现 | 1 (`loop_phases.cpp`) | ~120 (3 自由函数实现) | 0 | 0 |
| 重构 C++ 类 | 2 (`plan_execute_loop.cpp`, `react_loop.cpp`) | 0 | ~40 (内部委托) | ~30 (重复代码) |
| 新工具注册 | 1 (`pdk_entry.cpp`) | ~50 (2 工具注册 + 实现) | 0 | 0 |
| ADR 注释 | 1 (`agent_macros.h`) | 1 (注释行) | 0 | 0 |
| 文档同步 | 3 (ADR-0021 §3.2 + pdk/loop_agent/README.md + specs/architecture.md) | ~30 | ~50 | ~20 |
| **小计** | **~10 文件** | **~280** | **~90** | **~50** |

### 必过验证门（per AGENTS.md §FULL REGRESSION TEST FLOW）

#### 阶段 1: 单元测试 + mock (快速, ~2s)

```bash
ctest --test-dir build -LE must_realllm --output-on-failure
ctest --test-dir build/examples/pdk_chat_demo/tests -LE must_realllm --output-on-failure
ctest --test-dir build/examples/pdk_chat_demo_evolution/tests -LE must_realllm --output-on-failure
```

**预期**: 100% PASS, 0 failures

#### 阶段 2: must_realllm 真实 LLM（消耗 token, ~120s, 必须设 `DEEPSEEK_API_KEY`）

```bash
ctest --test-dir build -L must_realllm --output-on-failure
ctest --test-dir build/examples/pdk_chat_demo/tests -L must_realllm --output-on-failure
```

**预期**: 10/10 + 4/4 PASS（含 `test_plan_execute_realllm` 3 cases + `test_e2e_real_llm` ChatSession case）

#### 阶段 4: ABI 不退化

```bash
# G1 编译通过 (DEFINE_AGENT 路径不变)
cmake --build build --target g1_coding_assistant -j$(nproc)
nm build/pdk/g1_coding_assistant/libG1CodingAssistant.so | grep "DEFINE_AGENT\|ReactLoop"
# 预期: symbols 完整, ABI version=2 (per ADR-0041)

# ForkJoinLoop benchmark 不退化
ctest -R test_adr_0087_step5_1_benchmark --output-on-failure
# 预期: 4-worker 3.3× 加速数据不退化
```

#### 阶段 5: ship gate 工具

```bash
python3 tools/adr_lint.py 2>&1 | tail -3
# 预期: ✓ 所有 ADR 通过 lint 检查 (新增 ADR-0089, ADR-0021 §3.2 修订注记)
python3 tools/docs_drift_audit.py 2>&1 | grep SUMMARY
# 预期: SUMMARY: N DRIFT (已知 3 + 1), M WARNING, 本 change 不新增
```

### 公共 ABI 影响

- **零 ABI 变化**: 3 个 C++ loop 类公开 API 零变化（仅内部实现委托 helper）
- **零 PDK 公开头变化**: `agent_loops/*.h` 头文件签名零变化（新增 `loop_phases.h` 是 L3 内部头, 不在 PDK 公开接口）
- **零 PDK Dual-Repo 同步需求**: `scripts/sync-pdk.sh` 无需更新（公开接口零变化）

---

## Non-goals

- 不删除 C++ loop 类（DEFINE_AGENT + G1 编译期依赖, 物理不可删）
- 不删除 DSL 文件（ChatSession 生产路径）
- 不扩展 DSL 引擎加循环节点（超 scope, 需独立 ADR）
- 不迁移 must_realllm 测试到 DSL 路径（C++ 类公开 API 不变, 测试零修改）
- 不重写 `ForkJoinLoop` 内部实现（用户决策 D4: DomainWorkerPool 4-worker 不变）
- 不修改 `react.agent.md` 或 `fork_join.agent.md` DSL 文件（Phase 3 DSL 可选调用新工具, 不强制）
- 不引入新的 std::expected / std::variant 类型（保持 C++23 当前用法）

---

## Cooling-Off

**起点**: 本 OpenSpec change 创建完成时（2026-10-09T18:53Z, `openspec new change` 完成）
**必过冷却**: 24h（至 2026-10-10T18:53Z 满点）
**期间审查**:
- 用户 review proposal/design/tasks/specs 4 件套
- 可派 Oracle 进行 post-proposal 复评（如需更深入审查）
- 24h 满点后由主会话决定 ship-with-fixes 路径（per AGENTS.md Pattern #4）

---

## Related

- [ADR-0089 v1.3 amendment](https://github.com/chisuhua/HydraForge/blob/main/docs/adr/adr-0089-v1-3-amendment-loop-phase-shared-helpers.md) — 本 change 对应 ADR
- [ADR-0021 §3.2](https://github.com/chisuhua/HydraForge/blob/main/docs/adr/adr-0021-pdk-design.md#32-agent-loop-模板) — 修订对象
- [ADR-0067 L2/L3/L4 分层架构](https://github.com/chisuhua/HydraForge/blob/main/docs/adr/adr-0067-layered-plugin-architecture-split.md) — PDK 头依赖约束 P3
- [ADR-0041 PluginLoader 生命周期扩展](https://github.com/chisuahua/HydraForge/blob/main/docs/adr/adr-0041-pluginloader-lifecycle-extension.md) — ABI 边界
- [ADR-0087 Cloud Adapter Threading Model](https://github.com/chisuahua/HydraForge/blob/main/docs/adr/adr-0087-cloud-adapter-threading-model.md) — ForkJoinLoop 4-worker benchmark
- [ADR-0020 Thread Model Isolation](https://github.com/chisuahua/HydraForge/blob/main/docs/adr/adr-0020-thread-model-isolation.md) — DomainWorkerPool 模式
- [`.rddf/improvements/consolidate-loop-phases-to-shared-helpers.md`](https://github.com/chisuahua/HydraForge/blob/main/.rddf/improvements/consolidate-loop-phases-to-shared-helpers.md) — 5-segment improvement draft
- AGENTS.md Pattern #4 (SHIP-with-fixes + Oracle 介入)
- AGENTS.md Pattern #8 (OpenSpec Change pre-implementation dual-agent review)
- AGENTS.md §Reverse Indicator Rule (5 字段强制)
- AGENTS.md §FULL REGRESSION TEST FLOW (must_realllm 阶段 2 强制)
- `pdk/loop_agent/README.md` "双循环架构 (C1 决策矩阵)" 段（待改写）
- `tests/test_pdk_plan_execute.cpp` (5 cases, 零修改 - D2.inv.api)
- `tests/test_plan_execute_realllm.cpp` (3 cases must_realllm, 零修改 - D2.inv.api)
- `tests/test_pdk_macros.cpp` (DEFINE_AGENT 测试, 零修改 - D3)
- `pdk/g1_coding_assistant/src/g1_agent.cpp:132` (DEFINE_AGENT(React) 真实生产消费者, 零修改 - M3)

---

## Reverse Indicator (per AGENTS.md §Reverse Indicator Rule)

```
+ new_up: Loop phase 逻辑从"双轨 C++/DSL 平行实现" 收敛为"phase helper + 薄壳 + DSL 可选调用",
         消除测试/e2e coverage gap;
         must_realllm 10/10 PASS (含 test_plan_execute_realllm 3 cases);
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
  - 不涉及 ContextRequest schema 变更 (本 change 不改 pipeline schema)
  - 不涉及必须 [must_realllm] label 变化 (10 个 must_realllm binary 全部保留原 label + 范围)
```