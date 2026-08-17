## Context

当前 AgenticDSL 运行时(v3.10)尚未建立结构化的 Prompt 构造管线,三处关键空白:

- `src/common/prompts/` 目录**不存在**——prompt 模板散落在 `examples/agent_basic/.agent.md`、`examples/agent_simple/`、`examples/agent_loop/` 各示例的内嵌 string literal 中,无统一格式约束;
- `lib/prompts/` 目录**不存在**——既无 few-shot examples 库也无 held-out golden 评估集,`lib/` 当前仅含 `inference/ loop/ math/ utils` 子目录;
- `tools/measure_prompt_baseline` CLI **不存在**——`tools/` 目录仅有 `adr_lint.py / check_roadmap_drift.py / docs_drift_audit.py` 等元工具,无 prompt 测量工具;
- `llm_config.json`(根目录)已存在作为 LLM 后端配置入口,但仅覆盖模型路径/温度等运行时参数,与 Prompt 工程无关。

[ADR-0074 — Prompt Engineering + Evidence Gate](../../docs/adr/adr-0074-prompt-evidence-gate.md)(🔍 Proposed, 2026-08-03)派生自 [ADR-0071](../../docs/adr/adr-0071-llm-native-agenticdsl-architecture.md) §决策 D5,定义了 3 层 Prompt 策略与 Evidence Gate 闭环:

| 策略 | 内容 | 衔接 ADR |
|------|------|----------|
| **V1** schema constraint | Prompt 嵌入严格 JSON Schema 约束 | [ADR-0073](../../docs/adr/adr-0073-tool-json-schema-contract.md) ToolMetadata V3 schema |
| **V2** few-shot | 注入 4 维度 × 8 = 32 个标注 examples | ADR-0074 §决策 D1 |
| **V3** two-stage | 先 system(含 schema)+ 再 user(含 few-shot) | ADR-0074 §决策 D5 |

本 change 实施 ADR-0074 的 **C1(few-shot)+ C2(golden suite)+ C3(V1/V2/V3 + baseline)**,**不在范围**: C4 Evidence Gate 决议、ADR-0072 D2/D3 条件 ship、ToolCoordinator 集成、3 模型 × 50 tasks 的完整 W2 测量运行。

依赖链(per `roadmap.md` line 271-273):

```
W2 (baseline 工具) ──→ C1 (few-shot)
                    ──→ C2 (golden suite)
                          │
                          └──→ C3 (V1/V2/V3 实施 + 测量)
                                │
                                └──→ C4 (Evidence Gate, 独立 change)
```

## Goals / Non-Goals

**Goals:**

1. C1 — 采集 32 个 few-shot examples(4 维度 × 8),YAML 格式存 `lib/prompts/fewshot/`,每个 example 含 `dimension / input / output / rationale` 4 字段。
2. C2 — 采集 51 个 held-out golden tasks(L1=20 + L2=20 + L3=11,提案称"50 个"按 ≥ 50 取整),YAML 格式存 `lib/prompts/golden/`,每个 task 含 `task_id / input / expected_output / dimension / difficulty` 5 字段。
3. C3 — 在 `src/common/prompts/` 实施 V1/V2/V3 三种 prompt 构造器 + `tools/measure_prompt_baseline` CLI,支持 `--prompt V1|V2|V3 --golden-dir --output YAML --mock-mode`,产出符合 D-4 schema 的 YAML 报告。
4. 跑首次 baseline 测量(默认 mock mode)并发布 `docs/audits/<date>-execution-baseline-v1.md`,含 V1/V2/V3 对比表 + 95% CI + per-dimension 分解。
5. `ctest --output-on-failure` 全量零回归(golden suite 不纳入 ctest,仅验证工具本身构建 + 单元测试 PASS)。

**Non-Goals:**

- C4 Evidence Gate 决议文档(独立 change);
- ADR-0074 §决策 D4 Evidence Gate 阈值脚本(Go/No-Go 判定);
- ADR-0074 §决策 D7 失败事件分类(需 ADR-0068 §附录 A amendment PR);
- ADR-0074 §决策 D5 **subgraph 选择阶段**(Stage 1 top-3~5 选择)——本 change V3 仅做"system→user"消息拆分,Stage 1 留 Phase 6d C5;
- ADR-0074 §决策 D6 JSONL 训练数据格式(Wave 5+ Fine-tune 衔接);
- 3 模型 × 50 tasks 的真实 LLM W2 测量(仅产出工具,运行留给 `evidence-gate`);
- ToolCoordinator 集成(ADR-0073 D3,C9)。

## Decisions

### D-1. Few-shot 存储格式与目录结构

**决策**: YAML 格式 + 4 字段必填 + 文件命名 `{dimension}_{NN}.yaml`,存 `lib/prompts/fewshot/`,复用 vendor 的 `yaml-cpp` 解析。**替代方案拒绝**: JSON(提案明确 YAML)、单文件多 example(增大 PR review 难度)。

### D-2. Golden Suite Hold-out 强制

**决策**: `lib/prompts/golden/` 与 `lib/prompts/fewshot/` **物理隔离** + ship 前 `scripts/verify_golden_holdout.sh` grep 强制 0 匹配(`grep -rn "<task_id>" lib/prompts/fewshot/` 返回 0 行),纳入 `scripts/sprint-closeout.sh` Step 6。文件命名 `{domain}_{NN}.yaml`,`domain ∈ {auth, human, math, utils, inference, mcp}`。

### D-3. V3 Two-stage 注入顺序固定

**决策**: V3 在代码层显式 enum 强制"先 system 后 user"顺序,调用方不可逆序。

```cpp
enum class PromptStage { SystemFirst, UserSecond };
// src/common/prompts/v3.cpp
PromptPayload build_v3_prompt(const FewShotExamples& fewshots, const UserInput& input) {
  PromptPayload p;
  p.add_system(build_system_with_schema(tool_metadata_schema));  // Stage 1: system
  p.add_user(build_user_with_fewshots(input, fewshots));         // Stage 2: user
  return p;
}
```

- **ADR 影响**: V1/V2 隐式单阶段 → V3 显式两阶段属**重大设计变更**,但属于 ADR-0074 §决策 D5 子集,**不创建新 ADR**;
- 衔接: V3 system prompt 可被 [ADR-0069](../../docs/adr/adr-0069-tool-coordinator-hooks.md) pre-hook 进一步增强(Phase 6d C9 范围,本 change 不实现)。

### D-4. `tools/measure_prompt_baseline` CLI 设计

**决策**: 单一可执行 + argparse + YAML 输出。参数: `--prompt V1|V2|V3`(必填)、`--golden-dir`(默认 `lib/prompts/golden/`)、`--output YAML`(必填)、`--max-tasks`(默认全量)、`--mock-mode`(CI 友好, 不调真实 LLM)。

YAML 输出 schema(机器可读,供 `evidence-gate` change 直接消费):

```yaml
baseline_id: 2026-XX-XX-V1
prompt_version: V1
golden_tasks_total: 50
parse_valid_rate: 0.86              # [0, 1]
task_success_rate: { L1: 0.72, L2: 0.51, L3: 0.28 }   # 分层
per_dimension: { parse_valid: 0.86, task_success: 0.50, budget_hit: 0.10, error_recovery: 0.65 }
confidence_interval: { parse_valid: [0.78, 0.92] }   # 95% CI 二项分布
mock_mode: false
timestamp: 2026-XX-XXTHH:MM:SSZ
```

**替代方案拒绝**: JSON 输出(提案明确 YAML);直接调用 OpenAI/Anthropic API(避免引入 LLM API key 依赖)。

### D-5. V1/V2/V3 代码组织

**决策**: `src/common/prompts/{prompt_builder.h, v1.cpp, v2.cpp, v3.cpp}` + `tests/test_prompt_v1_v2_v3.cpp`(3 单元测试覆盖 V3 stage ordering 不变量)。`CMakeLists.txt` 用 `target_include_directories`(符合项目 anti-pattern 禁令,禁用 `include_directories()`)。

### D-6. Baseline 报告格式

**决策**: Markdown(`.md`)嵌 YAML block,路径 `docs/audits/<date>-execution-baseline-v1.md`(与 `2026-08-03-adr-0068-ship-gate.md` 格式一致)。必备章节: §1 Ship Gate 评分 + §2 V1/V2/V3 对比表 + §3 per-dimension 分解 + §4 测量日志 + §5 Open Issues,同级目录 `.yaml` 报告**必须** git-tracked。

### D-7. 架构合规性检查(强制)

**决策**: tasks.md 含独立架构合规验证任务 + ship 前 `tools/adr_lint.py` + `tools/docs_drift_audit.py` 双跑 PASS。合规依据: ADR-0074 §决策 D1/D2/D3 子集(本 change 范围)、ADR-0073 §决策(V1 schema 引用 ToolMetadata V3 schema)、[ADR-0008](../../docs/adr/adr-0008-structured-context.md)(Prompt 注入 working/episodic/semantic 三层,本 change 默认 working 层)、ADR-0068 §附录 A(本 change **不**注册新主题)。

## Risks / Trade-offs

| 风险 | 缓解 |
|------|------|
| **[Risk-1]** Few-shot 数据质量不一致 | 每 example 加 `rationale` + 架构组 review checklist (8 条); CI grep 验证每维度 ≥ 8 |
| **[Risk-2]** Golden suite 通过 prompt 泄漏 | ship 前 `scripts/verify_golden_holdout.sh` grep 强制 0 匹配; 纳入 sprint-closeout Step 6 |
| **[Risk-3]** V3 token 溢出(few-shot × 5 + schema > 8k) | few-shot 封顶 ≤ 5(从 32 抽样); 发送前 token 计数; > 8k 报警 |
| **[Risk-4]** parse-valid 率测量噪声(50 tasks 样本偏小) | 50 tasks 聚合 + 报告 95% CI; evidence-gate 阶段 3 模型 × 50 = 150 样本扩 CI |
| **[Risk-5]** Mock mode 与真实 LLM 偏差 | mock 仅 CI 用; 真实 LLM 测量 evidence-gate 跑; 报告区分 mock/real mode |
| **[Risk-6]** V3 Stage 1 范围蔓延 | tasks.md 显式声明 Stage 1 deferred, V3 仅 system→user 消息拆分 |

## Migration Plan

本 change **不涉及** production 代码变更(`src/common/prompts/` + `lib/prompts/` 为全新目录),无回滚策略。

部署步骤:

1. **C1+C2** ship: 32 few-shot + 51 golden tasks 入库(`git commit` + `scripts/verify_golden_holdout.sh` PASS);
2. **C3 代码** ship: V1/V2/V3 + CLI 实施 + 单元测试 PASS + ctest 零回归;
3. **C3 测量** ship: 跑 mock mode baseline + 发布 `docs/audits/<date>-execution-baseline-v1.{md,yaml}`;
4. **handoff**: 输出交接文档给 `evidence-gate` change(独立 change 消费 baseline 报告做 Go/No-Go 决议)。

回滚: 若 C3 测量发现 parse-valid 率远低于 85% 阈值(mock mode 仅 sanity check),无需回滚代码,仅在 baseline 报告标注"测量异常, 待 evidence-gate 重跑"。

## Open Questions

- **Q-1**: V3 是否包含 ADR-0074 §决策 D5 Stage 1 subgraph 选择?本 change 假设 V3 仅做"system→user"消息拆分, Stage 1 留给 Phase 6d C5, 待 `evidence-gate` 范围确认。
- **Q-2**: Few-shot 来源是否允许从 `examples/agent_basic/.agent.md` 迁移?提案未明确, 建议允许但需架构组 review license / attribution。
- **Q-3**: Mock mode baseline 是否产出独立报告(不与真实 LLM 测量混用)?建议是, 待 `evidence-gate` 确认。