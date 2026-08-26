# ship-ievaluator-reward-contract

## Why

ADR-0083（IEvaluator/RewardSignal 评估契约）🔍 Proposed，2026-08-26 自审修正（Oracle session `ses_fc3090b49ffe7yJwXhx1MoNz5N`）识别：

- ADR-0083 文档结构与 5 项决策点已 Oracle Pre-Review 通过（session `ses_fcba5e477ffeG9wEBHVhU64J0o`），但**契约代码尚未 ship**
- `include/agenticdsl/contract/ievaluator.h` 与 `include/agenticdsl/types/reward_signal.h` 在 `include/` 与 `src/` 中均不存在（`grep -r "class IEvaluator\|struct IEvaluator" include/ src/` 0 命中）
- cap-map §八.2 闭环 1/2 第 3 环"评估信号"实现状态由"✅ 已 ship"修正为"❌ IEvaluator 类代码不存在"

**Oracle 评审关键发现**（cap-map §八 Oracle 评审 + 2026-08-26 自审修正）：

- IEvaluator/RewardSignal 是 **6 个下游 ADR 的硬前置**：ADR-0061-09 GEPA / ADR-0061-08 AFlow / ADR-0061-07 PASTE / ADR-0074 Prompt Evidence Gate / ADR-0078 Fine-tune / ADR-0061-03 SkillCompiler
- 无 IEvaluator 契约则自进化方向无法启动
- ADR-0084 mutation-governance-contract 的决策 3（治理流程）也依赖本 ADR

**审计依据**:

- ADR-0083 头部 `**状态**` 与 §状态字段自相矛盾（头部 "✅ Approved" vs §状态 "🔍 Proposed"），2026-08-26 自审修正后统一为 🔍 Proposed
- 19/22 ship 能力中无 IEvaluator/RewardSignal 类（grep 验证）
- T14 行为回归套件 ship 后，变异/进化的"质量"评估维度无契约支撑（行为回归仅覆盖"等价性"）

**前置依赖（启动条件）**:

| 依赖 | 状态 | 说明 |
|---|---|---|
| ADR-0083 IEvaluator 🔍 Proposed | ✅ | 契约已起草，本次 ship 实施 |
| T14 行为回归套件 (ADR-0061-02) | ✅ 已 ship (2026-08-24) | 评估与回归门协调 |
| ADR-0074 Prompt Evidence Gate | ✅ Approved | RewardSignal.quality 用于 prompt 门控 |

## What Changes

本 change 将 IEvaluator 契约从"ADR 起草"阶段推进到"代码 ship"阶段：

- **新增契约类**:
  - `include/agenticdsl/contract/ievaluator.h` — `class IEvaluator`（双虚函数：`evaluate(trace)` + `compare(a, b)`）
  - `include/agenticdsl/types/reward_signal.h` — `struct RewardSignal`（三态 `quality` + `scalar` + 工厂方法）
  - `include/agenticdsl/types/execution_trace.h` — `struct ExecutionTrace`（含 `final_result` + 评估输入）

- **新增 V1 内置评估器**:
  - `src/modules/cognitive/task_success_evaluator.cpp` — `TaskSuccessEvaluator` (基于 `ToolResult.ok` 的 3 行实现，per ADR-0083 §决策 5)
  - `src/modules/cognitive/behavioral_equivalence_evaluator.cpp` — `BehavioralEquivalenceEvaluator` (V2 推迟到 follow-up，本 change 不实现)
  - `src/modules/cognitive/composite_evaluator.cpp` — `CompositeEvaluator` (V2 推迟到 follow-up)

- **新增测试**:
  - `tests/test_evaluator.cpp` — ≥ 4 cases（happy path / compare / V1 简单实现 / thread-safety）
- **事件集成**:
  - 复用 ADR-0068 EventBuilder 在 `tool.coordinator.cycle_detected` + `domain.task.completed` 等事件点写入评估结果

## Impact

- **影响范围**:
  - L1 编排层（cap-map L1）新增 IEvaluator 抽象 — 不破坏既有 CognitiveWorker/DomainWorkerPool
  - 评估器线程安全约束：评估器无状态或仅 readonly 状态（per ADR-0083 §不变量 3）

- **下游解锁**:
  - ADR-0084 mutation-governance-contract 决策 3（治理流程 IEvaluator 评估）前提
  - T19 GEPA MVP Phase 2 commit 启动（依赖本 ADR ship）
  - T20 AFlow MCTS 评估信号可比性前置
  - T21 Prompt Evidence Gate RewardSignal.quality 门控前置

- **Breaking Changes**: 无（新增契约类，不修改既有 API）

## ship gate 验证

- `python3 tools/adr_lint.py` 通过
- `ctest --output-on-failure -R test_evaluator` ≥ 4 cases / ≥ 8 assertions PASS
- `cd build && ctest --output-on-failure` 185/185 ctest PASS 零回归
- `grep -r "class IEvaluator" include/agenticdsl/contract/` 命中
- cap-map §八.2 闭环 1/2 第 3 环"实现状态"列更新为"✅ 已 ship"
- cap-map §二 G10 "🔍 Proposed (代码 ship 待办)" → "✅ Approved + 代码 ship"
- ADR-0083 头部 `## 状态` 章节更新为 `✅ Approved (ship 2026-08-XX)`

## 关联文档

- ADR-0083-evaluator-reward-contract.md
- ADR-0061-02-behavioral-regression.md (T14 已 ship)
- ADR-0074-prompt-evidence-gate.md ✅
- ADR-0068-event-emission-contract.md (EventBuilder 复用)
- `docs/architecture/self-evolution-architecture-2026-08.md` §四.2 评估/奖励/信用分配平面
- `docs/architecture/capability-application-map-2026-08.md` §二 G10 + §八.2 闭环 1/2