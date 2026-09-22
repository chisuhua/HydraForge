# harness-rsi-remove-governance

> **来源**: Pre-Wave3 Plan §2 G1 + openspec/changes/harness-rsi-remove-governance/{proposal,design,tasks,specs}/
> **生成**: 2026-09-21 via rdd-builder P0 pre-work（从 OpenSpec artifacts 提取 5 段 summary）
> **Dual-reviewed pre-impl**: Oracle `bg_c706862b` (5 SHIP-with-fixes) + Metis `bg_d9744d91` (3 BLOCK→修正), `a4f374b` 修正 commit 已 ship

## Why

C4 `harness-rsi-pilot` ship 后, Oracle 独立审查 (`bg_afa84d4d`) 确认 3 个债务:
1. **`tools_remove` 零治理** (harness_rsi.cpp:171-177): Gate 2 仅循环 `tools_add`, `tools_remove` 路径直接调 `registry.unregister_tool_function()` 无 `is_tool_allowed` 拦截
2. **SecureToolRegistry 裸委托** (secure_tool_registry.cpp:266-268): `unregister_tool_function` 直接委托 wrapped registry 无 `check_security`
3. **ToolRegistry 无 mutex** (registry.h:87-88): 全类无锁, unregister 引入运行期全局变异面

外加 `trace_id: ""` 硬编码 (harness_rsi.cpp:121) 切断因果链 (模式 #7 教训).

**Wave 3 解锁前置**: 若不修, Wave 3 (ADR-0078 Model-RSI pilot) 把未治理的 remove 当作已验证模式复制.

## What Changes

- **Gate 2 扩展 tools_remove**: `apply_harness_mutation` Gate 2 从仅循环 `tools_add` 扩展为 `tools_add + tools_remove` 对称检查,任一命中 `policy.denied_tools` → `MutationError::GovernanceDenied` + 零状态变更
- **SecureToolRegistry unregister 安全检查**: `unregister_tool_function` 委托前 `is_disabled(name)` 检查, disabled 工具**静默忽略** (void 签名无返回值, 语义锁定)
- **ToolRegistry mutation mutex**: `register_tool_function` / `unregister_tool_function` / `register_llm_tool` 三写路径加 `std::mutex mutation_mutex_` (1 把锁, 写写互斥), 读路径不加锁 + 文档化约束
- **trace_id 透传**: `MutationGateContext` 追加 `std::string trace_id` 字段 (末尾追加, 默认空, **非 BREAKING** vs design 原 BREAKING 定性过度, 已修正), `harness_rsi.cpp` 发射事件时从 ctx 取真实 trace_id

**Non-Goals**: 不改 `MutationGovernancePolicy` 结构 / 不做 per-mutation scope registry (Wave 3+) / 不引入 shared_mutex / 不修复 `eval_quality:"Unknown"` (独立 change evolution-verdict-reward-quality = G2)

## Acceptance

- [ ] AC-1: 新增测试 case `remove denied tool → GovernanceDenied + 零状态变更` PASS (system_prompt 未变 + registry 未 unregister)
- [ ] AC-2: 新增测试 case `remove trusted tool → success + registry unregister` PASS
- [ ] AC-3: 新增测试 case `add denied + remove denied 混合双拒绝 → GovernanceDenied + 零状态变更` PASS
- [ ] AC-4: 新增测试 case `trace_id 透传断言 (event.meta.trace_id == ctx.trace_id)` PASS
- [ ] AC-5: `test_tool_registry*` 新增 SecureToolRegistry unregister disabled tool 拒绝 PASS
- [ ] AC-6: `MutationDecision` 链路 (设计 D1-D4) 既有 9 cases / 43 assertions **零回归**
- [ ] AC-7: `ctest -R "test_harness_rsi_pilot|test_tool_registry|test_secure_tool_registry" --output-on-failure` **100% PASS**
- [ ] AC-8: `ctest -N` 计数 = expected (无新增 binary, =当前 baseline 209)
- [ ] AC-9: `openspec validate harness-rsi-remove-governance --strict` → "Change is valid"
- [ ] AC-10: 1 atomic commit per AGENTS.md 模式 #4 + Oracle post-impl `SHIP-with-fixes` 复评通过
- [ ] AC-11: archive 时 `git ls-files openspec/changes/archive/harness-rsi-remove-governance/` 验证 4 文件全在 (防 AGENTS.md Day-5 lesson 陷阱)
- [ ] AC-12: AGENTS.md Recent Changes + ADR-0068 Appendix A 同步追加

## Capabilities

### MUST DO (执行红线)
- 工作目录: `.rddf/wt/harness-rsi-remove-governance/` worktree (per `.rddf/wt/` 已 ignore)
- TDD 5 步: 先写 failing test → 验证 fail → 实施最小代码 → 验证 pass → defer commit
- 仅 1 atomic commit per AGENTS.md 模式 #4 (`fix(evolution): G1 harness-rsi-remove-governance`)
- 同步更新 `MutationGateContext` 全部构造点 (test_harness_rsi_pilot.cpp + c4 users)
- `lsp_diagnostics` 全部改动文件零错误
- 实施完成后必派 Oracle post-impl `SHIP-with-fixes` 复评 (per user 决策)
- Day-5 trap 防御: archive 时 `git ls-files` 4 文件验证

### MUST NOT DO
- ❌ 不要碰 C4 已 archive `harness-rsi-pilot` spec (design §Migration Plan 1)
- ❌ 不要改 `MutationGovernancePolicy` 结构 (denied_tools 语义不变)
- ❌ 不要做 per-mutation scope registry (Wave 3+ 评估)
- ❌ 不要引入 shared_mutex (全类加锁超出 pilot 需求)
- ❌ 不要 fix `eval_quality:"Unknown"` (独立 change = G2)
- ❌ 不要 amend 上次 commit (Atomic commits 不能 amend, 每 commit 必须独立可回溯)
- ❌ 不要在 main 直接修改 (违反 worktree-discipline)
- ❌ 不要跳 24h cooling-off + Oracle SHIP-with-fixes 复评

## Impact

**Production files (5)**:
- `include/agenticdsl/evolution/harness_rsi.h` — `MutationGateContext` + trace_id 字段 (末尾追加)
- `src/evolution/harness_rsi.cpp` — Gate 2 扩展 + trace_id 发射
- `src/common/tools/secure_tool_registry.cpp` — unregister 安全检查 (D2 决策)
- `src/common/tools/registry.h` — mutation_mutex_ + 文档化
- `src/common/tools/registry.cpp` — 3 写路径锁保护 (D3 决策)

**Test files (2)**:
- `tests/test_harness_rsi_pilot.cpp` — 新增 remove-governance cases (AC-1~AC-4)
- `tests/test_tool_registry*.cpp` — SecureToolRegistry unregister 安全测试 (AC-5)

**依赖变更**: 0 外部依赖, 0 PDK contract 变更 (`trace_id` 是 `MutationGateContext` 内部字段)
**总估时**: 1-2 天 (per plan §2 G1)
**推荐路线**: complex (per `.rddf/state/.planner-handoff.json::recommended_route=complex` + G1 cross-module impact)
**Execution mode**: worktree (per user 决策 + BREAKING API change + gate-mutex 引入 + cross-process 不确定性)
