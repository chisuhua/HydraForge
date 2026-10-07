# l2-evolution-real-llm-provider

> **来源**: 2026-10-06 全量回归测试完备性审查 (`AGENTS.md §Recent Changes` entry 自识别盲点 G1)
> **生成**: 2026-10-06 via completeness-audit
> **AGENTS.md Pattern**: #10 hygiene (systematic recording of deferred work)

**优先级**: ✅ shipped (commit 7fc6d46, 2026-10-07) | **阶段**: Phase B + provider 接线 | **分类**: 真实 LLM 验证盲点
**类型**: debt-tracking | **主题**: L2 reference example `--real-llm` 模式真实 LLM provider 接线
**状态**: ✅ shipped (per Oracle SHIP-with-fixes verdict `ses_eebfb88baffeFJaY5Z9S3Q1UtT` 2026-10-07)
**关联**:
- `openspec/changes/archive/2026-09-26-l2-evolution-finalization/` (Phase B only ship)
- `openspec/changes/archive/2026-09-26-l2-evolution-real-execution-chain/`
- `examples/pdk_chat_demo_evolution/main.cpp:37-132` (`--real-llm` 标志存在但实施 stub)
- `examples/pdk_chat_demo_evolution/evolution_session.cpp` (实施后含 real_provider_ 接线)

## Ship scope 收敛 (Oracle post-impl review verdict)

本 change 仅覆盖:
1. **Provider 接线**: `EvolutionSession` ctor 接受 `real_llm` provider 名 → `LLMProviderFactory::create(LLMConfig)` 注入 `real_provider_` (deepseek/minimax)
2. **D5 fail-fast**: 4 类配置错误 (unknown provider / missing key / placeholder URL / `--real-llm` 无 value) → stderr 报错 + exit non-zero (RAII-friendly)
3. **Phase B 真实 drop_ratio 验证**: Case 2 验证 `--release-metrics` 在 real LLM 下计算 drop_ratio (非 -1.0 mock 哨兵) + `context_ids` traceability
4. **Hermetic HOME + real LLM 双轨兼容**: Case 5 验证不冲突

**R8.1 红线触发 (>5% → exit non-zero) 仍 deferred** — 见 `l2-release-metrics-real-llm-drop-ratio.md` partial resolution note。L2 1-2 contexts 粒度物理不可行 (粒度 0%/50%/100%,无法构造 5% 阈值附近), 需 ≥20 mutations 才能稳定触发, 属独立 OpenSpec change。

## 已 ship

- ✅ `examples/pdk_chat_demo_evolution/evolution_session.cpp` (`construct_real_provider()` + `real_llm_error_` member + `real_llm_provider()` getter)
- ✅ `examples/pdk_chat_demo_evolution/evolution_session.h` (real_provider_ member 公开)
- ✅ `examples/pdk_chat_demo_evolution/main.cpp` (post-construction fail-fast + metrics.json context_ids)
- ✅ `examples/pdk_chat_demo_evolution/tests/test_l2_real_llm_e2e.cpp` (5 cases / 21 assertions)
- ✅ `examples/pdk_chat_demo_evolution/tests/CMakeLists.txt` (test_l2_real_llm_e2e binary, LABELS "must_realllm;l2-evolution", TIMEOUT 300)

## Pre-existing 限制 (与本 change 无关)

- `pdk/llama_engine` 全量 build 需 `external/AgenticLlama` symlink 指向 `/workspace/main/AgenticLlama` (主 working tree 之前 symlink 失效, 现已修复). Pre-existing, 2026-10-07 修复.

## 已知 follow-up (deferred to follow-up OpenSpec changes)

- R8.1 红线触发 (>5% exit-non-zero) — 见 `l2-release-metrics-real-llm-drop-ratio.md`
- 真实端到端 9 段闭环 — 见 `self-evolution-9-stage-real-llm-end-to-end.md`
- Harness mutation commit 真实 LLM — 见 `harness-mutation-commit-real-llm.md`
