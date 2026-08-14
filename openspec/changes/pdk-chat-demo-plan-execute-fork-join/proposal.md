# Proposal: PDK Chat Demo — PlanExecuteLoop + ForkJoinLoop DSL 示例与集成

> **STATUS: PROPOSAL** — Phase 6a U1

## Why

`include/agenticdsl/pdk/agent_loops/plan_execute_loop.h` 和 `fork_join_loop.h` 已 ship (Sprint 20), 但 `examples/pdk_chat_demo/` 缺少对应的 DSL 示例和工作流集成。U1 需要:

- 真实的 `.agent.md` DSL 文件展示 PlanExecuteLoop (3-state Planning→Executing→Verifying) 和 ForkJoinLoop (4-state Forking→Executing→Joining)
- 集成测试验证 end-to-end 执行使用 mock provider

**已完成的基础**:
- ✅ `PlanExecuteLoop::run(goal, ctx, token)` 已 ship (3 阶段 + retry)
- ✅ `ForkJoinLoop::run(branches, ctx, token)` 已 ship (并发 + fail-fast)
- ✅ `LoopResult` 统一返回类型
- ✅ `ReactLoop` 示例在 `examples/pdk_chat_demo/` (现有 E2E 测试)
- ✅ MockLLMProvider 基础设施

## What Changes

### 1. 新增 PlanExecuteLoop DSL 示例
- `examples/pdk_chat_demo/dsl/plan_execute_example.agent.md`
- 展示 3 阶段循环的 `.agent.md` DSL 格式
- Mock provider 预设 plan DSL 片段 + verify yes 响应

### 2. 新增 ForkJoinLoop DSL 示例
- `examples/pdk_chat_demo/dsl/fork_join_example.agent.md`
- 展示并行分支调度的 `.agent.md` DSL 格式
- Mock provider 预设 3 并发 branch 输出

### 3. 新增 PlanExecuteLoop 集成测试
- `examples/pdk_chat_demo/tests/test_plan_execute_loop_integration.cpp`
- 验证 end-to-end: MockLLMProvider → PlanExecuteLoop.run() → LoopResult
- 场景: plan success + verify success → Done

### 4. 新增 ForkJoinLoop 集成测试
- `examples/pdk_chat_demo/tests/test_fork_join_loop_integration.cpp`
- 验证 end-to-end: Mock provider → ForkJoinLoop.run() → LoopResult
- 场景: 3 branches 全成功 → Done

### 5. 更新 README
- `examples/pdk_chat_demo/README.md` 添加 DSL 示例章节说明

## Capabilities

### New Capabilities
- `plan-execute-dsl-example`: PlanExecuteLoop `.agent.md` DSL 示例
- `fork-join-dsl-example`: ForkJoinLoop `.agent.md` DSL 示例
- `plan-execute-integration-test`: PlanExecuteLoop 端到端集成测试
- `fork-join-integration-test`: ForkJoinLoop 端到端集成测试

## Impact

| 维度 | 影响 |
|------|------|
| 源代码变更 | 4 新文件 (2 .agent.md + 2 测试) + 1 README 更新 |
| 测试变更 | 2 新测试 (各 1-2 test case), 现有测试零回归 |
| 行为变更 | 仅 demo 示例, 无行为变化 |
| API 变更 | 无 — 仅消费既有 API |

## Non-goals

- **不修改 loop APIs** — `plan_execute_loop.h` / `fork_join_loop.h` / `react_loop.h` 仅消费不修改
- **不实施新 DSL 语法** — 仅使用已定义的 YAML 节点类型
- **不修改 ToolCoordinator / ToolMetadata / ADR-0068 Appendix A**
- **不修改任何 ADR 文档**

## Estimated Effort

- DSL 示例文件: 1h (2 × 30min)
- 集成测试: 2h (各 1h)
- README 更新: 30min
- 构建 + ctest 验证: 30min

**总计**: ~4h

## Test Strategy

- 2 新测试文件, 各 1-2 test case
- MockLLMProvider 预设响应 (无真实 LLM 依赖)
- ctest 零回归: 现有 147 test 保持 PASS
