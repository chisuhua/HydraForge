# Design: g11-closed-adr-0084-approved

## Context

ADR-0084 (Mutation Governance Contract) V1 代码已 ship (commits `0ed5604` / `cfc3532` / `a2b2d52`)，13 个测试用例 / 139 assertions 全部 PASS，ctest 187/187 全量零回归。V1 ship 后状态翻转与文档同步为独立收口工作。

本 change **零代码改动** — 仅文档状态同步 + GitHub Issue #14 关闭。

## Scope Boundaries

### 范围 IN
- 8 处文档状态字段同步（ADR-0084 状态、cap-map G11 行、cap-map §七 changelog、cap-map §八.5 排期、self-evolution §四.2、active-status §一 G11 跟踪段、gap-analysis §一、README §adr/ 表）
- GitHub issue #14 关闭（含 audit trail comment）
- OpenSpec archive（本 change）

### 范围 OUT
- 任何 C++ 源代码改动（V1 已 ship, 禁止回改）
- V2 评估器 / L4 权重支持（V1 边界显式禁止, deferred）
- T17 SkillCompiler 集成（独立 change）
- T19 Phase 2 commit 实际代码（独立 change）
- mutation_topics.cpp 主题注册（V1 已确认不存在, 仅文档层）

## Design Decisions

### D1 — 文档状态翻转顺序

按文档依赖层级从底层到顶层翻转：
1. **ADR-0084 §状态**（最底层契约源）
2. **capability-map §二 G11 + §三 B7 + §八.5**（架构地图）
3. **self-evolution-architecture §四.2**（自进化架构工作文档）
4. **gap-analysis §一**（ADR 状态基线）
5. **active-status §一 G11 跟踪段**（看板）
6. **README §adr/ 表**（索引层）

理由：底层变更先于高层引用，避免 cap-map 引述 ADR 状态时出现不一致窗口。

### D2 — Issue #14 audit trail 格式

GitHub issue #14 关闭 comment 必须包含：
- ADR-0084 ✅ Approved 状态翻转声明
- V1 代码 ship commit SHA (`a2b2d52`)
- G11 Closed 同步声明
- 决议依据 Oracle sessions 引用
- 相关 OpenSpec change 路径

理由：issue body 留完整依据链，便于后续评审追溯。

### D3 — ctest 计数动态化

ship gate 验证禁止硬编码 ctest 数量（如 `187/187`），必须使用 `ctest -N` 实测计数。

理由：未来其他 change ship 后基线会变化，硬编码会导致虚假失败。

### D4 — changelog v1.7 条目格式

cap-map §七 changelog 新增 v1.7 条目，按现有 v1.6 / v1.5 / v1.4 格式编写：
- 版本号 + 日期
- 变更摘要（编号列表）
- 依据：OpenSpec change `g11-closed-adr-0084-approved`

## Risks

| 风险 | 缓解 |
|---|---|
| 文档状态翻转后 grep 验收失败 | T0.4 / T1.1.4 / T1.5.4 等静态 check 任务 |
| Issue #14 关闭失败（无 gh CLI 或权限不足） | T2.3 验证 state，失败则 manual 关闭 |
| 文档改动引入新 drift | T1.9.2 docs_drift_audit 验证 |
| adr_lint 新错误（状态字段格式变化） | T1.9.1 adr_lint 验证 |

## Verification Gates

- ADR lint 全通过
- docs_drift_audit 无新增 CRITICAL
- openspec validate --strict PASS
- ctest 全量 PASS（动态计数）
- Issue #14 state = CLOSED

## Dependencies

- ✅ V1 代码 ship (commit `a2b2d52`)
- ✅ ADR-0084 文件存在
- ✅ cap-map / self-evolution / active-status / gap-analysis / README 当前状态已知

## Out of Scope (V1 deferred)

- T19 Phase 2 commit 实施
- T17 SkillCompiler 集成
- V2 evaluator (BehavioralEquivalence / Composite)
- L4 权重支持
- mutation_topics.cpp 运行时代码注册（V1 不存在，仅文档层）

## Success Criteria

- ADR-0084 状态 ✅ Approved
- G11 ✅ Closed
- Issue #14 CLOSED（含 audit trail）
- 8 处文档状态字段同步完成
- adr_lint + docs_drift_audit + openspec validate 全 PASS
- ctest 全量零回归
- OpenSpec archive 完成