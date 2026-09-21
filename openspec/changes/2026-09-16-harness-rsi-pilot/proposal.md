# Proposal: Harness-RSI Pilot —— 实验验证 Harness-RSI 价值

> **STATUS**: DRAFT (REVISION 2026-09-21 per Oracle bg_3672cb57 + Metis bg_1f291bc4 dual-agent review)
> **依赖**: C3 (h-d-m-transition-guard) ✅ ship + D8 (主题注册) ✅ ship
> **GO/NO-GO DECISION GATE**: pilot 完成后决定是否扩展 Model-RSI 方向
> **关联 ADR**: ADR-0084 (Mutation Governance, ✅ V1 ship), ADR-0061-13 (Distillation Output), ADR-0088 D4 (✅ — 显式取消 IHarnessRSI 三算子接口, 改用轻量函数)
> **追溯范围**: `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §四 C4 + §五 Sprint 36
> **优先级**: P2 (Wave 2.5, Sprint 36, Go/No-Go 决策)
> **估时**: 5d (修订后估时, per Oracle M2 + Metis DB1/DB2: 文档修正 + 接口扩展 + 实施)
> **rdd-workflow 阶段**: rdd-planner (本 change 当前阶段) → rdd-builder P0 (待 Oracle 2nd review APPROVE) → P1 plan → P2 execute

---

## Why (背景)

**YAGNI 原则 + Oracle 评审**:
- 不造重型调度框架，pilot 验证 Harness-RSI 价值前不投入
- pilot 结果决定后续是否需要更重的 Model-RSI 方向（Wave 3, ADR-0078）

**关键设计决策 — 拒绝 IHarnessRSI 接口** (per ADR-0088 D4 line 96-98):
- 显式取消 IDatasRsi / IHarnessRSI / IModelRSI 三算子接口
- C4 改为**轻量函数** `apply_harness_mutation(...)` — 零新接口类

---

## Oracle bg_3672cb57 + Metis bg_1f291bc4 关键纠偏 (2026-09-21, 14m 34s dual-agent)

**3 项 Critical 必须修正后才能进 rdd-builder P0**:

- **C1 [Critical] 消除 3 个幻影 API**:
  - `MutationGovernance::authorize()` → 实际是 `IMutationGovernor::propose(MutationContext) → MutationDecision` (imutation_governance.h:86, 8 字段 MutationContext 太重)
  - `IToolRegistry::unregister_tool_function()` → **不存在** (9 虚方法无 unregister; 仅 `has_tool` + `list_tools` + `register_tool_function`)
  - `config.system_prompt` → 实际是 `config.agent.system_prompt` (chat_session.h:123, AgentConfig struct 内字段)
- **C2 [Critical] 消除 core→PDK 反向依赖**: `src/evolution/` (agenticdsl_evolution 链接 agenticdsl_core) 不能 include `agenticdsl/pdk/chat_session.h` (chat_session.h:108 include `<core/engine.h>`, 引入 core→PDK→core 环). **修复**: 签名不取 `ChatConfig&`, 改取 `std::string& system_prompt` (prompt_delta 唯一触碰字段) + `std::vector<std::string>& tools`. 调用方传 `config.agent.system_prompt` + `config.agent.tools`
- **C3 [Critical] 重写 Case 3a 的 veto 机制**: `IMutationGovernor` 白名单作用于 `source_id`, 不存在按工具名 veto dangerous/trusted. **修复**: C4 用内部轻量 policy check `is_tool_allowed(meta, policy)` — 不调 `IMutationGovernor::propose` (DB2 选 1, Metis 推荐). Mock 简化, 不需要构造 8 字段 MutationContext

**3 项 Major 修正**:
- **M1 [Major] ADR-0084 审计配对**: `propose()` 成功必须配对 `commit()`. C4 决策: **不调 `IMutationGovernor::propose`** (per C3 选 1), 所以审计配对问题在 C4 scope 内不存在 (governance 不介入). Decision Record 注明此 trade-off.
- **M2 [Major] 签名扩展 + 返回值**: 引入 `MutationGateContext` 值类型封装 readiness 所需的 AttributionRecord + IEvaluator + IBudgetController + IInteractionBus. 返回类型 `Result<Genome,…>` 改 `Result<AppliedMutation,…>` (无 Genome, 无 IGenomeRegistry 依赖).
- **M3 [Major] Case 2 事件载荷断言**: 4/4 字段断言 (failed_conditions + attribution_verdict + eval_quality + budget_state, per ADR-0068 v2.2 line 253). 措辞: "mocked to return false" → "构造失败输入 (stub IEvaluator 返回 Quality::Poor + IBudgetController.exceeded()=true)".

**1 项 follow-up**: ADR-0088 D3 签名 drift (文档写 `(state, bus, registry, budget, baseline)`, header 实际 `(state, attribution, evaluator, budget)`) — 本 change 不修, 登记 follow-up.

---

## What Changes (修订版)

### 1. 轻量函数 `apply_harness_mutation` (修订签名 + 消除 core→PDK 依赖)

- **位置**: `src/evolution/harness_rsi.cpp` + `include/agenticdsl/evolution/harness_rsi.h`
- **修订签名** (per Oracle C2 消除 core→PDK):
  ```cpp
  struct MutationGateContext {
    const AttributionRecord* attribution;     // ADR-0086 v1.1
    IEvaluator* evaluator;                    // per transition_guard.h
    IBudgetController* budget;                // per ADR-0019 §1.4
    IInteractionBus* bus;                     // 发射 evolution.readiness.denied
    MutationGovernancePolicy policy;          // 轻量 policy (白名单 + 模式×等级, per C3)
  };

  struct GenomeMutations {
    std::string prompt_delta;                 // std::string version_hint 可选 idempotency (Metis 2.1)
    std::vector<std::string> tools_add;
    std::vector<std::string> tools_remove;
    std::optional<std::string> workflow_patch; // UnsupportedVariant 占位
  };

  struct AppliedMutation {
    std::vector<std::string> applied_prompts;
    std::vector<std::string> applied_tools_added;
    std::vector<std::string> applied_tools_removed;
  };

  Result<AppliedMutation, MutationError> apply_harness_mutation(
      const GenomeMutations& mutations,
      std::string& system_prompt,             // ← 改: 不取 ChatConfig& (C2)
      std::vector<std::string>& tools,         // ← 改: 同上 (C2)
      IToolRegistry& registry,                 // 必须 (per C1: 用 has_tool/list_tools 替代 unregister)
      const MutationGateContext& ctx);         // 封装 readiness + bus + policy
  ```

- **不引入新接口类** (per ADR-0088 D4)
- **支持 3 mutation 类型**: prompt_delta + tools_add + tools_remove; workflow_patch 返回 `UnsupportedVariant`
- **复用既有**: `IEvaluator` + `IBudgetController` (transition_guard.h:55-59 evaluate_readiness 实参) + `IInteractionBus::emit(EventBuilder(...).build())` (per D8)

### 2. 双重门禁 (修订顺序 + 内部 policy check)

- **门禁 1: `evaluate_readiness()`** (transition_guard.h:55-59, C3 ship)
  - 4 条件: Attribution Pass + 回归门 + 预算充足 + 无未控制混杂
  - 失败 → 发射 `evolution.readiness.denied` (D8 ship, 4 字段载荷) + 零状态变更 + 返回 `MutationError::NotReady{failed_conditions}`
- **门禁 2: 内部 `is_tool_allowed(meta, policy)`** (per Oracle C3 — 不调 IMutationGovernor::propose)
  - 简化: 白名单 + 模式×等级矩阵 (per ADR-0084 决策 1/6)
  - 拒绝 → 零状态变更 + 返回 `MutationError::GovernanceDenied{denial_reason}`
- **顺序**: readiness → policy (per Metis 3.5 + Oracle 1.4 验证; readiness 廉价, policy 需 evaluation_refs)

### 3. Mock 闭环 (3+1 case 修订)

- **Case 1 (prompt_delta apply, 场景 1)**: 输入 `prompt_delta = "Be concise."` → `apply_harness_mutation` → `REQUIRE(system_prompt.find("Be concise.") != npos)` (确定性, 不依赖 LLM)
- **Case 2 (readiness denied, 场景 2)**: 构造失败输入 (stub IEvaluator 返回 Quality::Poor + IBudgetController.exceeded()=true + AttributionRecord{verdict=NotAttempted}) → 返回 `MutationError::NotReady` + system_prompt 不变 + 事件载荷 4/4 字段断言 (per M3)
- **Case 3a (dangerous tool veto, 场景 3 deny)**: `MutationGovernancePolicy{denied_tools=["dangerous_tool"]}` → add dangerous_tool 失败 + `REQUIRE(registry.has_tool("dangerous_tool") == false)` + `REQUIRE(std::find(tools.begin(), tools.end(), "dangerous_tool") == tools.end())`
- **Case 3b (trusted tool add positive, 场景 3 positive)**: `MutationGovernancePolicy{}` (空 allow/deny lists) → add trusted_tool 成功 + `REQUIRE(registry.has_tool("trusted_tool") == true)` + `REQUIRE(std::find(tools.begin(), tools.end(), "trusted_tool") != tools.end())`

### 4. ~~Real LLM 1-turn~~ **删除** (per Metis 2.5 + DB4)

- **决策**: 删除 Case 4. `apply_harness_mutation` 签名无 LLM 调用路径. real LLM 验证推迟到 Wave 3 (ADR-0078 Model-RSI pilot).
- **Decision Record 注明**: "C4 scope 内无 LLM 调用路径; real LLM E2E 推迟到 Wave 3"

### 5. Go/No-Go Decision Record (修订 anti-bias 措施, per Metis 2.6 + Oracle 3.2)

- **位置**: `docs/audits/2026-XX-XX-harness-rsi-pilot-go-no-go.md`
- **格式模板** (实施前 Phase 2 写 skeleton):
  - 第一节: 观察到的失败/摩擦 + ctest 计数 + 原始 ctest 输出片段 (非转述) + 事件捕获 dump
  - 第二节: Oracle 独立复核结论 (派 Oracle 对原始证据复核, **非复核作者结论段**)
  - 第三节: Go/No-Go 结论
- **Go 判据 (4 must hold, 全部确定性)**:
  1. Case 1 PASS — `REQUIRE(system_prompt.find("Be concise.") != npos)`
  2. Case 2 PASS — system_prompt == initial + 事件载荷 4 字段完整
  3. Case 3a PASS — `REQUIRE(registry.has_tool("dangerous_tool") == false)` + tools vector 不含
  4. Case 3b PASS — `REQUIRE(registry.has_tool("trusted_tool") == true)` + tools vector 含
  5. ctest 全量 251 + 4 new = 255 tests, 零回归
- **No-Go 判据**: 任 1 of 4 mock case fail OR ctest regression → No-Go (YAGNI 有效决策, 不视作失败)

---

## Acceptance (修订版)

- [ ] **AC-1**: `apply_harness_mutation` 轻量函数 ship (修订签名, 6 参, 消除 core→PDK 依赖)
- [ ] **AC-2**: `IToolRegistry::unregister_tool_function` 添加 (per Metis DB1, 估时 +0.1d)
- [ ] **AC-3**: 双门禁集成完整 (evaluate_readiness + is_tool_allowed 内部 policy check)
- [ ] **AC-4**: Mock Case 1 (prompt_delta apply) PASS
- [ ] **AC-5**: Mock Case 2 (readiness denied) PASS — 4/4 事件载荷字段断言
- [ ] **AC-6**: Mock Case 3a (dangerous tool veto) PASS
- [ ] **AC-7**: Mock Case 3b (trusted tool add positive) PASS
- [ ] **AC-8**: ctest 全量 255 tests 零回归
- [ ] **AC-9**: Decision Record ship — 含原始 ctest 输出 + Oracle 独立复核结论 + Go/No-Go 结论

## Capabilities (MUST / MUST NOT)

### MUST
- **MUST** 复用 ADR-0088 D4 决策 — 不引入 IHarnessRSI / IModelRSI 接口
- **MUST** 修订签名消除 core→PDK 反向依赖 (per C2)
- **MUST** 用 `is_tool_allowed(meta, policy)` 内部 policy check, 不调 `IMutationGovernor::propose` (per C3 + Metis DB2)
- **MUST** 加 `IToolRegistry::unregister_tool_function` 虚方法 (per Metis DB1)
- **MUST** 失败路径零状态变更 (system_prompt + tools + registry 全部不动)
- **MUST** 事件载荷 4 字段断言 (per M3 + ADR-0068 v2.2)
- **MUST** Decision Record 含原始 ctest 输出 + Oracle 独立复核 (per Oracle 3.2)

### MUST NOT
- **MUST NOT** 引入 IHarnessRSI / IDatasRsi / IModelRSI 接口类 (`grep -rnE "class IHarnessRSI" src/ include/` = 0 hits)
- **MUST NOT** 调 `IMutationGovernor::propose` (MutationContext 8 字段太重, DB2 选 1)
- **MUST NOT** include `agenticdsl/pdk/chat_session.h` 从 `src/evolution/` (core→PDK 反向依赖, per C2)
- **MUST NOT** 实现 workflow_patch mutation 路径 (返回 UnsupportedVariant, Wave 3 再做)
- **MUST NOT** 调真实 LLM (Case 4 删除, real LLM 推迟 Wave 3)
- **MUST NOT** 用 LLM 响应差异作 Go 判据 (per Metis 3.1)

## Impact (影响范围, 修订)

| 文件 | 变更类型 | 行数估计 |
|------|---------|---------|
| `include/agenticdsl/evolution/harness_rsi.h` | 新增 | +50 行 (GenomeMutations + MutationGateContext + AppliedMutation + MutationError) |
| `src/evolution/harness_rsi.cpp` | 新增 | +180 行 (3 mutation 路径 + 双门禁 + EventBuilder 发射 + policy check) |
| `include/agenticdsl/contract/itool_registry.h` | 修改 | +5 行 (新增 unregister_tool_function 虚方法) |
| `src/common/tools/registry.h` + `.cpp` | 修改 | +15 行 (实现 unregister_tool_function) |
| `src/common/tools/secure_tool_registry.h` + `.cpp` | 修改 | +10 行 (委托) |
| `src/evolution/CMakeLists.txt` | 修改 | +5 行 (注册 harness_rsi.cpp) |
| `tests/test_harness_rsi_pilot.cpp` | 新增 | +150 行 (4 mock test cases) |
| `tests/CMakeLists.txt` | 修改 | +3 行 (注册 test_harness_rsi_pilot) |
| `docs/audits/2026-XX-XX-harness-rsi-pilot-go-no-go.md` | 新增 | +50 行 (Decision Record) |

**总估计**: +468 行 / -0 行 (新增 3 files, 改 5 files, **含 IToolRegistry 接口扩展 BREAKING 但向后兼容**)

## Non-goals (强化版)

- ❌ IHarnessRSI / IModelRSI 接口 (per ADR-0088 D4)
- ❌ Model-RSI 实际执行 (依赖 ADR-0078, pilot 验证后立项)
- ❌ 真实 LoRA 训练 / 多 Agent 协同进化 / Meta Co-Evolution
- ❌ workflow_patch mutation 路径实现 (Wave 3 后续)
- ❌ 真实 LLM 1-turn 验证 (Case 4 删除, 推迟 Wave 3)
- ❌ 调 `IMutationGovernor::propose` (8 字段 MutationContext 太重)
- ❌ 新框架依赖

## 估时分解 (修订后, per Oracle + Metis 共识)

| Phase | 任务 | 估时 |
|-------|------|------|
| Phase 1 | rdd-builder P0 (5-option approve) | 0.1d |
| Phase 2.4 | 修订 spec.md/tasks.md/proposal.md 应用本评审 6 项修正 | 0.3d |
| Phase 2.5 | 派 Oracle 2nd review 验证修正后 spec/tasks/proposal | 0.2d |
| Phase 3 | RED tests (4 cases + mock setup) | 0.5d |
| Phase 4.0 | IToolRegistry::unregister_tool_function 实施 (DB1) | 0.1d |
| Phase 4.1 | include/agenticdsl/evolution/harness_rsi.h 定义 | 0.3d |
| Phase 4.2 | src/evolution/harness_rsi.cpp body | 1.0d |
| Phase 4.3 | EventBuilder 实际发射代码 (transition_guard.cpp 集成) | 0.2d |
| Phase 5 | 全量验证 (ctest + adr_lint + docs_drift_audit + openspec validate) | 0.5d |
| Phase 6 | Oracle post-impl SHIP-with-fixes review | 0.3d |
| Phase 7 | Decision Record (含 Oracle 独立复核 30 min) | 0.5d |
| Phase 8 | Archive + Sync (master plan + active-status + Roadmap) | 0.2d |
| **总计** | | **4.2d** (估时上限 1 周, 实际 4.2d ≈ 5d 留 buffer) |

## 依赖

- **上游**: C3 (h-d-m-transition-guard) ✅ ship + D8 (主题注册) ✅ ship
- **下游** (Go 决策后): Wave 3 (ADR-0078 Model-RSI pilot) — 仅在 Go 路径触发, 含 Case 4 real LLM 验证

## rdd-workflow 阶段 (per ADR-0047/0048/0049)

- **当前**: rdd-planner 阶段 (本 change proposal/tasks/spec 待最终化)
- **下一步**: 派 Oracle 2nd review (per AGENTS.md 模式 #4 SHIP-with-fixes) → rdd-builder P0 → P1 plan → P2 execute
- **pre-impl 阶段**: 已派 dual-agent review (bg_3672cb57 + bg_1f291bc4, 2026-09-21)
- **post-impl 阶段**: 必须派 Oracle SHIP-with-fixes review per AGENTS.md 模式 #4

## 关联文档

- `docs/adr/adr-0088-h-d-m-transition-guard.md` D4 line 96-98 (IHarnessRSI 取消决策)
- `docs/adr/adr-0068-event-emission-contract.md` v2.2 (D8 ship, evolution.* topic schema line 253)
- `docs/adr/adr-0084-mutation-governance-contract.md` (V1 ship)
- `include/agenticdsl/contract/itool_registry.h` (9 虚方法, 无 unregister)
- `include/agenticdsl/contract/imutation_governance.h` (propose/commit/revert, 8 字段 MutationContext)
- `pdk/chat_session/include/agenticdsl/pdk/chat_session.h:115-127` (AgentConfig struct, system_prompt 字段 line 123)
- `include/agenticdsl/evolution/transition_guard.h:55-59` (evaluate_readiness 实参)
- Oracle bg_3672cb57 (2026-09-21, 7m 16s) — 实施路径审查 + 3 Critical + 3 Major + 3 RED FLAGS
- Metis bg_1f291bc4 (2026-09-21, 7m 18s) — 意图 + 5 DEAL-BREAKER + 6 必读文件
- AGENTS.md 模式 #4 (SHIP-with-fixes) + 模式 #8 (pre-impl dual-agent review)
