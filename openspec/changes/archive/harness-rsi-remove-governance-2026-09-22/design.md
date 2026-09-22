# Harness-RSI Remove Governance — Design

## Context

C4 `harness-rsi-pilot` ship 后，Oracle 独立审查 (bg_afa84d4d) 确认 3 个已验证债务：

1. **remove 零治理** (harness_rsi.cpp:171-177): Gate 2 只循环 `tools_add` (L131-136)，`tools_remove` 路径直接 `registry.unregister_tool_function()` 无任何 `is_tool_allowed` 检查 → 可移除审批 hook 类安全工具而无拦截
2. **SecureToolRegistry 裸委托** (secure_tool_registry.cpp:266-268): `unregister_tool_function` 直接委托 wrapped registry，无 `check_security`（对比 `call_tool` 有完整安全检查）
3. **ToolRegistry 无 mutex** (registry.h:87-88): `tools_` / `tool_metadata_` 无锁保护，unregister 引入"运行期改全局 registry"这一此前不存在的并发风险面

外加: `trace_id: ""` 空值 (harness_rsi.cpp:121) 切断因果链（模式 #7 教训: 语义字段必须有稳定标识符）。

## Goals / Non-Goals

**Goals**:
- tools_remove 路径与 tools_add 治理对称（零状态变更契约不变量保持）
- SecureToolRegistry unregister 过安全检查
- ToolRegistry mutation 并发风险最小化（或显式文档化约束）
- trace_id 从 ctx 透传

**Non-Goals**:
- 不改 `MutationGovernancePolicy` 结构（denied_tools 语义不变）
- 不做 per-mutation scope registry（Wave 3 再评估）
- 不引入完整并发模型（ToolRegistry 全类加锁超出本 change，Wave 3+ 评估）
- 不修复 `eval_quality:"Unknown"` 硬编码（独立 change: EvolutionVerdict 加字段是 C3 API 面变更）

## Decisions

### D1: Gate 2 扩展为 tools_add + tools_remove 对称检查

**决策**: `apply_harness_mutation` Gate 2 从只循环 `tools_add` 扩展为同时循环 `tools_add` + `tools_remove`，任一被 policy 拒绝 → `GovernanceDenied` + 零状态变更。

**Rationale**: 对称治理是 ADR-0084 精神（变异对象 L1-L4 分级 + 授权绑定）的直接应用。remove 是比 add 更危险的变异（全局 blast radius），无理由豁免。

**Alternatives**:
- (a) 只在 `MutationGovernancePolicy` 加 `denied_removals` 独立列表 → 过度设计，pilot 阶段单 policy 足够
- (b) 显式声明 remove 豁免语义（文档化"remove 不需要治理"）→ 与 ADR-0084 fail-closed 攻击面相悖

### D2: SecureToolRegistry unregister 安全检查

**决策**: `SecureToolRegistry::unregister_tool_function` 增加 `is_disabled(name)` 检查——disabled 工具**静默忽略** unregister（不委托到 wrapped registry），与 `call_direct` 的 disabled 检查一致。

**Rationale**: SecureToolRegistry 的存在意义 = 安全层不透明。disabled 工具是显式安全声明，绕过它移除是 fail-open。

**语义锁定**: `IToolRegistry::unregister_tool_function` 签名是 `void` (itool_registry.h:133) — **无返回值**。拒绝语义只能是静默忽略（不修改底层 registry），调用方通过 `has_tool()` 事后可观察。spec R2 scenario 已用 `has_tool == true` 断言，测试可行。

**Alternatives**:
- (a) 完整 `check_security(name, args)` 路径 → unregister 无 args，check_security 需空 args 变体，过度
- (b) 不检查（现状）→ 安全洞，Oracle 已确认必修
- (c) 改签名返回 bool → 破坏 IToolRegistry 纯虚 + 25 文件 override + 触发 contract 变更（与 Change 3 循环依赖），**否决**

### D3: ToolRegistry mutation mutex（最小并发面）

**决策**: `register_tool_function` / `unregister_tool_function` / `register_llm_tool` 三个写路径增加 `std::mutex mutation_mutex_`（1 把锁）；读路径 (has_tool / list_tools / call_tool / is_llm_tool / get_llm_params) 不加锁——文档化"读路径假设写路径单线程或外部同步"。

**Rationale**: Oracle 最低成本方案 = "显式文档化单线程 mutation 约束 + 在 unregister/register 加一把 mutex"。**`llm_tools_` (registry.h:95) 同样是可变 map，`register_llm_tool` 是第三个写路径，必须纳入同一把锁**（否则留下未保护变异面）。写写互斥是并发安全的 90% 收益，读写加锁需要 shared_mutex 升级，超出 pilot 需求。

**Alternatives**:
- (a) 全类 shared_mutex → 所有读路径加锁，改造面大，pilot 无并发读需求
- (b) 仅文档化不加锁 → Oracle 明确"应修"（unregister 引入全局变异面）
- (c) 只锁 register/unregister 不锁 register_llm_tool → 漏第三写路径，spec 场景可测出回归，**否决**

### D4: trace_id 透传

**决策**: `MutationGateContext` 增加 `std::string trace_id` 字段（默认空），`harness_rsi.cpp` 发射事件时用 `ctx.trace_id` 替代硬编码 `""`。

**Rationale**: 模式 #7 教训（语义字段必须有稳定标识符）+ ADR-0068 因果链需求。调用方（Wave 3 Model-RSI orchestrator）负责从 session/context 传入真实 trace_id。

**非 BREAKING 声明**: `MutationGateContext` 是聚合结构，`std::string trace_id` **追加到末尾 + 默认值兜底**时，既有位置初始化（少给尾参）与指定初始化（顺序匹配）均不破坏——design 原 BREAKING 定性过度，修正为"非 BREAKING，仅新增断言需更新测试"。

## Risks / Trade-offs

- [Gate 2 扩展遗漏 tools_remove 某子路径] → 测试覆盖 add+remove 双拒绝 case，回归守卫
- [mutex 死锁（如果未来 remove 回调内部调 register）] → mutation_mutex_ 非递归，文档化"回调不得反向注册"；锁序 SecureToolRegistry.mutex_ → ToolRegistry.mutation_mutex_ 单向，无倒置
- [trace_id 空值在 Wave 3 前仍可能出现在部分调用] → 默认空保持向后兼容，Wave 3 调用方必填
- [SecureToolRegistry disabled 拒绝 unregister 语义] → **锁定为静默忽略**（void 签名无返回值，itool_registry.h:133），调用方经 `has_tool()` 事后可观察
- [remove 不存在的工具 = 幂等 no-op] → `unregister_tool_function` 对不存在工具为 no-op（erase 返回 0），`applied_tools_removed` 仍记录该名（AppliedMutation 记录语义为"请求移除"而非"实际移除"）——spec 明确此语义，避免误读
- [remove 全局 blast radius（跨 agent）] → pilot 单 agent 验证；spec scenario 显式标注"blast radius 按 pilot 语义 = 全局"，Wave 3 多 agent 场景需 registry scope 隔离（登记为 Wave 3 前置评估项）

## Migration Plan

1. 本 change 独立 ship（原子 commit），不触碰 C4 已 archive 的 spec
2. `MutationGateContext` + trace_id 字段（末尾追加 + 默认值，非 BREAKING）：仅新增断言需更新测试，既有构造点无需改动
3. 回归守卫: 既有 9 cases / 43 assertions 零回归 + 新增 remove-governance cases
4. 回滚: 单 commit revert，无跨文件纠缠（除 trace_id 字段外全部局部）

## Open Questions

- (无 — Oracle 已给出 D1-D4 明确方向，pilot 无歧义)
