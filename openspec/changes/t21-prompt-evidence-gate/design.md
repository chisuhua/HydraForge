# Design: t21-prompt-evidence-gate

## Context

ADR-0074 (Prompt Engineering + Evidence Gate) ✅ Approved 评审通过 2026-08-25。本 change 实施 Wave 2 Phase 2.2: Prompt 质量门控 + Go/No-Go 标准，为 Phase 6a → Phase 6b 推进提供客观依据。

所有前置已 ship:
- IEvaluator V2 (commit `314561e`) - RewardSignal.quality 集成
- T14 行为回归 (BehavioralRegressionGate) - task-success 评估
- ADR-0073 Tool JSON Schema - ToolMetadata V3 字段
- ADR-0068 Appendix A v1.2.2 (含 6 个 gepa.* + 4 mutation.* + 1 evaluation.result + 3 skill.compilation.*)
- Phase 6a baseline data (`from-roadmap-phase-6c-execution-baseline` ship 2026-08-18)

T21 是**质量门控层**，不是契约层。所有依赖通过既有 API 调用，零契约修改。

## Scope Boundaries

### 范围 IN
- Few-shot Library (≥ 30 .md 实际生成)
- Golden Tasks Dataset (≥ 50 .json 实际生成)
- Evidence Gate (Go/No-Go 阈值)
- Baseline Measurement (3 LLM × 2 指标)
- Two-Stage Injection (≤8k tokens)
- JSONL Training Data Export
- ADR-0068 附录 A v1.3 → v1.4 (3 个新主题)
- ≥ 10 测试 cases
- 文档同步 (ADR-0074 + cap-map + active-status)

### 范围 OUT
- 真实 LLM API 集成 (V1 仅 Mock)
- 在线 token 精确计数器 (V2)
- Wave 5 Fine-tune 实际触发 (V2)
- IEvaluator / ILLMProvider / Worker 公开 API 修改
- Few-shot 自动生成 (V1 仅人工编写)

## Design Decisions

### D1 — PromptEvidenceGate 复用 IEvaluator V2 CompositeEvaluator

```cpp
class PromptEvidenceGate {
public:
    enum class Decision { Go, Conditional, No_Go };
    
    Decision evaluate(const std::string& prompt,
                     const std::string& response,
                     const GoldenTask& golden);
private:
    std::shared_ptr<IEvaluator> evaluator_;  // V2 CompositeEvaluator
    double parse_valid_threshold_ = 0.90;
    double conditional_lower_bound_ = 0.80;
};
```

理由：IEvaluator V2 已 ship，提供 TaskSuccess + BehavioralEquivalence 加权聚合。PromptEvidenceGate 作为编排层复用现有评估 API。

### D2 — Two-Stage Injection ≤ 8k tokens

```cpp
struct AssembledPrompt {
    std::string stage1_few_shots;   // ≤ 4k tokens
    std::string stage2_stdlib;       // ≤ 4k tokens
    std::string task_specific;
};

AssembledPrompt PromptAssembler::assemble(const Task& task) {
    // Stage 1: task-specific few-shots (relevant to task domain)
    // Stage 2: stdlib subgraphs selection (subgraph library)
    // Total: ≤ 8k tokens (chars / 4 estimation)
}
```

理由：ADR-0074 §决策 5 明确 ≤8k tokens prefix 约束，避免 LLM context 溢出。

### D3 — MockILLMProvider 模拟 3 LLM

```cpp
class MockLLMProvider : public ILLMProvider {
public:
    enum class Persona { GPT4, Claude, DeepSeek };
    void set_persona(Persona p) { persona_ = p; }
    std::string generate(const std::string& prompt) override {
        // 基于 persona 返回不同的 DSL 候选
        switch (persona_) {
            case GPT4: return generate_gpt4_style(prompt);
            case Claude: return generate_claude_style(prompt);
            case DeepSeek: return generate_deepseek_style(prompt);
        }
    }
};
```

理由：避免外部 API 依赖，确保 baseline 测量可重现。

### D4 — Few-shot Library 实际生成 (非占位)

V1 实际编写 ≥ 30 .md 文件，覆盖 stdlib 20+ subgraphs + auth + human + math + utils + inference 5 大领域。每个 few-shot 含 input/output/error 三个部分。

理由：ADR-0074 §决策 1 要求 ≥30 实际 demo，V1 不能用占位符。

### D5 — Golden Tasks Dataset 实际生成

V1 实际编写 ≥ 50 .json 文件，每个含 input + expected_output + validation_rules 三字段。

理由：ADR-0074 §决策 2 要求 ≥50 真实任务，V1 不能用占位符。

### D6 — 2 个 llm.dsl.* 主题处理策略差异化

- `llm.dsl.parse_failed`: 语法错误，重试一次后 emit
- `llm.dsl.schema_validation_failed`: 语义错误，直接 emit + 不重试

理由：ADR-0074 §决策 7 明确两种错误处理策略不同。

### D7 — JSONL Schema 复用 ADR-0080

```json
{"prompt": "...", "response": "...", "reward": float, "metadata": {...}}
```

理由：ADR-0080 AppendOnlyEventLog 已 ship，JSONL schema 是其导出格式。

## Risks

| 风险 | 缓解 |
|---|---|
| Few-shot / Golden 实际生成工作量大 | V1 接受实际编写任务，2 sprint 估时已含 |
| MockLLMProvider 行为不真实 | persona-based 输出 + 真实 DSL 候选生成 |
| Token 估算不精确 | V1 chars/4 简化，V2 在线精确计数 |
| docs_drift_audit 引入新 drift | Phase 4 验证 |
| ctest 数字硬编码 | tasks.md 禁止 + 动态基线 |

## Verification Gates

- ≥ 30 few-shot .md 文件 (实际生成非占位)
- ≥ 50 golden task .json 文件 (实际生成非占位)
- ≥ 10 cases test_prompt_evidence_gate PASS
- ctest 190+ 全量零回归（动态基线）
- adr_lint 82 ADR PASS
- docs_drift_audit 0 NEW drift
- 既有契约 0 diff

## Dependencies

### 满足
- ✅ IEvaluator V2 ship
- ✅ T14 行为回归 ship
- ✅ ADR-0074 ✅ Approved
- ✅ ADR-0073 Tool JSON Schema
- ✅ ADR-0068 amendment 路径 (skill.compilation.* + gepa.* + mutation.* 已 ship 验证)

### 不依赖
- T20 AFlow
- ADR-0078 Fine-tune (V2)
- 真实 LLM API

## Out of Scope (V2+ deferred)

- 真实 LLM API 集成
- 在线 token 精确计数器
- Few-shot 自动生成
- Wave 5 Fine-tune 实际触发
- Pareto 多目标评估
- TrajectoryFidelity 评估

## Success Criteria

- T21 Prompt Evidence Gate V1 ✅ Shipped
- ≥ 30 few-shot + ≥ 50 golden 实际生成
- 10+ cases PASS
- Go/No-Go 门控客观标准就绪
- Phase 6a → Phase 6b 推进依据明确
- OpenSpec archive 完成