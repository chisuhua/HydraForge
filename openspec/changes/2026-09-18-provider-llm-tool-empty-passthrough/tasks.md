# Tasks: Provider-LLM-Tool Empty Text Pass-Through Guard

> **STATUS: PLACEHOLDER** — depends on F1 ship confirmation + Oracle review

## 1. Pre-flight (~15 min)
- [ ] TBD: 读 pdk_entry.cpp:402/434/524 ProviderLLMTool 实现细节
- [ ] TBD: Oracle 评审 design（与 F1 main fix 的一致性 + 错误信息格式）
- [ ] TBD: 决定 throw 类型（runtime_error vs LLMError — per F1 复用 runtime_error 决策）

## 2. RED: Failing Tests (~30 min)
- [ ] TBD: 新建 tests/test_provider_llm_tool_empty.cpp 或扩展 tests/test_loop_agent_plugin.cpp
- [ ] TBD: 3 cases 测试 empty / non-empty / 回归
- [ ] TBD: 验证 FAIL（empty 路径未防护 → 测试 FAIL）

## 3. GREEN: Minimal Fix (~15 min)
- [ ] TBD: pdk_entry.cpp ProviderLLMTool 加 fail-fast 空校验 (~5 行)
- [ ] TBD: 验证 FAIL 测试现在 PASS
- [ ] TBD: 跑 focused ctest 100% PASS

## 4. Latent Sites 表更新 (~10 min)
- [ ] TBD: F1 design.md Site #3 标 ✅ FIXED（含 commit hash + Oracle session 引用）
- [ ] TBD: AGENTS.md Pattern #1 step 4 加引用本 change 作为同类防御模式例

## 5. Ship & Archive (~15 min)
- [ ] TBD: docs_drift_audit.py 0 DRIFT items
- [ ] TBD: openspec validate --strict "Change is valid"
- [ ] TBD: git atomic commit + archive 2026-09-18-provider-llm-tool-empty-passthrough