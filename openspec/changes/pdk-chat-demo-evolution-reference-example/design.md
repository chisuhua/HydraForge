# pdk-chat-demo-evolution-reference-example — Design

> **Status**: 🔍 Proposed Design (PLACEHOLDER 待 rdd-builder 实施阶段细化)
> **关联 Proposal**: [`./proposal.md`](./proposal.md)
> **关联 Spec**: [`./specs/pdk-chat-demo-evolution/spec.md`](./specs/pdk-chat-demo-evolution/spec.md)

---

## 一、Context (上下文)

### 1.1 触发现状

本 change 直接源于用户决策 (2026-09-23):
> "用 pdk-chat-demo 作为承载例, 给三方 SoT 架构做可运行的 reference example"

+ 3 份 SoT 文档 (`self-evolution` v1.5 / `harness` v1.0 / `rsi` v1.0) 已 ship + §十一 traceback 已加 + 5 阶段 ship 实证就位.

### 1.2 核心缺口 (per Decision Record)

**`docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md` §3 第 6 项**:
> "变异必须经 IGenomeRegistry 持久化 (版本锚点存在)" — C4 GO 回注为 "GO with post-hoc closure gate (genome-wiring)" → **Wave 3 Phase 2 D7 "load(genome@N) → 重建 ChatSession → 1 turn" 待 L2 启动**.

本 change 是这个 post-hoc closure gate 的**首实例化**.

### 1.3 已 ship 但未端到端验证的接口

- `IGenomeRegistry::commit(genome)` (C2 ship)
- `apply_harness_mutation` — **5 参自由函数** `(GenomeMutations, string& system_prompt, vector<string>& tools, IToolRegistry&, MutationGateContext&)` per `harness_rsi.h:73-78`, **不是 "6 字段 interface / MutationGateChain 类"** (后者 grep 0 命中, design 早期笔误) — (C4 ship)
- `IGenomeRegistry::load(name, version)` (C3 follow-up + G4 wiring ship)
- `ChatConfig::override_provider` + `ChatConfig::override_system_prompt` (**2 method, not 5**, per `chat_session.h:173-176`) (Sprint 19 ship)
- `LLMProviderFactory::register_dynamic` (Wave 3 Phase 1 D7 ship)
- `IDistillationWriter` (ADR-0061-13 ship)
- `IEvaluator` V2 — `BehavioralEquivalenceEvaluator` + `CompositeEvaluator` (G2 wiring ship)
- `eval_quality` 来源于 `agenticdsl::evaluation::Quality` 枚举, **L2 不计算 eval_quality** (per C5 P0 fix: L2 是入口设施不是 evaluator); 输出 `attribution_verdict` = `Attributed|Confounded|Insufficient|NotAttempted` 来自 `BehavioralEquivalenceEvaluator` 已 ship 路径

L2 的**唯一任务**: 实例化一个真实 demo 把这些接口**端到端串起来** — 既验证接口可组合性, 又给三方 SoT 提供 reference example.

---

## 二、Goals & Non-Goals

### Goals

1. **G1**: 提供 `examples/pdk_chat_demo_evolution/` 独立 binary, 一键跑通 mock + 真实 LLM 双模式 (5 秒 mock / 30 秒 真实)
2. **G2**: 实例化 6 段端到端链 (启动 → session → baseline → mutation → reload → compare), 走完 5-tier gate
3. **G3**: 把 V2 端到端缺口 `load(genome@N) → 重建 ChatSession → 1 turn` 在 L2 主路径实现并测
4. **G4**: trace JSONL schema 稳定 (4 段事件 × 8 字段), 给 distill-source-survey 提供新 consumption 路径
5. **G5**: 三方 SoT 文档 §十一 同步标 "L2 已 ship"
6. **G6**: Backwards compatible — `examples/pdk_chat_demo/` 主体零改动

### Non-Goals

- **N1**: 不修改 `examples/pdk_chat_demo/` 主体 (单方向 reference, 不能 reverse-impact)
- **N2**: 不引入新公共 API (L2 仅消费现有 API)
- **N3**: 不接 Wave 3 Phase 2 (D4-D7 完整化, 独立立项)
- **N4**: 不实现 S4 Agent-Agent 协同进化 (research 路径)
- **N5**: 不修改三层合同 (Contract layer / EventBuilder / Genome CRD) — 全 freeze, L2 仅消费

---

## 三、High-level Design

### 3.1 组件布局

```
                          ┌────────────────────────────────┐
                          │ examples/pdk_chat_demo_evolution/ │
                          │  (本 change 新建, N1 = 零改动主 demo) │
                          │                                  │
   ┌─────────────┐       │  ┌──────────────────────────────┐ │
   │ main.cpp    │───────│─→│ context_request.{h,cpp}     │ │  ← Phase E4 NEW (R13)
   │ --mock      │       │  │ JSONL parser + schema 校验    │ │
   │ --real-llm  │       │  │ (S28-S31: 零 hardcode)       │ │
   │ --trace     │       │  └──────────────┬───────────────┘ │
   │ --context-file     │       │                 │                   │
   │              │       │                 ▼                   │
   └─────────────┘       │  ┌──────────────────────────────┐ │
                          │  │ evolution_session           │ │
                          │  │ 包装 ChatSession + 6 Agent   │ │
                          │  │ + Genome 操作               │ │
                          │  └──────────────┬───────────────┘ │
                          │                 │                   │
                          │                 ▼                   │
                          │  ┌──────────────────────────────┐ │
                          │  │ evolution_tracer             │ │
                          │  │ 订阅 IInteractionBus,        │ │
                          │  │ 序列化为 JSONL stdout         │ │
                          │  │ (含 meta.context_id, R13)    │ │
                          │  └──────────────┬───────────────┘ │
                          │                 │                   │
                          │  ┌──────────────┴───────────────┐ │
                          │  │ 6 段事件流 (按 proposal.md) │ │
                          │  │ baseline / mutation /        │ │
                          │  │ reload / compare / trace/    │ │
                          │  │ 每段 trace 含 context_id     │ │
                          │  └──────────────────────────────┘ │
                          └────────────────────────────────┘
                                          │
                                          ▼  外部调用 (5-tier gate)
                          ┌────────────────────────────────┐
                          │ src/evolution/harness_rsi.cpp  │  (C4 ship)
                          │ src/core/genome/registry_*     │  (C2 + G4 ship)
                          │ src/core/chat_session.cpp       │  (Sprint 19 ship)
                          │ src/modules/distillation/*      │  (ADR-0061-13 ship)
                          └────────────────────────────────┘
```

> **§3.1 与 R13 (上下文驱动契约) 的关系**: `context_request.{h,cpp}` 是 L2 的**唯一输入入口** (per spec R13.1). main.cpp 启动时 `--context-file <path.jsonl>` 必填, 无 flag → exit non-zero (S28). 6 段事件流的 turn_input 全部来自 ContextRequest, **不是** fixtures/golden_inputs.jsonl (那是 reference 示例, 供用户 clone + modify, 见 spec R13.2).

### 3.1.5 ContextRequest Flow (Phase E4, per spec R13)

```
[用户]  --context-file examples/pdk_chat_demo_evolution/fixtures/contexts/{code,research,debug}-class-context.jsonl
                                   │
                                   ▼
[0] ContextRequest::load(file)                    ← Phase E4 核心 (NEW)
     ├── JSONL parser (nlohmann::json)
     ├── **VALIDATION ORDER** (P0'-1 fix 2026-09-23, per Metis DB-F): prefix checks MUST run BEFORE closed-enum validation
     │   ├── Step 1 (Prefix Reserved, R9.*): detect reserved prefixes (`mutation_metric_*` etc.) → reject + emit dedicated event
     │   ├── Step 2 (R13.1.3): closed-enum validation on `task_class` field
     │   └── Step 3 (R13.1): schema validation on remaining fields (context_id/turn_input/expected_eval_quality/invocation_mode/metadata)
     ├── 无 --context-file → exit non-zero + stderr "ERROR: L2 零 hardcode..." (S28)
     ├── 字段缺失必填 → exit non-zero + 行号 + 字段名 (S29)
     ├── turn_input 为空 → exit non-zero + context_id + 行号 (S30)
     ├── **turn_input 含 baseline hint 字面串** (e.g., regex `the answer is \w+`) → 拒绝 + emit `hint_containment_rejected` (P0'-2 fix, R9.1 filter mechanism, parser-side detection)
     ├── metadata.is_hidden=true → 接受 + 进 hidden 桶 + emit hidden_context_accepted_info (P0 fix; 不再拒绝, E2 红线)
     ├── task_class 含 mutation_metric_* 前缀 → 自动拒绝 + emit mutation_metric_rejected (R9.2, Step 1 prefix-rejection, **runs BEFORE closed-enum validation**)
     ├── turn_input 含网络关键字 (e.g., "fetch http://") → 自动拒绝 + emit turn_input_network_keyword_rejected (R9.3 降级, defer to Wave 4)
     └── metadata.sensitivity=internal/confidential → 不写盘 + redact trace (H2 凭证隔离, redact 策略见 spec R13.4 Redaction Policy 表 + per P2-4 fix 2026-09-23)

**P0'-1 NOTE**: Reserved prefix list is single source of truth. `mutation_metric_*` is reserved for R9.2. Future R9.* modes reserve additional prefixes by adding to this list. The parser implementation MUST maintain a centralized list (`pdk_chat_demo_evolution::detail::RESERVED_TASK_CLASS_PREFIXES` or equivalent internal constant).

**P0'-2 NOTE**: `turn_input` baseline hint detection is a **parser-side regex** check (not LLM-mediated). Implementation: `std::regex hint_pattern(R"(the answer is \w+)")` matched against `turn_input`. If matched → reject + emit `hint_containment_rejected` event. This makes R9.1 test deterministic and CI-runnable without depending on LLM behavior.
                                   │
                                   ▼  每行 ContextRequest 驱动一轮 6 段链
[1..6] 原 6 段流程 (baseline/mutation/reload/compare)
    ├── turn_input ← ContextRequest[i].turn_input          ← 替换硬编码
    ├── trace.meta.context_id ← ContextRequest[i].context_id  ← 替换缺失字段
    └── ContextRequest[i] 全部处理后, exit 0
```

#### 3.1.6 ContextRequest Fixture 目录 (per R13.3 + T0-4 ship-time, P0-10)

**SHIPPED reference fixtures** (`examples/pdk_chat_demo_evolution/fixtures/contexts/`, 用户可 clone + modify, per R13.3 S32):

| 文件 | entries | 用途 |
|------|---------|------|
| `code-class-context.jsonl` | 2 (K8s YAML, Python pytest) | R13.3 code 类 reference |
| `research-class-context.jsonl` | 2 (Attention paper summary, CRDT comparison) | R13.3 research 类 reference |
| `debug-class-context.jsonl` | 2 (k8s RBAC log analysis, HTTP latency diagnosis) | R13.3 debug 类 reference |

**Test fixtures** (`examples/pdk_chat_demo_evolution/tests/fixtures/context_request/`, 内部, 驱动 10 test binary):

| 文件 | 驱动测试 | 设计意图 |
|------|---------|---------|
| `valid_3class_combined.jsonl` | `test_context_request_e2e` Case R13.3.2 / R13.3.3 | 3 类合并 ContextRequest, 验证 `--accept-contexts` 3 类对比 + R8.1 ≥ 3 类对照矩阵 |
| `valid_single_code.jsonl` | `test_context_request_e2e` Case R13.3.1 | 单类 ContextRequest, 验证"单类不构成 generalizable" 防御 |
| `is_hidden_true.jsonl` | `test_context_request_e2e` Case R13.3.4 | `metadata.is_hidden=true` → 接受进 hidden 桶 + emit `hidden_context_accepted_info` (P0 fix) |
| `anti_cheat_hint_input.jsonl` | `test_anti_cheat_search_solution` | R9.1 静态 string-containment 检测 — turn_input 含 baseline hint 字面串 "the answer is X" |
| `anti_cheat_metric_tampering.jsonl` | `test_anti_cheat_metric_tampering` | R9.2 prefix-rejection — `task_class: "mutation_metric_evaluation"` 触发 `mutation_metric_rejected` 事件 |
| `anti_cheat_network_keyword.jsonl` | `test_anti_cheat_sandbox_escape` | R9.3 降级 keyword-rejection — turn_input 含 "fetch http://" 触发 `turn_input_network_keyword_rejected` 事件 |
| `r8_failure_fixtures.jsonl` | `test_reverse_indicators` Case R8.3 | 3 entries (gate-violation / budget-exceeded / attribution-confounded), 供 R8.3 第 3 段"失败样本保留拦截率" ablation |
| `invalid_missing_context_id.jsonl` | `test_context_request_validation` Case R13.1.1 | 故意缺 `context_id` → exit non-zero + 字段名 |
| `invalid_empty_turn_input.jsonl` | `test_context_request_validation` Case R13.1.2 | `turn_input: ""` → exit non-zero + context_id + 行号 |
| `invalid_task_class_enum.jsonl` | `test_context_request_validation` Case R13.1.3 | `task_class: "unsupported_class"` (不在闭枚举) → exit non-zero + 行号 |
| `invalid_invocation_mode.jsonl` | `test_context_request_validation` Case R13.1.4 | `invocation_mode: "mock_undefined_provider"` (不在枚举) → exit non-zero |

**Total**: 3 SHIPPED + 11 test = **14 fixture files** (per `find examples/pdk_chat_demo_evolution -name '*.jsonl' | wc -l` = 14).

**Fixture 路径注入**: `tests/CMakeLists.txt` 用 `configure_file` 把 fixture 目录注入 test binary 的 runtime path (per `examples/pdk_chat_demo/tests/CMakeLists.txt:existing pattern`). 测试中以相对路径 `tests/fixtures/context_request/<name>.jsonl` 引用.

**Fixture 维护策略**:
- SHIPPED reference fixture: 任何用户可见的 schema 字段变更 (R13.1 enum 扩展等) MUST 同步更新
- Test fixture: 与对应测试 case 同步更新; 故意构造 negative case (invalid_*.jsonl) **不**修, 即使 parser 行为变 (test 应反映 parser bug, 而非 fixture 跟随)
- JSONL 格式硬约束: 每行 1 个完整 JSON object, 无 trailing comma, 编码 UTF-8

> **is_hidden 语义 (P0 fix + P2-3 MUST 统一)**: 原 design §3.1.5 "is_hidden=true → emit hidden_context_rejected + 拒绝" **与 E2 红线 (公开/隐藏集分离评测) 冲突** — 全拒绝则 E2 隐藏集永远无法演示 (per Metis §6 deal-breaker). **改**: `is_hidden=true` → 接受但进**独立 hidden 桶** (不进公开集指标); trace 同步 `meta.is_hidden=true` 标记 + `meta.hidden_bucket=true` 双字段. `hidden_context_rejected` 事件**不**在此路径发射 (改为 MUST emit `hidden_context_accepted_info` 事件 per spec R13.4 + P2-3 fix 2026-09-23 — spec 是权威, design 此前 "可选" 是文本残留, 已统一为 MUST; ADR-0068 v2.4 amendment, 见 tasks T0-2).

### 3.2 6 段端到端 (核心 happy path)

```
[用户]  ./run_evolution_demo.sh --mock --context-file examples/pdk_chat_demo_evolution/fixtures/contexts/code-class-context.jsonl
                                  │
                                  ▼
[0] ContextRequest::load(file)                        ← Phase E4 (R13, NEW)
    ├── JSONL parser + schema 校验 (S28-S31)
    └── contexts_ vector 填充 (每行一个 ContextRequest)
                                  │
                                  ▼
[1] main::main(argc, argv)
    args: --mock | --real-llm <provider> --trace-events --capture-mode={None|Training} --context-file <path>
                                  │
                                  ▼
[2] EvolutionSession::run(args, contexts_)
    ├── capture_mode = args.capture_mode (默认 **None**; --capture-mode training 启用 IDistillationWriter)
    ├── ChatSession ctor (**11 参真实签名**, per `chat_session.h:210-230`):
    │     ChatSession(
    │       engine.get(), bus, &engine->get_tool_registry(),
    │       config.agent, config.session,
    │       cancellation_registry,   // shared with loop_agent
    │       nullptr,                  // timer: D9 lazy
    │       std::make_unique<agenticdsl::StdinInputSource>(),
    │       std::make_unique<agenticdsl::StderrLogger>(),
    │       session_manager.get(),   // 观察者, 不持有
    │       std::nullopt              // ResumeToken
    │     );
    │   **Provider 不通过 ctor 参数** — 经 `engine->set_llm_provider(MakeMockLLM())` 注入 (per `pdk_chat_demo/main.cpp:337` reference)
    ├── subscribe evolution_tracer to IInteractionBus (per event_log W3.P1 主题)

[3] baseline_run(ContextRequest[i].turn_input)   // 来自 ContextRequest, NOT hardcoded fixture
    ├── ChatSession::chat(turn_input)  // per `pdk_chat_demo/main.cpp:617` reference
    ├── tracer.record({"phase":"baseline", "context_id":<i.context_id>, "response":<content>, ...})
    └── 返回 baseline_response

[4] apply_harness_mutation_free_fn(mutations, system_prompt, tools, registry, ctx)  // **5 参自由函数** per `harness_rsi.h:73-78`, **不是 MutationGateChain 类** (后者 grep 0 命中)
    ├── G0 syntax: MutationRequest grammar check
    ├── G1 policy: is_tool_allowed(meta, policy) + semantic_locked_tools + denied_tools
    ├── G2 load: IGenomeRegistry::load(parent_version) + judge_data_freshness
    ├── G2.5 partial-apply: collect AppliedMutation (prompt_snapshot + tools_snapshot)
    ├── G3 persist-before-apply: fork(name, parent_version, final_spec) + commit(genome) (失败零状态变更)
    ├── tracer.record({"phase":"mutation", "context_id":<i.context_id>, "gate_passes":[...], ...})
    └── 返回 AppliedMutation

[5] reload_and_rerun(ContextRequest[i].turn_input)  // **V2 缺口闭环**
    ├── IGenomeRegistry::load(name, version) → Genome
    ├── Genome::to_chat_config(loaded) → AgentConfig  (M3: **L2 内部 helper** `pdk_chat_demo_evolution::detail::to_agent_config` — 见 §5.3 placement, 不触碰 `include/`)
    ├── ChatSession ctor (新 session, 用新 AgentConfig + 同 bus + 同 registry)  ←──── **L2 V2 关键**
    ├── ChatSession::chat(turn_input)
    ├── tracer.record({"phase":"reload", "context_id":<i.context_id>, "genome_version":..., "response":<content>})
    └── 返回 post_mutation_response

[6] compare_traces(baseline, post_mutation)
    ├── BehavioralEquivalenceEvaluator (IEvaluator V2 已 ship)
    ├── attribution_verdict 字段 (Attributed/Confounded/Insufficient/NotAttempted) 来自 evaluator V2 已 ship 输出 (per C5 P0 fix: L2 **不**计算 eval_quality, **不**定义 Acceptance 阈值; 单 turn 下 Hotelling T² 必然 Insufficient/NotAttempted per ADR-0086 kMinBaselineSamples=5 — 这是诚实的统计输出, 不是 bug)
    ├── tracer.record({"phase":"compare", "context_id":<i.context_id>, "verdict":"approved|denied", attribution_verdict:<verdict>})
    └── 返回 CompareResult

[7] output JSONL stdout
    └── 4 段事件 (baseline + mutation + reload + compare), 各 8 顶层字段 + meta 内 **12 字段** (per P2-1 fix 2026-09-23, 与 spec R3 authoritative 一致: genome_version/gate_passes/eval_quality/attribution_verdict/trace_id/capture_mode/context_id/task_class/is_hidden/sensitivity/hidden_bucket/expected_eval_quality)
    └── 循环下一行 ContextRequest[i+1] → 回到 [3], 全部处理完后 exit 0
```

### 3.3 Trace JSONL Schema (稳定性保证)

**8 字段统一 schema** (per capture-mode-and-distillation-writer-v1 spec dist/record schema):

```json
{
  "phase": "baseline | mutation | reload | compare",
  "timestamp_iso8601": "2026-09-23T...",
  "session_id": "<uuid>",
  "turn_input": "<string>",
  "response": "<string | null>",        // baseline/reload 非空, mutation 为 null
  "tokens": 0,                          // int, real-LLM 模式
  "cost_usd": 0.0,                      // float, real-LLM 模式
  "meta": {
    "genome_version": "<name>@<version> | null",
    "gate_passes": ["G0","G1",...],     // mutation 段才有
    "eval_quality": "Acceptable|Poor|Excellent | null",  // compare 段
    "attribution_verdict": "Attributed|Confounded|Insufficient|NotAttempted | null",  // compare 段
    "trace_id": "<uuid>",
    "capture_mode": "None | Training",
    "context_id": "<uuid>",             // ← R13 (M2 修复): 来源于 ContextRequest, 必填
    "task_class": "<enum>",             // ← R13: 来源于 ContextRequest
    "is_hidden": <bool>,                // ← R13: 来源于 ContextRequest (E2 公开/隐藏集)
    "sensitivity": "<public|internal|confidential>"  // ← R13: 来源于 ContextRequest (H2 凭证隔离)
  }
}
```

> Schema 稳定性: **任何字段不能在未升档 spec.md 前删除**. 加字段可以 (向后兼容), 改字段名不可以 (破坏 consumer).
>
> **R13 集成 (2026-09-23 Oracle M2 修复 + P2-1 fix)**: trace JSONL **必含** `meta.context_id` (per spec R13.2 第 5 条 + S31). 顶层 8 字段不变 (phase/timestamp_iso8601/session_id/turn_input/response/tokens/cost_usd/meta), meta 内部 **12 字段** (per P2-1 fix, 与 spec R3 authoritative 一致): `genome_version` + `gate_passes` + `eval_quality` + `attribution_verdict` + `trace_id` + `capture_mode` (原 6 字段) + `context_id` + `task_class` + `is_hidden` + `sensitivity` (R13 新 4 字段) + `hidden_bucket` (R13.4 P0 is_hidden 重定位新增) + `expected_eval_quality` (R13.1 user-declared echo, P0-4 P0'-3 确认 L2 不计算).

---

## 四、Design Decisions

### D1: 不修改 pdk_chat_demo 主体 (单方向 dependency)

**理由**:
- 主 demo 是项目 reference implementation, 不应被 L2 演化的实验波动污染
- evolution demo 反而是 reference-for-evolution, 不污染 main release cadence

**实施** (per P1-3 fix 2026-09-23, per Oracle F5 / Metis F5):
- L2 仅 `examples/pdk_chat_demo_evolution/` 新增
- 在**根 `CMakeLists.txt` 的 `examples/` 分段** (line ~255 `add_subdirectory(examples/pdk_chat_demo)` 后) 加 `add_subdirectory(examples/pdk_chat_demo_evolution)` 一行 (per P0-8 fix: `examples/CMakeLists.txt` 不存在, examples 注册全部走根 CMakeLists.txt)
- main `examples/pdk_chat_demo/` 零改动 (0 diff line)

**拒绝的反方案**: 
- ❌ 在 `examples/pdk_chat_demo/main.cpp` 加 `--evolution` flag (会污染 main demo)

### D2: 通过 AgentConfig 字段赋值实例化 harness (不引新 contract)

**理由**:
- Sprint 19 ship 的 `ChatConfig::override_provider` + `ChatConfig::override_system_prompt` (**2 method, not 5**, per `chat_session.h:173-176`) 提供 system_prompt 维度的 override 接口
- `AgentConfig` 公开字段 `system_prompt` + `tools` + `budget_limit_usd` + `timeout_ms` + `max_steps` (per `chat_session.h:119-128`) 直接支持 L2 mutation 的 prompt/tools 装载 — **不**依赖 override_* 缺失方法
- L2 端到端 wire 已够用
- 不引新 contract = 不破坏 contract-layer freeze (per Self-Review Checklist)

**实施**:
- L2 mutation 实质 = `apply_harness_mutation` 的 L1 调用, 但 V2 reload 阶段用 `AgentConfig` 字段赋值实例化
- 也就是说: mutation 生成 `Genome`, Genome → AgentConfig 路径用 **L2 新代码 `pdk_chat_demo_evolution::detail::to_agent_config()`** (M3 修复 2026-09-23: C4 从未 ship `Genome::to_chat_config`, 全仓库 grep=0; L2 需新增 internal helper, 落点见 §5.3 — **内部 namespace**, 不触碰 `include/`)

**NOTE (P0 fix)**: 原 D2 声称 "Sprint 19 ship 的 `ChatConfig::override_*` 5 方法" + "`override_tools` 已具备完整接口" **是事实错误** — chat_session.h:173-176 实只有 2 个 override_*. 真实可 wire 路径 = AgentConfig 字段直接赋值 (公共字段), 已足够覆盖 L2 mutation 的 prompt_delta/tools_add 装载需求.

**拒绝的反方案**:
- ❌ 新增 `Genome::materialize(ChatSession*)` 公共 API (N2 = break contracts)
- ❌ 改 `ChatConfig` 加 `override_tools` 等 (会触碰 `include/`, 违 S16 R7 freeze)

### D3: 复用 ChatSession 主 demo 的 plugin 装载模式 (不复制代码)

**理由**:
- pdk_chat_demo main.cpp (`examples/pdk_chat_demo/main.cpp:189-337`) 已经过 12+ 集成测试验证 (per test_chat_session_*.cpp)
- L2 直接复用同一 ChatSession ctor pattern + plugin loader 注册 + DSLEngine setup, 不写新 wrapper
- **NOTE**: 原 design §3.2 [2] 称 "register 6 Agent: Chat / Loop / Provider / Session / Budget / FS / Shell" — 实际 main.cpp 没有独立 6 Agent 注册步骤; **plugin .so 由 PluginLoader 加载, Provider 通过 engine->set_llm_provider() 注入** (per `pdk_chat_demo/main.cpp:227-247` PluginLoader + `:337` set_llm_provider). L2 复用同模式.

**实施**:
- L2 的 `evolution_session.cpp::init()` 复用 `examples/pdk_chat_demo/main.cpp` 的初始化序列 (作为 `static` reference, 不 include main.cpp):
  - DSLEngine 构造 → `make_shared<InMemoryBus>` → `engine->set_interaction_bus(bus)`
  - `PluginLoader` 加载 `config.plugins[].path` (`.so`)
  - `provider/register` tool call
  - **mock 模式**: `make_unique<MockLLMProvider>()` → `engine->set_llm_provider()` (per `pdk_chat_demo/main.cpp:306-307` + `:337`)
  - **real-LLM 模式**: `provider/resolve` tool → `LLMProviderFactory::create()` → `set_llm_provider()` (per `pdk_chat_demo/main.cpp:308-334`)
- 严格对齐 pdk_chat_demo lifecycle (含 Skill 子进程早期分支 + T1.9 stale cleanup)

**拒绝的反方案**:
- ❌ L2 复制 pdk_chat_demo main.cpp plugin_loader 装载代码 (drift 风险)
- ❌ L2 include `examples/pdk_chat_demo/main.cpp` (跨树依赖 + 不必要)

### D4: capture-mode 三态可显式启用 (per distill-source-survey 推荐方案)

**理由**:
- distill-source-survey-2026-08 推荐 SessionWriter JSONL 作为过渡数据源
- L2 默认 capture-mode=None, `--capture-mode training` 启用 IDistillationWriter

**实施**:
- `event_handler.cpp::set_capture_mode(CaptureMode::Training)`
- 进 IInteractionBus 后事件自动 JSONL 化 + IDistillationWriter 写盘
- 与 ADR-0080 v1.2 amendment (CaptureMode 三态 + Training fail-open) 对齐

### D5: trace 输出默认 stdout (而非 file)

**理由**:
- 简单, 无 path 管理
- 与 Unix pipeline 友好 (可以 `./run_evolution_demo.sh | jq` 直接消费)
- 真实项目用 IDistillationWriter 写盘 (与 L2 无关, 是 ADR-0080 范畴)

**实施**:
- L2 `evolution_tracer.cpp::emit(json)` 直接 `std::cout << json << "\n"`
- `--trace-events` flag 启用, 默认 close (避免污染 `--mock` demo 输出)

### D6: mutation 默认是 prompt_delta + tools_add (不触发 workflow_patch V1 defer)

**理由**:
- `workflow_patch` L3 V1 defer (Oracle bg_1f291bc4 DEAL-BREAKER per `harness-rsi-pilot/spec.md` Scenario "workflow_patch NOT supported")
- L2 默认 mutation = `prompt_delta` + `tools_add` + `tools_remove` (L1+L2 安全 path, per `GenomeMutations` 4 字段定义)
- `workflow_patch` 路径**不在 L2 flag 表面暴露** (删除原 `--mutation-tier=L3` flag 设计 per P0 fix; 引用不存在的 flag 是死装饰 / 反 AI 实现质量). workflow_patch 完整化随 Wave 3 Phase 2 立项 (独立 OpenSpec change).

**NOTE (P0 fix)**: 原 D6 文本提到 "--mutation-tier=L3 flag" 但 GenomeMutations struct 实无 workflow_patch 之外的 tier 概念; 实施时该 flag 将引用不存在的 mutation tier — 删除该 flag, 避免设计/代码漂移.

### D7: Evaluation 嵌入现成 IEvaluator V2 — 输出 attribution_verdict 而非 eval_quality

**理由**:
- IEvaluator V2 ship (BehavioralEquivalence + Composite)
- L2 用现成 interface, 不重新发明 evaluation 字段

**实施** (per C5 P0 fix):
- `evolution_tracer.cpp::compare_phases()` 调现成 `IEvaluator` V2 实例, **输出 `attribution_verdict`** (Attributed/Confounded/Insufficient/NotAttempted, per ADR-0086 v1.1 + BehavioralEquivalenceEvaluator 已 ship 输出)
- **L2 不计算 eval_quality** (per rsi §11.8.1 "不能宣称反作弊 100% 防御" 边界 + Metis §6 deal-breaker "L2 是入口不是 autonomous evaluator")
- `eval_quality` 字段 (per `agenticdsl::evaluation::Quality` enum) **仅作为 user-declared `expected_eval_quality` 字段从 ContextRequest 原样 echo** 到 trace meta; L2 **不**做实际质量评分
- 单 turn baseline vs reload 下, BehavioralEquivalence.compare() 在 kMinBaselineSamples=5 (per ADR-0086) 限制下必返回 Insufficient/NotAttempted — 这是诚实的统计输出, 不是 bug; ≥5 样本统计断言 deferred 到 Wave 3 Phase 2 D5 (独立 OpenSpec change)

**拒绝的反方案**:
- ❌ L2 自造 eval_quality 简易评分器 (per Metis DB3: 必然 pass 自证型评估)
- ❌ 用 BehavioralEquivalence::evaluate(单 trace) 占位 Acceptable(0.5) 装作真评估 (是 `behavioral_equivalence_evaluator.h` 中占位实现, 非真实评分)

---

## 五、File-level Design

### 5.1 `examples/pdk_chat_demo_evolution/main.cpp`

```cpp
int main(int argc, char** argv) {
  // 1. 解析 args (复用 pdk_chat_demo cli_args_parser 部分)
  bool trace_events = false;
  std::string capture_mode_str = "None";  // 默认 None (训练模式用 --capture-mode training)
  std::string provider_str = "mock";       // --mock | --real-llm deepseek
  std::string context_file;                // Phase E4: --context-file <path.jsonl> (R13 必填)

  // 2. 加载 ContextRequest (Phase E4: R13 唯一输入入口, 零 hardcode)
  //    无 --context-file → exit non-zero (S28)
  if (context_file.empty()) {
    std::cerr << "ERROR: L2 零 hardcode, 必须提供 ContextRequest via --context-file <path.jsonl>"
              << std::endl;
    return 1;
  }
  auto contexts = context_request::load_context_file(context_file);
  if (contexts.is_error()) {
    std::cerr << "ERROR: " << contexts.error() << std::endl;
    return 1;
  }

  // 3. 实例化 EvolutionSession (封装 ChatSession + plugin_loader + tracer + contexts)
  EvolutionSession session(provider_str, capture_mode_str, trace_events);
  session.set_contexts(contexts.value());  // Phase E4: 注入 ContextRequest

  // 4. 跑 6 段端到端 (每行 ContextRequest 驱动一轮)
  return session.run_6_phase_demo();
}
```

**估行数**: 70-90 行 (含 ContextRequest 加载).

### 5.2 `context_request.{h,cpp}` (R13 唯一输入入口, Phase E4 NEW)

```cpp
// context_request.h — ContextRequest schema (per spec.md R13.1)
struct Metadata {
  std::string domain;                 // e.g., "k8s", "auth"
  std::vector<std::string> tags;      // e.g., ["baseline", "Wave-3-Phase-2-candidate"]
  bool is_hidden = false;             // 公开集/隐藏集分离 (E2 红线)
  std::string sensitivity = "public"; // public | internal | confidential (H2 凭证隔离)
};

struct ContextRequest {
  std::string context_id;                       // UUID v4, 用户定义 (L2 不生成)
  std::string turn_input;                       // 用户给 ChatSession 的输入
  std::string task_class;                       // code_gen | research | summary | debug | ...
  std::optional<std::string> expected_eval_quality;  // Acceptable|Poor|Excellent|null
  std::string invocation_mode = "mock";          // mock | real_llm_deepseek | real_llm_custom
  Metadata metadata;
};

// context_request.cpp — parser + schema 校验 (S28-S31)
Result<std::vector<ContextRequest>, std::string> load_context_file(const std::string& path);
//   - 无 path → Err("L2 零 hardcode, 必须提供 --context-file")        (S28)
//   - 字段缺失必填 → Err("<行号>: <字段名>")                          (S29)
//   - turn_input 空 → Err("<行号>: turn_input 空")                    (S30)
//   - is_hidden=true → 接受 + 进 hidden 桶 + emit hidden_context_accepted_info (E2 红线: 公开/隐藏集分离评测, **不**拒绝)
//   - mutation_metric_* turn_input → 自动拒绝 (R9.2/R13.4) — prefix-rejection, 见 P0-7 fix
//   - sensitivity=internal/confidential → 标记不写盘 + redact trace  (H2, per P2-4 fix 三档 redaction 策略: public=none / internal=redact turn_input+response / confidential=redact + tags+domain)

json to_trace_meta(const ContextRequest& req);
//   - 生成 meta.context_id + meta.task_class + meta.expected_eval_quality
//   - 生成 meta.is_hidden + meta.hidden_bucket + meta.sensitivity (redact 控制)
```

**估行数**: h (~60 行) + cpp (~120 行).

### 5.3 `evolution_session.{h,cpp}` (核心)

```cpp
class EvolutionSession {
public:
  EvolutionSession(provider_str, capture_mode_str, trace_events_bool);
  int run_6_phase_demo();  // 入口 (Phase 0..Phase E6 = 7 internal steps)
  void set_contexts(std::vector<ContextRequest> contexts);  // Phase E4: R13 输入

private:
  // 7 阶段方法 (Phase 0..Phase 6)
  void phase0_load_contexts();     // Phase E4: ContextRequest::load + 校验 (R13, S28-S31)
  void phase1_init();              // DSLEngine + InMemoryBus + PluginLoader + MockLLMProvider/real-LLM (per D3 pattern)
  void phase2_baseline(const ContextRequest&);  // ChatSession::chat + tracer.record (per pdk_chat_demo/main.cpp:617)
  void phase3_mutation(const GenomeMutations&, const ContextRequest&);  // apply_harness_mutation 5 参 free function (per harness_rsi.h:73-78)
  void phase4_reload_rerun(const GenomeVersion&, const ContextRequest&);  // V2 缺口闭环核心
  void phase5_compare(const ContextRequest&);  // IEvaluator V2 (BehavioralEquivalence) + tracer.record (attribution_verdict)
  void phase6_emit_jsonl();        // stdout (per D5)

  // Members
  std::unique_ptr<DSLEngine> dsl_;
  std::shared_ptr<agenticdsl::IInteractionBus> bus_;  // shared with ChatSession
  agenticdsl::IToolRegistry* registry_ = nullptr;     // borrowed from dsl_->get_tool_registry()
  std::shared_ptr<hydraforge::pdk::CancellationRegistry> cancellation_registry_;
  std::unique_ptr<ILLMProvider> provider_;            // owned, engine->set_llm_provider consumes via move
  std::unique_ptr<EvolutionTracer> tracer_;
  std::unique_ptr<IEvaluator> evaluator_;             // V2 实例
  std::vector<ContextRequest> contexts_;              // Phase E4: R13 输入集合
  size_t current_context_idx_ = 0;                    // Phase E4: 当前处理的行
  AgentConfig current_agent_config_;                   // 主 AgentConfig (mutation → reload 复用)
  std::optional<GenomeVersion> last_genome_;

  // L2 内部 helper (per D2/M3): Genome → AgentConfig, 不触碰 include/
  static AgentConfig genome_to_agent_config(const Genome& g);
};
```

**估行数**: h (~110 行) + cpp (~280 行).

### 5.3 `evolution_tracer.{h,cpp}` (输出)

```cpp
class EvolutionTracer {
public:
  EvolutionTracer();  // subscribe IInteractionBus 内部
  void enable(bool on);  // --trace-events flag
  void record_phase(event, const ContextRequest* req = nullptr);  // 4 段事件入口
private:
  bool enabled_;
  void emit_jsonl(const json&);  // D5: stdout
  json collect_meta(const ContextRequest* req);  // 8 字段统一 schema + context_id (R13)
};
```

**估行数**: h (~60) + cpp (~170).

### 5.4 测试 (10 个 binary / ~30 cases, Phase E4 R13 + R8 + R9 增量后, P0-7 修正计数)

- `tests/test_evolution_session_mutation.cpp` — Case 1: mock mutation → 期望 6 gate 全 pass (5 cases)
- `tests/test_evolution_session_load.cpp` — Case 2: V2 缺口 load(genome@N) → ChatSession ctor → 期望真实装载 (5 cases)
- `tests/test_distillation_capture_mode.cpp` — Case 3: capture-mode=Training → JSONL + IDistillationWriter 路径 (4 cases)
- `tests/test_evolution_tracer_schema.cpp` — Case 4 (R4/R13): trace 4 段事件 × 8 顶层 + meta 字段 (4 cases)
- `tests/test_reverse_indicators.cpp` — Case 5 (R8): drop_ratio + failure trace + ablation (3 cases)
- `tests/test_anti_cheat_search_solution.cpp` — Case 6 (R9.1 P0-3): 静态 string-containment 检测 (1 case)
- `tests/test_anti_cheat_metric_tampering.cpp` — Case 7 (R9.2 P0-7): ContextRequest prefix-rejection + grep 静态契约 (1 case)
- `tests/test_anti_cheat_sandbox_escape.cpp` — Case 8 (R9.3 P0-6): 降级 — ContextRequest keyword-rejection (1 case) [原 R9.3 sandbox 物理不可实现, defer to Wave 4]
- `tests/test_context_request_validation.cpp` — Case 9 (R13.1): schema 校验 S28-S31 (5 cases)
- `tests/test_context_request_e2e.cpp` — Case 10 (R13.3): ≥ 3 类 ContextRequest 实证 + is_hidden 接受进 hidden 桶 (4 cases)

每个 binary 1-5 cases / 6-15 assertions minimum. **总计 10 binary / ~30 cases / ~50 assertions** (per P0-7 修正).

**CMake LABELS (P0-7 fix)**: 每个 test 用 `LABELS "l2-evolution"` 标记; 验收命令 `ctest -LE l2-evolution` 排除全部 L2 测试, 验证 baseline 211 零回归. **原 `-E pdk_chat_demo_evolution` 正则子串不命中新测试名, 验收门失效 (per Oracle M6)**.

### 5.5 `CMakeLists.txt`

```cmake
add_executable(pdk_chat_demo_evolution
  main.cpp
  context_request.cpp      # Phase E4: R13 parser (NEW)
  evolution_session.cpp
  evolution_tracer.cpp
)
target_link_libraries(pdk_chat_demo_evolution PRIVATE
  agenticdsl_core
  agenticdsl_common
  pdk_chat_session         # ChatSession 所在 target (per pdk/chat_session/CMakeLists.txt)
)
target_include_directories(pdk_chat_demo_evolution PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# tests
if(AGENTICDSL_BUILD_TESTS)
  add_executable(test_evolution_session_mutation tests/test_evolution_session_mutation.cpp)
  add_test(NAME test_evolution_session_mutation COMMAND test_evolution_session_mutation)
  set_tests_properties(test_evolution_session_mutation PROPERTIES LABELS "l2-evolution")
  # ... 类比 9 个 test, 每个均 LABELS "l2-evolution"
endif()
```

---

## 六、Backwards Compatibility (向后兼容)

### 6.1 公共 API 兼容 (per chat_session.h:210-230 + harness_rsi.h:73-78 真实签名, P0-1 修正)

| API | 真实签名 | 影响 |
|-----|---------|------|
| `ChatSession` ctor (**11 参** 含默认, not "4 参") | `(DSLEngine*, shared_ptr<IInteractionBus>, IToolRegistry*, const AgentConfig&, const SessionConfig&, shared_ptr<CancellationRegistry>, ITimerService* = nullptr, unique_ptr<IInputSource> = nullptr, unique_ptr<ILogger> = nullptr, SessionManager* = nullptr, optional<ResumeToken> = nullopt)` per `chat_session.h:210-230` | **不变** (L2 调用 11 参完整签名; provider 走 `engine->set_llm_provider()` 注入, 非 ctor 参数) |
| `ChatConfig::override_*` (**2 method, not "5 方法"**) | `override_provider(provider, model)` + `override_system_prompt(overwrite, append)` per `chat_session.h:173-176` | **不变** (L2 调用; **不**依赖不存在的 override_tools/override_budget/override_model_routing/override_prompt_prefix) |
| `IGenomeRegistry::commit/load/fork/walk_ancestors` | (现成) | **不变** |
| `apply_harness_mutation` (**5 参自由函数, not "6 字段 interface / MutationGateChain"**) | `(const GenomeMutations&, std::string& system_prompt, std::vector<std::string>& tools, IToolRegistry&, const MutationGateContext&)` per `harness_rsi.h:73-78` | **不变** (L2 调用 5 参自由函数; **不**调用虚构类 `MutationGateChain` — grep 0 命中, design 早期笔误) |
| `LLMProviderFactory::register_dynamic` | (现成) | **不变** |
| `IDistillationWriter` 接口 | (现成) | **不变** |
| `IEvaluator` V2 接口 (BehavioralEquivalence + Composite) | (现成) | **不变** |

### 6.2 Binary 兼容 (P0-7 fix)

- `examples/pdk_chat_demo/pdk_chat_demo` binary 行为零变化 (N1 强制保证)
- 全量 ctest `-LE l2-evolution` (CMake LABELS 排除所有 `test_evolution_*` / `test_anti_cheat_*` / `test_context_request_*` / `test_distillation_*` / `test_reverse_indicators` 测试) baseline 211 零回归; L2 add **10 new test binaries** (per P0-7 fix: 6 → 10 binary, 与 spec §5.4 + §R8/R9/R13 一致). **NOTE**: 原 `-E pdk_chat_demo_evolution` 子串正则**不**命中新测试名, 验收门失效 — 必须用 LABELS 机制.

### 6.3 数据兼容

- 不改 `~/.hydraforge/genomes/` 现有 layout (C2 ship)
- L2 写盘的 genome 与 Wave 3 Phase 1 D7 stubs 完全兼容
- trace JSONL 是 L2 新增, 不写盘 (per D5)

---

## 七、Risks

| # | 风险 | 影响 | 缓解 |
|---|------|------|------|
| R1 | ChatSession ctor 与 ChatConfig 结合出现 lifecycle 问题 (C4 5-tier gate 已 ship, 但 V2 reload 路径未 ship) | V2 缺口闭环实现可能 wave 5-tier gate 已经处理, 但 reload → 重建 ChatSession 仍是新路径 (per Decision Record §3 第 6 项) | L2 严格 follow 5-tier gate + `--strict` mode (任何 gate fail → exit non-zero) |
| R2 | `--real-llm` 模式需要 DEEPSEEK_API_KEY (sandbox 无 key) | 真实 LLM 验证受限 | 默认 `--mock` + Gate 验证; 单 dev developer local 跑 `--real-llm` |
| R3 | D7 stub (Wave 3 Phase 1) 与 L2 stub provider 命名冲突 | 命名 alias 风险 | L2 用 `_demo_evolution_<uuid>` prefix 避免冲突 |
| R4 | L2 demo 跑通路径 5-tier gate 中, G2 load + G3 commit 都涉及 IGenomeRegistry::commit, 频繁操作 file system | Disk 写入锁 contention + **AGENTS.md 模式 #10 fresh-deploy 静默回归** | **P0'-4 fix (2026-09-23)**: L2 默认 `--mock` 与 `--real-llm` 双模式均使用 `FilesystemGenomeRegistry` (per `src/core/genome/registry_filesystem.cpp` 现成实现), **但强制 hermetic env fixture**: 测试入口 `setenv("HOME", "/tmp/l2-test-<uuid>", 1)` + `setenv("HYDRAFORGE_GENOME_DIR", "$HOME/.hydraforge/genomes", 1)` + `mkdtemp` 隔离 HOME; 这样既不引入新 `InMemoryGenomeRegistry` (破 N2 边界), 又保证测试不污染宿主机 `~/.hydraforge/genomes/` (修 AGENTS.md 模式 #10 hygiene-fix fresh-deploy 风险). mock 与 real-llm 唯一区别是 LLM provider, 不是 registry |
| R5 | trace JSONL 输出可能被误认为 IDistillationWriter 写盘 (混淆) | 用户误解 trace 行为 | README 明确: "L2 trace = stdout, IDistillationWriter = 单独 `(per distill-source-survey)` flag 启用" |

---

## 八、Test Strategy

### 8.1 测试设计 (TDD 5 步 + per AGENTS.md)

**RED (写 failing tests)**:
- `test_evolution_session_mutation` Case 1 mock apply_mutation → 期望 5-tier gate 全部 pass + chat session mutation 应用 + eval_quality OK
- `test_evolution_session_load` Case 2 mock load(genome@N) → 期望 ChatSession 重新构造 + 响应差异 (baseline vs reload)
- `test_distillation_capture_mode` Case 3 capture-mode=Training → 期望 JSONL 路径触发 + IDistillationWriter 写盘

**GREEN (minimal impl)**:
- 复用 5-tier gate + ChatConfig + IGenomeRegistry 现成接口
- **`genome_to_agent_config()` 是 L2 新代码** (M3 修复 2026-09-23): 全仓库 grep `to_chat_config` = 0 代码命中 (Oracle 2026-09-23 验证), C4 从未 ship 此方法. L2 需在 `evolution_session.cpp` (或 L2 内部 helper) 新增, **明示为 L2 新代码而非"现成"**. 落点: `pdk_chat_demo_evolution::detail::to_agent_config(const Genome&)` (内部 namespace, **不**触碰公共头). Genome struct (name/version/parent/created_by/capture_mode/spec.{harness,tools,...}) → AgentConfig (system_prompt + tools + budget_limit_usd + timeout_ms). **注意**: 原 D2 声称 "to ChatConfig" + "AgentConfig{ChatConfig{}}" — **API 表面失真** (per P0-1 fix): ChatConfig 没有 tools 字段 (tools 在 AgentConfig), ChatConfig 仅 system_prompt override. L2 落点是 **AgentConfig**, 不是 ChatConfig.

**REFACTOR (清理)**:
- 抽公共 helper (EvolutionTracer::record_phase)
- D5 JSONL 输出统一 schema (含 context_id per M2)

**Oracle dual-agent review**:
- Metis: 路径完整性 + 边界
- Oracle: 设计物理可行性 + ABI / V2 缺口闭合

**archive**:
- 完整 4-file integrity per AGENTS.md Day 5 lesson

### 8.2 验收场景 (per spec.md §R1 + §R13, P0 修正)

- **S1** (mock): `./run_evolution_demo.sh --mock --context-file examples/pdk_chat_demo_evolution/fixtures/contexts/code-class-context.jsonl` exit 0 + 4 段事件 + 8 顶层字段 + meta.context_id ✓
- **S2** (real-LLM): DEEPSEEK_API_KEY set, `--real-llm deepseek --context-file ...` 在 30 秒内生成 4 段事件 ✓
- **S3** (V2 缺口闭环): `phase4_reload_rerun` 真实装载 (load → `genome_to_agent_config` → ChatSession 重建) ✓
- **S4** (capture-mode=Training): trace JSONL + IDistillationWriter 写盘 1 个 record ✓
- **S5** (zero regression): `examples/pdk_chat_demo/main.cpp` `git diff` 0 行 ✓
- **S28-S31** (R13 零 hardcode): 无 --context-file → exit non-zero + stderr; 字段缺失 → exit non-zero + 行号; turn_input 空 → exit non-zero; trace 含 meta.context_id ✓
- **S32-S34** (R13 ≥ 3 类): `examples/pdk_chat_demo_evolution/fixtures/contexts/{code,research,debug}-class-context.jsonl` 3 reference file + `--accept-contexts` flag ✓
- **S35** (L2 与 main demo 并存): 入口不同, 互不影响 ✓
- **S36** (R8.1 drop_ratio 机制演示): `--release-metrics` flag 输出 metrics.json + drop_ratio > 5% → exit non-zero (注入人造 fixture 验证机制, 非真实回归度量, per C5 P0 fix)
- **S37** (R8.2 failure trace): 3 个 failure case 输出 trace 行 (`failure_event` + `rule_id` + `rule_shipped_commit` + `reproduce_in_new_task_demo` + `context_id`) ✓
- **S38** (R8.3 ablation 机制): `--ablation-mode=full` 跑 3 段对照 (随 L2 ship 3 个 failure fixture ContextRequest 作语料) ✓
- **S39** (R9.1 静态检测): `test_anti_cheat_search_solution` 断言"agent 输出 ⊆ 不含 hint 字面串" (per P0-3 fix: 静态 string-containment, 与 LLM 解耦, CI 可跑)
- **S40** (R9.2 prefix-rejection + grep): `test_anti_cheat_metric_tampering` 模拟 `mutation_metric_*` ContextRequest → L2 prefix 拒绝; `grep -c "set_.*metric" include/agenticdsl/contract/ievaluator.h` = 0 (静态契约守卫)
- **S41** (R9.3 keyword rejection 降级): `test_anti_cheat_sandbox_escape` 模拟含网络关键字 turn_input → L2 keyword 拒绝 + emit `turn_input_network_keyword_rejected` (per P1-4 fix 2026-09-23: 之前误写为 `hidden_context_accepted_info`, **正确事件名应为 `turn_input_network_keyword_rejected`** per spec R9.3 + tasks T0-1; 仅 keyword 层; 真 sandbox 物理不可实现, defer to Wave 4)

### 8.3 Test Strategy 与 R13 集成 (Phase E4)

**T2 阶段 (6 段端到端) 必须先加载 ContextRequest**:
- T2.1 RED `test_evolution_session_mutation` 使用 `ContextRequest` fixture (不是硬编码 turn_input)
- T2.4 GREEN `phase4_reload_rerun(const ContextRequest&)` 签名带 ContextRequest

**T3 阶段 (trace) 必须含 context_id**:
- T3.1 Case 3.1 trace 断言: 8 顶层字段 + meta **12 字段** (per P2-1 fix, 含 context_id/task_class/is_hidden/sensitivity/hidden_bucket/expected_eval_quality + genome_version/gate_passes/eval_quality/attribution_verdict/trace_id/capture_mode)

**T6.7-T6.10 (R13) 是 T2/T3 的输入源改造**:
- T6.7 test_context_request_validation (S28-S31)
- T6.8 context_request parser + --context-file flag
- T6.9 test_context_request_e2e (S32-S34)
- T6.10 examples/pdk_chat_demo_evolution/fixtures/contexts/ 3 reference file + R13.4 集成

---

## 九、Cross-doc Consistency (实施期同步)

**L2 实施期 commit 前**:
- [ ] `docs/adr/adr-0068-event-emission-contract.md` Appendix A v2.4 amendment → 新增 **4 主题**: `mutation_metric_rejected` (R9.2, **per P0'-1**: prefix check 先于闭枚举) + `turn_input_network_keyword_rejected` (R9.3 降级) + `hidden_context_accepted_info` (R13.4 is_hidden 重定位, per P2-3 统一为 MUST) + `hint_containment_rejected` (R9.1 parser-side hint detection, per P0'-2 fix, replaces vacuous `result.find(hint)` assertion), owner=`pdk_chat_demo_evolution`. 7 幻影主题教训: 不注册不发射.
- [ ] `docs/architecture/harness-architecture-2026-09.md` §3.1 → 修正 ChatSession ctor 4-arg → 11-arg 真实签名 + ChatConfig::override_* 5 method → 2 method (per P0-1 SoT sync)
- [ ] `docs/architecture/self-evolution-architecture-2026-08.md` §十一 → 加 "L2 已 ship 行"
- [ ] `docs/architecture/harness-architecture-2026-09.md` §十一 → 加 "L2 已 ship 行"
- [ ] `docs/architecture/rsi-architecture-2026-09.md` §十一 → 加 "L2 已 ship 行"

**L2 实施期 commit 后**:
- [ ] `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §十 Drift Log → "+1 L2 row"
- [ ] `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §十一 Adjustment Log → "+1 L2 row"
- [ ] `docs/README.md` 索引表 (如有 reference 一段) → 新增 L2 行

---

## 十、Linkage

- **SoT (上游)**:
  - [`docs/architecture/self-evolution-architecture-2026-08.md` v1.5 §十一 + §十二](../../architecture/self-evolution-architecture-2026-08.md) (v1.5 + Verification Matrix 反向指标门 + 反作弊)
  - [`docs/architecture/harness-architecture-2026-09.md` v1.0 §十一 + §十二](../../architecture/harness-architecture-2026-09.md) (v1.0 + Verification Matrix 5-tier gate 反向校验)
  - [`docs/architecture/rsi-architecture-2026-09.md` v1.0 §十一 + §十二](../../architecture/rsi-architecture-2026-09.md) (v1.0 + Verification Matrix 真 RSI 三判据 + R8/R9 引用)
- **决策 (上游)**:
  - [`docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md` §3 第 6 项](../../audits/2026-09-21-harness-rsi-pilot-go-no-go.md) (post-hoc closure gate)
  - [`docs/architecture/pdk-chat-demo-distill-source-survey-2026-08.md`](../../architecture/pdk-chat-demo-distill-source-survey-2026-08.md) (推荐数据源)
- **下游 (本 change 创生)**:
  - `openspec/specs/pdk-chat-demo-evolution/spec.md` (R1-R9 需求: R1-R7 主需求 + R8 反向指标门 + R9 反作弊测试)
  - `openspec/changes/pdk-chat-demo-evolution-reference-example/tasks.md` (TDD 5 步 tasks + T6 R8/R9)

### 10.7 R8 反向指标门 + R9 反作弊测试设计 (2026-09-23 升级, P0 修正定位)

**R8 Reverse Indicator Gate (per spec.md §R8)** — **机制演示 + 静态契约守卫** (per C5 P0 fix + Metis §6 deal-breaker, **不**声称对 R8 红线能力的真实度量):
- L2 主流程需暴露 `--release-metrics` + `--regression-test-suite` + `--ablation-mode=full` 三个 flag
- 双向指标 (R8.1) + 失败可追溯 (R8.2) + 消融实验 (R8.3) 三档必输出
- drop_ratio > 5% 自动 block (R8.1 红线) — **机制可测** (注入人造 fixture 时确实 exit non-zero); 真实回归度量 **不** 属 L2 demo 范围 (项目无失败样本语料, 缺 baseline corpus)
- 消融第 3 段"旧失败样本保留拦截率"数据源 = 随 L2 ship 的 3 个 failure fixture ContextRequest (T6.10 顺手产出, per Oracle M4)

**R8.3 ablation 输出字段 (per P0'-3 fix 2026-09-23, per spec R8.3 scenario)**:
- Segment 1: `attribution_verdict` 分布 (per ADR-0086 v1.1 4 态) + `response_edit_distance` (Levenshtein normalized)
- Segment 2: `attribution_verdict` consistency across task_class (≥3 类 ContextRequest 对比矩阵)
- Segment 3: `expected_eval_quality` (user-declared, **L2 不计算** per P0-4 C5) retention rate against r8_failure_fixtures.jsonl 3 entries
- **禁止**: L2-computed `eval_quality` field (与 P0-4 D7 attribution_verdict-only 输出矛盾, Metis M-D deal-breaker 修复)

**R9 Anti-Cheat Test Suite (per spec.md §R9)** — **降级版机制演示** (per Oracle C1/C2 + Metis §6 deal-breaker):
- **R9.1 搜现成答案 (P0-3 fix)**: 改**静态 string-containment 检测** — 断言"agent 输出 ⊆ 不含 hint 字面串", 与 LLM 解耦, CI 可跑. **不**用 eval_quality diff (mock 模式下恒 PASS 不测任何东西, 真 LLM 模式 CI 必被 HYDRAFORGE_SKIP_REAL_LLM=1 短路). 同时解决 R9.1 正文 vs S23 方向矛盾.
- **R9.2 修改评判指标 (P0-7 fix)**: 改 **ContextRequest prefix-rejection** (`mutation_metric_*` 前缀拒绝机制) + **`grep -c "set_.*metric" include/agenticdsl/contract/ievaluator.h` = 0 静态契约守卫** (100% 确定性 PASS in CI). **不**用 "Gate 1 fail" 断言 (GenomeMutations 无篡改字段, vacuous test).
- **R9.3 串谋外部平台 (P0-6 fix)**: **降级** — ContextRequest turn_input keyword-rejection (e.g., "fetch http://...") + spec 明示"不覆盖 sandbox 层". **真 sandbox 物理不可实现** (无 network_mode 配置项, hardcoded "bridge"; L2 demo 不在 sandbox 内运行). 真实 sandbox 层需求 **defer to Wave 4** (独立 OpenSpec change).

**Cross-doc consistency**: R8/R9 与 3 份 SoT 文档 §十二 Verification Matrix 同步, 任何 change 在 SoT §十二 + L2 spec.md 必须双向引用. **R8/R9 在 L2 定位** = "机制可演示 + 静态契约可守卫" + "spec 文本避免 '验证 R9 三模式' 等过强措辞" (与 rsi §11.8.1 "不能宣称反作弊 100% 防御" 治理边界一致).

- **Roadmap**:
  - [`docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §五 Sprint 36](../../roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md) (V2 L2 立项窗口)

---

**Design phase 0**: 设计完整, 待 rdd-builder P1 (plan gen) + P2 (execute) 进入实施. 不需要 Phase 0 case 1 - 4 另起 (per ADR-0049 auto-decision complex branch).
