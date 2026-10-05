## 1. Pre-Implementation Dual-Agent Review (Pattern #8)

- [x] 1.1 Oracle session `ses_ef4386f3dffeiO3OvtOOCogRYy` 已审查 root cause (parser bug) + fix 方案
- [x] 1.2 Metis session `ses_ef433ad8bffewxBioiEktPFcFM` 已审查 spec/process/scope (推荐 Option 2)
- [x] 1.3 Oracle + Metis 收敛信号: Option 2 (改 lib/loop) + 可选 D2 alias 防御纵深
- [x] 1.4 V2 fix ship 决定 (独立 ship, defense-in-depth 价值已验证)
- [ ] 1.5 设计决策落地到代码 (Single-Dev 自审勾选)

## 2. RED: Failing Test (TDD Step 1)

- [ ] 2.1 新建 `tests/test_parser_dsl_schema.cpp` 独立 binary
- [ ] 2.2 Test Case 1: 解析 `lib/loop/react.agent.md`, 断言 `decide` 节点 `arguments` 非空含 `response` key
- [ ] 2.3 Test Case 2: 解析 `lib/loop/plan_execute.agent.md`, 断言 `execute` 节点 `arguments` 非空
- [ ] 2.4 Test Case 3: 解析 `lib/loop/fork_join.agent.md`, 断言 3 个 task 节点 `arguments` 非空
- [ ] 2.5 Test Case 4: (回归) 解析 `lib/inference/load.agent.md` 等 9 个已合规 DSL, 断言 `arguments` 仍非空
- [ ] 2.6 `ctest -R test_parser_dsl_schema` 期望 4 cases FAIL (RED 状态)

## 3. GREEN: Fix Implementation (TDD Step 2)

- [ ] 3.1 修改 `lib/loop/react.agent.md:26` `args:` → `arguments:` (decide)
- [ ] 3.2 修改 `lib/loop/react.agent.md:33` `args:` → `arguments:` (act, 字符串模板值仍不被解析, follow-up)
- [ ] 3.3 修改 `lib/loop/plan_execute.agent.md:24` `args:` → `arguments:` (execute)
- [ ] 3.4 修改 `lib/loop/fork_join.agent.md:22,30,38` 3 处 `args:` → `arguments:`
- [ ] 3.5 (可选 D2) `node_factory.cpp:176` 增加 alias 检测 (`j.contains("args")` fallback)
- [ ] 3.6 `ctest -R test_parser_dsl_schema` 期望 4 cases PASS (GREEN)
- [ ] 3.7 `ctest -L must_realllm -R "^test_e2e_real_llm$"` 期望 ChatSession case PASS (有 DEEPSEEK_API_KEY)
- [ ] 3.8 `ctest -LE must_realllm` 期望核心 + examples tree 100% PASS (零回归)
- [ ] 3.9 `ctest -R test_loop_agent_autonomous` 期望 Test 4 继续 PASS (无回归)

## 4. REFACTOR: CI Script + Spec Update (TDD Step 3)

- [ ] 4.1 新增 `tools/check_dsl_schema.sh` CI 脚本 (YAML 验证 12 lib/*.md)
- [ ] 4.2 `chmod +x tools/check_dsl_schema.sh` + 跑一次确认零违规
- [ ] 4.3 更新 `openspec/specs/dsl.md §5.2` 兼容性说明 (canonical `arguments:` + optional `args:` alias)
- [ ] 4.4 更新 `AGENTS.md` Recent Changes entry (本 change ship 后)

## 5. Ship Gate Verification (TDD Step 4)

- [ ] 5.1 focused ctest PASS (`test_parser_dsl_schema` 4/4 + `test_e2e_real_llm` 8/8 + `test_provider_llm_tool_empty` 11/11 + `test_node_executor_empty_response` 6/6 + `test_dsl_engine_ctx_bridge` 5/5)
- [ ] 5.2 核心 tree full ctest `-LE must_realllm` 100% PASS (零回归)
- [ ] 5.3 examples tree full ctest `-LE must_realllm` 100% PASS (零回归)
- [ ] 5.4 `git status --short` clean (post-commit)
- [ ] 5.5 `git show HEAD --stat` 列出 9 files changed (3 lib DSL + 1 parser + 1 test + 1 CI script + 1 spec + 1 AGENTS.md + 1 plan)
- [ ] 5.6 drop_ratio = 0% (rename 不影响功能)

## 6. Archive + Cooling-Off (TDD Step 5)

- [ ] 6.1 `git mv openspec/changes/2026-09-30-fix-lib-loop-args-parsing → archive/2026-09-30-fix-lib-loop-args-parsing` (Day-5 4-file integrity)
- [ ] 6.2 commit archive move
- [ ] 6.3 24h cooling-off 触发 (per AGENTS.md 治理链)
- [ ] 6.4 merge (如 worktree) + cleanup
- [ ] 6.5 main working tree 重 build + focused ctest (post-merge sanity check)

## Reverse Indicator

```
[Reverse Indicator]
+ new_up:
  - test_e2e_real_llm ChatSession case pre-existing FAIL → PASS (real DeepSeek)
  - lib/loop 3 files spec-aligned with dsl.md §5.2
  - parser regression test (DSL schema audit prevents future drift)
  - CI script (tools/check_dsl_schema.sh)
- old_down: drop_ratio = 0% (rename 改 key 不影响功能, 字符串模板 args 仍不被解析但属 pre-existing 第 2 潜伏 gap)
failure_traces:
  - test_e2e_real_llm ChatSession case "Missing 'response' argument" (since 2026-09-18 F1 ship)
  - Pre-existing from `5a9ab2e` 2026-07-20 (Sprint 21 lib/loop creation)
ablation:
  - Mock: 3-segment (react.agent.md rename 不影响 mock path)
  - Real LLM: lib/loop rename → parser 读 arguments → decide_react 收到 response → no longer FAIL
context_ids:
  - test_e2e_real_llm ChatSession case (the 1 pre-existing failure case)
```

## ⚠️ 已知遗留 (Independent follow-up)

1. **react.agent.md:33 act 节点字符串模板值 `args:`**: `args: "{{decision.action_args}}"` 是字符串模板值 (非 object), 即使改名为 `arguments:` 也会被 parser 的 `is_object()` 检查跳过. act 节点 args 永远为空. **本 change 不修**, 登记为独立 follow-up improvement.

2. **test_loop_agent_autonomous vs test_e2e_real_llm 矛盾点**: Metis 44min 调查未闭环, 推测是 stale build binary (`libLoopAgent.so` Oct 3 vs `node_factory.cpp` Sep 3), 但未实测. **本 change 不修**, 独立 follow-up.