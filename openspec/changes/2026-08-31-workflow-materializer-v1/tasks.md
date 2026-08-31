# Tasks — WorkflowMaterializer V1

> **关键不变量**: 输出纯 DSL 文本 (不直接构造 ParsedGraph), 无 LLM 调用, 纯模板渲染
> **估时**: 1 sprint
> **前置依赖**: 全部 ✅ ship (T20 MCTS + continue_with_generated_dsl + MarkdownParser)
> **Oracle 评审**: session `ses_facbd3ffbffeUjlJgZsgMWFiM4` (G1 真实 + DSL 文本输出修正)

## 1. Pre-flight Verification

- [ ] 1.1 验证 MCTS WorkflowGraph 结构可消费
  - 命令: `grep -A5 "struct WorkflowGraph" include/agenticdsl/cognitive/mcts_workflow_search.h`
  - 预期: nodes + edges 字段存在
- [ ] 1.2 验证 continue_with_generated_dsl 可用
  - 命令: `grep -rn "continue_with_generated_dsl" src/core/engine.cpp include/agenticdsl/pdk/agent_loops/plan_execute_loop.h examples/agent_loop/ tests/test_engine.cpp | wc -l`
  - 预期: ≥4 处调用
- [ ] 1.3 验证 MarkdownParser::parse_from_string 存在
  - 命令: `grep -rn "parse_from_string" src/modules/parser/ include/ 2>/dev/null | head -3`
  - 预期: ≥1 命中

## 2. Phase 0 — Materializer 核心 (DSL 文本输出)

- [ ] 2.1 新建 `include/agenticdsl/cognitive/workflow_materializer.h`:
  - `class WorkflowMaterializer` + `MaterializeConfig` + `materialize_to_dsl()` 静态方法
  - 签名: `static std::optional<std::string> materialize_to_dsl(const WorkflowGraph&, const TaskSpec&, const MaterializeConfig&, std::string* failure_reason)`
- [ ] 2.2 新建 `src/modules/cognitive/workflow_materializer.cpp`:
  - axis→DSL 映射实现 (design §决策 2 全部 6 轴)
  - DSL 文本序列化 (Markdown `### AgenticDSL` + yaml 代码块, design §决策 3)
  - 兜底逻辑 (design §决策 4: 空图/未实装 axis6/未覆盖组合 → nullopt)
  - lineage 事件发射 (design §决策 5: `workflow.materialized` 经 IInteractionBus)
- [ ] 2.3 新建 `tests/test_workflow_materializer.cpp` (≥6 cases):
  - `empty_graph_returns_nullopt`
  - `linear_axis1_none_axis6_produces_linear_dsl`
  - `axis6_reflect_produces_cognitive_tool_call_node` (tool_call `cognitive::gepa_reflect`)
  - `axis1_branching_produces_fork_join_pair`
  - `dsl_text_roundtrip_through_markdown_parser` (materialize → parse_from_string → ParsedGraph 成功)
  - `lineage_event_contains_workflow_hash_and_output_path`
- [ ] 2.4 编译 + 测试通过
  - 命令: `cmake --build build --target test_workflow_materializer && ./build/tests/test_workflow_materializer --reporter compact`
  - 预期: 6 cases / 20+ assertions all pass
- [ ] 2.5 v1.0 MCTS 测试零回归
  - 命令: `./build/tests/test_mcts_workflow_search --reporter compact`
  - 预期: 17 cases / 65 assertions all pass

## 3. Phase 0 — E2E demo (Oracle "最关键 1 件事")

- [ ] 3.1 新建 `examples/mcts_materialize_demo/main.cpp`:
  - MCTSWorkflowSearch 搜索 (mock evaluator) → 产出 best_workflow
  - Materializer.materialize_to_dsl → DSL 文本
  - DSLEngine::continue_with_generated_dsl → append
  - engine.execute 执行 (mock mode)
  - 验证端到端可跑
- [ ] 3.2 demo 注册到 `AGENTICDSL_BUILD_EXAMPLES` opt-in flag (默认 OFF)
- [ ] 3.3 demo 编译 + 运行验证
  - 命令: `cmake -DAGENTICDSL_BUILD_EXAMPLES=ON --preset debug && cmake --build build --target mcts_materialize_demo && ./build/examples/mcts_materialize_demo/mcts_materialize_demo --mock`
  - 预期: 退出码 0, 输出含 "materialized" + "executed"

## 4. Phase 0 — 事件注册 + 文档同步

- [ ] 4.1 ADR-0068 Appendix A 新增 `workflow.materialized` 主题 (v1.8+, 与 axis6.* 同 amendment 或紧随其后)
  - owner=WorkflowMaterializer cognitive 模块
  - payload: `workflow_hash, output_path, node_count, edge_count, axis6_specialists, spec_task_id`
- [ ] 4.2 `axis6-chain-workflow-architecture-2026-08.md` G1 缺口状态更新 (修复中 → 已修复)
- [ ] 4.3 orchestration-architecture-2026-08.md changelog 追加 (T1 ship 注记)

## 5. Ship Gate

- [ ] 5.1 `openspec validate 2026-08-31-workflow-materializer-v1 --strict` PASS
- [ ] 5.2 `python3 tools/adr_lint.py` 0 errors
- [ ] 5.3 `python3 tools/docs_drift_audit.py` 0 CRITICAL
- [ ] 5.4 `git diff --stat HEAD -- include/agenticdsl/contract/` = 0 行 (不变量 4)
- [ ] 5.5 ctest 全量零回归

## 6. Commit

- [ ] 6.1 git add 目标文件:
  - `include/agenticdsl/cognitive/workflow_materializer.h`
  - `src/modules/cognitive/workflow_materializer.cpp`
  - `tests/test_workflow_materializer.cpp`
  - `examples/mcts_materialize_demo/main.cpp`
  - `docs/adr/adr-0068-event-emission-contract.md`
  - 2 个架构 doc changelog
- [ ] 6.2 commit message:
  ```
  feat(materializer): WorkflowMaterializer V1 — WorkflowGraph → DSL 文本具体化桥 (G1)

  修复 G1 Materialize 缺口 (Oracle session ses_facbd3ffbffeUjlJgZsgMWFiM4 确认真实 + 方案修正):
  Materializer 输出 DSL 文本 (非直接构造 ParsedGraph), 复用 MarkdownParser 单一事实源
  + DSLEngine::continue_with_generated_dsl() 现有路径 (engine.cpp:390)。

  新增 workflow_materializer.{h,cpp} (纯函数静态方法, 无 LLM, 确定性模板渲染) +
  axis→DSL 映射表 (6 轴全覆盖, 与 axis6-chain-workflow §4.1 单一事实源) +
  workflow.materialized 事件 (lineage 追踪) + 6 测试 + E2E demo
  (examples/mcts_materialize_demo, MCTS→Materialize→append→execute 端到端)。

  不变量: 不直接构造 ParsedGraph / 无 LLM 调用 / fail-safe nullopt / contract 零修改 /
  不放 CognitiveWorker 队列 (ADR-0020)。v1.0 MCTS 17 cases 零回归。

  Ultraworked with [Sisyphus](https://github.com/code-yeongyu/oh-my-openagent)
  Co-authored-by: Sisyphus <clio-agent@sisyphuslabs.ai>
  ```

## 7. 工时估算

| Phase | 估时 |
|-------|------|
| Materializer 核心 (h+cpp+6 tests) | 0.5 sprint |
| E2E demo | 0.25 sprint |
| 事件注册 + 文档同步 + ship gate | 0.25 sprint |
| **总计** | **1 sprint** |

## 8. 后续追踪

- T2 `cognitive-specialists-as-tools` — `cognitive::gepa_reflect` 等 tool 实装 (本 change 的 DSL 节点引用这些 tool)
- T5 `evolution-readiness-gate-v1` — 依赖 ADR-0086 ship
- T6 `chain-evolution-driver-v1` — 依赖 T1+T2+T5
