# pdk-chat-demo-evolution-reference-example — Tasks

> **Status**: 🔍 Proposed Tasks (PLACEHOLDER 待 rdd-builder 实施阶段细化)
> **关联 Proposal**: [`./proposal.md`](./proposal.md)
> **关联 Design**: [`./design.md`](./design.md)
> **关联 Spec**: [`./specs/pdk-chat-demo-evolution/spec.md`](./specs/pdk-chat-demo-evolution/spec.md)

---

## Task Groups (TDD 5 步结构, per AGENTS.md 模式 + Oracle dual-review post-impl)

每组遵循 RED-GREEN-REFACTOR 5 步 (per AGENTS.md 模式):
1. **RED**: 写 failing test (用 mock LLM / capture-mode=None, exit non-zero 验证)
2. **GREEN**: minimal impl (复用现有 5-tier gate + ChatConfig + IGenomeRegistry, 不引新 contract)
3. **REFACTOR**: 公共 helper 抽 + schema 稳定化
4. **AUDIT**: Oracle dual-agent review (Metis 路径完整性 + Oracle 物理可行性)
5. **ARCHIVE**: 完整 4-file integrity per AGENTS.md Day 5 lesson

**Pre-T1 任务 (P0 修复文档级, 必须先完成)**:

#### T0-1 [GREEN] ADR-0068 Appendix A v2.4 amendment (P0-5 + P0'-2 fix)
- [ ] `docs/adr/adr-0068-event-emission-contract.md` Appendix A 表格新增 4 行:
  - `mutation_metric_rejected` (owner=`pdk_chat_demo_evolution`, payload `context_id` + `task_class_preview` + `reason`) — R9.2 prefix-rejection 触发事件 (per P0'-1: prefix check 优先级先于闭枚举校验)
  - `turn_input_network_keyword_rejected` (owner=`pdk_chat_demo_evolution`, payload `context_id` + `turn_input_preview` + `keyword`) — R9.3 降级 keyword-rejection 触发事件
  - `hidden_context_accepted_info` (owner=`pdk_chat_demo_evolution`, payload `context_id` + `task_class` + `is_hidden` + `bucket=hidden`) — R13.4 is_hidden=true 接受进 hidden 桶 (per P0 is_hidden 语义重定位, 不再拒绝)
  - **`hint_containment_rejected`** (owner=`pdk_chat_demo_evolution`, payload `context_id` + `turn_input_preview` (first 100 chars) + `matched_pattern`) — **R9.1 parser-side hint detection 触发事件 (P0'-2 fix, replaces vacuous "result.find(hint)" assertion)**; payload 字段锁定以保证静态契约守卫
  - `*` 若 R8.2 `failure_event` 事件未注册, 同次补登 (owner=`pdk_chat_demo_evolution`, payload `failure_event` + `rule_id` + `rule_shipped_commit` + `reproduce_in_new_task_demo` + `context_id`)
- [ ] ADR 头部追加 "Appendix A v2.4 amendment (2026-09-23, pdk-chat-demo-evolution-reference-example): 新增 4 个 pdk_chat_demo_evolution 主题 (含 P0'-2 新增 hint_containment_rejected)..."
- [ ] 验证 `grep -c "^- |" docs/adr/adr-0068-event-emission-contract.md` 行数 +N (新行数)

#### T0-2 [GREEN] harness-architecture-2026-09.md §3.1 API 表面校正 (P0-1 SoT sync)
- [ ] Line 128-133: ChatSession ctor 4-arg → 11-arg 真实签名 (复制 `chat_session.h:210-230` block)
- [ ] Line 136-140: ChatConfig::override_* 5 method → **2 method** (override_provider + override_system_prompt), 删 4 行 (`override_tools` / `override_budget` / `override_model_routing` / `override_prompt_prefix` 注明 "设计笔误, 实不存在, L2 改走 AgentConfig 字段直接赋值")
- [ ] Line 330: apply_harness_mutation 6 字段签名 → **5 参自由函数** (per harness_rsi.h:73-78 真实签名); 注明"MutationGateChain 是 design 笔误, 实为自由函数 + MutationGateContext struct"
- [ ] Line 451: 表中 `override_tools` 引用 → 注明"删除, 不存在该方法; tools 维度走 AgentConfig.tools 直接字段赋值"
- [ ] Line 490: `ChatConfig.override_* 5 method` → `ChatConfig.override_provider + override_system_prompt (2 method)`
- [ ] 验证 cross-doc consistency: harness-arch §3.1 与 pdk_chat_demo/main.cpp:484-492 + chat_session.h:210-230 真实代码一致

#### T0-3 [GREEN] spec.md 改 OpenSpec delta 格式 (P0-2 fix, ship gate 必须)
- [ ] spec.md L19-R13 段全部 `### R1:` 改 `### Requirement: <name>`
- [ ] spec.md `#### 验收场景` 改 `#### Scenario: S1 ... - **WHEN** ... - **THEN** ...` (OpenSpec 契约, WHEN/THEN 结构)
- [ ] spec.md 顶部加 `## ADDED Requirements` wrapper (per OpenSpec delta 契约, 新增 spec 必走 ADDED)
- [ ] 删除 spec.md L372-399 重复"关联文档"段 (旧 R1-R7-only 引用, drift)
- [ ] 验证 `openspec validate pdk-chat-demo-evolution-reference-example --strict` PASS

---

### T1: project skeleton (CMakeLists.txt + README + main.cpp stub)

**估时**: 0.5 天
**依赖**: 无

#### T1.1 [RED] CMakeLists.txt compile fail (no source files yet)
- [ ] 写 `examples/pdk_chat_demo_evolution/CMakeLists.txt` stub (target + test, **含 `LABELS "l2-evolution"`** per P0-7 fix)
- [ ] 在**根 `CMakeLists.txt` 的 `examples/` 分段** (line 254 `add_subdirectory(examples/pdk_chat_demo)` 后) 加 `add_subdirectory(examples/pdk_chat_demo_evolution)` 一行 (per P0-8 fix: `examples/CMakeLists.txt` 不存在, examples 注册全部走根 CMakeLists.txt)
- [ ] 验证 `cmake -S . -B build -DAGENTICDSL_BUILD_EXAMPLES=ON` exit non-zero (no source yet)

#### T1.2 [GREEN] minimal main.cpp + skeleton files
- [ ] 写 `main.cpp` stub (5 lines: return 0)
- [ ] 写 `evolution_session.{h,cpp}` empty
- [ ] 写 `evolution_tracer.{h,cpp}` empty
- [ ] 验证 `cmake build` exit 0 + main binary 编译

#### T1.3 [REFACTOR] README + run_evolution_demo.sh
- [ ] 写 `README.md` (端到端使用说明 + 验收命令 + R8/R9/R13 flag 说明 per P1-6 fix)
- [ ] 写 `run_evolution_demo.sh` (一键 mock/real-LLM 双模式 + 自动接受 `--context-file` 参数)
- [ ] ~~写 `fixtures/golden_inputs.jsonl` (7 个 reference turn 输入)~~ — **REMOVED per P1-5 fix 2026-09-23**: R13 前遗留, 与 R13.2 零 hardcode 直接冲突; L2 唯一输入源 = `--context-file` (SHIPPED reference 已在 T6.10 创建 `examples/pdk_chat_demo_evolution/fixtures/contexts/{code,research,debug}-class-context.jsonl`)

#### T1.4 [AUDIT] Metis: 路径完整性
- [ ] Metis review: example/pdk_chat_demo 主体无变化 + 新独立 binary 边界清晰

#### T1.5 [ARCHIVE] commit T1 (worktree)
- [ ] commit: `feat(example): pdk_chat_demo_evolution project skeleton`

---

### T2: 6-phase demo TDD (核心 happy path)

**估时**: 1.5 天
**依赖**: T1
**目标**: mock mode 跑通 6 段端到端, 5-tier gate 全 pass

#### T2.1 [RED] test_evolution_session_mutation (Case 1)
- [ ] 写 `tests/test_evolution_session_mutation.cpp` (5 cases, **CMake LABELS "l2-evolution"**):
  - Case 1.1: apply_harness_mutation(prompt_delta) → 期望 5-tier gate 全 PASS (G0/G1/G2/G2.5/G3)
  - Case 1.2: apply_harness_mutation(tools_add) → 期望 SemanticLockedTools 空时不挡
  - Case 1.3: apply_harness_mutation(无效 prompt_delta) → 期望 G0 fail (语法)
  - Case 1.4: apply_harness_mutation(denied_tools) → 期望 G1 fail (policy)
  - Case 1.5: apply_harness_mutation(workflow_patch) → 期望 `Result::failure(MutationError::UnsupportedVariant)` (per harness-rsi-pilot/spec.md Scenario "workflow_patch NOT supported"; P0-1 fix)
- [ ] 验证 `ctest -R test_evolution_session_mutation` exit non-zero (RED)

#### T2.2 [GREEN] EvolutionSession::phase3_mutation impl
- [ ] 写 `evolution_session.cpp::phase3_mutation`
- [ ] 调用**现成 5 参自由函数** `apply_harness_mutation(GenomeMutations, string& system_prompt, vector<string>& tools, IToolRegistry&, const MutationGateContext&)` per `harness_rsi.h:73-78` (per P0-1 fix: **不是 "MutationGateChain 类"**, grep 0 命中)
- [ ] 返回 `AppliedMutation` + Genome
- [ ] 测试 PASS

#### T2.3 [RED] test_evolution_session_load (Case 2 — V2 缺口闭环 ⭐⭐)
- [ ] 写 `tests/test_evolution_session_load.cpp` 5 cases (**CMake LABELS "l2-evolution"**):
  - Case 2.1: `load(genome@N)` → 期望 `genome_to_agent_config()` 返回 AgentConfig (per P0-1 fix: 不是 `Genome::to_chat_config`, 而是 internal helper `pdk_chat_demo_evolution::detail::to_agent_config`)
  - Case 2.2: ChatSession **11 参 ctor** 重建 (per `chat_session.h:210-230` 真实签名) → 期望装载成功
  - Case 2.3: baseline + reload 同 turn 输入 → 期望响应差异 (mutation 生效, mock 下 echo system_prompt)
  - Case 2.4: V2 缺口路径 (mock-only) → 期望 5-tier gate result Genome 实例化
  - Case 2.5: broken genome → 期望 load fail (NotFound) 而非 crash
- [ ] 验证 RED

#### T2.4 [GREEN] EvolutionSession::phase4_reload_rerun impl
- [ ] 写 `phase4_reload_rerun` impl
- [ ] 通过 `IGenomeRegistry::load(name, version)` + `pdk_chat_demo_evolution::detail::to_agent_config()` (L2 内部 helper, **不**触碰 include/) + `ChatSession(**11 参 ctor**, per P0-1 fix)` 重建
- [ ] mock mode 验证 PASS

#### T2.5 [REFACTOR] 5 段事件流 (phase1~phase6) 整合
- [ ] `run_6_phase_demo()` orchestration method
- [ ] phase1~phase5 各方法 refactor

#### T2.6 [AUDIT] Oracle dual-agent (Metis + Oracle)
- [ ] Metis: 5-tier gate 完整路径 + Phase 2 风险
- [ ] Oracle: V2 `load(genome@N)` 物理可行性 + ABI 不破坏

#### T2.7 [ARCHIVE] commit T2
- [ ] commit: `feat(example): pdk_chat_demo_evolution 6-phase demo end-to-end`

---

### T3: trace JSONL + capture-mode 集成 (ADR-0080 D10)

**估时**: 0.5 天
**依赖**: T2 (phase1~phase5 已可用)

#### T3.1 [RED] test_evolution_tracer_schema (Case 3)
- [ ] 写 `tests/test_evolution_tracer_schema.cpp` 4 cases:
  - Case 3.1: trace 4 段事件 → 期望 JSONL 8 字段 (phase/timestamp_iso8601/session_id/turn_input/response/tokens/cost_usd/meta)
  - Case 3.2: meta 含 genome_version (mutation 段) + eval_quality (compare 段)
  - Case 3.3: capture-mode=Training → 期望 IDistillationWriter 触发 (mock fixture)
  - Case 3.4: --trace-events flag 默认 close, 启用后 emit
- [ ] 验证 RED

#### T3.2 [GREEN] EvolutionTracer + capture-mode 集成
- [ ] 写 `evolution_tracer.cpp::subscribe_interaction_bus()`
- [ ] 写 `evolution_tracer.cpp::record_phase(json)` + `emit_jsonl()`
- [ ] 写 `main.cpp` argparse `--capture-mode={None|Training}`
- [ ] 测试 PASS

#### T3.3 [REFACTOR] JSONL schema 加 lock check
- [ ] 用 `nlohmann::json` (现成 dep) 替代手写 JSON 序列化
- [ ] schema 验证 helper: `assert_8_fields_consistent(json)`

#### T3.4 [AUDIT] Oracle: schema 稳定性 + IDistillationWriter 集成
- [ ] 验证 8 字段稳定性 (与 distill-source-survey 对齐)
- [ ] 验证 capture-mode fail-open 三重保护 (per ADR-0080 v1.2)

#### T3.5 [ARCHIVE] commit T3
- [ ] commit: `feat(example): pdk_chat_demo_evolution trace JSONL + capture-mode`

---

### T4: 实施期同步 3 份 SoT 文档

**估时**: 0.25 天
**依赖**: T2 + T3

#### T4.1 [GREEN] sync self-evolution-architecture §十一
- [ ] 加 row "L2 ✅ ship 2026-09-XX" (per design.md §九 cross-doc)
- [ ] 列: 端到端 6 段事件流
- [ ] 列: trace JSONL schema
- [ ] link to L2 README

#### T4.2 [GREEN] sync harness-architecture §十一
- [ ] 同上
- [ ] 标 `load(genome@N) → 重建 ChatSession → 1 turn` V2 缺口已闭环
- [ ] 例: 5-tier gate 在 L2 实例化

#### T4.3 [GREEN] sync rsi-architecture §十一
- [ ] 同上
- [ ] 标 Harness-RSI 在 L2 enabled
- [ ] 列: D7 stub provider 在 L2 端到端可用 (即 trace 选 providers 时能选)

#### T4.4 [GREEN] sync roadmap
- [ ] `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §十 Drift Log +1 L2 行
- [ ] §十一 Adjustment Log +1 L2 行

#### T4.5 [ARCHIVE] commit T4 (单独 atomic commit)
- [ ] commit: `docs(architecture+roadmap): sync L2 ship + SoT + roadmap cross-doc`

---

### T5: Oracle post-impl SHIP-with-fixes

**估时**: 0.5 天
**依赖**: T4

#### T5.1 [AUDIT] Oracle post-impl review
- [ ] 派 Oracle session: bg_x (待 rdd-builder 实施阶段生成)
- [ ] 期望 verdict: SHIP / SHIP-with-fixes / BLOCK
- [ ] 应用 Critical + Major fixes (如 SHIP-with-fixes)

#### T5.2 [GREEN] SHIP-with-fixes fixes
- [ ] 每次 fix commit atomic

#### T5.3 [ARCHIVE] 完整 archive (AGENTS.md Day 5 lesson)
- [ ] `git mv` 整个目录到 `openspec/changes/archive/pdk-chat-demo-evolution-reference-example/`
- [ ] 验证 4-file integrity (`.openspec.yaml + proposal.md + design.md + tasks.md + specs/<name>/spec.md`)

---

## Total Estimate

- T1: 0.5 天
- T2: 1.5 天
- T3: 0.5 天
- T4: 0.25 天 (cross-doc sync)
- T5: 0.5 天 (Oracle review + archive)
- **T6**: 0.5 天 (R8 反向指标门 + R9 反作弊测试, 2026-09-23 升级)
- **T6.7-T6.10**: 0.5 天 (R13 上下文驱动契约, 2026-09-23 升级 — ContextRequest 是 R8 反向指标门的源数据)
- **总计**: 4.25 天

**vs Master Plan 估时**: 2-3 天 (per proposal.md) → 1.25-2.25 天偏差 (R8 + R9 + R13 增量是 Cross-Doc Review 2026-09-23 后扩, 原估时未含)

---

### T6: R8 反向指标门 + R9 反作弊测试 (2026-09-23 升级)

**估时**: 0.5 天
**依赖**: T2 + T3 (主 demo 已可用, 注入 metrics 评估)

#### T6.1 [RED] test_reverse_indicators (R8.1 + R8.2 + R8.3)
- [ ] 写 `tests/test_reverse_indicators.cpp` 3 cases (与 spec.md R8.1/R8.2/R8.3 一一对应):
  - Case R8.1: drop_ratio > 5% 应 trigger exit non-zero + ack required
  - Case R8.2: 失败样本 trace 输出 4 字段 (`failure_event`, `rule_id`, `rule_shipped_commit`, `reproduce_in_new_task_demo`)
  - Case R8.3: ablation_report.json 3 段对照 (新/旧, 同任务/同 Harness, 拦截保留率)
- [ ] 验证 `ctest -R test_reverse_indicators` exit non-zero (RED)

#### T6.2 [GREEN] R8 metrics + flags 实施
- [ ] `evolution_session.cpp` 加 `--release-metrics` + `--regression-test-suite` + `--ablation-mode=full` flag
- [ ] 输出 `metrics.json` (new_up / new_down / old_up / old_down)
- [ ] 输出 `ablation_report.json` (3 段对照, **per P0'-3 fix**: 使用 attribution_verdict 分布 + response_edit_distance, NOT eval_quality diff)
- [ ] 测试 PASS

#### T0-5 [GREEN] Hermetic env fixture (P0'-4 fix, AGENTS.md 模式 #10 hygiene)
- [ ] `evolution_session.cpp` 启动时构造 `FilesystemGenomeRegistry` 之前调用 `setup_hermetic_home()`:
  - `setenv("HOME", "/tmp/l2-test-<uuid>", 1)` (uuid from `mkdtemp` template)
  - `setenv("HYDRAFORGE_GENOME_DIR", "$HOME/.hydraforge/genomes", 1)`
  - `mkdtemp` 隔离目录
- [ ] `evolution_session.cpp` 析构时调用 `cleanup_hermetic_home()` 递归删除 `/tmp/l2-test-<uuid>`
- [ ] mock + real-llm 双模式均走 hermetic HOME (NOT host `$HOME/.hydraforge/genomes/`)
- [ ] 验证: 测试结束 `/tmp` 不残留 `l2-test-*` 目录
- [ ] 验证: 测试不污染宿主机 `~/.hydraforge/genomes/` (per AGENTS.md 模式 #10 fresh-deploy 修复)

#### T6.3 [RED] test_anti_cheat (R9.1 + R9.2 + R9.3, per P0-3/7/6 降级)
- [ ] 写 `tests/test_anti_cheat_search_solution.cpp` 1 case (R9.1 — **P0-3 fix: 静态 string-containment 检测, 不依赖 LLM 输出**):
  - 输入: ContextRequest 含 baseline hint 字面串 ("the answer is X")
  - 断言: agent 输出不包含该字面串 (`result.find(hint) == std::string::npos`)
  - 实施 mock: MockLLMProvider 返回"echo system_prompt", 因此含 "the answer is X" 时 fail; L2 应过滤 hint
- [ ] 写 `tests/test_anti_cheat_metric_tampering.cpp` 1 case (R9.2 — **P0-7 fix: prefix-rejection + grep 静态契约**, 取代 vacuous "Gate 1 fail"):
  - 输入: ContextRequest `task_class: "mutation_metric_*"` 前缀
  - 断言: L2 拒绝 (exit non-zero) + emit `mutation_metric_rejected` 事件 (per T0-1 ADR-0068 v2.4 amendment)
  - 静态契约守卫: `grep -c "set_.*metric" include/agenticdsl/contract/ievaluator.h` = 0
- [ ] 写 `tests/test_anti_cheat_sandbox_escape.cpp` 1 case (R9.3 — **P0-6 fix: 降级为 keyword-rejection + Wave 4 defer**):
  - 输入: ContextRequest `turn_input` 含网络关键字 (e.g., "fetch http://...")
  - 断言: L2 keyword 拒绝 (exit non-zero) + emit `turn_input_network_keyword_rejected` 事件 (per T0-1)
  - spec/design 明示: 真 sandbox 网络隔离物理不可实现 (无 network_mode 配置, hardcoded "bridge"), defer to Wave 4
- [ ] 验证 RED

#### T6.4 [GREEN] R9 反作弊测试用例实施 (per P0-3/7/6 降级)
- [ ] Poolside test fixture: ContextRequest 含 baseline hint 字面串 (静态 string-containment)
- [ ] 复旦 metric tampering test fixture: `mutation_metric_*` 前缀 ContextRequest (L2 prefix-rejection)
- [ ] ExploitGym sandbox test fixture: `turn_input` 网络关键字 (keyword-rejection, **不**真 sandbox)
- [ ] 测试 PASS

#### T6.5 [AUDIT] Oracle: 反作弊测试充分性 + R8 双向指标覆盖
- [ ] Oracle session review R8.1-R8.3 + R9.1-R9.3 全部案例
- [ ] 应用 SHIP-with-fixes fixes (如 needed)

#### T6.6 [ARCHIVE] commit T6
- [ ] commit: `feat(example): pdk_chat_demo_evolution R8 + R9 (反向指标 + 反作弊)`

---

### T6.7-T6.10: R13 上下文驱动契约 (2026-09-23 升级)

**估时**: 0.5 天
**依赖**: T6.6 (R8 + R9 ship 后, ContextRequest 是 R8 反向指标门的"源数据")

#### T6.7 [RED] test_context_request_validation (R13.1 schema 校验)
- [ ] 写 `tests/test_context_request_validation.cpp` 5 cases (**CMake LABELS "l2-evolution"**):
  - Case R13.1.1: 缺 `context_id` → exit non-zero + 字段名
  - Case R13.1.2: `turn_input` 为空 → exit non-zero + context_id
  - Case R13.1.3: `task_class` 不在 **闭枚举** (`code_gen|research|summary|debug|classify|other`) → exit non-zero + 行号 (per P0-7 fix: `other` 兜底闭枚举)
  - Case R13.1.4: `invocation_mode` 不在枚举 → exit non-zero
  - Case R13.1.5: valid ContextRequest → 接受 + `meta.context_id` 进 trace
- [ ] 验证 `ctest -R test_context_request_validation` exit non-zero (RED)

#### T6.8 [GREEN] ContextRequest parser + L2 entrypoint flag 实施
- [ ] `evolution_session.cpp` 加 `--context-file <path.jsonl>` argparse
- [ ] ContextRequest JSONL parser (per spec.md R13.1 schema 8 字段)
- [ ] 启动无 `--context-file` → exit non-zero + stderr "ERROR: L2 零 hardcode..." (per S28)
- [ ] 任一 ContextRequest 字段缺失 → exit non-zero + 行号 + 字段名 (per S29-S30)
- [ ] 测试 PASS

#### T6.8a ⚠️ Design Deviation (Batch 1 ship, 2026-09-24, Oracle bg_1acb78c5 Major #1)

**事实**: Batch 1 ship (`a380ec7`) 的 `context_request.cpp` **未实现** `emit_event(bus, ...)` 调用。计划 T6.8 隐含假设 parser 内部发射 4 个 ADR-0068 v2.4 事件:
1. `mutation_metric_rejected` (R9.2 prefix-rejection)
2. `turn_input_network_keyword_rejected` (R9.3 keyword-rejection)
3. `hint_containment_rejected` (R9.1 parser-side regex detection)
4. `hidden_context_accepted_info` (R13.4 is_hidden=true accepted)

**根因**: Batch 1 scope 仅 Tasks 0-3 (skeleton + hermetic_home + context_request parser), 无 IInteractionBus 注入路径。`load_context_file(path, errors)` 签名无 bus 参数。

**影响**:
- Plan Task 6 (R9 反作弊测试) "expect fail until Task 3 emits events" — 即原本预期 Batch 1 完成事件发射，但 Batch 1 跳过
- `hidden_context_accepted_info` 事件同样推迟 (is_hidden 桶接受路径)
- 当前 `test_context_request_validation` 仅验证 parser 逻辑正确性（12 cases / 55 assertions PASS），不验证事件发射

**修复路径** (Batch 2 Task 5 必做):
- [x] `evolution_session.{h,cpp}` 构造期注入 `IInteractionBus*` 参数 (per ADR-0019 + ADR-0068 event topic payload schema)
- [x] `load_context_file` 签名扩展: `load_context_file(path, errors, bus*)` (新增第 3 个参数，opt-in nullptr fallback for backward compat)
- [x] 4 个事件发射点 (parser 内每条 detection 分支):
  - R9.1 hint detection → `emit_event(bus, "hint_containment_rejected", {context_id, turn_input_preview, matched_pattern})`
  - R9.3 network keyword → `emit_event(bus, "turn_input_network_keyword_rejected", {context_id, turn_input_preview, keyword})`
  - R9.2 prefix-rejection → `emit_event(bus, "mutation_metric_rejected", {context_id, task_class_preview, reason})`
  - R13.4 is_hidden accept → `emit_event(bus, "hidden_context_accepted_info", {context_id, task_class, is_hidden, bucket="hidden"})`
- [x] Task 6 (Batch 3) R9 tests 完成后应全部 PASS (anti-cheat fixtures 触发 detection → emit → verify)
- [x] 验证: `grep "emit_l2_event(bus" examples/pdk_chat_demo_evolution/context_request.cpp | wc -l` ≥ 4 (实际 = 4, 已 verify)

**Affirmation**: ✅ **T6.8a 已闭环 (Batch 2 commit `ad2f42c`)** — 4 emit sites verified via `test_l2_event_emission` (5 cases / 22 assertions PASS, InMemoryBus subscription). 修复闭环后 Batch 3 R9 反作弊测试可触发 detection → emit → verify 链路。

---

#### T6.9 [RED] test_context_request_e2e (R13.3 ≥ 3 类 ContextRequest 实证)
- [ ] 写 `tests/test_context_request_e2e.cpp` 4 cases (**CMake LABELS "l2-evolution"**):
  - Case R13.3.1: `--context-file code-class.jsonl` (单类, 1 个 ContextRequest) → 期望 `accept-contexts` flag 输出单类 `attribution_verdict` (per C5 P0 fix: 不输出 eval_quality, L2 不计算; 单类不构成 generalizable)
  - Case R13.3.2: `--context-file code+research+debug-3class.jsonl` (3 类, 各 1 个) → 期望 `--accept-contexts` 输出 3 类对比 (attribution_verdict per class)
  - Case R13.3.3: `--context-file code+research+debug-3class.jsonl` + `--release-metrics` → R8.1 期望 ≥ 3 类对照矩阵 (mechanism demonstration, per C5 fix)
  - Case R13.3.4: ContextRequest `metadata.is_hidden=true` → L2 **接受 + 进 hidden 桶** + emit `hidden_context_accepted_info` (per P0 is_hidden 重定位, 不再拒绝; E2 公开/隐藏集分离评测)
- [ ] 验证 RED

#### T6.10 [GREEN] `examples/pdk_chat_demo_evolution/fixtures/contexts/` 3 类 reference ContextRequest + R13.4 / R13.6 集成 (P0-10 fixture 目录)
- [x] 写 `examples/pdk_chat_demo_evolution/fixtures/contexts/code-class-context.jsonl` (K8s YAML / Python test 2 个, per fixture catalog)
- [x] 写 `examples/pdk_chat_demo_evolution/fixtures/contexts/research-class-context.jsonl` (文献摘要 / 论文对比 2 个)
- [x] 写 `examples/pdk_chat_demo_evolution/fixtures/contexts/debug-class-context.jsonl` (日志分析 / 性能调优 2 个)
- [ ] 实施 `metadata.is_hidden=true` 接受进 hidden 桶 (per P0 fix: 不再拒绝, E2 公开/隐藏集分离)
- [ ] R13.4 与 R8 + R9 集成: ContextRequest 进 trace `meta.context_id` + `failure_event` 字段引用 `context_id`
- [ ] 测试 PASS

#### T0-4 [GREEN] Test fixture 目录创建 (P0-10, 与 L2 binary 同期 ship)
- [x] 创建 `examples/pdk_chat_demo_evolution/tests/fixtures/context_request/` 目录 + 11 个 fixture 文件 (per design §3.1.5 fixture catalog):
  - [x] `valid_3class_combined.jsonl` — 3 entries (code/research/debug), for R13.3.2/R13.3.3
  - [x] `valid_single_code.jsonl` — 1 entry, for R13.3.1
  - [x] `is_hidden_true.jsonl` — 1 entry (is_hidden=true), for R13.3.4
  - [x] `anti_cheat_hint_input.jsonl` — R9.1 baseline hint 字面串 "the answer is X"
  - [x] `anti_cheat_metric_tampering.jsonl` — R9.2 task_class="mutation_metric_*" prefix
  - [x] `anti_cheat_network_keyword.jsonl` — R9.3 turn_input 含 "fetch http://"
  - [x] `r8_failure_fixtures.jsonl` — 3 entries (gate-violation / budget-exceeded / attribution-confounded), for R8.3 ablation
  - [x] `invalid_missing_context_id.jsonl` — R13.1.1 negative test
  - [x] `invalid_empty_turn_input.jsonl` — R13.1.2 negative test
  - [x] `invalid_task_class_enum.jsonl` — R13.1.3 negative test
  - [x] `invalid_invocation_mode.jsonl` — R13.1.4 negative test
- [ ] 验证: `find examples/pdk_chat_demo_evolution -name '*.jsonl' | wc -l` = 14 (3 SHIPPED + 11 test)
- [ ] 验证: 所有 fixture JSON 合法 (per `python3 -c 'import json; ...'` round-trip)
- [ ] 实施 `tests/CMakeLists.txt` 加 `configure_file` 把 fixture 路径注入 test binary 的 runtime path (per pdk_chat_demo/tests/CMakeLists.txt existing pattern)

#### T6.11 [AUDIT] Oracle: ContextRequest 零 hardcode + 跨 doc 一致性
- [ ] Oracle session review R13.1-R13.6 全部 case + L2 零 hardcode 验证
- [ ] 验证 trace JSONL `meta.context_id` 字段存在 + 反作弊 R9.2 (拒绝 mutation_metric_* ContextRequest) 实施
- [ ] 应用 SHIP-with-fixes fixes (如 needed)

#### T6.12 [ARCHIVE] commit T6.7-T6.10
- [ ] commit: `feat(example): pdk_chat_demo_evolution R13 上下文驱动契约 + ContextRequest`

---

## Out-of-Scope Tasks (deferred to future work)

- ❌ Wave 3 Phase 2 D4-D7 完整化 (独立立项)
- ❌ S4 Agent-Agent 协同进化 example (research 路径)
- ❌ 修改 `examples/pdk_chat_demo/` 主 demo (N1 强制)
- ❌ 引入新公共 API / Contract (N2 强制)
- ❌ L2 默认 ContextRequest (R13.2 零 hardcode 强制 — L2 不是 autonomous evaluator)

---

## Acceptance Criteria Checklist

L2 merge gate (含 R8 + R9 + R13 2026-09-23 升级, P0 修正):

- [ ] **T0-1 ADR-0068 Appendix A v2.4 amendment**: 3-4 个新主题注册, owner=`pdk_chat_demo_evolution`
- [ ] **T0-2 harness-architecture-2026-09.md §3.1 API 表面校正**: ChatSession ctor 4-arg → 11-arg, override_* 5 method → 2 method, apply_harness_mutation 6 字段 → 5 参
- [ ] **T0-3 spec.md OpenSpec delta 格式**: `## ADDED Requirements` + `### Requirement:` + `#### Scenario:` WHEN/THEN 结构, `openspec validate --strict` PASS
- [ ] `cmake build -DAGENTICDSL_BUILD_EXAMPLES=ON -DAGENTICDSL_BUILD_TESTS=ON` exit 0
- [ ] `ctest -R test_evolution_session_mutation` exit 0 (5+ cases, **CMake LABELS "l2-evolution"**)
- [ ] `ctest -R test_evolution_session_load` exit 0 (5+ cases)
- [ ] `ctest -R test_evolution_tracer_schema` exit 0 (4+ cases)
- [ ] `ctest -R test_distillation_capture_mode` exit 0 (4+ cases)
- [ ] `ctest -R test_reverse_indicators` exit 0 (3 cases / R8.1+R8.2+R8.3)
- [ ] `ctest -R test_anti_cheat_search_solution` exit 0 (1 case / R9.1 静态 string-containment, P0-3 fix)
- [ ] `ctest -R test_anti_cheat_metric_tampering` exit 0 (1 case / R9.2 prefix-rejection + grep, P0-7 fix)
- [ ] `ctest -R test_anti_cheat_sandbox_escape` exit 0 (1 case / R9.3 降级 keyword-rejection, P0-6 fix)
- [ ] `ctest -R test_context_request_validation` exit 0 (5 cases / R13.1)
- [ ] `ctest -R test_context_request_e2e` exit 0 (4 cases / R13.3, is_hidden 接受进 hidden 桶)
- [ ] `ctest -LE l2-evolution` 排除 10 binary, baseline 211 零回归 (**P0-7 fix**: 原 `-E pdk_chat_demo_evolution` 正则失效, 改用 LABELS)
- [ ] `git diff examples/pdk_chat_demo/` (pre-L2 vs post-L2-merge) = 0 lines (N1 保证)
- [ ] `./pdk_chat_demo_evolution` 启动无 `--context-file` → exit non-zero (R13.2 S28)
- [ ] `./pdk_chat_demo_evolution --context-file examples/pdk_chat_demo_evolution/fixtures/contexts/code-class-context.jsonl` exit 0 + JSONL 8 顶层字段 + `meta.context_id` 进 trace
- [ ] `./run_evolution_demo.sh --mock --release-metrics --ablation-mode=full --context-file examples/pdk_chat_demo_evolution/fixtures/contexts/{code,research,debug}-class-context.jsonl` exit 0 (R8 metrics + ablation report + ≥ 3 类 ContextRequest, **机制演示** per C5 fix)
- [ ] 3 份 SoT 文档 §十一 + §十二 + §12.9 (R13 引用) 同步 ship 行 + harness-arch §3.1 API 表面校正 (T0-2)
- [ ] 4-file archive 完整 (proposal + design + tasks + specs/<name>/spec.md + .openspec.yaml)
- [ ] `examples/pdk_chat_demo_evolution/fixtures/contexts/` 3 reference ContextRequest file ship (R13.3)
- [ ] **commit message `[Reverse Indicator]` 段** (per AGENTS.md 强制): `new_up` / `old_down` / `failure_traces` / `ablation` / `context_ids` 5 字段齐备 (含本 commit 涉及的所有 ContextRequest context_id 列表)
