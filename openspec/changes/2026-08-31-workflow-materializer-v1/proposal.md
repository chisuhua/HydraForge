# WorkflowMaterializer V1 — WorkflowGraph → DSL 文本具体化桥

> **状态**: 🔍 Proposed (2026-08-31, Oracle 评审修正版: Materializer 输出 DSL 文本而非直接构造 ParsedGraph)
> **关联文档**:
> - `docs/architecture/axis6-chain-workflow-architecture-2026-08.md` §一/§四 (G1 Materialize 缺口 + 转换规则)
> - `docs/adr/skill/adr-0061-08-v1-1-amendment-axis6.md` (Axis6 搜索维度, 🔍 Proposed)
> - `docs/adr/skill/adr-0061-08-aflow-search.md` v1.0 (MCTS V1 ✅ Shipped)
> - `openspec/changes/2026-08-31-mcts-axis6-cognitive-domain/` (Axis6 Phase 0 姊妹 change)
> **Oracle 评审**: session `ses_facbd3ffbffeUjlJgZsgMWFiM4` (G1 真实 + 方案修正: DSL 文本输出, 复用 continue_with_generated_dsl 现有路径)
> **最后更新**: 2026-08-31

## Why

### 缺口链

```
MCTSWorkflowSearch (T20 ✅ ship 2026-08-28)
  ↓ 搜索产出 WorkflowGraph (图宇宙 A, 结构模板)
  ├─ synthesize_results() 是 mock (mcts_workflow_search.cpp:124)
  └─ 零消费者 (grep MCTSWorkflowSearch 非测试 = 0 命中)
       ↓ 阻塞
G1 Materialize 缺口: WorkflowGraph → 可执行实体 转换器不存在
  ├─ grep "to_parsed_graph|to_dsl|materialize|WorkflowGraph.*parse" = 0 命中
  └─ Axis6 chain 即使搜出来也无法执行, 端到端价值为零
       ↓ 本 change 修复
WorkflowMaterializer V1 (DSL 文本输出版, Oracle 修正)
  ├─ WorkflowGraph → DSL 文本 (Markdown AgenticDSL 格式)
  ├─ DSLEngine::continue_with_generated_dsl(dsl_text) 复用现有路径 (engine.cpp:390, 已被 4 处调用)
  └─ 输出 ParsedGraph (图宇宙 B, 可执行) 经 MarkdownParser 单一事实源
```

### Oracle 修正 (关键)

**原始方案** (axis6-chain-workflow-architecture §四): Materializer 直接构造 ParsedGraph。
**Oracle 修正** (评审 session `ses_facbd3ffbffeUjlJgZsgMWFiM4`): 直接构造 ParsedGraph 会制造**第二套 ParsedGraph 构建逻辑**, 与 MarkdownParser 分裂; 应输出 **DSL 文本** 复用:
- `MarkdownParser::parse_from_string()` — 单一事实源 (signature/权限/节点解析)
- `DSLEngine::continue_with_generated_dsl()` — 现有 append 路径 (engine.cpp:390, 已被 plan_execute_loop.h:241 / agent_loop 示例 / test_engine.cpp:44 调用)
- signature_validation 语义不分裂

### 前置依赖 (全部已 ship)

| 依赖 | 状态 | 验证 |
|------|------|------|
| `MCTSWorkflowSearch` (T20) | ✅ ship | `include/agenticdsl/cognitive/mcts_workflow_search.h` |
| `DSLEngine::continue_with_generated_dsl()` | ✅ ship + 4 处调用 | `src/core/engine.cpp:390` |
| `MarkdownParser::parse_from_string()` | ✅ ship | engine.cpp:394 内部使用 |
| `DSLEngine::append_graphs()` | ✅ ship | engine.cpp:384 |
| ADR-0073 Tool JSON Schema 契约 | 🟡 Partial (Phase 6a manifest 部分采纳) | signature 校验参考 |

## What Changes

### Phase 0 (本 change 立即, ~1 sprint)

1. **`include/agenticdsl/cognitive/workflow_materializer.h`** (新建):
   - `class WorkflowMaterializer`
   - `static std::optional<std::string> materialize_to_dsl(const WorkflowGraph&, const TaskSpec&, const MaterializeConfig&)` — 输出 DSL 文本 (Markdown AgenticDSL 格式)
   - `struct MaterializeConfig { std::string output_path_prefix = "/dynamic/mcts/"; std::string signature_validation = "strict"; bool emit_lineage_event = true; }`

2. **`src/modules/cognitive/workflow_materializer.cpp`** (新建):
   - axis→DSL 节点映射 (axis6-chain-workflow §4.1 映射表)
   - DSL 文本序列化 (Markdown `### AgenticDSL` + yaml 代码块)
   - 空图 / 无效 axis 组合 → nullopt + failure_reason
   - `workflow.materialized` 事件发射 (经 IInteractionBus, lineage 追踪)

3. **`tests/test_workflow_materializer.cpp`** (新建, ≥6 cases):
   - 空 WorkflowGraph → nullopt
   - Linear axis1 + None axis6 → 线性 DSL 文本 (start→node→end)
   - axis6=Reflect 节点 → dsl_call `/lib/cognitive/gepa_reflect` 节点生成
   - axis1=Branching → fork/join 节点对生成
   - DSL 文本可被 MarkdownParser 解析回 ParsedGraph (round-trip)
   - emit lineage 事件含 workflow_hash + output_path

4. **集成 demo (手写 driver, 非 CognitiveWorker 队列)**: `examples/mcts_materialize_demo/main.cpp`
   - MCTSWorkflowSearch 搜索 → Materializer → continue_with_generated_dsl → execute
   - 证明端到端管线可跑 (Oracle "最关键 1 件事")

### 明确不做

- ❌ 直接构造 ParsedGraph (Oracle 修正: 避免第二套构建逻辑)
- ❌ ChainExecutor / CognitiveWorker 队列集成 (Oracle: MCTS 是长任务, 不放单线程队列; V1 = 同步 driver)
- ❌ 模式 3 双轨仲裁 (Oracle: 早产优化, V1 默认结构化路径)
- ❌ GenerateSubGraph 断链修复 (独立 change, Oracle 降级: V1 chain 可不含 GenerateSubGraph 节点)
- ❌ LLM 调用 (Materializer 纯模板渲染, 无 LLM)

## 不变量

- **不变量 1**: Materializer 输出纯 DSL 文本, 不直接构造 ParsedGraph (复用 MarkdownParser 单一事实源)
- **不变量 2**: 无 LLM 调用 (Materializer 是确定性模板渲染)
- **不变量 3**: 空图/无效 axis → nullopt + failure_reason, 不抛异常 (fail-safe)
- **不变量 4**: 5 contract 头文件零修改 (`include/agenticdsl/contract/`)
- **不变量 5**: axis→DSL 映射表与 axis6-chain-workflow §4.1 一致 (文档单一事实源)
- **不变量 6**: `workflow.materialized` 事件注册 ADR-0068 Appendix A (随 ship 同步 v1.8+)

## 风险

| 风险 | 影响 | 缓解 |
|------|------|------|
| **R1 DSL 文本与 ParsedGraph 语义漂移** | MarkdownParser 不支持某 axis 映射节点 | 不变量 1+5: 映射表与文档对齐 + round-trip 测试验证 parse 成功 |
| **R2 Materializer 成为孤儿组件** | 与 G2 同风险 (无消费者) | 本 change 含 demo driver (MCTS→Materialize→append→execute E2E), 证明有消费者 |
| **R3 axis 映射表遗漏** | 某些 axis 组合无法 materialize | 映射表完整覆盖 axis1-6 全部枚举值 + 未覆盖组合 → nullopt 兜底 |
| **R4 DSL 文本格式与 MarkdownParser 版本漂移** | parser 升级后 materialize 失败 | round-trip 测试锁定格式 + dsl.md v3.10 规范对齐 |
| **R5 demo 依赖 MCTS mock evaluator** | E2E demo 用 mock 评分, 与真实场景偏差 | demo 显式标注 mock-mode + 真实 evaluator 集成属 Phase 1 |
