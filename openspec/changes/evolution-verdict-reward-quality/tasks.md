# EvolutionVerdict Reward Quality — Tasks

## 1. Setup / 准备
- [ ] 1.1 读取 `transition_guard.h` EvolutionVerdict + evaluate_readiness 当前实现 + `reward_signal.h` Quality enum 值域
- [ ] 1.2 确认 `EvolutionVerdict` 全部构造点 (test_transition_guard.cpp + harness_rsi.cpp + 未来调用方) — 默认值兜底影响面

## 2. RED — 测试先行
- [ ] 2.1 `tests/test_transition_guard.cpp` 新增/更新: EvolutionVerdict 字段断言 (reward_quality 默认 Unknown + evaluate 填充 Good/Poor)
- [ ] 2.2 `tests/test_harness_rsi_pilot.cpp` 更新 Case 2: eval_quality 断言从 "Unknown" 改为真实值 (Good/Poor 两路径)
- [ ] 2.3 运行 RED: 确认新测试 FAIL (reward_quality 未实现)

## 3. GREEN — 实现
- [ ] 3.1 `transition_guard.h`: `EvolutionVerdict` 增加 `agenticdsl::RewardSignal::Quality reward_quality = Quality::Unknown;` 字段
- [ ] 3.2 `transition_guard.h`: `evaluate_readiness()` 在 `auto reward = evaluator.evaluate(...)` 后填充 `verdict.reward_quality = reward.quality;`
- [ ] 3.3 `harness_rsi.cpp`: 新增 `quality_name()` helper (Quality enum → 字符串, 与 attribution_verdict_name 同模式)
- [ ] 3.4 `harness_rsi.cpp` L117: `{"eval_quality", "Unknown"}` → `{"eval_quality", quality_name(verdict.reward_quality)}`
- [ ] 3.5 运行 GREEN: 全部新测试 + 既有测试 PASS

## 4. REFACTOR / 收尾
- [ ] 4.1 检查既有 test_transition_guard 13 cases / 47 assertions 零回归
- [ ] 4.2 注释更新: transition_guard.h D2 决策注释 + harness_rsi.cpp 头注释 (摩擦 1 resolved)
- [ ] 4.3 `lsp_diagnostics` 全部改动文件零错误

## 5. VERIFY — 验证
- [ ] 5.1 focused ctest: `test_transition_guard` + `test_harness_rsi_pilot` 100% PASS
- [ ] 5.2 回归确认: 8 相关测试 100% PASS, 0 新回归 (4 项 pre-existing failures 不变)
- [ ] 5.3 `openspec validate evolution-verdict-reward-quality --strict` → "Change is valid"

## 6. DOCS / 治理
- [ ] 6.1 Decision Record 更新: C4 go-no-go.md §3 摩擦 1 标注 "✅ resolved by evolution-verdict-reward-quality"
- [ ] 6.2 Roadmap: Pre-Wave3 收口门禁 checklist 项 (2) 标 ✅
- [ ] 6.3 `docs/active-status.md` + master plan 同步 (change ship + archive)
- [ ] 6.4 archive change (4 文件完整 per AGENTS.md Day 5 lesson)
