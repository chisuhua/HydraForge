# Wave 3 Fine-tune Base Model Pilot Phase 1 Specification

## ADDED Requirements

### Requirement: ADR-0078 状态翻牌 (AC-1)

The `docs/adr/adr-0078-finetune-base-model.md` MUST flip its status row from 🔍 Proposed → ✅ Approved (Wave 3 Phase 1 Pilot 激活), remove the "Wave 5+ descoped" label from the header, and add a Phase 1 容量评估 paragraph (referencing Pre-Wave3 Plan §3 + D1/D3/D7 边界决策表).

#### Scenario: ADR-0078 状态行翻牌

- **WHEN** 运行 `grep "^## 状态" -A 2 docs/adr/adr-0078-finetune-base-model.md`
- **THEN** 状态行含 `✅ Approved (Wave 3 Phase 1 Pilot 激活, 2026-09-23)` 且不含 "Wave 5+ descoped"

#### Scenario: Phase 1 容量评估段落存在

- **WHEN** 运行 `grep "## Phase 1 容量评估" docs/adr/adr-0078-finetune-base-model.md`
- **THEN** 段落存在且包含 D1/D3/D7 Phase 1 vs Phase 2 边界决策表

### Requirement: OpenSpec Change 5 文件完整 (AC-2, AC-3)

The change `wave-3-finetune-base-model-pilot-phase1` MUST contain 5 files (`.openspec.yaml` + `proposal.md` + `design.md` + `tasks.md` + `specs/wave-3-finetune-base-model/spec.md`) and pass `openspec validate --strict`.

#### Scenario: openspec validate 通过

- **WHEN** 运行 `openspec validate wave-3-finetune-base-model-pilot-phase1 --strict`
- **THEN** 输出含 "Change is valid", exit code 0

### Requirement: D1 基模选型评分 yaml 持久化 (AC-4)

The `docs/research/wave-3-base-model-selection.md` MUST contain ≥3 候选模型 4 维度评分 (Capability 30% / Latency 20% / Cost 20% / Openness 30%), 至少 1 个 Weighted ≥ 7.5, 4 过滤条件全过 (Capability ≥ 8.0, Openness ≥ 5.0, Cost ≤ $1/1M, Latency P95 ≤ 100ms/token), 且给出最终选择 + 论证.

#### Scenario: 候选模型数 ≥ 3

- **WHEN** 运行 `grep -c "name:" docs/research/wave-3-base-model-selection.md`
- **THEN** 输出 ≥ 3

#### Scenario: 最终选择 Weighted ≥ 7.5 且过滤全过

- **WHEN** 运行 `grep "weighted_score" docs/research/wave-3-base-model-selection.md`
- **THEN** 最终选择模型的 weighted_score ≥ 7.5 且 4 过滤条件 (capability/openness/cost/latency) 全部通过

### Requirement: D3 训练数据准备脚本 (AC-5)

The `scripts/prepare_training_data.py` MUST consume ADR-0074 D6 JSONL (`--input`, default `data/adr-0074-d6-baseline.jsonl`), add a `source` field to each record (`--source`, default `baseline`, idempotent — does not overwrite existing), filter `parse_valid == true && task_success == true` (missing fields default **true** — ADR-0074 V1 actual schema `{prompt, response, reward, metadata}` lacks these fields, absence means "compatible, include"), and write `--output` (default `data/wave-3-training-data.jsonl`). The `tests/test_training_data_pipeline.cpp` MUST have ≥1 case covering migration + filter.

#### Scenario: 迁移脚本加 source 字段

- **WHEN** 运行 `python3 scripts/prepare_training_data.py --input <fixture> --output <out> --source baseline`
- **THEN** `<out>` 存在, 每行含 `"source": "baseline"` 字段

#### Scenario: 过滤 parse_valid && task_success

- **WHEN** 输入 fixture 含 3 records (1 valid + 1 parse_valid=false + 1 task_success=false) 并运行脚本
- **THEN** 输出 records 数 == 1 (仅 valid 通过)

### Requirement: D7 Serving 集成 Phase 1 最小版 (AC-6)

The `src/common/llm/finetune_provider.h/cpp` MUST implement `FinetuneBaseModelProvider : ILLMProvider` (ctor accepts `LLMConfig`): `generate()` returns failure with message containing "Phase 2 deferred" (fail-fast, no silent empty response), `generate_stream()` returns nullptr (Phase 1 no streaming), `available_models()` returns non-empty (ModelInfo name=`agenticdsl-llama-3.1-70b-lora-v1`). The `LLMProviderFactory::register_dynamic("agenticdsl-llama-3.1-70b-lora-v1", factory_fn)` MUST be called in the factory constructor (actual API name `register_dynamic`, not `register_provider`). The `tests/test_llm_provider_factory.cpp` MUST have ≥1 case covering registration + stub instantiation.

#### Scenario: 构造自动注册 fine-tune provider

- **WHEN** 运行 `test_llm_provider_factory::register_dynamic registers fine-tune provider and instantiation returns stub`
- **THEN** `factory.has_dynamic("agenticdsl-llama-3.1-70b-lora-v1")` == true 且 `factory.create({.provider=...})` 返回非空 stub

#### Scenario: Stub 的 available_models 非空 + generate 返回 "Phase 2 deferred"

- **WHEN** 调用 `provider->available_models()` 和 `provider->generate(req, token)`
- **THEN** `available_models()` 非空, `generate()` 返回 `Result::failure` 且 `error().message` 含 "Phase 2 deferred"

### Requirement: 既有 test 零回归 (AC-7, AC-8)

The focused ctest run MUST pass for `test_training_data_pipeline` + `test_llm_provider_factory` + `test_llm_tool*` + `test_cost_tracking_decorator` + `test_genome_registry` (≥6 binary). `ctest -N` MUST show 2 new binaries with no PDK cross-library impact.

#### Scenario: focused ctest 全 PASS

- **WHEN** 运行 `ctest -R "test_training_data_pipeline|test_llm_provider_factory|test_llm_tool|test_cost_tracking_decorator|test_genome_registry" --output-on-failure`
- **THEN** 全部 PASS (≥6 binary)

### Requirement: 1 Atomic Commit + Oracle 复评 (AC-9)

The change MUST be committed as 1 atomic commit `feat(llm): Wave 3 finetune-base-model pilot phase 1` (AGENTS.md mode #4, no amend), and Oracle post-impl SHIP-with-fixes 复评 MUST be scheduled (主会话派, prompt 引用 builder-handoff `post_impl_review_prompt`).

#### Scenario: atomic commit 存在

- **WHEN** 运行 `git log --oneline -1`
- **THEN** commit message 为 `feat(llm): Wave 3 finetune-base-model pilot phase 1`

### Requirement: Archive 5 文件完整 (AC-10)

The `openspec/changes/archive/wave-3-finetune-base-model-pilot-phase1-2026-09-23/` MUST contain 5 files (`.openspec.yaml` + `proposal.md` + `design.md` + `tasks.md` + `specs/wave-3-finetune-base-model/spec.md`), and the active change dir MUST be removed.

#### Scenario: Day-5 trap guard

- **WHEN** 运行 `git ls-files openspec/changes/archive/wave-3-finetune-base-model-pilot-phase1-2026-09-23/`
- **THEN** 输出 5 文件全在 (`.openspec.yaml` + `proposal.md` + `design.md` + `tasks.md` + `specs/wave-3-finetune-base-model/spec.md`)

### Requirement: 文档同步 (AC-11)

The AGENTS.md Recent Changes MUST gain a Wave 3 Phase 1 ship entry (commit hash + archive path), `docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md` §5.1 MUST read "Wave 3 Phase 1 SHIPPED", and `.rddf/roadmap/2026-09-21-pre-wave3-to-wave3-execution-plan.md` Decision Record §5.1 MUST be synced.

#### Scenario: Decision Record §5.1 同步

- **WHEN** 运行 `grep "Wave 3 Phase 1 SHIPPED" docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md`
- **THEN** 命中 ≥ 1 处

### Requirement: Cooling-off 链式合规 (AC-12)

The Wave 3 cooling-off MUST start from the Wave 3 merge (24h), chaining from Pre-Wave3 G2 merge `dc12a17`. The `cooling_off_override_audit` field MUST document the user override.

#### Scenario: cooling-off 审计证据

- **WHEN** 读取 `.rddf/state/builder/wave-3-finetune-base-model.json::cooling_off_override_audit`
- **THEN** 字段存在且含 override_at / override_by / next_cooling_off_timer
