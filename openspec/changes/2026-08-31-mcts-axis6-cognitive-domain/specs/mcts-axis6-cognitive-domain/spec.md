# MCTS Axis6 cognitive_domain composition chain Specification

> **v2.1 (Oracle 评审 5 blocker + 3 阻塞修复后)**: B1 实装 API 对齐 (`SearchResult search(const TaskSpec&)` + ctor 注入) / B2 chain 语义 (节点级属性, 图级形态) / B3 commit-revert 触发统一 (governor commit 前置, `MutationDecision{approved}` 判定) / B4 ADR-0068 v1.8 主题注册 (本 change owns v1.8) / B5 兜底 Scenario + 绝对值口径 + W4 双发射语义分离 (axis6.* 为搜索层审计, 不替代 mutation.*)

## Purpose

> **本 spec 为 ADR-0061-08 v1.1 amendment 的实施契约**: MCTS 节点模板新增第 6 个属性维度 (Axis6), 每个 WorkflowNode 可标注一个 cognitive_domain specialist, 使 MCTS 生成的 WorkflowGraph 可描述 cognitive_domain composition chain (orchestration-architecture v1.5 §十四 M5 + §18.10.1)。
>
> **零新 contract 类** (不变量 7): 修改限于 `include/agenticdsl/cognitive/mcts_workflow_search.h` v1.1 增量, 不触动 `include/agenticdsl/contract/`。
>
> **v1.0 行为不变性** (不变量 2): 原 ctor (无 chain_config) 委托新 ctor 默认值, `enable_axis6=false` 行为 100% 等同 ADR-0061-08 v1.0。

## ADDED Requirements

### Requirement: Axis6CognitiveDomain enum 6 值完整 (节点级属性)

`Axis6CognitiveDomain` enum MUST 提供 6 个值 (`None` / `Reflect` / `Search` / `Compile` / `Reason` / `Meta_Select`), 作为 WorkflowNode 的**节点级属性** (与 axis1-5 同一抽象层), 覆盖现有 cognitive task specialists + Phase 1/V2 预留。

#### Scenario: enum 6 值完整

- **WHEN** 静态检查 `grep -cE 'None|Reflect|Search|Compile|Reason|Meta_Select' include/agenticdsl/cognitive/mcts_workflow_search.h`
- **THEN** ≥6 个 enum 值全部出现

#### Scenario: WorkflowNode.axis6 默认值 None

- **WHEN** 构造 `WorkflowNode` (默认字段)
- **THEN** `axis6 == Axis6CognitiveDomain::None`

#### Scenario: Reason 和 Meta_Select 是 Phase 1 / V2 占位

- **WHEN** 静态检查 `Reason` 和 `Meta_Select` 的实现注释
- **THEN** 注释明示 "Phase 1 预留" / "V2 阶段", 不在 Phase 0 实装

### Requirement: CognitiveDomainChainConfig struct 5 字段 (ctor 注入)

`CognitiveDomainChainConfig` MUST 含 5 字段: `enable_axis6` / `available_specialists` / `max_chain_depth=3` / `min_eval_improvement=0.05` (绝对值) / `max_nested_search_iterations=30`, 通过 ctor 注入 (与 SearchConfig 同级)。

#### Scenario: 5 字段完整

- **WHEN** 静态检查 `grep -E 'enable_axis6|available_specialists|max_chain_depth|min_eval_improvement|max_nested_search_iterations' include/agenticdsl/cognitive/mcts_workflow_search.h`
- **THEN** 5 字段全部出现

#### Scenario: 默认值

- **WHEN** 构造 `CognitiveDomainChainConfig` (默认字段)
- **THEN** `enable_axis6=false`, `max_chain_depth=3`, `min_eval_improvement=0.05` (绝对值), `max_nested_search_iterations=30`

#### Scenario: min_eval_improvement 是绝对值非百分比

- **WHEN** 静态检查 design.md / amendment 中 `min_eval_improvement` 的描述
- **THEN** 描述为 "绝对值" 或 "绝对改进", **不得**出现 "≥5%" 或 "5% 改进" 相对口径

### Requirement: ctor 重载对齐实装 API 风格

v1.1 ctor 重载 MUST 与实装 v1.0 ctor 风格一致 (配置全部 ctor 注入), 原 ctor MUST 保留并委托新 ctor 默认值。search() 签名 MUST 保持 `SearchResult search(const TaskSpec& spec)` 不变。

#### Scenario: v1.0 实装签名保留

- **WHEN** 静态检查 `grep "SearchResult search(const TaskSpec& spec)" include/agenticdsl/cognitive/mcts_workflow_search.h`
- **THEN** 1 行匹配 (实装真实签名)

#### Scenario: 原 ctor 保留并委托

- **WHEN** 静态检查 `src/modules/cognitive/mcts_workflow_search.cpp` 中原 ctor 实现
- **THEN** 原 ctor (无 chain_config) 内部调用新 ctor 并传 `CognitiveDomainChainConfig{}` 默认值

#### Scenario: 新 ctor 含 chain_config 参数

- **WHEN** 静态检查 `grep "CognitiveDomainChainConfig chain_config" include/agenticdsl/cognitive/mcts_workflow_search.h`
- **THEN** 新 ctor 签名存在 (evaluator, governor, regression_gate, SearchConfig, chain_config, bus)

#### Scenario: search() 签名不变

- **WHEN** 静态检查 `grep "SearchResult search(const TaskSpec& spec)" include/agenticdsl/cognitive/mcts_workflow_search.h | wc -l`
- **THEN** 唯一签名, 无 `max_iterations` 参数, 无 `unique_ptr<WorkflowGraph>` 返回

### Requirement: v1.0 17 cases 测试基线 0 回归

`tests/test_mcts_workflow_search.cpp` 17 cases / 65 assertions MUST 全部 PASS (不变量 1), 不受 Axis6 修改影响。

#### Scenario: v1.0 测试通过

- **WHEN** 运行 `./build/tests/test_mcts_workflow_search --reporter compact`
- **THEN** 17 cases / 65 assertions all pass

#### Scenario: 无 contract 头文件修改

- **WHEN** 运行 `git diff HEAD --stat -- include/agenticdsl/contract/`
- **THEN** 0 行变更 (不变量 7)

### Requirement: chain 语义为图级形态 (非独立算法)

Axis6 MUST 作为 WorkflowNode 节点级属性, chain MUST 由 MCTS 树自然生成 (子节点扩展采样 axis6), **不得**存在独立的 `search_chain` 算法或 "UCB1 特征向量 ordinal 编码" 叙述。

#### Scenario: 无独立 search_chain 函数

- **WHEN** 静态检查 `grep "search_chain\|searchChain" src/modules/cognitive/mcts_workflow_search.cpp`
- **THEN** 0 命中

#### Scenario: 无 UCB1 特征向量叙述

- **WHEN** 静态检查 design.md / amendment 中 "特征向量" / "ordinal 编码" / "one-hot" 叙述
- **THEN** 0 命中 (或仅出现在反例/澄清段落中)

#### Scenario: chain 深度 = 图中连续 axis6≠None 节点数

- **WHEN** 静态检查 `grep "连续.*axis6.*None\|continuous.*axis6" src/modules/cognitive/mcts_workflow_search.cpp docs/adr/skill/adr-0061-08-v1-1-amendment-axis6.md`
- **THEN** ≥1 行匹配 (chain 深度定义明确)

### Requirement: chain 深度硬上限 (max_chain_depth)

`max_chain_depth` MUST 作为**硬上限**截断图中连续 axis6≠None 节点数 (不变量 4), 防 cognitive↔cognitive 无限递归烧预算。

#### Scenario: max_chain_depth=1 截断

- **WHEN** 构造 chain_config `{max_chain_depth=1, available_specialists=[Reflect, Search]}`, 运行 search()
- **THEN** 返回的 WorkflowGraph 中连续 axis6≠None 节点数 ≤ 1

#### Scenario: 默认 max_chain_depth=3

- **WHEN** 构造 `CognitiveDomainChainConfig` (默认字段)
- **THEN** `max_chain_depth == 3`

### Requirement: 嵌套 Search 预算上限 (R3 修复)

axis6=Search 节点的嵌套 MCTS 迭代 MUST 受 `max_nested_search_iterations` (默认 30) 上限, 区别于主搜索 `max_iterations=100`, 防 100³ = 百万迭代爆炸。

#### Scenario: 嵌套预算字段存在

- **WHEN** 静态检查 `grep "max_nested_search_iterations" include/agenticdsl/cognitive/mcts_workflow_search.h`
- **THEN** 1 行匹配, 默认值 30

#### Scenario: 嵌套 Search 受预算限制

- **WHEN** chain 含 axis6=Search 节点, 嵌套搜索启动
- **THEN** 嵌套迭代数 ≤ max_nested_search_iterations (30), 非主搜索 max_iterations (100)

### Requirement: 改进停机为绝对值 (B5 修复)

新 chain 的 best_reward **绝对改进** < `min_eval_improvement` (默认 0.05 绝对值) MUST 触发停机, 口径统一为绝对差, 非相对百分比。

#### Scenario: 绝对值停机

- **WHEN** 当前 best_reward=0.65, 新候选 chain eval=0.70 (绝对改进 0.05 = 阈值), min_eval_improvement=0.10
- **THEN** 停机 (0.05 < 0.10), 不更新 best_workflow

#### Scenario: 文档口径统一

- **WHEN** 静态检查 design.md / amendment / spec 中 min_eval_improvement 描述
- **THEN** 全部描述为 "绝对值" 或 "绝对改进", 无 "≥5%" 相对表述

### Requirement: 空 specialists 兜底 (R6, axis6.degraded)

`available_specialists` 为空或仅含 None 时, MUST: enable_axis6 等效 false (行为等同 v1.0) + emit `axis6.degraded` 事件 (payload `{reason: "empty_specialists"}`)。

#### Scenario: 空 specialists 触发 degraded

- **WHEN** 构造 chain_config `{enable_axis6=true, available_specialists={}}`, 运行 search()
- **THEN** emit `axis6.degraded` 事件 (payload reason="empty_specialists"), 行为等同 enable_axis6=false

#### Scenario: 仅 None specialists 触发 degraded

- **WHEN** 构造 chain_config `{enable_axis6=true, available_specialists={None}}`, 运行 search()
- **THEN** emit `axis6.degraded` 事件, 行为等同 v1.0

### Requirement: commit/revert 触发统一 (governor commit 前置)

`axis6.commit` MUST 仅在 `governor_->commit()` 返回 `MutationDecision{approved=true}` 后 emit; `axis6.revert` MUST 仅在 `governor_->commit()` 返回 `MutationDecision{approved=false}` 后 emit (不再混用 "eval 下降" 触发器; 语义由 governor approve/deny 决定)。

#### Scenario: governor approved → axis6.commit

- **WHEN** `governor_->commit(MutationContext{chain_ref, eval_refs:[], version_id:chain.id})` 返回 `MutationDecision{approved=true}`
- **THEN** emit `axis6.commit` 事件 (payload `{chain, eval_score, depth, iterations_used}`); `governor` 内部已 emit `mutation.committed` (MCTS 层不替代, 仅追加搜索层审计)

#### Scenario: governor denied → axis6.revert

- **WHEN** `governor_->commit(MutationContext{chain_ref, eval_refs:[], version_id:chain.id})` 返回 `MutationDecision{approved=false}` (denial_reason 非空)
- **THEN** emit `axis6.revert` 事件 (payload `{chain, prev_eval_score, denial_reason}`); `governor` 内部已 emit `mutation.denied` (MCTS 层不替代)

#### Scenario: 轴 6 主题不替代 mutation.* 主题 (W4 修复)

- **WHEN** 静态检查 `src/modules/cognitive/mcts_workflow_search.cpp` 事件发射
- **THEN** 仅 emit `axis6.commit`/`axis6.revert`/`axis6.degraded`/`axis6.commit_denied` (V1); 不 emit `mutation.committed`/`mutation.denied` 等 governance 主题 (governor 专属)

#### Scenario: governor commit 调用先于 emit

- **WHEN** 静态检查 `src/modules/cognitive/mcts_workflow_search.cpp` commit_chain 实现
- **THEN** `governor_->commit()` 调用在 `bus_->emit(axis6.commit)` 之前 (governor 内部已 emit `mutation.committed`/`mutation.denied`, MCTS 层只追加 `axis6.commit`/`axis6.revert` 作为搜索层审计)

### Requirement: Axis6 specialists 通过注册项注入, 不硬编码

`cognitive_domain specialists` MUST 来自 §十四 M5 `DomainWorkerPool::register_domain_handler("cognitive", ...)` 注册项 (通过 `available_specialists` enum 值注入), **不可在 MCTSWorkflowSearch 实现内硬编码 specialist 实例 import** (不变量 5)。

#### Scenario: 无硬编码 specialist import

- **WHEN** 运行 `grep -E '#include.*GEPALoop|#include.*SkillCompiler|#include.*iper' src/modules/cognitive/mcts_workflow_search.cpp`
- **THEN** 0 命中

#### Scenario: 通过 available_specialists 注入

- **WHEN** 调用 search() with `available_specialists=[Reflect, Compile]`
- **THEN** MCTS 子节点 axis6 采样仅从 {Reflect, Compile} 中选择, 列表外值不参与

### Requirement: 3 个新事件主题注册 (ADR-0068 Appendix A v1.8)

`axis6.commit` / `axis6.revert` / `axis6.degraded` 3 个主题 MUST 随 Phase 0 ship 同步提交 **ADR-0068 Appendix A v1.8 amendment** (v1.7 已被 capture-mode 2026-08-29 占用)。

#### Scenario: ADR-0068 v1.8 amendment 存在

- **WHEN** 静态检查 `grep "Appendix A v1.8" docs/adr/adr-0068-event-emission-contract.md`
- **THEN** ≥1 行匹配 (ship 后)

#### Scenario: 3 主题注册

- **WHEN** 静态检查 ADR-0068 附录 A 中 `axis6.commit` / `axis6.revert` / `axis6.degraded` 3 行
- **THEN** 3 行全部存在, owner=MCTSWorkflowSearch cognitive 模块

#### Scenario: tasks.md 含 ADR-0068 更新任务

- **WHEN** 静态检查 `grep "adr-0068\|Appendix A v1.8" openspec/changes/2026-08-31-mcts-axis6-cognitive-domain/tasks.md`
- **THEN** ≥1 行匹配

### Requirement: 6 新测试覆盖关键场景

`tests/test_mcts_workflow_search_axis6.cpp` MUST 含 ≥6 cases 覆盖: enum 序列化 / ctor 委托退化 / chain 深度截断 / 嵌套预算 / 绝对改进停机 / 空 specialists degraded。

#### Scenario: 6 cases 完整

- **WHEN** 静态检查 `grep -c "TEST_CASE" tests/test_mcts_workflow_search_axis6.cpp`
- **THEN** ≥6 cases

#### Scenario: 关键场景覆盖

- **WHEN** 静态检查测试名包含: `enum_serialization` / `ctor_delegation_unchanged` / `chain_depth_limit` / `nested_search_budget` / `improvement_threshold_stop` / `empty_specialists_degraded`
- **THEN** 6 个测试名全部出现

### Requirement: 状态联动 (ship 后)

本 change Phase 0 ship 后, MUST:
- `docs/adr/skill/adr-0061-08-v1-1-amendment-axis6.md` 状态行 flip `🔍 Proposed` → `✅ Approved + Phase 0 ship`
- `docs/adr/skill/adr-0061-08-aflow-search.md` v1.0 `## 状态` 行追加 v1.1 注记
- `docs/architecture/agent-orchestration-architecture-2026-08.md` v1.5 → v1.6 + changelog
- **`docs/README.md` adr/skill 表 adr-0061-08 行状态修正** (stale 🔍 Proposed → ✅ V1 Shipped)

#### Scenario: 4 文件状态联动

- **WHEN** 本 change Phase 0 ship 完成
- **THEN** 4 个文件 grep 验证 (amendment flip + v1.0 注记 + doc v1.6 + README 修正)

## MODIFIED Requirements

### Requirement: WorkflowNode struct v1.1 扩展 (不破坏 v1.0 行为)

`WorkflowNode` v1.1 MUST 新增 `axis6` 字段 (默认 `None`), 不影响 v1.0 5 轴默认行为。

#### Scenario: 字段新增

- **WHEN** 静态检查 `grep -B1 -A1 "Axis6CognitiveDomain axis6" include/agenticdsl/cognitive/mcts_workflow_search.h`
- **THEN** `axis6 = Axis6CognitiveDomain::None` 默认值

#### Scenario: v1.0 行为不变

- **WHEN** 通过原 ctor (无 chain_config) 调用 search(spec)
- **THEN** 行为与 ADR-0061-08 v1.0 ship 版本完全一致

### Requirement: Phase 1 占位明确 (不在 Phase 0 实装)

本 change Phase 0 MUST NOT 实装: L2 IAgentHook `cognitive::*` 集成 / BehavioralEquivalenceEvaluator 默认注入 / `examples/mcts_axis6_cognitive_domain/` 示例 / 字符串→enum 映射机制。这些属于 Phase 1 (Sprint 27+), 且 **Phase 1 启动前置 = ADR-0086 信用分配契约立项** (显式 blocker)。

#### Scenario: Phase 0 文件不含 Phase 1 实装

- **WHEN** 静态检查 `grep -E "IAgentHookRegistry|BehavioralEquivalenceEvaluator|examples/mcts_axis6" src/modules/cognitive/mcts_workflow_search.cpp`
- **THEN** 0 命中

#### Scenario: tasks.md Phase 1 指针 + ADR-0086 blocker 声明

- **WHEN** 静态检查 `grep -E "Phase 1|ADR-0086.*立项|立项.*前置" openspec/changes/2026-08-31-mcts-axis6-cognitive-domain/tasks.md`
- **THEN** ≥1 行匹配

## CROSS-REFERENCED Requirements

### Requirement: 与 orchestration-architecture v1.5 §十四 M5 对齐

`CognitiveDomainChainConfig.available_specialists` MUST 与 §十四 M5 `DomainWorkerPool::register_domain_handler("cognitive", ...)` 注册项一一对应 (字符串→enum 映射机制属 Phase 1)。

#### Scenario: 注册项引用

- **WHEN** 静态检查 `grep -A2 "M5 cognitive domain 显式注册" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** §十四 M5 行引用 `domain_worker_pool.h:170` + `register_domain_handler("cognitive", ...)`

#### Scenario: 不变量 5 在 spec 中显式引用

- **WHEN** 静态检查 `grep "不变量 5" spec.md`
- **THEN** ≥1 行匹配

### Requirement: 与 ADR-0085 §决策 5 V1 不实施 MetaAgent 兼容

本 change MUST NOT 引入任何 MetaCognitiveAgent 类或等价集中决策机制。

#### Scenario: 无 MetaAgent 类新增

- **WHEN** 运行 `grep -r "class MetaCognitiveAgent\|class MetaAgent" src/ include/ 2>/dev/null`
- **THEN** 0 命中

#### Scenario: 决策 9 分布式实现引用

- **WHEN** 静态检查 `grep -E "决策 8|决策 9|分布式实现|Meta-Cognitive Agent 仍不需要" docs/adr/skill/adr-0061-08-v1-1-amendment-axis6.md openspec/changes/2026-08-31-mcts-axis6-cognitive-domain/design.md`
- **THEN** ≥1 行匹配

### Requirement: 5 contract 头文件零修改

本 change MUST NOT 修改任何 `include/agenticdsl/contract/` 下的头文件 (不变量 7)。

#### Scenario: contract 目录零修改

- **WHEN** 运行 `git diff HEAD --stat -- include/agenticdsl/contract/`
- **THEN** 0 行变更

#### Scenario: 仅有 cognitive/ 增量

- **WHEN** 运行 `git diff HEAD --stat -- include/agenticdsl/cognitive/`
- **THEN** 仅 mcts_workflow_search.h 修改
