# Tasks: pdk-cross-cutting-patterns

> **TDD 5 步结构**: 每任务按 Write failing test → Verify fail → Implement → Verify pass → Commit
> **关键不变量**（Oracle 审查 B3 + ADR-0085 不变量 1）: 既有契约 **零修改** — `include/agenticdsl/contract/` 8 个头 + `src/core/engine.h` 全部 0 diff
> **设计依据**: ADR-0085 Cross-Cutting Pattern PDK v1.2 + cross-cutting-architecture v1.2

## Phase 0: 抽象层（ICrossCuttingPattern + Orchestrator + 4 Pattern 类骨架）

- [ ] **T0.1** Write failing test: `tests/test_cross_cutting_orchestrator.cpp` 骨架（≥ 5 cases 占位）
- [ ] **T0.2** Verify fail: 编译失败（`fatal error: 'agenticdsl/pdk/cross_cutting/icross_cutting_pattern.h' file not found`）
- [ ] **T0.3** Implement: `include/agenticdsl/pdk/cross_cutting/icross_cutting_pattern.h`:
  ```cpp
  namespace hydraforge::pdk {
  class ICrossCuttingPattern {
  public:
      virtual ~ICrossCuttingPattern() = default;
      virtual const std::string& name() const = 0;
      virtual void apply(const nlohmann::json& pattern_config,
                        CrossCuttingContext& ctx) = 0;
  };
  struct CrossCuttingContext {
      agenticdsl::IAgentRegistry* agent_registry;
      agenticdsl::IAgentHookRegistry* agent_hook_registry;
      agenticdsl::IToolHookRegistry* tool_hook_registry;
      agenticdsl::IInteractionBus* bus;
      std::function<void(std::unique_ptr<agenticdsl::ILLMProvider>)> set_llm_provider;
      agenticdsl::IApprovalHandler* approval_handler;
  };
  namespace cross_cutting_pattern {
      constexpr const char* Decorator = "decorator-v1";
      constexpr const char* Hook = "hook-v1";
      constexpr const char* Composition = "composition-v1";
      constexpr const char* Bus = "bus-v1";
  }
  }
  ```
- [ ] **T0.4** Implement: `include/agenticdsl/pdk/cross_cutting/{decorator,hook,composition,bus}_pattern.h` 4 个 Pattern 类声明（apply 方法 stub）
- [ ] **T0.5** Implement: `include/agenticdsl/pdk/cross_cutting/cross_cutting_orchestrator.h`:
  ```cpp
  class CrossCuttingOrchestrator {
  public:
      CrossCuttingOrchestrator(agenticdsl::IAgentRegistry&, agenticdsl::IAgentHookRegistry&,
                                agenticdsl::IToolHookRegistry&, agenticdsl::IInteractionBus&,
                                std::vector<unique_ptr<ICrossCuttingPattern>> patterns = {});
      void dispatch(const nlohmann::json& config);
      void register_pattern(std::unique_ptr<ICrossCuttingPattern> pattern);
  };
  ```
- [ ] **T0.6** Verify pass: 编译成功 + 5 cases 编译通过（运行时仍 FAIL）
- [ ] **T0.7** Commit: `feat(pdk): CrossCuttingPattern + Orchestrator contracts (T0)`

## Phase 1: 4 Pattern 实现（核心）

- [ ] **T1.1** Write failing test: `tests/test_decorator_pattern.cpp` ≥ 2 cases
  - `decorator_apply_cost_tracking` (CostTrackingDecorator 装饰链)
  - `decorator_apply_chain_depth_limit` (链深 ≤4 硬约束验证)
- [ ] **T1.2** Verify fail: 2 cases FAIL
- [ ] **T1.3** Implement: `src/common/governance/cross_cutting/decorator_pattern.cpp`:
  - 读取 `config["decorators"]` 数组
  - 调用 `ILLMProviderDecorator::wrap_chain(innermost, decorator_factories)`
  - 通过 `ctx.set_llm_provider(std::move(chain))` 注入（用户构造时绑定到 DSLEngine）
  - **关键**: 链深 ≤4 硬约束（超过抛 `DecoratorChainTooDeep`）
- [ ] **T1.4** Verify pass: 2 cases PASS
- [ ] **T1.5** Write failing test: `tests/test_hook_pattern.cpp` ≥ 3 cases
  - `hook_apply_tool_pre_global` (target=tool, glob="*")
  - `hook_apply_agent_pre_scoped` (target=agent, glob="react-loop/*")
  - `hook_apply_approval_l4` (target=approval, 调用 approval_handler->process_request)
- [ ] **T1.6** Verify fail: 3 cases FAIL
- [ ] **T1.7** Implement: `src/common/governance/cross_cutting/hook_pattern.cpp`:
  - 读取 `config["hooks"]` 数组
  - target=tool → `ctx.tool_hook_registry->register_pre_hook()`
  - target=agent → `ctx.agent_hook_registry->register_pre_hook()`
  - target=approval → `ctx.approval_handler->process_request()` + 构造 ToolPreview
  - HookErrorPolicy 解析 (FailClosed/FailOpen)
- [ ] **T1.8** Verify pass: 3 cases PASS
- [ ] **T1.9** Write failing test: `tests/test_composition_pattern.cpp` ≥ 2 cases
  - `composition_apply_creates_agent_and_injects_hook`
  - `composition_apply_uses_agent_cfg_instance_id`
- [ ] **T1.10** Verify fail: 2 cases FAIL
- [ ] **T1.11** Implement: `src/common/governance/cross_cutting/composition_pattern.cpp`:
  - 读取 `config["agents"]` 数组
  - `ctx.agent_registry->register_agent(name, factory)`
  - `ctx.agent_registry->create(name, AgentConfig{instance_id})`
  - 自动注入 AgentPreHook 到 `ctx.agent_hook_registry`
  - **真实 API**: `register_agent` 返回 bool（重复注册 false 不抛）
- [ ] **T1.12** Verify pass: 2 cases PASS
- [ ] **T1.13** Write failing test: `tests/test_bus_pattern.cpp` ≥ 2 cases
  - `bus_apply_subscribe_topic_pattern`
  - `bus_apply_multiple_subscriptions`
- [ ] **T1.14** Verify fail: 2 cases FAIL
- [ ] **T1.15** Implement: `src/common/governance/cross_cutting/bus_pattern.cpp`:
  - 读取 `config["subscriptions"]` 数组
  - `ctx.bus->subscribe(topic, callback)` 每条
  - 事件回调: 转发到 handler（V1: log + counter；V2: 实际 adapter agent）
- [ ] **T1.16** Verify pass: 2 cases PASS + 全部既有测试零回归
- [ ] **T1.17** Commit: `feat(pdk): 4 Pattern implementations (Decorator/Hook/Composition/Bus) (T1)`

## Phase 2: Orchestrator 主实现 + DSL 加载器

- [ ] **T2.1** Write failing test: `tests/test_cross_cutting_orchestrator.cpp` ≥ 5 cases (扩展)
  - `orchestrator_dispatch_calls_correct_pattern`
  - `orchestrator_dispatch_unknown_pattern_fail_open` (M1: log warning + skip)
  - `orchestrator_dispatch_invalid_schema_throws` (M1: throw std::invalid_argument)
  - `orchestrator_register_pattern_adds_new_pattern` (扩展点)
  - `orchestrator_dispatch_multiple_patterns_in_order`
- [ ] **T2.2** Verify fail: 5 cases FAIL
- [ ] **T2.3** Implement: `src/common/governance/cross_cutting/cross_cutting_orchestrator.cpp`:
  - 构造函数: 接收 6 引用 + 默认注册 4 内置 Pattern
  - `dispatch(config)`: 遍历 config["patterns"]，每项按 type 分发
  - 未知 pattern → log warning + skip (M1 FailOpen)
  - schema 非法 → throw std::invalid_argument
  - 异常隔离: 每个 pattern apply() 用 try-catch 包裹（fail-safe per 不变量 4）
- [ ] **T2.4** Verify pass: 5 cases PASS
- [ ] **T2.5** Implement: `include/agenticdsl/pdk/cross_cutting/cross_cutting_config.h`:
  ```cpp
  class CrossCuttingConfig {
  public:
      static CrossCuttingConfig load(const std::string& yaml_path);
      nlohmann::json to_json() const;
      bool is_valid() const;
  private:
      nlohmann::json config_;
  };
  ```
- [ ] **T2.6** Implement: `src/common/governance/cross_cutting/cross_cutting_config.cpp`:
  - YAML → JSON 转换（V1 简化: 直接逐行解析，识别 `### AgenticDSL /<section>` + ```yaml 块）
  - 字段校验: patterns 数组必填、type ∈ {4 pattern names}、config 子对象必填
  - 错误处理: schema 非法 → throw std::invalid_argument
- [ ] **T2.7** Create: `examples/cross_cutting/dsl/high_security_mode.cc.md`:
  ```yaml
  ### AgenticDSL /__meta__
  version: "1.0"
  mode: high_security
  
  ### AgenticDSL /cross_cutting
  patterns:
    - type: decorator-v1
      config:
        decorators: ["CostTracking", "Compliance", "PII-Scrub"]
    - type: hook-v1
      config:
        hooks:
          - target: tool
            glob: "L3_*"
            type: pre
            priority: 1000
            policy: FailClosed
            handler: human-approval-v1
    - type: composition-v1
      config:
        agents:
          - name: privacy-policy-v1
            scope: "react-loop/*"
    - type: bus-v1
      config:
        subscriptions: ["mutation.committed"]
        handler: external-siem-adapter-v1
  ```
- [ ] **T2.8** Create: `examples/cross_cutting/dsl/cost_optimization_mode.cc.md`:
  - 简化示例（仅 decorator + bus，2 patterns）
- [ ] **T2.9** Create: `examples/cross_cutting/dsl/development_mode.cc.md`:
  - 开发模式（metrics + debug logging）
- [ ] **T2.10** Commit: `feat(pdk): CrossCuttingOrchestrator + DSL loader + 3 examples (T2)`

## Phase 3: CMake 集成 + E2E 测试

- [ ] **T3.1** Create: `src/common/governance/cross_cutting/CMakeLists.txt`:
  ```cmake
  add_library(agenticdsl_cross_cutting
      cross_cutting_orchestrator.cpp
      decorator_pattern.cpp
      hook_pattern.cpp
      composition_pattern.cpp
      bus_pattern.cpp
      cross_cutting_config.cpp
  )
  target_link_libraries(agenticdsl_cross_cutting PUBLIC
      agenticdsl_core agenticdsl_contract agenticdsl_policy agenticdsl_llm
  )
  target_include_directories(agenticdsl_cross_cutting PUBLIC include)
  ```
- [ ] **T3.2** Modify: `CMakeLists.txt` 添加 `add_subdirectory(src/common/governance/cross_cutting)`
- [ ] **T3.3** Verify: 根 `CMakeLists.txt` 与既有规则零冲突（仅追加）
- [ ] **T3.4** Write failing test: `tests/test_cross_cutting_dsl.cpp` ≥ 2 cases
  - `dsl_load_high_security_mode_yaml`
  - `dsl_load_invalid_schema_throws`
- [ ] **T3.5** Verify fail: 2 cases FAIL
- [ ] **T3.6** Implement: `tests/test_cross_cutting_dsl.cpp` 测试 + 集成验证
- [ ] **T3.7** Write failing test: `tests/test_cross_cutting_e2e.cpp` ≥ 2 cases
  - `e2e_high_security_mode_full_pipeline` (4 patterns + DSL load + dispatch)
  - `e2e_cost_optimization_mode_minimal` (2 patterns)
- [ ] **T3.8** Verify fail: 2 cases FAIL
- [ ] **T3.9** Implement: E2E 测试 mock:
  - 真实 IAgentRegistry / IAgentHookRegistry / IToolHookRegistry / IInteractionBus
  - Mock ILLMProvider (返回固定 prompt 候选)
  - Mock IApprovalHandler (记录请求 + 返回 true)
- [ ] **T3.10** Verify pass: 4 cases PASS (DSL + 2 E2E)
- [ ] **T3.11** Commit: `feat(pdk): CMake integration + DSL/E2E tests (T3)`

## Phase 4: 文档同步 + ship

- [ ] **T4.1** Modify: `docs/architecture/cross-cutting-hooks-architecture-2026-08.md` v1.2 → v1.3:
  - 头部 `最后验证` 追加 "v1.3 — V1 implementation shipped (commit xxx)"
  - §十一 `关联 ADR` 追加 ADR-0085 ✅ Approved 注记
- [ ] **T4.2** Modify: `docs/adr/adr-0085-cross-cutting-pattern-pdk.md` 头部:
  - 🔍 Proposed → ✅ Approved (ship 2026-08-XX — V1 implementation shipped)
  - 追加 ship 证据段: commits + tests + ctest baseline
- [ ] **T4.3** Modify: `docs/README.md` §adr/ 表: ADR-0085 行状态翻 ✅
- [ ] **T4.4** Modify: `docs/architecture/capability-application-map-2026-08.md`:
  - §一 +1 (新能力 #29 Cross-Cutting Pattern PDK V1)
  - §八 新增 T26 任务 (后续 T17/T15/T19/T20/T21 横切化)
  - §七 changelog v2.3 条目
- [ ] **T4.5** Modify: `docs/active-status.md` §一 T26 跟踪段
- [ ] **T4.6** Verify: `python3 tools/adr_lint.py` PASS (≥83 ADR)
- [ ] **T4.7** Verify: `python3 tools/docs_drift_audit.py` 0 NEW CRITICAL
- [ ] **T4.8** Verify: `openspec validate --changes --strict` PASS
- [ ] **T4.9** Verify: `ctest --output-on-failure` 全量 0 回归（动态基线）
- [ ] **T4.10** Verify 关键不变量（**Oracle B3 强制**）:
  - `git diff HEAD -- include/agenticdsl/contract/` 0 行
  - `git diff HEAD -- src/core/engine.h` 0 行
- [ ] **T4.11** Commit: `docs(architecture+adr): Cross-Cutting Pattern PDK V1 ship — ADR-0085 ✅ Approved`
- [ ] **T4.12** `openspec archive pdk-cross-cutting-patterns`

## 总估时

- Phase 0: 0.5 sprint（抽象层）
- Phase 1: 0.7 sprint（4 Pattern 实现）
- Phase 2: 0.4 sprint（Orchestrator + DSL 加载器 + 3 examples）
- Phase 3: 0.3 sprint（CMake 集成 + E2E 测试）
- Phase 4: 0.3 sprint（文档同步 + ship）
- **总估时: ~2.2 sprint**（与 ADR-0085 设计估时一致）

## 明确 out of scope (V2 deferred)

- Meta-Agent 自管理（CrossCuttingMetaAgent + 策略 4）
- Hot-Reload 反向取消（disable pattern API）
- 横切功能 Marketplace（社区贡献新 Pattern）
- 跨 Pattern 依赖编排
- 真实 LLM tokenizer（替代 chars/4 估算，T21 已 V1 简化）
- AppendOnlyEventLog 集成（原文持久化到 log，事件流仅含 hash + log_id）

## 关键不变量（强制遵守）

### Oracle B3 + ADR-0085 不变量 1

```bash
# 必须 0 行的 git diff
git diff HEAD -- include/agenticdsl/contract/i_llm_provider_decorator.h
git diff HEAD -- include/agenticdsl/contract/iinteraction_bus.h
git diff HEAD -- include/agenticdsl/contract/itool_hook_registry.h
git diff HEAD -- include/agenticdsl/contract/iagent_hook_registry.h
git diff HEAD -- include/agenticdsl/contract/iagent_registry.h
git diff HEAD -- include/agenticdsl/contract/iagent_composition.h
git diff HEAD -- include/agenticdsl/contract/event_builder.h
git diff HEAD -- include/agenticdsl/contract/ievaluator.h
git diff HEAD -- include/agenticdsl/contract/imutation_governance.h
git diff HEAD -- src/core/engine.h
git diff HEAD -- docs/adr/adr-0068-event-emission-contract.md
```

### 其他禁止事项

- ❌ 不修改 EventBuilder / IInteractionBus / ToolResult 等公开契约
- ❌ 不修改 IAgentRegistry / IAgentHookRegistry / IToolHookRegistry / ILLMProvider 接口
- ❌ 不修改 engine.h
- ❌ 不修改 ADR-0068 附录 A 主题名（仅 payload 字段更新如 T21）
- ❌ 在测试失败时强行 commit
- ❌ 硬编码 ctest 数字
- ❌ 引入新依赖（除 nlohmann::json 已 vendor）

## 测试要求汇总

- **≥ 5 cases** test_cross_cutting_orchestrator
- **≥ 2 cases** test_decorator_pattern
- **≥ 3 cases** test_hook_pattern
- **≥ 2 cases** test_composition_pattern
- **≥ 2 cases** test_bus_pattern
- **≥ 2 cases** test_cross_cutting_dsl
- **≥ 2 cases** test_cross_cutting_e2e
- **总计: ≥ 18 cases** （远超 ≥10 门槛）