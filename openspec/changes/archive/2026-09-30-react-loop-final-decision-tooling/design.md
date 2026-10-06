## Context

`test_e2e_real_llm` ChatSession case 当前测试状态（per Oracle dual-agent review 2026-09-30）：

```
test_e2e_real_llm ChatSession case 仍 FAIL
错误: "Template render error: variable 'decision.action_args' not found"
```

**根因**: act 节点（`lib/loop/react.agent.md:30-35`）无条件执行，即使 `decision.final=true`（LLM 直接回答）也尝试渲染空 `decision.action_args`。`parse_react_decision` L3 fallback（`pdk_entry.cpp:230-247`）返回 `action_tool=""` + `action_args=null`，但 `pdk_entry.cpp:114-117` 注释明确写了"act node, typically 'finish'"——**L3 偏离原始设计意图**。

## Root Cause (CONFIRMED)

### 当前行为（pre-fix）

```yaml
# lib/loop/react.agent.md:30-35 (不变)
- id: act
  type: tool_call
  tool: "{{decision.action_tool}}"        # render: "" (空字符串)
  arguments: "{{decision.action_args}}"  # render: null → 抛 Template render error
  output_keys: [tool_result]
  next: [/main/observe]
```

```cpp
// pdk_entry.cpp:230-247 L3 fallback (pre-fix)
out["final"] = true;
out["action_tool"] = "";      // ← 偏离设计意图（注释写"typically finish"）
out["action_args"] = nullptr; // ← 触发 render 失败
out["response"] = final_text;
```

```cpp
// pdk_entry.cpp:152-160 finish tool (pre-fix)
auto it = args.find("answer");
std::string answer = (it != args.end()) ? it->second : "Task complete";
// ↑ 仅读 "answer"；L3 final_text 走 action_args → args["input"]，finish 忽略 "input"，返回 "Task complete"
// ↑ 真实答案文本丢失（即使修复 action_tool="finish" 后）
```

### 修复后行为（per Oracle Path D）

```yaml
# lib/loop/react.agent.md:30-35 (不变 — act 节点本身无问题)
# act 节点调 finish → observe → end 完整走通
```

```cpp
// pdk_entry.cpp:242-247 L3 fallback (post-fix, 3 lines change)
out["final"] = true;
out["action_tool"] = "finish";    // 改: 恢复设计意图
out["action_args"] = final_text;  // 改: JSON string，避免双重包装
out["response"] = final_text;
```

```cpp
// pdk_entry.cpp:152-160 finish tool (post-fix, 2 lines change)
auto it = args.find("answer");
std::string answer;
if (it != args.end() && !it->second.empty()) {
  answer = it->second;
} else if (auto input_it = args.find("input"); input_it != args.end() && !input_it->second.empty()) {
  answer = input_it->second;       // 改: 兜底读 input（action_args 渲染结果）
} else {
  answer = "Task complete";
}
// ↑ 三级取参：answer → 非空 input → "Task complete"
```

```cpp
// pdk_entry.cpp:772-784 loop/run response extraction (post-fix, ~4 lines)
std::string response_text;
for (const char* k : {"response", "output", "llm_response", "plan_response", "final_result"}) {
  auto v = result.final_context.find(k);
  if (v != result.final_context.end() && v->is_string() && !v->get<std::string>().empty()) {
    response_text = v->get<std::string>();
    break;
  }
}
// 改: 优先用 decision.response（当存在且非空），让 ChatSession 拿到 parsed final text
if (response_text.empty()) {
  auto decision_v = result.final_context.find("decision");
  if (decision_v != result.final_context.end() && decision_v->is_object()) {
    auto resp_v = decision_v->find("response");
    if (resp_v != decision_v->end() && resp_v->is_string() && !resp_v->get<std::string>().empty()) {
      response_text = resp_v->get<std::string>();
    }
  }
}
if (response_text.empty()) response_text = result.message;
```

## Goals / Non-Goals

**Goals**:
- ✅ L3 fallback 返回 `finish` + final_text (string)
- ✅ finish 工具读 `input` 兜底
- ✅ loop/run response 提取优先 `decision.response`
- ✅ test_loop_agent_plugin.cpp:253-254 2 条断言更新
- ✅ 新增 1 个 L3 final e2e case
- ✅ test_e2e_real_llm ChatSession case PASS
- ✅ test_e2e_real_llm 4/4 cases PASS
- ✅ core tree 263+/263+ PASS + examples tree 33/33 PASS
- ✅ 24h cooling-off

**Non-goals**:
- ❌ 不实现 DSL CONDITION 节点（Path A）— 登记 follow-up
- ❌ 不实现 executor skip 机制（Path B）— 违反 fail-fast
- ❌ 不实现 skip_when 元数据（Path C）— 投入产出比不划算
- ❌ 不修复 plan_execute / fork_join loops — 无同类问题
- ❌ 不改 react.agent.md / chat_session.cpp / executor / parser — 核心架构零改动

## Decisions

### D1: Path D vs Path A/B/C（Oracle 推荐 Path D）

**决策**: Path D（核心）+ 2 个闭环补丁（必须）+ 1 个可选 polish。

**Rationale**:
- ✅ 最小代码改动（~10 行）恢复原始设计意图
- ✅ 不破坏 fail-fast 契约（Path B 的核心缺陷）
- ✅ 不引入新 NodeType 触发全 parser/scheduler 回归（Path A 的核心缺陷）
- ✅ 不为单消费者扩展 DSL 元数据契约（Path C 的核心缺陷）
- ✅ plan_execute / fork_join loops **无需同步修复**（已 read 验证无模板 tool）

### D2: `action_args` 用 string 而非 `{"input": final_text}` object

**决策**: `action_args = final_text` (JSON string)。

**Rationale**:
- parser (node_factory.cpp:177-178) 把 `arguments: "{{decision.action_args}}"` 包装为 `args["input"] = <模板>`
- 若 `action_args` 是 object，inja 渲染成 JSON dump → `args["input"] = "{\"input\":\"...\"}"` 双重包装
- 若 `action_args` 是 string，inja 渲染 raw string → `args["input"] = final_text`，干净
- finish 读 `input` 即可获得 final_text（无双重解析）

### D3: finish 工具读 `input` 兜底（必须）

**决策**: finish lambda 改为 `answer` → 非空 `input` → `"Task complete"` 三级取参。

**Rationale**:
- 现状只读 `answer` → L3 场景真实答案文本丢失
- act 节点把 `decision.action_args`（string）渲染到 `args["input"]`，finish 读 `input` 是最自然的路径
- 现有 `answer` 参数路径保持向后兼容（不变）

### D4: loop/run response 优先 `decision.response`（可选）

**决策**: 在现有提取链之后加 `decision.response` 优先逻辑。

**Rationale**:
- 当前提取链搜 `{response, output, llm_response, plan_response, final_result}`，不读 `decision`
- final 场景下 ChatSession 拿到原始 LLM 文本（可能含 "Final Answer:" 前缀），不干净
- decision 是 JSON object 在 context 中（`process_output_keys` 存入 `output_keys[0]` = "decision"）
- 加 decision.response 优先 → ChatSession 拿 parsed final text
- 对 L1/L2 action 路径无行为变化（`decision.response` == 原始文本 == `llm_response`）

### D5: 测试更新策略

**决策**: `test_loop_agent_plugin.cpp:253-254` 2 条断言随生产改动同 commit 更新；新增 1 个 e2e case。

**Rationale**:
- AGENTS.md Pattern #4：原子 commit，不允许中间态红
- 既有 L1/L2 测试不受影响（final=false 路径不变）
- 新增 e2e case 必须测 L3 真实走通 react DSL 到 end（`result.ok == true, steps >= 4`）

### D6: 不动 react.agent.md / chat_session.cpp

**决策**: react.agent.md 保持不变（act 节点本身无问题，调 finish 是其设计意图）；chat_session.cpp 不感知 `decision.final`（已验证 chat_session.cpp:517-528 只读 `response/steps/tokens_used/cost_usd`）。

**Rationale**:
- act 节点的 `tool: "{{decision.action_tool}}"` 模板是 React 模式**核心抽象**（dynamic dispatch），不能删
- chat_session 是消费者层，不应感知 react loop 内部语义（分层架构）
- 修复完全在 pdk/loop_agent 内部，零架构改动

## Implementation Detail

### File Changes

**`pdk/loop_agent/src/pdk_entry.cpp`**:
- L242-247 (3 lines): L3 fallback 改 `""`→`"finish"`、`nullptr`→`final_text`
- L152-160 (2 lines): finish 工具增加 `input` 兜底
- L772-784 (~4 lines): loop/run response 提取优先 `decision.response`
- 总改动: ~10 行

**`examples/pdk_chat_demo/tests/test_loop_agent_plugin.cpp`**:
- L253-254 (2 assertions): 更新 L3 期望值
- 新增 1 个 TEST_CASE: L3 final answer 完整走通 react DSL 到 end
- 总改动: 3 行 assertions + ~30 行新 test case

**`openspec/specs/dsl.md §5.2`**:
- 扩展 react loop 终止路径契约说明
- 总改动: ~10 行 spec

**`AGENTS.md`**:
- Recent Changes entry + Reverse Indicator 5 字段
- 总改动: ~15 行

### Edge Cases

| Case | Behavior |
|---|---|
| LLM 返回 `"Final Answer: X"` | `final_text = "X"` → action_args="X" → args["input"]="X" → finish answer="X" ✅ |
| LLM 返回 `"Final Answer:"`（空） | `final_text = ""` → action_args="" → args["input"]="" → finish 跳过空 input → "Task complete" ⚠️ |
| LLM 返回自然语言无工具调用 | `final_text = response` → 同上 ✅ |
| LLM 返回 L1 JSON `{"name":"finish","arguments":{...}}` | `final=false, action_tool="finish"` → act 调 finish，args 由 L1 解析（不变） ✅ |
| LLM 返回 L1 JSON `{"name":"fs/read","arguments":{"path":"x"}}` | `final=false, action_tool="fs/read"` → act 调 fs/read（既有路径） ✅ |
| LLM 返回 whitespace-only response | F1 V2 fail-fast 在 think 节点拦截，到 decide 时必非空 ✅ |
| decision.action_args 是 null（L1/L2 失败兜底） | **不可达** — L1/L2 总是返回 object，且 final=false |

### Cross-Loop Verification

- ✅ `plan_execute.agent.md` 无模板 `tool:` 字段（plan→execute→verify→end 静态路径），无需同步
- ✅ `fork_join.agent.md` 无模板 `tool:` 字段（fork→tasks→join→synthesize 静态路径），无需同步
- 仅 `react.agent.md` 受影响（唯一 dynamic dispatch DSL）

## Risks & Mitigations

| 风险 | 缓解 |
|------|------|
| L3 final_text 为空导致 finish 默认返回 "Task complete"（答案丢失） | 接受 trade-off（罕见边缘 case）；finish 兜底返回非空保证 success |
| 既有测试依赖 `action_tool == ""` 失败 | 已有 test_loop_agent_plugin.cpp:253-254 2 条断言，本 commit 同步更新 |
| chat_session 拿到 raw LLM 文本而非 parsed final text | D4 优先 `decision.response` 兜底（可选 polish） |
| LLM 输 `{"final":true}` 伪造 final 标志 | **不可达** — `final` 字段由 parser 控制，LLM 无法伪造（per Oracle findings） |
| Path D 引入的新契约被未来 DSL 设计误用 | 在 `dsl.md §5.2` 明确"finish 作为 ReAct 终止语义"约定 |

## Verification Plan

### Local verification (核心 + examples)

```bash
cmake --build build -j$(nproc)  # rebuild test_loop_agent_plugin + test_e2e_real_llm
ctest --test-dir build -R "test_loop_agent_plugin" --output-on-failure  # 22+1 cases PASS
ctest --test-dir build/examples/pdk_chat_demo/tests -L must_realllm -R "^test_e2e_real_llm$" --output-on-failure  # 4 cases PASS
ctest --test-dir build -LE must_realllm  # 263+/263+ PASS (零回归)
ctest --test-dir build/examples/pdk_chat_demo/tests -LE must_realllm  # 33/33 PASS (零回归)
```

### Source-level verification

- `grep -n "action_tool\|action_args" pdk_entry.cpp` — 验证 L3 fallback 改 3 行
- `grep -n "answer\|input" pdk_entry.cpp` — 验证 finish 工具兜底 2 行
- `git diff --stat` — 预期 ~10 行 production + ~30 行 test + ~25 行 docs

### RED state verification

临时还原 `pdk_entry.cpp:244` 回 `action_tool = ""` → 新增 e2e case FAIL（"Tool '' not registered" 或 template render error）→ 恢复 → PASS。

### Documentation verification

- `openspec validate openspec/changes/2026-09-30-react-loop-final-decision-tooling` exit 0
- `git show HEAD` — Reverse Indicator 5 字段完整