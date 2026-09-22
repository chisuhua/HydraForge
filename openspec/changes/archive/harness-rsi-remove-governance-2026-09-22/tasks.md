# Harness-RSI Remove Governance — Tasks

## 1. Setup / 准备
- [ ] 1.1 读取 C4 `harness_rsi.h` / `harness_rsi.cpp` + `secure_tool_registry.cpp` + `registry.h/.cpp` 确认当前实现面
- [ ] 1.2 确认 `MutationGateContext` 全部构造点 (test_harness_rsi_pilot.cpp + 未来调用方) — trace_id 字段 BREAKING 影响面

## 2. RED — 测试先行
- [ ] 2.1 `tests/test_harness_rsi_pilot.cpp` 新增 Case 5a: remove denied tool → GovernanceDenied + 零状态变更
- [ ] 2.2 `tests/test_harness_rsi_pilot.cpp` 新增 Case 5b: remove trusted tool → success + registry unregister
- [ ] 2.3 `tests/test_harness_rsi_pilot.cpp` 新增 Case 5c: add denied + remove denied 混合 → GovernanceDenied + 零状态变更
- [ ] 2.4 `tests/test_harness_rsi_pilot.cpp` 新增 Case 6: trace_id 透传断言 (meta.trace_id == ctx.trace_id)
- [ ] 2.5 `tests/test_tool_registry*.cpp` 新增 SecureToolRegistry unregister disabled tool 拒绝测试
- [ ] 2.6 运行 RED: 确认新测试 FAIL (功能未实现)

## 3. GREEN — 实现
- [ ] 3.1 `harness_rsi.h`: `MutationGateContext` 增加 `std::string trace_id` 字段 (默认空)
- [ ] 3.2 `harness_rsi.cpp` Gate 2: 扩展循环覆盖 `tools_remove` (与 tools_add 对称)
- [ ] 3.3 `harness_rsi.cpp`: `event.meta` 的 trace_id 从 `ctx.trace_id` 取 (替代 `""`)
- [ ] 3.4 `secure_tool_registry.cpp`: `unregister_tool_function` 加 `is_disabled` 检查 (disabled 静默拒绝)
- [ ] 3.5 `registry.h` + `registry.cpp`: `register_tool_function` / `unregister_tool_function` 加 `mutation_mutex_` 写-写互斥 + 文档化读路径约束
- [ ] 3.6 同步更新 test_harness_rsi_pilot 全部 `MutationGateContext` 构造点 (trace_id 字段)
- [ ] 3.7 运行 GREEN: 全部新测试 + 既有 9 cases / 43 assertions PASS

## 4. REFACTOR / 收尾
- [ ] 4.1 检查 Gate 2 扩展后零状态变更不变量仍成立 (Gate 2.5 partial-apply 预检未被破坏)
- [ ] 4.2 注释更新: harness_rsi.cpp 头注释 + registry.h 类注释 (mutation mutex 约束)
- [ ] 4.3 `lsp_diagnostics` 全部改动文件零错误

## 5. VERIFY — 验证
- [ ] 5.1 focused ctest: `test_harness_rsi_pilot` + `test_tool_registry` 三件套 100% PASS
- [ ] 5.2 全量 `ctest -N` 计数确认 (预期 252, 无新增 binary 或 +1 若独立 test)
- [ ] 5.3 回归确认: 8 相关测试 100% PASS, 0 新回归 (4 项 pre-existing failures 不变)
- [ ] 5.4 `openspec validate harness-rsi-remove-governance --strict` → "Change is valid"

## 6. DOCS / 治理
- [ ] 6.1 Decision Record 更新: C4 go-no-go.md §3 摩擦 2 标注 "(a) remove 治理 ✅ resolved by harness-rsi-remove-governance"
- [ ] 6.2 Roadmap: Pre-Wave3 收口门禁 checklist 项 (1) 标 ✅
- [ ] 6.3 `docs/active-status.md` + master plan 同步 (change ship + archive)
- [ ] 6.4 archive change (4 文件完整 per AGENTS.md Day 5 lesson)
