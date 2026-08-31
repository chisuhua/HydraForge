# Cognitive Specialists as Tools Specification

## Purpose

> 将 GEPALoop / MCTSWorkflowSearch / SkillCompiler 3 个 cognitive specialist 注册为 `cognitive::*` tool (G4 缺口修复, Oracle T2 修正: tool 路线优于 SKILL.md 化)。与 Materializer (T1) 的 axis6 映射一一对应, 天然走 ADR-0004 审批矩阵。

## ADDED Requirements

### Requirement: 3 个 cognitive::* tool 注册

`register_cognitive_tools()` MUST 注册 3 个 tool: `cognitive::gepa_reflect` / `cognitive::mcts_search` / `cognitive::skill_compile`, 命名遵循 ADR-0043 PDK 工具命名约定 (`cognitive` module + verb)。

#### Scenario: 3 tool 注册成功

- **WHEN** 调用 `register_cognitive_tools(registry, gepa, mcts, compiler, bus)`
- **THEN** registry.list 含 `cognitive::gepa_reflect`, `cognitive::mcts_search`, `cognitive::skill_compile`

#### Scenario: 命名约定对齐

- **WHEN** 静态检查 3 个 tool 名
- **THEN** 全部匹配 `cognitive::*` 命名空间 (ADR-0043)

### Requirement: 3 tool 包装对应 specialist 调用

每个 tool handler MUST 调用对应 specialist 的入口方法, MUST NOT 修改 specialist 实装。

#### Scenario: gepa_reflect 调用 GEPALoop

- **WHEN** 调用 `cognitive::gepa_reflect` tool (mock IEvaluator/IGovernor)
- **THEN** `GEPALoop::reflect_and_commit()` 被调用, 返回 ReflectionResult 序列化为 ToolResult

#### Scenario: mcts_search 调用 MCTSWorkflowSearch

- **WHEN** 调用 `cognitive::mcts_search` tool (mock evaluator)
- **THEN** `MCTSWorkflowSearch::search()` 被调用, 返回 SearchResult 序列化为 ToolResult

#### Scenario: skill_compile 调用 SkillCompiler

- **WHEN** 调用 `cognitive::skill_compile` tool
- **THEN** SkillCompiler 编译入口被调用

#### Scenario: specialist 实装零修改

- **WHEN** 运行 `git diff HEAD --stat -- include/agenticdsl/cognitive/gepa_loop.h include/agenticdsl/cognitive/mcts_workflow_search.h include/agenticdsl/cognitive/skill_compiler.h`
- **THEN** 0 行变更 (不变量 1)

### Requirement: ToolMetadata V2 全套

每个 tool MUST 携带完整 ToolMetadata V2: category + approval (三模式) + allowed_layers + cost_estimate + timeout_ms, 与 ADR-0004 §8 Layer×Category 矩阵对齐。

#### Scenario: 审批策略 plan/agent 审批, yolo 放行

- **WHEN** 检查 3 个 tool 的 `approval` 字段
- **THEN** `requires_approval_in_plan=true`, `requires_approval_in_agent=true`, `requires_approval_in_yolo=false`

#### Scenario: category=Execute + allowed_layers

- **WHEN** 检查 3 个 tool 的 `category` 和 `allowed_layers`
- **THEN** category=ToolCategory::Execute, allowed_layers={Workflow, Thinking}

#### Scenario: cost_estimate + timeout_ms 存在

- **WHEN** 检查 3 个 tool 的 `cost_estimate` 和 `timeout_ms`
- **THEN** 全部非默认值 (gepa: 0.05/30s, mcts: 0.10/60s, compile: 0.02/10s)

### Requirement: nullptr specialist fail-closed

specialist 为 nullptr 时, tool MUST 仍注册, 但 handler MUST 返回 `ToolResult::error(ErrorCode::Unavailable)`, 不抛异常。

#### Scenario: nullptr specialist 返回 error

- **WHEN** 调用 `register_cognitive_tools(registry, nullptr, nullptr, nullptr)`, 再调用任一 cognitive::* tool
- **THEN** 返回 ToolResult::error (fail-closed, 不抛异常)

### Requirement: 事件发射 (cognitive.specialist.*)

tool 调用开始/完成时 MUST emit `cognitive.specialist.invoked` / `cognitive.specialist.completed` 事件 (bus 非空时), 注册 ADR-0068 Appendix A。

#### Scenario: 事件发射

- **WHEN** 调用 cognitive::gepa_reflect tool
- **THEN** emit invoked + completed 2 个事件 (payload 含 tool_name, duration_ms)

#### Scenario: ADR-0068 附录 A 注册

- **WHEN** 静态检查 `grep "cognitive.specialist" docs/adr/adr-0068-event-emission-contract.md`
- **THEN** ≥2 行 (ship 后)

### Requirement: 与 Materializer axis6 映射一一对应 (MUST)

`axis6=Reflect` MUST 映射到 `cognitive::gepa_reflect`, `axis6=Search` MUST 映射到 `cognitive::mcts_search`, `axis6=Compile` MUST 映射到 `cognitive::skill_compile`, 名称一一对应 (不变量 5)。

#### Scenario: 映射一致性

- **WHEN** 静态检查 Materializer (T1) design 的 axis6 映射表与本 change 的 tool 注册名
- **THEN** 名称完全一致 (无漂移)

### Requirement: 6 测试覆盖 + 零回归

`tests/test_cognitive_specialists_as_tools.cpp` MUST 含 ≥6 cases, 全量 ctest 零回归。

#### Scenario: 6 cases 完整

- **WHEN** 静态检查 `grep -c "TEST_CASE" tests/test_cognitive_specialists_as_tools.cpp`
- **THEN** ≥6

#### Scenario: 零回归

- **WHEN** 运行 `ctest --output-on-failure`
- **THEN** 0 failures

## MODIFIED Requirements

### Requirement: 零 contract 修改

本 change MUST NOT 修改 `include/agenticdsl/contract/` 任何头文件。

#### Scenario: contract 零修改

- **WHEN** 运行 `git diff --stat HEAD -- include/agenticdsl/contract/`
- **THEN** 0 行变更

## CROSS-REFERENCED Requirements

### Requirement: 与 axis6-chain-workflow G4 对齐

本 change 是 `axis6-chain-workflow-architecture-2026-08.md` §六 G4 缺口的修复 (tool 路线), 文档更新时本 spec 同步。

#### Scenario: G4 缺口状态更新

- **WHEN** 本 change ship 后
- **THEN** axis6-chain-workflow 文档 G4 标注 "已修复 (tool 路线)" 或 changelog 注记
