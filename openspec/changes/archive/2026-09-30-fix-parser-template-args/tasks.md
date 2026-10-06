# Tasks: fix-parser-template-args

## Phase 1: Pre-implementation Verification

- [x] Explore parser code (node_factory.cpp make_tool_call) + executor (node_executor.cpp execute_tool_call) + InjaTemplateRenderer::render
- [x] Verify react.agent.md:30-35 act node uses string arguments + tool template
- [x] Verify `parse_react_decision` outputs `{"input": ...}` wrapper for string args (pdk_entry.cpp:198, 219)
- [x] Confirm prior fix (46a3958) only addressed canonical `arguments:`, not string-value arguments
- [x] Audit report §6.1:443 + roadmap §1.4 + dsl.md §5.2 current state captured for sync

## Phase 2: OpenSpec Change Artifacts

- [x] Create `openspec/changes/2026-09-30-fix-parser-template-args/.openspec.yaml`
- [x] Create `openspec/changes/2026-09-30-fix-parser-template-args/proposal.md`
- [x] Create `openspec/changes/2026-09-30-fix-parser-template-args/design.md`
- [x] Create `openspec/changes/2026-09-30-fix-parser-template-args/tasks.md` (this file)
- [x] Create `openspec/changes/2026-09-30-fix-parser-template-args/specs/parser-template-args/spec.md`
- [ ] Run `openspec validate openspec/changes/2026-09-30-fix-parser-template-args` → exit 0

## Phase 3: Production Code

### D1: Parser-side string arguments wrap

- [ ] Edit `src/modules/parser/node_factory.cpp:171-189 make_tool_call`
  - [ ] Add branch: `if (j["arguments"].is_string()) { args["input"] = j["arguments"].get<std::string>(); }`
  - [ ] Preserve existing object branch (per 46a3958 ship, no changes)
  - [ ] Add brief comment explaining single-arg convention rationale
- [ ] Verify: `cmake --build build --target agenticdsl_core -j$(nproc)` exit 0

### D2: Executor-side tool_name template render

- [ ] Edit `src/modules/executor/node_executor.cpp:execute_tool_call`
  - [ ] Stream path (L259-285): Add `InjaTemplateRenderer::render(node->tool_name, ctx)` before `has_tool()` + `dispatch_to_tool()`
  - [ ] Main path (L288-298): Same render before `has_tool()` + `dispatch_to_tool()`
  - [ ] Add brief comment explaining template render rationale
- [ ] Verify: `cmake --build build -j$(nproc)` exit 0 (rebuild all)

## Phase 4: Regression Guard

### D3: test_parser_template_args.cpp

- [ ] Create `tests/test_parser_template_args.cpp`
- [ ] Add to `tests/CMakeLists.txt` via `add_catch_test(test_parser_template_args)` (per existing pattern)
- [ ] Case 1 (parser string arguments wrap): Parse `arguments: "{{x.y}}"` → `tc->arguments.size() == 1 && tc->arguments.count("input") == 1`
- [ ] Case 2 (parser object arguments preserved): Parse `arguments: { key: "value" }` → 既有 path 不破坏
- [ ] Case 3 (executor tool_name render): Mock ToolRegistry 注册 `finish` 工具, Context 注入 `decision.action_tool = "finish"`, execute → 成功返回
- [ ] Case 4 (executor tool_name literal): `tool: "literal_tool"` literal 路径
- [ ] Verify RED: 临时还原修改 → test fail; 恢复 → test pass
- [ ] Verify: `cmake --build build --target test_parser_template_args -j$(nproc) && ./build/tests/test_parser_template_args` exit 0, 3-4 cases PASS

## Phase 5: Verification (per AGENTS.md FULL REGRESSION TEST FLOW §5)

- [ ] Local rebuild: `cmake --build build -j$(nproc)` exit 0
- [ ] Targeted: `ctest --test-dir build -R "test_parser_template_args" --output-on-failure` 3-4 cases PASS
- [ ] Targeted: `ctest --test-dir build/examples/pdk_chat_demo/tests -L must_realllm -R "^test_e2e_real_llm$" --output-on-failure` 4 cases PASS
- [ ] Core regression: `ctest --test-dir build -LE must_realllm` 262+/262+ PASS (zero regression)
- [ ] Examples regression: `ctest --test-dir build/examples/pdk_chat_demo/tests -LE must_realllm` 33/33 PASS (zero regression)
- [ ] `bash tools/check_dsl_schema.sh` GREEN (no spec drift)
- [ ] `python3 tools/adr_lint.py` exit 0 (no ADR regression)
- [ ] `git status --short` clean (post-commit)

## Phase 6: Documentation Sync

### Dspec: dsl.md §5.2

- [ ] Edit `docs/specs/dsl.md §5.2 tool_call`
  - [ ] Add `arguments` 字段 string-value 契约 (single-arg convention `input`)
  - [ ] Add `tool` 字段模板渲染契约 (execute-time render)
  - [ ] Reference this OpenSpec change + archive
- [ ] Verify: line count + section headers consistent

### Ddoc: audit §6.1 line 443

- [ ] Edit `docs/audits/2026-09-29-harness-self-evolution-rsi-audit.md:443`
  - [ ] Update ChatSession e2e real LLM entry: "本 change ship 关闭第 2 潜伏 gap, 4/4 cases PASS"
  - [ ] Reference `2026-09-30-fix-parser-template-args` archive

### Ddoc: roadmap §1.4

- [ ] Edit `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md §1.4`
  - [ ] Add Bug 4 记录: parser template args fix (commit TBD, ship in `2026-09-30-fix-parser-template-args`)
  - [ ] Update Bug 3 V2 残量记录: 关联本 change 关闭 ChatSession case
  - [ ] Maintain reverse indicator format

### Ddoc: AGENTS.md

- [ ] Edit `AGENTS.md` Recent Changes entry
  - [ ] Add reverse indicator: + new_up / - old_down / failure_traces / ablation / context_ids
  - [ ] Reference parser template args fix commit + archive
- [ ] Verify: line count consistent

### Ddoc: improvement archive

- [ ] Mark `.rddf/improvements/parser-string-template-args.md` as "✅ SHIPPED via 2026-09-30-fix-parser-template-args"
- [ ] Verify: not in active improvement list

## Phase 7: Ship

- [ ] Stage all 7 files (3 production + 1 test + 4 spec/docs/AGENTS.md)
- [ ] Verify `git status --short` shows expected 7-8 files
- [ ] Atomic commit with Reverse Indicator 5-field commit message
- [ ] `git mv openspec/changes/2026-09-30-fix-parser-template-args openspec/changes/archive/`
- [ ] Verify 5-file archive integrity (`.openspec.yaml` + `proposal.md` + `design.md` + `tasks.md` + `specs/parser-template-args/spec.md`)
- [ ] Atomic archive commit
- [ ] Trigger 24h cooling-off (cooling_off_until = 2026-10-01T00:00:00Z)

## Phase 8: Post-ship Verification

- [ ] `git log --oneline -5` shows expected commits
- [ ] Final ctest run after archive commit (still 100% green)
- [ ] Update `AGENTS.md` if any post-ship drift found (within same commit)
- [ ] Document 24h cooling-off expiry in `AGENTS.md` for next maintainer