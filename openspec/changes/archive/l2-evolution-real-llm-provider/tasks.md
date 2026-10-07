# l2-evolution-real-llm-provider Tasks

## 1. Setup + Pre-Impl Review

- [ ] 1.1 主会话确认 change scaffold (`openspec/changes/l2-evolution-real-llm-provider/`) 4 文件存在 (proposal.md + specs/ + design.md + tasks.md)
- [ ] 1.2 触发 24h cooling-off (从 change 创建时间 `2026-10-06T00:21Z` 起算, EXPIRES `2026-10-07T00:21Z`)
- [ ] 1.3 cooling-off 期间: 派 Metis pre-impl review (识别歧义点 / AI 失败模式 / spec 多义解读)
- [ ] 1.4 cooling-off 期间: 派 Oracle pre-impl review (架构 + 实施可行性 + 测试设计物理可行性)
- [ ] 1.5 收集 dual-agent review 反馈 + 应用 Critical/Major 修正到 4 artifacts
- [ ] 1.6 cooling-off 满 + dual-review 闭环 → 启动实施阶段

## 2. EvolutionSession Real LLM 接线

- [ ] 2.1 `examples/pdk_chat_demo_evolution/src/evolution_session.cpp` ctor: 添加 `real_provider_` 成员 (`unique_ptr<agenticdsl::ILLMProvider>`)
- [ ] 2.2 ctor 接受 `real_llm` provider name → 调 `agenticdsl::LLMProviderFactory::create(real_llm, api_key_env)` 注入
- [ ] 2.3 api_key_env 解析: `real_llm == "deepseek"` → `"DEEPSEEK_API_KEY"`, `real_llm == "minimax"` → `"MINIMAX_API_KEY"`, unknown → throw runtime_error
- [ ] 2.4 fail-fast on 4 类配置错误 (per design D5): unknown provider / missing key / placeholder URL / empty value
- [ ] 2.5 mock_provider_ 与 real_provider_ 双轨: real_provider_ 存在时优先使用 (per design D1)
- [ ] 2.6 `real_llm_provider()` 公开 getter 给 test binary 验证 provider 注入成功

## 3. main.cpp Real LLM Fail-Fast

- [ ] 3.1 `examples/pdk_chat_demo_evolution/main.cpp` parse 阶段: `--real-llm` 必须有 value (current 是可选,改为必填 value 否则 parse_error)
- [ ] 3.2 EvolutionSession 构造后: 立即验证 `session.real_llm_provider() != nullptr` 否则 exit non-zero + stderr "ERROR: LLM provider construction failed"
- [ ] 3.3 验证 `DEEPSEEK_API_KEY` / `MINIMAX_API_KEY` env var 存在 (real_llm 模式必填) 否则 exit non-zero + stderr "ERROR: real LLM requires DEEPSEEK_API_KEY or MINIMAX_API_KEY"
- [ ] 3.4 placeholder URL 拦截: `MINIMAX_API_URL == "https://api.minimax.chat"` → exit non-zero + stderr "ERROR: MINIMAX_API_URL placeholder, see AGENTS.md §G2"

## 4. Test Binary 实施

- [ ] 4.1 新增 `examples/pdk_chat_demo_evolution/tests/test_l2_real_llm_e2e.cpp` (~150 行)
- [ ] 4.2 Case 1 (must_realllm + l2-evolution): `--real-llm deepseek` 6-phase demo + trace JSONL 验证 (real response 字段非空 + tokens > 0 + cost_usd > 0)
- [ ] 4.3 Case 2 (must_realllm + l2-evolution): `--real-llm deepseek --release-metrics` drop_ratio 真实计算 + metrics.json 验证 (含 `drop_ratio` 字段非 -1.0)
- [ ] 4.4 Case 3 (l2-evolution only, no LLM): `--real-llm <unknown>` exit non-zero + stderr "ERROR: unknown provider"
- [ ] 4.5 Case 4 (l2-evolution only, no LLM): 缺 `DEEPSEEK_API_KEY` + 未设 `HYDRAFORGE_SKIP_REAL_LLM` → exit non-zero
- [ ] 4.6 Case 5 (must_realllm + l2-evolution): hermetic HOME + real LLM 双轨兼容 (`setup_hermetic_home()` + `--real-llm deepseek` 无冲突)
- [ ] 4.7 Catch2 tags: 所有 case 加 `[must_realllm]` + `[l2-evolution]` 双 tag
- [ ] 4.8 复用 `tests/test_helpers/real_llm_env.h::require_real_llm_env()` + `real_llm_env_skipped()` + `real_llm_config()` pattern

## 5. CMakeLists 注册

- [ ] 5.1 `examples/pdk_chat_demo_evolution/tests/CMakeLists.txt` 新增 `add_catch_test(test_l2_real_llm_e2e ...)` 注册
- [ ] 5.2 链接 `pdk_chat_session` + `agenticdsl_common` (含 LLMProviderFactory) + `pdk_chat_demo_evolution_lib`
- [ ] 5.3 设 `LABELS "must_realllm;l2-evolution"` 双 CTest label (per AGENTS.md §FULL REGRESSION TEST FLOW §6)
- [ ] 5.4 用 `set_tests_properties(... PROPERTIES LABELS ...)` 在 `add_test` 之后注册 (per `a21c92e` precedent)

## 6. SHIP-with-Fixes Cycle

- [ ] 6.1 主会话 commit 1: feat(evolution-session): real LLM provider construction (EvolutionSession ctor + main.cpp fail-fast)
- [ ] 6.2 主会话 commit 2: test(l2-evolution): real LLM e2e + drop_ratio R8.1 red-line (test_l2_real_llm_e2e.cpp + CMakeLists)
- [ ] 6.3 主会话 commit 3: docs(AGENTS.md): Recent Changes entry (per Pattern #4 atomicity)
- [ ] 6.4 派 Oracle post-impl SHIP-with-fixes review (background, ~30 min)
- [ ] 6.5 应用 Oracle Major 修正 → 新 atomic commit (per Pattern #4, NOT amend)
- [ ] 6.6 验证 ship gates:
  - 9/9 l2-evolution mock 测试零回归
  - 14/14 must_realllm 二进制 (含新增 test_l2_real_llm_e2e) 全 PASS
  - `ctest -L must_realllm` 10/10 + 4/4 examples PASS
  - drop_ratio 在 real LLM 模式下计算正确 (含 R8.1 红线触发 case)
  - adr_lint 0 errors
  - docs_drift_audit 0 new drift

## 7. Archive + Cleanup

- [ ] 7.1 `openspec archive l2-evolution-real-llm-provider` (per AGENTS.md Day 5 4-file integrity, 用 `git mv` 验证完整性)
- [ ] 7.2 更新 `docs/architecture/self-evolution-architecture-2026-08.md` §十一 ship row (L2 真实 LLM 闭环 ✅)
- [ ] 7.3 更新 `docs/architecture/harness-architecture-2026-09.md` §十二 H6 (4-tier gate 反向校验 ✅ real LLM 验证)
- [ ] 7.4 更新 `docs/architecture/rsi-architecture-2026-09.md` §十二 R3 元指标实证 (real LLM ≥3 类 ContextRequest ✅)
- [ ] 7.5 git status --short clean
- [ ] 7.6 更新 `.rddf/improvements/l2-evolution-real-llm-provider.md` 状态: pending → shipped (2026-10-XX)

## 8. Follow-up (后续 improvement)

- [ ] 8.1 启动 G7 follow-up: `l2-release-metrics-real-llm-drop-ratio` (依赖 G1 ship, 1-2 天)
- [ ] 8.2 启动 G8 follow-up: `self-evolution-9-stage-real-llm-end-to-end` (依赖 G1 + G4 + G7, 3-5 天)
- [ ] 8.3 G2 (MiniMax URL placeholder) 等 URL 修对后再补
- [ ] 8.4 G4 (Harness mutation commit 真实 LLM) 可并行立项,与 G1 不冲突
