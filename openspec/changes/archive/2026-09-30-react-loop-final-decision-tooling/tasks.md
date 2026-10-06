# Tasks: react-loop-final-decision-tooling

## Phase 1: Pre-implementation Verification

- [x] Oracle dual-agent review 4 paths (Path D 收敛, 详见 design.md D1)
- [x] Verify plan_execute.agent.md / fork_join.agent.md 无模板 tool 字段（无需同步）
- [x] Verify pdk_entry.cpp:114-117 注释确认 "finish" 是原始设计意图
- [x] Verify finish 工具已在 child registry 注册 (register_react_support_tools_for_child)
- [x] Verify chat_session.cpp 不感知 decision.final（仅读 response/steps/tokens_used/cost_usd）
- [x] Verify process_output_keys 存储 decision 为 JSON object（非 dump string）

## Phase 2: OpenSpec Change Artifacts

- [x] Create `openspec/changes/2026-09-30-react-loop-final-decision-tooling/.openspec.yaml`
- [x] Create `openspec/changes/2026-09-30-react-loop-final-decision-tooling/proposal.md`
- [x] Create `openspec/changes/2026-09-30-react-loop-final-decision-tooling/design.md`
- [x] Create `openspec/changes/2026-09-30-react-loop-final-decision-tooling/tasks.md` (this file)
- [x] Create `openspec/changes/2026-09-30-react-loop-final-decision-tooling/specs/react-loop-final-decision/spec.md`
- [ ] Run `openspec validate openspec/changes/2026-09-30-react-loop-final-decision-tooling` → exit 0

## Phase 3: Production Code

### D1: parse_react_decision L3 fallback (核心修复)

- [ ] Edit `pdk/loop_agent/src/pdk_entry.cpp:242-247` (L3 fallback)
  - [ ] `out["action_tool"] = "finish";` (改: 原 `""`)
  - [ ] `out["action_args"] = final_text;` (改: 原 `nullptr`, 注意是 JSON string)
  - [ ] `out["response"] = final_text;` (不变)
- [ ] Add brief comment referencing OpenSpec change + original design intent

### D3: finish 工具读 input 兜底 (闭环补丁 1, 必须)

- [ ] Edit `pdk/loop_agent/src/pdk_entry.cpp:152-160` (finish tool lambda)
  - [ ] 三级取参: `answer` → 非空 `input` → `"Task complete"`
  - [ ] 保持 backward compat (answer 路径不变)

### D4: loop/run response 优先 decision.response (闭环补丁 2, 可选)

- [ ] Edit `pdk/loop_agent/src/pdk_entry.cpp:772-784` (loop/run response extraction)
  - [ ] 在现有提取链后加 decision.response 优先逻辑
  - [ ] 仅当 decision 是 object 且含非空 string response 时使用

## Phase 4: Test Updates

### D5a: test_loop_agent_plugin.cpp 2 条断言更新

- [ ] Edit `examples/pdk_chat_demo/tests/test_loop_agent_plugin.cpp:253-254`
  - [ ] L253: `result.value("action_tool", "") == ""` → `result.value("action_tool", "") == "finish"`
  - [ ] L254: `result["action_args"].is_null()` → `result["action_args"].is_string() && result["action_args"] == "The result is 42."`
- [ ] Verify: `cmake --build build --target test_loop_agent_plugin -j$(nproc)` exit 0
- [ ] Verify: `./build/examples/pdk_chat_demo/tests/test_loop_agent_plugin` 22+1 cases PASS

### D5b: 新增 L3 final e2e case

- [ ] Add new TEST_CASE in `test_loop_agent_plugin.cpp`:
  - [ ] Case "loop/run: L3 final answer completes react loop without template error"
  - [ ] Use MockProvider returning "Final Answer: The result is 42."
  - [ ] Set parent provider + no mock_fallback (走真 DSL)
  - [ ] Assert: `result.ok == true`, `error_code == null`, `response 非空` (若 D4 done: `== "The result is 42."`), `steps >= 4`
- [ ] RED 验证: 临时还原 `pdk_entry.cpp:244` 回 `""` → 新 case FAIL（"Tool '' not registered" 或 template render error）

## Phase 5: Verification (per AGENTS.md FULL REGRESSION TEST FLOW §5)

- [ ] Local rebuild: `cmake --build build -j$(nproc)` exit 0
- [ ] Focused: `ctest --test-dir build/examples/pdk_chat_demo/tests -R "test_loop_agent_plugin" --output-on-failure` 22+1 cases PASS
- [ ] Targeted: `ctest --test-dir build/examples/pdk_chat_demo/tests -L must_realllm -R "^test_e2e_real_llm$" --output-on-failure` 4 cases PASS
- [ ] Core regression: `ctest --test-dir build -LE must_realllm` 263+/263+ PASS (zero regression)
- [ ] Examples regression: `ctest --test-dir build/examples/pdk_chat_demo/tests -LE must_realllm` 33/33 PASS (zero regression)
- [ ] `bash tools/check_dsl_schema.sh` GREEN
- [ ] `python3 tools/adr_lint.py` exit 0
- [ ] `openspec validate openspec/changes/2026-09-30-react-loop-final-decision-tooling` exit 0
- [ ] `git status --short` clean (post-commit)

## Phase 6: Documentation Sync

### Dspec: dsl.md §5.2

- [ ] Edit `openspec/specs/dsl.md §5.2 tool_call`
  - [ ] 明确 react loop 终止路径契约 (decision.final=true 时 act 节点调 finish 工具)
  - [ ] Reference this OpenSpec change archive

### Ddoc: AGENTS.md

- [ ] Edit `AGENTS.md` Recent Changes entry
  - [ ] Add reverse indicator: + new_up / - old_down / failure_traces / ablation / context_ids
  - [ ] Reference this OpenSpec change commit + archive

### Ddoc: improvement archive

- [ ] Mark `.rddf/improvements/react-loop-final-decision-tooling.md` as "✅ SHIPPED via 2026-09-30-react-loop-final-decision-tooling"

## Phase 7: Ship

- [ ] Stage all files (2 production + 1 test + 1 spec + 1 AGENTS.md + 1 improvement archive)
- [ ] Verify `git status --short` shows expected ~5 files
- [ ] Atomic commit with Reverse Indicator 5-field commit message
- [ ] `git mv openspec/changes/2026-09-30-react-loop-final-decision-tooling openspec/changes/archive/`
- [ ] Verify 5-file archive integrity (.openspec.yaml + proposal.md + design.md + tasks.md + specs/react-loop-final-decision/spec.md)
- [ ] Atomic archive commit
- [ ] Trigger 24h cooling-off (cooling_off_until = 2026-10-01T00:00:00Z)

## Phase 8: Post-ship Verification

- [ ] `git log --oneline -5` shows expected commits
- [ ] Final ctest run after archive commit (still 100% green)
- [ ] Update `AGENTS.md` if any post-ship drift found (within same commit)
- [ ] Document 24h cooling-off expiry in `AGENTS.md` for next maintainer

## Effort Estimate

- **Quick** (核心 D + 2 闭环补丁 + 2 断言更新 + 1 新 e2e case, <1h 编码)
- **Short** (含 OpenSpec artifacts + cooling-off + dual-agent review 完整治理闭环, 半天)