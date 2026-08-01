## Why

`examples/pdk_chat_demo/DESIGN.md:530` 声明 "Loop Agent 触发 via call_tool 'loop/run'",但 `chat_session.cpp:233` 的 `use_direct_llm = (llm != nullptr)` 短路使正常 demo 流程绕过 loop_agent——只要配置了真实 LLM provider,ChatSession 就直接调用 `ILLMProvider::generate()`,`pdk/loop_agent/` 实际沦为 dead code。设计与代码脱节形成 X4 隐藏缺陷。

ADR-0068 §决策 3 指定 loop_agent 为 `loop.turn.start` / `loop.turn.end` / `loop.decision` 三个主题的 owner;只有删除短路、让 `loop/run` 进入真实调用路径后,这三个幻影主题才有发射载体。否则下游 streaming、compaction、工具并行、steering 全部失去可观测的 Agent 生命周期事件。

关键场景已被 `test_e2e_mock.cpp:124-133` 伪造,但生产代码从未 emit 这些事件。本修复把调用链统一回 `loop/run`,让 loop_agent 在 mock fallback 和真实 LLM 路径下都产生一致的事件契约。

## What Changes

- **修改** `examples/pdk_chat_demo/chat_session.cpp:232-292`,删除 `use_direct_llm` 短路分支,统一走 `call_tool("loop/run")`。
- **修改** `examples/pdk_chat_demo/chat_session.cpp:236-275`,移除直连 LLM provider 的完整 prompt 构造与 `generate()` 调用逻辑,ChatSession 不再自己产生 LLM 响应。
- **新增** `pdk/loop_agent/src/pdk_entry.cpp:169-180` 的 mock fallback 路径保持可用:当 `loop/set_parent_provider` 未设置时,`loop/run` 返回与当前格式一致的 mock 响应。
- **新增** `pdk/loop_agent/src/pdk_entry.cpp` 在真实 DSL 执行路径中按 ADR-0068 附录 A 发射 `loop.turn.start`(`turn`,`step`)、`loop.decision`(`decision`,tool_call 时含 `tool`)、`loop.turn.end`(`turn`,`decision`)。
- **新增** `examples/pdk_chat_demo/tests/test_e2e_mock.cpp` 替换当前伪造事件段,改为验证 loop_agent 真实发射上述 3 个主题。
- **修改** `examples/pdk_chat_demo/DESIGN.md:530` 附近描述,确保 "6 个 Agent Plugin" 与 chat_session 实际调用路径一致。
- **不修改** `loop/run` 工具签名与现有 `test_loop_agent_plugin.cpp` 断言语义。
- **不修改** `loop.set_parent_provider` 的实现与 ToolMetadata(`StateModify` / `force_approval_always=true` / `{Workflow}`)。
- **不修改** `loop.done` 与 `loop.error` 的既有发射点(这些主题的 EventBuilder 迁移属于 `adr-0068-event-emission-contract` 提案范围)。
- **不修改** streaming 渲染、steering/异步 I/O、NodeExecutor 工具并行等下游能力。

## Capabilities

### New Capabilities
- `loop-agent-call-path-activation`: Loop Agent 通过 `loop/run` 工具成为 ChatSession 唯一的 ReAct 执行路径,真实 LLM provider 路径与 mock 路径都经由 loop_agent 内部 DSL 执行,并发射 `loop.turn.*` / `loop.decision` 生命周期事件。

### Modified Capabilities
- `pdk-chat-demo-orchestration`: ChatSession 从"直连 LLM 或回退 loop_agent"双路径改为统一调用 `loop/run`;真实 LLM 响应由 loop_agent 子引擎产生,ChatSession 仅负责历史维护、预算检查与事件转发。

## Impact

### 代码影响
- `examples/pdk_chat_demo/chat_session.cpp:232-292`:删除 `use_direct_llm` 分支,统一调用 `impl_->registry->call_tool("loop/run", loop_args)`。
- `pdk/loop_agent/src/pdk_entry.cpp:182-206`:在真实执行路径插入 `BusEvent` 发射点(需接入 `IInteractionBus`)。
- `examples/pdk_chat_demo/tests/test_e2e_mock.cpp:114-153`:替换伪造 emit 段为真实 loop_agent 事件验证。
- `examples/pdk_chat_demo/DESIGN.md:530-553`:核对 Loop Agent 触发描述与实际代码一致。

### 依赖
- 依赖 `loop-agent-dsl-execution` 已 ship 的 `DSLEngine::from_markdown(content, ILLMProvider&)` 子引擎 provider 传播能力。
- 若 `adr-0068-event-emission-contract` 先行落地则复用其 EventBuilder,否则直接构造 `BusEvent`。
- 无外部依赖变更。

### API 兼容性
- ✅ `loop/run` 工具签名与返回字段(`response`/`steps`/`tokens_used`/`cost_usd`/`success`/`error`)保持不变。
- ✅ `loop/set_parent_provider` 工具签名与 ToolMetadata 保持不变。
- ✅ `test_loop_agent_plugin.cpp` 现有断言零改动通过。
- ⚠️ ChatSession 行为变化:真实 LLM 模式下不再直接生成响应,而是经 `loop/run` 走 DSL 执行;这是设计意图,非回归。

### 验证
- `grep -rn "use_direct_llm" examples/` 返回 0。
- `test_loop_agent_plugin` 全绿(零改动)。
- 新增 3 个真实发射测试覆盖 `loop.turn.start` / `loop.turn.end` / `loop.decision`。
- ctest 全量零回归。
- `python3 tools/docs_drift_audit.py` 0 DRIFT。
- ADR-0068 附录 A 中 3 个主题状态 👻 → ✅。

### 风险
- 低 - 主要删除死分支并统一调用路径,`loop/run` 的 mock fallback 已存在;真实路径依赖已 ship 的 provider 传播。
- 中 - 需要确保 `pdk_chat_demo` 的 `IInteractionBus` 实例能传入 loop_agent,否则事件无法发射;需新增 `loop/run` 参数或线程局部引用。
