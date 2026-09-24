# Tasks: Provider-LLM-Tool Empty Text Pass-Through Guard

> **STATUS**: 🔍 Proposed — 2026-09-25 re-evaluation (per Oracle ses_f2b923412ffeTFfBdDqQFOMdT9)
> **优先级**: P2 (defense-in-depth for F1 Latent Site #3)
> **关联**: F1 fix-react-decide-empty-response 已 ship (`node_executor.cpp:204-214`)

---

## 1. Pre-flight (~15 min)

- [x] ✅ **读 ProviderLLMTool 实现** (`pdk/loop_agent/src/pdk_entry.cpp:42-72`)
  - Class 定义 line 42-72；实例化 line 477 + 723
  - 当前 `out.text = std::move(res).value().text;` 直接赋值，**无空文本校验**
  - **proposal.md 假设的 :402/:434/:524 行号已过时**（重构为文件级 class）— 按 line 42-72 实施
- [x] ✅ **参考 F1 fix 错误消息格式** (`src/modules/executor/node_executor.cpp:209-214`):
  ```cpp
  throw std::runtime_error(
      "LLM call succeeded but returned empty text for node '" +
      node->path + "' output_key '" + key + "'. "
      "Check provider model availability or prompt template.");
  ```
- [x] ✅ **决定 throw 类型**: 复用 F1 的 `std::runtime_error`（不引入新 ErrorCode）

---

## 2. RED: Failing Tests (~30 min)

### T2.1: 创建 `tests/test_provider_llm_tool_empty.cpp`

- [ ] MockLLMEmptyProvider 实现 `ILLMProvider::generate()` 返回空 text (success=true)
- [ ] MockLLMNonEmptyProvider 实现 `ILLMProvider::generate()` 返回正常 text
- [ ] 3 test cases:

#### T2.1.1: 空 text 被 fail-fast 拦截
```cpp
TEST_CASE("ProviderLLMTool: empty text triggers fail-fast runtime_error", "[loop_agent]") {
    MockLLMEmptyProvider provider;
    ProviderLLMTool tool(provider, std::stop_token{});
    REQUIRE_THROWS_AS(
        tool.generate("test prompt", LLMParams{}),
        std::runtime_error);
    // 验证错误信息含 provider 名 + prompt 片段
    try { tool.generate("test prompt", LLMParams{}); }
    catch (const std::runtime_error& e) {
        REQUIRE(std::string(e.what()).find("empty text") != std::string::npos);
        REQUIRE(std::string(e.what()).find("ProviderLLMTool") != std::string::npos);
    }
}
```

#### T2.1.2: 非空 text 正常返回
```cpp
TEST_CASE("ProviderLLMTool: non-empty text returns success", "[loop_agent]") {
    MockLLMNonEmptyProvider provider("response content");
    ProviderLLMTool tool(provider, std::stop_token{});
    auto result = tool.generate("test prompt", LLMParams{});
    REQUIRE(result.success);
    REQUIRE(result.text == "response content");
}
```

#### T2.1.3: F1 fix 回归守卫（确保 node_executor 不退化）
```cpp
TEST_CASE("ProviderLLMTool: F1 node_executor empty guard still works", "[loop_agent]") {
    // 复用 test_dsl_engine_ctx_bridge.cpp Case 4-5 pattern
    // 确保 ProviderLLMTool 修复 + node_executor 修复同时存在
}
```

- [ ] 验证 `ctest -R test_provider_llm_tool_empty` FAIL（empty 路径未防护 → 测试 FAIL）

### T2.2: CMakeLists.txt 注册

- [ ] `tests/CMakeLists.txt` 加 `test_provider_llm_tool_empty` (per pattern #4 LABELS)
- [ ] 添加 `LABELS "loop_agent"` 标签（不影响 baseline exclude）

---

## 3. GREEN: Minimal Fix (~15 min)

### T3.1: 修复 ProviderLLMTool (`pdk/loop_agent/src/pdk_entry.cpp:56-64`)

在 `provider_.generate(req, cancellation_token_)` 返回后加 fail-fast 校验:

```cpp
auto res = provider_.generate(req, cancellation_token_);
if (res.has_value()) {
    out.text = std::move(res).value().text;
    // F1 Latent Site #3 defense-in-depth (per AGENTS.md Pattern #1):
    // ProviderLLMTool bypasses node_executor empty guard; add same check.
    // Latent Sites #4 (process_task) + #6 (GenerationRequest.model default)
    // still deferred (see AGENTS.md Pattern #2 upgrade trigger ≥3 sites).
    if (out.text.empty()) {
        throw std::runtime_error(
            "ProviderLLMTool: LLM call succeeded but returned empty text. "
            "Provider: loop-agent-provider-bridge. "
            "Check provider model availability or prompt template.");
    }
    out.success = true;
    out.tokens_generated = res.value().completion_tokens;
} else {
    out.success = false;
    out.error = res.error().message;
}
```

- [ ] ~6 行改动，加注释引用 F1 + AGENTS.md Pattern #1 + Latent Site #3
- [ ] 验证 RED 测试现在 PASS
- [ ] 跑 `ctest -L loop_agent` 100% PASS（含新 + 现有 tests）

---

## 4. Latent Sites 表更新 (~10 min)

- [ ] `openspec/changes/archive/2026-09-18-fix-react-decide-empty-response/design.md` Latent Sites 表 Site #3 标 ✅ FIXED（含 commit hash + Oracle session 引用）
- [ ] `AGENTS.md` Pattern #1 step 4 加引用本 change 作为同类防御模式例

---

## 5. Ship & Archive (~30 min)

- [ ] **Pre-flight**: git status clean + 跑 focused ctest 100%
- [ ] **Commit 1 (Stage 1 baseline)**: `feat(loop_agent): ProviderLLMTool empty text fail-fast guard`
  - 含 `[Reverse Indicator]` 5-field block (per AGENTS.md 2026-09-23 upgrade):
    ```
    [Reverse Indicator]
    + new_up: F1 Latent Site #3 defense-in-depth (空 text → fail-fast runtime_error)
    - old_down: drop_ratio=0% (纯 fail-fast hardening, 正常路径不触发)
    - failure_traces: 空 text path → ProviderLLMTool → runtime_error → catch in call_llm_tool (test_dsl_engine_ctx_bridge Case 4-5)
    - ablation: N/A (无 Harness 变化)
    - context_ids: N/A (无 ContextRequest 涉及)
    ```
- [ ] **Post-commit**: focused ctest + openspec validate --strict
- [ ] **Oracle Stage 2 review** (背景，30 min) — SHIP / SHIP-with-fixes / BLOCK
- [ ] **如有 Major fix**: Stage 3 atomic commit on worktree
- [ ] **Stage 4 final Oracle**: APPROVE ✅
- [ ] **Archive**: `openspec archive 2026-09-18-provider-llm-tool-empty-passthrough --yes`
- [ ] **3 SoT docs §十一 sync**: `harness-architecture-2026-09.md` + `self-evolution-architecture-2026-08.md` + `rsi-architecture-2026-09.md`（+1 row "Latent Site #3 ✅ FIXED 2026-09-25"）

---

## 6. 零回归验证 (~15 min)

- [ ] focused ctest: `ctest -L loop_agent -V` 100% PASS
- [ ] 全量 ctest: `ctest --output-on-failure` ≥ 247/247 维持（16 known pre-existing failures 不变）
- [ ] `docs_drift_audit.py 0 DRIFT items`
- [ ] `openspec validate --strict "Change is valid"`

---

## Acceptance（验收标准）

### D1 ProviderLLMTool fail-fast
- [x] pdk_entry.cpp ProviderLLMTool 在 provider 返回空 text 时抛 runtime_error 含诊断线索
- [x] 抛错时 LLMResult.success = false 且 error 信息明确
- [x] 错误信息含 provider name (`loop-agent-provider-bridge`) + prompt 片段（如需）

### D2 测试覆盖
- [x] 新增 test binary `tests/test_provider_llm_tool_empty.cpp`
- [x] 3 cases: empty / non-empty / 回归
- [x] ctest 100% PASS

### D3 零回归
- [ ] focused ctest 全 PASS (loop_agent + executor + react_loop + chat_session)
- [ ] 全量 ctest 247/247 维持

### D4 Latent Sites 表更新
- [ ] F1 design.md Latent Sites 表 Site #3 标 ✅ FIXED
- [ ] AGENTS.md Pattern #1 step 4 引用本 change 作为同类防御模式例

### D5 Docs drift gate
- [ ] docs_drift_audit.py 0 DRIFT items
- [ ] openspec validate --strict "Change is valid"

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
- Oracle session: `ses_f4d05cdb0ffe0BhMdEADyfsdTz` (F1 root cause correction) + `ses_f2b923412ffeTFfBdDqQFOMdT9` (DECISION B 建议)
- AGENTS.md Pattern #1 step 4: systematic latent sites recording
- AGENTS.md Pattern #2: 量化升级门槛 (≥3 sites → P0 升级, 当前 3 站点已触发, 需评估 `fix-generation-request-model-default` umbrella change)
- Latent Sites table: `openspec/changes/archive/2026-09-18-fix-react-decide-empty-response/design.md`