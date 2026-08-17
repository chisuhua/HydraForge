## 1. Few-shot Examples 采集 (C1, ADR-0074 §决策 D1)

- [ ] 1.1 创建 `lib/prompts/fewshot/` 目录 + 在 `lib/prompts/README.md` 中记录 4 维度 taxonomy 与 4 字段 schema (`dimension / input / output / rationale`)
- [ ] 1.2 采集 32 个 examples (4 维度 × 8), 手工标注 + rationale 字段, 文件命名 `{dimension}_{01..08}.yaml`
- [ ] 1.3 编写 `tests/test_few_shot_examples.cpp` — 验证 4 维度各 ≥ 8 + 每 example 含 4 字段 + yaml-cpp 解析 PASS

## 2. Golden Suite YAML 采集 (C2, ADR-0074 §决策 D2)

- [ ] 2.1 创建 `lib/prompts/golden/` 目录 + `lib/prompts/golden/README.md` (51 任务列表 + 评分规则 + L1/L2/L3 分布 20/20/11)
- [ ] 2.2 采集 20 个 L1 + 20 个 L2 + 11 个 L3 tasks YAML, 6 领域覆盖 (auth/human/math/utils/inference/mcp), 文件命名 `{domain}_{NN}.yaml`
- [ ] 2.3 编写 `scripts/verify_golden_holdout.sh` — `grep -rn "<task_id>" lib/prompts/fewshot/` 强制 0 匹配 (per design.md D-2); 集成到 `scripts/sprint-closeout.sh` Step 6
- [ ] 2.4 编写 `tests/test_golden_suite.cpp` — 51 task 计数 + 5 字段校验 + ctest PASS

## 3. V1/V2/V3 Prompt 实施 (C3, ADR-0074 §决策 D3 + §决策 D5 子集)

- [ ] 3.1 创建 `src/common/prompts/` 目录 + `CMakeLists.txt` 注册到 `agenticdsl_common` (target_include_directories, 不用 include_directories)
- [ ] 3.2 实施 `src/common/prompts/prompt_builder.h` 抽象接口 + `v1.cpp` (schema constraint, 引用 ADR-0073 ToolMetadata V3 schema)
- [ ] 3.3 实施 `src/common/prompts/v2.cpp` (V1 + few-shot 注入, 从 `lib/prompts/fewshot/` 加载 ≤ 5 examples) + `token_counter.h` 发送前 token 计数
- [ ] 3.4 实施 `src/common/prompts/v3.cpp` (V2 + two-stage, 顺序固定 per design.md D-3) + 编写 `tests/test_prompt_v1_v2_v3.cpp` (3 单元测试覆盖 V3 stage ordering 不变量)

## 4. `tools/measure_prompt_baseline` CLI 实施 (C3, design.md D-4)

- [ ] 4.1 实施 `tools/measure_prompt_baseline.cpp` 主程序 (argparse 风格 + 5 参数校验: --prompt / --golden-dir / --output / --max-tasks / --mock-mode)
- [ ] 4.2 实施 scorer — 计算 parse-valid rate + task-success rate 分 L1/L2/L3 + per-dimension 4 维度分解 + 95% CI (二项分布)
- [ ] 4.3 实施 YAML 输出 (per design.md D-4 schema: baseline_id / prompt_version / parse_valid_rate / task_success_rate / per_dimension / confidence_interval / mock_mode / timestamp)
- [ ] 4.4 编写 `tests/test_measure_prompt_baseline.cpp` — mock mode 跑 3 tasks + 验证 YAML 输出 schema 合规 + ctest PASS

## 5. Baseline 测量 + 报告 + 文档 (C3, design.md D-6)

- [ ] 5.1 跑 `tools/measure_prompt_baseline --prompt V1|V2|V3 --mock-mode --output docs/audits/<date>-execution-baseline-v1.yaml` × 3 次 (V1/V2/V3 各 1 个 YAML)
- [ ] 5.2 编写 `docs/audits/<date>-execution-baseline-v1.md` (含 §1 Ship Gate 评分 + §2 V1/V2/V3 对比表 + §3 per-dimension 分解 + §4 测量日志 + §5 Open Issues), 链接同级 .yaml 报告
- [ ] 5.3 更新 `lib/prompts/README.md` — 补充 few-shot 和 golden suite 使用说明 + V1/V2/V3 差异说明 + measure_prompt_baseline 调用示例 + hold-out 验证命令
- [ ] 5.4 编写 handoff 文档 (`openspec/handoff/from-roadmap-phase-6c-execution-baseline.md`) 给 `evidence-gate` change (独立 change 消费 baseline 报告做 Go/No-Go 决议)

## 6. 架构合规验证 (强制)

- [ ] 6.1 跑 `python3 tools/adr_lint.py` (含 ADR-0074 §决策 D1/D2/D3 + ADR-0073 schema 引用 + ADR-0008 三层 Context + ADR-0068 §附录 A 不注册新主题) — 必须 0 errors
- [ ] 6.2 跑 `python3 tools/docs_drift_audit.py` — 必须 0 DRIFT items + 0 WARNING

## 7. 测试验证 (强制)

- [ ] 7.1 跑 `cmake --build build && ctest --output-on-failure` — 全量零回归 (147+ 测试 PASS, 含新增 `test_few_shot_examples` + `test_golden_suite` + `test_prompt_v1_v2_v3` + `test_measure_prompt_baseline`)
- [ ] 7.2 跑 `lsp_diagnostics` 在所有新增 `.cpp` / `.h` / `.yaml` / `.sh` 文件 — 必须 0 errors