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
