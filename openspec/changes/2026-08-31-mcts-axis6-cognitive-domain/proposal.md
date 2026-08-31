# MCTS Axis6 cognitive_domain composition chain

> **状态**: 🔍 Proposed (2026-08-31, ADR-0061-08 v1.1 amendment 评审中)
> **关联文档**:
> - `docs/architecture/agent-orchestration-architecture-2026-08.md` v1.5 §十四 M5 + §18.10.1
> - `docs/adr/skill/adr-0061-08-v1-1-amendment-axis6.md` (本 change 落地的 ADR amendment 草案, 🔍 Proposed)
> - `docs/adr/skill/adr-0061-08-aflow-search.md` v1.0 (✅ Approved + V1 Shipped 2026-08-28)
> - `docs/adr/skill/adr-0061-09-gepa-loop.md` (GEPA Loop, Axis6 候选 Reflect specialist)
> - `docs/adr/adr-0020-thread-model-isolation.md` (Cognitive/Domain Worker 隔离, per-instance DSLEngine)
> - `docs/adr/adr-0083-evaluator-reward-contract.md` (IEvaluator V2 BehavioralEquivalence ✅ Shipped)
> - `docs/adr/adr-0084-mutation-governance-contract.md` (MutationGovernance L1 ✅ Shipped)
> **最后更新**: 2026-08-31

## Why

### 缺口链

```
orchestration-architecture v1.5 §十四 M5
  "Cognitive Worker 把 cognitive 任务 specialists 作为 'cognitive' domain 注册到 DomainWorkerPool"
  (cognitive domain 显式注册 — 零代码, 零 ADR 改动)
       ↓ 解锁
orchestration-architecture v1.5 §18.10.1
  "cognitive→cognitive_domain→...→other_domain 链搜索触发条件"
  (cognitive_domain 链 + 自进化生成 chain — 需新搜索维度)
       ↓ 解锁
本 change: ADR-0061-08 v1.1 amendment (MCTS Axis6 cognitive_domain composition)
  (MCTS 搜索空间从 5 轴扩张到 6 轴, 第 6 轴枚举 cognitive_domain composition chain)
       ↓ 解锁
§18.10.1 A1-A5 全部就绪:
  - A1 L2 IAgentHook cognitive::* 监控
  - A2 BehavioralEquivalenceEvaluator V2 停机判据
  - A3 chain eval 改进触发 (MCTS Axis6 search → new chain)
  - A4 多 CognitiveWorker 实例隔离
  - A5 Meta-Cognitive Agent 仍不需要 (分布式实现)
```

### 真实用例 (来自用户提议 + Metis 评审)

**用例 1: 静态 cognitive_domain 注册 + 委派**
- Cognitive Agent 通过 §十八 sync-delegate / fan-out / hierarchical-plan 委派给 cognitive_domain agent (GEPALoop / MCTSWorkflowSearch / SkillCompiler / IPER)
- 完全由 M1-M4 + §十八 5 模式覆盖, **零新代码**

**用例 2: Cognitive ↔ cognitive_domain 循环 (GEPALoop 已 ship)**
- GEPALoop.reflect_and_commit = cognitive (CognitiveWorker 失败) → cognitive (反思生成候选) → cognitive (commit 决策) → next iteration
- 已 ship (T19 2026-08-27), **零新代码**

**用例 3: 自进化生成 cognitive_domain 链 (本 change 真正新增)**
- 当前 chain 评估 score 0.65, MCTS 搜索发现 chain "reflect→search→compile" eval score 0.71 (+9%)
- MCTS 通过 Axis6 枚举 chain 候选, IEvaluator V2 + BehavioralEquivalenceEvaluator 评估每个 chain
- 满足 chain eval 改进 ≥5% 触发 MutationGovernor L2+ workflow variants 提交
- **本 change 核心新增**

### 前置依赖 (全部已 ship)

| 依赖 | 状态 | 验证 |
|------|------|------|
| `include/agenticdsl/cognitive/mcts_workflow_search.h` (V1 ship 2026-08-28) | ✅ 17 cases / 65 assertions PASS | 实装基础 |
| `include/agenticdsl/contract/ievaluator.h` V2 (BehavioralEquivalence ✅ Shipped 2026-08-27) | ✅ ship | 评估信号 |
| `include/agenticdsl/cognitive/behavioral_equivalence_evaluator.h` V2 | ✅ ship | "充分理解"停机判据 |
| `include/agenticdsl/contract/imutation_governance.h` (L1 ✅ Shipped 2026-08-26) | ✅ L1 ship, L2+ deferred V2 | commit 授权 |
| `include/agenticdsl/contract/iagent_hook_registry.h` (L2 Agent Hook ✅ Shipped 2026-08-21) | ✅ ship | cognitive::* hook |
| `include/agenticdsl/cognitive/domain_worker_pool.h` (DomainWorkerPool Sprint 3 ✅ Ship) | ✅ ship | M5 cognitive domain 注册 |
| `orchestration-architecture-2026-08.md` v1.5 §十四 M5 + §18.10.1 | ✅ ship | cognitive domain 概念 |
| `ADR-0061-08 v1.1 amendment` 草案 (本文档配套) | 🔍 Proposed | ADR 评审中 |

### 关联 ADR 兼容性

| ADR | 兼容性 |
|-----|--------|
| ADR-0020 V3 二分法 (Cognitive/Domain Worker) | ✅ 扩展 (Domain 侧多 cognitive 注册项, 线程级隔离不变) |
| ADR-0061-08 v1.0 | ✅ 增量 (新增 Axis6 + CognitiveDomainChainConfig, 17 cases 测试基线 0 回归) |
| ADR-0083 IEvaluator V2 | ✅ 复用 (BehavioralEquivalenceEvaluator, Phase 1 集成 understanding 停机判据) |
| ADR-0084 MutationGovernance L1 | ✅ 复用 (L1 authorize 前置 axis6.commit; L2+ deferred V2) |
| ADR-0085 §决策 5 V1 不实施 MetaAgent | ✅ 兼容 (Axis6 通过分布式机制实现, 无需 MetaAgent 类) |
| ADR-0067 L2/L3/L4 分层 | ✅ 兼容 (Axis6 在 L2 cognitive 编排层, 不触及 L1/L3) |
| ADR-0068 Event Emission Contract | 🟡 **需 Appendix A v1.8 amendment 同步 ship** (3 个 axis6.* 主题, v1.7 已被 capture-mode 占用) |

## What Changes

> **v2.1 (Oracle 评审 5 blocker + 3 阻塞修复后)**: B1 实装 API 对齐 (`SearchResult search(const TaskSpec&)` + ctor 注入 + `governor->commit(ctx).approved` 判定) / B2 chain 语义 (节点级属性, 图级形态) / B3 commit-revert 触发统一 (governor commit 前置实装 API) / B4 ADR-0068 v1.8 主题注册 (本 change owns v1.8) / B5 兜底 Scenario + 绝对值口径 + W4 双发射语义分离 (axis6.* 为搜索层审计, 不替代 mutation.*)

### Phase 0 (本 change 立即, ~1-1.5 sprint)

1. **`include/agenticdsl/cognitive/mcts_workflow_search.h`** 新增:
   - `enum class Axis6CognitiveDomain { None, Reflect, Search, Compile, Reason, Meta_Select }` (6 值, **节点级属性**, 与 axis1-5 同一抽象层)
   - `struct CognitiveDomainChainConfig { bool enable_axis6 = false; std::vector<Axis6CognitiveDomain> available_specialists; int max_chain_depth = 3; double min_eval_improvement = 0.05; int max_nested_search_iterations = 30; }` (5 字段, **无 understanding_evaluator — 移至 Phase 1**)
   - `WorkflowNode` 加 `Axis6CognitiveDomain axis6 = Axis6CognitiveDomain::None;` 字段
   - **ctor 重载** `(evaluator, governor, regression_gate, SearchConfig, CognitiveDomainChainConfig, bus)`; 原 ctor 保留并委托新 ctor 默认值
   - `search()` 签名 **保持 `SearchResult search(const TaskSpec& spec)` 不变** (Oracle B1 修复: 对齐实装, 非虚构 unique_ptr 版本)

2. **`src/modules/cognitive/mcts_workflow_search.cpp`** 新增:
   - 原 ctor 委托新 ctor (`CognitiveDomainChainConfig{}` 默认)
   - R6 兜底: `available_specialists` 空/仅 None → enable_axis6=false + emit `axis6.degraded`
   - 子节点扩展采样 axis6 (复用 v1.0 UCB1 per-child, **无特征向量/ordinal 叙述**)
   - chain 深度截断 (连续 axis6≠None 节点数 ≤ `max_chain_depth`)
   - 嵌套 Search 预算 (`max_nested_search_iterations=30`, R3)
   - 改进停机 (best_reward **绝对改进** < `min_eval_improvement` 即停, 绝对值口径)
   - commit_chain: `governor_->commit(ctx).approved` 前置 — approved → `axis6.commit`, denied → `axis6.revert` (Oracle B3 + B1 修复: 实装 API 是 propose/commit, 无 authorize)

3. **`tests/test_mcts_workflow_search_axis6.cpp`** (新建, ≥6 cases):
   - Axis6 enum 序列化 round-trip
   - 原 ctor 委托行为等同 v1.0 (零回归)
   - `max_chain_depth=1` 截断
   - 嵌套 Search 预算 ≤ 30
   - 绝对改进阈值停机
   - 空 specialists → `axis6.degraded` 事件

4. **`docs/adr/adr-0068-event-emission-contract.md`** Appendix A **v1.8 amendment** (Oracle B4 修复): 注册 3 个 axis6.* 主题

5. **4 文件状态联动**: amendment flip / v1.0 注记 / orchestration doc v1.6 / **docs/README.md adr-0061-08 行状态修正 (stale)**

### Phase 1 (后续 Sprint, 待 §十八 §18.10.1 A1-A4 全 ship, ~2 sprint)

5. **L2 IAgentHook handler**: `cognitive::*` 命名空间, emit `cognitive.understanding_check` 事件 (与 §18.10.1 A1 对齐)
6. **BehavioralEquivalenceEvaluator V2 集成**: 作为 `understanding_evaluator` 默认注入
7. **`mcts.commit` 事件 + MutationGovernor L2+ workflow variants 授权**: V2 阶段 (ADR-0061-08 V2 范围)
8. **`examples/mcts_axis6_cognitive_domain/` 示例**: 完整 3 层 chain demo (cognitive→reflect→search→other_domain)

## 不变量

- **不变量 1**: v1.0 17 cases / 65 assertions 测试基线 0 回归 (`tests/test_mcts_workflow_search.cpp` 必须全 pass)
- **不变量 2**: `enable_axis6 = false` (默认) 行为 100% 等同 v1.0 (无 silent behavior change)
- **不变量 3**: `axis6.commit` 必须先经 `governor_->commit(ctx)` 返回 `MutationDecision{approved=true}` L1 接受, 再 emit (L2+ deferred V2)
- **不变量 4**: `max_chain_depth` 默认 3, 图中连续 axis6≠None 节点数硬上限 + `max_nested_search_iterations=30` 嵌套预算 (R3)
- **不变量 5**: cognitive_domain specialists 来自 §十四 M5 `register_domain_handler("cognitive", ...)` 注册项, **不可在 MCTSWorkflowSearch 内硬编码 specialist 实现**
- **不变量 6**: `tools/adr_lint.py` + `docs_drift_audit.py` + `openspec validate --strict` 全 PASS
- **不变量 7**: 5 个 contract 头文件零修改 (`include/agenticdsl/contract/`)

## 风险

| 风险 | 影响 | 缓解 |
|------|------|------|
| **R1 v1.0 行为漂移** | 17 cases 测试失败 | 不变量 1+2: 默认 `enable_axis6=false`, 阶段 0 全部测试基线回归验证 |
| **R2 cognitive_domain specialists 硬编码** | 与 §十四 M5 注册机制脱节 | 不变量 5: 通过 `available_specialists` 参数注入, code review 重点 |
| **R3 嵌套 Search 成本爆炸** | 100³ = 百万迭代 | `max_nested_search_iterations=30` 嵌套预算 (30³ ≈ 2.7万) + `max_chain_depth` 硬上限 |
| **R4 Meta-Cognitive 概念复活** | ADR-0085 §决策 5 被绕过 | 决策 8: 显式声明 Axis6 通过分布式机制实现, 无需 MetaAgent 类 |
| **R5 L1/L2+ 授权混淆** | commit 无授权 | 澄清: Phase 0 `axis6.commit` = L1 搜索审计 (governor `commit(ctx).approved` 后 emit); L2+ workflow variants 生效属 V2 |
| **R6 §十四 M5 未实施** | Axis6 无法获取 specialists | 兜底: `available_specialists` 空/仅 None → emit `axis6.degraded` + 行为等同 v1.0 |
| **R7 ADR-0086 未立项** | Phase 1 触发条件无 owner | **Phase 1 启动前置显式声明: ADR-0086 信用分配契约立项** |
| **R8 事件主题未注册** | axis6.* 成幻影主题 (违反 ADR-0068) | **ADR-0068 Appendix A v1.8 amendment 同步 ship** (v1.7 已被 capture-mode 占用, 3 个 axis6.* 主题注册, tasks §3 必含) |

## Out of Scope (明确不做)

- ❌ 新增 `MetaCognitiveAgent` 类 (ADR-0085 §决策 5 V1 不实施)
- ❌ 新增 contract 头文件 (`include/agenticdsl/contract/` 零 diff)
- ❌ 扩展 `IAgent` 接口 (Oracle Path 2 No-Go 2.3, 等待 AgentWorker 落地)
- ❌ 新建独立 cognitive worker pool (违反 ADR-0020 隔离)
- ❌ 实施 `IAgentComposition::stream()` Phase 2 实装 (另案 ADR-0060 v2)
- ❌ 实现 A3-A5 的自进化触发 / 多 CognitiveWorker 实例 / 横切 L2 hook 集成 (Phase 1 范围, 待 A1-A4 触发条件全 ship)
- ❌ L2+ mutation variants 授权 (ADR-0061-08 V2 范围)
