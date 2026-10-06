## Context

`test_e2e_real_llm` ChatSession case 当前测试状态（per `fix-lib-loop-args-parsing` commit `46a3958` ship 报告）:

```
test_e2e_real_llm ChatSession case 仍 FAIL
错误: "Tool '{{decision.action_tool}}' not registered for node: /main/act"
```

**根因**: act 节点的 `tool:` 字段是模板字符串（`{{decision.action_tool}}`），`arguments:` 是单字符串值（`{{decision.action_args}}`）。两者都依赖执行时**模板渲染** + **动态分发**，但当前实现:

1. **Parser**: `make_tool_call` 仅读 `arguments: is_object()`，string value 静默忽略；`tool:` 直接存 literal 不解析模板。
2. **Executor**: `execute_tool_call` 用 `tool_registry_.has_tool(node->tool_name)` 直查 registry，**不渲染模板**。

`lib/loop/react.agent.md:30-35` act 节点设计意图:

```yaml
- id: act
  type: tool_call
  tool: "{{decision.action_tool}}"        # execute-time render
  arguments: "{{decision.action_args}}"  # execute-time render, single-arg
```

`parse_react_decision` 输出 (pdk_entry.cpp:167-248):
- L1 (OpenAI function_call JSON): `action_tool="fs/read"`, `action_args={"path": "..."}`
- L1 (tool+args): `action_tool="fs/read"`, `action_args={"input": "..."}` (line 198 wrapper)
- L2 (XML): `action_tool="fs/read"`, `action_args={"path": "..."}` 或 wrapper
- L3 (Final Answer): `final=true` → end 节点（不会到 act）

## Root Cause (CONFIRMED)

**Two-layer dispatch pattern missing**:
- Layer 1: `tool:` field template render (decision.action_tool → "fs/read" / "finish" / etc.)
- Layer 2: `arguments:` field value render (decision.action_args → JSON string)

Current code (per `src/modules/parser/node_factory.cpp:171-189` + `src/modules/executor/node_executor.cpp:252-301`):
- Parser: literal storage
- Executor: literal registry lookup, no template render on `tool_name`

**Empirical verification**: parse `react.agent.md` → act node `tool_name = "{{decision.action_tool}}"` (literal), `arguments = {}` (empty, string value ignored). Execute → `has_tool("{{decision.action_tool}}") = false` → "not registered" error.

## Goals / Non-Goals

**Goals**:
- ✅ `make_tool_call` 接受 `arguments: "<string>"` 形式 (wrap as `{"input": "<value>"}`)
- ✅ `execute_tool_call` 在 main path + stream path 对 `node->tool_name` 调用 `InjaTemplateRenderer::render()`
- ✅ 新增 `tests/test_parser_template_args.cpp` regression guard
- ✅ 验证 `test_e2e_real_llm` 4/4 PASS
- ✅ `docs/specs/dsl.md §5.2` 扩展 string arguments 契约
- ✅ 24h cooling-off

**Non-goals**:
- 不重写 `parse_react_decision` fallback 链
- 不扩展 `assign` / `llm_call` 等其他节点类型
- 不实现 string-template 与 object 混编（如 `key1: "a", key2: "{{b}}"`）
- 不动 `chat_session.cpp` 或 `pdk_chat_demo/tests/` (本 change 是 parser+executor 修复)

## Decisions

### D1: Parser-side: string value arguments → wrap as `{"input": "<value>"}`

**决策**: 在 `make_tool_call` (node_factory.cpp:171-189) 增加:
```cpp
if (j.contains("arguments") && j["arguments"].is_string()) {
  args["input"] = j["arguments"].get<std::string>();
} else if (j.contains("arguments") && j["arguments"].is_object()) {
  // 既有路径 (per 46a3958 ship 的 canonical `arguments:`)
}
```

**Rationale**:
- ✅ 与 `parse_react_decision` line 198 + 219 fallback 对齐 (`{"input", args_str}`)
- ✅ 单参数模式最常见（action_args 通常是一个字符串 input）
- ✅ Executor 端 args values 已有 `InjaTemplateRenderer::render()` (node_executor.cpp:291)
- ✅ 零 schema drift

### D2: Executor-side: tool_name template render before registry lookup

**决策**: 在 `execute_tool_call` (node_executor.cpp:259-285 + 288-298) 增加:
```cpp
std::string rendered_tool_name = InjaTemplateRenderer::render(node->tool_name, ctx);
if (!tool_registry_.has_tool(rendered_tool_name)) {
  throw std::runtime_error("Tool '" + rendered_tool_name + "' not registered for node: " + node->path);
}
auto [tool_result, new_context] = dispatch_to_tool(rendered_tool_name, node->path, rendered_args);
```

**Rationale**:
- ✅ 与既有 args values 渲染模式一致（InjaTemplateRenderer + ctx）
- ✅ 透明支持 `tool:` 字面值 + `{{...}}` 模板值（向后兼容）
- ✅ main path + stream path 同步修，避免两条路径行为不一致

### D3: 测试用例设计

**新文件**: `tests/test_parser_template_args.cpp`

**Cases**:
1. **Case 1 (parser string arguments wrap)**: 解析 `arguments: "{{x.y}}"` 形式 → ToolCallNode.arguments 含 `{"input": "{{x.y}}"}` key
2. **Case 2 (parser object arguments preserved)**: 解析既有 object 形式 → 行为不变（防回归）
3. **Case 3 (executor tool_name render)**: 构造 Context 注入 `decision.action_tool = "finish"`，mock registry 注册 `finish` 工具 → execute_tool_call 返回成功
4. **Case 4 (executor tool_name literal)**: 解析 `tool: "loop/decide_react"` literal → 渲染后仍 = "loop/decide_react"（向后兼容验证）
5. **Case 5 (RED 验证)**: 临时还原修改 → test fail (守卫根因)

### D4: 同步治理

- `AGENTS.md` Recent Changes entry (本 change ship 记录)
- `docs/audits/2026-09-29-harness-self-evolution-rsi-audit.md:443` ChatSession 状态更新
- `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md §1.4` 新增 Bug 4 记录（与 Bug 3 平级）
- `docs/specs/dsl.md §5.2` 扩展 string arguments 契约 + `tool` 模板渲染契约
- `.rddf/improvements/parser-string-template-args.md` archived (本 change 关闭该 improvement)

### D5: 保留范围

**不动**:
- `parse_react_decision` (pdk_entry.cpp:167-248) L1/L2/L3 fallback
- `chat_session.cpp` (消费者)
- `lib/loop/*.agent.md` (已对齐 per 46a3958)
- `loop/decide_react` 注册逻辑
- 其他 8 个 `lib/*` DSL 文件（不需改）

## Risks & Mitigations

| 风险 | 缓解 |
|------|------|
| Executor 渲染 tool_name 引入新 race (template 渲染依赖 ctx) | `InjaTemplateRenderer::render` 是 static 方法无 race (per `template_renderer.h:18`) |
| `tool: "{{decision.action_tool}}"` 错误渲染为 literal "{{decision.action_tool}}" (模板 syntax 错) | 既有 Inja 容错: render 失败时返回原 string，抛 "not registered" 错误（同既有错误路径） |
| string arguments wrap 可能破坏既有 object 调用方 | Case 2 (object preserved) 显式验证既有路径行为不变 |
| 本 change 间接影响其他 8 个 lib/* 文件 (template render 可能误渲染字面值) | 既有 lib/* DSL 全用 `arguments: <object>` 形式（per 46a3958 ship 验证），本 change 仅新增 string value 路径，object 路径零修改 |

## Verification Plan

### Local verification (核心 + examples)

```bash
cmake --build build -j$(nproc)  # rebuild test_parser_template_args + test_e2e_real_llm
ctest --test-dir build -R "test_parser_template_args" --output-on-failure  # 3-4 cases PASS
ctest --test-dir build/examples/pdk_chat_demo/tests -L must_realllm -R "^test_e2e_real_llm$" --output-on-failure  # 4 cases PASS
ctest --test-dir build -LE must_realllm  # 262+/262+ PASS (零回归)
ctest --test-dir build/examples/pdk_chat_demo/tests -LE must_realllm  # 33/33 PASS (零回归)
```

### Source-level verification

- `grep -n "is_string()\|render" src/modules/parser/node_factory.cpp src/modules/executor/node_executor.cpp` — 验证两处扩展
- `git diff --stat` — 7 files (3 production + 1 test + 4 spec/docs/AGENTS.md)

### Documentation verification

- `openspec validate openspec/changes/2026-09-30-fix-parser-template-args` exit 0
- `git show HEAD` — Reverse Indicator 段 5 字段完整