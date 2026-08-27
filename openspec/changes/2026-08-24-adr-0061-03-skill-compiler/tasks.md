# Tasks: adr-0061-03-skill-compiler

> **重要：本 change 处于"筹备"状态**——提案 + 任务清单 + spec 已写定，**不立即实施**。实施启动条件：B1 评审会议通过（ADR-0071/0074 + 3 个新 ADR 决策）。
>
> **TDD 5 步结构**: 每任务按 Write failing test → Verify fail → Implement → Verify pass → Commit

## Phase 0: 筹备（已 ship, 2026-08-24）

- [x] **T0.1** 起草 proposal.md（承认前置依赖, 不立即 ship）
- [x] **T0.2** 起草 tasks.md（本文件）
- [x] **T0.3** 起草 specs/skill-compiler/spec.md（设计草图 + 不变量）
- [x] **T0.4** 在 capability-application-map §四 T17 标注 "依赖 ADR-0071 获批"

## Phase 1: 实施启动条件（B1 评审已通过 2026-08-25）

- [x] **T1.1** ADR-0071 评审通过 → Approved (B1 议程 4) — ✅ 2026-08-25 Promotion
- [x] **T1.2** IEvaluator (ADR-0083) ship → T14 行为回归可用 — ✅ 2026-08-26 ship
- [x] **T1.3** Trajectory IR (T15) ship → 编译输入数据格式就绪 — ⏸ **DEFERRED** (软依赖, 本 change 用 TrajectoryPlaceholder 合成输入, T15 ship 后无缝替换)
- [x] **T1.4** G11 变异治理契约 ship → 审计链路就绪 — ✅ ADR-0084 V1 ship 2026-08-26

## Phase 2: 骨架（待 T1.1-T1.4 完成后, 估时 2 sprint）

- [ ] **T2.1** Write failing test: `tests/test_skill_compiler.cpp` 骨架（≥ 5 cases 占位）
- [ ] **T2.2** Verify fail: 编译失败（无实现）
- [ ] **T2.3** Implement minimal: `include/agenticdsl/contract/iskill_compiler.h` 接口声明
- [ ] **T2.4** Verify pass: 编译成功, 5 个 TEST_CASE 编译通过
- [ ] **T2.5** Commit: `feat(cognitive): SkillCompiler interface skeleton (T17)`

## Phase 3: 核心编译逻辑（待 Phase 2 后, 估时 1 sprint）

- [ ] **T3.1** Write failing test: `compile_basic_skill` (输入 SKILL.md → 输出编译产物)
- [ ] **T3.2** Verify fail: 实现返回原 skill
- [ ] **T3.3** Implement: SKILL.md 解析 + 优化模板生成
- [ ] **T3.4** Verify pass: 编译产物结构正确
- [ ] **T3.5** Commit: `feat(cognitive): SKILL.md compile basic flow`

## Phase 4: 集成（待 Phase 3 后, 估时 1 sprint）

- [ ] **T4.1** 与 T14 行为回归集成：编译后自动验证
- [ ] **T4.2** 与 ADR-0083 IEvaluator 集成：编译质量评分
- [ ] **T4.3** 与 G11 变异治理集成：emit `skill.compilation.*` 3 个事件
- [ ] **T4.4** ADR-0068 附录 A amendment：注册 `skill.compilation.{started,succeeded,failed}` 主题
- [ ] **T4.5** Commit: `feat(cognitive): SkillCompiler full integration`

## Phase 5: 验证（待 Phase 4 后）

- [ ] **T5.1** 全量 ctest 验证：`ctest --output-on-failure -R test_skill_compiler`
- [ ] **T5.2** 回归验证：184+ ctest 全量 0 回归
- [ ] **T5.3** OpenSpec validate：`openspec validate 2026-08-24-adr-0061-03-skill-compiler --strict`
- [ ] **T5.4** ADR-0061-03 状态更新：✅ Approved → ✅ Approved + Shipped
- [ ] **T5.5** capability-application-map 更新：§一 +1 (新能力) 或 §四 T17 → Completed

## 总任务数: 19 项 (4 已 ship + 4 启动条件 + 11 待实施)

## Ship Gate 验证（Phase 5）

- [ ] `tests/test_skill_compiler.cpp` ≥ 5 cases / 10+ assertions PASS
- [ ] `tools/adr_lint.py` exit 0
- [ ] `tools/docs_drift_audit.py` 0 NEW DRIFT items
- [ ] `openspec validate` exit 0
- [ ] `ctest` 全量 0 回归（基线 184/184）

## 工作量估算

- Phase 0: 0.5 day (已 ship)
- Phase 1: 事件驱动（B1 评审会议）
- Phase 2: 1 day
- Phase 3: 1 day
- Phase 4: 1 day
- Phase 5: 0.5 day
- **总计实施: 3.5 days = 0.7 sprint**（前提 Phase 1 启动条件完成）

## 与其他任务的依赖关系

```
T17 SkillCompiler (本任务)
    ├─→ 依赖 T14 行为回归 ✅ ship (2026-08-24)
    ├─→ 依赖 T15 Trajectory IR (B2 + ADR-0061-06 v1.1 amendment 待批)
    ├─→ 依赖 ADR-0083 IEvaluator ship (A2 草案完成, 待 B1 评审)
    ├─→ 依赖 G11 变异治理 ship (待 A2+A3+A4 完成后独立 ADR)
    └─→ 依赖 ADR-0071 Approved (B1 评审会议)
         ↓
    解锁: T19 GEPA MVP + T21 Prompt Evidence Gate
```

## 风险与备选

| 风险 | 缓解 | 备选 |
|---|---|---|
| B1 评审会议延期超过 2 周 | 提前发评审材料 | Phase 2 单独启动（V1 简化版，仅编译不评估）|
| ADR-0071 评审失败 | v1.1 amendment 拆分 | SkillCompiler 缩窄到"编译 SKILL.md → .agent.md"单一转换 |
| IEvaluator 复杂度过高 | V1 TaskSuccessEvaluator (3 行实现) | V2 推迟，V1 仅编译不评估 |
| 变异治理 (G11) 缺位 | 推迟 Phase 4.3 | 仅做"提示词优化"不做"运行时自修改" |