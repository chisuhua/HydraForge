# l2-release-metrics-real-llm-drop-ratio

> **来源**: 2026-10-06 全量回归测试完备性审查 (`AGENTS.md §Recent Changes` entry 自识别盲点 G7)
> **生成**: 2026-10-06 via completeness-audit (partial resolution: 2026-10-07 per Oracle SHIP-with-fixes verdict)
> **AGENTS.md Pattern**: #10 hygiene (systematic recording of deferred work)

**优先级**: 🟠 P1 | **阶段**: 自由 (phase-n/a) | **分类**: 真实 LLM 验证盲点
**类型**: debt-tracking | **主题**: L2 reference example R8.1 drop_ratio 红线真实 LLM 计算
**状态**: pending (mock 覆盖 ship 2026-09-26, partial resolution 2026-10-07, 红线触发验证仍 deferred)
**关联**:
- `rsi-architecture-2026-09.md` §十二 R8 反向指标门 + R8.1 红线
- `L2 spec §R8 反向指标门`
- `openspec/changes/archive/2026-09-26-l2-evolution-real-execution-chain/` (`d42b47b` --release-metrics ship)
- `openspec/changes/archive/2026-09-26-l2-evolution-finalization/` (`--release-metrics` 实施)
- `openspec/changes/archive/l2-evolution-real-llm-provider/` (`7fc6d46` real LLM provider 接线 + drop_ratio 计算)

## 2026-10-07 partial resolution (per Oracle SHIP-with-fixes verdict `ses_eebfb88baffeFJaY5Z9S3Q1UtT`)

`l2-evolution-real-llm-provider` (commit `7fc6d46`) 实现 Case 2 部分覆盖:

✅ **已 ship**:
- 真实 LLM 模式下 `--release-metrics` 计算真实 `drop_ratio` (per finalization C4 公式 `mutated_failures / max(1, mutated_passes + mutated_failures)`)
- `metrics.json` 包含 `drop_ratio` 字段 (非 `-1.0` mock 哨兵) + `context_ids` 列表 (R8.1 红线 traceability)
- `real-drop-ratio-computed-from-baseline-vs-mutated` spec scenario 验证

❌ **仍 deferred** (本 ship 未覆盖):
- **R8.1 红线触发 (`drop_ratio > 0.05 → exit non-zero`) 真实 LLM 验证**: L2 实际 fixture 只有 1-2 contexts, drop_ratio 粒度 0%/50%/100%, 无法构造稳定 5% 阈值附近场景 (spec scenario `real-drop-ratio-gt-5-percent-exit-nonzero` 物理不可行).
- spec scenario `real-drop-ratio-gt-5-percent-exit-nonzero` **零测试覆盖** (Case 2 不断言 `rc==0` 以回避红线分支)

## Red line 触发验证 strategy (deferred 实施建议)

需独立 OpenSpec change:
- 构造 ≥20 mutations 的 fixture (mock mode 可行, real LLM mode 非可行)
- 在 mock mode 注入高失败率 fixture 验证 R8.1 exit-non-zero 触发 (semantic 不算 "real LLM 验证" 但 R8.1 红线机制需 unit test 覆盖)
- 或: 修改 drop_ratio 公式语义 (例如 R8.1 阈值从 `> 5%` 改为 `> N/N_abs(failures)` 按上下文数量缩放), 在小 fixture 下也能触发
- 或: 接受 R8.1 红线仅在 CI/long-run 实测下自然触发 (累积 drop_ratio), 不在 unit test 覆盖

## 当前 ship gate 状态 (2026-10-07)

- `ctest -L l2-evolution -j$(nproc)` 10/10 PASS (含 test_l2_real_llm_e2e Case 2)
- `test_l2_real_llm_e2e` Case 2: 真实 LLM drop_ratio 计算 + context_ids traceability ✅
- R8.1 红线触发 unit test ❌ (deferred)

## Trigger conditions (立即立项条件)

任何下列条件满足即升级 P0 → 立即立项:
1. 用户要求 "R8.1 红线真实 LLM 端到端" 验证
2. Wave 3 Phase 2 / Wave 4 启动需要真实 drop_ratio 数据
3. 任何 ship gate 因 drop_ratio 红线触发

## Status: 🟡 PARTIAL-SHIPPED (2026-10-07, Case 2 ship + R8.1 红线触发 deferred)

**Status 澄清 (2026-10-15 audit verification, per Pattern #10 hygiene)**:

本提案是 **partial-resolution 状态**（非 Pattern #10 drift）— Case 2 部分已 ship + 仍 deferred 部分由 3 trigger conditions 显式定义.

### ✅ Case 2 已 ship (2026-10-07 per commit `7fc6d46`)

**OpenSpec archive**: `openspec/changes/archive/l2-evolution-real-llm-provider/` (5 files tracked per commit `3ebb6a9` Day-5 4-file integrity)

**代码 ship commits**:
- `7fc6d46` (2026-10-07) feat(l2): wire --real-llm provider into EvolutionSession + fail-fast (Sprint 36+)
- `ba6edcf` (2026-10-07) chore(hygiene): Oracle post-impl SHIP-with-fixes closure (Major 1+2 + Minor 4)
- `3ebb6a9` (2026-10-07) chore(openspec): archive l2-evolution-real-llm-provider (Day-5 4-file integrity verified)
- `81ac871` (post-L2-real-LLM execution plan + blind-tracking governance)

**代码实证** (commit `7fc6d46`):
- `examples/pdk_chat_demo_evolution/evolution_session.cpp` — `construct_real_provider()` + `real_llm_error_` member + `real_llm_provider()` getter
- `examples/pdk_chat_demo_evolution/evolution_session.h` — `real_provider_` member 公开
- `examples/pdk_chat_demo_evolution/main.cpp` — post-construction fail-fast + metrics.json context_ids + R8.1 红线机制 (line 60-64 `if (drop_ratio > 0.05) return 1`)
- `examples/pdk_chat_demo_evolution/tests/test_l2_real_llm_e2e.cpp` — Case 2 验证 `--release-metrics` 真实 LLM 写入 drop_ratio + context_ids
- `examples/pdk_chat_demo_evolution/tests/CMakeLists.txt` — test_l2_real_llm_e2e binary, LABELS "must_realllm;l2-evolution", TIMEOUT 300

**Test 验证** (commit `7fc6d46` ship gate):
- `ctest -L l2-evolution -j$(nproc)` 10/10 PASS (含 test_l2_real_llm_e2e Case 2)
- `test_l2_real_llm_e2e` Case 2: 真实 LLM drop_ratio 计算 + context_ids traceability ✅
- must_realllm binary 重编成功 (编译零 error)
- drop_ratio = 0% baseline 零回归

### ❌ R8.1 红线触发验证 (deferred, 物理不可行)

**物理约束**:
- L2 实际 fixture 只有 1-2 contexts (per `tests/fixtures/context_request/` 11 个 fixture, 最多 3 lines)
- drop_ratio 粒度 0% / 50% / 100% (1-2 contexts 二值)
- 5% 阈值附近无法稳定构造场景

**Spec scenario 缺口**:
- `real-drop-ratio-gt-5-percent-exit-nonzero` **零测试覆盖** (Case 2 不断言 `rc==0` 以回避红线分支)

**Strategy 选项** (per proposal line 32-36, 3 paths):
- (a) 构造 ≥20 mutations fixture (mock mode 可行, real LLM mode 非可行)
- (b) 改 drop_ratio 公式语义 (阈值从 `> 5%` 改为 `> N/N_abs(failures)` 按上下文数量缩放)
- (c) 接受 R8.1 红线仅在 CI/long-run 实测下自然触发 (累积 drop_ratio), 不在 unit test 覆盖

**关联 descope** (per 提案 4 spec R5):
- `harness-mutation-commit-real-llm` (commit `0477ead` 2026-10-15) spec R5 显式 descope Case 4 drop_ratio
- R5.reason: "drop_ratio 红线 5% 触发在 L2 1-2 contexts 粒度物理不可行 (per 2026-10-06 l2-evolution-real-llm-provider C4 deferred ack), 需 ≥20 mutations 才能稳定触发, 红线验证 MUST 属独立 OpenSpec change"

### Pattern 沉淀 (Pattern #10 hygiene 第 18 实例)

本提案 **不是** 典型 Pattern #10 drift (提案 self-declared 状态正确 + partial ship evidence 完整 + 3 trigger conditions 显式定义 + Case 2 ship commits 完整). 但**提案文件无显式 Status 段**, 易被未来 audit 误判为 "R8.1 红线 0 覆盖 = 完全 OPEN". 本 Status 段补全后, 防未来 audit 误判.

**与 R9/R13.4 误标对比**:
- R9 (rsi-anti-cheat-real-llm): parser-side 拦截 + LLM 不在回路 → must_realllm 误标 (强变体) → 关闭 Path 1
- R13.4 (r13-4-confidential-redaction-real-llm): LLM 在回路但断言 LLM-independent → must_realllm 误标 (弱变体) → 关闭 Path 1
- **本提案**: 真实 LLM drop_ratio 已 ship (Case 2 PASS) + R8.1 红线触发物理不可行 → **不是 must_realllm 误标** → hygiene 修正 Path A (推荐)
