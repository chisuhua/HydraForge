# Tasks: t21-prompt-evidence-gate

> **TDD 5 步结构**: 每任务按 Write failing test → Verify fail → Implement → Verify pass → Commit
> **关键不变量**: 既有契约**零修改**（IEvaluator / ILLMProvider / CognitiveWorker / DomainWorkerPool / MutationGovernor / TrajectoryIR）。T21 是**质量门控层**，不是契约层。
> **V1 简化**: Mock ILLMProvider 避免外部 API 依赖；few-shots 与 golden tasks 实际生成（非占位）。

## Phase 0: Few-shot Library + Golden Tasks (估时 0.5 sprint)

- [x] **T0.1** Write failing test: `tests/test_prompt_evidence_gate.cpp` 骨架（≥ 10 cases 占位）
- [x] **T0.2** Verify fail: 编译失败（`fatal error: 'prompt/evidence_gate.h' file not found`）
- [x] **T0.3** Implement minimal: `include/agenticdsl/prompt/evidence_gate.h` 契约声明
- [x] **T0.4** Verify pass: 编译成功，10 cases 编译通过（运行时仍 FAIL）
- [x] **T0.5** Commit: `feat(prompt): evidence_gate contract skeleton (T0)`
- [ ] **T0.6** Generate `lib/prompt/few_shots/*.md` ≥ 30 个 few-shot demos（V1 实际生成）
- [ ] **T0.7** Generate `lib/prompt/golden/*.json` ≥ 50 个 golden tasks（V1 实际生成）
- [ ] **T0.8** Commit: `feat(prompt): few_shots library + golden tasks dataset (T0 data)`

## Phase 1: Evidence Gate 核心 (估时 0.5 sprint)

- [ ] **T1.1** Write failing test: `evidence_gate_parse_valid_passes` (parse-valid=95% → Go)
- [ ] **T1.2** Write failing test: `evidence_gate_parse_valid_conditional` (parse-valid=85% → Conditional)
- [ ] **T1.3** Write failing test: `evidence_gate_parse_valid_fail` (parse-valid=70% → No-Go)
- [ ] **T1.4** Verify fail: 3 cases FAIL (evidence_gate 未实现)
- [ ] **T1.5** Implement: `src/modules/prompt/evidence_gate.cpp`:
  - `PromptEvidenceGate::evaluate(prompt, response, golden_task) -> Decision {Go, Conditional, No-Go}`
  - 阈值: ≥90% Go / 80-89% Conditional / <80% No-Go
  - 复用 IEvaluator V2 CompositeEvaluator 评估 task-success
- [ ] **T1.6** Verify pass: 3 cases PASS + 全部既有测试零回归
- [ ] **T1.7** Commit: `feat(prompt): Evidence Gate core with Go/No-Go thresholds (T1)`

## Phase 2: Baseline Measurement + Two-Stage Injection (估时 0.5 sprint)

- [ ] **T2.1** Write failing test: `baseline_measurement_3_llms_2_metrics` (3 MockLLM × 2 指标 → baseline.json)
- [ ] **T2.2** Write failing test: `two_stage_injection_under_8k_tokens` (Stage 1 + Stage 2 ≤ 8k)
- [ ] **T2.3** Write failing test: `two_stage_injection_over_8k_emits_event` (超出 → emit `prompt.token_limit_exceeded`)
- [ ] **T2.4** Verify fail: 3 cases FAIL
- [ ] **T2.5** Implement: `tools/baseline/measure_prompt_baseline.py`:
  - 调用 PromptEvidenceGate + MockLLMProvider × 3 (模拟 GPT-4 / Claude / DeepSeek)
  - 输出 baseline.json (parse-valid + task-success 矩阵)
- [ ] **T2.6** Implement: `src/modules/prompt/prompt_assembler.cpp`:
  - `PromptAssembler::assemble(task)` 两阶段注入 (≤4k + ≤4k = ≤8k)
  - 超出 token limit → emit `prompt.token_limit_exceeded`
- [ ] **T2.7** Verify pass: 3 cases PASS
- [ ] **T2.8** Commit: `feat(prompt): baseline measurement + two-stage injection (T2)`

## Phase 3: JSONL Export + ADR-0068 v1.4 主题 (估时 0.3 sprint)

- [ ] **T3.1** Write failing test: `jsonl_export_schema_compliance` (输出符合 ADR-0080 JSONL schema)
- [ ] **T3.2** Write failing test: `llm_dsl_parse_failed_event_emitted` (parse 错误 → emit `llm.dsl.parse_failed`)
- [ ] **T3.3** Write failing test: `llm_dsl_schema_validation_failed_event_emitted` (schema 错误 → emit `llm.dsl.schema_validation_failed`)
- [ ] **T3.4** Verify fail: 3 cases FAIL
- [ ] **T3.5** Implement: `tools/prompt/export_training_data.py`:
  - 导出 JSONL: `{"prompt": "...", "response": "...", "reward": float, "metadata": {...}}`
- [ ] **T3.6** Implement: Evidence Gate 添加事件发射:
  - parse 错误 → emit `llm.dsl.parse_failed` (topic + payload: prompt + error_position + retry_count)
  - schema 错误 → emit `llm.dsl.schema_validation_failed` (topic + payload: prompt + violation + no_retry)
- [ ] **T3.7** Modify `docs/adr/adr-0068-event-emission-contract.md` 附录 A v1.3 → v1.4:
  - 注册 2 个 `llm.dsl.*` 主题（owner: PromptEvidenceGate）
  - 注册 1 个 `prompt.token_limit_exceeded` 主题（owner: PromptAssembler）
- [ ] **T3.8** Verify pass: 3 cases PASS
- [ ] **T3.9** Commit: `feat(prompt): JSONL export + ADR-0068 v1.4 (T3)`

## Phase 4: 文档同步 + ship (估时 0.2 sprint)

- [ ] **T4.1** Modify `docs/adr/adr-0074-prompt-evidence-gate.md` 头部 `##状态`:
  - 追加 V1 ship 证据段 (commit hash + 测试数 + ctest baseline)
- [ ] **T4.2** Modify `docs/architecture/capability-application-map-2026-08.md`:
  - 头部 v2.1 → v2.2 + 最后验证 2026-08-27
  - §一 +1（新能力 #28 Prompt Evidence Gate）
  - §八 T21 → ✅ Completed
  - §七 changelog v2.2 条目
- [ ] **T4.3** Modify `docs/active-status.md` §一 T21 跟踪段
- [ ] **T4.4** Modify `docs/architecture/self-evolution-architecture-2026-08.md` §四
- [ ] **T4.5** Verify: `python3 tools/adr_lint.py` + `docs_drift_audit.py` 全通过
- [ ] **T4.6** Verify: `openspec validate --changes --strict` PASS
- [ ] **T4.7** Verify: `ctest --output-on-failure` 全量 0 回归（动态基线）
- [ ] **T4.8** Commit: `feat(prompt): ship T21 Prompt Evidence Gate V1 - Wave 2 → Wave 3 Go/No-Go 门控就绪`
- [ ] **T4.9** `openspec archive t21-prompt-evidence-gate`

## 总估时

- Phase 0: 0.5 sprint
- Phase 1: 0.5 sprint
- Phase 2: 0.5 sprint
- Phase 3: 0.3 sprint
- Phase 4: 0.2 sprint
- **总估时: ~2.0 sprint**（接近 1 月，符合 cap-map §八 T21 估时）

## 明确 out of scope (V2+ deferred)

- 真实 LLM API 集成（V1 仅 Mock）
- 在线 token 精确计数器（V1 使用字符数估算）
- Wave 5 Fine-tune 实际触发（ADR-0078）
- Pareto 多目标评估（依赖 IEvaluator V3+）
- TrajectoryFidelity 评估（依赖 T15 V2 schema）
- Few-shot 自动生成（V1 仅人工编写）

## 关键不变量（强制遵守）

- ❌ IEvaluator V2 公开 API **零修改**
- ❌ ILLMProvider 公开 API **零修改**
- ❌ CognitiveWorker/DomainWorkerPool **零修改**
- ❌ MutationGovernor/TrajectoryIR/SkillCompiler **零修改**
- ❌ 修改既有 ADR 状态
- ❌ 触发真实 LLM API 调用（仅 Mock）
- ❌ 在测试失败时强行 commit
- ❌ 硬编码 ctest 数字

## V1 简化策略

- **Few-shots 实际生成**: V1 不放占位符，30+ 个 `.md` 文件实际编写
- **Golden tasks 实际生成**: V1 不放占位符，50+ 个 `.json` 文件实际编写
- **MockILLMProvider**: 与 T19 一致，避免外部 API 依赖
- **Token 计数简化**: 使用字符数 / 4 估算（V1 简化，V2 在线精确计数）
- **阈值固定**: Go ≥90% / Conditional 80-89% / No-Go <80% (per ADR-0074 §决策 4)