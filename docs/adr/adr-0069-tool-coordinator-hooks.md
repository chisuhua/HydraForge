# ADR-0069: ToolCoordinator Hook 注入点 (Tool Call Interception Hooks)

## 状态

🟡 Partial (2026-08-04 — middleware 改造 + budget_agent pre-hook + 5 类测试已 ship; §决策 7 Approved 条件 1-4 满足, 条件 5 ctest 零回归中 `test_cost_tracking_decorator` 为 pre-existing 失败 (commit `514c441` Phase 5 引入), 其余 95/96 PASS)

## 领域

L1 OS Services / 工具调用链扩展点机制

## 关联

- [ADR-0031 — 执行策略](./adr-0031-execution-policy.md) — 审批语义 (ApprovalHandler) 与 Layer 权限的唯一事实源, 本 ADR 不重复管辖; §8 defer 项协调见 §决策 6
- [ADR-0004 — ToolRegistry 安全模型](./adr-0004-toolregistry-security.md) — Layer×Category 矩阵 (硬门, 不可被 hook 绕过)
- [ADR-0068 — 事件发射契约](./adr-0068-event-emission-contract.md) — `tool.execution.start/end` 发射点定义, 与本 ADR 注入点同源
- [ADR-0021 — PDK 设计](./adr-0021-pdk-design.md) — PDK 扩展性原则 (P1-P6), hook 注册是 PDK 表达力的核心载体
- [`docs/architecture/layer-based-missing-capabilities-analysis.md`](../architecture/layer-based-missing-capabilities-analysis.md) §三 X2 / §五 L1-2 — 决策源头
- [`docs/research/pi-agent-vs-pdk-chat-demo-analyze.md`](../research/pi-agent-vs-pdk-chat-demo-analyze.md) — pi-agent hook 模型对照

## 背景

### 问题

当前工具调用链**无任何拦截/修改注入点**。`ToolCoordinator::execute()` 是 6 步线性流（`src/common/tools/tool_coordinator.h` 头注释）：

```
NestingGuard (RAII) → layer check → ApprovalHandler → audit.invoked → call_tool → audit.completed → return
```

其中 ApprovalHandler 只能**批准/拒绝**二元判断，无法修改参数或结果。对照 pi-agent 的拦截能力（X2 复核表）：

| 能力 | pi-agent | HydraForge 现状 |
|------|---------|----------------|
| 工具调用拦截 + 参数修改 + block | ✅ `beforeToolCall` | ⚠️ ApprovalHandler 仅可拒绝, **不可修改** |
| 工具结果修改 | ✅ `afterToolCall` | ❌ 无 |

**X2 原始缺口表混装了三种不同归属的钩子**，本 ADR 立项时（D3 决议）已完成归属拆分：

| X2 钩子 | 真实归属 | 管辖 |
|---------|---------|------|
| 工具调用 pre/post 拦截 | **ToolCoordinator** | ✅ 本 ADR |
| `transformContext` (LLM 消息注入/裁剪) | LLM 调用链 | ❌ L1-4 后续决策 (decorator 链) |
| `session_before_*` 会话生命周期 | Session 层 | ❌ L1-3 / ADR-0068 session 主题 (通知面) |
| `input` 输入拦截 | L4 应用层 | ❌ ChatSession 自己的事 |

**窗口期判断**: ToolCoordinator 是 Sprint 14 (2026-07-31) 才 ship 的年轻组件, 调用方少 (NodeExecutor 优先级链注入)。此时设计扩展点成本最低; 调用方扩散后再加将是破坏性重构。

### 目标

1. PDK plugin 可注册 pre-hook (拦截/修改参数/拒绝) 与 post-hook (修改结果) —— X2 核心价值。
2. 安全模型零弱化: layer check 与 ApprovalHandler 保持一等公民硬门地位, hook 围绕而非取代。
3. 失败语义显式化: 每个 hook 注册时声明 fail-open / fail-closed。
4. 与 ADR-0068 事件契约同源: hook 注入点即 `tool.execution.*` 发射点。

## 决策

### 1. 管辖边界

| 关注点 | 管辖 ADR |
|--------|---------|
| **工具调用 pre/post 钩子机制** | ✅ 本 ADR |
| 审批语义 (何时需要人审、三种 transport) | ADR-0031 |
| Layer×Category 权限矩阵 | ADR-0004 |
| 钩子触发的事件通知 (topic/payload) | ADR-0068 |
| LLM 上下文变换钩子 | L1-4 后续决策 (decorator 链, 候选 TracingDecorator 同位) |
| 会话生命周期钩子 | L1-3 (通知面归 ADR-0068 session 主题) |

### 2. 钩子模型: pre/post 双列表, 不选洋葱链

否决 koa 式洋葱链 (next 嵌套): 与 NestingGuard 的 RAII 语义叠加后心智成本高, 且 pre/post 列表已覆盖 pi-agent 全部 block/modify 用例。执行流程:

```
pre_hooks[] (按 priority 升序; 可改 args / 可 deny)
  → layer check        (ADR-0004 硬门, 不可绕过, 不可被 hook 禁用)
  → ApprovalHandler    (ADR-0031 内置, 不改为 hook)
  → audit.invoked      (ADR-0068)
  → registry.call_tool()
  → post_hooks[] (按 priority 升序; 可改 result)
  → audit.completed    (记录 post-hook 修改后的最终结果 — 审计必须反映真实生效结果)
  → return
```

### 3. Hook 接口与注册 API (L3 契约)

```cpp
// include/agenticdsl/contract/itool_hook_registry.h (新建)
enum class HookErrorPolicy { FailClosed, FailOpen };  // 注册时声明

struct PreHookResult {
    enum Action { Continue, Deny, ModifyArgs } action = Continue;
    std::unordered_map<std::string, std::string> modified_args;  // ModifyArgs 时生效
    std::string deny_reason;                                     // Deny 时生效
};
struct PostHookResult {
    bool modify_result = false;
    ToolResult modified_result;
};

using PreHook  = std::function<PreHookResult(const ToolMetadata&, const ToolCallContext&,
                                             const std::unordered_map<std::string, std::string>& args)>;
using PostHook = std::function<PostHookResult(const ToolMetadata&, const ToolCallContext&,
                                              const ToolResult&)>;

class IToolHookRegistry {
public:
    virtual ~IToolHookRegistry() = default;
    virtual void register_pre_hook(const std::string& tool_glob, PreHook hook,
                                   int priority, HookErrorPolicy policy) = 0;
    virtual void register_post_hook(const std::string& tool_glob, PostHook hook,
                                    int priority, HookErrorPolicy policy) = 0;
};
```

- `tool_glob` 支持通配 (`shell/*`, `*`)，与 ADR-0043 命名约定一致。
- `priority` 数值小者先执行；同 priority 按注册顺序。
- ToolCoordinator 持有 `IToolHookRegistry*` (可空, 空=无 hook, 零行为变化, 向后兼容)。

### 4. 失败语义

- **同步钩子 only**；异步 hook defer (与 ADR-0030 V2 异步运行时协同, 不在本期)。
- hook 抛异常时按注册声明处理: `FailClosed` (安全类: 预算/合规) → 视为 Deny 并记 `tool.audit.denied`; `FailOpen` (观测类) → 跳过该 hook 继续, 记 warning 到 audit meta。
- post-hook 抛异常同理 (FailClosed → 返回错误 ToolResult; FailOpen → 返回原始结果)。

### 5. 与 ADR-0068 的协同

- `tool.execution.start` 发射点 = pre_hooks 之后、layer check 之前(含最终生效 args); `tool.execution.end` = post_hooks 之后 (含最终生效 result)。
- hook 的 Deny 必须 emit `tool.audit.denied` (reason = hook 名 + deny_reason), 审计链完整。

### 6. 与 ADR-0031 §8 defer 项的协调

本 ADR **不解决** ADR-0031 §8 defer 的语义问题 (`meta.min_layer` 强制语义、成本预算闭环、超时、审批历史持久化——留 C6)。但声明: C6 未来实施如需注入点 (如 `get_layer(meta)` router 钩子), **必须使用本 ADR 的 hook 机制, 不得新增旁路拦截层**——防止长出第二套拦截机制。

### 7. 转 Approved 条件

1. `IToolHookRegistry` 契约 + ToolCoordinator middleware 改造落地;
2. 至少 1 个真实 PDK plugin 使用 pre-hook (建议 budget_agent 超限降级);
3. `tool.execution.*` 发射与 ADR-0068 Registry 对齐;
4. 审计测试: Deny 路径 / ModifyArgs 路径 / FailClosed / FailOpen / post-hook 修改后 audit.completed 内容, 5 类测试通过;
5. ctest 零回归 + `adr_lint` 0 错误。

**估时**: 1 Sprint (middleware 改造 + 注册 API + 测试), 与 Wave 1 分析文档一致。

---

## Ship Evidence (2026-08-04)

- `include/agenticdsl/contract/itool_hook_registry.h` — L3 contract shipped (HookErrorPolicy / PreHookResult / PostHookResult / PreHook / PostHook / IToolHookRegistry).
- `src/common/tools/tool_hook_registry.{h,cpp}` — storage, ADR-0043 glob matching, priority ordering, FailClosed/FailOpen handling shipped.
- `src/common/tools/tool_coordinator.{h,cpp}` — pre/post hook injection; `tool.execution.start/end` 复用 ADR-0068 既有 EventBuilder V2 发射点（不重复发射）; Deny/FailClosed 路径 emit `tool.audit.denied`; null registry 保持原行为。
- `pdk/budget_agent/src/hooks.cpp` — real PDK FailClosed budget pre-hook (`pdk_register_hooks`, tool_glob=`*`) shipped.
- Tests:
  - `tests/test_tool_coordinator_hooks.cpp` — 8 cases (contract compile / matching / Deny / post-hook redaction / null-registry compat / priority / glob / FailOpen / FailClosed) 34 assertions PASS.
  - `tests/test_budget_agent_hooks.cpp` — plugin integration over-budget deny case (dlopen + pdk_register_hooks) PASS.
- ctest: 95/96 PASS 零回归（+2 新测试; 1 pre-existing fail `test_cost_tracking_decorator` 与 ADR-0069 无关, commit `514c441` Phase 5 引入）。
- Status: 🟡 Partial; §决策 7 Approved 条件 1-4 满足, 条件 5 待 `test_cost_tracking_decorator` 修复后复核。
