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
