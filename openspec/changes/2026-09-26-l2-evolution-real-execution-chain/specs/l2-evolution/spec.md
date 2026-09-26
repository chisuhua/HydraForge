# l2-evolution — Spec

> **Spec for**: `l2-evolution-real-execution-chain`
> **Supersedes**: `archive/2026-09-24-pdk-chat-demo-evolution-reference-example/specs/pdk-chat-demo-evolution/spec.md` (partial — real execution chain)

---

## R1: Dual-mode provider

The binary MUST support both `--mock` and `--real-llm <provider>` modes.
In mock mode, phase1_init MUST construct a `LLMProviderFactory::create(LLMConfig{"mock"})`
and inject it into DSLEngine via `set_llm_provider()`.
In real-LLM mode, construct with the specified provider name.

### Scenario: S1.1 --mock provider
- **WHEN** `--mock` flag is passed
- **THEN** phase1_init creates a MockLLMProvider and `ChatSession::chat()` returns instantly with echo of system_prompt
- **THEN** trace JSONL shows `"tokens"` > 0 (mock provider returns total_steps + tokens ≈ 1)

### Scenario: S1.2 --real-llm provider
- **WHEN** `--real-llm deepseek` flag is passed
- **THEN** phase1_init constructs CloudLLMAdapter via LLMProviderFactory
- **THEN** `ChatSession::chat()` makes real HTTP call (requires DEEPSEEK_API_KEY env)

---

## R2: 6-Phase chain — REAL wiring (not facade)

Each phase MUST have concrete operations, not hardcoded trace data.

### R2.1 phase1_init
- Construct `DSLEngine` (default ctor)
- `set_interaction_bus(bus_)`
- `LLMProviderFactory::create()` → `set_llm_provider()`
- `ChatSession` 11-param ctor with `AgentConfig` and `SessionConfig`

### R2.2 phase2_baseline
- `chat_session_->chat(turn_input)` with real response capture
- Trace fields: `response` from `ChatResult.response`, `tokens` from `ChatResult.total_tokens`, `cost_usd` from `ChatResult.cost_usd`

### R2.3 phase3_mutation
- Call `apply_harness_mutation()` 5-param free function
- Use `FilesystemGenomeRegistry` with hermetic HOME
- Trace `genome_version` from `AppliedMutation.committed_genome_version`

### R2.4 phase4_reload_rerun
- `IGenomeRegistry::load()` → `genome_to_agent_config()` → `ChatSession` rebuild → `chat()` rerun
- Trace `response` from rerun result (NOT hardcoded)

### R2.5 phase5_compare
- `IEvaluator::compare(before, after)` → `attribution_verdict`
- Verdict ∈ {Attributed, Confounded, Insufficient, NotAttempted}

### R2.6 phase6_emit_jsonl
- Flush remaining trace events received from tracer

---

## R8: --release-metrics (REAL computation)

The `--release-metrics` flag MUST:

### R8.1 drop_ratio computation
- Collect: `baseline_total` = number of contexts processed
- Collect: `baseline_failures` = number of baseline phases where `ChatResult.success == false`
- Collect: `mutated_passes` = number of compare phases where `attribution_verdict == "Attributed"`
- Compute: `drop_ratio = (baseline_failures - mutated_passes) / baseline_total`
- Write `metrics.json` with `{baseline_total, baseline_failures, mutated_passes, drop_ratio, original_drop_ratio: 0.0, new_up, old_down}`
- If `drop_ratio > 0.05` → exit non-zero

### Scenario: S8.1 --release-metrics with normal run
- **WHEN** `--release-metrics --mock --context-file <file>` runs
- **THEN** `metrics.json` is created in CWD
- **THEN** `metrics.json["drop_ratio"]` is a number (possibly 0.0)
- **THEN** file contains `baseline_total`, `baseline_failures`, `mutated_passes` fields

---

## R13.4: Sensitivity redaction (NEW)

ContextRequest with `metadata.sensitivity` set to non-public MUST have their trace fields redacted.

### Redaction policy

| sensitivity level | turn_input | response | meta.tags | meta.domain |
|-----------------|------------|----------|-----------|-------------|
| none/public     | passthrough | passthrough | passthrough | passthrough |
| internal        | [REDACTED-internal] | [REDACTED-internal] | passthrough | passthrough |
| confidential    | [REDACTED-confidential] | [REDACTED-confidential] | [REDACTED-confidential] | [REDACTED-confidential] |

### Scenario: S13.4.1 confidential redaction
- **WHEN** a ContextRequest has `metadata.sensitivity = "confidential"`
- **THEN** trace JSONL for that context has `"turn_input": "[REDACTED-confidential]"`
- **THEN** trace JSONL has `"response": "[REDACTED-confidential]"`
- **THEN** `meta` has `"[REDACTED-confidential]"` for tags/domain fields

### Scenario: S13.4.2 public passthrough
- **WHEN** a ContextRequest has `metadata.sensitivity` absent or `"public"`
- **THEN** trace JSONL is NOT redacted (original values preserved)

---

## Cross-doc consistency

Three SoT documents MUST carry a "L2 ✅ ship" row with the real-execution-chain change:

| 文档 | §十一级别 | 行内容 |
|------|-----------|--------|
| harness-architecture-2026-09.md | §十一 | L2 reference example (real execution chain) ✅ ship 2026-09-26 |
| rsi-architecture-2026-09.md | §十一 | L2 reference example (real execution chain) ✅ ship 2026-09-26 |
| self-evolution-architecture-2026-08.md | §十一 | L2 reference example (real execution chain) ✅ ship 2026-09-26 (校订/新增) |

---