# pdk-chat-demo-evolution spec — Requirements

> **Status**: 🔍 Proposed Spec (PLACEHOLDER 待 rdd-builder 实施阶段细化)
> **关联 Proposal**: [`../../proposal.md`](../../proposal.md)
> **关联 Design**: [`../../design.md`](../../design.md)

## Purpose

本规范定义 `examples/pdk_chat_demo_evolution/` 子项目 (本 change L2 创生) 的功能需求。

**核心命题 (proposal §Why.4)**: HydraForge 自进化方向架构以 3 份 Source of Truth 文档 (`self-evolution-architecture-2026-08.md` v1.5 + `harness-architecture-2026-09.md` v1.0 + `rsi-architecture-2026-09.md` v1.0) 为一致基线。本 change 提供 L2 reference example, 让用户能 clone 项目后实际跑通"5-tier gate 端到端 + trace + reload 真实装载"的 mutation 路径。

---

## ADDED Requirements

### Requirement: standalone-binary-and-dual-mode

`examples/pdk_chat_demo_evolution/main.cpp` binary **MUST** 接受以下 **9 个 CLI flag** (R1 + R8 + R13 unified flag surface, per P1-6 fix 2026-09-23 — resolves phantom flag issue per Metis DB-C):

**R1 main mode (5 flags)**:
1. `--mock` — Mock LLM mode (CI/CT 验证, 5 秒内 exit 0)
2. `--real-llm <provider>` — Real LLM mode (需 `DEEPSEEK_API_KEY` env var, 30 秒内 exit 0)
3. `--capture-mode={None|Training}` — 默认 `None`, `Training` 启用 IDistillationWriter (P0-1 fix 同步 design.md §D4)
4. `--trace-events` — 默认 off, 启用后 4 段事件 emit 到 stdout
5. `--context-file <path.jsonl>` — **必填 (R13 唯一输入入口)**, 用户提供 ContextRequest

**R8 reverse-indicator (3 flags, 全部 P0-5 + P1-6 fix 定义行为)**:
6. `--release-metrics` — 输出 `metrics.json` (new_up / new_down / old_up / old_down + drop_ratio), drop_ratio > 5% → exit non-zero (per R8.1)
7. `--regression-test-suite` — 跑 N 个 pre-ship acceptance suite, 输出 drop matrix (per R8.1); N 为内部配置 (默认 = spec 5-tier gate 测试套件)
8. `--ablation-mode=full` — 输出 `ablation_report.json` 3 段对照 (per R8.3, **P0'-3 fix**: 使用 attribution_verdict 分布 + response_edit_distance)

**R13 helper (1 flag, P1-6 fix phantom → real)**:
9. `--accept-contexts` — 输出 **已处理 ContextRequest 列表**到 stderr (含 context_id + task_class + is_hidden + bucket), 用于**用户调试可见性**(确认 6 段链真实处理了哪些 ContextRequest, 防止 L2 静默忽略某些行); R13.3 ≥ 3 类对比时输出 3-class `attribution_verdict` 分布表

**R13 必填约束**: 无 `--context-file` flag → exit non-zero + stderr "ERROR: L2 零 hardcode, 必须提供 ContextRequest via --context-file <path.jsonl>"

**L2 行为约束 (per AGENTS.md §REAL LLM TESTING)**: `--real-llm` 模式从 **env var** `DEEPSEEK_API_KEY` 读取凭证, **不** 通过 CLI flag (`--real-llm deepseek-key=...`); 这是 L2 **不允许**的 hardcode 模式

#### Scenario: mock-mode-5sec-exit-0

- **WHEN** `./run_evolution_demo.sh --mock --context-file examples/pdk_chat_demo_evolution/fixtures/contexts/code-class-context.jsonl` 被调用
- **THEN** binary MUST exit 0 within 5 seconds
- **AND** MUST emit JSONL trace with 4 phase events (baseline / mutation / reload / compare) per `--trace-events` flag

#### Scenario: real-llm-30sec-exit-0

- **WHEN** `DEEPSEEK_API_KEY=...` is set AND `--real-llm deepseek --trace-events --context-file ...` 被调用
- **THEN** binary MUST exit 0 within 30 seconds
- **AND** MUST emit 4 phase events with real LLM responses

#### Scenario: missing-context-file-exit-nonzero

- **WHEN** binary starts WITHOUT `--context-file` flag
- **THEN** MUST exit non-zero
- **AND** MUST print stderr "ERROR: L2 零 hardcode, 必须提供 ContextRequest via --context-file <path.jsonl>"

#### Scenario: accept-contexts-stdout-list

- **WHEN** `--accept-contexts` flag is enabled AND `--context-file code+research+debug-3class.jsonl` is provided (3 类合并)
- **THEN** stderr MUST list all 3 processed ContextRequest with `context_id` + `task_class` + `is_hidden` + `bucket`
- **AND** MUST include 3-class `attribution_verdict` comparison table (per R13.3 ≥ 3 类 实证要求)
- **AND** `--accept-contexts` MUST NOT alter exit code (purely diagnostic)

#### Scenario: regression-test-suite-runs-acceptance-suite

- **WHEN** `--regression-test-suite` is enabled
- **THEN** binary MUST run internal 5-tier gate acceptance suite (configurable via `HYDRAFORGE_REGRESSION_SUITE` env var, default = internal suite)
- **AND** MUST output drop_matrix.json with per-suite pass/fail counts
- **AND** MUST exit 0 if all suites pass, exit non-zero if any fail

#### Scenario: release-metrics-drop-ratio-block

- **WHEN** `--release-metrics` is enabled AND injected fixture has drop_ratio > 5%
- **THEN** binary MUST exit non-zero (R8.1 mechanism activated)
- **AND** MUST output `metrics.json` with the dropped ratio and triggering context_ids

---

### Requirement: six-phase-end-to-end-chain

Binary **MUST** execute the 6-phase chain in order (per design.md §3.2). All `turn_input` MUST come from ContextRequest (NOT hardcoded fixtures). **All 6 phases driven by ContextRequest[i] row by row** — per R13.2, L2 binary does NOT auto-generate ContextRequest.

1. **Phase 0 (Load Contexts)**: `ContextRequest::load(--context-file)` → schema validation (per scenario `ctx-validation-...`) → `contexts_` vector
2. **Phase 1 (Init)**: instantiate `ChatSession` (per 11-param real signature, see standalone-binary-... AND chat-session-11-param-construction scenario) + `PluginLoader` (per pdk_chat_demo pattern) + register `evolution_tracer` to subscribe `IInteractionBus`
3. **Phase 2 (Baseline)**: `ChatSession::chat(ContextRequest[i].turn_input)` → record `{phase: "baseline", context_id: ..., response: ...}`
4. **Phase 3 (Mutation)**: call `apply_harness_mutation(prompt_delta + tools_add)` (per **5-param free function signature** `harness_rsi.h:73-78`, NOT "MutationGateChain class") → walk 5-tier gate → emit `{phase: "mutation", context_id: ..., gate_passes: [...], genome_version: ..., applied_tools: ...}`
5. **Phase 4 (Reload + Rerun)** ⭐: `IGenomeRegistry::load(name, version)` + `pdk_chat_demo_evolution::detail::to_agent_config()` (L2 internal helper, NOT public API) + **ChatSession 11-param ctor rebuild** → same `ContextRequest[i].turn_input` → record `{phase: "reload", context_id: ..., genome_version: ..., response: ...}`
6. **Phase 5 (Compare)**: call `IEvaluator` V2 (`BehavioralEquivalence`) → emit `{phase: "compare", context_id: ..., verdict: "approved|denied", attribution_verdict: "Attributed|Confounded|Insufficient|NotAttempted"}` (**NOT eval_quality, per C5 P0 fix**)
7. **Phase 6 (Emit JSONL)**: stdout (per design.md §3.3 D5) → loop next `ContextRequest[i+1]`, all processed → exit 0

#### Scenario: all-six-phases-emit-jsonl-trace

- **WHEN** mock mode runs with 1 ContextRequest
- **THEN** `evolution_tracer` MUST emit 4 phase events to stdout (when `--trace-events` enabled)
- **AND** each event MUST include `meta.context_id` (per R13.2 S31)

#### Scenario: v2-gap-reload-real-load

- **WHEN** Phase 4 reload is executed with mock mutation
- **THEN** `IGenomeRegistry::load(name, version)` MUST return the committed Genome
- **AND** `pdk_chat_demo_evolution::detail::to_agent_config(loaded)` MUST return a valid AgentConfig
- **AND** ChatSession 11-param ctor MUST succeed
- **AND** ChatSession MUST respond to `ContextRequest[i].turn_input` with mutated behavior

---

### Requirement: five-tier-gate-sequence

L2 demo **MUST** execute 5-tier gate in order (per design.md §3.2 Phase 3):

- **Gate 0 (Syntax)**: `MutationRequest` field required check + types
- **Gate 1 (Policy + 3 Signals)**:
  - `policy.semantic_locked_tools` NOT in `mutation.tools_add`
  - `policy.denied_tools` NOT in `mutation.tools_add`
  - `attribution_verdict == "Attributed"` (per ADR-0086 v1.1 `judge_data_freshness()`)
  - `eval_quality != "Poor"` (per ADR-0083 V2 `Composite`) — **NOTE**: eval_quality is user-declared via `expected_eval_quality` per C5 fix, NOT L2-computed
  - `budget_state != "exceeded"`
- **Gate 2 (Load + Freshness)**:
  - `IGenomeRegistry::load(name, parent_version)` succeeds
  - `judge_data_freshness(data, current, registry)` returns "Attributed" (per C3 follow-up)
- **Gate 2.5 (Partial-Apply)**: collect `AppliedMutation` snapshot (prompt_snapshot + tools_snapshot), prepare in-memory apply (no side effects)
- **Gate 3 (Persist-Before-Apply)**:
  - `fork(name, parent_version, final_spec)` persists to `IGenomeRegistry` (per G4)
  - commit failure → `RegistryRejected` + zero state change
  - commit success → `genome.committed` event + load into chat session

#### Scenario: all-five-gates-pass-success

- **WHEN** `apply_harness_mutation(prompt_delta, ...)` is called with valid mutation
- **AND** evaluate_readiness returns can_proceed=true
- **AND** is_tool_allowed returns true
- **THEN** 5-tier gate MUST all PASS
- **AND** `AppliedMutation` MUST be returned

#### Scenario: gate-0-fail-on-invalid-input

- **WHEN** `apply_harness_mutation(invalid prompt_delta)` is called
- **THEN** Gate 0 MUST fail
- **AND** binary MUST exit non-zero

#### Scenario: gate-1-fail-on-denied-tools

- **WHEN** `apply_harness_mutation(denied_tools)` is called with `policy.denied_tools` matching
- **THEN** Gate 1 MUST fail
- **AND** `mutation.denied` event MUST be emitted

---

### Requirement: trace-jsonl-schema-stability

`evolution_tracer` **MUST** emit JSONL 4 phase events to stdout (per design.md §3.3):

Top-level 8 fields (per spec S8):
- `phase` (string: baseline | mutation | reload | compare)
- `timestamp_iso8601` (ISO 8601 string)
- `session_id` (UUID string)
- `turn_input` (string)
- `response` (string | null — null in mutation phase)
- `tokens` (int, real-LLM mode)
- `cost_usd` (float, real-LLM mode)
- `meta` (object, see below)

**`meta` object fields** (per P0-3 fix: trace JSONL must contain `meta.context_id` per R13.2 S31):
- `genome_version` (string `name@version` | null)
- `gate_passes` (array of strings | null — mutation phase)
- `eval_quality` (string Acceptable|Poor|Excellent | null — compare phase)
- `attribution_verdict` (string Attributed|Confounded|Insufficient|NotAttempted | null — compare phase)
- `trace_id` (UUID string)
- `capture_mode` (string None|Training)
- `context_id` (UUID string — **R13 required**)
- `task_class` (enum string — R13 required)
- `is_hidden` (bool — R13 required)
- `sensitivity` (enum string public|internal|confidential — R13 required)
- `hidden_bucket` (bool — R13.4 P0 fix: true when is_hidden=true, marks event in hidden bucket)
- `expected_eval_quality` (string | null — R13 user-declared, **L2 echo, NOT compute**)

**Schema stability guarantee**: NO field can be deleted without bumping spec version. Add field backward-compatible. Rename field breaks consumers.

#### Scenario: trace-4-events-with-8-top-level-and-meta-context-id

- **WHEN** mock mode runs with 1 ContextRequest AND `--trace-events` enabled
- **THEN** stdout MUST contain exactly 4 JSONL lines (one per phase)
- **AND** each line MUST contain top-level 8 fields
- **AND** each line's `meta` MUST contain `context_id` (from ContextRequest)
- **AND** each line's `meta` MUST contain `task_class`, `is_hidden`, `sensitivity` (from ContextRequest)

#### Scenario: schema-stability-no-field-removal

- **WHEN** any future change modifies spec.md (add/remove/rename trace fields)
- **THEN** MUST bump spec version
- **AND** MUST NOT remove fields without explicit migration plan

---

### Requirement: capture-mode-integration

L2 **MUST** support `--capture-mode=Training` flag, which enables `IDistillationWriter` to write Session JSONL to disk (per distill-source-survey-2026-08.md recommended path).

#### Scenario: capture-mode-training-writes-distillation-record

- **WHEN** `--capture-mode=Training` is enabled
- **THEN** `IDistillationWriter` MUST write at least 1 `DistillationRecord` to disk

#### Scenario: capture-mode-none-no-write

- **WHEN** `--capture-mode=None` (default) is enabled
- **THEN** NO `DistillationRecord` MUST be written to disk

#### Scenario: capture-mode-failopen-triple-protection

- **WHEN** capture-mode=Training fails open (e.g., path mismatch per ADR-0080 v1.2)
- **THEN** `event_log.capture_mode_downgrade` event MUST be emitted
- **AND** binary MUST continue running (NOT crash)

---

### Requirement: backwards-compatibility-no-main-demo-modification

L2 **MUST NOT** modify `examples/pdk_chat_demo/` main binary. The two binaries (`pdk_chat_demo` + `pdk_chat_demo_evolution`) MUST coexist.

#### Scenario: hermetic-env-fixture-mock-and-real-llm-both

> **P0'-4 修复 (2026-09-23)**: 解决 Metis DB-D (mock "mem-only registry" 在代码库不存在, 修 AGENTS.md 模式 #10 fresh-deploy 风险).

- **WHEN** L2 binary starts in either `--mock` or `--real-llm` mode
- **THEN** L2 MUST set hermetic env fixture BEFORE constructing `FilesystemGenomeRegistry`:
  - `setenv("HOME", "/tmp/l2-test-<uuid>", 1)` (unique per process invocation)
  - `setenv("HYDRAFORGE_GENOME_DIR", "$HOME/.hydraforge/genomes", 1)` (per C2 ship convention)
  - `mkdtemp` to create the HOME directory
- **AND** L2 MUST use `FilesystemGenomeRegistry` (per `src/core/genome/registry_filesystem.cpp` 现成实现) — NOT a new in-memory registry (would violate N2 public API freeze)
- **AND** L2 MUST NOT write to host `$HOME/.hydraforge/genomes/` (mode `-mock` and `--real-llm` both run in hermetic HOME)
- **AND** on shutdown, L2 MUST `cleanup_hermetic_home()` to remove `/tmp/l2-test-<uuid>` recursively (via teardown destructor)

#### Scenario: zero-diff-on-main-demo

- **WHEN** `git diff examples/pdk_chat_demo/` is run (pre-L2 vs post-L2-merge)
- **THEN** MUST show 0 lines diff (per T1.4 N1 guarantee)

#### Scenario: full-ctest-baseline-211-zero-regression

- **WHEN** `ctest -LE l2-evolution` is run (P0-7 fix: **CMake LABELS** mechanism, NOT `-E pdk_chat_demo_evolution` regex which fails to match new test names)
- **THEN** baseline 211 tests MUST pass with 0 regression
- **AND** 10 new L2 test binaries MUST be excluded (mut + load + distillation + tracer + reverse_indicators + anti_cheat×3 + context_request_validation + context_request_e2e)

#### Scenario: main-demo-cli-behavior-unchanged

- **WHEN** `examples/pdk_chat_demo/pdk_chat_demo --mock --help` is run
- **THEN** behavior MUST be zero change from pre-L2

---

### Requirement: public-api-contract-freeze

L2 **MUST NOT** introduce new public API or Contract. L2 MUST only consume:

- `ChatSession` ctor (per `chat_session.h:210-230` real signature: **11 params, NOT "4 params"**, see `chat-session-11-param-construction` scenario)
- `ChatConfig::override_provider` + `ChatConfig::override_system_prompt` (**2 method, NOT 5**, see `chat-config-2-method-only` scenario; tools/budget/model_routing dimensions go via AgentConfig public fields)
- `IGenomeRegistry` interface (commit/load/fork/walk_ancestors)
- `apply_harness_mutation` (per **5-param free function signature** `harness_rsi.h:73-78`, **NOT "6-field interface / MutationGateChain class"** which does not exist in code)
- `LLMProviderFactory::register_dynamic` (Wave 3 Phase 1 D7 stub reuse)
- `IDistillationWriter` interface (D10 Capture)
- `IEvaluator` V2 (BehavioralEquivalence + Composite, **already ship**)

#### Scenario: include-dir-zero-diff

- **WHEN** `git diff include/` is run (pre-L2 vs post-L2-merge)
- **THEN** MUST show 0 lines diff (public interface freeze)

#### Scenario: binary-nm-ldd-only-existing-sos

- **WHEN** L2 demo binary is inspected via `nm` / `ldd`
- **THEN** MUST only depend on existing `.so` files (no new library dependencies)

#### Scenario: chat-config-2-method-only

- **WHEN** `grep "override_" include/agenticdsl/pdk/chat_session.h` is run
- **THEN** MUST show only 2 methods: `override_provider(provider, model)` + `override_system_prompt(overwrite, append)`
- **AND** MUST NOT show `override_tools` / `override_budget` / `override_model_routing` / `override_prompt_prefix` (design/proposal claims of "5 methods" are factually incorrect)

#### Scenario: chat-session-11-param-construction

- **WHEN** L2 constructs ChatSession
- **THEN** MUST use 11-param ctor `(DSLEngine*, shared_ptr<IInteractionBus>, IToolRegistry*, const AgentConfig&, const SessionConfig&, shared_ptr<CancellationRegistry>, ITimerService* = nullptr, unique_ptr<IInputSource> = nullptr, unique_ptr<ILogger> = nullptr, SessionManager* = nullptr, optional<ResumeToken> = nullopt)` per `chat_session.h:210-230`
- **AND** MUST inject LLM provider via `engine->set_llm_provider()` (NOT via ctor param — provider is NOT a ctor param)

---

### Requirement: cross-doc-consistency-three-SoT-ship-row

After L2 ship, all 3 Source of Truth docs MUST add "L2 ✅ ship" row in §十一 traceback:

- `docs/architecture/self-evolution-architecture-2026-08.md` §十一
- `docs/architecture/harness-architecture-2026-09.md` §十一 (AND §3.1 API surface correction per T0-2)
- `docs/architecture/rsi-architecture-2026-09.md` §十一

#### Scenario: all-three-SoT-ship-row-added

- **WHEN** L2 merge commit is created
- **THEN** all 3 SoT docs MUST have "L2 ✅ ship" row in §十一
- **AND** harness-arch §3.1 MUST reflect corrected ChatSession ctor 11-param signature + ChatConfig::override_* 2 method (per T0-2)

---

### Requirement: reverse-indicator-gate-R8-mechanism-demonstration

L2 demo **MUST** expose 3 flags (`--release-metrics`, `--regression-test-suite`, `--ablation-mode=full`) and **SHALL** implement the R8.1 / R8.2 / R8.3 mechanism demonstrations per the scenarios below.

> **来源**: per Cross-Doc Review 2026-09-23, 基于三阶段文档评审 Gap #2 + 用户提交 16 模块 E6 + 反向指标精神
> **核心命题 (P0-4 + C5 fix)**: L2 R8 是**机制演示 + 静态契约守卫**, **不**声称对 R8 红线能力的真实度量. 与 rsi §11.8.1 "不能宣称反作弊 100% 防御" 治理边界一致.

R8.1 双向指标 (new_up / new_down / old_up / old_down) + drop_ratio > 5% 自动 block.
R8.2 失败样本可追溯 (failure_event + rule_id + rule_shipped_commit + reproduce_in_new_task_demo + context_id).
R8.3 消融实验 3 段对照 (同任务不同 Harness / 同 Harness 不同任务 / 失败样本保留拦截率).

**Mechanism demonstration caveat**: mock 模式下 drop_ratio 按构造恒 0, 项目无失败样本语料. L2 ship 时附 3 个 failure fixture ContextRequest (T6.10 产出) 作为 R8.3 第 3 段数据源.

#### Scenario: r8-1-drop-ratio-mechanism-block

- **WHEN** `--release-metrics` is enabled AND injected fixture has drop_ratio > 5%
- **THEN** binary MUST exit non-zero (R8.1 mechanism activated)

#### Scenario: r8-2-failure-trace-4-fields

- **WHEN** failure event occurs in L2 demo
- **THEN** trace line MUST include 4 fields: `failure_event` + `rule_id` + `rule_shipped_commit` + `reproduce_in_new_task_demo`
- **AND** MUST include `context_id` linking to ContextRequest

#### Scenario: r8-3-ablation-3-segments

> **P0'-3 修复 (2026-09-23)**: 解决 Metis M-D (R8.3 仍引用 `eval_quality diff`, 与 P0-4 "L2 不计算 eval_quality" 矛盾). **改用 attribution_verdict 分布 + response edit distance** (与 P0-4 D7 attribution_verdict 输出对齐).

- **WHEN** `--ablation-mode=full` is enabled
- **THEN** ablation_report.json MUST contain 3 segments
- **AND** **Segment 1 (same-task-different-Harness)**: baseline_response vs post_mutation_response → MUST report (a) `attribution_verdict` distribution per category (Attributed/Confounded/Insufficient/NotAttempted, per ADR-0086 v1.1) AND (b) `response_edit_distance` (Levenshtein normalized 0.0-1.0)
- **AND** **Segment 2 (same-Harness-different-task)**: `attribution_verdict` consistency across task_class (per R13.3 ≥ 3 类 ContextRequest, comparison matrix)
- **AND** **Segment 3 (failure-sample-retention-rate)**: using 3 ship-time fixture ContextRequests from `r8_failure_fixtures.jsonl` (per T6.10), retention rate computed against `expected_eval_quality` (user-declared baseline, **NOT L2-computed** per P0-4 C5 fix)
- **AND** MUST NOT contain any `eval_quality` field as L2-computed metric (P0-4 explicit constraint; `eval_quality` only appears as `expected_eval_quality` echo from ContextRequest)

---

### Requirement: anti-cheat-test-suite-R9-degraded-mechanism

L2 **MUST** include 3 anti-cheat test binaries (R9.1 / R9.2 / R9.3) per the scenarios below. L2 **SHALL** position the R9 test suite as a **degraded mechanism demonstration** (per Oracle C1/C2 + Metis §6 deal-breaker).

> **来源**: per Cross-Doc Review 2026-09-23, Gap #1 + 用户提交 16 模块 R1-R4 + 反作弊 3 模式
> **核心命题 (P0-3/6/7 fix)**: L2 R9 是**降级版机制演示** (per Oracle C1/C2 + Metis §6 deal-breaker). 真实反作弊需依赖 LLM provider 自觉行为 + 真实 sandbox infra, 均不在 L2 demo 范围.

#### Scenario: r9-1-parser-side-hint-detection

> **P0'-2 修复 (2026-09-23)**: 解决 Metis DB-B + FM-2 (R9.1 filter 未定义, mock 静态断言 vacuous). **改为 parser-side regex 检测**, LLM 不介入, 断言确定可测.

- **WHEN** ContextRequest `turn_input` matches baseline hint regex pattern (default `R"(the answer is \w+)"`)
- **THEN** L2 MUST reject with exit non-zero
- **AND** MUST emit `hint_containment_rejected` event (per T0-1 ADR-0068 v2.4 amendment, owner=`pdk_chat_demo_evolution`)
- **AND** MUST NOT call LLM provider (parser-side detection runs BEFORE Phase 2 baseline)
- **AND** rejection event payload MUST include `context_id` + `turn_input_preview` (first 100 chars) + `matched_pattern`
- **AND** assertion `result.find(hint) == std::string::npos` MUST hold (trivially true: parser rejected before LLM, no output produced)
- **NOTE**: The hint regex pattern is configurable via L2 internal constant (e.g., `pdk_chat_demo_evolution::detail::R9_1_HINT_REGEX`). Future R9.* modes reserve additional regex patterns. CI-runnable per AGENTS.md §REAL LLM TESTING.

#### Scenario: r9-2-prefix-rejection-and-grep-guard

- **WHEN** ContextRequest `task_class` starts with `mutation_metric_*` prefix
- **THEN** L2 MUST reject with exit non-zero
- **AND** MUST emit `mutation_metric_rejected` event (per T0-1 ADR-0068 v2.4 amendment)
- **AND** `grep -c "set_.*metric" include/agenticdsl/contract/ievaluator.h` MUST equal 0 (static contract guard)
- **NOTE**: P0-7 fix replaces original "Gate 1 fail" assertion (GenomeMutations has no field to carry metric mutation, vacuous test)

#### Scenario: r9-3-keyword-rejection-degraded-with-wave-4-deferral

- **WHEN** ContextRequest `turn_input` contains network keyword (e.g., "fetch http://...")
- **THEN** L2 MUST reject with exit non-zero
- **AND** MUST emit `turn_input_network_keyword_rejected` event (per T0-1 ADR-0068 v2.4 amendment)
- **NOTE**: P0-6 fix downgrades original sandbox `network_mode=none` requirement. Real sandbox is **physically unimplementable** in L2 scope (no `network_mode` config in `docker_backend.cpp:186` hardcoded `"NetworkMode", "bridge"`; L2 demo not running inside Docker). Full sandbox layer **deferred to Wave 4** (independent OpenSpec change)

---

### Requirement: context-request-schema-R13

L2 binary **MUST** accept `--context-file <path.jsonl>` flag with the 6 top-level + 4 metadata sub-fields schema per the scenarios below. L2 **MUST NOT** auto-generate ContextRequest.

> **来源**: 用户原话 "L2 只是提供了用户交互的设施, 具体还要用户提供一个具体上下文请求, 这个上下文请求创建的目标才能做 harness/自进化/rsi 的验证"
> **核心命题**: L2 是 **reference example 入口设施**, **不**是 autonomous evaluator. 任何 harness-rsi / data-rsi / model-rsi 验证**必须** 由用户提供 ContextRequest 触发, 不是 L2 demo 自身自动跑.

L2 binary **MUST** accept `--context-file <path.jsonl>` flag. Each line MUST be 1 ContextRequest JSON object with **6 top-level fields** (plus `metadata` object with **4 sub-fields**). L2 **MUST NOT** auto-generate ContextRequest.

Top-level fields:
- `context_id` (string, **user-defined UUID v4**, L2 does NOT auto-generate)
- `turn_input` (string, **non-empty**, L2 does NOT validate content — user full responsibility)
- `task_class` (enum string, **CLOSED ENUM** per P0-7 fix: `code_gen | research | summary | debug | classify | other` — `other` is fallback for extensibility)
- `expected_eval_quality` (optional string Acceptable|Poor|Excellent|null, **user-declared baseline expectation**, used in R8.2 failure traceability)
- `invocation_mode` (enum string, `mock | real_llm_deepseek | real_llm_custom`, default `mock`)
- `metadata` (object, see below)

`metadata` sub-fields:
- `domain` (optional string, e.g., "k8s", "auth")
- `tags` (optional array of strings)
- `is_hidden` (optional bool, default `false` — **P0 fix: accept into hidden bucket, NOT reject**, per E2 red-line public/hidden separation)
- `sensitivity` (optional enum string `public | internal | confidential`, default `public`)

### R13.4 Sensitivity Redaction Policy (per P2-4 fix 2026-09-23)

**L2 MUST redact trace output per sensitivity level**:

| sensitivity | trace.reduction |
|-------------|-----------------|
| `public` | **none** (all fields visible) |
| `internal` | redact `turn_input` + `response` (replace with `[REDACTED-internal]`); preserve `context_id` + `task_class` for debugging |
| `confidential` | redact `turn_input` + `response` + `expected_eval_quality` + `metadata.tags` + `metadata.domain` (replace with `[REDACTED-confidential]`); preserve only `context_id` + `task_class` + `metadata.sensitivity` for audit trail |

**Redaction implementation**: trace JSONL MUST emit `[REDACTED-<level>]` literal strings for redacted fields. The parser-side detection (`pdk_chat_demo_evolution::detail::redact_trace_fields(json, sensitivity)`) MUST be invoked BEFORE the field is serialized to stdout. Reentrant invariants:

- L2 MUST NOT write redacted trace fields to disk (per R13.5)
- L2 MUST NOT include redacted fields in `metrics.json` / `ablation_report.json`
- The `hidden_context_accepted_info` event payload MUST be sensitivity-aware (redact payload fields per same policy)

**NOTE**: This policy applies to L2 trace output. **Real LLM provider call payload is NOT redacted by L2** — that's the provider's responsibility (per `LLMConfig.api_key` redaction in main demo path).

#### Scenario: r13-1-1-missing-context-id-reject

- **WHEN** ContextRequest JSONL line is missing `context_id` field
- **THEN** L2 MUST exit non-zero
- **AND** stderr MUST include line number + field name

#### Scenario: r13-1-2-empty-turn-input-reject

- **WHEN** ContextRequest `turn_input` is empty string
- **THEN** L2 MUST exit non-zero
- **AND** stderr MUST include `context_id` + line number

#### Scenario: r13-1-3-task-class-not-in-closed-enum-reject

- **WHEN** ContextRequest `task_class` is not in closed enum `code_gen | research | summary | debug | classify | other`
- **THEN** L2 MUST exit non-zero
- **AND** stderr MUST include line number

#### Scenario: prefix-checks-run-before-closed-enum-validation

> **P0'-1 修复 (2026-09-23)**: 解决 R9.2 fixture (`mutation_metric_evaluation`) vs R13.1.3 闭枚举校验的优先级冲突 (per Metis DB-F Deal-breaker)

- **WHEN** ContextRequest `task_class` starts with reserved prefix `mutation_metric_*` (R9.2 prefix-rejection)
- **THEN** prefix detection MUST run BEFORE closed-enum validation
- **AND** L2 MUST reject with exit non-zero + emit `mutation_metric_rejected` event (per T0-1 ADR-0068 v2.4 amendment, see scenario `r9-2-prefix-rejection-and-grep-guard`)
- **AND** L2 MUST NOT invoke generic enum validation for prefix-reserved values
- **NOTE**: Reserved prefixes are explicitly carved out of the closed enum: `mutation_metric_*` (R9.2 detection prefix). Future R9.* modes may reserve additional prefixes by adding to this enumeration. The prefix reservation list MUST be the single source of truth in the parser implementation (`pdk_chat_demo_evolution::detail::RESERVED_TASK_CLASS_PREFIXES` or equivalent).

#### Scenario: r13-1-5-valid-contextrequest-accepted-with-context-id-in-trace

- **WHEN** valid ContextRequest is parsed
- **THEN** L2 MUST accept
- **AND** trace JSONL MUST include `meta.context_id` from ContextRequest

#### Scenario: r13-4-is-hidden-true-accept-into-hidden-bucket

- **WHEN** ContextRequest `metadata.is_hidden=true`
- **THEN** L2 MUST **accept** (NOT reject) the ContextRequest (P0 fix from original design that rejected)
- **AND** event MUST be routed to **hidden bucket** (not in public-set metrics)
- **AND** trace MUST include `meta.is_hidden=true` + `meta.hidden_bucket=true` dual fields
- **AND** `hidden_context_accepted_info` event MUST be emitted (per T0-1 ADR-0068 v2.4 amendment)
- **NOTE**: P0 fix addresses Metis §6 deal-breaker — original "reject" semantic conflicts with E2 red-line (public/hidden SEPARATE evaluation, NOT reject). rsi §11.8.8.2 "is_hidden=true must explicitly acknowledge" semantics.

#### Scenario: r13-3-three-class-contextrequest-empiric

L2 ship MUST include 3 reference ContextRequest files:

- `examples/pdk_chat_demo_evolution/fixtures/contexts/code-class-context.jsonl` (K8s YAML / Python test, ≥2 entries)
- `examples/pdk_chat_demo_evolution/fixtures/contexts/research-class-context.jsonl` (literature summary / paper comparison, ≥2 entries)
- `examples/pdk_chat_demo_evolution/fixtures/contexts/debug-class-context.jsonl` (log analysis / performance tuning, ≥2 entries)

- **WHEN** `--context-file code+research+debug-3class.jsonl` is run with all 3 classes
- **THEN** `--accept-contexts` flag MUST output 3-class `attribution_verdict` comparison (per C5 P0 fix: NOT eval_quality)
- **AND** `--release-metrics` MUST output ≥ 3-class comparison matrix (R8.1 mechanism demonstration)

#### Scenario: r13-2-zero-hardcode-无默认

- **WHEN** L2 binary starts WITHOUT `--context-file` flag
- **THEN** MUST exit non-zero (S28)
- **AND** NO default ContextRequest MUST be provided
- **AND** all ContextRequests MUST come from user-provided JSONL

#### Scenario: r13-6-l2-and-main-demo-coexist

- **WHEN** both `examples/pdk_chat_demo/pdk_chat_demo` AND `examples/pdk_chat_demo_evolution/pdk_chat_demo_evolution` are present
- **THEN** both binaries MUST coexist with different entry points
- **AND** main demo MUST NOT have `--context-file` flag (free chat)

---

## Cross-Doc References

| SoT 文档 | L2 实施后更新 |
|---------|--------|
| [`docs/architecture/self-evolution-architecture-2026-08.md`](../../../../architecture/self-evolution-architecture-2026-08.md) §十一 / §十二 | 加 "L2 ✅ ship" row + 6 段事件流注 + §十二 反向指标门引用 |
| [`docs/architecture/harness-architecture-2026-09.md`](../../../../architecture/harness-architecture-2026-09.md) §十一 / §十二 / §3.1 | 加 "L2 ✅ ship" row + V2 缺口闭环注 + §十二 5-tier gate 反向校验引用 + **§3.1 API 表面校正 (T0-2)** |
| [`docs/architecture/rsi-architecture-2026-09.md`](../../../../architecture/rsi-architecture-2026-09.md) §十一 / §十二 | 加 "L2 ✅ ship" row + D7 stub 端到端注 + §十二 真 RSI 三判据引用 |
| [docs/adr/adr-0068-event-emission-contract.md](../../../../adr/adr-0068-event-emission-contract.md) Appendix A | **v2.4 amendment (T0-1)**: 注册 `mutation_metric_rejected` + `turn_input_network_keyword_rejected` + `hidden_context_accepted_info` (per R9.2/R9.3/R13.4) |
| [`docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md`](../../../../roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md) §十 / §十一 | +1 row each (L2 ship + cross-doc consistency) |
| [AGENTS.md "Reverse Indicator Rule"](../../../../AGENTS.md) | commit `[Reverse Indicator]` 段必填 + R13 引用 |