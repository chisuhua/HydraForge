## 1. Root Cause Diagnosis

- [ ] 1.1 Add `std::cerr` debug print to `node_executor.cpp:147` (`llm_call` output_keys 写入处) showing `result["text"]` type and value
- [ ] 1.2 Add `std::cerr` debug print to `node_executor.cpp:230` (`tool_call` args 渲染处) showing rendered `{{llm_response}}` value and ctx presence
- [ ] 1.3 Reproduce demo with stdin pipe (`echo "..." | pdk_chat_demo`), capture full stderr output
- [ ] 1.4 Decide root cause from debug output: (a) data type mismatch, (b) ctx 传递断点, (c) inja silent 空字符串, (d) react.agent.md schema 不匹配
- [ ] 1.5 Document root cause in `design.md` Open Questions section, update D1/D2 if needed

## 2. Pre-Implementation Consultation

- [ ] 2.1 `task(subagent_type="oracle")` 咨询 D3 fork/join 节点 ctx 隔离策略（per design.md Open Questions Q2）
- [ ] 2.2 决策前置: fork_join.agent.md 当前 schema 是否需要调整（如需, 在本 change 范围 OR 新独立 change）
- [ ] 2.3 Apply Oracle 评审修正到 design.md Decisions

## 3. RED: Failing Test

- [ ] 3.1 在 `tests/test_dsl_engine_ctx_bridge.cpp` 新建独立 test binary (per `tests/AGENTS.md` §REAL-LLM TEST PATTERNS helper 三态分离)
- [ ] 3.2 Test Case 1: `llm_call` → next node ctx bridge (unit, mock LLM) — 验证 `{{llm_response}}` 渲染非空 string
- [ ] 3.3 Test Case 2: `react.agent.md` 端到端 mock mode — think→decide→act→observe→end 完整路径, Assistant 文本非空
- [ ] 3.4 Test Case 3: tool_call args 渲染空 template 校验 — 验证 D2 EmptyTemplateArgError 抛错
- [ ] 3.5 `ctest -R test_dsl_engine_ctx_bridge` 期望 Case 1-3 FAIL

## 4. GREEN: Fix Implementation

- [ ] 4.1 移除 1.1/1.2 debug prints
- [ ] 4.2 实施最小修复（基于 1.4 根因）:
  - 如果 D1 验证失败: 修 `node_executor.cpp:147` `new_context[key] = result["text"].get<std::string>()`
  - 如果 D2 需要: 加 `node_executor.cpp` EmptyTemplateArgError 校验层
  - 如果 react.agent.md schema 需要调整: 修 `lib/loop/react.agent.md` (new schema)
- [ ] 4.3 `ctest -R test_dsl_engine_ctx_bridge` 期望 Case 1-3 PASS
- [ ] 4.4 跑 core ctest `-E 'pkm_temporal_demo|test_scenarios'` 期望 100% PASS (零回归)

## 5. Real LLM End-to-End Tests (chat-real-llm-coverage Phase H)

- [ ] 5.1 在 `tests/test_react_loop_real_llm.cpp` 新建独立 test binary
- [ ] 5.2 Test Case R1: react loop "Hello" prompt + 真实 DeepSeek LLM — Assistant 非空, steps >= 1
- [ ] 5.3 Test Case R2: react loop 中文 prompt `用一句话解释 std::jthread` — Assistant 含 jthread 关键词
- [ ] 5.4 Test Case R3: react loop 多轮对话 (turn 1 + turn 2) — 第 2 turn 上下文感知
- [ ] 5.5 Test Case R4: plan_execute "研究量子计算" 端到端 — verify success 路径
- [ ] 5.6 Test Case R5: fork_join 3 分支并行 — synthesize 节点含 3 分支结果
- [ ] 5.7 复用 `tests/test_helpers/real_llm_env.h` helper + `[realllm]` tag + `require_real_llm_env()` 守卫
- [ ] 5.8 手动跑 `HYDRAFORGE_SKIP_REAL_LLM=0 DEEPSEEK_API_KEY=... ctest -R test_react_loop_real_llm --output-on-failure` 验证 PASS

## 6. Ship-with-Fixes Validation

- [ ] 6.1 Metis dual-agent review (mandatory per `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md §九.1`)
- [ ] 6.2 Oracle dual-agent review (mandatory per AGENTS.md 模式 #8)
- [ ] 6.3 Apply all Critical/Major fixes per dual-agent review
- [ ] 6.4 跑全量 ctest `-E 'pkm_temporal_demo|test_scenarios'` 期望 100% PASS
- [ ] 6.5 跑 real LLM demo 端到端验证 (含 mock fallback path 不破坏)
- [ ] 6.6 跑 `tools/adr_lint.py` 期望 0 errors
- [ ] 6.7 跑 `tools/docs_drift_audit.py` 期望 0 DRIFT

## 7. Archive & Sync

- [ ] 7.1 `openspec archive fix-react-decide-empty-response` 移动到 archive 目录
- [ ] 7.2 更新 `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md`:
  - [ ] §十 Drift Log 新增行 (root cause + fix commit + Oracle session 引用)
  - [ ] §一.4 Bug 3 残量风险移除 (此 bug 是 ship-with-known-issue 部分, 修复后 ✅ FIXED)
- [ ] 7.3 更新 `docs/active-status.md` (若 archive 状态变化)
- [ ] 7.4 更新 `chat-real-llm-coverage` OpenSpec change (if any): 标记 Phase H 完成