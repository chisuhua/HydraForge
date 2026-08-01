## Context

`examples/pdk_chat_demo/DESIGN.md:530` 将 Loop Agent 定位为 ChatSession 的下游执行者:

```
Chat Agent (main.cpp)
   │
   ▼
Loop Agent 触发（via call_tool "loop/run")
   ├─ emit "loop.turn.start" (turn=1, step=1)
   ...
```

但实际实现中 `chat_session.cpp:232` 取得 LLM provider 后设置 `use_direct_llm = (llm != nullptr)`。当 demo 以真实 LLM provider 启动(非 `--mock` 模式)时,该布尔值为 true,ChatSession 直接构造完整 prompt 并调用 `llm->generate()`,完全跳过 `loop/run`。结果:

1. `pdk/loop_agent/` 的真实 DSL 执行路径在 demo 中从未被触发。
2. ADR-0068 附录 A 指定由 loop_agent 拥有的 `loop.turn.start` / `loop.turn.end` / `loop.decision` 三个主题成为"幻影主题"——`test_e2e_mock.cpp:124-133` 在测试里手工伪造这些事件,生产代码零 emit。
3. 下游 streaming、compaction、工具并行、steering 全部依赖这些事件来观察 Agent 生命周期,但事件没有真实载体。

`loop/run` 的 mock fallback 路径( `pdk/loop_agent/src/pdk_entry.cpp:169-180` )其实已存在,只是从未被 ChatSession 真实调用。修复的核心是删除短路,统一走 `call_tool("loop/run")`,并在真实执行路径中补上事件发射。

## Goals / Non-Goals

**Goals:**
- 删除 `chat_session.cpp:233` 的 `use_direct_llm` 短路分支,让 ChatSession 在所有模式下都调用 `loop/run`。
- 保持 `loop/run` 工具签名与返回字段不变,现有 `test_loop_agent_plugin.cpp` 断言零改动通过。
- 保持 mock fallback 路径可用:当 `loop/set_parent_provider` 未设置时,`loop/run` 仍返回当前格式的 mock 响应。
- 在 loop_agent 真实 DSL 执行路径中按 ADR-0068 附录 A 发射 `loop.turn.start`、`loop.turn.end`、`loop.decision` 三个主题。
- 替换 `test_e2e_mock.cpp` 中对应伪造段,改为验证真实发射。
- 核对 `DESIGN.md` §八 "6 个 Agent Plugin" 描述与代码一致。
- 维持 ctest 全量零回归。

**Non-Goals:**
- 不修改 `loop/run` 或 `loop/set_parent_provider` 的工具签名与 ToolMetadata。
- 不实现 streaming 渲染(L4-3)、steering/异步 I/O(L4-2)、NodeExecutor 工具并行(L0-2)。
- 不迁移 `loop.done` / `loop.error` 的既有发射点到 EventBuilder(属于 `adr-0068-event-emission-contract` 提案)。
- 不实现 `EventBuilder` 本身;若 `adr-0068-event-emission-contract` 先行落地则复用,否则直接构造 `BusEvent`。
- 不修改 `ILLMProvider` 接口或 `LLMProviderFactory`。
- 不扩展 PluginLoader ABI 来传递 parent engine 引用(依赖已 ship 的 `thread_local` + `loop/set_parent_provider` 方案)。

## Decisions

### Decision 1: 删除短路分支而非添加开关

**方案选择**:直接删除 `use_direct_llm` 布尔值及其 `if (use_direct_llm)` 分支,统一走 `impl_->registry->call_tool("loop/run", loop_args)`。

**Rationale**:
- 设计文档已明确 Loop Agent 是 ChatSession 的执行下游,双路径是历史遗留的技术债。
- 添加开关会留下"默认走 direct LLM"的兼容路径,继续让 loop_agent 在真实 provider 场景下不可用。
- 删除后代码更简洁,`chat_session.cpp` 只负责编排(历史、预算、事件转发),LLM 推理完全交给 loop_agent。

**Alternatives Considered**:
- **加配置开关**(`agent_cfg.use_loop_agent`):保留双路径,默认 false。拒绝——会导致修复无效,loop_agent 继续 dead code。
- **改默认值为 true**:保留 `use_direct_llm` 变量但默认 true。拒绝——仍允许绕过,且多一个状态变量。
- **保留 direct LLM 作为 fallback**:删除 loop fallback 保留 direct。拒绝——与设计文档相反,且破坏 mock 模式一致性。

### Decision 2: 统一 loop/run 入参格式

**方案选择**:无论真实 LLM 还是 mock 模式,`chat_session.cpp` 都传递相同的 `loop_args` 字典:包含 `loop_type`、`prompt`、`system_prompt`、`history`、`tools`、`max_steps`。

**Rationale**:
- `loop/run` 当前 lambda 已接收这些字段(见 `pdk/loop_agent/src/pdk_entry.cpp:158-160`),无需改动工具签名。
- 统一入参避免真实/mock 路径分裂,便于单测覆盖。
- `history` 以 JSON 字符串传递,与当前 `loop_args["history"] = nlohmann::json(impl_->messages).dump()` 一致。

**Alternatives Considered**:
- **真实模式额外传 `provider_ptr`**:由 ChatSession 主动设置 `loop/set_parent_provider`。拒绝——已在 demo 初始化或测试 fixture 中设置,重复设置增加覆盖风险。
- **在 loop_args 中内嵌 provider 指针**:扩展 `loop/run` 签名。拒绝——破坏现有测试断言,且引入不安全指针跨工具边界。

### Decision 3: loop_agent 内部保持 mock fallback

**方案选择**:当 `tls_parent_provider == nullptr` 时,`loop/run` 仍返回当前 mock 响应(`response`/`steps`/`tokens_used`/`cost_usd`/`success`),并输出 error log。

**Rationale**:
- 保持 `test_loop_agent_plugin.cpp` 的 "no provider mock fallback" case 零改动通过。
- 兜底设计确保测试场景 `llm == nullptr` 下 demo 仍可运行,与 `loop-agent-dsl-execution` ship 时的 Q7 决议一致。
- 删除短路后,`llm == nullptr` 的测试场景仍需要可执行路径。

**Alternatives Considered**:
- **删除 mock fallback 并强制设置 provider**:测试需要全部重写,且 `--mock` 启动路径变复杂。拒绝——不满足"测试零改动"验收标准。
- **在 ChatSession 判断 provider 是否存在再决定调用**:这正是当前短路,已被列为要删除的缺陷。拒绝。

### Decision 4: 事件发射直接构造 BusEvent

**方案选择**:在 `pdk/loop_agent/src/pdk_entry.cpp` 的真实执行路径中,通过 `IInteractionBus` 直接 `emit(BusEvent{topic, ToolResult{...}})` 构造事件。字段严格遵循 ADR-0068 附录 A:
- `loop.turn.start`: `{turn, step}`
- `loop.decision`: `{decision, tool?}`
- `loop.turn.end`: `{turn, decision}`

**Rationale**:
- ADR-0068 附录 A 是 Canonical Topic Registry,本修复是这三个主题从 👻 到 ✅ 的关键步骤。
- 直接构造 `BusEvent` 避免阻塞于 `adr-0068-event-emission-contract` 的 `EventBuilder` 是否先落地。
- 如果 `adr-0068-event-emission-contract` 先行 ship,可在后续迁移到 EventBuilder,不影响本 change 的契约字段。

**Alternatives Considered**:
- **等待 EventBuilder 落地再一起实现**:延迟本修复,阻塞 Wave 1 后续 streaming/steering 等依赖。拒绝——本 change 必须让 loop_agent 进入调用路径。
- **在 ChatSession 中伪造事件**:继续绕过 loop_agent 但手工 emit 事件。拒绝——未修复设计脱节,且事件内容与 loop_agent 实际步骤不一致。
- **在 DSL 节点层 emit 事件**:由 `NodeExecutor` 或 DSL 图产生。拒绝——ADR-0068 明确 owner 是 loop_agent,事件语义属于 Agent 生命周期而非单个节点。

### Decision 5: 传递 bus 引用到 loop_agent 的方式

**方案选择**:通过 `loop/run` 工具参数传入 bus 指针或 session_id,loop_agent 在本地持有 `IInteractionBus*` 进行 emit;若未传入则跳过事件发射但不失败。

**Rationale**:
- `loop/run` 工具签名需保持向后兼容,不能强制要求所有调用方都传 bus。
- `pdk/loop_agent` 是独立 `.so`,无法直接访问 ChatSession 的 `impl_->bus`。
- 通过工具参数传递是插件架构下最干净的解耦方式。

**Alternatives Considered**:
- **扩展 PluginLoader ABI 传入 PluginInitContext**:最干净,但属于 ADR-0052 Phase 6+ 范围。拒绝——本 change 不引入 ABI 变更。
- **使用 `thread_local` 全局 bus 指针**:与 `loop/set_parent_provider` 的 `thread_local` 方案一致,但 bus 生命周期复杂,易 dangling。拒绝——bus 不是线程私有的,且覆盖语义不清晰。
- **在 ChatSession 中发射事件,loop_agent 仅返回元数据**:回退到方案 4 的伪实现。拒绝——未把 loop_agent 作为 owner。

## Risks / Trade-offs

### Risk 1: 真实 LLM 路径下循环次数/成本不可控

**Mitigation**:
- `loop/run` 的 DSL 执行已受 `max_steps` 参数约束,ChatSession 继续传递 `impl_->agent_cfg.max_steps`。
- 预算检查仍在 ChatSession 中完成,`loop/run` 返回的 `cost_usd` 被累加到 budget controller。
- 若真实 LLM 产生无限循环,由 DSL 图自身的 Assert/End 节点或 `max_steps` 终止。

### Risk 2: loop_agent 需要访问 bus 但参数未传

**Mitigation**:
- Decision 5 选择可选参数,未传 bus 时仅跳过事件发射,`loop/run` 仍返回正常结果。
- `test_loop_agent_plugin.cpp` 现有测试不验证事件,因此无需传 bus。
- `pdk_chat_demo` 的 `test_e2e_mock.cpp` 将显式传入 bus 并验证发射。

### Risk 3: `loop.turn.*` 事件字段与 ADR-0068 不一致

**Mitigation**:
- 字段严格按附录 A 实现:`turn`/`step` 为整数,`decision` 为字符串枚举("tool_call"/"respond"/"give_up"/"observe"),`tool` 为字符串(仅在 decision="tool_call" 时出现)。
- 新增测试断言 payload 键集合。
- 若后续 ADR-0068 修订字段,统一在 `adr-0068-event-emission-contract` change 中迁移。

### Trade-off 1: 直接构造 BusEvent vs 等待 EventBuilder

**Trade-off**:直接构造在 EventBuilder 落地后需要二次迁移,但能让本 change  unblock Wave 1。
**Decision**:接受。迁移成本限于 pdk_entry.cpp 中 3-4 个 emit 调用,且字段不变。

### Trade-off 2: 删除 direct LLM 路径 vs 保留快速单步响应

**Trade-off**:direct LLM 路径可一步得到响应,loop_agent DSL 执行会带来额外开销(解析、调度、多节点)。
**Decision**:接受。design 文档已指定 Agent 架构,且单步 simple chat 场景可通过 `--mock` 或简化 loop_type 保持低延迟。
