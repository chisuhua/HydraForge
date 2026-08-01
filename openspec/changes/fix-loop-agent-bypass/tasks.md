# Tasks: fix-loop-agent-bypass

> **类型**: debt | **优先级**: P0 | **阶段**: wave-1 | **分类**: core-impl
> **来源**: `improvements/fix-loop-agent-bypass.md` §架构依据 / §关键场景 / §验收标准
> 估时 ~3-5 天

## 1. chat_session.cpp 删除 use_direct_llm 短路

- [ ] 1.1 在 `examples/pdk_chat_demo/chat_session.cpp:232` 删除 `bool use_direct_llm = (llm != nullptr);` 行
- [ ] 1.2 在 `examples/pdk_chat_demo/chat_session.cpp:235-292` 删除 `if (use_direct_llm) { ... } else { ... }` 双分支,改为统一执行 `else` 分支体
- [ ] 1.3 保留 `loop_args` 字典构造(`loop_type`/`prompt`/`system_prompt`/`history`/`tools`/`max_steps`),确认字段名与 `pdk/loop_agent/src/pdk_entry.cpp:158-160` 一致
- [ ] 1.4 保留 `impl_->registry->call_tool("loop/run", loop_args)` 调用
- [ ] 1.5 删除直连 LLM 路径中临时变量 `full_prompt`、`req`、`gen_result`、`code_str` 等无用代码
- [ ] 1.6 验证:`grep -rn "use_direct_llm" examples/` 返回 0
- [ ] 1.7 提交:`git commit -m "fix(pdk_chat_demo): remove use_direct_llm bypass in ChatSession::chat"`

## 2. loop_agent 真实路径事件发射

- [ ] 2.1 在 `pdk/loop_agent/src/pdk_entry.cpp` 确定 `IInteractionBus` 访问方式:通过 `loop/run` 新增可选参数 `bus_ptr` + `session_id` 或复用 `thread_local` 存储
- [ ] 2.2 若采用 `bus_ptr` 参数:在 `loop/run` lambda 解析 `args["bus_ptr"]` 与 `args["session_id"]`;未提供时跳过 emit 但不失败
- [ ] 2.3 在真实 DSL 执行路径(`pdk/loop_agent/src/pdk_entry.cpp:182-206`) turn 开始前发射 `loop.turn.start`,payload 含 `turn`/`step`
- [ ] 2.4 在决策点(think 节点选择 tool_call/respond/give_up 后)发射 `loop.decision`,payload 含 `decision`,tool_call 时含 `tool`
- [ ] 2.5 在 turn 结束(observe 后)发射 `loop.turn.end`,payload 含 `turn`/`decision`
- [ ] 2.6 确认 payload 字段与 ADR-0068 附录 A(`docs/adr/adr-0068-event-emission-contract.md:171-173`) 完全一致
- [ ] 2.7 若 `adr-0068-event-emission-contract` 已提供 `EventBuilder`,将 2.3-2.5 改用 `EventBuilder`;否则直接构造 `BusEvent`
- [ ] 2.8 验证:编译 `pdk/loop_agent` target 无错误(`cmake --build build --target LoopAgent -j$(nproc)`)
- [ ] 2.9 提交:`git commit -m "feat(loop_agent): emit loop.turn.start/end and loop.decision events in real path"`

## 3. 测试替换与新增

- [ ] 3.1 读取 `examples/pdk_chat_demo/tests/test_e2e_mock.cpp:114-153` 当前伪造 emit 段
- [ ] 3.2 删除 `test_e2e_mock.cpp` 中 `emit("loop.turn.start", ...)`、`emit("loop.decision", ...)`、`emit("loop.turn.end", ...)` 手工伪造行
- [ ] 3.3 改用 `ChatSession::chat()` 或 `engine->get_tool_registry().call_tool("loop/run", ...)` 触发真实 loop_agent 执行,捕获 bus 事件
- [ ] 3.4 新增 `TEST_CASE("loop/run emits loop.turn.start with turn and step", ...)` 断言 bus 中存在 `loop.turn.start` 且 payload 含 `turn`/`step`
- [ ] 3.5 新增 `TEST_CASE("loop/run emits loop.decision with decision and tool", ...)` 断言 `loop.decision` 存在,且 `decision="tool_call"` 时含 `tool`
- [ ] 3.6 新增 `TEST_CASE("loop/run emits loop.turn.end with turn and decision", ...)` 断言 `loop.turn.end` 存在且 payload 含 `turn`/`decision`
- [ ] 3.7 确认 `test_loop_agent_plugin.cpp` 零改动:6 个现有 TEST_CASE 继续 pass
- [ ] 3.8 验证:`ctest -R loop_agent --output-on-failure` 全绿
- [ ] 3.9 提交:`git commit -m "test(pdk_chat_demo): replace fake events with real loop_agent emission tests"`

## 4. DESIGN.md 与文档一致性核对

- [ ] 4.1 读取 `examples/pdk_chat_demo/DESIGN.md:520-553` Loop Agent 触发描述
- [ ] 4.2 核对 "Loop Agent 触发(via call_tool 'loop/run')" 与代码一致;若存在 "direct LLM" 或 "双路径" 描述则修正为统一路径
- [ ] 4.3 核对 §八 "6 个 Agent Plugin" 列表中 Loop Agent 角色描述
- [ ] 4.4 若 `docs/architecture/layer-based-missing-capabilities-analysis.md` §三 X4 / §八 L4-1 有相关状态表,更新其状态或标注依赖本 change
- [ ] 4.5 验证:`python3 tools/docs_drift_audit.py` 0 DRIFT
- [ ] 4.6 提交:`git commit -m "docs(pdk_chat_demo): align DESIGN.md with unified loop/run call path"`

## 5. 验证与 ship gate

- [ ] 5.1 运行 `cmake --build build -j$(nproc)` 全量编译零错误
- [ ] 5.2 运行 `ctest --output-on-failure` 全量零回归
- [ ] 5.3 运行 `grep -rn "use_direct_llm" examples/` 验证返回 0
- [ ] 5.4 运行 `ctest -R loop_agent --output-on-failure` 验证 `test_loop_agent_plugin` 全绿(6 个 case 通过)
- [ ] 5.5 运行新增 3 个真实发射测试(`ctest -R e2e_mock --output-on-failure` 或对应 target)
- [ ] 5.6 运行 `python3 tools/docs_drift_audit.py` 确认 0 DRIFT
- [ ] 5.7 运行 `python3 tools/adr_lint.py` 确认 ADR 格式通过
- [ ] 5.8 运行 `openspec validate fix-loop-agent-bypass` 验证 change artifacts 通过
- [ ] 5.9 更新 ADR-0068 附录 A 中 `loop.turn.start` / `loop.turn.end` / `loop.decision` 状态从 👻 到 ✅
- [ ] 5.10 提交:`git commit -m "chore: ship fix-loop-agent-bypass (Wave 1 P0)"`

---

## Ship Summary

- **验收标准**:
  - `grep -rn "use_direct_llm" examples/` 返回 0 ✅
  - `test_loop_agent_plugin` 全绿(零改动) ✅
  - 新增 3 个真实发射测试通过 ✅
  - ctest 全量零回归 ✅
  - `python3 tools/docs_drift_audit.py` 0 DRIFT ✅
- **OpenSpec change**: `openspec/changes/fix-loop-agent-bypass/`
- **Proposal**: `improvements/fix-loop-agent-bypass.md` (2026-08-01 已批准)
- **能力**: `loop-agent-call-path-activation` (New) + `pdk-chat-demo-orchestration` (Modified)

## Follow-ups

1. 若 `adr-0068-event-emission-contract` 在本 change 实施期间 ship,将 2.3-2.5 的 `BusEvent` 直接构造迁移到 `EventBuilder`。
2. `loop.done` / `loop.error` 的 EventBuilder 迁移留在 `adr-0068-event-emission-contract` 范围。
3. streaming 渲染、steering、工具并行等下游能力可在本 change ship 后启动。
