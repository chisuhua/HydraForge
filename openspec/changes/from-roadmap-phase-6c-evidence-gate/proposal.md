# from-roadmap-phase-6c-evidence-gate

## Why

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

## What Changes

**In Scope**:

- 创建 `docs/audits/<date>-evidence-gate-v1.md` 决策文档模板（前置：data plan + 测量方法 + 决策树 + 行动项）。
- 实施 3 阈值决策树：parse-valid < 85% → C5 触发（C5 ADR-0072 D2 `$var`）+ C6 触发；85% ≤ parse-valid < 90% → C6 触发（临界带优化）；parse-valid ≥ 90% → C5/C6 跳过。
- Evidence Gate 第一次实际执行：消费 C3 baseline 数据，输出 PASS / FAIL / CONDITIONAL / ABORT 决议。
- 根据决议同步 `docs/active-status.md` §一（Phase 6c 状态行）与 §四（Phase 7 启动条件项 #1）。
- **Out of Scope**:
- baseline 测量本身（C1/C2/C3 属 `execution-baseline` 提案）。
- ADR-0072 D2/D3 的代码实施（C5/C6 属 `execution-dsl` 提案，由 Evidence Gate 决议触发）。
- 持续测量基础设施（regression cron / 每次 prompt 变更后 24h 测量）—— ADR-0074 §决策 D3 末项，留 Sprint 28+。
- 阈值本身的调整（ADR-0074 §不变量 4：阈值变更需架构组评审，不在本提案权限内）。

### 关键场景

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

**Out of Scope**:

- (no items specified)

## Capabilities

- MUST 使用 C3 baseline 产出的真实测量数据（audit 报告 `docs/audits/<date>-execution-baseline-v1.md`），禁止合成或外部推断。
- MUST 决策文档每个数值引用具体 file:line 证据（如 `parse-valid: 88.2% (baseline-v1.md:42)`），便于审计追溯。
- MUST 决策文档发布至 `docs/audits/<date>-evidence-gate-v1.md`（ADR-0074 §不变量 4 强制）。
- MUST 决议后 24h 内更新 `docs/active-status.md` §一（Phase 6c 状态行）与 §四（Phase 7 启动条件项 #1），与 §0 触发 ADR-0072 D5（如适用）。
- SHOULD 决策树实现为独立函数 `evaluate_gate(parse_valid, task_success_l1, task_success_l2, task_success_l3)` 接受 4 阈值输入，返回 4 状态枚举（PASS / FAIL / CONDITIONAL / ABORT），便于 Phase 8+ 复用。

## Impact

- MUST NOT 自动触发 C5/C6——决议文档仅记录建议；C5/C6 启动需独立 OpenSpec change + 人类评审（避免 baseline 异常数据下错误触发）。

## Acceptance

- [ ] `docs/audits/<date>-evidence-gate-v1.md` 已创建，含 §数据 plan + §测量方法 + §决策树 + §行动项 + §决议 5 章节。

---

## ⚠️ Errata (2026-08-25 MOMUS REJECT 触发)

> **来源**: MOMUS plan 评审 (`bg_03b93dbe`, 13m18s) — 完整 finding 见 `docs/audits/2026-08-25-evidence-gate-momus-decision-summary.md`

### MUST-FIX 2 修复记录 (scope-neutral, 待 user X/Y/Z/W 决策)

1. **§关键场景 1 矛盾**(原 88.2% → PASS,与决策树 [85,90) → CONDITIONAL 矛盾):
   - 真实测量数据(per `docs/audits/2026-08-18-execution-baseline-v1.md`)为 **mock_mode** (file: 报告 §1 + yaml:16 `mock_mode: true`),不构成真实 LLM 能力结论
   - 决策树 (design D-4): `[85.0%, 90.0%)` 左闭右开 → 88.24% 应判 **Conditional** (临界带),**非** PASS
   - 本 change 的裁决需真实 3 模型 baseline, 真实数据消费 scope 由 user 选 X/Y/Z/W 后 fill
2. **数据完整性 check** (per design D-3): `parse_valid < 0 OR > 1` 视为数据缺失, 立即 `Abort` (不进入阈值比较)
3. **L1/L2/L3 阈值判定**: v1 仅签名占位, 完整 D-4 "全部满足" 语义由 ADR-0074 §决策 D5 v2 amendment 实施 (Sprint 25+); 本 v1 决议依据为 `parse_valid` 单维度 + 数据完整性 check
4. **ADR-0074 状态已 Approved** (file: `docs/adr/adr-0074-prompt-evidence-gate.md:5`): 7.x "Proposed → Partial" 翻牌指令已删除, 改为 §决策 D5 实证字段追加决议记录

### Acceptance 修正

- [ ] `src/common/prompts/evidence_gate.h` header-only 纯函数 ship (4 状态 × 4 阈值, per D-3 数据完整性 + D-4 左闭右开)
- [ ] `tests/test_evidence_gate.cpp` ≥5 case 全 PASS (4 boundary + ≥1 Abort)
- [ ] `docs/audits/evidence-gate-v1.md.template` 决议文档 5 章节 skeleton ship (X 路线; Y/Z 路线填入实际数据)
- [ ] `tools/adr_lint.py` + `tools/docs_drift_audit.py` + `openspec validate` 全 exit 0
- [ ] `ctest --output-on-failure` 全量 184/184 PASS 不变 (per AGENTS.md 2026-08-25 ground truth, per MUST-FIX 3 修正)
- [ ] (X 路线) 真实 LLM 数据决议 ship 推迟至独立 OpenSpec change (Sprint 25+); 决议文档 ship 仅占位 + 模板
- [ ] 决策树实现为 `evaluate_gate()` 函数（4 阈值输入 → 4 状态输出），附 3 边界用例单元测试（parse-valid=84.9/85.0/89.9/90.0 → FAIL/CONDITIONAL/CONDITIONAL/PASS）。
- [ ] 所有决议数值引用 C3 baseline 报告（`docs/audits/<date>-execution-baseline-v1.md`），每个数字含 file:line 链。
- [ ] 3 阈值（parse-valid 85% / task-success L1 70% / parse-valid 临界带 90%）明确定义，与 ADR-0074 D4 原文对齐。
- [ ] `docs/active-status.md` §一 Phase 6c 状态行 + §四 Phase 7 启动条件项 #1 已更新（24h 内）；Evidence Gate 决议结果（PASS/FAIL/CONDITIONAL/ABORT）在 §一可见。
- [ ] ADR-0074 D4 状态：若决议 PASS → 状态从 🔍 Proposed 翻牌 🟡 Partial（W2 baseline 已 ship + Evidence Gate 已 ship）；若任一阈值 FAIL → 保持 🔍 Proposed，决议录入 ADR-0074 §决策 D4 实证字段。

