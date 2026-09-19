# Tasks: H→D→M Transition Guard (C3)

> **STATUS**: DRAFT (post Oracle dual-agent review `bg_fed9d7c0`, 7 Critical fixes applied)
> **Target**: C3 h-d-m-transition-guard
> **总估时**: 4-5 days (修正 Oracle 🟠-2: 原 3-4d 偏低)

---

## 1. Pre-flight (0.5d)

- [ ] 1.1 确认 ADR-0086 v1.1 已 ship（含 HarnessChange kind + 决策 9 完整版 judge_data_freshness + kMinBaselineSamples=5）
  - 验证: `include/agenticdsl/types/attribution_record.h` 存在（不是 `contract/`）
  - 验证: `agenticdsl::evolution::ConfounderKind::HarnessChange` 可引用
  - 验证: `agenticdsl::evolution::kMinBaselineSamples == 5`
- [ ] 1.2 锁定 walk_ancestors 接口签名（与 ADR-0086 v1.1 决策 9 协调）
  - 签名: `virtual Result<LineageWalk, GenomeError> walk_ancestors(const std::string& name, uint64_t from_version, std::optional<uint64_t> to_version = std::nullopt) = 0;`
  - LineageWalk: `intermediate_versions: std::vector<uint64_t>` (closest-first, excludes-self) + `intermediate_metadata: std::vector<Genome>` (NOT GenomeMetadata, 含 spec.harness)
  - 通知 ADR-0086 v1.1 实施者使用该签名 + struct 类型
- [ ] 1.3 锁定 estimated_llm_calls 默认值 = 1（Oracle Open Question 2 决议）
- [ ] 1.4 锁定 5 状态枚举语义（Done 等价 Idle，Done→非 Idle 拒绝）
- [ ] 1.5 设计 TransitionGuard 完整 API（5 状态 + 2 Verdict overloads + Verdict struct + GenomeVersion struct）

## 2. Dual-agent review (0.5d)

- [x] 2.1 派 Metis background `bg_ba048665`（intentionality / spec ambiguity / AI failure modes）— DONE 2026-09-20
- [x] 2.2 派 Oracle background `bg_fed9d7c0`（architecture / implementation feasibility / physical viability）— DONE 2026-09-20
- [x] 2.3 收集 2 份报告，应用 7 Critical + 6 Major 修正 — DONE 2026-09-20
- [ ] 2.4 轻量 spot-check Oracle review（针对修正后的 🔴 项，验证修订充分性）
- [ ] 2.5 应用 spot-check 修正（如有）

## 3. Implementation (2.5d，修正 Oracle 🟠-2)

### 3.0 IGenomeRegistry::walk_ancestors 公开接口扩展（修正 Oracle 🟠-4 + 🔴-5）

- [ ] 3.0.1 include/agenticdsl/genome/genome.h 加 `struct LineageWalk { std::vector<uint64_t> intermediate_versions; std::vector<Genome> intermediate_metadata; }` + `virtual Result<LineageWalk, GenomeError> walk_ancestors(const std::string& name, uint64_t from_version, std::optional<uint64_t> to_version = std::nullopt) = 0;`
- [ ] 3.0.2 src/core/genome/registry_filesystem.cpp 实现 walk_ancestors（~80 行）
  - 从 from_version 出发，沿 parent 链向上 walk（closest-first）
  - cycle detection: `visited = (name, version)` pair + 10000 depth cap（C2 M1 修复复用）
  - 每版本 HMAC 校验（`hmac_verify` 已有，walk 中显式调用）
  - 失败映射: NotFound | BrokenLineage | IntegrityViolation
  - excludes-self: from_version 不在返回集中
- [ ] 3.0.3 tests/test_genome_registry.cpp 加 walk_ancestors 测试（4 case：正常 walk / cycle / depth cap / HMAC 失败）
- [ ] 3.0.4 genome-registry spec MODIFIED delta（specs/genome-registry/spec.md）— "5 public methods" → "6 public methods"

### 3.1 TransitionGuard 头文件（~120 行）

- [ ] 3.1.1 include/agenticdsl/evolution/transition_guard.h 完整 API
  - `enum class EvolutionState { Idle, Harness, Data, Model, Done }`
  - `struct EvolutionVerdict { bool can_proceed; EvolutionState recommended_next; std::string reason; std::vector<std::string> failed_conditions; }`
  - **GenomeVersion 复用**（spot-check New Issue 1）：从 `include/agenticdsl/types/attribution_record.h` `#include` 引入（**不在 transition_guard.h 重复定义**，避免 ODR 违规）。ADR-0086 v1.1 ship 在先，attribution_record.h 是单一所有权方
  - `constexpr bool constexpr_can_transition(EvolutionState from, EvolutionState to) noexcept`（修正 Oracle 🔴-3: literal type bool）
  - `EvolutionVerdict can_transition(EvolutionState from, EvolutionState to, const GenomeVersion& current, const GenomeVersion& last_harness_change)`
  - `EvolutionVerdict evaluate_readiness(const AttributionRecord& attribution, const RewardSignal& eval_signal, const ExecutionBudget& budget)`（修正 Oracle 🟠-5: 3 参，无冗余 confounders）

### 3.2 TransitionGuard 实现（~280 行）

- [ ] 3.2.1 src/modules/evolution/transition_guard.cpp 完整实现
  - constexpr_can_transition: 查表（H→M 禁 + Done→非 Idle 禁 + 其他允许）
  - can_transition: 调 constexpr + 包装 Verdict + 无 Genome fail-closed（修正 Oracle 🟠-2）
  - evaluate_readiness: 3 条件顺序检查 + 修正类型（RewardSignal::Quality::Poor / budget 显式维度）
- [ ] 3.2.2 事件发射（修正 Oracle 🟠-5: 2 个明确主题，幻影主题修正）
  - can_transition 拒绝 → emit `evolution.transition.denied` (per ADR-0068 Appendix A 新增)
  - evaluate_readiness 拒绝 → emit `evolution.readiness.denied` (per ADR-0068 Appendix A 新增)

### 3.3 ADR-0068 Appendix A 新增事件主题注册（修正 Oracle 🟠-5）

- [ ] 3.3.1 docs/adr/adr-0068-event-emission-contract.md Appendix A 增补 2 行
  - `evolution.transition.denied` (TransitionGuard emit on H→M 禁则违规)
  - `evolution.readiness.denied` (TransitionGuard emit on 3+1 条件失败)
- [ ] 3.3.2 specs/event-emission-contract/spec.md 增补 MODIFIED Requirement

### 3.4 测试（11-13 cases）

- [ ] 3.4.1 tests/test_transition_guard.cpp 新建
  - constexpr_can_transition 测试（4 case：H→M false / H→D true / 自环 true / Done→非 Idle false）
  - can_transition 运行期测试（5 case：H→M 拒绝 + reason + failed_conditions / H→D 允许 / 自环 no-op / 无 Genome fail-closed / Done→非 Idle 拒绝）
  - evaluate_readiness 3 条件测试（6 case：全满足 → Model / 条件 1 fail / 条件 2 fail（Poor）/ 条件 3 fail（预算不足）/ ADR-0086 不可用 fail-closed / estimated_llm_calls 默认 1）
  - 事件发射测试（2 case：can_transition 拒绝 emit / evaluate_readiness 拒绝 emit）
- [ ] 3.4.2 RED 验证 → GREEN 实施

## 4. Ship-with-fixes (0.5d)

- [ ] 4.1 派 Oracle 实施审查 background（focus 在 7 Critical 修正落地）
- [ ] 4.2 应用 Critical 修正（独立 commit 保持原子性）
- [ ] 4.3 spec amendments（如有）
- [ ] 4.4 跑全量 `ctest --output-on-failure`
  - 预期: 0 regression（除已知 pre-existing failures）
  - 重点关注 test_genome_registry (walk_ancestors 新增) + test_transition_guard (新 binary)

## 5. Doc sync (0.25d)

- [ ] 5.1 docs/research/rsi-three-operators-hydraforge-mapping-2026-09.md
  - §1.2 行 32 ADR-0086 v1.1 + C3 引用同步（从 🟡 placeholder → ✅/✅）
  - §3.2 C3 段同步 4→3+1 修正 + walk_ancestors 签名锁定
- [ ] 5.2 docs/architecture/self-evolution-architecture-2026-08.md §六 S4 promotion criteria 同步 3+1 条件
- [ ] 5.3 docs/adr/adr-0061-agent-evolution-and-solidification.md 附录 A 同步（如已写）
- [ ] 5.4 docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md §四 C3 状态更新（fill → ship）

## 6. Archive (0.25d)

- [ ] 6.1 openspec archive → archive/2026-09-20-2026-09-16-h-d-m-transition-guard/
- [ ] 6.2 git ls-files 验证 6 文件完整（Day-5 lesson）：
  - `.openspec.yaml`
  - `proposal.md`
  - `tasks.md`
  - `specs/transition-guard/spec.md`
  - `specs/event-emission-contract/spec.md` (NEW per 🟠-5)
  - `specs/genome-registry/spec.md` (NEW per 🟠-4)
  - 不通过则修复后重新 archive
- [ ] 6.3 commit: `feat(transition-guard): h-d-m-state-machine-3+1-conditions-walk-ancestors-extension`
- [ ] 6.4 通知 C4 fill author 引用 guard API + walk_ancestors 签名

## 7. Out-of-scope

- ❌ 不新建 3 算子接口 (IDataRSI/IHarnessRSI/IModelRSI) — Oracle M2 评审取消
- ❌ 不实现 Model-RSI 实际执行（依赖 ADR-0078）
- ❌ 不实现 IModelRSI（仅在 ADR-0078 下登记占位）
- ❌ 不实现完整自进化闭环（待 C4 pilot 验证）
- ❌ 不绑定 MutationGovernor.commit()（V1，YAGNI；governor 与 guard 职责分离）
- ❌ 不修改 ConfounderRecord 序列化 schema（属 ADR-0086 v1.1 范围）
- ❌ 不实现 walk_ancestors 之外的 IGenomeRegistry 扩展

## 8. References

- Oracle review sessions:
  - `bg_ba048665` (2026-09-20, Metis: 5 Critical + 5 Major)
  - `bg_fed9d7c0` (2026-09-20, Oracle: 7 Critical + 6 Major + 8 Minor)
- Prior Oracle reviews:
  - `ses_f45b96c94ffevTy454aeDBK7U2` (continuation, 7 项 action 列表)
  - `ses_f55f307f6ffeRJ9SIny8iUbZ8Y` (2026-09-16, M2 评审取消 3 算子接口)
- Project documents:
  - `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §四 C3
  - `docs/research/rsi-three-operators-hydraforge-mapping-2026-09.md`
  - `docs/architecture/self-evolution-architecture-2026-08.md`
  - `docs/adr/adr-0083-evaluator-reward-contract.md` (✅ V2)
  - `docs/adr/adr-0084-mutation-governance-contract.md` (✅ V1)
  - `docs/adr/adr-0086-credit-assignment-contract.md` (🟡 Amendment in flight)
  - `docs/adr/adr-0061-02-behavioral-regression.md` (✅ T14 Shipped)
  - `docs/adr/adr-0080-append-only-event-log.md` (✅ v1.2)
  - `docs/adr/adr-0068-event-emission-contract.md` (✅ v1.2.1)
- OpenSpec changes:
  - `openspec/changes/archive/2026-09-19-2026-09-16-genome-registry/` (C2 ✅ Shipped)
  - `openspec/changes/2026-09-20-adr-0086-v1-1-harness-change-confounder/` (ADR-0086 v1.1 amendment)
  - `openspec/changes/2026-09-16-harness-rsi-pilot/` (C4 placeholder)
- AGENTS.md patterns: §模式 8: OpenSpec dual-agent review（实施前硬性门）
