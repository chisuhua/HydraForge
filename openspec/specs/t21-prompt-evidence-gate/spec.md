# t21-prompt-evidence-gate Specification

## Purpose
TBD - created by archiving change t21-prompt-evidence-gate. Update Purpose after archive.
## Requirements
### Requirement: Few-shot Library ≥ 30 个真实 demo

The `lib/prompt/few_shots/` directory MUST contain at least 30 `.md` demo files (not placeholders). Each file MUST demonstrate a specific DSL subgraph usage with input/output.

#### Scenario: Few-shot library 文件数

- **WHEN** 静态检查 `find lib/prompt/few_shots/ -name "*.md" | wc -l`
- **THEN** 输出 ≥ 30

#### Scenario: Few-shot 内容真实非占位

- **WHEN** 静态检查 `grep "TODO\|PLACEHOLDER\|FIXME" lib/prompt/few_shots/*.md | head -5`
- **THEN** 不应出现占位标记（V1 真实生成）

### Requirement: Golden Tasks Dataset ≥ 50 个真实任务

The `lib/prompt/golden/` directory MUST contain at least 50 `.json` task files. Each MUST include `input` + `expected_output` + `validation_rules`.

#### Scenario: Golden tasks 文件数

- **WHEN** 静态检查 `find lib/prompt/golden/ -name "*.json" | wc -l`
- **THEN** 输出 ≥ 50

#### Scenario: Golden tasks schema 合规

- **WHEN** 静态检查 `jq -e '.input and .expected_output and .validation_rules' lib/prompt/golden/*.json | head -5`
- **THEN** 50+ 文件必须含三字段 schema

### Requirement: Evidence Gate Go/No-Go 阈值

The `PromptEvidenceGate::evaluate()` MUST return Decision based on parse-valid percentage:
- ≥ 90% → Go (Wave 3 推进)
- 80-89% → Conditional (Oracle 预审)
- < 80% → No-Go (Wave 2 迭代)

#### Scenario: Go 决策 (parse-valid=95%)

- **WHEN** 运行 `test_prompt_evidence_gate::evidence_gate_parse_valid_passes`
- **THEN** 返回 `Decision::Go`

#### Scenario: Conditional 决策 (parse-valid=85%)

- **WHEN** 运行 `test_prompt_evidence_gate::evidence_gate_parse_valid_conditional`
- **THEN** 返回 `Decision::Conditional`

#### Scenario: No-Go 决策 (parse-valid=70%)

- **WHEN** 运行 `test_prompt_evidence_gate::evidence_gate_parse_valid_fail`
- **THEN** 返回 `Decision::No-Go`

### Requirement: Baseline Measurement 3 LLM × 2 指标

The `tools/baseline/measure_prompt_baseline.py` MUST measure 3 mock LLMs × 2 metrics (parse-valid + task-success) and output `baseline.json`.

#### Scenario: 3 LLM × 2 指标 baseline

- **WHEN** 运行 `python3 tools/baseline/measure_prompt_baseline.py`
- **THEN** 输出 `baseline.json` 含 3 LLM (gpt-4 / claude / deepseek) × 2 metrics (parse-valid / task-success)

### Requirement: Two-Stage Injection ≤ 8k tokens

The `PromptAssembler::assemble(task)` MUST use two-stage injection:
- Stage 1: task-specific few-shots (≤4k tokens)
- Stage 2: stdlib subgraphs selection (≤4k tokens)
- Total ≤ 8k tokens

#### Scenario: 注入 ≤ 8k tokens

- **WHEN** 运行 `test_prompt_evidence_gate::two_stage_injection_under_8k_tokens`
- **THEN** Stage 1 ≤ 4k + Stage 2 ≤ 4k = Total ≤ 8k

#### Scenario: 超出 8k tokens 发射事件

- **WHEN** 运行 `test_prompt_evidence_gate::two_stage_injection_over_8k_emits_event`
- **THEN** emit `prompt.token_limit_exceeded` 事件

### Requirement: JSONL Export Schema

The JSONL training data MUST conform to `{"prompt": "...", "response": "...", "reward": float, "metadata": {...}}` schema per ADR-0080.

#### Scenario: JSONL schema 合规

- **WHEN** 运行 `test_prompt_evidence_gate::jsonl_export_schema_compliance`
- **THEN** 导出 JSONL 含 prompt + response + reward + metadata 四字段

### Requirement: 2 个 llm.dsl.* 事件主题注册

The ADR-0068 附录 A v1.4 MUST register 2 new topics:
- `llm.dsl.parse_failed` (syntax errors, owner: PromptEvidenceGate)
- `llm.dsl.schema_validation_failed` (semantic errors, owner: PromptEvidenceGate)

#### Scenario: 2 主题注册

- **WHEN** 静态检查 `grep "llm.dsl.parse_failed\|llm.dsl.schema_validation_failed" docs/adr/adr-0068-event-emission-contract.md`
- **THEN** 2 主题必须全部出现

#### Scenario: parse_failed 事件发射

- **WHEN** 运行 `test_prompt_evidence_gate::llm_dsl_parse_failed_event_emitted`
- **THEN** 触发 DSL parse 错误时 emit `llm.dsl.parse_failed`

#### Scenario: schema_validation_failed 事件发射

- **WHEN** 运行 `test_prompt_evidence_gate::llm_dsl_schema_validation_failed_event_emitted`
- **THEN** 触发 schema 验证错误时 emit `llm.dsl.schema_validation_failed`

### Requirement: 既有契约零修改

The V1 implementation MUST NOT modify IEvaluator / ILLMProvider / CognitiveWorker / DomainWorkerPool public APIs.

#### Scenario: 契约文件未变更

- **WHEN** `git diff HEAD~1 -- include/agenticdsl/contract/ievaluator.h include/agenticdsl/llm/illm_provider.h include/agenticdsl/cognitive/cognitive_worker.h include/agenticdsl/cognitive/domain_worker_pool.h`
- **THEN** 不应有 V1 implementation 相关 diff

### Requirement: ctest 全量零回归

The `ctest --output-on-failure` MUST report ALL tests PASS with zero regressions relative to T19 ship baseline (189/190, 1 pre-existing timing flake).

#### Scenario: ctest 全量 PASS

- **WHEN** 运行 `ctest --output-on-failure`
- **THEN** 所有测试 PASS, 0 failures（pre-existing flake 不计入）
- **AND** 测试计数 ≥ T19 baseline + 1 (test_prompt_evidence_gate target)

#### Scenario: test_prompt_evidence_gate 专项

- **WHEN** 运行 `ctest --output-on-failure -R test_prompt_evidence_gate`
- **THEN** ≥ 10 cases PASS, ≥ 30 assertions

### Requirement: adr_lint + docs_drift_audit 全通过

The `python3 tools/adr_lint.py` MUST exit 0, and `python3 tools/docs_drift_audit.py` MUST NOT introduce new CRITICAL drift.

#### Scenario: adr_lint 全过

- **WHEN** 运行 `python3 tools/adr_lint.py`
- **THEN** 退出码 0, 无 ADR lint 错误

#### Scenario: docs_drift_audit 零新增

- **WHEN** 运行 `python3 tools/docs_drift_audit.py`
- **THEN** Prompt Evidence Gate 相关 drift 为 0 项新增

### Requirement: ADR-0074 V1 ship 注记 + cap-map §一 +1

The ADR-0074 header MUST append V1 ship evidence. The capability-application-map MUST add new capability #28 (Prompt Evidence Gate) to §一 and bump version v2.1 → v2.2.

#### Scenario: ADR-0074 V1 状态注记

- **WHEN** 静态检查 `grep "V1 ship\|Evidence Gate" docs/adr/adr-0074-prompt-evidence-gate.md`
- **THEN** V1 ship 证据必须可见

#### Scenario: cap-map §一 +1 新能力

- **WHEN** 静态检查 `grep "Prompt Evidence Gate" docs/architecture/capability-application-map-2026-08.md | head -3`
- **THEN** §一表格必须新增 Prompt Evidence Gate 能力行

### Requirement: V1 边界遵守 (Mock LLM + 简化 token 计数)

The T21 V1 MUST use MockILLMProvider (no real LLM API calls). Token counting MUST use simple character/4 estimation. Real LLM API integration and precise token counter deferred to V2.

#### Scenario: MockILLMProvider 唯一来源

- **WHEN** 静态检查 `grep "openai_api\|anthropic_api\|deepseek_api" tools/baseline/measure_prompt_baseline.py`
- **THEN** 不应出现真实 LLM API 调用代码（V1 仅 Mock）

#### Scenario: 字符数估算 token

- **WHEN** 运行 PromptAssembler::assemble(task)
- **THEN** 使用 chars / 4 估算（非精确 token 计数）

