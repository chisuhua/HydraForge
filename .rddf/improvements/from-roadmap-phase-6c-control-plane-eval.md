# from-roadmap-phase-6c-control-plane-eval

**优先级**: P1 | **来源**: from-roadmap (phase-6c/control-plane-eval)
**阶段**: phase-6c | **分类**: control-plane-eval
**类型**: governance
**主题**: 启动条件评估；Control Plane决策树

## 架构依据

ADR-0079 Control Plane 启动决策树（per active-status.md §四 4 项条件）：

- 启动条件 1：AgentForge ≥2 agent 已 ship（当前仅 1 个）。
- 启动条件 2：Solo Dev 容量 ≥2 人 OR ≥80h/双周（当前 1 人 ~27h/周）。
- 启动条件 3：ADR-0068 §附录 A amendment PR 14 候选主题 ship ✅（2026-08-13 archived）。
- 启动条件 4：ADR-0073 完整 ship D2+D3（当前 D2/D4 部分 ship，D3 待 C9）。
- 启动条件 5：Evidence Gate PASS（parse-valid ≥85% + task-success L1 ≥70%）。
- 启动条件 6：ADR-0075 EnvBackend ship（Local+Docker）。

## 范围

- **In Scope**:
  - `scripts/control-plane-eval.py` 一键评估脚本（输入 6 项条件状态 + 输出决策表）。
  - `docs/audits/<date>-control-plane-eval-v1.md` 决议文档模板（含每项条件引用 + 决策表 + 后续路径）。
  - 决策树：6 项全 PASS → Phase 7 启动；任意 FAIL → descope 或继续前置 ship。
  - Sprint Review 入项（active-status.md §一 6 项条件状态）。
  - 3 类测试：全 PASS / 部分 FAIL / 全 FAIL 路径。
- **Out of Scope**:
  - 6 项条件本身的实施（依赖各 ADR ship）。
  - Control Plane 实际 ship（依赖 Phase 7a/b/c 提案）。
  - 自动重新评估（手动运行）。

## 关键场景

- GIVEN 6 项条件：①❌ ②❌ ③✅ ④🟡 ⑤❌ ⑥❌
  WHEN control-plane-eval.py 执行
  THEN 输出 descope 建议（条件 1+2 阻塞 → descope 或 descope 启动）。

- GIVEN 6 项条件全部 ✅
  WHEN 执行
  THEN 输出 PASS，建议立即启动 Phase 7a。

- GIVEN 条件 4 ADR-0073 仍 🟡 Partial（D3 待 C9）
  WHEN 执行
  THEN 输出 CONDITIONAL，建议 ship C9 后再决议。

## 技术约束

- MUST 6 项条件状态自动检测（git log + openspec validate + docs grep）。
- MUST 决议文档 git-tracked + 与 active-status.md 联动。
- MUST decision tree 公式化（脚本实现）。
- MUST NOT 引入 LLM 决策（human review only）。
- SHOULD 每项条件状态含证据 file:line 引用。

## 验收标准

- control-plane-eval.py 可执行（3 路径测试）。
- 决议模板 + 决议文档 git-tracked。
- active-status.md 引用决议（≤ 24h 时差）。
- ctest 全量零回归。
- 阻塞 Phase 7 启动评估（phase-7a 前置）。

## Status: 🟡 PARTIAL (2026-10-15 audit verification, per 2026-10-14 audit methodology)

**Status 段补全** (per Pattern #10 hygiene, 2026-10-15 audit verification):

本提案**部分 ship** (与 OpenSpec claim 范围不匹配):

- **✅ ship 范围** (per commit `a742195` 2026-09-03 + `886ce66` 2026-10-15 hygiene):
  - `scripts/control-plane-eval.py` C2 fail-soft (--relaxed mode) — 仅 C2 (Solo Dev 容量) 路径
  - 5 类测试 (`tests/test_control_plane_eval_relaxed.py`): 默认保持 + relaxed 3 case + C1 仍阻塞 + C2 PASS 无影响
  - Oracle SHIP-with-fixes verdict `ses_eebfb88b` (0 Critical / 2 Major / 4 Minor)
- **❌ 未 ship 范围** (per proposal 验收标准对比):
  - 完整 6 项启动条件一键评估脚本 (C1+C2+C3+C4+C5+C6)
  - 3 路径测试 (全 PASS / 部分 FAIL / 全 FAIL)
  - 决议模板 + 决议文档 git-tracked
  - active-status.md 引用决议

**决议**: 🟡 PARTIAL 标记 (per本日 hygiene 模式 5 维判定 + 本会话 P0 audit 实证)

**关联 ship 提案**:
- `control-plane-eval-c2-alignment` (commit `886ce66` 2026-10-15) 已 Status 段 ✅ ship
- 本提案范围"完整 6 项"是 5-维判定中"承诺 vs 实际"差距, **不是 Pattern #10 drift** (头注无 SUPERSEDED 标记 + 有部分 ship 实证)

**Pattern 沉淀**: 提案文件"完整 N 项"claim vs 实际"部分 M 项 ship" 范围不匹配时, 应标 🟡 PARTIAL 而非 ✅ SHIPPED, 防止未来 audit 误判.