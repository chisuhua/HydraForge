# from-roadmap-phase-6c-execution-baseline

## Why

ADR-0074 定义了三层 Prompt 策略（V1 schema constraint / V2 few-shot / V3 two-stage injection），本提案覆盖 C1 + C2 + C3 的实施：

- **V1（schema constraint）**：Prompt 中嵌入严格 JSON Schema 约束，强制 LLM 输出结构化数据，降低解析失败率。
- **V2（few-shot）**：在 Prompt 中注入 4 维度 × 8 = 32 个标注好的 few-shot examples，覆盖 parse valid / task success / budget hit / error recovery 四类维度，提升少样本泛化能力。
- **V3（two-stage）**：分两阶段注入——先发 system prompt（含 schema 约束），再发 user prompt（内嵌 few-shot examples），避免一次性 token 溢出并提升注入稳定性。

**Golden Suite 方法论**：C2 采集的 50 个 held-out golden tasks 从训练 prompt 中完全隔离，确保 Evidence Gate 测量不受数据泄露影响，忠实反映 V1/V2/V3 prompt 在未见过的任务上的真实表现。

**依赖链**（per roadmap.md line 271-273）：

```
W2 (baseline 测量工具) ──→ C1 (few-shot 采集)
                        ──→ C2 (golden suite YAML)
                              │
                              └──→ C3 (V1/V2/V3 prompt 实施 + 测量)
                                    │
                                    └──→ C4 (Evidence Gate 决议)
```

本提案**不**依赖 ADR-0074 的 D4（Evidence Gate），该决策由独立 `evidence-gate` 提案覆盖。

## What Changes

**In Scope**:

- C1: ADR-0074 D1 few-shot examples 采集——4 维度 × 8 examples = 32 个 few-shot，YAML 格式存储至 `lib/prompts/fewshot/`。
- C2: ADR-0074 D2 held-out golden suite——50 个真实任务 YAML，含 `task_id / input / expected_output / dimension / difficulty` 字段，存入 `lib/prompts/golden/`。
- C3: ADR-0074 D3 V1/V2/V3 prompt 代码实施 + 首次 baseline 测量——`tools/measure_prompt_baseline` 跑 golden suite，输出 parse-valid率和 task-success 率。
- Baseline 测量报告 `docs/audits/<date>-execution-baseline-v1.md`（含 V1 vs V2 vs V3 对比数据）。
- **Out of Scope**:
- C4 Evidence Gate 第一次决议（独立 `evidence-gate` 提案）。
- ADR-0072 D2/D3 条件性 ship（C5/C6，按 Evidence Gate 触发）。
- ToolCoordinator 校验层集成（ADR-0073 D3，C9）。
- 3 模型 × 50 tasks 的完整 W2 测量（已 carry-over W2 任务中）。

### 关键场景

- GIVEN 32 个 few-shot examples 采集完毕（4 维度各 8 个）
  WHEN `tools/measure_prompt_baseline` 以 V2 prompt 在 golden suite 上运行
  THEN parse-valid 率比 V1（无 few-shot）提升，task-success 率同步记录。

- GIVEN 50 个 golden tasks YAML 正确格式化且与训练 prompt 完全隔离
  WHEN V1 / V2 / V3 三种 prompt 分别跑 golden suite
  THEN 各产生独立 baseline 数据，golden tasks **不**出现在任何训练 prompt 中（可审计 grep 验证）。

- GIVEN V3 two-stage prompt（system 含 schema 约束 + user 含 few-shot）
  WHEN 与 V1 / V2 在 golden suite 上对比
  THEN V3 在 parse-valid 和 task-success 两个指标上均 ≥ V2 且 ≥ V1，数据写入 baseline 报告。

- GIVEN baseline 测量报告显示 parse-valid ≥ 85% 且 task-success L1 ≥ 70%
  WHEN Evidence Gate 决议时（C4）
  THEN Golden Suite 数据作为 held-out 证据，报告格式符合 `docs/audits/<date>-evidence-gate-v1.md` 输入要求。

**Out of Scope**:

- (no items specified)

## Capabilities

- MUST 使用 4 维度 taxonomy（parse valid / task success / budget hit / error recovery）采集 few-shot examples，确保维度覆盖均衡。
- MUST golden suite 从训练 prompt 中完全 hold-out，采集前先 grep 确认无泄露。
- MUST V3 prompt 实现为两阶段注入：先 system prompt（含 JSON Schema 约束），再 user prompt（含 few-shot examples），两阶段顺序固定。
- MUST 输出机器可读的 YAML 格式 baseline 报告（含 parse-valid rate / task-success rate / per-dimension breakdown），供 C4 Evidence Gate 自动消费。

## Impact

- MUST NOT 将 golden examples 混入 prompt 模板；golden suite 仅用于测量，不参与 prompt 构建。
- MUST NOT 在 C3 ship 前跳过 baseline 测量；测量报告是 C3 的强制产出。

## Acceptance

- [ ] C1 完成：32 个 few-shot examples 采集完毕（4 维度 × 8），存储于 `lib/prompts/fewshot/`，每个 example 含 `dimension / input / output / rationale` 字段。
- [ ] C2 完成：50 个 golden tasks YAML 存储于 `lib/prompts/golden/`，每个 task 含 `task_id / input / expected_output / dimension / difficulty`，且 grep 验证 golden tasks 不出现在 `lib/prompts/` 其他文件中。
- [ ] C3 完成：V1 / V2 / V3 三种 prompt 代码实现（`src/common/prompts/` 或 `lib/prompts/`），`tools/measure_prompt_baseline` 可接受 `--prompt V1|V2|V3 --golden-dir lib/prompts/golden/` 并输出 YAML 报告。
- [ ] Baseline 测量报告 `docs/audits/<date>-execution-baseline-v1.md` 已生成，含 V1 vs V2 vs V3 parse-valid 对比表（目标 V3 ≥ 85%）和 task-success L1 数据（目标 ≥ 70%，C4 Gate 前仅记录，deferred to C4 判定）。
- [ ] `ctest --output-on-failure` 全量零回归（ctest 不跑 golden suite，仅验证工具本身构建和基础测试通过）。
- [ ] 文档更新：`lib/prompts/README.md` 补充 few-shot 和 golden suite 使用说明（含目录结构和 V1/V2/V3 差异说明）。

