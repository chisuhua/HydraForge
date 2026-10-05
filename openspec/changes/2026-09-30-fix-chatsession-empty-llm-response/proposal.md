## Why

`test_e2e_real_llm` ChatSession case 自 2026-09-18 F1 ship (`commit a96842e`) 至今仍是 **pre-existing failure**——`Tool 'loop/decide_react' failed: Missing 'response' argument`。F1 修复了 `node_executor.cpp:204-214` (main path) + `:147-156` (stream path) 的 `is_string() && empty()` 检查，但 ChatSession → loop/run → child DSL → think (llm_call) → decide (tool_call) 链路仍有 **3 类空响应**绕过 F1 fail-fast:

1. **`null` JSON 值**：`is_string()` 返回 false，不抛
2. **whitespace-only 字符串**（`" "`、`"\n"`）：`get<string>().empty()` 返回 false，不抛
3. **ProviderLLMTool 返回 success + 非 string 字段**：JSON 序列化路径漏检

任何一种情况都会让"空文本"绕过 F1，到达 decide 节点 → `loop/decide_react` (pdk_entry.cpp:434) 抛 `Missing 'response' argument`。

**审计报告误导标签**：commit `a21c92e` (must_realllm tag ship) 和 `docs/audits/2026-09-29-harness-self-evolution-rsi-audit.md:443` 把这个失败错误标注为 "PluginLoader whitelist"——实际白名单通过 (`HYDRAFORGE_PLUGIN_PATH` 由 ctest 注入), 真正失败点是 F1 V2 残余（已在 `chat-real-llm-coverage Phase H` follow-up 登记，本 change 关闭该 follow-up）。

**测试可移植性 bug**：直接运行 `./test_e2e_real_llm`（无 ctest env injection）会撞 PluginLoader 白名单墙（"path not in whitelist, rejected"），导致开发者本地无法直接 debug。这与本次主修复独立，但建议合并 ship 一次性清理。

## What Changes

- **修复 1（主修复，根因）**：扩展 `node_executor.cpp` 空 text 检查（main path L204-214 + stream path L147-156）覆盖 `null` / empty string / whitespace-only 3 类盲区（**SHIP-with-fixes**: null 检查前移到 `get<std::string>()` 之前）
- **修复 2（防御纵深）**：扩展 `pdk/loop_agent/src/pdk_entry.cpp` ProviderLLMTool 空 text 检查（L63-68）覆盖 whitespace-only 场景（**SHIP-with-fixes**: 改用 `this->name()` 而非虚构的 `provider_name_`）
- **修复 3（测试可移植性）**：在 `examples/pdk_chat_demo/tests/test_e2e_real_llm.cpp` 顶部 `find_plugin_dir()` + setenv, 让直接运行也能加载（**SHIP-with-fixes**: 从 hard-coded 改为动态查找复用 find_loop_dir 模式）
- **修复 4（regression guard）**：新增 `tests/test_node_executor_empty_response.cpp` 覆盖 3 类空响应场景（null / whitespace / empty string）
- **修复 5（同步 source guard + replica）**：同步更新 `tests/test_provider_llm_tool_empty.cpp` Case 3 source guard 签名 + replica 逻辑（**SHIP-with-fixes 新增**：D2 改动会破坏现有 source guard）
- **修复 6（审计修正）**：更新 `docs/audits/2026-09-29-harness-self-evolution-rsi-audit.md:443` 误导标签 → 准确描述 "F1 V2 residual"
- **修复 7（治理同步）**：更新 `AGENTS.md` Recent Changes entry + FULL REGRESSION TEST FLOW §2 预期 9/10 → 10/10 + a21c92e 历史条目勘误 + roadmap §1.4 Bug 3 V2 修复记录

## Capabilities

### New Capabilities
<!-- Capabilities being introduced. Replace <name> with kebab-case identifier -->
- `empty-llm-response-failfast`: DSL `llm_call` 节点空响应 fail-fast 契约扩展 — 覆盖 null / empty string / whitespace-only 3 类空响应，统一 main path + stream path + ProviderLLMTool 3 道防线，确保下游消费者（`decide_react` / `execute_plan` / `process_task`）永远收到非空文本

### Modified Capabilities
<!-- Existing capabilities whose REQUIREMENTS are changing. Check openspec/specs/. Leave empty if no requirement changes. -->
- `openspec/specs/react-agent-llm-ctx-bridge/spec.md` (R3 "think 节点空响应时显式失败 (F1 fix)" amendment) — 从 "empty string" 扩展为 "null / whitespace / empty string" 3 类

## Impact

**Production code (5 files)**:
- `src/modules/executor/node_executor.cpp` (L147-156 stream path + L204-214 main path 空 text 检查扩展)
- `pdk/loop_agent/src/pdk_entry.cpp` (L63-68 ProviderLLMTool 空 text 检查扩展)
- `examples/pdk_chat_demo/tests/test_e2e_real_llm.cpp` (顶部 setenv + find_plugin_dir)
- `tests/test_node_executor_empty_response.cpp` (新增, regression guard)
- `tests/test_provider_llm_tool_empty.cpp` (Case 3 source guard + replica 同步) — **SHIP-with-fixes 新增**

**Spec / docs (4 files)**:
- `openspec/specs/react-agent-llm-ctx-bridge/spec.md` (R3 amendment)
- `docs/audits/2026-09-29-harness-self-evolution-rsi-audit.md:443` (审计标签修正)
- `AGENTS.md` (Recent Changes + FULL REGRESSION §2 + a21c92e 勘误)
- `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` (§1.4 Bug 3 V2 修复记录)

**Total: 9 files (5 production + 4 spec/docs), 1 atomic commit** (per AGENTS.md 模式 #4 SHIP-with-fixes 流程).

## Non-goals

- 不重写 ReactLoop / PlanExecuteLoop / ForkJoinLoop C++ class
- 不重写 DSL node 类型（start / llm_call / tool_call / assign / end 5 类保持不变）
- 不引入新的 DSL 节点类型
- 不修 LLM provider 实现（上游 DeepSeek/MiniMax 真实 bug 不在本次修复范围）
- 不重写 react.agent.md / plan_execute.agent.md / fork_join.agent.md
- 不修复 `loop/execute_plan` / `loop/process_task` 同类空响应 bug（如有, 独立 follow-up）
- 不删除 `loop/decide_react` 在 pdk_entry.cpp:434 的 `Missing 'response' argument` 检查（保留作为最后防线）

## Dependencies

- `commit a96842e` (F1 fix-react-decide-empty-response) ✅ 已 ship
- `commit a21c92e` (must_realllm tag + CTest label) ✅ 已 ship
- `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md §1.4 Bug 3 残量风险`
- `openspec/specs/react-agent-llm-ctx-bridge/spec.md` (F1 ship 产物的 spec)
- `pdk_chat_demo_evolution` reference example (`test_e2e_real_llm*` 4 个 binary)

## Pre-Implementation Dual-Agent Review (per AGENTS.md Pattern #8)

按 AGENTS.md Pattern #8 (`openspec/changes/*/proposal.md` 跨 ≥3 files + 生产代码 + spec 决策需复核), 已派 `task(subagent_type="oracle", run_in_background=true)` (session `ses_f0cd9341cffeUIvJukey0P8ANM`, 36m39s) + `task(subagent_type="metis", run_in_background=true)` (session `ses_f0cd932b0ffezVxAi65OlTXiaY`, 12m10s) 并行双审查。

**Oracle + Metis 反馈收敛**（4 个 cross-validated C 级 deal-breaker）:

| # | Finding | 双 agent 一致 | 修正 |
|---|--------|---------------|------|
| **C1** | D1 null/non-string 检查在 `get<std::string>()` 后是死代码 — nlohmann::json::type_error.302 在 L203 提前抛 | ✅ Oracle + Metis | D1 修正：null 检查前移到赋值前，基于 `result["text"]` 而非 `new_context[key]` |
| **C2** | `provider_name_` 字段不存在（ProviderLLMTool 无此成员，ILLMProvider 接口也无 name() 方法） | ✅ Oracle + Metis | D2 修正：改用 `this->name()` (ILLMTool::name() 返回硬编码 "loop-agent-provider-bridge") |
| **C3** | D2 改动破坏 `tests/test_provider_llm_tool_empty.cpp` Case 3 source guard + replica 同步遗漏 → 第 7 文件未列 | ✅ Oracle + Metis | 加入 Implementation Summary 第 3 文件，扩展 scope 到 9 files |
| **C4** | D3 hard-coded `/workspace/project/HydraForge/build/pdk` 违反用户显式 "env 可移植性" 隐含需求 | ✅ Metis | D3 修正：复用项目既有 `find_loop_dir()` 动态查找模式，引入 `find_plugin_dir()` |

**Metis 单独 findings**（应用 Minor 修正）:
- **A 级歧义**: TDD RED 阶段 Case 1 null 会抛 type_error（不是 silent）→ 测试必须断言消息子串，否则假 GREEN
- **A 级歧义**: ASCII whitespace 限制（`std::isspace` 不识别 U+3000 中文全角）→ spec 明确 ASCII-only
- **AI pitfall**: 5-in-1 commit 混合关注点（生产 + 测试 + 文档）→ 决定保持 1 atomic commit (per Pattern #4 原子性) 但诚实记录 scope
- **治理隐含需求**: AGENTS.md 多处同步 + a21c92e 历史条目勘误（commit immutable 但 Recent Changes entry 可加 errata 注释）

**Oracle 单独 findings**（应用 Minor 修正）:
- **机制澄清**: F1 仅覆盖 empty string，null/non-string 实际走 type_error 路径（fail 但消息不友好），whitespace-only 才是真正 silent → D1 修正已正确分类 3 类
- **CMakeLists.txt:349 单路径验证**: PROJECT_BINARY_DIR 在子目录中 = 顶层 build dir → 与 ctest ENVIRONMENT 注入兼容，无需调整 CMake

**SHIP-with-fixes 应用流程** (per AGENTS.md 模式 #4):
1. ✅ 并行双审查 → 收集反馈 → 收敛信号
2. ✅ 应用 4 个 C 级修正到 design.md / proposal.md (atomic edit)
3. ⏸ Single-Dev 自审勾选 (无需再次完整 Oracle 审查) — **待用户复核**
4. ⏸ TDD 5 步 + 1 atomic commit + archive + cooling-off

**Verdict**: **SHIP-with-fixes** (4 C 级 + 3 Minor 修正全部 applied, design 物理可行)