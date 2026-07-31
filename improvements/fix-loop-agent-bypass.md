# fix-loop-agent-bypass

**优先级**: P0 | **来源**: layer-based-missing-capabilities-analysis.md §三 X4 / §八 L4-1 + active-status Wave 1 #1
**阶段**: wave-1 | **分类**: core-impl
**类型**: debt

## 架构依据
- `examples/pdk_chat_demo/DESIGN.md:530` 声明 "Loop Agent 触发 via call_tool 'loop/run'"，但 `chat_session.cpp:233` 的 `use_direct_llm = (llm != nullptr)` 短路使正常 demo 流程绕过 loop_agent，`pdk/loop_agent/` 实为 dead code — 设计与代码脱节（X4 隐藏缺陷）。
- ADR-0068 §决策 3 指定 loop_agent 为 `loop.turn.start` / `loop.turn.end` / `loop.decision` 的 owner；只有本修复让 loop_agent 进入调用路径，这 3 个幻影主题才有发射载体。
- 阻塞下游：streaming（§五）、compaction（§七）、工具并行（§八）、steering（§六）全部借鉴路径。
- 兜底设计已确认：删除短路后统一走 `call_tool("loop/run")`，loop_agent plugin 内部检测无真实 provider 时 mock fallback（测试零改动）。

## 范围
- **In Scope**: 删除 `use_direct_llm` 短路分支（统一走 `call_tool("loop/run")`）；loop_agent 内部 mock fallback；loop_agent 发射 `loop.turn.start` / `loop.turn.end` / `loop.decision`（字段按 ADR-0068 附录 A：turn/step/decision/tool）；3 个主题的真实发射测试（替换 `test_e2e_mock.cpp` 对应伪造段）；DESIGN.md 一致性核对。
- **Out Scope**: streaming 渲染（L4-3）、steering/异步 I/O（L4-2）、NodeExecutor 工具并行（L0-2）、`loop.done/error` 既有发射点的 EventBuilder 迁移（属 adr-0068-event-emission-contract 提案）。

## 关键场景
- GIVEN demo 以真实 LLM provider 启动，WHEN 用户发送消息，THEN 调用链经过 `loop/run` 工具（loop_agent 执行），且 bus 收到 `loop.turn.start`（含 `turn`, `step`）。
- GIVEN 测试场景 `llm == nullptr`，WHEN `call_tool("loop/run")` 触发，THEN loop_agent 内部 mock fallback 执行成功，现有 `test_loop_agent_plugin.cpp` 断言零改动通过。
- GIVEN loop_agent 执行一轮 ReAct，WHEN 到达决策点，THEN bus 收到 `loop.decision`（含 `decision`，tool_call 时含 `tool`）。

## 技术约束
- MUST 删除短路分支（而非加开关/改默认 — 不留双路径技术债）。
- MUST 保持 `loop/run` 工具签名与现有测试断言语义不变。
- MUST 发射字段与 ADR-0068 附录 A Registry 一致。
- MUST NOT 在本提案实现 EventBuilder（若 adr-0068-event-emission-contract 先行落地则复用，否则直接构造 BusEvent，由该提案迁移阶段统一收口）。
- SHOULD 核对 DESIGN.md §八 "6 个 Agent Plugin" 描述与实际一致。

## 验收标准
- `grep -rn "use_direct_llm" examples/` 返回 0。
- `test_loop_agent_plugin` 全绿（零改动）+ 新增 3 个真实发射测试通过。
- ctest 全量零回归；`python3 tools/docs_drift_audit.py` 0 DRIFT。
- ADR-0068 附录 A 中 3 个主题状态 👻 → ✅（随 adr-0068-event-emission-contract 统一更新亦可）。
