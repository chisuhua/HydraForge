# Parser Extension for String-Template `args:` Values (DSL Tool Call)

## Context

Discovered during `2026-09-30-fix-lib-loop-args-parsing` (V2 fix ship + parser bug discovery). Per Oracle `ses_ef3975c6effea6vvoN59WSD26k` review (2026-09-30 inline execution):

> **第二潜伏 gap**: `react.agent.md:33` `args: "{{decision.action_args}}"` 是字符串模板值 (非 object), 即使改名为 `arguments:` 也会被 parser `is_object()` 检查跳过 — act 节点 args 永远为空.

## Why (独立 follow-up improvement)

`lib/loop/react.agent.md:33` 的 act 节点:
```yaml
- id: act
  type: tool_call
  tool: "{{decision.action_tool}}"
  args: "{{decision.action_args}}"   # ← 字符串模板值, 非 object
  output_keys: [tool_result]
```

当前 parser (`src/modules/parser/node_factory.cpp:171-189`) 仅 `is_object()` 检查 → 字符串值 args 被静默丢弃 → `ToolCallNode.arguments` 为空 map → act 节点 dispatch 时 `args["action_args"]` 缺失 → `loop/process_task` 收到空 args → 后续执行失败.

**实测确认**: 待实施 OpenSpec change `2026-09-30-fix-lib-loop-args-parsing` ship 后 (lib/loop `args:` → `arguments:` rename), act 节点仍会失败 (因为 `arguments: "{{decision.action_args}}"` 仍是字符串值).

## What Changes (proposal scope)

- **修复 1**: `src/modules/parser/node_factory.cpp` `make_tool_call` 扩展接受字符串值 (`j["arguments"].is_string()` 渲染为 single-arg map with key 默认 `"_"` 或 first available DSL field)
- **修复 2**: parser regression test (新增 case: `react.agent.md:33` act 节点 arguments 为字符串)
- **修复 3**: `lib/loop/react.agent.md:33` 验证 act 节点 end-to-end 真实 LLM 调用 (需要 DEEPSEEK_API_KEY)

## Out of Scope

- 不修 parser 的 inja 模板渲染 (template_renderer.cpp 已知正确, act 节点 args 是上层问题)
- 不改 react.agent.md schema (保留 string template 是合规 DSL 用法)

## Dependencies

- `2026-09-30-fix-lib-loop-args-parsing` (待 ship) — 此 improvement 是其 follow-up
- `docs/specs/dsl.md §5.2` (DSL canonical spec) — 需扩展说明 string-value arguments 用法

## Acceptance Criteria (建议)

- AC-1: `make_tool_call` 接受 `arguments: "string template"` 形式, 渲染为 single-arg dispatch
- AC-2: 真实 LLM 测试 `test_e2e_real_llm ChatSession case` 完整跑通 8/8 cases (含 act 节点)
- AC-3: regression test 覆盖字符串值 + object 值 两种形式
- AC-4: dsl.md §5.2 spec 更新说明 string-value 用法

## Estimated Effort

- Quick (1-2 hours): parser 扩展 + regression test + spec update
- E2E 真实 LLM 验证 (有 DEEPSEEK_API_KEY 时): 30 min

## Reference

- V2 fix ship: `openspec/changes/archive/2026-09-30-fix-chatsession-empty-llm-response/`
- Parser fix ship (pending): `openspec/changes/2026-09-30-fix-lib-loop-args-parsing/`
- Oracle review: `ses_ef3975c6effea6vvoN59WSD26k` M3 minor + 此 gap 登记
- Metis review: `ses_ef433ad8bffewxBioiEktPFcFM` 附带发现 #2

## Status

📋 **登记但未立项**. 待 `2026-09-30-fix-lib-loop-args-parsing` ship 后, 独立 OpenSpec change 立项实施.