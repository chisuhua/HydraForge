## Context

`commit a96842e` (F1 fix-react-decide-empty-response, 2026-09-18) 修复了 `node_executor.cpp` `llm_call` 节点空 string 响应的 fail-fast check。但 explore agent 调查 (session `ses_f0d04c768ffedKjnL37eO0hOJo`) 揭示 F1 检查在 3 类空响应下仍然 fail-open:

```cpp
// F1 current code (node_executor.cpp:209):
if (new_context[key].is_string() && new_context[key].get<std::string>().empty()) {
    throw std::runtime_error("LLM call succeeded but returned empty text...");
}
```

**3 类空响应盲区**:
1. **`null` JSON 值**：`is_string()` → false → 检查不抛
2. **whitespace-only 字符串**（`" "`、`"\n"`、`"\t"`）：`get<string>().empty()` → false → 检查不抛
3. **非 string 类型**（number/object/array/null）：`is_string()` → false → 检查不抛

实际 ChatSession e2e 失败链 (per explore 调查):

```
ChatSession::chat() [chat_session.cpp:438]
  → impl_->registry->call_tool("loop/run", loop_args) [chat_session.cpp:502]
    → loop/run lambda [pdk_entry.cpp:645]
      → DSLEngine::from_markdown(agent_content) [line 727]
      → child->register_llm_tool("llama-default", ProviderLLMTool) [line 731-733]
      → register_react_support_tools_for_child(*child) [line 737]
      → child->run(ctx, Autonomous) [line 754]
        → react.agent.md:12 think node (type: llm_call)
          → execute_dsl_node() [node_executor.cpp:119]
            → tool_registry_.call_llm_tool("llama-default", ...) [node_executor.cpp:196]
              → ProviderLLMTool::generate() [pdk_entry.cpp:47]
                → provider_.generate(req, token) [line 57] ← 真实 LLM 调用
              → F1 empty text check [node_executor.cpp:209] ← 漏 3 类空响应
        → react.agent.md:23 decide node (type: tool_call)
          → execute_tool_call() [node_executor.cpp:234]
            → dispatch_to_tool("loop/decide_react", ...) [node_executor.cpp:280]
              → lambda [pdk_entry.cpp:430]
                if (it->second.empty()) return error_result("Missing 'response' argument") [line 434] ← 🔴 失败点
```

**ProviderLLMTool 自身的检查**（pdk_entry.cpp:63-68）也只覆盖 `out.text.empty()`，不覆盖 whitespace-only。

## Root Cause

| Layer | F1 修复 | ProviderLLMTool 检查 | 盲区 |
|-------|---------|---------------------|------|
| `node_executor.cpp` main path (L204-214) | `is_string() && empty()` | N/A | null / 非 string / whitespace |
| `node_executor.cpp` stream path (L147-156) | `is_string() && empty()` | N/A | null / 非 string / whitespace |
| `pdk_entry.cpp` ProviderLLMTool (L63-68) | N/A | `out.text.empty()` | whitespace |
| `pdk_entry.cpp` loop/decide_react (L434) | N/A | `it->second.empty()` | 最后防线（保留） |

3 层防御都需要扩展。第 4 层是 `loop/decide_react` 的最后防线（保留）。

## Goals / Non-Goals

**Goals**:
- ✅ 修复 test_e2e_real_llm ChatSession case pre-existing failure（real DeepSeek PASS）
- ✅ 扩展 F1 fail-fast 覆盖 null / 非 string / whitespace 3 类空响应
- ✅ ProviderLLMTool 防御纵深（whitespace 场景）
- ✅ 修复 test 直接运行可移植性（HYDRAFORGE_PLUGIN_PATH setenv）
- ✅ Regression guard test 覆盖 3 类空响应场景
- ✅ 审计报告误导标签修正（"PluginLoader whitelist" → "F1 V2 residual"）
- ✅ 保留现有契约向后兼容（F1 main/stream path 已 ship 行为不变；multi_turn / generate_subgraph / errors 3 个测试继续 PASS）
- ✅ 24h Cooling-Off（per AGENTS.md 治理链）

**Non-Goals**:
- 不重写 ReactLoop / PlanExecuteLoop / ForkJoinLoop C++ class
- 不重写 DSL node 类型
- 不引入新的 DSL 节点类型
- 不修 LLM provider 实现（上游 DeepSeek / MiniMax 真实 bug 不在本次范围）
- 不重写 react.agent.md / plan_execute.agent.md / fork_join.agent.md
- 不修 loop/execute_plan / loop/process_task 同类空响应 bug（如有, 独立 follow-up）

## Decisions

### D1: 空值判定扩展策略（VERIFIED — minimal fail-fast 3 类覆盖，含 SHIP-with-fixes 修正）

**决策**: 在 F1 现有 `is_string() && empty()` 检查基础上扩展, 覆盖 3 类空响应:

```cpp
// Before (F1, node_executor.cpp:203-211):
new_context[key] = result["text"].get<std::string>();  // ← null 时这里抛 nlohmann type_error.302 (未到 F1)
if (new_context[key].is_string() && new_context[key].get<std::string>().empty()) {
    throw std::runtime_error("LLM call succeeded but returned empty text...");
}

// After (V2 — SHIP-with-fixes corrected):
// 1. 先检查 result["text"]（赋值前，避免 get<string>() 先抛 type_error）
const auto& text_value = result["text"];
bool is_empty_response = text_value.is_null() ||
    (text_value.is_string() && (
        text_value.get<std::string>().empty() ||
        std::all_of(text_value.get<std::string>().begin(),
                    text_value.get<std::string>().end(),
                    [](unsigned char c){ return std::isspace(c); })
    ));
if (is_empty_response) {
    throw std::runtime_error(
        "LLM call returned null/empty/whitespace response for node '" +
        node->path + "' output_key '" + key + "'. "
        "Check provider model availability, prompt template, or response format.");
}
// 2. 赋值
new_context[key] = text_value.get<std::string>();
```

**Rationale**:
- ✅ 最小修复（per AGENTS.md "bugfix rule: fix minimally, never refactor while fixing"）
- ✅ 不引入新 ErrorCode (直接 runtime_error, 错误信息含诊断线索)
- ✅ 不改 react.agent.md / plan_execute.agent.md / fork_join.agent.md schema (向后兼容)
- ✅ 不影响合法 falsy 值（`"0"` / `"false"` / `0.0` 等非空字符串/数值均不被误判）
- ✅ 防御性深度: 同一校验同时覆盖 main path (L204-214) + stream path (L147-156)
- ✅ **修正后**：null 检查在 `get<std::string>()` 之前（避免 type_error 提前抛），覆盖**真**的 3 类 silent/visible 空响应（null + empty string + whitespace-only）

**机制澄清**（per Oracle 审查）:
- F1 的 `is_string() && empty()` 检查**仅**覆盖 empty string
- null 和 non-string 类型在 `result["text"].get<std::string>()` (L203) 即抛 `nlohmann::json::type_error.302`（fail-fast 但错误消息不友好）
- **真正 silent pass 的是 whitespace-only**（`get<std::string>()` 成功，`empty()` false）
- V2 修复：(a) 把 null 检查前移到赋值前，提供统一友好错误消息；(b) 新增 whitespace-only 检测覆盖 silent pass

**ASCII whitespace 范围声明**（per Metis 审查）:
- `std::isspace` 仅识别 ASCII whitespace (0x09-0x0D, 0x20)
- 不识别中文全角空格 U+3000 / Unicode 空白字符
- LLM 真实响应以 ASCII whitespace 为主，V2 接受 ASCII-only 限制
- spec.md 明确标注："`" "` / `"\n"` / `"\t"` (ASCII whitespace)"

**NOT chosen alternatives**:
- (a) 在 DSL 模板渲染端校验 → 治标不治本, 错误信息丢失（"Missing 'response'" 不如直接指出 "empty text"）
- (b) 删除 `loop/decide_react` 的 `Missing 'response' argument` 检查 → 失去最后防线
- (c) 改 inja strict mode → 全项目级, 可能破坏 245 现有测试, scope 过大
- (d) ProviderLLMTool 仅防御纵深 → 不能修 node_executor 的盲区, 治标

### D2: ProviderLLMTool 防御纵深（VERIFIED — whitespace-only 覆盖，含 SHIP-with-fixes 修正）

**决策**: 在 `pdk_entry.cpp:63-68` 现有 `out.text.empty()` 检查基础上扩展 whitespace-only 判定:

```cpp
// Before:
if (out.text.empty()) {
    throw std::runtime_error("ProviderLLMTool: LLM call succeeded but returned empty text...");
}

// After (V2 — SHIP-with-fixes corrected: provider_name_ 不存在, 改用 this->name()):
bool is_empty_text =
    out.text.empty() ||
    std::all_of(out.text.begin(), out.text.end(),
                [](unsigned char c){ return std::isspace(c); });
if (is_empty_text) {
    throw std::runtime_error(
        "ProviderLLMTool: LLM returned empty/whitespace text. "
        "Provider=" + this->name() +  // ILLMTool::name() 返回 "loop-agent-provider-bridge"
        ", model=" + req.params.model + ". "
        "Check provider model availability or response format.");
}
```

**Rationale**:
- ✅ D1 已在 node_executor 层做完整检查, ProviderLLMTool 是 defense-in-depth（第 1 道 vs 第 2 道）
- ✅ ProviderLLMTool 是 loop_agent 专属 (per `pdk/loop_agent/src/pdk_entry.cpp:43-82` 类定义, 仅 `pdk/loop_agent` 一个 plugin 使用), 其他 PDK 用 ILLMProvider 接口, 改动作用域封闭
- ✅ 错误信息含 provider name (使用 `this->name()` 即 ILLMTool::name() 返回 "loop-agent-provider-bridge") + model name (`req.params.model`)
- ✅ **修正后**：移除虚构的 `provider_name_` 字段（ProviderLLMTool 类实际不存在此成员，ILLMProvider 接口也无 name() 方法），改用既有 `this->name()`

**NOT chosen alternatives**:
- (a) 移除 ProviderLLMTool 的检查 → 失去 defense-in-depth
- (b) ProviderLLMTool 也做 null/non-string 检查 → 已包含在 result.has_value() 早返回, 不重复

**⚠️ Side effect (per Metis 审查)**: D2 改动会**破坏** `tests/test_provider_llm_tool_empty.cpp` Case 3 source guard（检查 `"ProviderLLMTool: LLM call succeeded but returned empty text"` 签名）+ replica 逻辑 `provider_llm_tool_generate_replica`。本 change 必须**同步**更新 source guard 签名 + replica 实现（见 Implementation Summary 第 7 文件）。

### D3: test_e2e_real_llm 直接运行可移植性（VERIFIED — dynamic find_plugin_dir + setenv，含 SHIP-with-fixes 修正）

**决策**: 在 `examples/pdk_chat_demo/tests/test_e2e_real_llm.cpp` 复用项目既有 `find_loop_dir()` 动态查找模式, 引入 `find_plugin_dir()` helper, setenv 时使用动态计算的 build/pdk 路径:

```cpp
// At top of test_e2e_real_llm.cpp (复用既有模式):
namespace {
// 复用既有 find_loop_dir() 模式 (test_e2e_real_llm.cpp:48-60), 引入 find_plugin_dir():
std::string find_plugin_dir() {
    // HYDRAFORGE_PLUGIN_PATH 已设置 → 直接使用 (test 优先于 default)
    if (const char* env = std::getenv("HYDRAFORGE_PLUGIN_PATH")) {
        std::string s(env);
        size_t colon = s.find(':');
        return colon == std::string::npos ? s : s.substr(0, colon);
    }
    // 否则从 current_path 向上搜 build/pdk
    for (auto p = fs::current_path(); p != p.root_path(); p = p.parent_path()) {
        auto candidate = p / "build" / "pdk";
        if (fs::exists(candidate)) return candidate.string();
    }
    throw std::runtime_error("Plugin dir not found and HYDRAFORGE_PLUGIN_PATH not set");
}

struct PluginPathSetter {
    PluginPathSetter() {
        if (std::getenv("HYDRAFORGE_PLUGIN_PATH") == nullptr) {
            setenv("HYDRAFORGE_PLUGIN_PATH", find_plugin_dir().c_str(), 1);
        }
    }
} plugin_path_setter;
}  // namespace
```

**Rationale**:
- ✅ 与项目既有模式一致 (`find_loop_dir()` 从 cwd 向上搜 `lib/loop`, test_loop_agent_autonomous.cpp 的 LOOP_AGENT_SO_PATH 查找模式)
- ✅ **修正后**：跨环境可移植（任何 clone 路径 `/home/user/work/HydraForge/build/pdk` 都能找到），**不再 hard-coded** `/workspace/project/HydraForge/build`
- ✅ 直接运行 `./test_e2e_real_llm` 不撞白名单墙
- ✅ 已有 env var 时不覆盖（ctest mode 优先）
- ✅ 与 CMake `set_tests_properties(... ENVIRONMENT ...)` (CMakeLists.txt:349) 保持一致（PROJECT_BINARY_DIR 在子目录 = 顶层 build dir）

**NOT chosen alternatives**:
- (a) 仅修 CMake 不修测试 → 直接运行继续失败
- (b) 硬编码 `/workspace/project/HydraForge/build/pdk` → **违反用户"env 可移植性"隐含需求**（SHOWN BY METIS REVIEW）
- (c) 移除白名单 → 违反 ADR-0022 §5.1 Layer 1 安全

### D4: regression guard test（VERIFIED — Mock 注入 3 类场景）

**决策**: 新增 `tests/test_node_executor_empty_response.cpp` 独立 binary, 覆盖 3 类空响应:

```cpp
// 3 cases:
// Case 1: null response (ProviderLLMTool 返回 Result 失败, 但 LLMResult.text 字段为 null JSON)
// Case 2: whitespace-only response (" " / "\n")
// Case 3: empty string response (F1 现有覆盖, regression guard)
```

**Rationale**:
- ✅ Mock 注入, 不依赖真实 LLM / API key
- ✅ 沿用 F1 的 test pattern (test_dsl_engine_ctx_bridge.cpp Case 1-5)
- ✅ 独立 binary (per tests/AGENTS.md Pattern #3 helper 三态分离)
- ✅ 不加 `[must_realllm]` tag (mock only, 不消耗 token)
- ✅ focused ctest 验证 3/3 PASS

**NOT chosen alternatives**:
- (a) 加 `[must_realllm]` tag → mock 已覆盖, 真实 LLM 重复验证 ROI 低
- (b) 与 test_dsl_engine_ctx_bridge 合并 → 关注点不同, 独立 binary 更清晰

### D5: 审计报告修正（VERIFIED — 误导标签修正）

**决策**: 更新 `docs/audits/2026-09-29-harness-self-evolution-rsi-audit.md:443` 标签:

```diff
-| **ChatSession e2e DeepSeek** | 端到端 chat + multi-turn + GenerateSubGraph + errors | **test_e2e_real_llm* 系列: 7/8 cases** (1 pre-existing PluginLoader whitelist 失败, 与本改动无关) | **[must_realllm]** |
+| **ChatSession e2e DeepSeek** | 端到端 chat + multi-turn + GenerateSubGraph + errors | **test_e2e_real_llm* 系列: 7/8 cases** (1 pre-existing F1 V2 residual — ChatSession e2e `Missing 'response' argument`, 已 ship in 2026-09-30 `fix-chatsession-empty-llm-response`) | **[must_realllm]** |
```

**Rationale**:
- ✅ 修正误导性描述（"PluginLoader whitelist" → "F1 V2 residual"）
- ✅ 引用本 change 归档路径（trace 追溯）
- ✅ 一次性 audit 同步, 后续不再有 misleading label 误导读者

### D6: ship commit 与 24h cooling-off（VERIFIED — atomic + governance）

**决策**:
- 5 个改动 → 1 个 atomic commit（per AGENTS.md 模式 #4 SHIP-with-fixes 流程）
- 不拆 commit（避免原子性破坏）
- 触发 24h cooling-off（per AGENTS.md 治理链）
- OpenSpec change 归档到 `openspec/changes/archive/2026-09-30-fix-chatsession-empty-llm-response/` (Day-5 4-file integrity verified)

## Implementation Summary

| File | Lines | Change |
|------|-------|--------|
| `src/modules/executor/node_executor.cpp` | ~15 lines | 扩展 main path (L204-214) + stream path (L147-156) 空 text 检查为 null / empty / whitespace 3 类（SHIP-with-fixes 修正：null 检查前移到 `get<std::string>()` 之前） |
| `pdk/loop_agent/src/pdk_entry.cpp` | ~7 lines | ProviderLLMTool (L63-68) 空 text 检查扩展 whitespace（SHIP-with-fixes 修正：改用 `this->name()` 而非虚构的 `provider_name_`） |
| `tests/test_provider_llm_tool_empty.cpp` | ~10 lines (sync) | **SHIP-with-fixes 新增**：Case 3 source guard 签名更新 + replica 逻辑同步扩展 whitespace |
| `examples/pdk_chat_demo/tests/test_e2e_real_llm.cpp` | ~15 lines | 顶部 namespace + `find_plugin_dir()` (复用 find_loop_dir 动态模式) + PluginPathSetter 注入 HYDRAFORGE_PLUGIN_PATH（**SHIP-with-fixes 修正**：从 hard-coded 改为动态查找） |
| `tests/test_node_executor_empty_response.cpp` | ~80 lines (new) | 3 cases regression guard (null / whitespace / empty string) |
| `openspec/specs/react-agent-llm-ctx-bridge/spec.md` | ~10 lines | R3 amendment: "empty string" → "null / whitespace / empty string" |
| `docs/audits/2026-09-29-harness-self-evolution-rsi-audit.md` | 1 line | §6.1 line 443 误导标签修正 |
| `AGENTS.md` | ~5 lines | Recent Changes entry + FULL REGRESSION TEST FLOW §2 预期 9/10 → 10/10 + a21c92e 历史条目勘误注释 |
| `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` | ~3 lines | §1.4 Bug 3 残量风险追加 V2 修复记录 |

**Total**: 9 files (7 production + 2 docs/spec), 1 atomic commit per AGENTS.md 模式 #4 SHIP-with-fixes 流程.

## Open Questions (post-Oracle+Metis review)

- Q1 ✅ (D1): null / whitespace 检查是否过度？**RESOLVED** — Oracle 审查确认真正 silent 仅 whitespace；null/non-string 已抛 type_error（V2 修复改为友好消息）
- Q2 ✅ (D2): ProviderLLMTool 改动作用域？**RESOLVED** — Oracle + Metis 确认 loop_agent 唯一；**NEW**: D2 改动破坏 test_provider_llm_tool_empty.cpp source guard + replica（已加入 Implementation Summary 第 3 文件）
- Q3 (D4): regression guard test 是否需要 `[must_realllm]` tag？**仍 OPEN** — mock only 测试, CI 无需 key, 但需 Oracle 验证 CI skip 行为
- Q4 (D3 hard-code vs dynamic): **RESOLVED via SHIP-with-fixes** — 改用 `find_plugin_dir()` 动态查找复用 find_loop_dir 模式
- Q5 (5-in-1 commit): **DECIDED** — 保持 1 atomic commit (per AGENTS.md 模式 #4 原子性); 7 production + 2 docs 同 commit 是 Single-Dev 可接受的 scope

## Cross-References

- F1 archived change: `openspec/changes/archive/2026-09-18-fix-react-decide-empty-response/`
- chat-real-llm-coverage Phase H follow-up: closed by this change
- Audit report: `docs/audits/2026-09-29-harness-self-evolution-rsi-audit.md`
- Roadmap tracking: `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md §1.4 Bug 3 残量风险`