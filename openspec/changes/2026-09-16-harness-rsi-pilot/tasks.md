# Tasks: Harness-RSI Pilot

> **STATUS**: DRAFT (REVISION 2026-09-21 per Oracle bg_3672cb57 + Metis bg_1f291bc4 dual-agent review)
> **3 项 Critical 修正**: C1 幻影 API + C2 core→PDK 反向依赖 + C3 IMutationGovernor veto 机制
> **3 项 Major 修正**: M1 审计配对 + M2 签名扩展 + M3 事件载荷 4/4 字段断言
> **Case 4 删除** (real LLM 推迟 Wave 3, per Metis 2.5)

## Phase 1: rdd-builder P0 (5-option approval gate) (0.1d)
- [ ] 1.1 调 rdd-builder P0 — 验证 proposal.md 状态从 PLACEHOLDER → DRAFT
- [ ] 1.2 验证 P0 5-option auto-decision — 当前 change scope 修订后估时 4.2d ≤ 5d, 推荐 rdd-builder 标准路径 (非 quick-path)
- [ ] 1.3 输出 `.planner-handoff.json` v3 (awaiting_builder flag)

## Phase 2: Pre-impl dual-agent review (AGENTS.md 模式 #8) (已 ship, 修订文档) (0.3d)
- [x] 2.1 Oracle bg_3672cb57 评审 (2026-09-21, 7m 16s) — ✅ 已 ship 3 Critical + 3 Major + 3 RED FLAGS
- [x] 2.2 Metis bg_1f291bc4 评审 (2026-09-21, 7m 18s) — ✅ 已 ship 5 DEAL-BREAKER + 6 必读文件
- [x] 2.3 应用 dual-agent 评审建议 — C1-C3 + M1-M3 + DB1/DB2 全部修正入 proposal.md
- [ ] 2.4 修订 spec.md/tasks.md/proposal.md 应用本评审 6 项修正 (Phase 2.4 of proposal)
- [ ] 2.5 派 Oracle 2nd review 验证修正后 spec/tasks/proposal 一致性 (Phase 2.5 of proposal)
- [ ] 2.6 Oracle 2nd review verdict ≥ 80/100 才能进 rdd-builder P0 implementation, 否则修订重审

## Phase 3: RED tests (0.5d)
- [ ] 3.1 test_harness_rsi_pilot.cpp Case 1 — prompt_delta apply RED
- [ ] 3.2 test_harness_rsi_pilot.cpp Case 2 — readiness denied + 4/4 事件载荷字段断言 RED
- [ ] 3.3 test_harness_rsi_pilot.cpp Case 3a — dangerous tool veto RED
- [ ] 3.4 test_harness_rsi_pilot.cpp Case 3b — trusted tool add positive RED
- [ ] 3.5 ctest test_harness_rsi_pilot RED: 4/4 FAIL (RED 阶段)

## Phase 4: GREEN implementation (2d, 含接口扩展)
- [ ] 4.0 **DB1 修复**: IToolRegistry::unregister_tool_function 实施
  - [ ] 4.0.1 include/agenticdsl/contract/itool_registry.h: +5 行 (新虚方法)
  - [ ] 4.0.2 src/common/tools/registry.h + .cpp: +15 行 (实现 unregister)
  - [ ] 4.0.3 src/common/tools/secure_tool_registry.h + .cpp: +10 行 (委托)
  - [ ] 4.0.4 验证 ctest 全量 251 零回归 (接口扩展向后兼容)
- [ ] 4.1 include/agenticdsl/evolution/harness_rsi.h — 定义 GenomeMutations + MutationGateContext + AppliedMutation + MutationError + MutationGovernancePolicy (5 struct + 4 error variant)
- [ ] 4.2 src/evolution/harness_rsi.cpp — apply_harness_mutation body
  - [ ] 4.2.1 门禁 1 — evaluate_readiness 集成 (transition_guard.h:55-59)
  - [ ] 4.2.2 失败路径 — emit evolution.readiness.denied 4 字段 + 零状态变更 + MutationError::NotReady
  - [ ] 4.2.3 门禁 2 — is_tool_allowed 内部 policy check (不调 IMutationGovernor::propose)
  - [ ] 4.2.4 mutation 路径 1 — prompt_delta (system_prompt 字段更新)
  - [ ] 4.2.5 mutation 路径 2 — tools_add (IToolRegistry::register_tool_function + AgentConfig.tools vector 同步)
  - [ ] 4.2.6 mutation 路径 3 — tools_remove (新加的 IToolRegistry::unregister_tool_function + vector 移除)
  - [ ] 4.2.7 mutation 路径 4 — workflow_patch 返回 MutationError::UnsupportedVariant
  - [ ] 4.2.8 返回 AppliedMutation 结构 (记录实际应用的 mutation 列表)
- [ ] 4.3 EventBuilder 实际发射代码 (transition_guard.cpp 集成, per Metis DB5)
  - [ ] 4.3.1 transition_guard.cpp: 替换 constexpr 常量为可调 helper
  - [ ] 4.3.2 新增 emit_evolution_transition_denied + emit_evolution_readiness_denied 2 helper
  - [ ] 4.3.3 harness_rsi.cpp 调 helper 而非裸 EventBuilder
- [ ] 4.4 src/evolution/CMakeLists.txt — 注册 harness_rsi.cpp
- [ ] 4.5 ctest test_harness_rsi_pilot GREEN: 4/4 PASS

## Phase 5: 全量验证 (0.5d)
- [ ] 5.1 ctest 全量 251 + 4 new tests = 255 — 零回归验证 (per AGENTS.md pattern #4)
- [ ] 5.2 adr_lint 0 errors — ADR-0088/0084/0068 状态字段验证
- [ ] 5.3 docs_drift_audit 0 DRIFT — capability-map + active-status 同步检查
- [ ] 5.4 openspec validate --strict PASS
- [ ] 5.5 git diff --stat 验证 (per AGENTS.md pattern #4: +468/-0 行, 含 IToolRegistry BREAKING 接口扩展)

## Phase 6: Oracle post-impl SHIP-with-fixes review (0.3d)
- [ ] 6.1 派 Oracle post-impl 评审 — 模式 #4 SHIP-with-fixes 流程 (Critical 必应用 + Major 纳入 + Minor 留 follow-up)
- [ ] 6.2 应用 Oracle 修正 — 不 amend baseline, 新增独立 commit (per pattern #4)
- [ ] 6.3 派 Oracle 2nd 复核 — 拿 APPROVE 才能 ship

## Phase 7: Go/No-Go Decision Record (含 Oracle 独立复核) (0.5d)
- [ ] 7.1 写 `docs/audits/2026-XX-XX-harness-rsi-pilot-go-no-go.md` Decision Record skeleton (实施前已写)
- [ ] 7.2 Decision Record 填实: (a) 原始 ctest 输出片段 (非转述) + 事件捕获 dump
- [ ] 7.3 派独立 Oracle 复核 Decision Record 证据 (~30 min, per AGENTS.md pattern #8 ROI, **不**复核作者结论段)
- [ ] 7.4 Go 路径 (4/4 mock PASS + ctest 255 零回归): 立项 ADR-0078 Model-RSI pilot (Wave 3) → 写新 OpenSpec change `2026-XX-XX-adr-0078-model-rsi-pilot`
- [ ] 7.5 No-Go 路径: 归档 skeleton + 等需求驱动 (YAGNI 有效, 不是失败)

## Phase 8: Archive + Sync (0.2d)
- [ ] 8.1 openspec archive 2026-09-16-harness-rsi-pilot → archive/2026-XX-XX-2026-09-16-harness-rsi-pilot/
- [ ] 8.2 git ls-files 验证 4 文件完整 (per AGENTS.md Day 5 lesson)
- [ ] 8.3 更新 master plan §四 C4 状态 + §十 Drift Log + §十一 Adjustment Log
- [ ] 8.4 更新 active-status.md — ctest 251→255 + active -1 (C4 archived)
- [ ] 8.5 更新 Roadmap 附录 B — C4 实施路径 + Oracle sessions + dual-agent 评审记录

## Acceptance

- [ ] AC-1: apply_harness_mutation 轻量函数 ship (修订签名, 6 参, 消除 core→PDK 依赖)
- [ ] AC-2: IToolRegistry::unregister_tool_function 添加 (DB1 修复)
- [ ] AC-3: 双门禁集成完整 (evaluate_readiness + is_tool_allowed 内部 policy check)
- [ ] AC-4: Mock Case 1 (prompt_delta apply) PASS
- [ ] AC-5: Mock Case 2 (readiness denied) PASS — 4/4 事件载荷字段断言
- [ ] AC-6: Mock Case 3a (dangerous tool veto) PASS
- [ ] AC-7: Mock Case 3b (trusted tool add positive) PASS
- [ ] AC-8: ctest 全量 255 tests 零回归
- [ ] AC-9: Decision Record ship — 含原始 ctest 输出 + Oracle 独立复核结论 + Go/No-Go 结论

## Out of scope (per Oracle + Metis MUST NOT)

- ❌ IHarnessRSI / IModelRSI 接口 (ADR-0088 D4 显式取消, grep 0 hits 必须)
- ❌ 调 `IMutationGovernor::propose` (DB2 选 1: 内部 is_tool_allowed policy check)
- ❌ include `agenticdsl/pdk/chat_session.h` 从 `src/evolution/` (C2 core→PDK 反向依赖)
- ❌ workflow_patch mutation 路径实现 (返回 UnsupportedVariant, Wave 3 后续)
- ❌ 真实 LLM 1-turn 验证 (Case 4 删除, 推迟 Wave 3, Decision Record 注明)
- ❌ LLM 响应差异作 Go 判据 (Metis 3.1 confirmation bias 风险)
- ❌ 真实 LoRA 训练 / 多 Agent 协同进化 / Meta Co-Evolution

## 估时分解 (修订后)

| 阶段 | 任务 | 估时 |
|------|------|------|
| Phase 1 | rdd-builder P0 approve | 0.1d |
| Phase 2 | dual-agent review + 文档修订 + Oracle 2nd review | 0.5d |
| Phase 3 | RED tests | 0.5d |
| Phase 4.0 | DB1: IToolRegistry::unregister_tool_function 实施 | 0.1d |
| Phase 4.1-4.3 | harness_rsi.h/cpp + EventBuilder 集成 | 1.5d |
| Phase 5 | 全量验证 | 0.5d |
| Phase 6 | Oracle SHIP-with-fixes | 0.3d |
| Phase 7 | Decision Record (含 Oracle 独立复核 30 min) | 0.5d |
| Phase 8 | Archive + Sync | 0.2d |
| **总计** | | **4.2d** ≈ **5d 留 buffer** (符合 proposal 1-2 周估时下限) |

## References

- Oracle bg_3672cb57 (2026-09-21, 7m 16s) — 3 Critical + 3 Major + 3 RED FLAGS
- Metis bg_1f291bc4 (2026-09-21, 7m 18s) — 5 DEAL-BREAKER + 6 必读文件 + Case 4 删除建议
- Oracle bg_154a7031 (2026-09-21, 5m 23s) — 上一轮评审 (D8 ship 期间)
- Metis bg_139e388b (2026-09-21, 1m 45s) — 上一轮评审 (D8 ship 期间)
- `docs/adr/adr-0088-h-d-m-transition-guard.md` D4 (IHarnessRSI 取消决策, line 96-98)
- `docs/adr/adr-0068-event-emission-contract.md` v2.2 (D8 ship, 2026-09-21)
- `docs/adr/adr-0084-mutation-governance-contract.md` (V1 ship)
- AGENTS.md 模式 #4 (SHIP-with-fixes) + 模式 #8 (pre-impl dual-agent review)
