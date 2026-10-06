## Why

`test_e2e_real_llm` ChatSession case 自 2026-09-18 F1 ship 起一直 FAIL。前 3 个 ship 已修前 3 潜伏 gap（V2 fail-fast / parser rename / parser template args），错误现在锁定在 **react loop 流程控制缺 `decision.final` 路由**：

```
2026-09-30 ec92623 ship 后 test_e2e_real_llm ChatSession case 错误:
  "Template render error: variable 'decision.action_args' not found"
```

### 根因 (CONFIRMED — Oracle dual-agent review 2026-09-30)

`lib/loop/react.agent.md:30-35` act 节点无条件执行，即使 `decision.final=true`（LLM 直接回答而非工具调用）也尝试渲染 `tool: "{{decision.action_tool}}"` + `arguments: "{{decision.action_args}}"`。

`pdk/loop_agent/src/pdk_entry.cpp:230-247` `parse_react_decision` L3 fallback（自然语言无工具调用）返回：
```cpp
out["action_tool"] = "";        // ← 偏离设计意图
out["action_args"] = nullptr;   // ← 触发 react.agent.md:33 template render 失败
out["final"] = true;
```

但 `pdk_entry.cpp:114-117` 注释明确写道："react.agent.md references ... a dynamic `{{decision.action_tool}}` (act node, **typically 'finish'**)"——**原始设计意图就是 final 时走 `finish` 工具**，L3 返回 `""` 才是偏离设计的 bug。

### 4 候选路径评估（Oracle dual-agent review 收敛）

| Path | 描述 | Verdict |
|---|---|---|
| **A** | 新 `NodeType::CONDITION` + react.agent.md 拆条件分支 | 🔴 拒绝 — 新 NodeType 触发 parser/executor/scheduler 全套改动，blast radius 覆盖 263 个 core ctest，为单 must_realllm case 引入此风险不划算 |
| **B** | executor 跳过空 tool_name + chat_session 适配 | 🔴 拒绝 — 违反 AGENTS.md F1 V2 fail-fast 原则；且 act 跳过后 observe 节点 `{{tool_result}}` 渲染照样抛错，必须级联跳过 |
| **C** | act + observe 加 `skip_when` 元数据 | ⚠️ 中期候选 — 机制干净但为单消费者扩展 DSL 元数据契约（需同步 dsl.md §5 + audit 脚本） |
| **D** | L3 fallback 返回 `action_tool="finish"` + `action_args=final_text` + finish 工具读 input 兜底 | ✅ **推荐** — ~10 行代码改动，2 条测试断言更新，1 个新 e2e case；恢复原始设计意图 |

### Path D 的关键证据（Oracle 实证）

- `pdk_entry.cpp:114-117` 注释："dynamic `{{decision.action_tool}}` (act node, typically 'finish')"——**L3 fallback 返回 `""` 是偏离原始设计**
- `pdk_entry.cpp:141-161` `finish` 工具**已在 child registry 注册**（`register_react_support_tools_for_child` 包含），可直接被 act 节点调用
- `plan_execute.agent.md` / `fork_join.agent.md` **无任何模板 `tool:` 字段**（全部静态），无同类问题，无需同步修复

## What Changes

- **修复 1（核心）**：`pdk/loop_agent/src/pdk_entry.cpp:242-247` L3 fallback 改 3 行：
  - `action_tool` 从 `""` 改为 `"finish"`
  - `action_args` 从 `nullptr` 改为 `final_text`（JSON string 而非 object，避免双重包装）
  - `response` 字段保持 `final_text` 不变
- **修复 2（闭环）**：`pdk/loop_agent/src/pdk_entry.cpp:152-160` `finish` 工具 lambda 增加 `input` 兜底：`answer` → 非空 `input` → `"Task complete"` 三级取参
- **修复 3（可选 polish）**：`pdk/loop_agent/src/pdk_entry.cpp:772-784` `loop/run` response 提取优先 `decision.response`（当 `decision` 是 object 且含非空 string `response`），让 ChatSession 拿到 parsed final text 而非 raw LLM 输出
- **测试 1**：更新 `examples/pdk_chat_demo/tests/test_loop_agent_plugin.cpp:253-254` 2 条 L3 断言（`""` → `"finish"`，`is_null()` → `is_string() == final_text`）
- **测试 2（新增）**：`test_loop_agent_plugin.cpp` 加 1 个 e2e case — L3 final answer 完整走通 react DSL 到 end（断言 `result.ok == true, steps >= 4`）

## Capabilities

### Modified Capabilities
- `openspec/specs/dsl.md §5.2`: 明确 `tool_call` 节点在 `decision.final=true` 时的终止路径（act 节点调 `finish` 工具作为 ReAct 终止语义）

### New Capabilities
- `react-loop-decision-final-routing`: react loop 支持 `decision.final=true` 终止路径（复用既有 `finish` 工具作为 terminal action，不引入新 NodeType）

## Impact

**Production code (2 files)**:
- `pdk/loop_agent/src/pdk_entry.cpp` (L242-247 改 3 行 + L152-160 改 2 行 + L772-784 改 ~4 行)
- `pdk/loop_agent/src/pdk_entry.cpp` 总改动 ~10 行

**Test (1 file + 1 new test case)**:
- `examples/pdk_chat_demo/tests/test_loop_agent_plugin.cpp` (L253-254 2 条断言更新 + 1 个新 e2e case)

**Spec / docs (3 files)**:
- `openspec/specs/dsl.md §5.2` (扩 react loop 终止路径契约)
- `AGENTS.md` Recent Changes entry (含 Reverse Indicator 5 字段)
- `.rddf/improvements/react-loop-final-decision-tooling.md` 标记为 ✅ SHIPPED

**Total: 4 files (2 production + 1 test + 1 spec), 1 atomic commit** (per AGENTS.md Pattern #4 SHIP-with-fixes)

## Non-goals

- 不实现 DSL 条件节点（Path A）— 登记为独立 follow-up improvement，待第 2 个消费者出现时再立项
- 不实现 executor skip 机制（Path B）— 违反 fail-fast 原则
- 不实现 skip_when 元数据（Path C）— 为单消费者扩 DSL 表面不值得
- 不修复 plan_execute / fork_join loops — 无同类问题（已 read 验证）
- 不改 react.agent.md DSL 文件 — react.agent.md 自身不变（act 节点调 `finish` 是其设计意图）
- 不改 chat_session.cpp — 无需适配（Path B 的硬性约束在本方案下不成立）
- 不改 executor / parser / scheduler — 全程不动核心架构

## Acceptance Criteria

- [ ] `parse_react_decision` L3 fallback 返回 `action_tool="finish"` + `action_args=final_text`（JSON string）+ `final=true`
- [ ] `finish` 工具读 `args["answer"]` → 非空 `args["input"]` → `"Task complete"` 三级兜底
- [ ] `loop/run` response 提取优先 `decision.response`（当存在且非空）
- [ ] `test_loop_agent_plugin.cpp:253-254` 2 条断言更新（`""`→`"finish"`，`is_null()`→`is_string() == final_text`）
- [ ] 新增 `test_loop_agent_plugin.cpp` 1 个 e2e case — L3 final answer 完整走通 react DSL
- [ ] `test_e2e_real_llm` ChatSession case PASS (real DeepSeek)
- [ ] `test_e2e_real_llm` 4/4 cases PASS（零回归）
- [ ] core tree ctest `-LE must_realllm` 263+/263+ PASS（零回归）
- [ ] examples tree ctest `-LE must_realllm` 33/33 PASS（零回归）
- [ ] `openspec validate` PASS
- [ ] AGENTS.md Recent Changes entry + Reverse Indicator 5 字段

## Cooling-Off

Per AGENTS.md 治理链：24h cooling-off 触发。cooling_off_until = 2026-10-01T00:00:00Z (UTC+8 时区 2026-10-01 08:00)。

依赖 `ec92623` ship 的 cooling-off 独立计时 — 两者不重叠，本 change 不阻塞 next change 立项。