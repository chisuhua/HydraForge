# from-roadmap-phase-6b-execution-baseline

> **status: SUPERSEDED by from-roadmap-phase-6c-execution-baseline (2026-09-02)** — 2026-09-02 cleanup.
> Phase 6c 版已 archived (per `iteration.json` 8/8 archived + Phase 6c 2026-09-02 ship 收官).
> 本文件保留作历史设计意图记录，详见 `proposal-suggestions.md` §3.2.

**优先级**: P0 | **来源**: from-roadmap (phase-6b/execution-baseline, ADR-0074 D1+D2+D3)
**阶段**: phase-6b | **分类**: execution-baseline
**类型**: feature
**主题**: few-shot examples；golden suite；V1/V2/V3 prompt

## 架构依据

ADR-0074 baseline（Prompt 模板基础设施）要求 3 件 ship 才能让 Execution Plane 启动评估有依据：

- ADR-0074 D1 few-shot examples 30+ 采集：4 维度 × 8 examples（结构化 / 工具调用 / 错误恢复 / 长上下文）。
- ADR-0074 D2 held-out golden suite 50 tasks YAML：保留任务集，回归验证。
- ADR-0074 D3 V1/V2/V3 prompt 实施 + 测量：V1 schema 约束 + V2 few-shot 注入 + V3 两阶段注入（独立 + 组合）。
- 4 维度 × 8 examples × 50 tasks 形成测量矩阵（2400 baseline 测量样本）。

## 范围

- **In Scope**:
  - `docs/baselines/few-shot-30.md` 4 维度 × 8 examples 内容。
  - `docs/baselines/golden-suite-50.yaml` 50 task YAML + 期望输出。
  - `include/agenticdsl/prompt/v1_schema.h` V1 schema 约束实现（约束 prompt 字段必填）。
  - `include/agenticdsl/prompt/v2_fewshot.h` V2 few-shot 注入（运行时按 4 维度选择 examples）。
  - `include/agenticdsl/prompt/v3_two_phase.h` V3 两阶段注入（独立 prompt + 组合 prompt）。
  - `tests/test_prompt_baseline.cpp` 3 类测试（V1 schema 拒绝非法 / V2 few-shot 注入正确 / V3 两阶段独立性）。
  - `tools/measure-baseline.py` baseline 测量脚本（2400 样本运行 + parse-valid 统计）。
- **Out of Scope**:
  - LLM 调用（依赖现有 `LLMProvider`）。
  - 基模选型（ADR-0078 defer）。
  - Evidence Gate 决议本身（→ execution-baseline evidence-gate 提案）。

## 关键场景

- GIVEN V1 schema 约束 prompt 缺少 `user_input` 字段
  WHEN LLM 调用
  THEN 返回 `schema_validation_error`，不进入 provider。

- GIVEN V2 few-shot 维度 = "tool_call"
  WHEN prompt 构造
  THEN 自动注入 8 个 tool_call examples 到 LLM 上下文。

- GIVEN V3 两阶段注入（独立 + 组合）
  WHEN prompt 构造
  THEN 独立阶段 prompt 与组合阶段 prompt 分别生成（互不污染）。

- GIVEN baseline 测量脚本运行 2400 样本
  WHEN 测量完成
  THEN 输出 `parse-valid: X%` `task-success: Y%`，存入 `docs/baselines/measurement-2026-XX.json`。

## 技术约束

- MUST baseline 数据与代码脱钩（docs/baselines/ 目录，git-tracked）。
- MUST V1/V2/V3 实现独立可测（单元测试覆盖每件）。
- MUST measurement 脚本记录 sample_id + latency + parse-valid 三元组（可回放）。
- MUST NOT 修改现有 `ILLMProvider` 接口（baseline 是 prompt 层，不影响 provider）。
- SHOULD 4 维度 × 8 examples 与 50 tasks 在内容上正交（避免测量偏差）。

## 验收标准

- few-shot-30.md + golden-suite-50.yaml 落地。
- V1/V2/V3 三个模块独立单元测试通过。
- measurement 脚本可重现运行（2400 样本，输出 JSON）。
- ctest 全量零回归。
- 阻塞 ADR-0074 D4 Evidence Gate 决议（→ evidence-gate 提案）。

## Status: ⛔ SUPERSEDED by from-roadmap-phase-6c-execution-baseline (2026-09-02)

**Status 段补全** (per Pattern #10 hygiene, 2026-10-15 audit verification):

**5-维判定**:
- 维度 1 self-declared: header `> **status: SUPERSEDED by from-roadmap-phase-6c-execution-baseline (2026-09-02)**` 已标注
- 维度 2 archive: ✅ `openspec/changes/archive/2026-09-01-from-roadmap-phase-6c-execution-baseline/` (per commit `911d19f`)
- 维度 3 commit: ✅ commit `911d19f` (mark 6 Phase 6b proposals SUPERSEDED)
- 维度 4 ADR: 关联 ADR-0074 D4 Evidence Gate 决议 (状态 待 audit 验证)
- 维度 5 fallback: 6c-execution-baseline 已 ship (per `from-roadmap-phase-6c-execution-baseline.md:## Status: ✅ SHIPPED (2026-10-14 audit verification)`)

**关联 ship 提案**:
- `from-roadmap-phase-6c-execution-baseline` ✅ SHIPPED (早 session 2026-10-14 audit 实证)
- 6c 版替代 6b 版 execution-baseline 测量 (per `iteration.json` 8/8 archived + Phase 6c 2026-09-02 ship 收官)

**判定结论**: ⛔ SUPERSEDED (本提案 6b execution-baseline 测量被 6c-execution-baseline 替代 ship, 本提案保留作历史设计意图记录, 详见 `proposal-suggestions.md` §3.2)

**Pattern 沉淀**: 6b 头注 `> **status: SUPERSEDED by from-roadmap-phase-6c-execution-baseline (2026-09-02)**` 已标注 (commit `911d19f`), 但**缺标准 ## Status 段** (本日 hygiene 模式 5 字段格式). 本 commit 补全标准化 Status 段.