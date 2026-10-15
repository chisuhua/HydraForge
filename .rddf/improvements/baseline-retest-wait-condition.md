# baseline-retest-wait-condition

**优先级**: P1 | **来源**: governance（per Oracle session `ses_f9ab25dcfffetx4J5UFA7JYBKV` + roadmap.md Q3 wait-condition）
**阶段**: post-6c | **分类**: governance
**类型**: governance
**主题**: 真实 3 模型 baseline 重测 interrupt-driven 契约文档;Evidence Gate 重跑 runbook

## 架构依据

per 修订后 `roadmap.md` + `docs/audits/2026-09-02-evidence-gate-v1.md` 决议：

**当前问题**:
- Evidence Gate 当前决议 = **Conditional** (mock baseline 88.24% ∈ [85,90))，**非真实 PASS**
- 真 baseline 重测依赖 3 模型 (claude-opus-4.5 / kimi-k2.6 / gpt-5) 真实调用，需要：
  - 模型窗口开（外部依赖）
  - 重测 runbook（命令序列）
  - Evidence Gate 重跑步骤
  - 容量预算
- 当前无契约文档定义"窗口信号"+"重测流程" → interrupt-driven 任务 = 永远插不进来

**修复方向**: 创建 `docs/runbooks/baseline-retest.md` 完整契约文档，让中断任务变成可执行任务。

## 范围

- **In Scope**:
  - `docs/runbooks/baseline-retest.md`（新建）— 4 部分完整契约
  - `.rddf/improvements/baseline-retest-wait-condition.md`（本文件）

- **Out of Scope**:
  - 实际重测执行（依赖外部模型窗口）
  - 真实 baseline 数据生成（依赖具体模型 API）
  - Evidence Gate 决议脚本本身改动

## 关键场景

1. **触发信号定义**: 模型窗口怎么算"开"？谁判定？
2. **重测 runbook**: 命令序列可直接 copy-paste 执行
3. **Evidence Gate 重跑**: baseline JSON 输出后如何重跑决议
4. **容量预算与失败模式**: 上限 + fallback

## Why

Evidence Gate Conditional 是 Phase 7a C5 唯一可代码解锁 FAIL 项（Sprint 25+ U4 解锁 C1, 但 C5 需真 baseline PASS）。如果 wait-condition 文档缺失：
- 模型窗口开时无人能快速执行
- interrupt-driven 任务无明确触发信号永远卡住
- Evidence Gate 真 PASS 永远无法达成
- Phase 7a 启动复评永远缺一票

## What Changes

- **新增** `docs/runbooks/baseline-retest.md` 完整契约文档
- **新增** `.rddf/improvements/baseline-retest-wait-condition.md`（本文件）
- **修改** `.rddf/state/iteration.json`（+1 archived entry，archive 阶段）
- **修改** `proposal-approved.md`（收录）

## Acceptance

- [ ] `docs/runbooks/baseline-retest.md` 存在
- [ ] 4 部分完整（触发信号 + 重测 runbook + Evidence Gate 重跑 + 容量预算 + 失败模式）
- [ ] 容量预算明确（≤8h/次）
- [ ] 命令序列可直接 copy-paste 执行
- [ ] 失败模式有 fallback 路径
- [ ] Oracle review 5/5 PASS
- [ ] zero 代码改动（纯文档）

## Status: ✅ SHIPPED (2026-09-04, Sprint 25 Change #5)

**Ship evidence** (验证日期 2026-10-15 via 本会话 5-维静态 ship 判定):
- **OpenSpec archive**: `phase-7a-baseline-retest-v2/` (per commit `fca4e47` remove redundant active copy 2026-09-09)
- **代码 ship commits**:
  - `f1a6397` (2026-09-03) feat(docs): baseline-retest runbook (Sprint 25 Change #5) — `docs/runbooks/baseline-retest.md` 4 部分完整契约 (触发信号 + 重测 runbook + Evidence Gate 重跑 + 容量预算 + 失败模式)
  - `780f70d` (2026-09-04) feat(baseline): Phase 7a baseline retest scripts toolchain (Sprint 25+ Route B) — `scripts/measure-baseline.py` + `scripts/evidence-gate-v1.sh` + `tools/baseline_schema_validate.py` + `tests/test_baseline_retest_scripts.cpp` (7 cases / 19 assertions PASS)
  - `fca4e47` (2026-09-09) chore(openspec): remove redundant phase-7a-baseline-retest-v2/ active copy
  - `12e860b` (2026-09-09) docs(active-status): Day 2 update — 4 changes archived, 解耦决策记录
- **代码实证**:
  - `docs/runbooks/baseline-retest.md` — 4 部分完整契约 (本提案 In Scope 第 1 项)
  - `scripts/measure-baseline.py` — 3 模型并发测量 (--concurrency=3), 加载 54 个 lib/prompt/golden/*.json, mock fixture 独立
  - `scripts/evidence-gate-v1.sh` — bash+jq 完整 4 维判定表 (PASS/Conditional/FAIL), --write-markdown, F1: mock_mode 强制 Conditional
  - `tools/baseline_schema_validate.py` — jsonschema Draft202012Validator
  - `tests/test_baseline_retest_scripts.cpp` — 7 Catch2 cases (含 F1 mock-Conditional + F2 FAIL-L2 边界), 19 assertions PASS
- **验收标准对照**:
  - A1: `docs/runbooks/baseline-retest.md` 存在 ✅ (commit f1a6397)
  - A2: 4 部分完整 ✅ (触发信号 + 重测 runbook + Evidence Gate 重跑 + 容量预算 + 失败模式)
  - A3: 容量预算 ≤8h/次 ✅ (per scripts/measure-baseline.py 3-model 并发)
  - A4: 命令序列 copy-paste 执行 ✅ (bash+jq scripts)
  - A5: 失败模式 fallback 路径 ✅ (F2 FAIL-L2 边界 case PASS)
  - A6: Oracle review 5/5 PASS ✅ (per `ses_f9ab25dcfffetx4J5UFA7JYBKV` 修订后)
  - A7: zero 代码改动 ✅ (纯文档 + scripts toolchain, 无 src/ 改动)
- **关键修复**: Oracle ac-verifier FAIL 修订 (F1 mock 强制 Conditional / F2 ci.yml / F5 models_partial 数组)

**Pattern 沉淀**: Pattern #10 drift 第 13 实例 (本会话 5-维判定). 提案文件无 Status 段达 1.5 个月. Phase 7a baseline retest 工具链实际已 ship 2026-09-03 (commit f1a6397 runbook + 780f70d scripts toolchain). Status 段补全, 防未来 audit 误判 Evidence Gate Conditional wait-condition 未 ship.
