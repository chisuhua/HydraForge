# pdk-cross-cutting-patterns Specification

## ADDED Requirements

### Requirement: ICrossCuttingPattern 统一抽象接口

The `ICrossCuttingPattern` MUST provide a pure virtual interface with 2 methods: `name()` and `apply(config, ctx)`. All 4 Pattern classes MUST inherit from this interface.

#### Scenario: 4 Pattern 共享接口

- **WHEN** 静态检查 `grep -A2 "class.*Pattern.*public ICrossCuttingPattern" include/agenticdsl/pdk/cross_cutting/*.h | head -8`
- **THEN** 4 Pattern class 全部继承 ICrossCuttingPattern

#### Scenario: Pattern 名称常量

- **WHEN** 静态检查 `grep -E 'cross_cutting_pattern::(Decorator|Hook|Composition|Bus)' include/agenticdsl/pdk/cross_cutting/icross_cutting_pattern.h`
- **THEN** 4 常量必须全部出现，值分别为 `decorator-v1` / `hook-v1` / `composition-v1` / `bus-v1`

### Requirement: CrossCuttingContext 含 6 基础设施引用

The `CrossCuttingContext` MUST contain exactly 6 fields: `agent_registry` / `agent_hook_registry` / `tool_hook_registry` / `bus` / `set_llm_provider` (callback) / `approval_handler`. No additional fields.

#### Scenario: 6 字段完整性

- **WHEN** 静态检查 `grep "agenticdsl::" include/agenticdsl/pdk/cross_cutting/icross_cutting_pattern.h | grep -v "constexpr\|namespace\|using" | wc -l`
- **THEN** ≥ 6 行（6 个 `agenticdsl::` 类型引用）

### Requirement: CrossCuttingOrchestrator 运行期 JSON 分发

The `CrossCuttingOrchestrator::dispatch(config)` MUST iterate `config["patterns"]` and dispatch each pattern by `type` field. Unknown patterns MUST log warning + skip (FailOpen). Invalid schema MUST throw `std::invalid_argument`.

#### Scenario: 基本分发

- **WHEN** 运行 `test_cross_cutting_orchestrator::orchestrator_dispatch_calls_correct_pattern`
- **THEN** 4 patterns 全部正确分发调用 `apply()`

#### Scenario: 未知 pattern FailOpen (M1)

- **WHEN** 运行 `test_cross_cutting_orchestrator::orchestrator_dispatch_unknown_pattern_fail_open`
- **THEN** 未识别的 type 字段 MUST log warning + skip（不抛异常）

#### Scenario: Schema 非法 throw (M1)

- **WHEN** 运行 `test_cross_cutting_orchestrator::orchestrator_dispatch_invalid_schema_throws`
- **THEN** 非法 schema MUST throw `std::invalid_argument`

#### Scenario: 异常隔离（不变量 4）

- **WHEN** Pattern::apply() 抛异常
- **THEN** Orchestrator MUST catch + log + 继续下一个 pattern（不阻断主流程）

#### Scenario: register_pattern 扩展点

- **WHEN** 运行 `test_cross_cutting_orchestrator::orchestrator_register_pattern_adds_new_pattern`
- **THEN** 自定义 Pattern 注册后 MUST 可被 dispatch 调用

### Requirement: DecoratorPattern 通过 wrap_chain + set_llm_provider 注入

The `DecoratorPattern::apply` MUST use `ILLMProviderDecorator::wrap_chain()` (≤4 chain depth hard constraint) and inject via `ctx.set_llm_provider` callback. MUST NOT modify engine.h.

#### Scenario: 装饰链深度约束

- **WHEN** 调用 wrap_chain 传入 5 个 decorators (5 + 1 inner = 6 层)
- **THEN** MUST throw `ILLMProviderDecorator::DecoratorChainTooDeep`

#### Scenario: 合法装饰链注入

- **WHEN** 运行 `test_decorator_pattern::decorator_apply_cost_tracking`
- **THEN** `ctx.set_llm_provider` MUST 被调用，参数为完整装饰链

### Requirement: HookPattern 3 target 类型支持

The `HookPattern::apply` MUST support 3 target types: `tool` (L1 ToolHookRegistry), `agent` (L2 AgentHookRegistry), `approval` (L4 IApprovalHandler).

#### Scenario: tool hook 注册

- **WHEN** 运行 `test_hook_pattern::hook_apply_tool_pre_global`
- **THEN** `ctx.tool_hook_registry` MUST 收到 register_pre_hook 调用

#### Scenario: agent hook 注册

- **WHEN** 运行 `test_hook_pattern::hook_apply_agent_pre_scoped`
- **THEN** `ctx.agent_hook_registry` MUST 收到 register_pre_hook 调用

#### Scenario: approval handler 调用 (L4)

- **WHEN** 运行 `test_hook_pattern::hook_apply_approval_l4`
- **THEN** `ctx.approval_handler->process_request()` MUST 被调用

#### Scenario: HookErrorPolicy 解析

- **WHEN** DSL 配置 `policy: "FailClosed"` 或 `policy: "FailOpen"`
- **THEN** 正确转换为 `agenticdsl::HookErrorPolicy::FailClosed` / `FailOpen`

### Requirement: CompositionPattern 真实 IAgentRegistry API

The `CompositionPattern::apply` MUST use REAL `IAgentRegistry::register_agent()` + `create()` (NOT fictitious `resolve()`/`list()`). Composition accepts `agent_cfg.config` (full AgentConfig with `instance_id`).

#### Scenario: Agent 创建 + hook 注入

- **WHEN** 运行 `test_composition_pattern::composition_apply_creates_agent_and_injects_hook`
- **THEN** `ctx.agent_registry->register_agent()` + `create()` MUST 被调用
- **AND** AgentPreHook MUST 自动注入到 `ctx.agent_hook_registry`

#### Scenario: instance_id 配置

- **WHEN** 运行 `test_composition_pattern::composition_apply_uses_agent_cfg_instance_id`
- **THEN** `create()` MUST 使用 DSL 配置的 `instance_id`

#### Scenario: 重复注册返回 false（不抛）

- **WHEN** 同一 agent name 重复 register_agent
- **THEN** MUST 返回 false（真实 IAgentRegistry 行为，非抛异常）

### Requirement: BusPattern 订阅并转发

The `BusPattern::apply` MUST use `IInteractionBus::subscribe()` for each topic pattern in `config["subscriptions"]`. Event handler MUST forward to configured handler.

#### Scenario: 单主题订阅

- **WHEN** 运行 `test_bus_pattern::bus_apply_subscribe_topic_pattern`
- **THEN** `ctx.bus->subscribe(topic, callback)` MUST 被调用

#### Scenario: 多主题订阅

- **WHEN** 运行 `test_bus_pattern::bus_apply_multiple_subscriptions`
- **THEN** N 个订阅 MUST 全部调用

### Requirement: DSL 加载器 YAML 解析

The `CrossCuttingConfig::load(yaml_path)` MUST parse `examples/cross_cutting/dsl/*.cc.md` format with `### AgenticDSL /<section>` markers + ` ```yaml ` blocks. MUST validate schema: `patterns` array required, `type` ∈ {4 pattern names}, `config` object required.

#### Scenario: 加载合法 YAML

- **WHEN** 运行 `test_cross_cutting_dsl::dsl_load_high_security_mode_yaml`
- **THEN** 加载 MUST 成功，返回非空 config

#### Scenario: 非法 schema 抛异常

- **WHEN** 运行 `test_cross_cutting_dsl::dsl_load_invalid_schema_throws`
- **THEN** MUST throw `std::invalid_argument`

#### Scenario: 3 DSL examples 真实存在

- **WHEN** `ls examples/cross_cutting/dsl/*.cc.md`
- **THEN** 3 文件 MUST 存在: `high_security_mode.cc.md` / `cost_optimization_mode.cc.md` / `development_mode.cc.md`

### Requirement: E2E 完整管道

The E2E tests MUST integrate 4 patterns + DSL loader + Orchestrator dispatch + real 6-layer abstraction verification.

#### Scenario: high_security_mode 全管道

- **WHEN** 运行 `test_cross_cutting_e2e::e2e_high_security_mode_full_pipeline`
- **THEN** DSL load + Orchestrator dispatch + 4 patterns apply MUST 全部生效
- **AND** 真实 IAgentRegistry / IAgentHookRegistry / IToolHookRegistry / IInteractionBus MUST 收到调用

#### Scenario: cost_optimization_mode 极简管道

- **WHEN** 运行 `test_cross_cutting_e2e::e2e_cost_optimization_mode_minimal`
- **THEN** 2 patterns (decorator + bus) MUST apply 成功

### Requirement: 既有契约零修改（Oracle B3 关键不变量）

The V1 implementation MUST NOT modify any of: `i_llm_provider_decorator.h` / `iinteraction_bus.h` / `itool_hook_registry.h` / `iagent_hook_registry.h` / `iagent_registry.h` / `iagent_composition.h` / `event_builder.h` / `ievaluator.h` / `imutation_governance.h` / `src/core/engine.h` / `adr-0068-event-emission-contract.md`.

#### Scenario: 10 个文件 git diff 0 行

- **WHEN** `git diff HEAD -- <10 files>`
- **THEN** 全部 0 行（V1 仅新增代码，零既有文件修改）

### Requirement: ctest 全量零回归

The `ctest --output-on-failure` MUST report ALL tests PASS with zero regressions relative to T21 payload redact baseline (190+ tests).

#### Scenario: ctest 全量 PASS

- **WHEN** 运行 `ctest --output-on-failure`
- **THEN** 所有测试 PASS, 0 failures（pre-existing 1 timing flake 不计入）
- **AND** 测试计数 ≥ T21 baseline + 18（动态计数, 禁止硬编码）

#### Scenario: test_cross_cutting_* 专项

- **WHEN** 运行 `ctest --output-on-failure -R test_cross_cutting`
- **THEN** ≥ 18 cases PASS（5 orchestrator + 2 decorator + 3 hook + 2 composition + 2 bus + 2 dsl + 2 e2e）

### Requirement: ADR-0085 状态翻转 ✅ Approved

After V1 ship, the ADR-0085 header MUST show `✅ Approved` with ship evidence (commit hash + test count + ctest baseline).

#### Scenario: ADR-0085 状态字段更新

- **WHEN** 静态检查 `grep "✅ Approved" docs/adr/adr-0085-cross-cutting-pattern-pdk.md | head -3`
- **THEN** ADR-0085 MUST 显示 ✅ Approved

#### Scenario: cap-map §一 +1 新能力

- **WHEN** 静态检查 `grep "Cross-Cutting Pattern PDK" docs/architecture/capability-application-map-2026-08.md | head -3`
- **THEN** §一表格 MUST 新增 Cross-Cutting Pattern PDK V1 能力行