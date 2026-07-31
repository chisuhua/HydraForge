# adr-0069-tool-coordinator-hooks

**优先级**: P0 | **来源**: ADR-0069（D3 立项, 2026-07-31）+ layer-based-missing-capabilities-analysis.md §三 X2 / §五 L1-2 + active-status Wave 1 #4
**阶段**: wave-1 | **分类**: core-impl
**类型**: feature

## 架构依据
- ADR-0069 全文：管辖边界（工具链钩子归 0069，审批语义归 ADR-0031，LLM 上下文钩子归 L1-4，会话钩子归 L1-3）、pre/post 双列表模型（否决洋葱链）、`IToolHookRegistry` L3 契约、HookErrorPolicy 失败语义、ADR-0068 发射点同源、ADR-0031 §8 defer 协调条款。
- 实证基线：`ToolCoordinator::execute()` 6 步线性流（`src/common/tools/tool_coordinator.h` 头注释），ApprovalHandler 仅二元批准/拒绝不可修改；pi-agent `beforeToolCall`/`afterToolCall` 无对应载体。
- 窗口期：ToolCoordinator 为 Sprint 14 新组件，调用方少（NodeExecutor 优先级链），此时加注入点零破坏。

## 范围
- **In Scope**: `include/agenticdsl/contract/itool_hook_registry.h`（新建 L3 契约：PreHook/PostHook/HookErrorPolicy/IToolHookRegistry）；ToolCoordinator middleware 改造（pre_hooks → layer check → ApprovalHandler → audit.invoked → call_tool → post_hooks → audit.completed）；`tool.execution.start/end` 发射（与 ADR-0068 Registry 对齐，audit 同源点）；hook Deny 路径 emit `tool.audit.denied`；5 类测试（Deny/ModifyArgs/FailClosed/FailOpen/post-hook 后 audit.completed 内容为修改后结果）；1 个真实 plugin 使用 pre-hook（建议 budget_agent 超限降级）。
- **Out Scope**: `transformContext` LLM 上下文钩子（L1-4 decorator 链决策）；session 生命周期钩子（L1-3/ADR-0068）；异步 hook（defer ADR-0030 V2）；ADR-0031 §8 defer 语义项（min_layer 强制/成本闭环/超时/审批历史，留 C6，但 C6 注入点必须用本机制）。

## 关键场景
- GIVEN plugin 注册 pre-hook（`shell/*`, priority 10, FailClosed），WHEN 调用 `shell/exec`，THEN hook 先执行且可修改 args；hook 返回 Deny 时 bus 收到 `tool.audit.denied`（reason 含 hook 名），layer check 与 ApprovalHandler 不执行。
- GIVEN hook 抛异常，WHEN 策略为 FailClosed，THEN 视为 Deny；WHEN 策略为 FailOpen，THEN 跳过该 hook 继续执行并记 warning 到 audit meta。
- GIVEN post-hook 修改 result，WHEN 流程结束，THEN `tool.execution.end` 与 `audit.completed` 的 payload 为**修改后**结果。
- GIVEN 无 hook 注册（IToolHookRegistry* 为空），WHEN 任意工具调用，THEN 行为与改造前逐字节一致（向后兼容）。

## 技术约束
- MUST layer check 与 ApprovalHandler 保持一等公民硬门，hook 不可绕过/禁用；MUST 同步钩子 only。
- MUST hook 排序：priority 升序，同 priority 按注册顺序；MUST `tool_glob` 遵循 ADR-0043 命名约定。
- MUST NOT 新增第二套拦截/旁路机制（C6 defer 项未来注入点复用本机制）；MUST NOT 修改 ApprovalHandler 公开 API。
- SHOULD ToolCoordinator 改造与 hook 契约分 commit；SHOULD budget_agent pre-hook 作为首个真实用例。

## 验收标准
- 5 类测试全部通过（Deny/ModifyArgs/FailClosed/FailOpen/audit 最终一致性）+ 无 hook 路径零行为变化回归。
- `tool.execution.start/end` 真实发射且与 ADR-0068 附录 A Registry 对齐。
- 1 个真实 plugin pre-hook 用例落地（budget_agent）。
- ctest 全量零回归；`python3 tools/adr_lint.py` 0 错误；`python3 tools/docs_drift_audit.py` 0 DRIFT。
- ADR-0069 状态可转 🟡 Partial（转 Approved 条件见其 §决策 7）。
