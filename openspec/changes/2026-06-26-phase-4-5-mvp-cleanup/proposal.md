# Proposal: Phase 4.5 — MVP Cleanup

> **STATUS: ACTIVE** 🟡
> **关联 ADR**: ADR-0019 / ADR-0020 / ADR-0031 / ADR-0033 / ADR-0004 (全部 ✅ Approved)
> **追溯范围**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C8
> **前置依赖**: C3 + C4 + C5 + C6 全部 ship ✅ (2026-07-31 / 2026-07-02)
> **最后更新**: 2026-07-03

## Why

Phase 0 MVP 交付后，`SimpleCognitiveOrchestrator` (C1 B-stage 单轮 ReAct) 一直作为核心编排器使用。原始 plan 要求 Phase 4.5 "替换为正式实现"，但实际演进中：

- **Sprint 2** 的 `CognitiveWorker` 将 SimpleCognitiveOrchestrator 封装为 per-agent 执行引擎
- **Sprint 20** 的 `ReactLoop` / `PlanExecuteLoop` / `ForkJoinLoop` 在 PDK 中将其作为内部委托
- 该组件经过 7+ Sprint 的测试验证 (52/52 ctest pass, ASan/TSan 100% clean)

**结论**: SimpleCognitiveOrchestrator 已从 MVP 演化为稳定内部组件，替换风险远大于收益。
C8 的清理策略应调整为 **@internal 标记 + 清理遗留注释**，而非重写。

同时需要：
- 清理 2 个遗留 `TODO(mvp)` 注释
- 梳理 `examples/` 目录的 8 个条目（6 个 C++ 示例 + 2 个文档引用）
- 更新 `docs/roadmap-status.md` 将 Phase 4 标记为 100%

## What Changes

### 1. SimpleCognitiveOrchestrator → @internal 标记

**现状**:
- `cognitive_worker.cpp`: CognitiveWorker::submit_task() 内部委托
- `react_loop.h`: ReactLoop.run() 构造 SimpleCognitiveOrchestrator 内部实例
- `agent_macros.h`: DEFINE_AGENT React 模式委托
- `test_simple_orchestrator.cpp`: 5 个独立测试 (52/52 全绿)
- `slice_01_tool_call/main.cpp`: 端到端示例

**变更**:
1. **不替换** SimpleCognitiveOrchestrator — 它是经过充分测试的稳定组件
2. 文件头注释改为 `@internal`，标记为 "Phase 1 stable implementation detail"
3. 清理 2 处 `TODO(mvp)` 注释 (`.h` 行 5, `.cpp` 行 109)，替换为当前状态的准确描述
4. 不迁移头文件位置 (PDK ReactLoop 需要 include)

### 2. MockLLMProvider — 保持现状

**决策**: CI 核心依赖, **不做任何修改**。
- 所有 52 个测试依赖 MockLLMProvider
- 6 个 examples 通过 `--mock` 运行
- 不降级, 不移动, 不改 API

### 3. examples/ 目录梳理

| 目录 | 类型 | 决策 |
|------|------|------|
| `agent_basic/` | C++ 示例 (编译运行) | ✅ 保留 |
| `agent_simple/` | C++ 示例 (Sprint 19 MockLLM migration) | ✅ 保留 |
| `agent_loop/` | C++ 示例 (Sprint 19 MockLLM migration) | ✅ 保留 |
| `slice_01_tool_call/` | C++ 示例 (端到端) | ✅ 保留 |
| `phase1_model_router_plugin/` | C++ 示例 (C7 Model Router, 4 .so) | ✅ 保留 |
| `phase1_plugin_demo/` | C++ 示例 (PluginLoader 3 modes) | ✅ 保留 |
| `skill_porting/` | 文档 (skill taxonomy + `.md` DSL) | ✅ 保留 (参考文档) |
| `superpowers/` | 文档 (12 `.agent.md` workflow) | ✅ 保留 (对标参考) |

**结论**: 所有 8 个 entry 均有明确用途，无需移除。在 `examples/README.md` 中补充说明。

### 4. 文档更新

- `docs/roadmap-status.md`: Phase 4 → 100%, Phase 4.5 → 100%
- `AGENTS.md` § Recent Changes: 记录 Phase 4.5 ship
- `include/agenticdsl/cognitive/simple_orchestrator.h`: `TODO(mvp)` → `@internal`
- `src/modules/cognitive/simple_orchestrator.cpp`: `TODO(mvp)` → 准确注释

## Capabilities

### ADDED Requirements

- `phase-4-5-orchestrator-internalized`: `SimpleCognitiveOrchestrator` MUST 标记为 `@internal` stable component（非替换）
- `phase-4-5-todo-mvp-removed`: 所有 `TODO(mvp)` MUST 移除 (4 个位置)
- `phase-4-5-examples-audited`: `examples/` 目录 MUST 经过梳理（8 个 entry 全部保留，添加 README.md）
- `phase-4-5-roadmap-100-percent`: `docs/roadmap-status.md` Phase 4 + 4.5 MUST 100%

### 占位 stub 处理（vs 原始 placeholder）

由于 `phase-4-5-cleanup` 是全新 capability，按 OpenSpec 规则新建 spec **仅允许 ADDED**，因此将原 placeholder 中的 3 个 stub 处理如下:

- `phase-4-5-simple-orchestrator-replaced` → 替换为 `phase-4-5-orchestrator-internalized`（design.md Decision 0：演进为 @internal stable component）
- `phase-4-5-mock-llm-provider-ci-only` → MockLLMProvider 不做修改, 删去此 requirement
- `phase-4-5-layer0-spec-updated` → layer0.md 未引用 SimpleCognitiveOrchestrator, 不需要更新

## Impact

**修改文件**:
- `include/agenticdsl/cognitive/simple_orchestrator.h` (注释仅)
- `src/modules/cognitive/simple_orchestrator.cpp` (注释仅)
- `docs/roadmap-status.md` (状态更新)
- `AGENTS.md` (Recent Changes 追加)
- `examples/README.md` (新建: 目录说明)

**API 兼容性**: **零 breaking change** — 仅注释 + 文档变更

## Non-goals

- **不替换** SimpleCognitiveOrchestrator（已演化为稳定内部组件）
- **不重写** CognitiveWorker / DomainWorkerPool / ReactLoop
- **不修改** MockLLMProvider API
- **不实施** Phase 5 自举

## Estimated Effort

**总计**: 1 天 (Sprint 18 收尾)

| 任务 | 估时 |
|------|:----:|
| SimpleCognitiveOrchestrator @internal 标记 + TODO(mvp) 清理 | 0.5h |
| examples/ 目录梳理 + README.md | 0.5h |
| docs/roadmap-status.md + AGENTS.md 更新 | 0.5h |
| ctest 验证 + openspec validate | 0.5h |