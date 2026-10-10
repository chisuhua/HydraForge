# Spec: loop-phases-shared-helpers

**Change ID**: `consolidate-loop-phases-to-shared-helpers`
**Status**: 🔍 Proposed (24h cooling-off 起点 = OpenSpec change 立项完成时, ~2026-10-10T18:53Z 满点)
**关联**: [proposal.md](../../proposal.md) · [design.md](../../design.md) · [tasks.md](../../tasks.md) · [ADR-0089 v1.3](../../../../docs/adr/adr-0089-v1-3-amendment-loop-phase-shared-helpers.md)

> **本 spec 定义**: Phase helper 共享架构的需求契约。新增 capability `loop-phases-shared-helpers`, 修订 capability `loop-execute-only` (新增 `loop/run_plan` + `loop/run_verify` 工具), 其他 capability 零变化。

---

## ADDED Requirements

### Requirement: loop-phases-shared-helpers

The system MUST provide three phase helper functions (`run_plan_phase` / `run_execute_phase` / `run_verify_phase`) in `include/agenticdsl/pdk/agent_loops/loop_phases.h` as the single source of truth for phase logic. C++ loop classes (`PlanExecuteLoop` / `ReactLoop`) MUST delegate to these helpers internally. The helpers MUST NOT hold retry state, MUST NOT hold engine ownership, MUST NOT emit events.

#### Scenario: helper.plan_phase.success

- **WHEN** 调用方传入 `ILLMProvider&` (LLM 返回非空 DSL 片段) + 有效 `goal` + `LayeredContext`
- **THEN** `loop_phases::run_plan_phase()` 返回 `std::optional<std::string>` 含值, 值是 LLM 生成的 DSL 片段

#### Scenario: helper.plan_phase.empty_response

- **WHEN** 调用方传入 `ILLMProvider&` (LLM 返回空 response) + 有效 `goal` + `LayeredContext`
- **THEN** `loop_phases::run_plan_phase()` 返回 `std::optional<std::string>` 为 `std::nullopt`

#### Scenario: helper.plan_phase.model_clear_invariant

- **WHEN** 调用方传入任意 `ILLMProvider&` (无 model 概念, model 由 adapter/factory 持有)
- **THEN** `loop_phases::run_plan_phase()` MUST 显式调 `req.params.model.clear()` 在调 `llm->generate(req, token)` **之前**
- **AND** `req.params.model.clear()` 调用 MUST 带完整 "NOT redundant" 注释解释为何非冗余 (per `openspec/changes/fix-generation-request-model-default/` 修复链)
- **AND** 注释内容 MUST 从 `plan_execute_loop.h:225/289` 原注释段落完整同步, 不得删减

#### Scenario: helper.execute_phase.success

- **WHEN** 调用方传入 `DSLEngine&` (engine 持有者, 跨 retry 累积用) + 有效 generated DSL 字符串
- **THEN** `loop_phases::run_execute_phase()` 返回 `true`, engine 内部 `continue_with_generated_dsl(generated_dsl)` 调用成功, 累积子图

#### Scenario: helper.execute_phase.parse_failure

- **WHEN** 调用方传入 `DSLEngine&` + 非 DSL 字符串 (解析失败)
- **THEN** `loop_phases::run_execute_phase()` 返回 `false`, `execute_error_out` 填充异常消息 (非 `std::nullopt`), engine 状态保持 (不破坏 retry 累积)

#### Scenario: helper.verify_phase.success_yes

- **WHEN** 调用方传入 `ILLMProvider&` (LLM 响应含 "yes", 大小写不敏感) + 有效 `goal` + result_data
- **THEN** `loop_phases::run_verify_phase()` 返回 `true`

#### Scenario: helper.verify_phase.failure_no_or_empty

- **WHEN** 调用方传入 `ILLMProvider&` (LLM 响应含 "no", 或响应空, 或 LLM 调用失败)
- **THEN** `loop_phases::run_verify_phase()` 返回 `false`

#### Scenario: helper.verify_phase.model_clear_invariant

- **WHEN** 调用方传入任意 `ILLMProvider&`
- **THEN** `loop_phases::run_verify_phase()` MUST 显式调 `req.params.model.clear()` 在调 `llm->generate(req, token)` **之前** (与 plan_phase 同一不变量)
- **AND** 注释内容与 plan_phase 同步 (DRY), 但函数语义独立

#### Scenario: helper.no_retry_semantics

- **WHEN** 任何 helper 函数被调用 (plan / verify / execute)
- **THEN** helper MUST NOT 持 `while(true)` 循环, MUST NOT 计数 `retries_used`, MUST NOT 持状态机 (`state_` 字段)
- **AND** retry 编排 MUST 由调用方 (PlanExecuteLoop) 负责, helper 是原子操作

#### Scenario: helper.no_engine_ownership

- **WHEN** `loop_phases::run_execute_phase()` 被调用
- **THEN** helper MUST 接受 `DSLEngine&` (reference), MUST NOT 获取 `unique_ptr` 所有权
- **AND** engine 生命周期 MUST 由调用方 (PlanExecuteLoop 成员 `engine_`) 管理, helper MUST NOT 构造 / 析构 engine

#### Scenario: helper.no_bus_dependency

- **WHEN** 任何 helper 函数被调用
- **THEN** helper MUST NOT 持 `IInteractionBus&`, MUST NOT 发射 `loop.turn.*` 事件 (per C1 工具规则 D6 bus_ptr 边界)
- **AND** 事件发射 MUST 由 caller 统一负责 (per ADR-0068 附录 A 语义单点)

---

### Requirement: loop-run-plan-tool-execution (new optional tool)

The system MUST register `loop/run_plan` and `loop/run_verify` tool functions in `pdk/loop_agent/src/pdk_entry.cpp`. The tools MUST call into `loop_phases::run_plan_phase` / `loop_phases::run_verify_phase` respectively. Existing DSL files (`lib/loop/*.agent.md`) MUST NOT be modified by this change.

#### Scenario: tool.optional_dsl_adoption

- **WHEN** `loop/run_plan` 工具在 `pdk_entry.cpp` 注册成功
- **THEN** 现有 `lib/loop/react.agent.md` / `plan_execute.agent.md` / `fork_join.agent.md` MUST 零修改, DSL 文件继续走原有 path (DSL 的 think/react 节点或 fork/join 节点)
- **AND** 新工具只是可选能力, ChatSession 生产路径行为不变

#### Scenario: tool.invocation_contract

- **WHEN** DSL (或 caller) 调 `loop/run_plan(args)` with `goal` 参数
- **THEN** 工具函数调用 `loop_phases::run_plan_phase(*tls_parent_provider, goal, ctx, std::stop_token{})`, 返回 JSON:
  - 成功: `{"ok": true, "success": true, "error_code": null, "plan": <generated DSL string>}`
  - 失败: `{"ok": false, "success": false, "error_code": "Unknown" 或 "InvalidParams", "error": <error message>}`

#### Scenario: tool.missing_provider

- **WHEN** DSL 调 `loop/run_plan` 但 `tls_parent_provider` 未设 (per C1 工具 mock fallback 路径)
- **THEN** 工具函数返回 `{"ok": false, "success": false, "error_code": "Unknown", "error": "Parent LLM provider not set..."}` (per pdk_entry.cpp line 692-719 mock fallback 模式)

#### Scenario: tool.approval_policy

- **WHEN** `loop/run_plan` 工具注册到 ToolRegistry
- **THEN** `ApprovalPolicy::requires_approval_in_agent = true` (与现有 `loop/run` 一致)
- **AND** `allowed_layers = {LayerProfile::Workflow}` (与现有 loop 工具一致)

#### Scenario: tool.no_event_emission

- **WHEN** `loop/run_plan` 工具被调用 (成功或失败)
- **THEN** 工具函数 MUST NOT 发射 `loop.turn.start` / `loop.decision` / `loop.turn.end` 事件 (per C1 工具规则 D6)
- **AND** `loop.turn.*` 事件仍由 caller (ChatSession::chat 或 loop/run wrapper) 统一发射

---

### Requirement: plan-execute-loop-thin-shell (C++ 类薄壳契约)

`PlanExecuteLoop` MUST internally delegate to `loop_phases::run_*` helpers while preserving its public API. The class MUST retain retry orchestration (`while(true)` + `retries_used`) and engine ownership (`engine_` member). The public API MUST remain byte-identical to pre-change.

#### Scenario: cpp.api_zero_change

- **WHEN** 任何 caller 构造 `PlanExecuteLoop(engine, bus, max_retries=3)` 并调 `run(goal, ctx, token)`
- **THEN** 公开 API 签名 (`run` 参数列表 + 返回 `LoopResult` 结构) MUST 零变化
- **AND** `LoopResult` 字段 (`success` / `message` / `retries_used` / `failed_phase` / `final_context`) MUST 零变化
- **AND** `PlanExecuteLoop::State` 枚举 (`Planning` / `Executing` / `Verifying` / `Done` / `Retry`) MUST 零变化
- **AND** `state()` 成员函数 MUST 零变化
- **AND** `tests/test_pdk_plan_execute.cpp` 5 cases + `tests/test_plan_execute_restart.cpp` 3 cases + `tests/test_plan_execute_realllm.cpp` 3 cases (must_realllm) MUST 零修改

#### Scenario: cpp.message_strings_unchanged

- **WHEN** PlanExecuteLoop::run() 完成或失败
- **THEN** `result.message` 字符串字面量 MUST 与 v0.2 (pre-change) 逐字节相同:
  - `"PlanExecuteLoop: completed successfully"` (verify success)
  - `"PlanExecuteLoop: plan phase failed (empty LLM response)"` (plan 空响应)
  - `"PlanExecuteLoop: execute phase failed: " + exec_err.value_or("unknown")` (execute 失败)
  - `"PlanExecuteLoop: verify failed after " + std::to_string(retries_used) + " retries"` (verify 失败 max_retries 用完)
- **AND** `tests/test_plan_execute_realllm.cpp:153` 的 `result.message == "PlanExecuteLoop: completed successfully"` 断言不退化

#### Scenario: cpp.retry_state_machine_preserved

- **WHEN** PlanExecuteLoop::run() 内部 verify 失败
- **THEN** `while(true)` retry 循环 + `state_ = State::Retry` + `result.retries_used++` MUST 全部在类内 (不外溢到 helper)
- **AND** helper (`loop_phases::run_*`) MUST NOT 持 retry 状态, MUST NOT 计数 retries_used
- **AND** `result.retries_used <= max_retries_` 不变量 MUST 保持 (per `tests/test_plan_execute_realllm.cpp:110` 断言)

#### Scenario: cpp.engine_ownership_preserved

- **WHEN** PlanExecuteLoop 跨 retry 调用 helper
- **THEN** `engine_` (unique_ptr<DSLEngine>) MUST 仍是类成员, MUST NOT 外溢到 helper
- **AND** helper `run_execute_phase(engine&)` MUST 接引用而非 unique_ptr (per D2.inv.engine, Oracle C2 状态归属)
- **AND** 跨 retry `continue_with_generated_dsl()` 调用 MUST 同 engine, 子图累积 MUST 保留

#### Scenario: cpp.test_files_zero_modification

- **WHEN** PlanExecuteLoop 委托 helper 后
- **THEN** 以下测试文件 MUST 零修改 (D2.inv.api):
  - `tests/test_pdk_plan_execute.cpp` (5 cases, mock 状态机)
  - `tests/test_plan_execute_restart.cpp` (3 cases, retry 状态机)
  - `tests/test_plan_execute_realllm.cpp` (3 cases, must_realllm 真实 LLM)
  - `examples/pdk_chat_demo/tests/test_plan_execute_loop_integration.cpp` (1 case)

---

### Requirement: react-loop-thin-shell (C++ 类薄壳契约)

`ReactLoop` MUST internally delegate to `loop_phases::run_plan_phase` while preserving its public API. `agent_macros.h:52` MUST add an ADR-0089 reference comment but LoopDispatcher specializations MUST remain code-identical.

#### Scenario: react.api_zero_change

- **WHEN** 任何 caller 调 `ReactLoop::run_once()` 或经 `loop/decide_react` 工具调用
- **THEN** ReactLoop 公开 API 签名 MUST 零变化, `state()` / `result.message` 字段不变
- **AND** `tests/test_pdk_macros.cpp` DEFINE_AGENT(React) 测试 MUST 零修改
- **AND** `pdk/g1_coding_assistant/src/g1_agent.cpp:132` 的 `DEFINE_AGENT(CodingAssistant, AgentLoopType::React)` 编译期路径 MUST 零变化 (Oracle M3 G1 真实生产消费者)

#### Scenario: react.macro_comment_added

- **WHEN** `agent_macros.h:52` 加 ADR-0089 注释
- **THEN** LoopDispatcher 3 specialization (React / PlanExecute / ForkJoin) MUST 零代码变化, 仅加 1 行注释
- **AND** 注释文本 MUST include "ADR-0089 v1.3 amendment (2026-10-09)" 与 "委托 shared phase helpers (per ADR-0021 §3.2 amendment)" 字样

---

### Requirement: fork-join-loop-semantics-unchanged (用户决策 D4)

`ForkJoinLoop` MUST NOT be changed by this change. DomainWorkerPool 4-worker + InMemoryBus event-driven concurrency MUST remain byte-identical to pre-change. `lib/loop/fork_join.agent.md` MUST NOT be modified.

#### Scenario: forkjoin.zero_code_change

- **WHEN** 本 change 实施 (5 atomic commits)
- **THEN** `include/agenticdsl/pdk/agent_loops/fork_join_loop.h/.cpp` MUST 零修改 (不在 commit 1-5 改动范围)
- **AND** `DomainWorkerPool` 4-worker + `InMemoryBus` `domain.task.*` 事件同步 + 默认 handler echo args + fail-fast 行为 MUST 零变化 (`fork_join_loop.h:176-293` 完整保留)
- **AND** `lib/loop/fork_join.agent.md` MUST 零修改, DSL 路径 (fork node + `loop/process_task`) MUST 零变化

#### Scenario: forkjoin.test_files_zero_modification

- **WHEN** ForkJoinLoop 零修改
- **THEN** 以下测试文件 MUST 零修改:
  - `tests/test_pdk_fork_join.cpp` (5 cases, mock 并发)
  - `examples/pdk_chat_demo/tests/test_fork_join_loop_integration.cpp` (1 case)

#### Scenario: forkjoin.benchmark_unchanged

- **WHEN** `tests/test_adr_0087_step5_1_benchmark` 复跑
- **THEN** 4-worker 3.3× 加速数据 MUST NOT 退化 (与 pre-change baseline 偏差 < 5%)
- **AND** benchmark 直接使用 `DomainWorkerPool` + provider, 不引用 loop 类, 不受 helper 抽取影响

---

### Requirement: g1-coding-assistant-binary-unchanged (G1 ABI 兼容)

G1 binary MUST compile successfully after the change. `DEFINE_AGENT(CodingAssistant, AgentLoopType::React)` in `pdk/g1_coding_assistant/src/g1_agent.cpp:132` MUST remain a valid compile-time reference to `ReactLoop`.

#### Scenario: g1.compile_success

- **WHEN** `cmake --build build --target g1_coding_assistant -j$(nproc)` 执行
- **THEN** exit 0, `pdk/g1_coding_assistant/libG1CodingAssistant.so` 重新生成, 不报 `ReactLoop` undefined reference
- **AND** `DEFINE_AGENT(CodingAssistant, AgentLoopType::React)` 编译期路径 MUST 零变化

#### Scenario: g1.symbols_present

- **WHEN** `nm build/pdk/g1_coding_assistant/libG1CodingAssistant.so | grep -E "ReactLoop|DEFINE_AGENT"` 执行
- **THEN** symbols 命中 (ReactLoop 仍是实类, ABI version=2 不变 per ADR-0041)
- **AND** LoopDispatcher specialization 符号完整, 宏展开与 pre-change 字节级相同

---

### Requirement: pdk-loop-agent-binary-with-new-tools (Phase 3 工具注册)

`libLoopAgent.so` MUST compile successfully and export the new tool symbols after the change.

#### Scenario: pdk.compile_success

- **WHEN** `cmake --build build --target loop_agent -j$(nproc)` 执行
- **THEN** exit 0, `pdk/loop_agent/libLoopAgent.so` 重新生成, MUST 含 `loop/run_plan` + `loop/run_verify` 注册

#### Scenario: pdk.symbols_present

- **WHEN** `nm build/pdk/loop_agent/libLoopAgent.so | grep -E "loop/run_plan|loop/run_verify"` 执行
- **THEN** symbols 命中, ABI version=2 不变

#### Scenario: pdk.dsl_optional_no_force

- **WHEN** `loop/run_plan` + `loop/run_verify` 注册成功
- **THEN** 现有 `lib/loop/{react,plan_execute,fork_join}.agent.md` DSL 文件 MUST 零修改, 仍走原路径
- **AND** 新工具只是能力扩展, ChatSession 生产路径行为 MUST 零变化
- **AND** `tests/test_loop_agent_plugin.cpp` 22 cases + 后续追加 e2e cases 零修改 (若不调用新工具)

---

## MODIFIED Requirements

### Requirement: agent-macro-defs (修订)

`include/agenticdsl/pdk/agent_macros.h:52` MUST add an ADR-0089 reference comment. The LoopDispatcher 3 specializations MUST remain code-identical (no template changes, no type alias changes).

#### Scenario: agent_macro.comment_only

- **WHEN** PlanExecuteLoop / ReactLoop / ForkJoinLoop 公开 API 零变化
- **THEN** `agent_macros.h:52` MUST 加 1 行 ADR 注释, LoopDispatcher 3 specialization MUST 零代码变化
- **AND** `DEFINE_AGENT(name, loop_type)` 宏展开 MUST 与 pre-change 字节级相同 (除注释)
- **AND** G1 + test_pdk_macros 编译期路径 MUST 零变化

#### Scenario: agent_macro.adr_pointer

- **WHEN** `docs/adr/adr-0021-pdk-design.md` §3.2 末尾加 "实施注记 (2026-10-09)" 段 + 指回 ADR-0089 链接
- **THEN** ADR-0021 §3.2 主文档 MUST 明确指引读者: 薄壳委托 helper 是 ADR-0089 v1.3 amendment 引入, 与原 V0.2 描述协同不冲突
- **AND** `python3 tools/adr_lint.py` MUST 仍 "✓ 所有 ADR 通过 lint 检查"

---

### Requirement: pdk-loop-agent-readme (修订)

`pdk/loop_agent/README.md` "双循环架构 (C1 决策矩阵)" 段 MUST be rewritten to describe the post-change "phase helper + thin shell + DSL optional" architecture.

#### Scenario: pdk-readme.dual_loop_to_shared_helpers

- **WHEN** 本 change 实施完成
- **THEN** `pdk/loop_agent/README.md` "双循环架构" 段 MUST 从 "C1 决策矩阵" 描述改写为:
  - 删除 "统一重构触发条件: 任一循环实现出现第 3 个消费者" 段 (已满足 + 已重构)
  - 新增 "Phase helper 共享" 段 (描述 `loop_phases.h` 抽取 + 公开 API 零变化)
  - 新增 "DSL 可选调用" 段 (描述 `loop/run_plan` / `loop/run_verify` 工具)

---

## REMOVED Requirements

**无** — 本 change 不删除任何 Requirement, 所有修订都是 ADDITIVE (新增 capability) + MODIFY (注释 + README 段改写), 无破坏性变更。

---

## Cross-Cutting Invariants (per ADR-0089 v1.3)

- **D1.inv.model**: `req.params.model.clear()` MUST 在 helper 内保留, 注释解释 (Oracle M1 修复链延续) — per `helper.plan_phase.model_clear_invariant` + `helper.verify_phase.model_clear_invariant`
- **D2.inv.api**: 3 个 C++ 类公开 API MUST 零变化 (G1 + 6 个测试 binary 零修改) — per `cpp.api_zero_change` + `react.api_zero_change` + `cpp.test_files_zero_modification` + `forkjoin.test_files_zero_modification`
- **D2.inv.retry**: retry 编排 (`while(true)` + `retries_used`) MUST 保留在 `PlanExecuteLoop::run()` 内, helper MUST NOT retry 语义 — per `cpp.retry_state_machine_preserved` + `helper.no_retry_semantics`
- **D2.inv.engine**: `engine_` 成员 MUST 保留在类内, 跨 retry 累积 `continue_with_generated_dsl` (Oracle C2) — per `cpp.engine_ownership_preserved` + `helper.no_engine_ownership`
- **D4.inv.forkjoin**: ForkJoinLoop `DomainWorkerPool` 4-worker 语义 MUST NOT 变化, ADR-0087 benchmark MUST NOT 退化 — per `forkjoin.zero_code_change` + `forkjoin.benchmark_unchanged`
- **D5.inv.optional**: 新工具 `loop/run_plan` / `loop/run_verify` MUST 可选, DSL 文件 MUST NOT 强制改写 — per `tool.optional_dsl_adoption` + `pdk.dsl_optional_no_force`

---

## Test Coverage (must_realllm + mock + 真 LLM)

### Mock 测试 (CI 必过, 快速)

- `tests/test_loop_phases.cpp` (Phase 0 新增, 8 cases: plan/verify/execute 各 2-3 cases)
- `tests/test_pdk_plan_execute.cpp` (5 cases, 零修改)
- `tests/test_pdk_fork_join.cpp` (5 cases, 零修改)
- `tests/test_plan_execute_restart.cpp` (3 cases, 零修改)
- `tests/test_pdk_macros.cpp` (1+ cases DEFINE_AGENT, 零修改)
- `examples/pdk_chat_demo/tests/test_plan_execute_loop_integration.cpp` (1 case, 零修改)
- `examples/pdk_chat_demo/tests/test_fork_join_loop_integration.cpp` (1 case, 零修改)

### must_realllm 真实 LLM 测试 (CI skip + 本地 DEEPSEEK_API_KEY 必过)

- `tests/test_plan_execute_realllm.cpp` (3 cases, 零修改) — 核心回归门
- `examples/pdk_chat_demo/tests/test_e2e_real_llm.cpp` (ChatSession case) — DSL 路径回归门

### MUST PASS 的契约断言 (per AGENTS.md Pattern #3)

- `helper.plan_phase.model_clear_invariant` — `req.params.model.empty()` 用 Recording Provider 验证
- `cpp.message_strings_unchanged` — `result.message == "PlanExecuteLoop: completed successfully"` 字面量不变
- `cpp.retry_state_machine_preserved` — `result.retries_used <= max_retries` 不变量
- `forkjoin.benchmark_unchanged` — 4-worker 3.3× 加速不退化

---

## References

- [ADR-0089 v1.3 amendment](../../../../docs/adr/adr-0089-v1-3-amendment-loop-phase-shared-helpers.md)
- [ADR-0021 §3.2](../../../../docs/adr/adr-0021-pdk-design.md#32-agent-loop-模板) — 修订对象
- [ADR-0041 PluginLoader 生命周期扩展 + ABI](../../../../docs/adr/adr-0041-pluginloader-lifecycle-extension.md)
- [ADR-0067 L2/L3/L4 分层架构](../../../../docs/adr/adr-0067-layered-plugin-architecture-split.md)
- [ADR-0087 Cloud Adapter Threading Model (4-worker benchmark)](../../../../docs/adr/adr-0087-cloud-adapter-threading-model.md)
- [ADR-0080 v1.2 amendment D10](../../../../docs/adr/adr-0080-v1-2-amendment-d10-decouple.md) — 同模式 (薄壳 + 共享 + DSL 可选调用) 先例
- Metis session `ses_edfe8b94effeLHZtjyKMyWFTXO` — Intent + ambiguity
- Oracle session `ses_edfe8b7daffeysJHJPj9PaG5SZ` — Architecture + physical feasibility
- AGENTS.md Pattern #4 (SHIP-with-fixes) + Pattern #8 (dual-agent review)
- AGENTS.md §Reverse Indicator Rule (5 字段强制)
- AGENTS.md §FULL REGRESSION TEST FLOW (must_realllm 阶段 2)
- [proposal.md](../../proposal.md) · [design.md](../../design.md) · [tasks.md](../../tasks.md)