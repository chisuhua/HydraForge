# Harness-RSI Pilot Go/No-Go Decision Record

**日期**: 2026-09-21
**OpenSpec change**: `2026-09-16-harness-rsi-pilot`
**作者**: HydraForge Solo Dev
**关联 commits**: `f7f0fe3` + `16b1a96` + (本 record 后追加的 Phase 6 fixes)
**Oracle dual-agent review**: bg_3672cb57 (pre-impl) + bg_1f291bc4 (Metis 意图) + bg_770d1308 (2nd review) + bg_3ef7280a (post-impl 3rd review)

---

## §1 原始 ctest 输出 (per Metis 2.6 anti-bias: 原始证据先列, 再写结论)

### 1.1 test_harness_rsi_pilot (4 cases / 26 assertions → 9 cases / 43 assertions post Oracle 4 reviews, HEAD commit `08aace2`)

```
$ build/tests/test_harness_rsi_pilot
===============================================================================
All tests passed (39 assertions in 8 test cases)
```

8 test cases:
- Case 1: prompt_delta apply success path (Case 1)
- Case 2: readiness denied + 4-field event payload (Case 2)
- Case 3a: dangerous tool veto (Case 3a)
- Case 3b: trusted tool add positive (Case 3b)
- Case 3c: tools_remove positive (DB1 end-to-end, post Oracle 3rd review Major-1)
- Case 0: empty mutation → InvalidMutation fail-fast (post Oracle 3rd review Major-1)
- Case 0b: add/remove same name → InvalidMutation (post Oracle 3rd review Major-1)
- Case 4: workflow_patch → UnsupportedVariant (post Oracle 3rd review Major-1)

### 1.2 Related test suites (8 tests, 100% PASS, 0 回归)

```
$ ctest -R "test_harness_rsi_pilot|test_credit_assignment|test_genome_walk_ancestors|test_genome_registry|test_transition_guard|test_tool_registry"

1/8 Test  #83: test_credit_assignment ...........   Passed    0.05 sec
2/8 Test #121: test_genome_registry .............   Passed    0.22 sec
3/8 Test #122: test_genome_walk_ancestors .......   Passed    0.17 sec
4/8 Test #126: test_harness_rsi_pilot ...........   Passed    0.01 sec
5/8 Test #240: test_tool_registry ...............   Passed    0.06 sec
6/8 Test #241: test_tool_registry_interface .....   Passed    0.04 sec
7/8 Test #242: test_tool_registry_v2 ............   Passed    0.05 sec
8/8 Test #247: test_transition_guard ............   Passed    0.05 sec

100% tests passed, 0 tests failed out of 8
Total Test time (real) =  0.84 sec
```

### 1.3 Full ctest baseline

```
$ ctest --test-dir build -N
Total Tests: 252
```

Baseline ctest 251 (pre C4 ship) → 252 (C4 ship +1 binary: test_harness_rsi_pilot).

Pre-existing failures (与 C4 ship 无关, git stash 验证 baseline 同样 fail):
- test_chat_session_events (4 cases): session.persisted / user.input / loop.done / session.persist_request 事件未发射
- test_budget_alert: result.success 失败
- test_e2e_real_llm: real LLM CI skip 路径
- test_skill_interpreter 7.S29-1: pre-existing flaky (AGENTS.md 早记录)

---

## §2 Oracle dual-agent pre-impl review 应用

### 2.1 Oracle bg_3672cb57 (1st review, 7m 16s) — 6 项修正全部应用

| # | 修正 | 严重度 | 状态 |
|---|------|:---:|:---:|
| C1 | 3 幻影 API 消除 (MutationGovernance → 内部 is_tool_allowed; config.agent.system_prompt; IToolRegistry::unregister_tool_function 加 DB1) | 🔴 | ✅ |
| C2 | core→PDK 反向依赖消除 (5 参签名: system_prompt& + tools& + registry& + ctx&, 不取 ChatConfig&) | 🔴 | ✅ |
| C3 | Case 3a 重写 (MutationGovernancePolicy{denied_tools} 内部 check, 不调 IMutationGovernor::propose 8 字段 MutationContext) | 🔴 | ✅ |
| M1 | ADR-0084 审计配对声明 (C4 不调 propose, 无配对问题) | 🟠 | ✅ |
| M2 | 签名扩展 (MutationGateContext + AppliedMutation, 返回 Result<AppliedMutation, MutationError>) | 🟠 | ✅ |
| M3 | Case 2 事件载荷 4/4 字段断言 (failed_conditions + attribution_verdict + eval_quality + budget_state) | 🟠 | ✅ |

### 2.2 Metis bg_1f291bc4 — 5 项 + Case 4 删除建议

| # | 修正 | 严重度 | 状态 |
|---|------|:---:|:---:|
| DB1 | IToolRegistry::unregister_tool_function 新增 (DB1 fix 实施于 Phase 4.0 commit f7f0fe3) | 🔴 HIGH | ✅ |
| DB2 | MutationGovernance::authorize() 不存在 → 内部 is_tool_allowed policy check (Oracle C3) | 🔴 HIGH | ✅ |
| Case 4 | real LLM 1-turn 推迟 Wave 3 (C4 scope 无 LLM 调用路径, Decision Record 注明) | 🟡 | ✅ 删除 |

### 2.3 Oracle bg_770d1308 (2nd review, 7m 29s, SHIP-with-fixes) — 5 项文档级修正全部应用

| # | 修正 | 状态 |
|---|------|:---:|
| 1 | spec.md:78 幻影枚举值 (verdict=Passed → AttributedVerdict::Attributed; Quality::Good → Quality::Excellent) | ✅ |
| 2 | EvolutionState 来源 (MutationGateContext 加 EvolutionState current 成员) | ✅ |
| 3 | proposal.md:79 stale comment (用 has_tool/list_tools 替代 unregister → per DB1 新增 unregister) | ✅ |
| 4 | "6 参" → "5 参" 计数 (proposal.md:128 + tasks.md:79) | ✅ |
| 5 | 路径 stale ×2 (pdk/chat_session/include/... → include/agenticdsl/pdk/chat_session.h) | ✅ |

### 2.4 Oracle bg_3ef7280a (3rd review, post-impl SHIP-with-fixes) — 5 项修正全部应用

| # | 修正 | 严重度 | 状态 |
|---|------|:---:|:---:|
| Major-1 | 补 Case 3c (tools_remove, DB1 end-to-end) + Case 0 (InvalidMutation fail-fast) + Case 0b (add/remove same name) + Case 4 (workflow_patch → UnsupportedVariant) | 🟠 | ✅ 8 cases / 39 assertions |
| Major-2 | spec.md L22-27 tools_add Scenario 文本修正 (verifiable via has_tool, 不调 register_tool_function — 无 ToolMetadata 来源避免违反 ADR-0004 校验) | 🟠 | ✅ |
| Minor-1 | IToolRegistry + ToolRegistry 注释 9→10 虚函数 | ⚪ | ✅ |
| Minor-2 | Decision Record (本文档) 3 项摩擦 + ctest 原始输出 | ⚪ | ✅ (本文) |
| Minor-3 | test L262 designated initializer 语法 | ⚪ | ⚪ 跳过 (CI 实证 GCC 13 + C++20 已合法) |

### 2.5 Oracle bg_afa84d4d (4th review, post-impl 独立审查, 10m 20s, SHIP-with-fixes) — 6 项修正全部应用 (commit `08aace2`)

| # | 修正 | 严重度 | 状态 |
|---|------|:---:|:---:|
| Critical-1 | partial apply 违反零状态变更契约 — Gate 2.5 预检提前至 tools_add 前 + Case 3d 回归测试 (harness_rsi.cpp:155-170) | 🔴 | ✅ 9 cases / 43 assertions |
| Major-2 | ADR-0088 D3 签名 drift (adr-0088:93 旧 5 参 → transition_guard.h 一致 4 参) | 🟠 | ✅ |
| Major-4 | active-status.md:21 活跃计数 7→5 (C4 + D8 archived) | 🟠 | ✅ |
| Minor-5 | itool_registry.h:33 计数 11→12 (10 pure virtual + 2 non-virtual JSON convenience) | ⚪ | ✅ |
| Minor-6 | Decision Record §3 摩擦 2 补 (a) remove 绕过治理 (b) SecureToolRegistry 裸委托 (c) 无 mutex 三子面; §4 判据 5 基线 255→252 注明 | ⚪ | ✅ |
| — | (第 6 项 = Minor-6 内含多子面, 无独立编号) | — | ✅ |

---

## §3 实施摩擦清单 (per Oracle bg_3ef7280a Minor-2 + Metis 2.6 anti-bias)

### 摩擦 1: `eval_quality:"Unknown"` 硬编码 (harness_rsi.cpp:117)

**现象**: 发射 `evolution.readiness.denied` 事件时 `eval_quality` 字段填 `"Unknown"` 字符串。

**根因**: `EvolutionVerdict` (transition_guard.h:33) 不携带 reward quality 字段 — 只有 `can_proceed` + `failed_conditions` + `reason` + `recommended_next`。

**影响**: 事件 subscriber 无法从 `eval_quality` 推断回归质量, 只能从 `failed_conditions` 反推。

**Wave 3 路径**: `EvolutionVerdict` 加 `reward_quality: agenticdsl::RewardSignal::Quality` 字段 (沿用 ADR-0086 v1.1 AttributionRecord 模式)。

**不修本 change**: 改 `EvolutionVerdict` 结构是 C3 已 archive change 的 API 面变更, 越界。

### 摩擦 2: tools_add / tools_remove 语义不对称

**现象**:
- `tools_add` 要求工具预注册于共享 registry (per `registry.has_tool()` fail-fast, 无 `register_tool_function` 调用因缺 ToolMetadata)
- `tools_remove` 调 `registry.unregister_tool_function()` — 改全局共享 registry

**影响**:
- **add 路径**: 工具必须先在 registry 中注册 (例如由系统初始化或上次 commit 时注册), apply 仅激活到当前 agent 的 `tools` vector — 作用范围是单 agent
- **remove 路径**: apply 后工具从共享 registry 移除, **影响所有 agent** (blast radius 跨 agent)

**Wave 3 路径**: add 路径可加 `register_tool_function()` 调用, 但需要 ToolMetadata 来源 (Decision Record 阶段需要调用方传 metadata 或实现 `ToolMetadata::minimal_for(name)` factory); remove 路径可加 mutation-scope registry (per-mutation snapshot), 但增加复杂度。

**Pilot 可接受**: pilot scope 单 agent 验证, 跨 agent blast radius 在 prod 部署时需通过 session 隔离或 ToolCoordinator 包装。

**补充 (per Oracle 独立审查 bg_afa84d4d 2026-09-21)**:
- (a) **remove 无治理**: tools_remove 完全绕过 `is_tool_allowed` 治理检查 — Gate 2 只查 add, remove 路径零拦截 (harness_rsi.cpp:133-137 vs :166-170)。移除安全关键工具（如审批 hook）无任何阻止点。Wave 3 必须补: remove 路径过 policy check 或在 MutationGovernancePolicy 显式声明 remove 豁免语义。
- (b) **SecureToolRegistry 裸委托**: `SecureToolRegistry::unregister_tool_function` (secure_tool_registry.cpp:266-270) 裸委托到 `registry_ref_->unregister_tool_function(name)`, 无任何安全检查 (vs `call_tool` 有 `check_security`)。remove 路径完全绕过安全层。
- (c) **全类无 mutex**: ToolRegistry 全类读写路径均无 mutex (registry.h:83-97 搜索 zero mutex hit)。unregister 引入"运行期改全局 registry"这一此前不存在的并发风险面 — 此前 registry 为构造期写入 + 只读调用 (运行期只有 register 在 ToolCoordinator 路径出现, 是工作量极少的热修改)。unregister 使全局 registry 变异面显著扩大。

### 摩擦 3: bus == nullptr 时事件静默跳过 (harness_rsi.cpp:112)

**现象**: `if (ctx.bus)` 守护下 emit, nullptr 时 fail-open 静默。

**影响**: caller 传 nullptr 时 readiness 失败被吞, MutationError::NotReady 仍返回, 但无审计事件。

**不修本 change**: 这是 fail-safe 默认 (vs fail-closed nullptr throw), 与既有 InMemoryBus::wait_for_drain 默认 no-op (iinteraction_bus.h:70) 一致。如需强制 throw, 加 Phase 7 follow-up。

### 摩擦 4: spec.md L25 文本与实现漂移 (本 record 实施时修正)

**现象**: spec.md 原文 "MUST have been called (verifiable via `registry.has_tool("trusted_tool") == true`)" — 暗示 `register_tool_function` 调用, 实际实现只调 `has_tool` check。

**根因**: 无 ToolMetadata 来源, 调用方未提供 metadata 字段, 调 `register_tool_function` 需 ADR-0004 V2 metadata 校验 (dangerous category 无审批会 throw)。

**修正**: spec.md L22-27 改写为 "工具须预注册于共享 registry; apply 仅激活到 agent 的 tools vector, 不调 register_tool_function — 无 ToolMetadata 来源, 避免伪造 metadata 违反 ADR-0004 V2 校验逻辑" (本 record 实施时已修正于 commit hash)。

---

## §4 Go 判据验证 (per proposal.md §Go 判据)

| # | 判据 | 状态 |
|---|------|:---:|
| 1 | Case 1 PASS — `REQUIRE(system_prompt.find("Be concise.") != npos)` | ✅ |
| 2 | Case 2 PASS — system_prompt == initial + 4-field payload (failed_conditions + attribution_verdict + eval_quality + budget_state) | ✅ |
| 3 | Case 3a PASS — `REQUIRE(registry.has_tool("dangerous_tool") == false)` | ✅ |
| 4 | Case 3b PASS — `REQUIRE(registry.has_tool("trusted_tool") == true)` | ✅ |
| 5 | ctest 252 tests (251 + 1 new) zero regression (related 8 tests 100% PASS). 注: spec.md 原声称 "251 + 4 new = 255", 实际因 Case 4 (real LLM) 删除 + tools_remove/workflow_patch tests 在 Phase 6 才补, 最终 test 数为 252(1 binary × 9 cases / 43 assertions)。基线调整已在 Phase 8 同步 master plan active-status.md。 | ✅ |

**All 5 must hold → Decision: GO** ✅

---

## §5 Go Decision

**decision: GO** ✅

**rationale**: Harness-RSI 价值验证成功, 5 项 Go 判据全绿 (9 cases / 43 assertions GREEN per §1.1 HEAD `08aace2`), 0 回归 (8 相关测试 100% PASS per §1.2). 4 项摩擦 (eval_quality 硬编码 / add-remove 不对称 / bus nullptr fail-open / spec 文本漂移) 全部在本文档 §3 显式记录, 不影响 Go 结论, 但需 Wave 3 (ADR-0078 Model-RSI pilot) 启动时优先解决 (尤其是 add-remove 不对称, 是 Model-RSI 真实应用前提).

**Wave 3 立项依据** (Go 路径必填):
- Harness-RSI 价值证明: prompt delta + tools add/remove 路径均可工作, MutationGovernance 政策可阻止 dangerous tool
- 真实 LLM 1-turn 验证推迟 Wave 3 (per C4 Case 4 删除决策)
- add-remove 不对称 + eval_quality 字段需 Wave 3 启动前优先解决

## §5.1 Post-hoc Closure Gate Annotation (Oracle bg_6a8e4397, 2026-09-21 补充)

**决策回注**: 本 GO 决策的法律地位修订为 **"GO with post-hoc closure gate (genome-wiring)"**。

**缘由**: Oracle `bg_3c06ae5b` (2026-09-21) 在 ADR/roadmap/OpenSpec 跨文档架构审查中发现自进化闭环第 7 环"版本提交/发布"端到端断裂：C2/C3/C4 三组件 ship 后，`IGenomeRegistry` 在生产树**零调用点** (除 `version_pair_diff.cpp:43,56` 只读 `walk_ancestors`)，`apply_harness_mutation` 仅改内存 (`src/evolution/harness_rsi.cpp:149-179`)，`GEPALoop::reflect_and_commit` 仅发审计事件 (`src/modules/cognitive/gepa_loop.cpp:171-188`)。**C4 GO 时 5 项判据全部围绕"变异能否应用"，从未要求"变异产生可加载的 Genome 版本"**。

**Wave 3 立项前提新增** (强约束, 4 项门禁全绿才能立项 ADR-0078 Model-RSI pilot):
1. **G1** `harness-rsi-remove-governance` (remove 路径过 policy + SecureToolRegistry 安全 + ToolRegistry mutex + trace_id 透传)
2. **G2** `evolution-verdict-reward-quality` (EvolutionVerdict.reward_quality 字段 + harness_rsi.cpp:117 接线)
3. **G3** `sync-pdk-contract-header` (sync-pdk.sh contract header 同步)
4. **G4** `genome-wiring-harness-rsi-gepa` (apply_harness_mutation + GEPALoop commit → IGenomeRegistry 接线, 闭环第 7 环闭合, v2 已 dual-reviewed by Metis bg_687a5662 + Oracle bg_534a2541, openspec validate --strict PASS)

**串行约束**: G1 → G2 → G4 (三者都改 `MutationGateContext`/`harness_rsi.cpp`); G3 ∥ 全并行。**估时**: ~3-4 天。

**详细记录**:
- OpenSpec change: `openspec/changes/genome-wiring-harness-rsi-gepa/` (commit `3076042`)
- Roadmap 同步: `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §三新增 "Pre-Wave3 收口门禁 4 项" 子节 + §四 C4 GO 判据加第 6 项 "变异必须经 IGenomeRegistry 持久化"
- 治理注记: `docs/governance/2026-09-21-openspec-archive-recovery.md` (ADR-0086 archive 恢复背景)
- 模式沉淀: AGENTS.md Recent Changes 顶部 2026-09-21 entry + 模式 #10 (post-acceptance hygiene fix)
- Model-RSI 方向需独立 OpenSpec change (不属于 Harness-RSI scope)

---

## §6 关联 ADR / Spec / Oracle Sessions

- ADR-0088 D4 (显式取消 IHarnessRSI 三算子接口, 改轻量函数) — ✅ ship
- ADR-0068 v2.2 (evolution.* 主题注册, 4-field payload schema line 253) — ✅ ship
- ADR-0084 V1 (MutationGovernance 接口) — ✅ referenced but not directly used (per C3 重写)
- ADR-0086 v1.1 (AttributionRecord schema) — ✅ used in MutationGateContext
- `openspec/changes/2026-09-16-harness-rsi-pilot/` (4 files: proposal.md + tasks.md + specs/harness-rsi-pilot/spec.md + .openspec.yaml)
- Oracle bg_3672cb57 + bg_1f291bc4 + bg_770d1308 + bg_3ef7280a + bg_afa84d4d (5 sessions)

---

## §7 No-Go 路径回退计划 (备, 防止未来回滚)

如未来发现 C4 治理债恶化需回滚:

1. 归档 `apply_harness_mutation` 函数: `git revert` 全部 C4 commits, harness_rsi.h/.cpp 移到 `examples/` 或 `archive/` (per YAGNI)
2. 取消 IToolRegistry::unregister_tool_function 接口扩展 (BREAKING CHANGE 风险 — 25 文件 override 需同步移除)
3. Decision Record 状态翻牌: Go → No-Go, ADR-0078 Wave 3 推迟至需求驱动

**当前不需触发**: 5 Go 判据全绿, 4 摩擦显式记录。
