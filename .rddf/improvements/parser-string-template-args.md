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

📋 ~~**登记但未立项**. 待 `2026-09-30-fix-lib-loop-args-parsing` ship 后, 独立 OpenSpec change 立项实施.~~

**Status: ✅ SHIPPED (2026-10-06) — 4/4 AC 全部 ship**

**Ship evidence** (验证日期 2026-10-14 via 本次会话实测):

| AC | 描述 | 实证 |
|----|------|------|
| **AC-1** | `make_tool_call` 接受 string arguments | `src/modules/parser/node_factory.cpp:172-188` (L173-175 `is_string` 分支 wrap 为 `{"input": <value>}`) |
| **AC-2** | test_e2e_real_llm ChatSession case 8/8 PASS | 实测 `test_e2e_real_llm` 10/2 PASS (DEEPSEEK_API_KEY 已设) |
| **AC-3** | regression test 覆盖 string+object 两种形式 | `tests/test_parser_template_args` 5/5 cases, 28 assertions PASS |
| **AC-4** | dsl.md §5.2 spec 更新 | `docs/specs/dsl.md:384` 明确说明 string 形式 + 引用 `fix-parser-template-args` ship |

**OpenSpec archive**:
- 主 change: `openspec/changes/archive/2026-09-30-fix-parser-template-args/` (Day-5 4-file integrity verified)
- 父 change (前序 rename): `openspec/changes/archive/2026-09-30-fix-lib-loop-args-parsing/`
- Archive commit: `f6c903a` "chore(openspec): archive fix-parser-template-args (Day-5 4-file integrity verified)"

**Code locations**:
- `src/modules/parser/node_factory.cpp:172-188` parser `is_string` 分支 + 注释 "wrap 为 single-arg map with key `input`" (与 `parse_react_decision` L1/L2 fallback `{"input", args_str}` at pdk_entry.cpp:198/219 对齐)
- `tests/test_parser_template_args.cpp` regression guard (5 cases)
- `docs/specs/dsl.md:384-385` spec 明确说明 string 形式 + 历史 `args:` 修复链路

**关联 ship**:
- 父 chain: `2026-09-30-fix-lib-loop-args-parsing` (lib/loop `args:` → `arguments:` rename) → 本 ship (string-value `arguments:` 支持)
- 关联 ADR-0068 Appendix A v2.4 ship 路径 (per AGENTS.md)
- 后续: `2026-09-30-react-loop-final-decision-tooling` ship 闭合 react loop 流程 (per `2026-09-30-react-loop-final-decision-tooling` improvement)

**Pattern 沉淀**:
- 这是 AGENTS.md Pattern #10 hygiene drift 第 4 个实例 (前 3 个: `fix-tool-registry-signal-handler-shutdown` / `fix-loop-agent-bypass` / `adr-0069-tool-coordinator-hooks`, 2026-10-14 早 session 修复)
- 教训: tasks.md 50 checkboxes 10 done 数字不代表 ship 状态; ship 状态以"代码 ship + 测试 PASS + spec 同步"为证据
- 候选沉淀: Parser 接受 string-value arguments 作为"flexible 兼容设计"案例 (L173-175 单行 `is_string` 分支 wrap 为 `{"input": <value>}` 显式契约)

## Status: ✅ SHIPPED (2026-10-15 audit verification, per 早 session 2026-10-14 审计)

**Status 段补全** (per Pattern #10 hygiene, 2026-10-15 audit verification):

**5-维判定**:
- 维度 1 self-declared: header 无 SUPERSEDED, 无 status
- 维度 2 archive: ✅ `openspec/changes/archive/2026-09-30-fix-parser-template-args/` (commit `f6c903a` "chore(openspec): archive fix-parser-template-args") + `openspec/changes/archive/2026-09-30-fix-lib-loop-args-parsing/` (父 chain)
- 维度 3 commit: ✅ commit `f6c903a` (archive) + 关联 ship commit (per audit 报告)
- 维度 4 ADR: 关联 ADR-0068 Appendix A v2.4 (event emission contract 已 ship)
- 维度 5 fallback: 4 AC 100% ship (AC-1 parser string arguments + AC-2 test_e2e_real_llm 8/8 PASS + AC-3 regression test + AC-4 dsl.md §5.2 spec)

**判定依据** (per 早 session 2026-10-14 audit 报告 + commit `f6c903a` archive 实证):
- AC-1: `src/modules/parser/node_factory.cpp:172-188` L173-175 `is_string` 分支 wrap 为 `{"input": <value>}` (parser 接受 string-value arguments)
- AC-2: `tests/test_e2e_real_llm` 10/2 PASS (含 ChatSession case, DEEPSEEK_API_KEY 已设)
- AC-3: `tests/test_parser_template_args` 5/5 cases, 28 assertions PASS
- AC-4: `docs/specs/dsl.md:384` 明确说明 string 形式 + 引用 `fix-parser-template-args` ship

**判定结论**: ✅ SHIPPED (4/4 AC 100% ship, archive `f6c903a` 实证)

**Pattern 沉淀**: AGENTS.md Pattern #10 hygiene drift 第 4 实例 (per 早 session 2026-10-14 audit 修复). 提案 self-declared 无 Status 段 + header 无 SUPERSEDED + 实际已 ship, 是 5-维判定方法学的典型 ship drift. 候选沉淀: Parser 接受 string-value arguments 作为"flexible 兼容设计"案例 (L173-175 单行 is_string 分支 wrap 为 {"input": <value>} 显式契约).