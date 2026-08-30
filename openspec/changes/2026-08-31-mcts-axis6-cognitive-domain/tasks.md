# Tasks — MCTS Axis6 cognitive_domain composition chain

> **v2 (Oracle 评审 5 blocker 修复后)**: B1 实装 API 对齐 / B2 chain 语义 / B3 commit-revert 统一 / B4 ADR-0068 v1.8 主题注册任务补入 / B5 兜底测试 + 绝对值口径
> **关键不变量（设计 §决策 4）**: v1.0 17 cases / 65 assertions 测试基线 0 回归, 原 ctor 委托新 ctor 默认值
> **估时**: 1-1.5 sprint (Phase 0)
> **预期 commit 数**: 1
> **前置依赖**: 全部 ✅ ship (详见 proposal.md §前置依赖)
> **设计依据**: ADR-0061-08 v1.1 amendment 草案 `docs/adr/skill/adr-0061-08-v1-1-amendment-axis6.md` 🔍 Proposed (v1.1-draft.2)
> **Oracle 评审**: session `ses_fad1df19effezRRly8ZxHJsp2P` (Conditional-Go, 5 blocker 已修复)

## 1. Pre-flight Verification (Setup)

- [ ] 1.1 验证 ADR-0061-08 v1.0 完整保留 (本 change 不修改 v1.0)
  - 命令: `head -5 docs/adr/skill/adr-0061-08-aflow-search.md`
  - 预期: "✅ Approved ... ✅ V1 Shipped 2026-08-28"
- [ ] 1.2 验证 orchestration-architecture v1.5 §14 M5 + §18.10.1 已 ship
  - 命令: `grep -c "M5 cognitive domain 显式注册\|18.10.1" docs/architecture/agent-orchestration-architecture-2026-08.md`
  - 预期: ≥2 行
- [ ] 1.3 验证 v1.0 MCTS 测试基线 (17 cases / 65 assertions)
  - 命令: `cmake --build build --target test_mcts_workflow_search && ./build/tests/test_mcts_workflow_search --reporter compact`
  - 预期: 17 cases / 65 assertions all pass
- [ ] 1.4 验证实装 API 签名 (Oracle B1 修复依据)
  - 命令: `grep "SearchResult search(const TaskSpec& spec)" include/agenticdsl/cognitive/mcts_workflow_search.h`
  - 预期: 1 行匹配 (真实签名, 非虚构 unique_ptr 版本)
- [ ] 1.5 验证 MutationGovernor L1 ship (axis6.commit 需 L1 授权)
  - 命令: `grep "class IMutationGovernor\|class MutationGovernor" include/agenticdsl/contract/imutation_governance.h | head -1`
  - 预期: 1 行匹配
- [ ] 1.6 验证 DomainWorkerPool 已 ship (§14 M5 注册接口)
  - 命令: `grep "register_domain_handler" include/agenticdsl/cognitive/domain_worker_pool.h | head -1`
  - 预期: 1 行匹配
- [ ] 1.7 验证 ADR-0068 附录 A 当前版本 (Oracle B4 修复依据: v1.7 已占用, 本 change 用 v1.8)
  - 命令: `grep "Appendix A v1.7" docs/adr/adr-0068-event-emission-contract.md | head -1`
  - 预期: 1 行匹配 (v1.7 = capture-mode 2026-08-29)
- [ ] 1.8 验证 amendment 草案 v1.1-draft.2 已存在 (5 blocker 修复后版本)
  - 命令: `grep "v1.1-draft.2\|B1 实装 API 对齐" docs/adr/skill/adr-0061-08-v1-1-amendment-axis6.md`
  - 预期: ≥1 行匹配

## 2. Phase 0 — Axis6 enum + ChainConfig + ctor 重载 (对齐实装 API)

- [ ] 2.1 修改 `include/agenticdsl/cognitive/mcts_workflow_search.h`:
  - 新增 `enum class Axis6CognitiveDomain { None, Reflect, Search, Compile, Reason, Meta_Select }` (节点级属性)
  - 新增 `struct CognitiveDomainChainConfig { bool enable_axis6 = false; std::vector<Axis6CognitiveDomain> available_specialists; int max_chain_depth = 3; double min_eval_improvement = 0.05; int max_nested_search_iterations = 30; }` (5 字段, 无 understanding_evaluator — 移至 Phase 1)
  - `WorkflowNode` 加 `Axis6CognitiveDomain axis6 = Axis6CognitiveDomain::None;` 字段
  - 新增 ctor 重载 `(evaluator, governor, regression_gate, SearchConfig, CognitiveDomainChainConfig, bus)`
  - 原 ctor 保留 (签名不变, 内部委托新 ctor)
- [ ] 2.2 修改 `src/modules/cognitive/mcts_workflow_search.cpp`:
  - 原 ctor 实现改为委托新 ctor: `MCTSWorkflowSearch(e, g, rg, config, bus) : MCTSWorkflowSearch(e, g, rg, config, CognitiveDomainChainConfig{}, bus) {}`
  - 新 ctor 实现: 存储 chain_config_; R6 兜底 — `available_specialists` 空/仅 None → enable_axis6=false + emit `axis6.degraded`
  - 子节点扩展: 从 `available_specialists` 采样 axis6 (复用 v1.0 UCB1 per-child, **无特征向量/ordinal 叙述**)
  - chain 深度截断: 连续 axis6≠None 节点数 ≤ max_chain_depth
  - 嵌套 Search 预算: axis6=Search 节点嵌套迭代 ≤ max_nested_search_iterations (30)
  - 改进停机: best_reward 绝对改进 < min_eval_improvement (0.05 绝对值) 即停
  - commit_chain: `if (governor_->authorize(chain) == accepted) emit(axis6.commit) else emit(axis6.revert)` (authorize 前置)
- [ ] 2.3 新建 `tests/test_mcts_workflow_search_axis6.cpp` (≥6 cases):
  - `enum_serialization` — Axis6 6 值 round-trip
  - `ctor_delegation_unchanged` — 原 ctor 调用行为等同 v1.0 (verify v1.0 17 cases 0 回归)
  - `chain_depth_limit` — `max_chain_depth=1` 截断到 1 层
  - `nested_search_budget` — axis6=Search 嵌套 ≤ 30 迭代
  - `improvement_threshold_stop` — 绝对改进 < 0.05 停机
  - `empty_specialists_degraded` — 空 specialists → axis6.degraded 事件 + 行为等同 v1.0
- [ ] 2.4 验证 6+ cases 编译通过 + PASS
  - 命令: `cmake --build build --target test_mcts_workflow_search_axis6 && ./build/tests/test_mcts_workflow_search_axis6 --reporter compact`
  - 预期: 6 cases / 25+ assertions all pass
- [ ] 2.5 验证 v1.0 17 cases 0 回归 (不变量 1)
  - 命令: `./build/tests/test_mcts_workflow_search --reporter compact`
  - 预期: 17 cases / 65 assertions all pass

## 3. Phase 0 — ADR-0068 Appendix A v1.8 主题注册 (Oracle B4 修复)

- [ ] 3.1 编辑 `docs/adr/adr-0068-event-emission-contract.md` Appendix A:
  - 新增 v1.8 amendment 注记: "Appendix A v1.8 amendment (2026-XX-XX, mcts-axis6-cognitive-domain Phase 0 ship): 新增 3 个 `axis6.*` 主题 (MCTSWorkflowSearch cognitive 模块, axis6.commit / axis6.revert / axis6.degraded, 全部为 emit 审计 + chain 生命周期)"
  - 附录 A 表格新增 3 行: `axis6.commit` / `axis6.revert` / `axis6.degraded` (owner=MCTSWorkflowSearch cognitive 模块, payload 对齐决策 6)
- [ ] 3.2 验证 3 主题注册
  - 命令: `grep -c "axis6.commit\|axis6.revert\|axis6.degraded" docs/adr/adr-0068-event-emission-contract.md`
  - 预期: ≥3 行

## 4. Phase 0 — 文档同步 (4 文件状态联动)

- [ ] 4.1 `docs/adr/skill/adr-0061-08-aflow-search.md` v1.0 `## 状态` 行追加 v1.1 注记:
  - 追加: "v1.1 amendment: ✅ Approved (评审通过 2026-XX-XX) + Phase 0 ship (YYYY-MM-DD), 详见 `adr-0061-08-v1-1-amendment-axis6.md`"
- [ ] 4.2 `docs/adr/skill/adr-0061-08-v1-1-amendment-axis6.md` 状态行翻转:
  - "🔍 **Proposed**" → "✅ **Approved (评审通过 2026-XX-XX)** ✅ **+ Phase 0 ship (YYYY-MM-DD, commit `<hash>`)**"
- [ ] 4.3 `docs/architecture/agent-orchestration-architecture-2026-08.md` v1.5 → v1.6:
  - 头部: v1.5 → v1.6
  - changelog 追加 v1.6 行 (ADR-0061-08 v1.1 Phase 0 ship 同步 + §14 M5 + §18.10.1 引用)
- [ ] 4.4 **`docs/README.md` adr/skill 表 adr-0061-08 行状态修正** (Oracle 评审新发现: stale 🔍 Proposed → ✅ V1 Shipped 2026-08-28 + v1.1 amendment)
  - 当前行: "`adr-0061-08-aflow-search.md` | AFlow-style MCTS 工作流搜索 | P2 | 🔍 Proposed |"
  - 改为: "`adr-0061-08-aflow-search.md` | AFlow-style MCTS 工作流搜索 | P2 | ✅ Approved (✅ V1 Shipped 2026-08-28, T20; v1.1 amendment Axis6 评审中) |"
- [ ] 4.5 验证状态联动
  - 命令: `grep -c "v1.1 amendment" docs/adr/skill/adr-0061-08-aflow-search.md docs/adr/skill/adr-0061-08-v1-1-amendment-axis6.md docs/architecture/agent-orchestration-architecture-2026-08.md`
  - 预期: ≥3 行

## 5. Phase 0 — Ship Gate 验证

- [ ] 5.1 `tools/adr_lint.py` 0 errors
  - 命令: `python3 tools/adr_lint.py | tail -3`
  - 预期: 0 errors
- [ ] 5.2 `tools/docs_drift_audit.py` 0 CRITICAL
  - 命令: `python3 tools/docs_drift_audit.py | tail -3`
  - 预期: 0 CRITICAL
- [ ] 5.3 `openspec validate --strict` PASS
  - 命令: `openspec validate 2026-08-31-mcts-axis6-cognitive-domain --strict`
  - 预期: valid
- [ ] 5.4 不变量 7 验证 (contract 零修改)
  - 命令: `git diff --stat HEAD -- include/agenticdsl/contract/`
  - 预期: 0 行
- [ ] 5.5 不变量 5 验证 (无硬编码 specialist import)
  - 命令: `grep -E '#include.*GEPALoop|#include.*SkillCompiler|#include.*iper' src/modules/cognitive/mcts_workflow_search.cpp`
  - 预期: 0 命中
- [ ] 5.6 ctest 全量零回归
  - 命令: `ctest --output-on-failure 2>&1 | tail -3`
  - 预期: 0 failures (含 v1.0 17 cases + 新 6 cases)

## 6. Phase 0 — Commit

- [ ] 6.1 Git status 确认目标文件
  - 预期:
    - `M include/agenticdsl/cognitive/mcts_workflow_search.h`
    - `M src/modules/cognitive/mcts_workflow_search.cpp`
    - `?? tests/test_mcts_workflow_search_axis6.cpp`
    - `M docs/adr/adr-0068-event-emission-contract.md` (Appendix A v1.8)
    - `M docs/adr/skill/adr-0061-08-aflow-search.md` (v1.1 注记)
    - `M docs/adr/skill/adr-0061-08-v1-1-amendment-axis6.md` (状态 flip)
    - `M docs/architecture/agent-orchestration-architecture-2026-08.md` (v1.6)
    - `M docs/README.md` (adr-0061-08 行状态修正)
- [ ] 6.2 Git add 全部
- [ ] 6.3 Git commit (conventional format, 含 Oracle 评审引用):
  ```
  git commit -m "feat(mcts): Axis6 cognitive_domain composition chain (ADR-0061-08 v1.1)" \
    -m "Phase 0 ship: MCTS 节点模板新增第 6 个属性维度 (Axis6CognitiveDomain: None/Reflect/Search/Compile/Reason/Meta_Select), WorkflowNode 可标注 cognitive_domain specialist, MCTS 生成的 WorkflowGraph 可描述 cognitive_domain composition chain (orchestration-architecture v1.5 §14 M5 + §18.10.1)。" \
    -m "对齐实装 API (Oracle B1 修复): ctor 重载 (evaluator, governor, regression_gate, SearchConfig, CognitiveDomainChainConfig, bus), 原 ctor 委托新 ctor 默认值 (enable_axis6=false 行为 100% 等同 v1.0, 17 cases / 65 assertions 0 回归)。search() 签名 SearchResult search(const TaskSpec&) 不变。" \
    -m "chain 语义 (Oracle B2 修复): Axis6 是 WorkflowNode 节点级属性 (非独立 chain 搜索算法), chain 是图级形态 (MCTS 树自然生成, 复用 v1.0 UCB1 per-child, 无特征向量叙述)。" \
    -m "事件语义统一 (Oracle B3 修复): axis6.commit 仅 governor authorize accepted 后 emit, axis6.revert 仅 denied 后 emit (不再混用 eval 下降), axis6.degraded 空 specialists 兜底 (R6)。" \
    -m "ADR-0068 Appendix A v1.8 amendment (Oracle B4 修复): 注册 3 个 axis6.* 主题 (v1.7 已被 capture-mode 占用)。停机判据: max_iterations / max_chain_depth=3 / max_nested_search_iterations=30 (R3) / min_eval_improvement=0.05 绝对值 (B5)。" \
    -m "6 新测试 (test_mcts_workflow_search_axis6.cpp): enum serialization / ctor delegation / depth limit / nested budget / improvement stop / empty degraded。Meta-Cognitive Agent 仍不需要 (决策 8 分布式实现, ADR-0085 §决策 5 兼容)。" \
    -m "Oracle 评审: session ses_fad1df19effezRRly8ZxHJsp2P (Conditional-Go, 5 blocker 全部修复)。Phase 1 启动前置: ADR-0086 信用分配契约立项 (显式 blocker 声明)。" \
    -m "docs/README.md adr/skill 表 adr-0061-08 行状态修正 (stale 🔍 Proposed → ✅ V1 Shipped)。" \
    -m "Ultraworked with [Sisyphus](https://github.com/code-yeongyu/oh-my-openagent)" \
    -m "Co-authored-by: Sisyphus <clio-agent@sisyphuslabs.ai>"
  ```
- [ ] 6.4 验证 commit hash
  - 命令: `git log -1 --format='%H %s'`

## 7. Phase 1 — 后续追踪 (不在本 change 范围)

- [ ] 7.1 **Phase 1 启动前置 blocker: ADR-0086 信用分配契约立项** (当前未立项, 显式声明)
- [ ] 7.2 L2 IAgentHook handler `cognitive::*` 命名空间 (§18.10.1 A1) — 0.5 sprint
- [ ] 7.3 BehavioralEquivalenceEvaluator V2 understanding 停机判据接线 (§18.10.1 A2) — 0.5 sprint
- [ ] 7.4 注册名字符串→Axis6CognitiveDomain enum 映射机制 (不变量 5 调用方侧) — 0.5 sprint
- [ ] 7.5 多 CognitiveWorker 实例 chain 执行 + `examples/mcts_axis6_cognitive_domain/` — 1 sprint
- [ ] 7.6 V2 阶段: L2+ mutation variants 授权 — 1 sprint

## 8. 工时估算

| Phase | 估时 |
|-------|------|
| Phase 0 (enum + ctor + chain 语义 + 6 cases + ADR-0068 v1.8 + 4 文件状态联动) | 1-1.5 sprint |
| Phase 1 (Hook + 集成 + 映射 + example, 前置 ADR-0086 立项) | 2 sprint |
| V2 阶段 (L2+ mutation variants) | 1 sprint |
| **总计** | **~4-5 sprint** |

## 9. 风险监控 (Sprint 收官时复检)

- [ ] 9.1 不变量 1 验证 (v1.0 测试基线 0 回归)
- [ ] 9.2 不变量 2 验证 (原 ctor 委托退化)
- [ ] 9.3 不变量 4 验证 (max_chain_depth 硬上限)
- [ ] 9.4 不变量 5 验证 (无硬编码 specialist import, grep 0 命中)
- [ ] 9.5 不变量 7 验证 (contract 零修改)
- [ ] 9.6 ADR-0068 附录 A v1.8 3 主题注册验证 (无幻影主题)
