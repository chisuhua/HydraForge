# from-roadmap-phase-6c-evidence-gate

**优先级**: P0 | **来源**: from-roadmap (phase-6c/evidence-gate)
**阶段**: phase-6c | **分类**: evidence-gate
**类型**: governance
**主题**: Evidence Gate决议；parse-valid阈值；task-success阈值

## 架构依据

**Evidence Gate 概念**（per ADR-0074 §决策 D4）：基于经验数据的 Go/No-Go 决策机制，使用 parse-valid 与 task-success L1/L2/L3 三个量化阈值评估 Prompt + DSL 现状，作为 Wave 2 → Wave 3 推进的唯一客观标准。不允许"感觉差不多"的隐性推进。

**ADR-0074 §决策 D4 阈值**（3 模型平均）：

| 指标 | 阈值 | 测量依据 |
|------|:----:|---------|
| parse-valid | ≥85% | D3 baseline |
| task-success L1 | ≥70% | D3 baseline |
| task-success L2 | ≥50% | D3 baseline |
| task-success L3 | ≥30% | D3 baseline |

**Evidence Gate 政策框架**（per `docs/active-status.md` §四）：Phase 7 Control Plane 启动条件第 1 项 = "Phase 6c Evidence Gate PASS (parse-valid ≥85% + task-success L1 ≥70%)"；当前 5/6 启动条件未满足（待 C4 决议）。

**DSL 扩展条件触发**（per ADR-0072 §决策 D2/D3 conditional triggers）：Evidence Gate 三阈值划分对应 ADR-0072 D2/D3 触发条件——D2 `$var` 在 parse-valid < 85% 触发；D3 declarative style 在 85% ≤ parse-valid < 90% 临界带触发；D5 双语法共存期在 D2+D3 触发后强制。

**Wave 推进政策**（ADR-0074 §不变量 4）：Evidence Gate 不可绕过——任何 Wave 推进必须附 Evidence Gate 决议文档（即 `docs/audits/<date>-evidence-gate-v1.md`）。

## 范围

- **In Scope**:
  - 创建 `docs/audits/<date>-evidence-gate-v1.md` 决策文档模板（前置：data plan + 测量方法 + 决策树 + 行动项）。
  - 实施 3 阈值决策树：parse-valid < 85% → C5 触发（C5 ADR-0072 D2 `$var`）+ C6 触发；85% ≤ parse-valid < 90% → C6 触发（临界带优化）；parse-valid ≥ 90% → C5/C6 跳过。
  - Evidence Gate 第一次实际执行：消费 C3 baseline 数据，输出 PASS / FAIL / CONDITIONAL / ABORT 决议。
  - 根据决议同步 `docs/active-status.md` §一（Phase 6c 状态行）与 §四（Phase 7 启动条件项 #1）。
- **Out of Scope**:
  - baseline 测量本身（C1/C2/C3 属 `execution-baseline` 提案）。
  - ADR-0072 D2/D3 的代码实施（C5/C6 属 `execution-dsl` 提案，由 Evidence Gate 决议触发）。
  - 持续测量基础设施（regression cron / 每次 prompt 变更后 24h 测量）—— ADR-0074 §决策 D3 末项，留 Sprint 28+。
  - 阈值本身的调整（ADR-0074 §不变量 4：阈值变更需架构组评审，不在本提案权限内）。

## 关键场景

- GIVEN C3 baseline 报告显示 parse-valid = 88.2% + task-success L1 = 73.5%（≥85% / ≥70%）
  WHEN C4 Evidence Gate 评估执行
  THEN 决议 **PASS**——仅 ship baseline（不触发 C5/C6），active-status.md §一更新 "Evidence Gate PASS = true"。

- GIVEN C3 baseline 报告显示 parse-valid = 82.1%（< 85%）
  WHEN C4 Evidence Gate 评估执行
  THEN 决议 **FAIL**——触发 ADR-0072 D2 `$var` 实施（C5，8h P0*），resolution 写入决议文档 §行动项。

- GIVEN C3 baseline 报告显示 parse-valid = 87.5%（85% ≤ x < 90% 临界带）
  WHEN C4 Evidence Gate 评估执行
  THEN 决议 **CONDITIONAL**——触发 ADR-0072 D3 declarative style（C6，4h P0*），不触发 C5，决议文档 §临界带说明记录。

- GIVEN C3 测量数据缺失或 incomplete（golden suite < 50 tasks / 3 模型未全部报告 / YAML 报告字段缺漏）
  WHEN C4 Evidence Gate 评估执行
  THEN 决议 **ABORT**——审计文档 §数据完整性检查小节列出缺失项，要求 C1+C2+C3 重新测量，Evidence Gate 不裁决。

## 技术约束

- MUST 使用 C3 baseline 产出的真实测量数据（audit 报告 `docs/audits/<date>-execution-baseline-v1.md`），禁止合成或外部推断。
- MUST 决策文档每个数值引用具体 file:line 证据（如 `parse-valid: 88.2% (baseline-v1.md:42)`），便于审计追溯。
- MUST 决策文档发布至 `docs/audits/<date>-evidence-gate-v1.md`（ADR-0074 §不变量 4 强制）。
- MUST 决议后 24h 内更新 `docs/active-status.md` §一（Phase 6c 状态行）与 §四（Phase 7 启动条件项 #1），与 §0 触发 ADR-0072 D5（如适用）。
- MUST NOT 自动触发 C5/C6——决议文档仅记录建议；C5/C6 启动需独立 OpenSpec change + 人类评审（避免 baseline 异常数据下错误触发）。
- SHOULD 决策树实现为独立函数 `evaluate_gate(parse_valid, task_success_l1, task_success_l2, task_success_l3)` 接受 4 阈值输入，返回 4 状态枚举（PASS / FAIL / CONDITIONAL / ABORT），便于 Phase 8+ 复用。

## 验收标准

- [ ] `docs/audits/<date>-evidence-gate-v1.md` 已创建，含 §数据 plan + §测量方法 + §决策树 + §行动项 + §决议 5 章节。
- [ ] 决策树实现为 `evaluate_gate()` 函数（4 阈值输入 → 4 状态输出），附 3 边界用例单元测试（parse-valid=84.9/85.0/89.9/90.0 → FAIL/CONDITIONAL/CONDITIONAL/PASS）。
- [ ] 所有决议数值引用 C3 baseline 报告（`docs/audits/<date>-execution-baseline-v1.md`），每个数字含 file:line 链。
- [ ] 3 阈值（parse-valid 85% / task-success L1 70% / parse-valid 临界带 90%）明确定义，与 ADR-0074 D4 原文对齐。
- [ ] `docs/active-status.md` §一 Phase 6c 状态行 + §四 Phase 7 启动条件项 #1 已更新（24h 内）；Evidence Gate 决议结果（PASS/FAIL/CONDITIONAL/ABORT）在 §一可见。
- [ ] ADR-0074 D4 状态：若决议 PASS → 状态从 🔍 Proposed 翻牌 🟡 Partial（W2 baseline 已 ship + Evidence Gate 已 ship）；若任一阈值 FAIL → 保持 🔍 Proposed，决议录入 ADR-0074 §决策 D4 实证字段。
