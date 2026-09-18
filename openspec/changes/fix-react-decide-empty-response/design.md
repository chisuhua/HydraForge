## Context

`commit 0b0da50` (`fix(tool_result): bridge top-level 'error' field to meta.error_message for PDK diagnostic`) 修复了 PDK 工具失败错误信息隐形系统性问题，使 `loop/decide_react` 真实失败原因首次可见：`Tool 'loop/decide_react' failed: Missing 'response' argument`。

## Root Cause (CONFIRMED — Oracle `ses_f4d05cdb0ffe0BhMdEADyfsdTz`)

**CORRECTED from initial hypothesis**: `flatten_layers` nesting is NOT on the react.agent.md execution path. `DSLEngine::run(LayeredContext)` 直接 `scheduler.execute(ctx.working)` 透传 L3 working 顶层 keys 给 flat Context，NodeExecutor 全部 render 调用走 `const Context&`（flat JSON）重载。`InjaTemplateRenderer::render(LayeredContext)` 重载（唯一走 flatten_layers 的入口）的**生产调用方为 0**（唯一调用方是 `tests/test_context_adapter.cpp:123`）。

**Actual root cause**: think 节点（llm_call）写入 output_key 的值为空字符串。反应链：
   1. `call_llm_tool` 返回 `result.success=true` + `result.text=""`（真实 LLM 因配置错误或模型遮蔽返回空响应）
   2. `node_executor.cpp:194` `new_context[key] = result["text"].get<std::string>()` 静默写入空字符串
   3. `react.agent.md` decide 节点 `args: response: "{{llm_response}}"` 渲染为空字符串
   4. `loop/decide_react` 收到空 `response` → 返回 `"Missing 'response' argument"`

**Verified by standalone inja test** (`tests/test_dsl_engine_ctx_bridge.cpp` Case 1-3):
   - present-but-empty key → inja renders `""` **silently (no throw)** — 这是关键: 生产路径 inja 不抛错, 而是静默返回空
   - missing key → inja 抛 `RenderError` — 这就是为什么不能用 inja strict mode 替代
   - 结论: fail-fast 必须放在生产者端（llm_call output），不能放在消费者端（decide 渲染端）

**Why mock mode 不能 reproduce**: `pdk/loop_agent/src/pdk_entry.cpp` 的 mock_fallback 路径（line 686-696）直接返回 mock response，**不执行** `lib/loop/react.agent.md` 子图。需要真实 LLM 或直接 NodeExecutor 级 mock 注入空 text 才能复现。

## Goals / Non-Goals

**Goals:**
- ✅ 完整诊断 react loop `decide` 节点真实 LLM 端到端失败根因
- ✅ 最小修复（per AGENTS.md "bugfix rule: fix minimally, never refactor while fixing"）
- ✅ 加 NodeExecutor 级 mock 注入回归测试
- ⏸ 真实 LLM 端到端测试降级为 skip-guarded smoke skeleton (sandbox 无 API key)
- ⏸ chat-real-llm-coverage Phase H — 推到 follow-up (Single-Dev 有 key 时实施)
- ✅ 保留现有契约向后兼容（不破坏已 ship 的 P0 mock 路径 + C1 13 unit test）

**Non-Goals:**
- 不重写 ReactLoop C++ class (保留 Sprint 20 ship 行为)
- 不重写 DSL node 类型 (start/llm_call/tool_call/assign/end 保持不变)
- 不引入新的 DSL 节点类型
- 不修复 fork_join / plan_execute 类似 ctx bridge bug (本 change 仅 react; 若根因诊断发现通用问题, 后续独立 change 处理)

## Decisions

### D1: `llm_call` output_keys 数据类型策略 (VERIFIED — no change needed)

**决策**: llm_call 节点 output_keys[0] 默认存 `result.text` 字符串 (current 行为), 多 output_keys 按 JSON path 分别存 result 子字段 (per process_output_keys pattern, line 541+).

**验证结果**: 现有行为正确。`new_context[key] = result["text"].get<std::string>()` 是 string 写入。flat ctx 透传到 decide 节点时, `{{llm_response}}` 能找到 string 值。**根因不在此处**, 而在 LLM 返回了空 text（上游问题, 见 Context）。

### D2: 修复策略 (REVISE — minimal fail-fast at llm_call output)

**决策**: `node_executor.cpp:194-205` 在写入 output_key 后, 若值为空字符串, 立即抛 `runtime_error("LLM call succeeded but returned empty text for node '...' output_key '...'. Check provider model availability or prompt template.")`。同样校验复制到 stream 分支 (line 147-157, per AGENTS.md 模式 #1 step 4 系统性记录同类潜伏站点)。

**Rationale**:
- ✅ 最小修复 (AGENTS.md "bugfix rule: fix minimally")
- ✅ 在生产者端 (llm_call output) fail-fast, 而不是消费者端 (decide 渲染端) — 因为 inja 无法区分 "缺失 key" vs "key 存在但空值" (前者抛错后者静默 `""`)
- ✅ 不引入新 ErrorCode (直接 runtime_error, 错误信息含诊断线索)
- ✅ 不改 react.agent.md schema (向后兼容 P0 mock_fallback + C1 13 unit test)
- ✅ 防御性深度: 同一校验同时覆盖 stream 分支 (虽然当前 react.agent.md 不走 stream, 但未来 streaming react loop 会触发)

**NOT chosen alternatives**:
- (a) inja strict mode → 全项目级, 可能破坏 245 现有测试, scope 过大
- (b) decide 渲染后校验 → 治标不治本, 错误信息丢失 ("Missing 'response'" → 不如直接指出 "empty text")
- (c) ProviderLLMTool 层校验 → 多余防御, 下游已有 2 道 guard (node_executor + decide_react empty-check)
- (d) 改 `flatten_layers` 拍平 → 治疗错误的病, react 路径根本不经过 flatten_layers

### D3: fork/join 节点 ctx 隔离 vs 共享 (RESOLVED — current schema OK)

**Resolved by Oracle audit**: `loop/decide_react` 在 `pdk_entry.cpp:106` (child) 和 `:405` (parent/plugin) 注册了两次，**不是 bug** — 两个不同 registry，两套独立 ToolMetadata，均含 empty check (line 118/423)。fork_join 当前 schema 用 `{{user_input}}` 顶层访问, 不依赖跨分支 ctx 共享 — **当前 fork_join 不受影响**。

### D5: 真实 LLM 测试 CI 守卫 (DEGRADED — skip-guarded skeleton)

**决策**: 原计划 6 cases 端到端测试降级为 **skip-guarded smoke skeleton ×1 case** (per tests/AGENTS.md Pattern #1 三态分离 + Oracle `ses_f4caa8cf0ffeajSx05M235DwDz` 推荐)。当 sandbox 有 `DEEPSEEK_API_KEY` 时跑, 无 key 时 `SUCCEED("skipped")` 退出。

**Rationale**:
- mock 注入 (MockLLMEmptyTool) 已验证 fix 路径
- NodeExecutor-level 测试覆盖完整 — 不依赖网络
- 真实 LLM 6 cases 投入产出比低 (需 key, 仍 flakiness, 无法 sandbox 验证)
- Single-Dev 有 key 时可手动扩 Phase H (chat-real-llm-coverage follow-up)

## Implementation Summary

| File | Lines | Change |
|---|---|---|
| `src/modules/executor/node_executor.cpp` | +21 | Main path empty check (line 194-205) + stream path empty check (line 147-157) |
| `tests/test_dsl_engine_ctx_bridge.cpp` | +182 (new file) | 5 cases / 13 assertions: inja behavior docs (3 cases) + NodeExecutor GREEN guard (Case 4) + regression (Case 5) |

**Test results**: `All tests passed (13 assertions in 5 test cases)`. 0 regression on focused test set (test_loop_agent_autonomous|test_loop_agent_plugin|test_dsl_engine_ctx_bridge|test_executor|test_scheduler 7/7 PASS).

## Latent Sites (AGENTS.md 模式 #1 step 4 系统性记录)

| Site | Status | Risk | Action |
|---|---|---|---|
| `pdk/loop_agent/src/pdk_entry.cpp:46-66` ProviderLLMTool::generate | empty text 透传 (success=true + text="") | 多余防御 — 下游已有 2 道 guard | 记录, 不修 |
| `pdk/loop_agent/src/pdk_entry.cpp:402-450` loop/process_task | empty result 字段透传 | fork_join 模板可能渲染空 → 需独立 change | 记录, 不在本 F1 scope |
| `context_flatten.h:39-50` flatten_layers | 注释自相矛盾 (line 24-25 vs line 33-34) | doc-vs-code drift, 不影响生产路径 | 独立 follow-up change |
| `GenerationRequest.model` 字段遮蔽 (Wave 1 #1 同类) | 已 ship 修复 (commit `0b0da50`) + 独立 follow-up change `fix-generation-request-model-default` | 已知问题 | 跟踪独立 change |

## Risks / Trade-offs

- **[Risk: 现有测试依赖空文本穿透]** → Mitigation: 全量 ctest `-E 'pkm_temporal_demo|test_scenarios'` ship gate 验证
- **[Risk: whitespace-only 文本 (`"\n"`) 通过校验]** → Mitigation: 不 trim, 保持最小修复, design.md 记录为 known limitation
- **[Risk: 修复 break 现有 245/245 ctest]** → Mitigation: TDD 5 步, RED→GREEN→REFACTOR, 每个 commit 后跑 focused ctest 验证零回归
- **[Risk: ProviderLLMTool/process_task 同类风险]** → Mitigation: 记录为 latent site, 独立 change 处理 (不在本 F1 scope)

## Migration Plan

1. ✅ Oracle 咨询根因诊断 (Oracle `ses_f4d05cdb0ffe0BhMdEADyfsdTz`) — 12m 55s
2. ✅ 设计评审完成 (Oracle `ses_f4caa8cf0ffeajSx05M235DwDz`) — 4m 3s
3. ✅ GREEN 修复实施 (main path + stream path)
4. ✅ NodeExecutor 级测试 (Case 4 真 GREEN guard, 非 lambda 副本)
5. ⏳ 全量 ctest ship gate (进行中)
6. ⏳ archive + master plan §一.4 bug3 移除 + active-status 同步

## Oracle Sessions

- `ses_f4d05cdb0ffe0BhMdEADyfsdTz` (12m 55s): 设计评审, 纠正初判根因
- `ses_f4caa8cf0ffeajSx05M235DwDz` (4m 3s): 完成审计, 确认方案 + 建议修订