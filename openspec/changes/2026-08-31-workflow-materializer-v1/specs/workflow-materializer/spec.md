# WorkflowMaterializer V1 Specification

## Purpose

> WorkflowMaterializer V1 是 G1 Materialize 缺口的修复: 将 MCTSWorkflowSearch 产出的 WorkflowGraph (图宇宙 A, 结构模板) 转换为 DSL 文本 (Markdown AgenticDSL 格式), 经 `DSLEngine::continue_with_generated_dsl()` 现有路径进入图宇宙 B (ParsedGraph, 可执行)。
>
> **Oracle 修正 (session `ses_facbd3ffbffeUjlJgZsgMWFiM4`)**: 输出 DSL 文本而非直接构造 ParsedGraph, 复用 MarkdownParser 单一事实源。
>
> **零 contract 修改**: 新增限于 `include/agenticdsl/cognitive/workflow_materializer.h` + `src/modules/cognitive/workflow_materializer.cpp`。

## ADDED Requirements

### Requirement: materialize_to_dsl 输出 DSL 文本

`WorkflowMaterializer::materialize_to_dsl()` MUST 返回 `std::optional<std::string>` (DSL 文本), MUST NOT 直接构造 ParsedGraph, MUST 无 LLM 调用 (纯模板渲染)。

#### Scenario: 输出为 DSL 文本非 ParsedGraph

- **WHEN** 静态检查 `grep "materialize_to_dsl" include/agenticdsl/cognitive/workflow_materializer.h`
- **THEN** 返回类型为 `std::optional<std::string>`

#### Scenario: 无 LLM 调用

- **WHEN** 运行 `grep -E "llm_provider|generate\(|ILLMProvider" src/modules/cognitive/workflow_materializer.cpp`
- **THEN** 0 命中

#### Scenario: 无 ParsedGraph 直接构造

- **WHEN** 运行 `grep -E "ParsedGraph|make_unique<ParsedGraph>|new ParsedGraph" src/modules/cognitive/workflow_materializer.cpp`
- **THEN** 0 命中 (除注释/文档引用)

### Requirement: axis→DSL 映射表完整覆盖 6 轴

Materializer MUST 支持 axis1 (Template) + axis2 (Param) + axis3 (Tool) + axis4 (Control) + axis5 (Error) + axis6 (CognitiveDomain) 全部枚举值的 DSL 映射, 映射规则与 `axis6-chain-workflow-architecture-2026-08.md` §4.1 单一事实源一致。

#### Scenario: axis6 认知域节点走 tool_call (V1 路线)

- **WHEN** 输入 WorkflowGraph 含 axis6=Reflect 节点, 调用 materialize_to_dsl
- **THEN** 输出 DSL 含 `type: tool_call` + `tool: cognitive::gepa_reflect` (非 dsl_call, Oracle T2 路线)

#### Scenario: axis1=Branching 生成 fork/join

- **WHEN** 输入 WorkflowGraph 含 axis1=Branching 节点
- **THEN** 输出 DSL 含 `type: fork` 节点 + 对应 `type: join` 节点

#### Scenario: 全部 axis6 枚举值有映射或明确拒绝

- **WHEN** 输入 axis6 ∈ {None, Reflect, Search, Compile} → 有映射; axis6 ∈ {Reason, Meta_Select} → nullopt + "axis6_not_implemented_in_v1"
- **THEN** 6 个值全部被覆盖 (无遗漏)

### Requirement: DSL 文本可被 MarkdownParser 解析回 ParsedGraph (round-trip)

materialize_to_dsl 输出的 DSL 文本 MUST 可被 `MarkdownParser::parse_from_string()` 成功解析为 ParsedGraph, 格式与 dsl.md v3.10 规范对齐。

#### Scenario: round-trip 成功

- **WHEN** 测试 `dsl_text_roundtrip_through_markdown_parser` 运行
- **THEN** materialize → parse_from_string → ParsedGraph 非空, 节点数与输入 WorkflowGraph 一致

#### Scenario: DSL 文本含 signature

- **WHEN** 检查 materialize_to_dsl 输出
- **THEN** 含 `signature:` 块 (inputs/outputs/version/stability), 与 ADR-0073 对齐

### Requirement: 空图/无效 axis 兜底 fail-safe

Materializer MUST 对空图/未实装 axis6/未覆盖 axis 组合返回 `nullopt` + failure_reason, MUST NOT 抛异常。

#### Scenario: 空图返回 nullopt

- **WHEN** 输入 `WorkflowGraph{nodes: {}, edges: {}}`
- **THEN** 返回 nullopt + failure_reason="empty_workflow_graph"

#### Scenario: 不抛异常

- **WHEN** 输入任意无效组合
- **THEN** 不抛异常, 返回 nullopt

### Requirement: workflow.materialized 事件发射

Materializer MUST 在成功 materialize 后 (emit_lineage_event=true 时) 经 IInteractionBus emit `workflow.materialized` 事件, payload 含 workflow_hash + output_path + node_count + axis6_specialists + spec_task_id。

#### Scenario: 事件 payload 完整

- **WHEN** 成功 materialize 后检查事件
- **THEN** payload 含 `workflow_hash`, `output_path`, `node_count`, `edge_count`, `axis6_specialists`, `spec_task_id`

#### Scenario: 事件注册 ADR-0068 附录 A

- **WHEN** 静态检查 `grep "workflow.materialized" docs/adr/adr-0068-event-emission-contract.md`
- **THEN** ≥1 行 (ship 后, Appendix A v1.8+)

### Requirement: 与 continue_with_generated_dsl 现有路径集成

Materializer 产出的 DSL 文本 MUST 可直接传入 `DSLEngine::continue_with_generated_dsl()` (engine.cpp:390) 完成 parse + append, 无需额外转换。

#### Scenario: E2E demo 端到端可跑

- **WHEN** 运行 `examples/mcts_materialize_demo` (mock mode)
- **THEN** 退出码 0, 输出含 "materialized" + "executed", 证明 MCTS→Materialize→append→execute 管线可跑

#### Scenario: demo 不进 CognitiveWorker 队列

- **WHEN** 静态检查 `grep "CognitiveWorker" examples/mcts_materialize_demo/main.cpp`
- **THEN** 0 命中 (Oracle 修正: 同步 driver, 不放单线程队列)

### Requirement: 6 测试覆盖关键场景

`tests/test_workflow_materializer.cpp` MUST 含 ≥6 cases: 空图 / linear / axis6 reflect / branching fork-join / round-trip / lineage 事件。

#### Scenario: 6 cases 完整

- **WHEN** 静态检查 `grep -c "TEST_CASE" tests/test_workflow_materializer.cpp`
- **THEN** ≥6

#### Scenario: v1.0 MCTS 测试零回归

- **WHEN** 运行 `./build/tests/test_mcts_workflow_search --reporter compact`
- **THEN** 17 cases / 65 assertions all pass

## MODIFIED Requirements

### Requirement: 零 contract 修改

本 change MUST NOT 修改 `include/agenticdsl/contract/` 任何头文件。

#### Scenario: contract 零修改

- **WHEN** 运行 `git diff --stat HEAD -- include/agenticdsl/contract/`
- **THEN** 0 行变更

### Requirement: 不修改 MarkdownParser / MCTSWorkflowSearch

本 change MUST NOT 修改 MarkdownParser 或 MCTSWorkflowSearch 实装 (复用而非扩展)。

#### Scenario: parser/MCTS 零修改

- **WHEN** 运行 `git diff --stat HEAD -- src/modules/parser/ src/modules/cognitive/mcts_workflow_search.cpp include/agenticdsl/cognitive/mcts_workflow_search.h`
- **THEN** 0 行变更

## CROSS-REFERENCED Requirements

### Requirement: axis 映射与架构文档单一事实源一致

axis→DSL 映射表 MUST 与 `docs/architecture/axis6-chain-workflow-architecture-2026-08.md` §4.1 保持一致, 文档更新时映射表同步。

#### Scenario: 映射表引用一致性

- **WHEN** 静态检查 `grep -c "axis1\|axis6" docs/architecture/axis6-chain-workflow-architecture-2026-08.md openspec/changes/2026-08-31-workflow-materializer-v1/design.md`
- **THEN** 两文件映射规则一致 (人工评审 + grep 交叉验证)

### Requirement: 与 Axis6 amendment 的边界

Materializer 是 Axis6 搜索结果的具体化器, 不实现 Axis6 搜索逻辑本身 (Axis6 搜索属 `openspec/changes/2026-08-31-mcts-axis6-cognitive-domain/`)。

#### Scenario: 无 MCTS 搜索逻辑

- **WHEN** 运行 `grep -E "ucb1|MCTS|search\(" src/modules/cognitive/workflow_materializer.cpp`
- **THEN** 0 命中 (Materializer 只负责转换, 不负责搜索)
