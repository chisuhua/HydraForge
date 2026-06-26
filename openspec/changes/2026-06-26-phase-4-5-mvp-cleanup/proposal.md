# Proposal: Phase 4.5 — MVP Cleanup

> **STATUS: PLACEHOLDER** ⚠️
> **本 change 详细 design/spec/tasks 待 C6 (Sprint 16) 完成后填充**
> **触发条件**: C6 (`2026-06-26-adr-0004-v2-metadata-approval`) ship + archive
> **关联 ADR**: ADR-0019 / ADR-0020 / ADR-0031 (全部 ✅ Approved) / ADR-0033 (✅ Approved)
> **追溯范围**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C8

## Why

Phase 1 (Sprint 0/1a/1b/2/3/4/5) 实施后, 引入 `SimpleCognitiveOrchestrator` (C1 B-stage 实施的单轮 ReAct) 作为 MVP 内部实现. 标记 `TODO(mvp)` 注释, Phase 4.5 收尾时清理.

Phase 4.5 是 docs/implementation-roadmap.md §项目全景 中定义的清理阶段:
- 替换 SimpleCognitiveOrchestrator 为正式实现
- 移除 MockLLMProvider 和硬编码路由 (但 CI 必须保留 Mock!)
- examples/ 目录职责梳理

不解决此问题: (a) Phase 0 §Phase 通用完成标准 遗留 `TODO(mvp)` 标记; (b) 真实生产场景无法使用 SimpleCognitiveOrchestrator; (c) Phase 5 自举无法启动.

## What Changes (待 C6 完成后详细制定)

### 1. SimpleCognitiveOrchestrator 替换 (Sprint 18 Day 0-1)

1. 评估: SimpleCognitiveOrchestrator 当前使用情况
2. 替换为正式实现:
   - 基于 `CognitiveWorker` (Sprint 2) + `IExecutionPolicy` (C3)
   - 或基于新 `IExecutionPolicy` 完整版 (PlanPolicy)
3. 移除 `TODO(mvp)` 标记

### 2. MockLLMProvider 评估 (Sprint 18 Day 1)

1. **必须保留** MockLLMProvider (CI 依赖, 无 LLM API key 时仍可运行测试)
2. 评估: 是否从公共 API 降级为 CI-only fixture
3. 决策: 保留为 `agenticdsl::llm::create_mock_provider()` (C1 引入), 仅 `examples/` 中可选用

### 3. examples/ 目录梳理 (Sprint 18 Day 1)

1. 当前 examples:
   - `agent_basic/` (主要示例, 加载 .agent.md)
   - `agent_simple/` (DEPRECATED 注释, 需复核)
   - `agent_loop/` (DEPRECATED 注释, 需复核)
   - `phase1_plugin_demo/` (Sprint 5, 3 modes)
2. 评估: 哪些保留, 哪些移除, 哪些合并
3. 更新 examples/CMakeLists.txt

### 4. 文档更新 (Sprint 18 Day 1)

1. `docs/specs/layer0.md`: Engine 不再依赖 SimpleCognitiveOrchestrator
2. `docs/roadmap-status.md` §七 已知遗留: 全部清空
3. `AGENTS.md` § Recent Changes: Phase 4.5 ship 标记

## Capabilities (待详细制定)

### ADDED Requirements (placeholder)

- `phase-4-5-simple-orchestrator-replaced`: `SimpleCognitiveOrchestrator` MUST 被正式实现替换
- `phase-4-5-mock-llm-provider-ci-only`: `MockLLMProvider` MUST 降级为 CI-only fixture
- `phase-4-5-examples-cleanup`: `examples/` 目录 MUST 职责梳理
- `phase-4-5-todo-mvp-removed`: 所有 `TODO(mvp)` 标记 MUST 移除
- `phase-4-5-layer0-spec-updated`: `docs/specs/layer0.md` MUST 更新 (Engine 不依赖 SimpleCognitiveOrchestrator)
- `phase-4-5-roadmap-100-percent`: `docs/roadmap-status.md` Phase 0-4 MUST 100% (Phase 4.5 100%)

## Impact (待 C6 完成后评估)

**预期修改文件**:
- `src/modules/cognitive/simple_orchestrator.{h,cpp}` (替换或删除)
- `src/modules/cognitive/orchestrator.{h,cpp}` (新建正式实现, 基于 CognitiveWorker + IExecutionPolicy)
- `src/common/llm/mock_provider.{h,cpp}` (评估降级)
- `examples/CMakeLists.txt` (目录梳理)
- `examples/agent_simple/` (评估移除)
- `examples/agent_loop/` (评估移除)
- `docs/specs/layer0.md` (更新)
- `docs/roadmap-status.md` §一 (更新 100%)

**API 兼容性**:
- **可能 breaking change**: `SimpleCognitiveOrchestrator` 移除
- 向后兼容: 通过 `IOrchestrator` 接口 + factory 选择

## Non-goals (placeholder)

- **不实施** Phase 5 自举 (远期)
- **不重写** CognitiveWorker / DomainWorkerPool (Sprint 2/3 ship)
- **不修改** PDK 公共 API (ADR-0021 T4b 治理锁定)

## Estimated Effort (placeholder)

**总计**: 1-2 天 (Sprint 18 收尾)

**前置依赖**: C3 (P1-P2) + C4 (P3-P4) + C5 (Session) + C6 (ADR-0004 V2) 全部 ship
**后续依赖**: 无 (Phase 4.5 100% 收尾, Phase 5 待启动)

## 详细制定 TODO (待 C6 完成后执行)

- [ ] 1. 评估: SimpleCognitiveOrchestrator 当前使用情况
- [ ] 2. 决策: MockLLMProvider 降级 vs 保留
- [ ] 3. 评估: examples/ 目录保留/移除/合并方案
- [ ] 4. 写本 change proposal.md (What Changes 详细化)
- [ ] 5. 写 design.md (5 个 Decision: SimpleCognitiveOrchestrator 替换方案 / MockLLMProvider 降级 / examples 梳理 / 文档更新 / 兼容性策略)
- [ ] 6. 写 tasks.md (5-10 sections, 10-20 tasks)
- [ ] 7. 写 specs/phase-4-5-cleanup/spec.md (5-8 ADDED Requirements)
- [ ] 8. 移除所有 "PLACEHOLDER" 标记, 更新 STATUS 行
- [ ] 9. `openspec validate 2026-06-26-phase-4-5-mvp-cleanup` exit 0
- [ ] 10. 更新 master plan C8 状态: ⚪ placeholder → 🟡 active
- [ ] 11. 启动 Sprint 18 实施
