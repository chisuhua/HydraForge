# adr-0061-03-skill-compiler

## Why

ADR-0061-03（SkillCompiler 实施）✅ Approved (P0, 父 ADR-0061 拆分, 2026-07-16)但**零代码实施**——`docs/archive/compiler/` 仅有 2026-05-24 的预研设计稿归档，无生产代码。

**Oracle 评审关键发现**（`docs/architecture/capability-application-map-2026-08.md` §八 T17）：

- SkillCompiler 是 **T17 短期任务**——依赖 T14 (行为回归门禁，已 ship) + ADR-0071 获批（评审会议筹备中）
- 与 ADR-0061-06 Trajectory IR (T15) + ADR-0083 IEvaluator (新立) 协同形成自进化 prompt/skill 改进闭环
- 是 **Agent 自进化的"变异对象生成器"**——GEPA/T19 需要 SkillCompiler 输出改进 prompt/skill

**审计依据**:

- `grep -r "SkillCompiler\|skill_compiler" src/ include/ pdk/ tests/` 零命中（`docs/archive/compiler/` 除外）
- `docs/archive/compiler/README.md` 标注 "设计已决但未实施;2026-07-06 归档"
- ADR-0061-03 ✅ Approved (2026-07-16) 但 14 周无实施

**前置依赖（v1 启动条件）**:

| 依赖 | 状态 | 说明 |
|---|---|---|
| T14 行为回归套件 (ADR-0061-02) | ✅ 已 ship (2026-08-24) | 编译后验证编译产物与原版行为等价 |
| Trajectory IR (ADR-0061-06 v1.1) | 🔍 Proposed amendment (T15 待启动) | 编译输入数据格式 |
| IEvaluator (ADR-0083) | 🔍 Proposed (评审会议待开) | 编译质量评估信号 |
| ADR-0071 (LLM-native 顶层) | 🔍 Proposed (评审会议待开) | 顶层方向 |

**本任务定位**: T17 启动 OpenSpec change（承认前置依赖），**不立即 ship 代码**——等 B1 评审会议（ADR-0071/0074 + 3 个新 ADR 决策）通过后启动实施。

## What Changes

**In Scope (OpenSpec 准备)**:

- 本 change 的 proposal.md + tasks.md + specs/ 文件（**当前工作**）
- 设计草图：SkillCompiler 输入 / 输出 / 接口契约
- 与 GEPA/T19 的接口预留（`IFeedbackCollector`）
- 风险评估（编译产物可能引入 prompt injection 攻击面）

**Out of Scope (实施阶段,待评审会议后启动)**:

- `include/agenticdsl/contract/iskill_compiler.h`（L1 契约层）
- `src/modules/cognitive/skill_compiler.cpp`（V1 实现）
- `tests/test_skill_compiler.cpp`（≥ 5 cases）
- 与 IEvaluator (ADR-0083) 的集成（待 A2 ship）
- 与 Trajectory IR (T15) 的集成（待 T15 ship）

### 关键场景（设计草图）

- **GIVEN** 一个 SKILL.md (Anthropic Skills 格式) + 性能指标 (Trajectory IR + IEvaluator 输出)
  **WHEN** 调用 `SkillCompiler::compile()`
  **THEN** 返回优化后的 skill.md（含改进 prompt + 验证 evidence）

- **GIVEN** skill 编译后产物
  **WHEN** 调用 `BehavioralRegression::verify(original, compiled)`
  **THEN** 返回 Verdict (Pass/Fail/Inconclusive)，确保编译产物不退化

- **GIVEN** 编译失败的 skill（IEvaluator 评估为 Poor）
  **WHEN** 调用 `SkillCompiler::rollback()`
  **THEN** 恢复原 skill 状态，emit `skill.compilation.failed` 事件

### 不变量

- SkillCompiler 是**纯函数式**（无副作用，编译输入 → 编译输出，不修改原 skill）
- 编译产物必须通过 T14 行为回归（Verdict ≠ Fail）
- 编译产物必须在 IEvaluator (ADR-0083) 上得分不低于原版
- 编译过程 emit `skill.compilation.{started,succeeded,failed}` 3 个事件（ADR-0068 附录 A 后续注册）

### 风险

| 风险 | 影响 | 缓解 |
|---|---|---|
| Prompt injection 攻击面 | 编译产物可能引入恶意指令 | G11 变异治理契约强制审计 + 只读输出 |
| 编译回路风险 | 反复编译可能导致 skill drift | T14 回归门 + IEvaluator 评分基线 |
| ADR-0071 未批 | SkillCompiler 失去顶层方向 | B1 评审会议通过后启动 |