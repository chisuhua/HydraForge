## Context

`test_e2e_real_llm` ChatSession case pre-existing failure (since 2026-09-18 F1 ship) 真正根因是 **DSL parser schema 不匹配**:

```yaml
# lib/loop/react.agent.md:26
- id: decide
  type: tool_call
  tool: loop/decide_react
  args:                            # ← parser 忽略这个 key
    response: "{{llm_response}}"
```

```cpp
// src/modules/parser/node_factory.cpp:176
if (j.contains("arguments") && j["arguments"].is_object()) {  // ← 只查 "arguments"
```

`docs/specs/dsl.md:377` 明确 canonical key 是 `arguments:` (per §5.2 `关键字段: tool, arguments, output_mapping`)。但 lib/loop 3 文件 6 处用 `args:` (从 `5a9ab2e` 2026-07-20 创建时引入, 人为因素 — 其他 9 个 lib DSL 文件全部合规)。

## Root Cause (CONFIRMED — Oracle `ses_ef4386f3dffeiO3OvtOOCogRYy` + Metis `ses_ef433ad8bffewxBioiEktPFcFM` dual-agent)

`make_tool_call` (src/modules/parser/node_factory.cpp:171-189) 严格只读 `arguments:` key。lib/loop/*.agent.md 用 `args:` → parser **静默丢弃** → `ToolCallNode.arguments` 为空 map → decide 节点 dispatch `loop/decide_react` 时 `args.find("response") == args.end()` → `error_result("InvalidParams", "Missing 'response' argument")` → handle_tool_errors → runtime_error → child->run FAIL。

**Empirical verification** (Metis 实证): parse react.agent.md directly → `decide` 节点 `args.size=0`。V2 fix (`fix-chatsession-empty-llm-response` ship at commit 1c729e3) 不修复此 bug，因为 LLM 返回 `"Hello there!"` 非空（V2 fix 不触发），但 parser 已丢弃 args → 失败链不变。

## Goals / Non-Goals

**Goals**:
- ✅ 修复 lib/loop 3 文件 `args:` → `arguments:` (spec 对齐)
- ✅ 可选防御纵深: parser alias `args:` / `arguments:` (向后兼容)
- ✅ 新增 parser regression test (防止未来 spec drift)
- ✅ 新增 CI 脚本 (tools/check_dsl_schema.sh)
- ✅ 验证 `test_e2e_real_llm` ChatSession case PASS (real DeepSeek)
- ✅ 24h cooling-off (per AGENTS.md 治理链)

**Non-Goals**:
- 不修复 react.agent.md:33 字符串模板值 args (独立第 2 潜伏 gap)
- 不重写 DSL parser 整体架构
- 不改 lib/inference 或 lib/math 等其他已合规 DSL 文件
- 不修复 test_loop_agent_autonomous vs test_e2e_real_llm 矛盾点

## Decisions

### D1: lib/loop/*.agent.md 文件改 `args:` → `arguments:` (✅ Oracle 推荐 Option 2)

**决策**: 3 文件 6 处 YAML key 改名 (零 parser 风险, spec-aligned).

**Rationale**:
- ✅ spec `docs/specs/dsl.md:377` 明确规定 `arguments:` canonical
- ✅ 与 lib/inference|math 既有一致 (其他 9 个 lib DSL 全部合规)
- ✅ 零 parser 风险, 零新代码路径
- ✅ 立即修 test_e2e_real_llm 真正根因

### D2: Parser alias `args:` (可选防御纵深 — VERIFIED per Oracle + Metis)

**决策**: parser 同时接受 `arguments:` 和 `args:` (向后兼容), 但 spec 主项仍是 `arguments:`。

```cpp
// src/modules/parser/node_factory.cpp:175-177
if ((j.contains("arguments") || j.contains("args")) &&
    (j.contains("arguments") ? j["arguments"].is_object() : j["args"].is_object())) {
  const auto& args_key = j.contains("arguments") ? "arguments" : "args";
  for (auto& [key, value] : j[args_key].items()) { ... }
}
```

**Rationale**:
- ✅ 防御纵深: 第三方 .agent.md 文件用 `args:` 仍能加载
- ✅ 内部 lib/* 全部用 `arguments:` (canonical, no spec drift)
- ⚠️ 制造双 canonical key 债务 (per Metis)

**NOT chosen alternatives**:
- (a) 只改 lib/loop (Option 2 only) — 严格按 Oracle + Metis 收敛推荐, 不加 alias, 纯 spec 对齐
- (b) 只改 parser (Option 1 only) — 不修 lib/loop, 违反 spec

### D3: Parser regression test (✅ Oracle 推荐)

**新文件**: `tests/test_parser_dsl_schema.cpp`

**测试用例**:
1. 解析 `lib/loop/react.agent.md`, `decide` 节点 `arguments` 非空且含 `response` key
2. 解析 `lib/loop/plan_execute.agent.md`, `execute` 节点 `arguments` 非空
3. 解析 `lib/loop/fork_join.agent.md`, 3 个 task 节点 `arguments` 非空
4. (回归) 解析 `lib/inference/load.agent.md` 等 9 个已合规 DSL, `arguments` 仍非空
5. (未来防御) parser 接受 `args:` alias 时, `arguments` 仍 prefer canonical

### D4: CI 脚本 (✅ Oracle 推荐)

**新文件**: `tools/check_dsl_schema.sh`

```bash
#!/bin/bash
# Walk all lib/*.md, verify tool_call nodes use 'arguments:' (not 'args:')
set -euo pipefail
for f in $(find lib -name "*.agent.md"); do
  python3 -c "
import yaml, sys
with open('$f') as fh:
    doc = yaml.safe_load(fh)
for node in (doc.get('nodes') or []):
    if node.get('type') == 'tool_call' and 'args' in node:
        print(f'ERROR: $f node={node[\"id\"]} uses args: should be arguments:')
        sys.exit(1)
"
done
```

## Implementation Summary

| File | Lines | Change |
|------|-------|--------|
| `lib/loop/react.agent.md` | line 26 | `args:` → `arguments:` (decide node) |
| `lib/loop/react.agent.md` | line 33 | `args:` → `arguments:` (act node, 仍不被 is_object 解析, follow-up) |
| `lib/loop/plan_execute.agent.md` | line 24 | `args:` → `arguments:` (execute node) |
| `lib/loop/fork_join.agent.md` | lines 22,30,38 | `args:` → `arguments:` (3 task nodes) |
| (可选) `src/modules/parser/node_factory.cpp` | line 175-177 | alias `args:` |
| `tests/test_parser_dsl_schema.cpp` | ~80 (new) | 5 cases regression guard |
| `tools/check_dsl_schema.sh` | ~30 (new) | CI validation script |
| `openspec/specs/dsl.md §5.2` | ~5 | 兼容性说明 (canonical `arguments:` + optional `args:` alias) |
| `AGENTS.md` | Recent Changes | entry + check_dsl_schema.sh reference |

## Open Questions

- Q1: parser alias (D2) 是否 ship? (Oracle 推荐不加, Metis 推荐加防御纵深)
- Q2: act 节点字符串模板 args (param fix) 是否在本 change scope? (独立 follow-up, 不阻塞)
- Q3: test_loop_agent_autonomous vs test_e2e_real_llm 矛盾点是否需深查? (Metis 推测 stale build, 独立 follow-up)

## Cross-References

- V2 fix: `openspec/changes/archive/2026-09-30-fix-chatsession-empty-llm-response/` (defense-in-depth, ships)
- V2 ship commits: `1c729e3` + `74ceecd`
- F1 archived: `openspec/changes/archive/2026-09-18-fix-react-decide-empty-response/`
- DSL spec: `docs/specs/dsl.md §5.2` (canonical key)
- Audit report: `docs/audits/2026-09-29-harness-self-evolution-rsi-audit.md` (§6.9 line 443 corrected by V2)