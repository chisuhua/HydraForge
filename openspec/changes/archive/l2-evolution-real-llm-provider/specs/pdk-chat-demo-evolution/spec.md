# pdk-chat-demo-evolution Specification Delta (l2-real-llm-provider)

## Purpose

本 delta spec 修改 canonical `pdk-chat-demo-evolution` capability (per
`openspec/specs/pdk-chat-demo-evolution/spec.md`),把 `--real-llm <provider>`
模式从 stub 升级为真实 LLM provider 接线,补 L2 reference example 在真实 LLM
(DeepSeek / MiniMax) 下的端到端闭环。

Supersedes scope: 仅 `--real-llm` 实施层 (当前是 stub);mock 模式
(`--mock`) 行为完全不变。

Pre-existing related work (已 ship, 不在本 delta 范围):
- `2026-09-24-pdk-chat-demo-evolution-reference-example` archive (8 commits)
- `2026-09-26-l2-evolution-real-execution-chain` archive (Phase A+B: phase1-2 real wiring)
- `2026-09-26-l2-evolution-finalization` archive (Phase C: phase3-5 mock wiring + R13.4 + housekeeping)
- `2026-09-30-fix-parser-template-args` archive (parser + executor string arguments)

---

## ADDED Requirements

### Requirement: real-llm-provider-construction

`pdk_chat_demo_evolution::EvolutionSession` ctor MUST 接受 `real_llm` provider 名并 MUST 调 `agenticdsl::LLMProviderFactory::create(provider, api_key_env)` 注入真实 LLM provider,NOT 仅存储字符串。API key MUST 来自 `real_llm_env.h` helper (per `tests/test_helpers/real_llm_env.h::real_llm_config()`),helper 三态: `HYDRAFORGE_SKIP_REAL_LLM=1` → 不可用于 real-llm mode (exit non-zero); `DEEPSEEK_API_KEY` set → provider = "deepseek", url = `https://api.deepseek.com`; `MINIMAX_API_KEY` set (with non-placeholder URL) → provider = "minimax"。

#### Scenario: deepseek-real-llm-mode-runs-6-phase-demo

- **WHEN** `DEEPSEEK_API_KEY` is set AND binary called with `--real-llm deepseek --context-file ...`
- **THEN** EvolutionSession MUST instantiate `DeepseekProvider` via `LLMProviderFactory::create("deepseek", "DEEPSEEK_API_KEY")`
- **AND** 6-phase demo MUST run with real LLM responses in trace JSONL
- **AND** `--release-metrics` MUST write real drop_ratio to `/tmp/l2-metrics.json` (or `--metrics-output` path)
- **AND** binary MUST exit 0 within 30 seconds per canonical `standalone-binary-and-dual-mode` requirement

#### Scenario: minimax-real-llm-mode-fail-fast-on-placeholder-url

- **WHEN** `MINIMAX_API_KEY` is set AND binary called with `--real-llm minimax`
- **AND** `MINIMAX_API_URL` env var NOT set OR points to placeholder (`api.minimax.chat`)
- **THEN** EvolutionSession MUST exit non-zero with stderr "ERROR: MINIMAX_API_URL placeholder, see AGENTS.md §G2"
- **AND** MUST NOT call placeholder URL (NXDOMAIN fail-fast)

#### Scenario: missing-api-key-exit-nonzero

- **WHEN** binary called with `--real-llm deepseek` AND `DEEPSEEK_API_KEY` is NOT set AND `HYDRAFORGE_SKIP_REAL_LLM` is NOT set
- **THEN** EvolutionSession MUST exit non-zero with stderr "ERROR: real LLM requires DEEPSEEK_API_KEY or HYDRAFORGE_SKIP_REAL_LLM=1"
- **AND** MUST NOT mock fallback silently (real-llm mode = explicit user intent)

#### Scenario: invalid-provider-name-exit-nonzero

- **WHEN** binary called with `--real-llm <unknown-provider>`
- **THEN** EvolutionSession MUST exit non-zero with stderr "ERROR: unknown provider <name>, valid: mock, deepseek, minimax"
- **AND** MUST NOT fall back to mock provider (silent failure)

---

### Requirement: l2-real-llm-drop-ratio-r8-red-line

Real LLM 模式下 `--release-metrics` MUST 计算真实 `drop_ratio` (NOT `-1.0` mock 哨兵),并 MUST 按 R8.1 红线 `drop_ratio > 5% → exit non-zero` 触发 gate。

#### Scenario: real-drop-ratio-computed-from-baseline-vs-mutated

- **WHEN** 6-phase demo runs in real LLM mode AND `--release-metrics` enabled
- **THEN** `drop_ratio` MUST equal `mutated_failures / max(1, mutated_passes + mutated_failures)` per finalization C4 formula
- **AND** metrics.json MUST contain `drop_ratio` field as float (NOT `-1.0` mock sentinel)

#### Scenario: real-drop-ratio-gt-5-percent-exit-nonzero

- **WHEN** real LLM mode produces `drop_ratio > 0.05` (e.g., 8/100 mutations failed)
- **THEN** binary MUST exit non-zero (R8.1 red-line)
- **AND** stderr MUST print "[R8.1] drop_ratio=X.XX exceeds 5% threshold, blocking ship"
- **AND** triggering `context_ids` MUST be in metrics.json for traceability

#### Scenario: real-drop-ratio-le-5-percent-exit-zero

- **WHEN** real LLM mode produces `drop_ratio <= 0.05` (e.g., 2/100 mutations failed)
- **THEN** binary MUST exit 0
- **AND** metrics.json MUST contain `drop_ratio` + `context_ids` + `attribution_verdict` distribution

---

### Requirement: hermetic-home-compatibility-with-real-llm

Real LLM mode MUST 兼容 L2 `setup_hermetic_home()` (per `2026-09-26-l2-evolution-finalization/specs/pdk-chat-demo-evolution/spec.md` hermetic-home-composition requirement)。API key MUST 来自 env var (NOT hermetic HOME 内的 credential file),避免 HMAC key path collision。

#### Scenario: real-llm-mode-with-hermetic-home-no-collision

- **WHEN** real LLM mode runs with `setup_hermetic_home()` pre-configured
- **THEN** LLM provider API key MUST be read from `DEEPSEEK_API_KEY` env var directly
- **AND** MUST NOT access `~/.hydraforge/credentials` or similar (would conflict with hermetic HOME)
- **AND** HMAC key path MUST remain under hermetic root (per finalization hermetic-home-composition)

---

## MODIFIED Requirements

### Requirement: standalone-binary-and-dual-mode (delta)

`--real-llm <provider>` 标志行为 MUST 修改 (per `real-llm-provider-construction` 新 requirement): `--real-llm deepseek` MUST instantiate DeepseekProvider via `LLMProviderFactory::create()`, 6-phase demo runs with real LLM; `--real-llm minimax` MUST exit non-zero if URL placeholder (per `real-llm-provider-construction` Scenario 2); `--real-llm <unknown>` MUST exit non-zero (per Scenario 4); `--real-llm` without value MUST exit non-zero (parse error).

Other 8 flags behavior unchanged: `--mock`, `--capture-mode`, `--trace-events`,
`--context-file`, `--release-metrics`, `--regression-test-suite`,
`--ablation-mode=full`, `--accept-contexts`.

#### Scenario: real-llm-mode-emits-real-llm-responses-in-trace

- **WHEN** `DEEPSEEK_API_KEY` is set AND `--real-llm deepseek --trace-events --context-file ...`
- **THEN** trace JSONL MUST contain real LLM response text (NOT `"Mock evolution response"` sentinel)
- **AND** `response.tokens` field MUST be > 0 (real LLM token count, not mock 1)
- **AND** `response.cost_usd` field MUST be > 0 (real cost, not 0.0)
