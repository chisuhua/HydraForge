# Proposal: Provider-LLM-Tool Empty Text Pass-Through Guard

> **STATUS: PLACEHOLDER** ⚠️
> **触发**: F1 `fix-react-decide-empty-response` SHIPPED 后 (2026-09-18) latent sites recording per AGENTS.md Pattern #1 step 4
> **追溯范围**: `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §四 F1 子节 "Latent Sites" 表 + `pdk/loop_agent/src/pdk_entry.cpp` provider bridge
> **优先级**: P2 (defense-in-depth, F1 fix 在主路径已防，但 latent sites 仍需守)
> **估时**: 1-2h
> **关联 ADR**: 无 (与 F1 fix 同主题, 但在不同 layer)

---

## Why（背景概要）

**F1 SHIP** (2026-09-18) 已在 `node_executor.cpp` main path `:194-205` + stream path `:147-157` 加 fail-fast 空校验。**但是**:

**Latent Sites (per F1 design.md Latent Sites 表)**:

| # | Site | 状态 | 风险 |
|---|------|------|------|
| 1 | `node_executor.cpp` main path | ✅ F1 fixed | — |
| 2 | `node_executor.cpp` stream path | ✅ F1 fixed | — |
| 3 | `ProviderLLMTool` (provider bridge 层) | ⚠️ TODO | **本 change** |
| 4 | `process_task` cognitive worker | ⚠️ TODO | follow-up |
| 5 | `flatten_layers` (非 react path, 初判错) | ✅ N/A | — |
| 6 | `GenerationRequest.model` default | ⚠️ TODO | ship-with-known-issue |

**Site #3 详细说明**:

`ProviderLLMTool` 是 pdk loop_agent 中 provider bridge 层 (per `pdk_entry.cpp:402/434/524`)。当 provider (LLM) 返回 `text=""` 时, **当前实现**仍写入 ctx (`ctx["text"] = ""`), 下游 tool_call 的 inja template `{{text}}` 渲染空字符串 → tool 可能"成功"执行但没拿到应有数据。

**实际案例**:
- `lib/loop/react.agent.md` 第 4 步 `act` 节点: tool_call args `description: "{{think_response}}"`
- 如果 think 节点 upstream ProviderLLMTool 写入空字符串, act 节点 inja 渲染 `description: ""` → tool 执行但描述为空 → silent degradation
- F1 fix 在 node_executor 输出处拦截, 但如果 ProviderLLMTool 直接被 loop_agent 调用 (不经 node_executor), 则 bypass F1 防护

**没有此 fix 的后果**:
- Latent site #3 仍可触发 silent empty text propagation
- Provider-LLM 调用路径与 executor 调用路径分裂 (双轨防御)
- AGENTS.md Pattern #1 step 4 systematic recording 缺一项

---

## What Changes（具体变更范围）

### In Scope

- **`pdk/loop_agent/src/pdk_entry.cpp`**: `ProviderLLMTool` 入口加 fail-fast 空校验（与 F1 node_executor 同 pattern）
  - 校验位置: provider 调用返回 `text` 后，写入 ctx 前
  - 校验逻辑: `text.empty()` → throw `LLMError` 或 `runtime_error` 含诊断线索
- **Tests**: 新增 `tests/test_provider_llm_tool_empty.cpp` 或扩展 `tests/test_loop_agent_plugin.cpp`
  - Case 1: ProviderLLMTool with MockEmptyTextProvider → expect throw + ctx unchanged
  - Case 2: ProviderLLMTool with MockNonEmptyTextProvider → expect ctx["text"] = expected
  - Case 3: 回归 — ensure F1 node_executor 空校验仍工作
- **AGENTS.md Pattern #1 step 4 update**: latent sites 表更新 Site #3 ✅ FIXED

### Out of Scope

- `process_task` cognitive worker 空校验 (Site #4，独立 follow-up)
- `GenerationRequest.model` default 修复 (Site #6，独立 follow-up)
- 新 ErrorCode 引入（per F1 minimal fix principle，复用 runtime_error）

---

## Acceptance（验收标准）

### D1 ProviderLLMTool fail-fast
- [ ] pdk_entry.cpp ProviderLLMTool 在 provider 返回空 text 时抛 runtime_error 含诊断线索
- [ ] 抛错时 ctx 不被污染（部分写入不允许）
- [ ] 错误信息含 provider name + node name（方便追溯）

### D2 测试覆盖
- [ ] 新增 test binary 或扩展现有 test binary
- [ ] 至少 3 cases: empty / non-empty / 回归
- [ ] ctest 100% PASS

### D3 零回归
- [ ] focused ctest 全 PASS (loop_agent + executor + react_loop + chat_session)
- [ ] 全量 ctest 247/247 维持（16 known pre-existing failures 不变）

### D4 Latent Sites 表更新
- [ ] F1 design.md Latent Sites 表 Site #3 标 ✅ FIXED
- [ ] AGENTS.md Pattern #1 step 4 引用本 change 作为同类防御模式例

### D5 Docs drift gate
- [ ] docs_drift_audit.py 0 DRIFT items
- [ ] openspec validate --strict "Change is valid"

---

## Capabilities（能力影响）

无新能力/接口变更。纯 defense-in-depth hardening。

---

## Impact（影响面）

- **代码层**: pdk_entry.cpp ProviderLLMTool ~+5 行 fail-fast 校验
- **测试层**: 新增 ~+50 行 test cases
- **文档层**: F1 design.md + AGENTS.md 微调
- **风险**: 低（fail-fast 仅 catch 已 silent failure 场景，不影响正常路径）
- **回归**: 极低（校验点新加，正常路径不触发）

---

## Tasks（执行步骤，~1-2h 估时）

### 1. Pre-flight (~15 min)
- [ ] TBD: 读 `pdk_entry.cpp:402/434/524` ProviderLLMTool 实现细节
- [ ] TBD: Oracle 评审 design（与 F1 main fix 的一致性 + 错误信息格式）
- [ ] TBD: 决定 throw 类型（runtime_error vs LLMError — per F1 复用 runtime_error 决策）

### 2. RED: Failing Tests (~30 min)
- [ ] TBD: 新建 `tests/test_provider_llm_tool_empty.cpp` 或扩展 `tests/test_loop_agent_plugin.cpp`
- [ ] TBD: 3 cases 测试 empty / non-empty / 回归
- [ ] TBD: 验证 FAIL（empty 路径未防护 → 测试 FAIL）

### 3. GREEN: Minimal Fix (~15 min)
- [ ] TBD: pdk_entry.cpp ProviderLLMTool 加 fail-fast 空校验 (~5 行)
- [ ] TBD: 验证 FAIL 测试现在 PASS
- [ ] TBD: 跑 focused ctest 100% PASS

### 4. Latent Sites 表更新 (~10 min)
- [ ] TBD: F1 design.md Site #3 标 ✅ FIXED（含 commit hash + Oracle session 引用）
- [ ] TBD: AGENTS.md Pattern #1 step 4 加引用本 change 作为同类防御模式例

### 5. Ship & Archive (~15 min)
- [ ] TBD: git atomic commit + archive `2026-09-18-provider-llm-tool-empty-passthrough`

---

## Reference

- F1 archived: `openspec/changes/archive/2026-09-18-fix-react-decide-empty-response/`
- Oracle session: `ses_f4d05cdb0ffe0BhMdEADyfsdTz` (F1 root cause correction)
- AGENTS.md Pattern #1 step 4: systematic latent sites recording
- Latent Sites table: `openspec/changes/archive/2026-09-18-fix-react-decide-empty-response/design.md` (待查)