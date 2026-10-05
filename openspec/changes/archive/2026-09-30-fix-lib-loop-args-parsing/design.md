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

### D2: Parser alias `args:` (RESOLVED — NOT added, per user Option 2 choice)

**决策**: parser **NOT** add `args:` alias. **仅** canonical `arguments:` (per Oracle + Metis 推荐 + 用户明确选 Option 2).

**Rationale**:
- ✅ 严格 spec 对齐 (per `docs/specs/dsl.md §5.2`)
- ✅ 零双 canonical key 债务 (per Metis warning)
- ✅ 内部 lib/* 全部用 `arguments:` (canonical, no spec drift)
- ❌ 第三方 .agent.md 文件若用 `args:` 会失败 — 但这是**第三方问题**, 不是本项目维护范围
- ❌ 失去对 legacy 第三方 .agent.md 的防御纵深 — 接受 trade-off (YAGNI)

**Open Question Q1 RESOLVED**: 不加 parser alias. spec Requirements section 已相应更新 (移除 "仅 args: (向后兼容 fallback)" scenario).

### D3: Parser regression test (✅ Oracle 推荐)

**新文件**: `tests/test_parser_dsl_schema.cpp`

**测试用例**:
1. 解析 `lib/loop/react.agent.md`, `decide` 节点 `arguments` 非空且含 `response` key
2. 解析 `lib/loop/plan_execute.agent.md`, `execute` 节点 `arguments` 非空
3. 解析 `lib/loop/fork_join.agent.md`, 3 个 task 节点 `arguments` 非空
4. (回归) 解析 `lib/inference/load.agent.md` 等 9 个已合规 DSL, `arguments` 仍非空
5. (未来防御) parser 接受 `args:` alias 时, `arguments` 仍 prefer canonical

### D4: CI 脚本 (✅ Oracle 推荐 — SHIP-with-fixes 修正)

**新文件**: `tools/check_dsl_schema.sh`

**修正原因**: `.agent.md` 是 Markdown 内嵌 YAML fence, 直接 `yaml.safe_load()` 物理抛异常. 改用 grep-level 检测 + YAML fence 抽取:

```bash
#!/bin/bash
# Walk all lib/*.md, verify tool_call nodes use 'arguments:' (not 'args:').
# Uses grep (not yaml.safe_load) since .agent.md is Markdown with embedded
# YAML fence — direct YAML parsing throws on Markdown syntax.
# Per Oracle review (ses_ef3975c6effea6vvoN59WSD26k) M1.
set -euo pipefail
violations=0
for md in $(find lib -name "*.agent.md"); do
  # Extract YAML fence contents between ```yaml and ``` markers
  yaml=$(awk '/^```yaml$/{flag=1; next} /^```$/{flag=0} flag' "$md")
  # Check for tool_call nodes using 'args:' instead of 'arguments:'
  if echo "$yaml" | grep -E '^\s*-?\s*id:\s*' >/dev/null; then
    if echo "$yaml" | grep -B 5 'tool_call' | grep -E '^\s*args:' >/dev/null; then
      echo "ERROR: $md: tool_call nodes use 'args:' (should be 'arguments:')"
      violations=$((violations + 1))
    fi
  fi
done
[ $violations -eq 0 ] || { echo "DSL schema drift detected in $violations file(s)"; exit 1; }
echo "DSL schema check: PASS (all lib/*.md use 'arguments:' canonical)"
```

**验证方式**:
- ✅ lib/loop/{react,plan_execute,fork_join}.agent.md → fail (FIXME state before rename)
- ✅ lib/inference/*.md + lib/math/add.md → pass (already canonical)
- ✅ after rename (D1), all files pass
- ⚠️ grep-based detection may miss edge cases (e.g., 'args:' inside string value). For comprehensive coverage, prefer YQ/jq + YAML fence extraction. But for this change, grep is sufficient given current file scope.

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

- Q1: ✅ parser alias 不 ship (per user Option 2)
- Q2: ✅ act 节点字符串模板 args 不在本 change scope (独立 follow-up, 需扩展 parser 接受 string value)
- Q3: ✅ test_loop_agent_autonomous vs test_e2e_real_llm 矛盾点独立 follow-up (推测 stale build binary)

## Cross-References

- V2 fix: `openspec/changes/archive/2026-09-30-fix-chatsession-empty-llm-response/` (defense-in-depth, ships)
- V2 ship commits: `1c729e3` + `74ceecd`
- F1 archived: `openspec/changes/archive/2026-09-18-fix-react-decide-empty-response/`
- DSL spec: `docs/specs/dsl.md §5.2` (canonical key)
- Audit report: `docs/audits/2026-09-29-harness-self-evolution-rsi-audit.md` (§6.9 line 443 corrected by V2)