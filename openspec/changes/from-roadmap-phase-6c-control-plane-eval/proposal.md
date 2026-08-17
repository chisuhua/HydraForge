# from-roadmap-phase-6c-control-plane-eval

## Why

ADR-0079 Control Plane 启动决策树（per active-status.md §四 4 项条件）：

- 启动条件 1：AgentForge ≥2 agent 已 ship（当前仅 1 个）。
- 启动条件 2：Solo Dev 容量 ≥2 人 OR ≥80h/双周（当前 1 人 ~27h/周）。
- 启动条件 3：ADR-0068 §附录 A amendment PR 14 候选主题 ship ✅（2026-08-13 archived）。
- 启动条件 4：ADR-0073 完整 ship D2+D3（当前 D2/D4 部分 ship，D3 待 C9）。
- 启动条件 5：Evidence Gate PASS（parse-valid ≥85% + task-success L1 ≥70%）。
- 启动条件 6：ADR-0075 EnvBackend ship（Local+Docker）。

## What Changes

**In Scope**:

- `scripts/control-plane-eval.py` 一键评估脚本（输入 6 项条件状态 + 输出决策表）。
- `docs/audits/<date>-control-plane-eval-v1.md` 决议文档模板（含每项条件引用 + 决策表 + 后续路径）。
- 决策树：6 项全 PASS → Phase 7 启动；任意 FAIL → descope 或继续前置 ship。
- Sprint Review 入项（active-status.md §一 6 项条件状态）。
- 3 类测试：全 PASS / 部分 FAIL / 全 FAIL 路径。
- **Out of Scope**:
- 6 项条件本身的实施（依赖各 ADR ship）。
- Control Plane 实际 ship（依赖 Phase 7a/b/c 提案）。
- 自动重新评估（手动运行）。

### 关键场景

- GIVEN 6 项条件：①❌ ②❌ ③✅ ④🟡 ⑤❌ ⑥❌
  WHEN control-plane-eval.py 执行
  THEN 输出 descope 建议（条件 1+2 阻塞 → descope 或 descope 启动）。

- GIVEN 6 项条件全部 ✅
  WHEN 执行
  THEN 输出 PASS，建议立即启动 Phase 7a。

- GIVEN 条件 4 ADR-0073 仍 🟡 Partial（D3 待 C9）
  WHEN 执行
  THEN 输出 CONDITIONAL，建议 ship C9 后再决议。

**Out of Scope**:

- (no items specified)

## Capabilities

- MUST 6 项条件状态自动检测（git log + openspec validate + docs grep）。
- MUST 决议文档 git-tracked + 与 active-status.md 联动。
- MUST decision tree 公式化（脚本实现）。
- SHOULD 每项条件状态含证据 file:line 引用。

## Impact

- MUST NOT 引入 LLM 决策（human review only）。

## Acceptance

- control-plane-eval.py 可执行（3 路径测试）。
- 决议模板 + 决议文档 git-tracked。
- active-status.md 引用决议（≤ 24h 时差）。
- ctest 全量零回归。
- 阻塞 Phase 7 启动评估（phase-7a 前置）。

