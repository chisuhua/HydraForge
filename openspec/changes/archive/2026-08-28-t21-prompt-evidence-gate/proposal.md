# t21-prompt-evidence-gate

## Why

ADR-0074 (Prompt Engineering + Evidence Gate) ✅ Approved 评审通过 2026-08-25。Wave 2 Phase 2.2 是 LLM-native AgenticDSL 架构 (ADR-0071) §决策 D5 的具体实施：训练数据 + Prompt 策略 + Evidence Gate。

**Oracle 评审关键发现** (cap-map §八 + ADR-0074 §决策 1-7):
- 5 个具体空白需填补：few-shot ≥30、held-out ≥50、baseline 3 LLM ×2 指标、Go/No-Go 门控、两阶段注入 ≤8k
- 2 个候选幻影主题 `llm.dsl.parse_failed` / `llm.dsl.schema_validation_failed` 需 ADR-0068 §附录 A amendment
- RewardSignal.quality 用于 prompt 门控（依赖 IEvaluator V2 ship）
- Phase 6a C2 C3 `from-roadmap-phase-6c-execution-baseline` ✅ ship 2026-08-18 提供前置 few-shot 数据 + baseline 测量方法学

**审计依据**:
- ADR-0074 ✅ Approved 评审通过 2026-08-25
- ADR-0071 ✅ Approved (父 ADR, LLM-native AgenticDSL 架构)
- ADR-0068 ✅ Approved (事件发射契约，含 D7 主题注册 amendment)
- ADR-0001 ✅ Approved (ILLMProvider 流式接口)
- ADR-0008 ✅ Approved (LayeredContext, working/episodic/semantic 三层)
- IEvaluator V2 ✅ Shipped (commit `314561e`) - RewardSignal.quality 已可用
- ADR-0068 Appendix A v1.2.2 ✅ Shipped 2026-08-27 (含 6 个 gepa.* + 4 个 mutation.* + 1 个 evaluation.result 主题)

**前置依赖**（全部已满足）：
- ✅ IEvaluator V2 ship (RewardSignal.quality 用于 prompt 门控)
- ✅ T14 行为回归 ship (BehavioralRegressionGate, 用于 task-success 评估)
- ✅ ADR-0073 Tool JSON Schema (ToolMetadata V3 字段)
- ✅ Phase 6a baseline data handoff (`from-roadmap-phase-6c-execution-baseline` ship 2026-08-18)
- ✅ ADR-0068 amendment 路径明确 (skill.compilation.* + gepa.* + mutation.* 已 ship 验证模式)

## What Changes

**新增 Prompt Evidence Gate 引擎** (`src/modules/prompt/evidence_gate.{h,cpp}`):

1. **Few-shot Library** (D1):
   - `lib/prompt/few_shots/` 目录 ≥ 30 个 `.md` 演示样本
   - 涵盖 stdlib 全部 20+ subgraphs
   - 每个 few-shot 含 input/output/error 三个文件
   - **关键**: 本 change ship ≥30 (V1 实际生成,非占位)

2. **Golden Tasks Dataset** (D2):
   - `lib/prompt/golden/` 目录 ≥ 50 个 held-out 评估任务
   - 跨多领域覆盖 (auth / human / math / utils / inference)
   - 每个 task 含 input + expected_output + validation_rules
   - **关键**: 本 change ship ≥50

3. **Baseline Measurement** (D3):
   - `tools/baseline/measure_prompt_baseline.{sh,py}` - 3 LLM × 2 指标测量脚本
   - 集成 MockLLMProvider + 真实 ILLMProvider 抽象
   - 输出 JSON 格式 baseline.json (parse-valid + task-success 矩阵)
   - **关键**: V1 使用 MockLLMProvider (零外部 API 依赖)

4. **Evidence Gate** (D4):
   - `PromptEvidenceGate::evaluate(prompt, response)` 返回 Go/No-Go 决策
   - 阈值定义:
     - parse-valid ≥ 90% → Go (Wave 3 推进)
     - parse-valid 80-89% → Conditional (Oracle 预审)
     - parse-valid < 80% → No-Go (Wave 2 迭代)
   - **关键**: 复用 IEvaluator V2 CompositeEvaluator (TaskSuccess + BehavioralEquivalence 加权)

5. **Two-Stage Injection** (D5):
   - `PromptAssembler::assemble(task)` 两阶段注入:
     - Stage 1: task-specific few-shots (≤4k tokens)
     - Stage 2: stdlib subgraphs selection (≤4k tokens, 总 ≤8k)
   - 超出 token limit → emit `prompt.token_limit_exceeded` (新主题)

6. **JSONL Training Data** (D6):
   - `tools/prompt/export_training_data.{sh,py}` - 导出 JSONL 训练数据
   - Schema: `{"prompt": "...", "response": "...", "reward": float, "metadata": {...}}`
   - 集成 ADR-0080 AppendOnlyEventLog 持久化

7. **Failure Event Classification** (D7):
   - ADR-0068 附录 A amendment: 注册 2 个新主题:
     - `llm.dsl.parse_failed` (syntax errors)
     - `llm.dsl.schema_validation_failed` (semantic errors)
   - 处理策略差异化: parse_failed 重试一次 + emit；schema_validation_failed 直接 emit + 不重试

**新增测试** (`tests/test_prompt_evidence_gate.cpp`):
- ≥ 10 cases 覆盖完整 Evidence Gate 流程
- 真实 few-shot 库加载 + 真实 golden tasks 评估
- Mock ILLMProvider (避免外部 API 依赖)
- IEvaluator V2 CompositeEvaluator 集成
- 两阶段注入 token limit 验证

## Impact

**影响范围**:
- ILLMProvider **零修改** (依赖既有 generate() 流式回调)
- IEvaluator V2 **零修改** (作为依赖使用)
- CognitiveWorker/DomainWorkerPool **零修改**
- ADR-0068 仅**附录 A 增量注册** 2 个新主题 (符合既有 amendment 流程)

**下游解锁**:
- T19 GEPA MVP (Phase 2 commit 已 ship, 需 prompt 质量保证)
- T20 AFlow MCTS 工作流搜索 (工作流节点为 prompt)
- ADR-0078 Fine-tune 训练数据来源 (Wave 5+, JSONL 数据已 ship)
- B7 自进化基础应用 prompt 质量门控
- Phase 6a → Phase 6b 推进的 Go/No-Go 客观标准

**V1 边界** (per ADR-0074 §决策):
- ✅ Mock ILLMProvider (零外部 API 依赖, V1 简化)
- ✅ ≥30 few-shots + ≥50 golden tasks (V1 实际生成)
- ✅ 3 LLM × 2 指标 baseline (MockLLMProvider 模拟)
- ✅ 两阶段注入 ≤8k tokens (实施但 V1 不强制启用)
- ⏸ 真实 LLM API 集成 (V2 deferred)
- ⏸ 在线 token 计数器 (V2 deferred, V1 使用简单字符数估算)
- ⏸ Wave 5 Fine-tune 实际触发 (V2 deferred)

**Breaking Changes**: 无 (仅新增模块与 2 个 ADR-0068 主题)

## ship gate 验证

- `python3 tools/adr_lint.py` 通过 (≥82 ADR)
- `python3 tools/docs_drift_audit.py` 通过 (无新增 CRITICAL drift)
- `openspec validate --changes --strict` PASS
- `ctest --output-on-failure` 全量 0 回归 (动态基线, 189→190 净增)
- `ctest -R test_prompt_evidence_gate` ≥ 10 cases / ≥ 30 assertions PASS
- `find lib/prompt/few_shots/ -name "*.md" | wc -l` ≥ 30
- `find lib/prompt/golden/ -name "*.json" | wc -l` ≥ 50
- ADR-0068 附录 A v1.3 → v1.4 (新增 2 个 llm.dsl.* 主题)
- cap-map §一 +1 (新能力 #28 Prompt Evidence Gate)
- ADR-0074 头部追加 V1 ship 注记

## 关联文档

- `docs/adr/adr-0074-prompt-evidence-gate.md` (待追加 V1 ship 注记)
- `docs/adr/adr-0068-event-emission-contract.md` (附录 A v1.3 → v1.4 amendment)
- `docs/adr/adr-0071-llm-native-agenticdsl-architecture.md` (父 ADR)
- `docs/adr/adr-0073-tool-json-schema-contract.md` (ToolMetadata V3 schema)
- `include/agenticdsl/contract/ievaluator.h` (V2 ship, 零修改)
- `include/agenticdsl/testing/behavioral_regression.h` (T14 ship, 零修改)
- `include/agenticdsl/llm/illm_provider.h` (既有, 零修改)
- `lib/prompt/few_shots/` (新增, ≥30 .md)
- `lib/prompt/golden/` (新增, ≥50 .json)
- `tools/baseline/measure_prompt_baseline.{sh,py}` (新增)
- `tools/prompt/export_training_data.{sh,py}` (新增)
- `tests/test_prompt_evidence_gate.cpp` (新增, ≥10 cases)
- ADR-0075 EnvBackend (env-aware prompt baseline)
- ADR-0076 MCP Server (MCP inputSchema 字段复用)
- ADR-0080 AppendOnlyEventLog (训练数据持久化)