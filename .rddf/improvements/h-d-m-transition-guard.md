# h-d-m-transition-guard

**优先级**: P0 | **来源**: Phase 6c MetaRSI-v1 关键路径 C3 (rdd-arch ADR-0088 立项)
**阶段**: phase-6c | **分类**: rsi-meta-cognitive
**类型**: feature
**主题**: H→D→M Transition Guard — MetaRSI-v1 关键规则强制状态机

## 架构依据

ADR-0088 (🔍 Proposed 2026-09-20) 立项后,本 change 是 ADR-0088 首次实施。Phase 6c MetaRSI-v1 关键路径: C2 genome-registry ✅ ship 2026-09-19 → **C3 h-d-m-transition-guard (本 change)** → C4 harness-rsi-pilot。

**关键依赖链** (per Oracle M2 + continuation 共识):

```
ADR-0086 v1.1 ✅ ship 2026-09-20 → C3 h-d-m-transition-guard (硬前置已 ship)
ADR-0088 🔍 Proposed (本 change rdd-arch 立项) → C3 fill
                       ↓
                    C4 harness-rsi-pilot (待 C3 ship)
```

**前置全部 ship** (per proposal.md 关联 ADR 段):
- ADR-0083 V2 IEvaluator ✅ Shipped
- ADR-0084 V1 MutationGovernance ✅ Shipped
- **ADR-0086 v1.1 Credit Assignment ✅ Shipped 2026-09-20** (merge commit 886def1) — 条件 1 (Attributed) + judge_data_freshness stub 待 C3 实装
- ADR-0080 v1.1/v1.2 EventLog ✅ Approved
- ADR-0061-02 T14 行为回归 ✅ Shipped — 条件 2 (回归门)
- C2 IGenomeRegistry ✅ Shipped 2026-09-19 — walk_ancestors() 待 C3 扩展
- ExecutionBudget + IBudgetController ✅ Shipped Sprint 11 — 条件 3 (预算)

**OpenSpec change 已 DRAFT**:
- `openspec/changes/2026-09-16-h-d-m-transition-guard/proposal.md` (469 lines)
- 含 D1-D7 决策 + 7 项 Oracle continuation actions
- Oracle session IDs: `ses_f45b96c94ffevTy454aeDBK7U2` (continuation 2026-09-20) + `ses_f55f307f6ffeRJ9SIny8iUbZ8Y` (M2 评审)

## What Changes

### 模块边界 (per ADR-0088 D1-D7)

**新增**:
1. `include/agenticdsl/evolution/transition_guard.h` (~280 行) — EvolutionState 5 态枚举 + EvolutionVerdict struct + can_transition() 编译期 + evaluate_readiness() 运行期
2. `src/modules/evolution/transition_guard.cpp` (~280 行) — 状态机实现 + 三条件门控 (Attributed + 回归门 + 预算)
3. `tests/test_transition_guard.cpp` (TDD 5 步, ≥15 cases) — 状态转换覆盖 + 三条件门控 fail cases

**修改**:
4. `include/agenticdsl/genome/genome.h` (~30 行增) — IGenomeRegistry 新增 `virtual Result<LineageWalk, GenomeError> walk_ancestors(name, from_version, to_version=nullopt) = 0;` + LineageWalk struct 定义
5. `src/core/genome/registry_filesystem.cpp` (~40 行增) — walk_ancestors override 实现
6. `src/evolution/version_pair_diff.cpp` (~50 行增) — judge_data_freshness stub → 完整实装 (4 cases per spec/credit-assignment-v1-1)
7. `tests/test_genome_walk_ancestors.cpp` (新增, ~6 cases) — walk_ancestors TDD 覆盖

**复用** (per Oracle M2 D7 YAGNI):
- IEvaluator (ADR-0083) — evaluate(Genotype)
- MutationGovernance (ADR-0084) — L1-L4 变异等级
- AttributionRecord (ADR-0086) — Attributed/Confounded/Insufficient/NotAttempted
- IBudgetController — max_tokens/max_llm_calls/max_duration_sec
- EventLog + EventBuilder (ADR-0068) — `evolution.transition.denied` + `evolution.readiness.denied` 事件发射 (per ADR-0088 D8, 原 `evolution.scheduler.denied` 是幻影主题已修正)

**不**新建平行接口 (per Oracle M2 YAGNI).

## Acceptance (验收标准)

- [ ] **AC-1**: EvolutionState 5 态枚举 + EvolutionVerdict struct 在 `include/agenticdsl/evolution/transition_guard.h` 定义 (compile-time `static_assert` 验证 `is_enum_v<EvolutionState> == true`).
- [ ] **AC-2**: `can_transition(from, to)` 编译期函数返回 bool,覆盖 5×5 = 25 状态对,其中合法转换 ≥6 (Idle→Harness, Harness→Data, Data→Model, Model→Done, Done→Idle, 任意→Idle for reset). 非法转换 ≥19.
- [ ] **AC-3**: `evaluate_readiness(state, bus, registry, budget, baseline)` 返回 EvolutionVerdict,三条件门控 ① Attributed (per ADR-0086) ② 回归门 PASS (per T14, baseline 类型 = `BaselineSnapshot` 复用 T14) ③ 预算充足 (per IBudgetController). 任意条件 fail → can_proceed=false + failed_conditions **累积报告所有 3 条件结果** (非短路)+ `evolution.readiness.denied` 事件发射 (per ADR-0088 D8 修正).
- [ ] **AC-4**: IGenomeRegistry `walk_ancestors(name, from_version, to_version=nullopt)` virtual method + LineageWalk struct schema `{intermediate_versions: vector<uint64_t>, intermediate_metadata: vector<Genome>}` (含 `Genome.spec.harness`, NOT GenomeMetadata) 在 `include/agenticdsl/genome/genome.h` 定义.
- [ ] **AC-5**: `RegistryFilesystem::walk_ancestors` override 实现 (per `src/core/genome/registry_filesystem.cpp`) 支持 lineage walk + lazy load + cache,大 lineage (≥100 versions) < 100ms.
- [ ] **AC-6**: `judge_data_freshness(data, current, registry)` 完整实装 (替换 ADR-0086 v1.1 stub),实现 4 cases per spec: (1) data==current → Attributed fast-path (2) data not in lineage → Confounded (3) data in lineage but Harness changed after → Confounded with reason="harness changed after data generation" + confounders[0].kind=HarnessChange (4) data in lineage with no subsequent Harness change → Attributed.
- [ ] **AC-7**: test_transition_guard.cpp ≥15 cases / ≥50 assertions PASS (含 state transitions 25 case + 三条件门控 fail cases + reset to Idle + EventLog 事件发射验证).
- [ ] **AC-8**: test_genome_walk_ancestors.cpp ≥6 cases / ≥20 assertions PASS (含 lineage walk happy path + version range clamp + empty lineage + harness string comparison 4 cases).
- [ ] **AC-9**: test_credit_assignment 现有 12 cases / 40 assertions 零回归 (judge_data_freshness 完整实装后,spec R1 confounder-harness-change-kind 4 scenarios 全部通过).
- [ ] **AC-10**: ctest 全量 248+ tests 零回归 (test_credit_assignment + test_transition_guard + test_genome_walk_ancestors 新增),3 项 pre-existing baseline failure (`test_budget_alert` + `test_chat_session_events` + `test_e2e_real_llm`) 与本 change 零关联 (per `git stash` 验证).
- [ ] **AC-11**: ADR-0088 status 翻牌 (🔍 Proposed → ✅ Approved), decision 1-7 + ship evidence 段追加.
- [ ] **AC-12**: capability-application-map-2026-08.md §一 +#33 H→D→M Transition Guard (L4 13→14, 总计 32→33 项), self-evol doc §一.1.3 + §五 + §七 #9 同步 (per ADR-0088 v1.1 翻牌).
- [ ] **AC-13**: openspec archive `2026-09-16-h-d-m-transition-guard` 后 4 文件完整 (`.openspec.yaml` + `proposal.md` + `specs/transition-guard/spec.md` + `tasks.md`), per AGENTS.md Day-5 lesson.

## Capabilities (MUST / MUST NOT)

### MUST

- **MUST** 复用 ADR-0086 v1.1 `agenticdsl::evolution::ConfounderKind::HarnessChange` 类型 (避免重新定义, 遵守 ADR-0088 D5 单一所有权约束)
- **MUST** 复用 `GenomeVersion struct` 来自 `include/agenticdsl/types/attribution_record.h` (per ADR-0088 D5 + spot-check New Issue 1)
- **MUST** 复用 ADR-0083 IEvaluator / ADR-0084 MutationGovernance / ADR-0086 AttributionRecord 既有契约 (per Oracle M2 D7)
- **MUST** 发射 `evolution.transition.denied` + `evolution.readiness.denied` 事件 (per ADR-0088 D8 + ADR-0068 v1.8 amendment 待注册) — `can_transition()` false 时发射 transition 事件, `evaluate_readiness()` 三条件门控任意 fail 时发射 readiness 事件
- **MUST** 实现 IGenomeRegistry `walk_ancestors` 默认实现返回 `Result::failure(GenomeError::NotImplemented)` + FilesystemGenomeRegistry override 完整 lineage walk (per ADR-0088 D9 + Oracle CRITICAL Q6 — 避免 LSP cascade, 项目模式 #9 ITimerService 先例)
- **MUST** judge_data_freshness 完整实装 (per ADR-0088 D6 + spec/credit-assignment-v1-1 §data-freshness-algorithm; nullopt `to_version` 语义 = 追溯到 root, 默认 `from_version` 的 root ancestor, depth cap 10000 per C2 ship M1 fix)
- **MUST** `EvolutionState::Done` 提供显式 `reset_to_idle()` API (per Metis Q7 — Done 状态外部观察但不触发隐式转换)
- **MUST** TDD 5 步 (Write failing test → Verify fail → Implement → Verify pass → Commit) — per AGENTS.md 工程层 TDD 纪律
- **MUST** atomic commits per AGENTS.md 模式 #4 (≥3 atomic commits: feat + docs + archive + merge)
- **MUST** rdd-verifier PASS 8+/8+ ACs (per rdd-builder P3 archive gate)
- **MUST** Oracle dual-agent review per AGENTS.md 模式 #8 (Metis + Oracle background 30 min + 应用 6+ Critical/Major 修正)

### MUST NOT

- **MUST NOT** 新建 IDatasRsi / IHarnessRSI / IModelRSI 三算子接口 (per Oracle M2 D1 YAGNI 原则, 与既有 ADR-0083/0084/0086 契约栈重复)
- **MUST NOT** 重新定义 `GenomeVersion struct` 在 evolution/transition_guard.h (违反 ADR-0088 D5 单一所有权, 触发 ODR)
- **MUST NOT** 修改现有 `AttributionRecord` / `ConfounderRecord` 字段 (向后兼容, 仅扩展 HarnessChange kind)
- **MUST NOT** 跳过 TDD 步骤直接实现 (违反 AGENTS.md 工程纪律)
- **MUST NOT** 在 worktree 主分支直接 commit (违反 AGENTS.md worktree 隔离)
- **MUST NOT** 引入 3 个以上新 bus 主题 (新增 `evolution.transition.denied` + `evolution.readiness.denied` 2 个, 复用 ADR-0068 v1.8 amendment 注册; 避免 spec 蔓延)
- **MUST NOT** 触碰 test_budget_alert / test_chat_session_events / test_e2e_real_llm pre-existing failure (3 项 baseline bug 与本 change 零关联)

## Impact (影响范围)

### 直接影响

| 模块 | 变更类型 | 行数估计 |
|------|---------|---------|
| `include/agenticdsl/evolution/transition_guard.h` | **新增** | +280 |
| `src/modules/evolution/transition_guard.cpp` | **新增** | +280 |
| `tests/test_transition_guard.cpp` | **新增** | +400 (≥15 cases) |
| `include/agenticdsl/genome/genome.h` | 修改 | +30 |
| `src/core/genome/registry_filesystem.cpp` | 修改 | +40 |
| `src/evolution/version_pair_diff.cpp` | 修改 | +50 |
| `tests/test_genome_walk_ancestors.cpp` | **新增** | +200 (≥6 cases) |
| `docs/adr/adr-0088-h-d-m-transition-guard.md` | 修改 | +10 (status flip + ship evidence) |
| `docs/architecture/capability-application-map-2026-08.md` | 修改 | +20 (#33 + L4 14 + 算术链) |
| `docs/architecture/self-evolution-architecture-2026-08.md` | 修改 | +10 (§1.3 + §五 + §七 #9) |
| `docs/README.md` | 修改 | +5 (ADR-0088 row update) |
| `CMakeLists.txt` (root) | 修改 | +1 (add_subdirectory src/modules/evolution) |
| `src/modules/evolution/CMakeLists.txt` | **新增** | +20 (静态库) |
| `tests/CMakeLists.txt` | 修改 | +8 (target_link_libraries for 2 test binaries) |

**总估计**: +1350 行 / -5 行

### 间接影响

- **C4 harness-rsi-pilot**: C3 ship 后 unblocked, 可启动
- **Phase 6c Stage Gate 评估**: C2-C4 全部 ship 后 2 周稳定期
- **G16 正式 Closed**: credit assignment + H→D→M guard + pilot 三件套闭环
- **self-evolution §七 #9 推进**: Agent-Agent/Agent-Environment 协同进化待 S4 promotion criteria 满足后单独立项

### 风险

1. **LSP cascade false positive** (per AGENTS.md 模式 #1) — IGenomeRegistry 新增 virtual method, 所有 mock 实现需 override
2. **judge_data_freshness 性能** — 大 lineage (≥100 versions) 可能 O(n), 需 lazy 评估 + cache
3. **三条件门控 false negative** — 状态机 hard-closed 与 GEPALoop 失败反思修订闭环集成时, 需谨慎避免阻断 prompt 修订
4. **atomic commits 回溯成本** — 多文件改动需 ≥3 atomic commits 保持回溯能力 (per AGENTS.md 模式 #4)

## 估时

| 阶段 | 估时 | 依据 |
|------|------|------|
| rdd-planner (本阶段) | 30 min | 创建 improvement 5-segment + planner-handoff update |
| rdd-builder P0-P3 | 4-6 天 | 实施 + 测试 + review + archive |
| **总估时** | **5-7 天** | 含 Oracle dual-agent review (30 min 后台, 已 ship per bg_a87e7fe3/bg_0bb0f96c + 6 项 Critical 修正已应用) + rdd-verifier PASS |

## 关联 ADR

- **前置 (全部 ✅ ship)**: ADR-0083 / ADR-0084 / ADR-0086 v1.1 / ADR-0080 v1.2 / ADR-0061-02 / C2 IGenomeRegistry
- **本 change 立项**: ADR-0088 🔍 Proposed (本 change ship 后翻牌 ✅ Approved)
- **下游 unblock**: C4 harness-rsi-pilot
