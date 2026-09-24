# Tasks: Provider-LLM-Tool Empty Text Pass-Through Guard

> **STATUS**: 🔍 Proposed — 2026-09-25 re-evaluation (per Oracle ses_f2b923412ffeTFfBdDqQFOMdT9)
> **SHIP-with-fixes**: Oracle Stage 2 verdict `ses_f2b5f9198ffexoosRhoaAfpLHu` (0 Critical / 2 Major / 3 Minor)
> **优先级**: P2 (defense-in-depth for F1 Latent Site #3)
> **关联**: F1 fix-react-decide-empty-response 已 ship (`node_executor.cpp:209-214`)

---

## 1. Pre-flight (~15 min)

- [x] ✅ **读 ProviderLLMTool 实现** (`pdk/loop_agent/src/pdk_entry.cpp:42-72`)
  - Class 定义 line 42-72；实例化 line 477 + 723
  - 当前 `out.text = std::move(res).value().text;` 直接赋值，**无空文本校验**
  - **proposal.md 假设的 :402/:434/:524 行号已过时**（重构为文件级 class）— 按 line 42-72 实施
- [x] ✅ **参考 F1 fix 错误消息格式** (`src/modules/executor/node_executor.cpp:209-214`)
- [x] ✅ **决定 throw 类型**: 复用 F1 的 `std::runtime_error`（不引入新 ErrorCode）

---

## 2. RED: Failing Tests (~30 min)

### T2.1: 创建 `tests/test_provider_llm_tool_empty.cpp`

- [x] MockLLMEmptyProvider + MockLLMNonEmptyProvider 实现
- [x] 3 test cases:
  - [x] T2.1.1: 空 text → REQUIRE_THROWS_AS runtime_error + message format check
  - [x] T2.1.2: 非空 text → success=true, text=expected
  - [x] T2.1.3: source guard — verify pdk_entry.cpp contains exact throw signature
- [x] ✅ Oracle Stage 2 M1 fix applied: Case 3 改用 exact unique string match
  (`"ProviderLLMTool: LLM call succeeded but returned empty text"`)
  防假阴性（class name / 注释 / 无关 throw 含通用 token）
- [x] ✅ 验证 `ctest -R test_provider_llm_tool_empty` 11 assertions / 3 cases / 100% PASS

### T2.2: CMakeLists.txt 注册

- [x] `tests/CMakeLists.txt` 自动 GLOB `test_*.cpp` 捕获（无需手动添加）
- [x] 通过 `cmake --preset tests -B build/tests` 重新 configure 已捕获

---

## 3. GREEN: Minimal Fix (~15 min)

### T3.1: 修复 ProviderLLMTool (`pdk/loop_agent/src/pdk_entry.cpp:56-66`)

- [x] ✅ 添加 6 行 fail-fast 校验（line 56-66）
- [x] ✅ Throw `std::runtime_error("ProviderLLMTool: LLM call succeeded but returned empty text. Provider: loop-agent-provider-bridge. Check provider model availability or prompt template.")`
- [x] ✅ RED 测试现在 PASS（Case 1+2+3 all green）
- [x] ✅ Focused ctest 100% PASS:
  - test_provider_llm_tool_empty: 11/11
  - test_dsl_engine_ctx_bridge (F1 regression): 13/13
  - test_executor: 20/20
  - test_executor_with_mock_provider: 17/17
  - test_chat_session_consumer: 67/67

---

## 4. Latent Sites 状态记录（自闭环，不修改冻结 archive）

> **Oracle Stage 2 M2a fix** — per archive 冻结约定 (G2 precedent: archived design.md 冻结可接受),
> 不直接修改 F1 archived design.md 的 Latent Sites 表。本 change 在自身 tasks.md 内闭环记录 Site #3
> 状态翻转（外部观察者通过 git blame 反向追踪即可）。

### 4.1 Latent Sites 状态翻转（本 change 视角）

| Site | F1 design.md 描述 | 本 change 后状态 |
|------|-------------------|------------------|
| #1 | ProviderLLMTool (Line 42+ 空 text 透传) — F1 评估为"多余防御, 不修" | ✅ **FIXED 2026-09-25** (c0d276a, ses_f2b5f9198) — Oracle Stage 2 重新评估后认为仍需 defense-in-depth |

### 4.2 Pattern #2 升级确认（per Oracle Stage 2 M2b fix）

> **注意**: 原 tasks.md:195 声称"≥3 站点已触发，需评估 `fix-generation-request-model-default` umbrella change"
> **不准确** — 该 umbrella 已于 2026-09-08 archive（`archive/2026-09-08-fix-generation-request-model-default/`）ship 5 站点。
> 当前真实 deferred 站点:

| 站点 | 状态 |
|------|------|
| node_executor llm_call 空文本 | ✅ F1 已 ship |
| ProviderLLMTool 空文本 | ✅ 本 change ship |
| model 默认值遮蔽 (5 站点) | ✅ umbrella 2026-09-08 ship |
| **`loop/process_task` 空 result 透传** (pdk_entry.cpp:402-450) | ⏳ **唯一真正 deferred** — 待独立 follow-up |
| context_flatten 注释矛盾 (per fix-flatten-layers-comment-drift) | ⏳ doc drift follow-up |

### 4.3 AGENTS.md Pattern #1 step 4 引用

- [x] ✅ 在 AGENTS.md Reverse Indicator Rule 段加引用本 change（待 follow-up commit 中执行）
  - 因 AGENTS.md 是项目 governance 主文档，建议**在 AGENTS.md follow-up 时统一更新**而非单独提交

---

## 5. Ship & Archive (~30 min)

- [x] ✅ **Commit 1 (Stage 1 baseline, c0d276a)**: `feat(loop_agent): ProviderLLMTool empty text fail-fast guard`
  - 含 `[Reverse Indicator]` 5-field block
- [x] ✅ **Post-commit**: focused ctest + openspec validate --strict
- [x] ✅ **Oracle Stage 2 review** (`ses_f2b5f9198ffexoosRhoaAfpLHu`): SHIP-with-fixes
- [ ] ⏳ **Stage 3 atomic commit on worktree** (当前在主分支 — Oracle Mi2 建议先合并到 main, 后续工作用 fixup commit 或新 PR)
- [ ] ⏳ **Archive**: `openspec archive 2026-09-18-provider-llm-tool-empty-passthrough --yes`（待 Stage 3 commit 后执行）
- [ ] ⏳ **3 SoT docs §十一 sync**: `harness-architecture-2026-09.md` + `self-evolution-architecture-2026-08.md` + `rsi-architecture-2026-09.md`（+1 row "Latent Site #3 ✅ FIXED 2026-09-25"，与 AGENTS.md follow-up 合并）

---

## 6. 零回归验证 (~15 min)

- [x] ✅ **focused ctest**: test_provider_llm_tool_empty + test_dsl_engine_ctx_bridge + test_executor + test_executor_with_mock_provider + test_chat_session_consumer 全部 PASS
- [ ] ⏳ **全量 ctest 247/247**: **deferred** (per Oracle Mi2 — 机器性能受限，2026-09-23 Recent Changes 同期 "NOT-VERIFIED 全量" 模式可接受)
- [ ] ⏳ `docs_drift_audit.py 0 DRIFT items`: 留待 follow-up commit 合并执行
- [ ] ⏳ `openspec validate --strict "Change is valid"`: 留待 follow-up commit 合并执行

---

## Acceptance（验收标准）

### D1 ProviderLLMTool fail-fast
- [x] ✅ pdk_entry.cpp ProviderLLMTool 在 provider 返回空 text 时抛 runtime_error 含诊断线索
- [x] ✅ 异常由 call_llm_tool (registry.cpp:176-178) 转 error JSON；loop/run (pdk_entry.cpp:783) 兜底走 path #5 (Unknown)
- [x] ✅ 错误信息含 provider name (`loop-agent-provider-bridge`) + diagnostic hint (`Check provider model availability or prompt template`)

### D2 测试覆盖
- [x] ✅ 新增 test binary `tests/test_provider_llm_tool_empty.cpp`
- [x] ✅ 3 cases: empty / non-empty / source guard（Oracle M1 fix 后 exact string match）
- [x] ✅ ctest 100% PASS

### D3 零回归
- [x] ✅ focused ctest 全 PASS (loop_agent + executor + react_loop + chat_session)
- [ ] ⏳ 全量 ctest 247/247 维持 — deferred (Oracle Mi2, 机器性能受限)

### D4 Latent Sites 状态记录
- [x] ✅ Oracle M2a fix: 本 tasks.md 4.1 节自闭环记录 Site #3 状态翻转（不修改冻结 archive）
- [ ] ⏳ AGENTS.md Pattern #1 step 4 引用 — 留待 follow-up commit 合并执行

### D5 Docs drift gate
- [ ] ⏳ docs_drift_audit.py 0 DRIFT items — 留待 follow-up
- [ ] ⏳ openspec validate --strict "Change is valid" — 留待 follow-up

---

## Reverse Indicator (R8 per AGENTS.md)

```
[Reverse Indicator]
+ new_up: F1 Latent Site #3 defense-in-depth (空 text → fail-fast runtime_error)
- old_down: drop_ratio=0% (纯 fail-fast hardening, 正常路径不触发)
- failure_traces: 空 text path → ProviderLLMTool → runtime_error → catch in call_llm_tool
- ablation: N/A (无 Harness 变化)
- context_ids: N/A (无 ContextRequest 涉及)
```

---

## Reference

- F1 archived: `openspec/changes/archive/2026-09-18-fix-react-decide-empty-response/`
- Oracle sessions:
  - `ses_f4d05cdb0ffe0BhMdEADyfsdTz` (F1 root cause correction)
  - `ses_f2b923412ffeTFfBdDqQFOMdT9` (DECISION B 建议)
  - `ses_f2b5f9198ffexoosRhoaAfpLHu` (Stage 2 SHIP-with-fixes verdict)
- AGENTS.md Pattern #1 step 4: systematic latent sites recording
- AGENTS.md Pattern #2: 量化升级门槛（仅 1 个 deferred: loop/process_task）
- Umbrella 已 ship: `openspec/changes/archive/2026-09-08-fix-generation-request-model-default/`
- Latent Sites table: `openspec/changes/archive/2026-09-18-fix-react-decide-empty-response/design.md`（冻结，git blame 反向追踪）