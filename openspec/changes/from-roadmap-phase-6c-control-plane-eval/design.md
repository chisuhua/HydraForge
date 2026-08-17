## Context

ADR-0079 Control Plane 启动决策树（per active-status.md §四 6 项条件）：

- 启动条件 1：AgentForge ≥2 agent 已 ship（当前仅 1 个）。
- 启动条件 2：Solo Dev 容量 ≥2 人 OR ≥80h/双周（当前 1 人 ~27h/周）。
- 启动条件 3：ADR-0068 §附录 A amendment PR 14 候选主题 ship ✅（2026-08-13 archived）。
- 启动条件 4：ADR-0073 完整 ship D2+D3（当前 D2/D4 部分 ship，D3 待 C9）。
- 启动条件 5：Evidence Gate PASS（parse-valid ≥85% + task-success L1 ≥70%）。
- 启动条件 6：ADR-0075 EnvBackend ship（Local+Docker）。

当前评估缺口：
- `scripts/control-plane-eval.py` 一键评估脚本不存在（需手动跑 6 项条件判断）；
- `docs/audits/` 目录尚无 control-plane-eval 决议模板；
- `docs/active-status.md` §四 6 项条件当前为分散状态字段（人工汇总），需集中评估脚本；
- 决策树逻辑散落在多个 ADR 章节（ADR-0068/0073/0075/0079），无统一入口。

本提案聚焦评估机制本身——提供脚本 + 模板 + 测试，**不**实施 6 项条件（依赖各 ADR ship）。

## Goals / Non-Goals

**Goals:**

1. **`scripts/control-plane-eval.py` 一键评估脚本**：输入 6 项条件状态 + 输出决策表（PASS / FAIL / CONDITIONAL / ABORT）。
2. **`docs/audits/<date>-control-plane-eval-v1.md` 决议文档模板**：含每项条件引用 + 决策表 + 后续路径建议。
3. **决策树公式化**：6 项全 PASS → Phase 7 启动建议；任意 FAIL → descope 或继续前置 ship。
4. **每项条件状态含证据 file:line 引用**：grep git log + openspec validate + docs 自动检测。
5. **3 类测试**：全 PASS / 部分 FAIL / 全 FAIL 路径。
6. **架构合规性 + 零回归**：`ctest --output-on-failure` 全量零回归（147/147 baseline + 新增测试 PASS）。
7. **active-status.md 联动**：决议文档 ship 后 24h 内同步 active-status.md §四 Phase 7 启动条件项状态。

**Non-Goals:**

- 6 项条件本身的实施（依赖各 ADR ship，本提案不重做）。
- Control Plane 实际 ship（依赖 Phase 7a/b/c 提案，留 Sprint 25+）。
- 自动重新评估（手动运行 `scripts/control-plane-eval.py`，避免 CI 频繁触发）。
- LLM 决策引入（per proposal Impact §MUST NOT — human review only）。

## Decisions

### D-1. 评估脚本用 Python（与项目元工具一致）

**决策**: `scripts/control-plane-eval.py` 使用 Python 3.11+，复用项目元工具 `tools/adr_lint.py` / `tools/docs_drift_audit.py` 的解析模式（YAML frontmatter + regex + git log）。

**理由**: 与项目现有元工具栈一致（Python），无新增语言依赖；CI 集成已成熟。

### D-2. 6 项条件状态自动检测 + 人工覆盖

**决策**: 脚本自动检测 6 项条件状态（git log + openspec validate + docs grep），但允许 `--override` flag 人工覆盖（如条件 2 容量评估需人工判断，自动检测无法量化"团队人数"）。

**理由**: 条件 1/3/4/5/6 可自动检测（git log / openspec / docs），条件 2（团队容量）需人工 override。

### D-3. 决议文档 4 章节固定结构

**决策**: `docs/audits/<date>-control-plane-eval-v1.md` 强制 4 章节：§6 项条件状态（含 file:line 引用）+ §决策表（PASS / FAIL / CONDITIONAL / ABORT）+ §后续路径（启动 Phase 7a / descope / 继续前置 ship）+ §决议（single-choice）。

**理由**: 与 `from-roadmap-phase-6c-evidence-gate` 提案的 5 章节决议文档结构对齐，便于横向对比多个 Gate 决议。

### D-4. 决策树：6 项全 PASS → 推荐启动 Phase 7

**决策**: 决策树公式化实现为 `evaluate_control_plane(conditions: dict) -> ControlPlaneStatus`：
- 6 项全 PASS → `RecommendStart`
- 任一 FAIL（条件 1/2/5/6） → `DescopeOrContinue`
- 条件 3 单独 ✅ + 其他条件 🟡 → `Conditional`
- 数据缺失 → `Abort`

**理由**: 公式化决策树便于回归测试（3 类路径覆盖）+ Phase 7+ 复用。

### D-5. human review only（per proposal Impact §MUST NOT）

**决策**: 脚本输出**仅**为建议，决议需 human review 在决议文档签字。脚本不修改任何 tracked 文件（除决议文档主动 ship）。

**理由**: proposal Impact §MUST NOT 强制；避免脚本自动重写 active-status.md 或触发 ADR 状态变更。

## Risks / Trade-offs

- **[Risk: 条件 2（团队容量）无法自动检测]** → Mitigation: D-2 `--override` flag 人工覆盖；脚本默认从 active-status.md §四读取当前 capacity 字段。
- **[Risk: 决议文档 24h 内 active-status 同步漏更新]** → Mitigation: 与 `from-roadmap-phase-6c-evidence-gate` 提案同源策略——pre-commit hook 验证引用关系。
- **[Risk: 评估脚本与 active-status.md 状态不一致]** → Mitigation: 脚本入口验证 active-status.md 状态字段 + 决议文档引用一致；不一致则 exit code 1。
- **[Risk: 决策树公式化遗漏边界条件]** → Mitigation: 3 类测试覆盖（全 PASS / 部分 FAIL / 全 FAIL），边界用例 ≥6 case。

## Migration Plan

1. 本提案独立 ship（不依赖其他 ADR，仅消费 active-status.md + git log）。
2. CI 阶段验证脚本可执行 + 测试通过。
3. 决议文档 ship 后 24h 内 active-status.md 同步。
4. Phase 7 启动前由 human 手动运行 `scripts/control-plane-eval.py` 输出决策表。

回滚策略：决议文档可追加 v2 版本（不覆盖 v1）；脚本可独立修改无需回滚计划。

## Open Questions

1. 评估脚本是否集成 CI（每次 Sprint 收官自动跑）？当前手动运行，避免 CI 频繁触发；如需自动运行建议 Phase 7+ 决议。
2. 决议文档是否需要 Phase 6+ 历史快照（保留 v1/v2/v3 横向对比）？当前单版本追加，横向对比留 follow-up。
3. 条件 2（团队容量）量化标准（≥2 人 OR ≥80h/双周）是否需要更细致定义？当前方案留架构组评审。
