# pdk-cross-cutting-patterns

## Why

ADR-0085 (Cross-Cutting Pattern PDK) 🔍 Proposed（待评审转 ✅ Approved，文档已 ship v1.2 含 Oracle 评审修正 + High/Medium 全 修正）定义了 4 PDK Pattern + CrossCuttingOrchestrator + ICrossCuttingPattern 抽象 + 横切功能 DSL。本 change 是 ADR-0085 的实施载体。

**Oracle 横切审查发现** (cross-cutting-architecture v1.2 §11)：
- **T17/T15/IEvaluator V2/T21**: 零硬编码横切关注点 ✅（最佳实践）
- **T19 GEPA + T20 MCTS**: 2 处硬编码横切关注点（Yolo 模式硬编码 + L0 装饰链未复用）
- **G11 Mutation**: 零硬编码（横切实践参照基准）
- **T21 Payload**: ✅ 已 ship 修复（commit t21-payload-redact）

**审计依据**:
- ADR-0085 Cross-Cutting Pattern PDK（🔍 Proposed → 待评审 → ✅ Approved）
- ADR-0021 PDK Design ✅ Approved（PDK Plugin 范式）
- ADR-0068 Event Emission Contract ✅ Approved（27+ 主题注册）
- ADR-0069 ToolCoordinator Hook ✅ Approved（HookErrorPolicy）
- ADR-0081 Pre-Step Hook Contract ✅ Approved（IAgentHookRegistry）
- ADR-0082 Agent First-Class Registry ✅ Approved（IAgentRegistry）
- ADR-0083 IEvaluator ✅ Approved + ship
- ADR-0084 Mutation Governance ✅ Approved + ship
- 横切架构文档 `docs/architecture/cross-cutting-hooks-architecture-2026-08.md` v1.2（1583 行）

**前置依赖**（全部已满足）:
- ✅ 6 层抽象扩展点全部 ship（L0 ILLMProviderDecorator + L1 ToolHookRegistry + L2 AgentHookRegistry + L3 IAgentRegistry + L4 IApprovalHandler + L5 IInteractionBus）
- ✅ ADR-0085 v1.2 文档 + Oracle 评审修正完成（2 commits）
- ✅ T21 Payload Redact 已 ship（hash-only PII defense 范式参考）
- ✅ Engine.h:130 `set_llm_provider` API 已 ship（DecoratorPattern 注入通道）
- ✅ HookErrorPolicy (FailClosed/FailOpen) 已 ship
- ✅ EventBuilder API 已 ship
- ✅ nlohmann::json 已 vendor（DSL YAML 解析）

## What Changes

### Phase 0 契约层（4 文件 + 1 抽象）

1. **`include/agenticdsl/pdk/cross_cutting/icross_cutting_pattern.h`** (新)
   - `class ICrossCuttingPattern` 抽象接口
   - `struct CrossCuttingContext` (6 字段: agent_registry / agent_hook_registry / tool_hook_registry / bus / set_llm_provider 回调 / approval_handler)
   - 常量: `cross_cutting_pattern::Decorator/Hook/Composition/Bus`

2. **`include/agenticdsl/pdk/cross_cutting/cross_cutting_orchestrator.h`** (新)
   - `class CrossCuttingOrchestrator`（无状态 dispatcher）
   - 构造签名: `CrossCuttingOrchestrator(... , std::vector<unique_ptr<ICrossCuttingPattern>> patterns = {})`
   - `dispatch(const nlohmann::json& config)` 主入口
   - `register_pattern(std::unique_ptr<ICrossCuttingPattern>)` 扩展点

3. **`include/agenticdsl/pdk/cross_cutting/decorator_pattern.h`** (新)
4. **`include/agenticdsl/pdk/cross_cutting/hook_pattern.h`** (新)
5. **`include/agenticdsl/pdk/cross_cutting/composition_pattern.h`** (新)
6. **`include/agenticdsl/pdk/cross_cutting/bus_pattern.h`** (新)

### Phase 1 实现层（4 .cpp + 1 主文件）

7. **`src/common/governance/cross_cutting/cross_cutting_orchestrator.cpp`**
   - 运行期 JSON 分发（基于 pattern type 字段）
   - 未知 pattern → FailOpen（log warning + skip），throw 仅限 schema 非法
   - 默认注册 4 内置 pattern（V1 硬编码简化，M7）

8. **`src/common/governance/cross_cutting/decorator_pattern.cpp`**
   - 接收 `decorators: ["CostTracking", "Compliance", "PII-Scrub"]` 配置
   - 调用 `ILLMProviderDecorator::wrap_chain()` 构造装饰链（链深 ≤4 硬约束）
   - 通过 `ctx.set_llm_provider` 回调（用户构造时绑定到 DSLEngine::set_llm_provider）
   - **零修改 engine.h**（Oracle B1+B3 关键不变量）

9. **`src/common/governance/cross_cutting/hook_pattern.cpp`**
   - 接收 `hooks: [...]` 配置（target=tool/agent/approval + glob + priority + policy + handler）
   - target=tool → `ctx.tool_hook_registry->register_pre_hook()`
   - target=agent → `ctx.agent_hook_registry->register_pre_hook()`
   - target=approval → `ctx.approval_handler->process_request()`（L4 通道）
   - HookErrorPolicy 解析 (FailClosed/FailOpen)

10. **`src/common/governance/cross_cutting/composition_pattern.cpp`**
    - 接收 `agents: [{name, scope, config}]` 配置
    - `ctx.agent_registry->register_agent(name, factory)`
    - `ctx.agent_registry->create(name, AgentConfig{instance_id})`
    - 自动注入 AgentPreHook 到 `ctx.agent_hook_registry`
    - **真实 IAgentRegistry API** (Oracle H1): register_agent/create/unregister

11. **`src/common/governance/cross_cutting/bus_pattern.cpp`**
    - 接收 `subscriptions: [...]` 配置
    - `ctx.bus->subscribe(topic_pattern, callback)`
    - 事件转发到 handler（V1: 简单 log + metrics counter）

12. **`src/common/governance/cross_cutting/CMakeLists.txt`** (新)
    - 注册 5 个 .cpp 文件
    - 链接 agenticdsl_core + agenticdsl_contract + agenticdsl_policy + agenticdsl_llm

### Phase 2 DSL 加载器

13. **`include/agenticdsl/pdk/cross_cutting/cross_cutting_config.h`** (新)
    - `class CrossCuttingConfig` 加载 + 解析 YAML
    - 复用 ADR-0073 nlohmann JSON Schema 校验器（V1 简化: 仅字段必填校验）

14. **`src/common/governance/cross_cutting/cross_cutting_config.cpp`**
    - YAML → JSON 转换（轻量：直接逐行解析）
    - 字段校验：patterns 数组必填、type ∈ {decorator-v1, hook-v1, composition-v1, bus-v1}
    - 错误处理：throw std::invalid_argument（schema 非法时）

15. **`examples/cross_cutting/dsl/high_security_mode.cc.md`** (新)
    - YAML DSL 示例（high security mode）
    - 4 patterns 全启用: decorator + hook (L3 approval) + composition (privacy-policy) + bus (mutation.* → siem)

16. **`examples/cross_cutting/dsl/cost_optimization_mode.cc.md`** (新)
    - 简化示例（仅 decorator + bus，无 hook/composition）

17. **`examples/cross_cutting/dsl/development_mode.cc.md`** (新)
    - 开发模式（metrics + debug logging）

### Phase 3 测试（≥10 cases）

18. **`tests/test_cross_cutting_orchestrator.cpp`** (新)
    - `orchestrator_dispatch_calls_correct_pattern` (基本分发)
    - `orchestrator_dispatch_unknown_pattern_fail_open` (M1: 不 throw)
    - `orchestrator_dispatch_invalid_schema_throws` (M1: schema 非法 throw)
    - `orchestrator_register_pattern_adds_new_pattern` (扩展点)
    - `orchestrator_dispatch_multiple_patterns_in_order` (顺序保证)

19. **`tests/test_decorator_pattern.cpp`** (新, ≥2 cases)
    - `decorator_apply_cost_tracking`
    - `decorator_apply_chain_depth_limit` (≤4 硬约束)

20. **`tests/test_hook_pattern.cpp`** (新, ≥3 cases)
    - `hook_apply_tool_pre_global`
    - `hook_apply_agent_pre_scoped` (per-agent type glob)
    - `hook_apply_approval_l4` (L4 ApprovalHandler 调用)

21. **`tests/test_composition_pattern.cpp`** (新, ≥2 cases)
    - `composition_apply_creates_agent_and_injects_hook`
    - `composition_apply_uses_agent_cfg_instance_id`

22. **`tests/test_bus_pattern.cpp`** (新, ≥2 cases)
    - `bus_apply_subscribe_topic_pattern`
    - `bus_apply_multiple_subscriptions`

23. **`tests/test_cross_cutting_dsl.cpp`** (新, ≥2 cases)
    - `dsl_load_high_security_mode_yaml`
    - `dsl_load_invalid_schema_throws`

24. **`tests/test_cross_cutting_e2e.cpp`** (新, ≥2 cases)
    - `e2e_high_security_mode_full_pipeline` (4 patterns + DSL load + dispatch)
    - `e2e_cost_optimization_mode_minimal`

### Phase 4 文档同步 + ship

25. 更新 `docs/architecture/cross-cutting-hooks-architecture-2026-08.md` v1.2 → v1.3 (实施注记)
26. 更新 `docs/adr/adr-0085-cross-cutting-pattern-pdk.md` 头部：🔍 Proposed → ✅ Approved (ship 2026-08-XX)
27. 更新 `docs/README.md` §adr/ 表格：ADR-0085 行状态翻 ✅
28. 更新 `docs/architecture/capability-application-map-2026-08.md`:
    - §一 +1（新能力 #29: Cross-Cutting Pattern PDK V1）
    - §八新增 T26 任务（T17/T15/T19/T20/T21 后续横切化任务）
29. 更新 `docs/active-status.md` §一 T26 跟踪段
30. `openspec archive pdk-cross-cutting-patterns`

## Impact

**影响范围**:
- **新代码**: 5 .h + 5 .cpp + 6 test files + 3 DSL examples + 2 CMakeLists（零既有文件修改除文档）
- **零契约变更**: IInteractionBus / EventBuilder / ToolResult / ILLMProvider / IAgentRegistry 等公开 API **零修改**（Oracle B3 关键不变量）
- **零既有测试变更**: 既有 190+ tests 全保留，仅新增 ≥13 cases

**下游影响**:
- 解锁 **T19 GEPA 反射路径注入 L0 装饰链**（CostTracking/Compliance/Retry → 反思 token 计费 + 审计）
- 解锁 **T20 MCTS mutation 参数配置化**（替代硬编码 Yolo 模式）
- 解锁 **横切功能 Marketplace**（社区贡献新 Pattern 通过 register_pattern()）
- 解锁 **Meta-Agent 自管理 V2**（基于 Orchestrator 的自适应系统）
- 解锁 **T15 to_otel_spans** BusPattern 消费（`mcts.*`/`gepa.*` → OTel SIEM）

**V1 边界**（per ADR-0085 D6）:
- ✅ 4 PDK Pattern class + Orchestrator + ICrossCuttingPattern + DSL 加载器
- ✅ ≥10 测试 cases（实际 ≥13）
- ✅ 3 DSL examples（high_security / cost_optimization / development）
- ⏸ V1 不实施：Meta-Agent 自管理（V2 deferred）
- ⏸ V1 不实施：Hot-Reload 反向取消（V2）
- ⏸ V1 不实施：横切功能 Marketplace（V2）
- ⏸ V1 不实施：跨 Pattern 依赖编排（V2）

**Breaking Changes**: 无（仅新增 PDK 子模块）

## ship gate 验证

- `python3 tools/adr_lint.py` 通过（≥83 ADR）
- `python3 tools/docs_drift_audit.py` 通过（无新增 CRITICAL drift）
- `openspec validate --changes --strict` PASS
- `ctest --output-on-failure` 全量 0 回归（动态基线，约 190+ → 203+）
- `ctest -R test_cross_cutting` ≥ 13 cases / ≥ 40 assertions PASS
- `ctest -R test_decorator|hook|composition|bus_pattern` ≥ 9 cases PASS
- `git diff HEAD -- include/agenticdsl/contract/ src/core/engine.h` 0 行（Oracle B3 关键不变量）
- ADR-0085 状态翻 ✅ Approved + ship 注记
- cap-map §一 +1（新能力 #29）
- 3 DSL examples 文件存在且语法合法

## 关联文档

- `docs/adr/adr-0085-cross-cutting-pattern-pdk.md` v1.2（设计依据，417 行）
- `docs/architecture/cross-cutting-hooks-architecture-2026-08.md` v1.2（架构参考，1583 行）
- ADR-0021 / ADR-0068 / ADR-0069 / ADR-0081 / ADR-0082 / ADR-0083 / ADR-0084（已 ship）
- `include/agenticdsl/contract/i_llm_provider_decorator.h`（L0 装饰基类）
- `include/agenticdsl/contract/iagent_registry.h`（L3 Agent 注册）
- `include/agenticdsl/contract/itool_hook_registry.h`（L1 工具 hook）
- `include/agenticdsl/contract/iagent_hook_registry.h`（L2 Agent hook）
- `include/agenticdsl/policy/iapproval_handler.h`（L4 Approval）
- `include/agenticdsl/contract/iinteraction_bus.h`（L5 事件总线）
- `src/core/engine.h:130` `set_llm_provider` API（DecoratorPattern 注入通道）
- `src/common/governance/mutation_governor.cpp`（G11 横切实践参照）
- `include/agenticdsl/prompt/prompt_hash.h`（T21 payload redact 范式）
- `openspec/specs/pdk-cross-cutting-patterns/`（本次 ship 产出的 spec）