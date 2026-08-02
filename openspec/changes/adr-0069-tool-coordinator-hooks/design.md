## Context

`ToolCoordinator` 在 Sprint 14 (2026-07-31) 作为 standalone middleware ship（ADR-0031 §决策 5 Oracle Option C），其内部流程在 `src/common/tools/tool_coordinator.h` 头注释中固化为 6 步线性流：

```
NestingGuard (RAII) → layer check → ApprovalHandler → audit.invoked → call_tool → audit.completed → return
```

该设计解决了 ADR-0031 P3-P4 的 layer check、审批、审计三大关注点，但缺失扩展注入点：

1. **ApprovalHandler 仅二元决策**：只能批准或拒绝，无法修改工具参数或结果。
2. **无 pre-tool-call 拦截层**：PDK plugin 无法实现 pi-agent 风格的 `beforeToolCall`（参数修改 + block）。
3. **无 post-tool-call 结果修改层**：PDK plugin 无法实现 pi-agent 风格的 `afterToolCall`。
4. **事件发射点未与 hook 同源**：ADR-0068 要求 `tool.execution.start/end` 与工具调用链注入点同源，当前 audit 事件独立于任何 hook 机制。

本 change 在 ToolCoordinator 这一年轻组件上增加 hook 注入点。其调用链简单（仅 NodeExecutor 优先级链注入）、接口契约清晰，是引入扩展点的最佳窗口期。

## Goals / Non-Goals

**Goals:**

- 新建 L3 契约 `IToolHookRegistry`，暴露 `register_pre_hook` / `register_post_hook` 接口。
- 改造 `ToolCoordinator::execute()` 流程，支持按 priority 升序执行的 pre-hook 列表与 post-hook 列表。
- 保持 layer check 与 ApprovalHandler 的一等公民硬门地位，hook 不可绕过、不可禁用。
- 显式化 hook 失败语义：`FailClosed` 视为 Deny，`FailOpen` 跳过并记录 warning。
- 对齐 ADR-0068 事件契约：pre_hooks 后发射 `tool.execution.start`，post_hooks 后发射 `tool.execution.end`。
- Deny 路径发射 `tool.audit.denied`，reason 包含 hook 名与 deny_reason。
- 落地至少 1 个真实 PDK plugin 用例（`budget_agent` 预算超限降级 pre-hook）。
- 5 类核心测试通过，无 hook 路径零行为回归，ctest 全量零回归。

**Non-Goals:**

- 不实现异步 hook（defer 至 ADR-0030 V2 异步运行时）。
- 不实现 LLM 上下文变换钩子 `transformContext`（归属 L1-4 decorator 链 / TracingDecorator）。
- 不实现 session 生命周期钩子（归属 L1-3 / ADR-0068 session 主题）。
- 不修改 `ApprovalHandler` 公开 API 或审批语义。
- 不解决 ADR-0031 §8 defer 的语义项（`min_layer` 强制、成本闭环、超时、审批历史），但要求 C6 相关注入点必须复用本机制。
- 不引入第二套拦截/旁路机制。

## Decisions

### Decision 1: 采用 pre/post 双列表模型，否决洋葱链

**Rationale**:

- koa 式洋葱链（`next()` 嵌套）与 `ToolCoordinatorNestingGuard` 的 RAII 语义叠加后心智成本高，且 pre/post 双列表已完全覆盖 pi-agent `beforeToolCall` / `afterToolCall` 的 block/modify 用例。
- 双列表与现有 6 步线性流自然融合：`pre_hooks[]` 在 layer check 之前执行，`post_hooks[]` 在 call_tool 之后执行，无需重写控制流。
- 审计一致性更易保证：pre_hooks 结束后 args 已最终化，post-hooks 结束后 result 已最终化，分别作为 `tool.execution.start` 与 `tool.execution.end` 的 payload。

**Alternatives Considered**:

- 洋葱链（koa middleware）—— 拒绝：与 NestingGuard 的 thread_local RAII 叠加易产生隐藏调用顺序 bug，且对 modify/block 用例无额外表达力。
- 单一 hook 列表（pre only，无 post）—— 拒绝：无法覆盖 result 修改用例，pi-agent `afterToolCall` 能力缺失。

### Decision 2: 仅支持同步钩子

**Rationale**:

- 同步语义与 `ToolCoordinator::execute()` 的同步返回值 `ToolResult` 一致，零额外并发复杂度。
- 异步 hook 需要等待/回调基础设施，与 ADR-0030 V2 异步运行时强相关；在异步运行时未稳定前引入会导致双重并发风险。
- PDK plugin 当前以同步 `.so` 工具为主，同步 hook 满足 Wave 1 全部真实用例（budget_agent 预算检查）。

**Alternatives Considered**:

- 同步 + 异步双模 API —— 拒绝：异步模式依赖 ADR-0030 V2，且当前无真实用例；引入会扩大 API 表面积。
- 仅异步钩子 —— 拒绝：与 `execute()` 同步语义冲突，所有调用点需改写成 async/await 风格。

### Decision 3: Hook 排序按 priority 升序，同 priority 按注册顺序；tool_glob 遵循 ADR-0043

**Rationale**:

- priority 升序（数值小者先执行）是常见约定，与 Linux nice value、日志 level 等心智模型一致。
- 同 priority 按 FIFO 保证注册顺序可预测，避免非确定性执行。
- `tool_glob` 复用 ADR-0043 PDK 工具命名约定（如 `shell/*`、`*`），避免新增匹配语言。

**Alternatives Considered**:

- priority 降序 —— 拒绝：与大多数框架（Express、Koa 中间件优先级）习惯相反。
- 按 hook 名字典序 —— 拒绝：破坏注册顺序语义，插件加载顺序不再可控。
- 新增专用通配语法 —— 拒绝：与 ADR-0043 现有约定重复，增加学习与实现成本。

### Decision 4: HookErrorPolicy 采用 FailClosed / FailOpen 二值

**Rationale**:

- 二值策略足够表达安全类 hook（预算/合规必须阻断）与观测类 hook（日志/指标应跳过失败）的差异。
- 注册时显式声明策略，调用时无需外部配置即可确定失败路径。
- FailClosed 统一映射为 Deny，可复用现有 `tool.audit.denied` 审计事件；FailOpen 统一映射为跳过 + audit meta warning，保持调用继续。

**Alternatives Considered**:

- 三值策略（FailClosed / FailOpen / Retry）—— 拒绝：同步钩子无重试语义，重试应在上层会话/编排器处理。
- 运行时通过上下文参数决定 —— 拒绝：策略应在注册时静态确定，避免运行时决策引入不可预测性。

### Decision 5: layer check 与 ApprovalHandler 保持一等公民硬门，hook 不可绕过

**Rationale**:

- 安全模型不可弱化：ADR-0004 Layer×Category 矩阵与 ADR-0031 审批语义是已有的事实源，hook 只能围绕它们工作，不能替代或禁用。
- pre-hook 即使返回 `Continue` 或 `ModifyArgs`，也必须经过 layer check 和 ApprovalHandler；pre-hook 返回 `Deny` 时，layer check 与 ApprovalHandler 不执行（已被 hook 拒绝）。
- 该设计确保任何恶意或缺陷 hook 无法通过伪装成 hook 来绕过审批层。

**Alternatives Considered**:

- 允许 hook 短路 layer check / ApprovalHandler —— 拒绝：直接破坏 ADR-0004 / ADR-0031 安全模型。
- 将 ApprovalHandler 重构为 hook 的一种 —— 拒绝：ApprovalHandler 需要 transport、超时、callback 等独立语义，与通用 hook 不等价。

## Risks / Trade-offs

### Risk 1: pre-hook 修改 args 后 layer check / ApprovalHandler 的上下文一致性

**Mitigation**:

- `ToolCoordinator::execute()` 接收的 `args` 为 `const unordered_map&`，pre-hook 通过 `ModifyArgs` 返回新的 map；新 map 作为后续 layer check、ApprovalHandler、`tool.execution.start` 的统一输入。
- 在实现中显式用 `effective_args` 变量替换原始 `args`，避免后续步骤意外引用旧值。

### Risk 2: post-hook 抛 FailClosed 异常时 audit.completed 是否应记录

**Mitigation**:

- FailClosed 的 post-hook 异常视为调用失败，返回 `ToolResult::error`，同时 emit `tool.audit.denied`（reason 含 hook 名 + 异常摘要）。
- 不 emit `tool.audit.completed`，因为最终结果不是成功完成而是 hook 失败。

### Risk 3: Hook 注册顺序影响测试结果可重复性

**Mitigation**:

- 同 priority 严格 FIFO，测试用例显式控制注册顺序。
- 测试覆盖同 priority 多 hook 场景，验证顺序稳定。

### Risk 4: 引入 `IToolHookRegistry*` 可空指针增加运行时复杂度

**Mitigation**:

- 仅在 setter 被调用时启用 hook 路径；nullptr 时所有逻辑短路，与旧路径逐字节一致。
- 默认构造/未注册 hook 场景零开销：不创建任何容器遍历。

### Trade-off 1: pre/post 双列表 vs 单一 hook 接口

**Trade-off**: 双列表需要维护两个注册接口和两个遍历点，API 表面积更大。
**Decision**: 接受。双列表的语义清晰性（拦截点在调用前、修改点在调用后）远超单一接口的复杂度节省。

### Trade-off 2: 同步钩子 vs 未来异步需求

**Trade-off**: 当前仅支持同步，未来若 ADR-0030 V2 需要异步 hook，可能需新增 `IToolHookRegistryAsync` 接口或扩展 policy。
**Decision**: 接受。Wave 1 真实用例全部同步；异步需求 deferred，届时可在不破坏同步接口的前提下扩展。

### Trade-off 3: `tool.execution.start` 放在 pre_hooks 之后而非最初

**Trade-off**: 某些观察者可能期望在 hook 执行前就看到 start 事件，但本设计将 start 视为“即将进入安全审批层”的信号。
**Decision**: 接受。与 ADR-0068 同源点原则一致：start = 工具调用真实意图已最终化（含 hook 修改后 args），end = 结果已最终化（含 post-hook 修改后 result）。

## Migration Plan

- 本 change 为**纯新增能力**：`ToolCoordinator` 新增 `set_hook_registry(IToolHookRegistry*)` setter，旧调用方不调用即保持原行为。
- 若 NodeExecutor 后续希望注入 hook registry，调用新增 setter 即可；不调用无影响。
- PDK plugin 可通过 `DECLARE_TOOL` / `DEFINE_AGENT` 注册时机获取 `IToolHookRegistry*`（具体由 plugin loader 上下文提供，本 change 仅定义契约）。

## Open Questions

1. `budget_agent` plugin 的 pre-hook 是复用现有 `pdk/budget_agent/` 目录还是新建示例？—— 实施时根据目录现状决定；本 change 目标为“至少 1 个真实 plugin 用例”。
2. `tool.execution.start/end` 的 payload schema 是否在 ADR-0068 中已有明确定义？—— 实施前需再次确认，确保字段名与 ADR-0068 Registry 一致。
