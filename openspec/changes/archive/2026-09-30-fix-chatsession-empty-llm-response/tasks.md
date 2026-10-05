## 1. Pre-Implementation Dual-Agent Review (Pattern #8)

- [ ] 1.1 `task(subagent_type="oracle", run_in_background=true)` 审查 design.md D1-D6 + acceptance criteria + scope
- [ ] 1.2 `task(subagent_type="metis", run_in_background=true)` 审查 design.md 隐含意图 + spec 歧义点
- [ ] 1.3 收集 Oracle + Metis 反馈, 收敛信号 (cross-validated findings 优先)
- [ ] 1.4 应用 SHIP-with-fixes 修正到 design.md (atomic edit, 不 amend baseline)
- [ ] 1.5 设计决策落地到代码 (无需再次完整 Oracle 审查, Single-Dev 自审勾选即可)

## 2. RED: Failing Test (TDD Step 1)

- [ ] 2.1 新建 `tests/test_node_executor_empty_response.cpp` 独立 binary
- [ ] 2.2 Test Case 1: 模拟 ProviderLLMTool 返回 null response → DSL 应抛明确错误
- [ ] 2.3 Test Case 2: 模拟 ProviderLLMTool 返回 whitespace-only response (`" "` / `"\n"`) → DSL 应抛明确错误
- [ ] 2.4 Test Case 3: 模拟 ProviderLLMTool 返回 empty string response → DSL 应抛明确错误 (F1 regression guard)
- [ ] 2.5 `ctest -R test_node_executor_empty_response` 期望 Case 1-3 FAIL (RED 状态)
- [ ] 2.6 验证 test_e2e_real_llm ChatSession case 在 main 分支确实 FAIL (RED 状态, 锁定基线)

## 3. GREEN: Fix Implementation (TDD Step 2)

- [ ] 3.1 扩展 `node_executor.cpp` main path 空 text 检查 (L204-214) → 覆盖 null / empty / whitespace (**SHIP-with-fixes**: null 检查前移到 `get<std::string>()` 之前, 基于 `result["text"]` 而非 `new_context[key]`)
- [ ] 3.2 扩展 `node_executor.cpp` stream path 空 text 检查 (L147-156) → 同上 (基于 `result["text"]`)
- [ ] 3.3 扩展 `pdk_entry.cpp` ProviderLLMTool 空 text 检查 (L63-68) → 覆盖 whitespace (**SHIP-with-fixes**: 改用 `this->name()` 而非虚构 `provider_name_`)
- [ ] 3.4 **同步** 更新 `tests/test_provider_llm_tool_empty.cpp` Case 3 source guard 签名 + `provider_llm_tool_generate_replica` 逻辑（**SHIP-with-fixes 新增**: D2 改动会破坏现有测试）
- [ ] 3.5 修 `test_e2e_real_llm.cpp` 顶部 `find_plugin_dir()` + PluginPathSetter setenv HYDRAFORGE_PLUGIN_PATH (**SHIP-with-fixes**: 动态查找复用 find_loop_dir 模式, 不 hard-code)
- [ ] 3.6 `ctest -R test_node_executor_empty_response` 期望 Case 1-3 PASS
- [ ] 3.7 `ctest -R test_provider_llm_tool_empty` 期望 11/11 PASS (Case 3 source guard 更新后)
- [ ] 3.8 `ctest -L must_realllm` 期望 test_e2e_real_llm ChatSession case PASS (有 DEEPSEEK_API_KEY)
- [ ] 3.9 `ctest -LE must_realllm` 期望核心 + examples tree 100% PASS (零回归)

## 4. REFACTOR: Spec Amendment + Audit + Doc Sync (TDD Step 3)

- [ ] 4.1 更新 `openspec/specs/react-agent-llm-ctx-bridge/spec.md` R3 amendment (empty string → null/whitespace/empty string)
- [ ] 4.2 更新 `docs/audits/2026-09-29-harness-self-evolution-rsi-audit.md:443` 标签 (F1 V2 residual)
- [ ] 4.3 更新 `AGENTS.md` Recent Changes entry (本 change ship 后)
- [ ] 4.4 更新 `AGENTS.md` FULL REGRESSION TEST FLOW §2 预期 9/10 → 10/10 (本 change 修复后 test_e2e_real_llm PASS)
- [ ] 4.5 更新 `AGENTS.md` Recent Changes entry for a21c92e 加勘误注释（"PluginLoader whitelist"标签实际是 F1 V2 residual, 历史 commit immutable 但 entry 文本可加 errata）
- [ ] 4.6 更新 `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md §1.4 Bug 3 残量风险` 追加 V2 修复记录（"2026-09-30 ship in 2026-09-30-fix-chatsession-empty-llm-response"）

## 5. Ship Gate Verification (TDD Step 4)

- [ ] 5.1 focused ctest PASS (test_node_executor_empty_response 3/3 + test_dsl_engine_ctx_bridge 5/5 + test_provider_llm_tool_empty 11/11 + test_e2e_real_llm ChatSession PASS)
- [ ] 5.2 直接运行 `./build/examples/pdk_chat_demo/tests/test_e2e_real_llm` 不撞白名单墙（setenv 生效验证）
- [ ] 5.3 核心 tree full ctest `-LE must_realllm` 100% PASS (零回归)
- [ ] 5.4 examples tree full ctest `-LE must_realllm` 100% PASS (零回归)
- [ ] 5.5 `git status --short` clean (post-commit)
- [ ] 5.6 `git show HEAD --stat` 列出 9 files changed (5 production + 2 test + 2 docs/spec, 含 AGENTS.md 同步)
- [ ] 5.7 drop_ratio = 0% (per AGENTS.md Reverse Indicator Rule, 增强是新增约束不影响 happy path)
- [ ] 5.8 1 atomic commit (per AGENTS.md 模式 #4 SHIP-with-fixes, 不拆 commit)

## 6. Archive + Cooling-Off (TDD Step 5)

- [ ] 6.1 `openspec archive 2026-09-30-fix-chatsession-empty-llm-response --path openspec/changes/archive/2026-09-30-fix-chatsession-empty-llm-response` (Day-5 4-file integrity verified)
- [ ] 6.2 24h cooling-off 触发（per AGENTS.md 治理链, post-archive）
- [ ] 6.3 merge worktree → main (--no-ff 保留 change history) + cleanup worktree + branch -D
- [ ] 6.4 main working tree 重 build + focused ctest PASS (post-merge sanity check)
- [ ] 6.5 chat-real-llm-coverage Phase H follow-up 关闭（per `docs/roadmap/...roadmap.md §1.4 Bug 3 残量风险`）

## Reverse Indicator (含 SHIP-with-fixes 修正 + 实施阶段真实根因发现)

```
[Reverse Indicator]
+ new_up: 
  - 3 类空响应 fail-fast 全覆盖 (null / whitespace / empty) — 唯一真正 silent pass = whitespace-only (per Oracle 审查)
  - test_node_executor_empty_response 6/6 PASS (3 cases: empty string regression guard + whitespace fail-fast + happy path)
  - test_e2e_real_llm 直接运行可移植性 (find_plugin_dir 动态查找, 跨环境 clone) — D3 SHIP-with-fixes 修正
  - test_provider_llm_tool_empty.cpp Case 3 source guard 同步更新 (避免 regression) — D2 SHIP-with-fixes 新增第 7 文件
  - AGENTS.md / audit / roadmap 多处同步, 治理一致性提升
- old_down: drop_ratio = 0% (扩展是 fail-fast 新增约束, 不影响 happy path; 0 个现有能力退化)
failure_traces: 
  - 直接运行 ./test_e2e_real_llm → "path not in whitelist, rejected" (env portability, before D3 fix)
  - test_provider_llm_tool_empty.cpp Case 3 source guard 找旧签名 "LLM call succeeded but returned empty text" FAIL (before replica sync)
  - Pre-existing from 2026-09-18 (F1 commit a96842e) V2 follow-up
ablation: 
  - Mock 模式: 3-segment (baseline / mutated / rerun) 全 mock-identical (新检查只在真实空响应时触发, mock 永不返回空)
  - Real LLM: 增强前后 drop_ratio = 0% (happy path 不变, 仅空响应触发 fail-fast)
  - 跨环境 clone: 直接运行 ./test_e2e_real_llm 在 /home/user/work/ 等非默认路径下也能加载 (before D3 fix 会撞白名单墙)
context_ids: 
  - test_node_executor_empty_response (regression guard for whitespace silent pass)

## ⚠️ 实施阶段真实根因发现 (实施 SHIP-with-fixes 流程中, 不影响本 change ship)

**重要**: 在 Inline Execution 阶段实测 test_e2e_real_llm ChatSession case **仍 FAIL** with "Missing 'response' argument". debug 揭示真正根因**不是** F1 V2 residual (whitespace silent pass), 而是 **DSL parser schema 不匹配**:
- `src/modules/parser/node_factory.cpp:176` 只查 `arguments:` key (per DSL spec `docs/specs/dsl.md §5.2`)
- `lib/loop/{react,plan_execute,fork_join}.agent.md` 3 个文件 6 处用 `args:` (从创建时 `5a9ab2e` 2026-07-20 引入, 是人为因素 — 其他 9 个 lib DSL 全部合规)
- 导致 react loop 的 decide 节点 `args: {response: "{{llm_response}}"}` 被 parser **静默丢弃**, decide_react 收到空 args → `args.find("response") == args.end()` → "Missing 'response' argument"

**V2 fix 价值再评估** (per Oracle `ses_ef4386f3dffeiO3OvtOOCogRYy` + Metis `ses_ef433ad8bffewxBioiEktPFcFM` dual-agent review):
- ✅ V2 fail-fast 是**真实存在的 bug 修复** (whitespace silent pass), test_node_executor_empty_response 6/6 PASS 实证
- ✅ 是 test_e2e_real_llm ChatSession case 失败的**正交**原因 (parser bug 才是真根因), 但 V2 fix 独立 ship 不阻止测试因果链
- ✅ 防御纵深价值: 即使未来 parser bug 修了, V2 fix 仍防 LLM 真返空响应的 edge case

**Ship 决策** (user 显式确认 "Ship V2 fix + 开新 parser change"):
- Commit A (本 change): ship V2 fix + env portability + source guard sync + doc sync (本 OpenSpec 工作树)
- Commit B (新 OpenSpec change `fix-lib-loop-args-parsing`): lib/loop/*.agent.md `args:` → `arguments:` + parser regression test + parser `args:` alias 防御纵深

**⚠️ 附带发现 (独立第 2 潜伏 gap, 不在本 change scope)**:
`react.agent.md:33` `args: "{{decision.action_args}}"` 是**字符串模板值** (非 object), 即使改名为 `arguments:` 也会被 parser 的 `is_object()` 检查跳过 — act 节点 args 永远为空. 登记为独立 follow-up improvement.
```