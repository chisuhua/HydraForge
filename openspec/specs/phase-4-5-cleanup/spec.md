# phase-4-5-cleanup Specification

## Purpose
Phase 4.5 MVP 收官(C8,Sprint 18) — `SimpleCognitiveOrchestrator` (C1 B-stage 单轮 ReAct) 经 7+ Sprint 验证稳定 (52/52 ctest + ASan/TSan 100% clean),已演进为 Phase 0 内部实现(由 Sprint 2 CognitiveWorker 封装 + Sprint 20 ReactLoop / PlanExecuteLoop / ForkJoinLoop 在 PDK 内部委托);清理策略:**@internal 标记 + TODO(mvp) 清理**(非重写,避免回归风险),examples/ 目录梳理 8 个条目 + docs/roadmap-status.md Phase 4 + 4.5 → 100%。
## Requirements
### Requirement: phase-4-5-orchestrator-internalized

`SimpleCognitiveOrchestrator` MUST 标记为 `@internal` stable implementation detail。
头文件注释 MUST 反映当前架构（由 CognitiveWorker + ReactLoop 封装）。

#### Scenario: 文件头注释更新

- **WHEN** 读取 `include/agenticdsl/cognitive/simple_orchestrator.h` 行 1-11
- **THEN** 注释包含 `@internal` 和 "Phase 0 单轮 ReAct 编排器 — Phase 1+ 由 CognitiveWorker + ReactLoop 封装"
- **AND** 不包含 `TODO(mvp)` 字符串

#### Scenario: 实现文件 TODO(mvp) 清理

- **WHEN** 读取 `src/modules/cognitive/simple_orchestrator.cpp` 行 109
- **THEN** 注释包含 "多轮循环由 CognitiveWorker 在上层管理"
- **AND** 不包含 `TODO(mvp)` 字符串

---

### Requirement: phase-4-5-todo-mvp-removed

所有 `src/` 和 `include/` 中的 `TODO(mvp)` MUST 被移除。

#### Scenario: grep 验证

- **WHEN** 运行 `grep -r "TODO(mvp)" src/ include/`
- **THEN** 返回空结果 (0 matches)

---

### Requirement: phase-4-5-examples-audited

`examples/` 目录 MUST 经过梳理，包含 `examples/README.md` 说明所有 entry 的用途。

#### Scenario: README.md 存在

- **WHEN** 检查 `examples/README.md`
- **THEN** 文件存在
- **AND** 包含 8 个 entry 的表格 (6 C++ examples + 2 reference docs)

#### Scenario: 无删除

- **WHEN** 对比 `git diff --stat examples/`
- **THEN** 仅新增 `README.md`
- **AND** 无文件删除

---

### Requirement: phase-4-5-roadmap-100-percent

`docs/roadmap-status.md` Phase 4 和 Phase 4.5 MUST 标记为 100% 完成。

#### Scenario: Phase 4 标记

- **WHEN** 读取 `docs/roadmap-status.md` §一 Phase 4 行
- **THEN** 包含 `100% ██████████`
- **AND** 包含 `✅ 已完成`
- **AND** 包含 "2026-07-02" (C7 ship 日期)

#### Scenario: Phase 4.5 标记

- **WHEN** 读取 `docs/roadmap-status.md` §一 Phase 4.5 行
- **THEN** 包含 `100% ██████████`
- **AND** 包含 `✅ 已完成`
- **AND** 包含 "2026-07-03" (C8 ship 日期)

---

### Requirement: phase-4-5-agents-md-updated

`AGENTS.md` § Recent Changes MUST 包含 Phase 4.5 ship 记录。

#### Scenario: Recent Changes 追加

- **WHEN** 读取 `AGENTS.md` § Recent Changes 第一条
- **THEN** 包含 "2026-07-03 (Sprint 18 / C8 ship - phase-4-5-mvp-cleanup)"
- **AND** 包含 "52/52 ctest 零回归"
- **AND** 包含 "SimpleCognitiveOrchestrator @internal 标记 + TODO(mvp) 清理 + examples/ 梳理"

---

