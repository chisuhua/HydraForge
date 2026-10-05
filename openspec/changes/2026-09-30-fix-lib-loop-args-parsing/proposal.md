## Why

`test_e2e_real_llm` ChatSession case pre-existing failure (自 2026-09-18 F1 ship 起一直 FAIL) 真正根因是 **DSL parser schema 不匹配**：

- `src/modules/parser/node_factory.cpp:176` 的 `make_tool_call` 只读 `arguments:` key
- `lib/loop/{react,plan_execute,fork_join}.agent.md` 3 文件 6 处用 `args:` (从 `5a9ab2e` 2026-07-20 创建时引入，**人为因素** — 其他 9 个 lib DSL 文件全部用 `arguments:`)
- 结果: react loop 的 decide 节点 `args: {response: "{{llm_response}}"}` 被 parser **静默丢弃**，`ToolCallNode.arguments` 为空 map，`loop/decide_react` 收到空 args → `args.find("response") == args.end()` → `Missing 'response' argument`

**诊断时间线** (per Oracle `ses_ef4386f3dffeiO3OvtOOCogRYy` + Metis `ses_ef433ad8bffewxBioiEktPFcFM` dual-agent review, 2026-09-30):
- V2 fix ship (`fix-chatsession-empty-llm-response`, ship at 1c729e3 + 74ceecd) — defense-in-depth, 修复 whitespace silent pass (3 类空响应 fail-fast, **不是** test_e2e_real_llm 真正根因)
- Inline Execution 实测 V2 fix 后 test_e2e_real_llm ChatSession case **仍 FAIL** with "Missing 'response' argument"
- Debug 揭示真正根因: lib/loop 的 `args:` 被 parser 丢弃 (与 V2 fix 的 fail-fast 完全正交)
- Oracle + Metis 收敛信号: ship V2 fix 独立 (defense-in-depth 价值已验证) + 新 OpenSpec change 修 parser

**`docs/specs/dsl.md:377` 明确规定** `tool_call` 节点关键字段为 `tool`, `arguments`, `output_mapping` — canonical key 是 `arguments:`。lib/loop 3 文件违反此规范。

## What Changes

- **修复 1（spec 对齐）**: `lib/loop/{react,plan_execute,fork_join}.agent.md` 3 文件 `args:` → `arguments:` (3 处 file 6 处 YAML key 改名)
- **Options (向后兼容)** : `node_factory.cpp:176` 增加 `args:` alias 检测 (parser 同时接受 `arguments:` 和 `args:`，向后兼容已 ship 的 lib/loop 文件 + 第三方 .agent.md 文件)
- **修复 3（回归守卫）**: 新增 `tests/test_parser_dsl_schema.cpp` 解析所有 12 个 lib/*.md 文件并验证 `arguments` key 一致性（防止未来 spec drift）
- **修复 4（spec 审计）**: `tools/check_dsl_schema.sh` 脚本验证所有 .agent.md 文件字段与 `dsl.md §5.2` 一致

## Capabilities

### New Capabilities
- `lib-loop-args-parsing`: lib/loop/* DSL 文件 + parser `args:` / `arguments:` alias 一致性契约
- `dsl-schema-audit`: tools/check_dsl_schema.sh CI 验证脚本（防止 spec drift）

### Modified Capabilities
- `openspec/specs/dsl.md §5.2`: 明确 `arguments:` canonical + 兼容 `args:` alias（向后兼容）

## Impact

- `lib/loop/react.agent.md:26` (decide)
- `lib/loop/react.agent.md:33` (act, **附带独立第 2 潜伏 gap**: 字符串模板值不被 parser 解析, 需 follow-up)
- `lib/loop/plan_execute.agent.md:24` (execute)
- `lib/loop/fork_join.agent.md:22,30,38` (task_a/b/c)
- `src/modules/parser/node_factory.cpp:176` (可选 alias)
- `tests/test_parser_dsl_schema.cpp` (新增)
- `tools/check_dsl_schema.sh` (新增)
- `openspec/specs/dsl.md §5.2` (兼容性更新)
- `AGENTS.md` Recent Changes entry

## Non-goals

- 不修复 react.agent.md:33 字符串模板值 args (第 2 潜伏 gap, 独立 follow-up)
- 不重写 DSL parser 整体架构 (仅加 alias)
- 不改 lib/inference 或 lib/math 等其他 9 个已合规 DSL 文件
- 不修复 test_loop_agent_autonomous vs test_e2e_real_llm 矛盾点 (Metis 44min 调查未闭环, 推测是 stale build binary, 但本次不修)

## Dependencies

- `commit 1c729e3` (V2 fix) + `commit 74ceecd` (archive) ✅ 已 ship
- `openspec/changes/archive/2026-09-30-fix-chatsession-empty-llm-response/` (V2 design doc)
- `docs/specs/dsl.md §5.2` (DSL canonical spec)
- `src/modules/parser/node_factory.cpp:176` (parser 当前实现)

## Oracle + Metis 反馈

Per Oracle `ses_ef4386f3dffeiO3OvtOOCogRYy` + Metis `ses_ef433ad8bffewxBioiEktPFcFM`:

✅ **推荐 Option 2** (改 lib/loop 文件) — spec-aligned, 零 parser 风险, 与 lib/inference|math 既有一致
❌ **不推荐 Option 1** (parser 兼容 `args:` alias) — 制造双 canonical key 债务
❌ Option 3 (两者都改) — 过度工程

**V2 fix 独立 ship 价值已证实**:
- V2 fail-fast 修复 whitespace silent pass (真实 bug)
- V2 fix 是 defense-in-depth，与 parser bug 正交
- 不 ship V2 fix = 失去防御纵深价值

**附带回退建议**: parser alias (Option 1) 作为防御纵深而非 spec 主项 —— 已 ship 的第三方 .agent.md 文件可能用 `args:`, 但我们内部 lib/* 全部改为 `arguments:` (canonical)。

## Tasks
- [ ] 1.1 修改 `lib/loop/react.agent.md:26` `args:` → `arguments:`
- [ ] 1.2 修改 `lib/loop/react.agent.md:33` `args:` → `arguments:` (字符串模板值仍不被解析, 单独 follow-up)
- [ ] 1.3 修改 `lib/loop/plan_execute.agent.md:24` `args:` → `arguments:`
- [ ] 1.4 修改 `lib/loop/fork_join.agent.md:22,30,38` 3 处 `args:` → `arguments:`
- [ ] 1.5 (可选防御纵深) `node_factory.cpp:176` 增加 `args:` alias 检测 (`j.contains("args") || j.contains("arguments")`)
- [ ] 1.6 新增 `tests/test_parser_dsl_schema.cpp` 解析 12 个 lib/*.md 验证 `arguments` 一致性
- [ ] 1.7 新增 `tools/check_dsl_schema.sh` CI 脚本
- [ ] 1.8 更新 `openspec/specs/dsl.md §5.2` 兼容性说明
- [ ] 1.9 跑 `ctest -L must_realllm` 验证 test_e2e_real_llm ChatSession case PASS
- [ ] 1.10 跑全量 ctest 零回归
- [ ] 1.11 archive + 24h cooling-off

## Reverse Indicator

```
[Reverse Indicator]
+ new_up: test_e2e_real_llm ChatSession case pre-existing FAIL → PASS (real DeepSeek);
  lib/loop 3 files spec-aligned with dsl.md §5.2;
  parser regression test (DSL schema audit prevents future drift);
  CI script (tools/check_dsl_schema.sh)
- old_down: drop_ratio = 0% (rename 改 key 不影响功能, 字符串模板 args 仍不被解析)
failure_traces:
  - test_e2e_real_llm ChatSession case "Missing 'response' argument" (since 2026-09-18 F1 ship)
  - Pre-existing from `5a9ab2e` 2026-07-20 (Sprint 21 lib/loop creation)
ablation:
  - Mock: 3-segment (react.agent.md rename 不影响 mock path)
  - Real LLM: lib/loop rename → parser 读 arguments → decide_react 收到 response → no longer FAIL
context_ids:
  - test_e2e_real_llm ChatSession case (the 1 pre-existing failure case)
```