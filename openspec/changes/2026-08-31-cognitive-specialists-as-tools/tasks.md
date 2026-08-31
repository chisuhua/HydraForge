# Tasks — Cognitive Specialists as Tools

> **关键不变量**: 仅包装不修改 specialist 实装, fail-closed, contract 零修改, 与 Materializer axis6 映射一一对应
> **估时**: 0.5 sprint
> **前置依赖**: 全部 ✅ ship (GEPALoop T19 + MCTSWorkflowSearch T20 + SkillCompiler T17 + IToolRegistry + ToolMetadata V2)
> **Oracle 评审**: session `ses_facbd3ffbffeUjlJgZsgMWFiM4` (G4 真实, tool 路线)

## 1. Pre-flight Verification

- [ ] 1.1 验证 3 specialist 入口签名
  - 命令: `grep "reflect_and_commit\|SearchResult search\|class SkillCompiler" include/agenticdsl/cognitive/{gepa_loop,mcts_workflow_search,skill_compiler}.h`
  - 预期: 3 命中
- [ ] 1.2 验证 IToolRegistry register_tool_function 存在
  - 命令: `grep "register_tool_function" include/agenticdsl/contract/itool_registry.h | head -2`
  - 预期: ≥1 命中
- [ ] 1.3 验证 ToolMetadata V2 字段 (execution_policy.h)
  - 命令: `grep -c "category\|approval\|allowed_layers\|cost_estimate\|timeout_ms" src/common/policy/execution_policy.h`
  - 预期: ≥5

## 2. Phase 0 — cognitive_tools 实现

- [ ] 2.1 新建 `src/modules/cognitive/cognitive_tools.h`:
  - `void register_cognitive_tools(IToolRegistry&, shared_ptr<GEPALoop>, shared_ptr<MCTSWorkflowSearch>, shared_ptr<SkillCompiler>, shared_ptr<IInteractionBus> = nullptr)`
  - 3 个 ToolMetadata V2 常量定义 (决策 2)
- [ ] 2.2 新建 `src/modules/cognitive/cognitive_tools.cpp`:
  - 3 个 tool handler lambda 包装 (决策 4)
  - nullptr 兜底 (specialist 未配置 → ToolResult::error Unavailable)
  - 事件发射 (cognitive.specialist.invoked / .completed, bus 非空时)
- [ ] 2.3 新建 `tests/test_cognitive_specialists_as_tools.cpp` (≥6 cases):
  - `register_3_tools_success` — 注册后 list 含 cognitive::gepa_reflect / mcts_search / skill_compile
  - `gepa_reflect_invokes_gepa_loop` — mock IEvaluator/IGovernor, 调用后 reflect_and_commit 被调用
  - `mcts_search_invokes_mcts_workflow_search` — mock evaluator, 调用后 search 被调用
  - `skill_compile_invokes_skill_compiler` — 调用后编译入口被调用
  - `approval_policy_plan_agent_yolo` — plan/agent 需审批, yolo 放行 (ToolMetadata V2 验证)
  - `null_specialist_returns_error` — nullptr specialist → ToolResult::error (fail-closed)
- [ ] 2.4 编译 + 测试通过
  - 命令: `cmake --build build --target test_cognitive_specialists_as_tools && ./build/tests/test_cognitive_specialists_as_tools --reporter compact`
  - 预期: 6 cases / 20+ assertions all pass

## 3. Phase 0 — 事件注册 + 文档同步

- [ ] 3.1 ADR-0068 Appendix A 注册 2 个 `cognitive.specialist.*` 主题 (v1.8+, 与 axis6/workflow.materialized/budget.evolution_cycle 同 amendment 或紧随)
  - `cognitive.specialist.invoked` / `cognitive.specialist.completed`
  - owner=cognitive_tools 模块
- [ ] 3.2 axis6-chain-workflow G4 缺口状态更新 (tool 路线修复中)
- [ ] 3.3 orchestration-architecture §十四 M5 注记 (cognitive specialists 已注册为 tool)

## 4. Ship Gate

- [ ] 4.1 `openspec validate 2026-08-31-cognitive-specialists-as-tools --strict` PASS
- [ ] 4.2 `python3 tools/adr_lint.py` 0 errors
- [ ] 4.3 `python3 tools/docs_drift_audit.py` 0 CRITICAL
- [ ] 4.4 `git diff --stat HEAD -- include/agenticdsl/contract/` = 0 行
- [ ] 4.5 ctest 全量零回归

## 5. Commit

- [ ] 5.1 git add:
  - `src/modules/cognitive/cognitive_tools.h` + `.cpp`
  - `tests/test_cognitive_specialists_as_tools.cpp`
  - `docs/adr/adr-0068-event-emission-contract.md`
  - 架构 doc 更新
- [ ] 5.2 commit message:
  ```
  feat(cognitive): cognitive-specialists-as-tools — GEPA/MCTS/SkillCompiler 注册为 cognitive::* 工具 (G4)

  修复 G4 缺口 (Oracle session ses_facbd3ffbffeUjlJgZsgMWFiM4 确认真实 + 修正: tool 注册路线优于 SKILL.md 化):
  GEPALoop/MCTSWorkflowSearch/SkillCompiler 是纯 C++ class, DSL 无法 dsl_call。

  新增 src/modules/cognitive/cognitive_tools.{h,cpp} — register_cognitive_tools()
  注册 3 个 cognitive::* tool (gepa_reflect / mcts_search / skill_compile),
  每个 ToolMetadata V2 全套 (category=Execute + approval plan/agent 审批 +
  allowed_layers Workflow/Thinking + cost_estimate + timeout_ms)。

  Oracle T2 修正: tool 路线比写 3 个 .agent.md 更简单, 天然走 ADR-0004 审批矩阵。
  与 Materializer (T1) axis6 映射一一对应 (axis6=Reflect → cognitive::gepa_reflect)。
  fail-closed (nullptr specialist → ToolResult::error), contract 零修改。

  6 新测试 (注册/3 specialist 调用/审批策略/null 兜底)。2 个 cognitive.specialist.* 事件。
  估时 0.5 sprint。与 T1 Materializer 并行。

  Ultraworked with [Sisyphus](https://github.com/code-yeongyu/oh-my-openagent)
  Co-authored-by: Sisyphus <clio-agent@sisyphuslabs.ai>
  ```

## 6. 工时估算

| Phase | 估时 |
|-------|------|
| cognitive_tools 实现 (h+cpp+6 tests) | 0.3 sprint |
| 事件注册 + 文档同步 + ship gate | 0.2 sprint |
| **总计** | **0.5 sprint** |

## 7. 后续追踪

- T1 `workflow-materializer-v1` — Materializer 产出的 axis6 节点引用本 change 的 cognitive::* tool
- T6 `chain-evolution-driver-v1` — 依赖 T1+T2+T5
