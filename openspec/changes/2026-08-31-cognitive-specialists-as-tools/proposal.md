# Cognitive Specialists as Tools — GEPA/MCTS/SkillCompiler 注册为 cognitive::* 工具

> **状态**: 🔍 Proposed (2026-08-31, Oracle 评审 T2: tool 注册路线优于 SKILL.md 化)
> **关联文档**:
> - `docs/architecture/axis6-chain-workflow-architecture-2026-08.md` §六 G4 (Cognitive Specialists 未 SKILL.md 化, Oracle 修正: tool 路线)
> - `docs/architecture/agent-orchestration-architecture-2026-08.md` §十四 M5 (cognitive domain 显式注册)
> - `openspec/changes/2026-08-31-workflow-materializer-v1/` (T1 姊妹 change — Materializer 产出的 axis6 节点引用 `cognitive::*` tool, 本 change 提供实装)
> - `openspec/changes/2026-08-31-mcts-axis6-cognitive-domain/` (Axis6 Phase 0 — axis6=Reflect/Search/Compile 节点的执行目标)
> **Oracle 评审**: session `ses_facbd3ffbffeUjlJgZsgMWFiM4` (G4 真实, tool 注册路线优于 SKILL.md 化: 更简单 + 天然走 ADR-0004 审批矩阵)
> **最后更新**: 2026-08-31

## Why

### 缺口链

```
axis6-chain-workflow §六 G4: Cognitive Specialists 未可调用
  ├─ GEPALoop / MCTSWorkflowSearch / SkillCompiler 是纯 C++ class
  ├─ DSL 无法 dsl_call 纯 C++ class (lib/cognitive/, lib/reasoning/ 不存在)
  └─ Materializer (T1) 产出的 axis6=Reflect 节点引用 cognitive::gepa_reflect tool — 但 tool 未注册
       ↓ 阻塞
chain 执行阶段 4: axis6 节点无法调度到实际 specialist
       ↓ 本 change 修复 (Oracle T2 修正: tool 路线非 SKILL.md)
cognitive-specialists-as-tools:
  ├─ gepa_reflect / mcts_search / skill_compile 注册为 tool (cognitive:: 命名空间)
  ├─ ToolMetadata V2 (category + approval + allowed_layers + cost_estimate + timeout_ms)
  └─ 天然走 ADR-0004 安全矩阵 + ADR-0031 审批 + ToolCoordinator
```

### Oracle T2 修正 (关键)

**原方案** (axis6-chain-workflow §六 G4): 写 3 个 SKILL.md (`/lib/cognitive/*.agent.md`)。
**Oracle 修正**: **C++ class 可注册为 tool** (ToolRegistry + ToolMetadata V2 已就绪), axis6=Reflect → `tool_call: cognitive::gepa_reflect` 即可, 比写 3 个 .agent.md **更简单**, 且天然走 ADR-0004 审批矩阵。SKILL.md 化是 V2 的可组合性需求, 非 V1 阻塞。

**选择 tool 路线的理由**:
1. **零新 DSL 文件** — 无需写 .agent.md, 无需 lib/cognitive/ 目录
2. **天然治理** — ToolMetadata V2 category + approval + allowed_layers + cost_estimate + timeout_ms 全套, 自动走 ADR-0004 Layer×Category 矩阵 + ADR-0031 ExecutionPolicy 审批
3. **与 Materializer (T1) 对齐** — T1 的 axis6 节点生成 `type: tool_call, tool: "cognitive::gepa_reflect"`, 本 change 提供对应 tool 实装
4. **可测试** — tool 调用走 ToolResult 标准化信封 (ADR-0023), 测试用 MockLLMProvider + tool 注册即可

## What Changes

### Phase 0 (本 change 立即, ~0.5 sprint)

1. **`src/modules/cognitive/cognitive_tools.h` + `.cpp`** (新建):
   - `register_cognitive_tools(IToolRegistry& registry, ...)` 函数 — 注册 3 个 cognitive::* tool
   - 3 个 tool handler (lambda 包装现有 C++ class):
     - `cognitive::gepa_reflect` — 包装 `GEPALoop::reflect_and_commit(failed_trace)`
     - `cognitive::mcts_search` — 包装 `MCTSWorkflowSearch::search(spec)`
     - `cognitive::skill_compile` — 包装 `SkillCompiler` 编译入口
   - 每个 tool 的 ToolMetadata V2:
     - `category = ToolCategory::Execute` (认知任务执行)
     - `approval = {requires_approval_in_plan: true, requires_approval_in_agent: true, requires_approval_in_yolo: false}` (plan+agent 审批, yolo 放行)
     - `allowed_layers = {LayerProfile::Workflow, LayerProfile::Thinking}` (认知任务允许层)
     - `cost_estimate` (GEPA 反思 ~0.05 USD, MCTS 搜索 ~0.10 USD, 编译 ~0.02 USD)
     - `timeout_ms` (GEPA 30s, MCTS 60s, 编译 10s)

2. **`tests/test_cognitive_specialists_as_tools.cpp`** (新建, ≥6 cases):
   - 3 tool 注册成功 (register_cognitive_tools 调用后 list 含 cognitive::*)
   - `cognitive::gepa_reflect` 调用 → GEPALoop.reflect_and_commit 被调用 (mock IEvaluator/IGovernor)
   - `cognitive::mcts_search` 调用 → MCTSWorkflowSearch.search 被调用 (mock evaluator)
   - `cognitive::skill_compile` 调用 → SkillCompiler 编译被调用
   - ToolMetadata V2 审批策略: plan/agent 需审批, yolo 放行
   - 非法参数 → ToolResult::error (fail-closed)

3. **`examples/pdk_chat_demo/` 集成** (可选, demo): ChatSession 启动时 `register_cognitive_tools(tool_registry, ...)`, 用户可通过 DSL tool_call 调用 cognitive specialists

### 明确不做

- ❌ 写 SKILL.md (`/lib/cognitive/*.agent.md`) — Oracle: tool 路线更简单, SKILL.md 化是 V2 可组合性需求
- ❌ 修改 GEPALoop/MCTSWorkflowSearch/SkillCompiler 实装 — 仅包装, 不修改
- ❌ 新增 contract 头文件 — tool 注册走现有 IToolRegistry 接口
- ❌ IPER / Reason specialist — 未实装 (Phase 1 预留)
- ❌ Meta_Select specialist — V2 阶段

## 不变量

- **不变量 1**: 不修改 GEPALoop / MCTSWorkflowSearch / SkillCompiler 实装 (仅 lambda 包装)
- **不变量 2**: 3 tool 全部走 ToolMetadata V2 (category + approval + allowed_layers + cost_estimate + timeout_ms)
- **不变量 3**: fail-closed — 非法参数 → ToolResult::error, 不抛异常
- **不变量 4**: contract 零修改 (`include/agenticdsl/contract/`)
- **不变量 5**: 与 Materializer (T1) 的 axis6 映射一致 — axis6=Reflect → tool `cognitive::gepa_reflect` (名称一一对应)

## 风险

| 风险 | 影响 | 缓解 |
|------|------|------|
| **R1 tool 与 specialist 签名漂移** | specialist 实装演进后 tool 签名过期 | 不变量 1 (仅包装) + 编译期类型检查 + 测试 case 2-4 锁定调用链 |
| **R2 审批策略过严/过松** | cognitive 任务被误拦或误放行 | ToolMetadata V2 approval 三模式显式 (plan+agent 审批, yolo 放行), 参考 ADR-0004 §8 矩阵 |
| **R3 成本估计不准** | cost_estimate 与实际 LLM 成本偏差 | 保守估计 (偏高) + 后续实测校准 |
| **R4 注册时序问题** | register_cognitive_tools 未被调用导致 tool 缺失 | 调用方显式注册 + 测试 case 1 验证注册成功 |
