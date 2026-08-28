# ADR-0085: Cross-Cutting Pattern PDK (横切功能 PDK 模式)

**日期**: 2026-08-28
**父主题**: HydraForge 横切架构工作文档 `docs/architecture/cross-cutting-hooks-architecture-2026-08.md` v1.2
**状态**: ✅ **Approved** (2026-08-28 — Oracle 3 轮复审全部通过, GitHub issue #15 Self-Review 决议)

> **V1 范围**: 4 个独立 PDK Pattern class + `CrossCuttingOrchestrator` 编排器 + `ICrossCuttingPattern` 抽象接口 + 横切功能 DSL 格式（`*.cc.md`）。**V1 不实施**：横切功能 Agent 自管理（可选高级特性，V2 deferred）。

> **Ship 证据 (2026-08-28)**: 
> - GitHub issue #15 Self-Review 决议通过 (`https://github.com/chisuhua/HydraForge/issues/15`)
> - Oracle 3 轮复审全部通过 (Round 1: 1 Blocking + 4 High + 9 Medium; Round 2: Blocking 修正 commit `040e9bd`; Round 3: H+M 修正 commit `2e1f4e4`)
> - adr_lint 83 ADR 文件 0 错误; openspec validate 4/4 PASS; docs_drift_audit 0 NEW CRITICAL
> - 零契约变更验证: 6 层抽象 (i_*.h) + engine.h + iagent_composition.h 全部 0 diff
> - 命名空间卫生: agenticdsl:: 限定 92 处 (≥30 门槛)
> - **实施载体**: OpenSpec change `pdk-cross-cutting-patterns` 后续创建 (~2.2 sprint 估时)
> - **T21 payload 泄露面**已 ship 修复 (commit `abe0b3f`) 作为本 ADR 设计模式的应用示范
> - **Oracle session**: `ses_fb9839be4ffeEdO0T7O6SfFUSi` (横切架构 adversarial review)
>
> **不引入**: 新增横切功能类别、新增 hook 类型、新增事件主题（所有现有 6 层扩展点 L0-L5 + 27+ 主题保持不变）。
>
> **核心设计**: 横切功能管理采用与 PDK Loop Agent **完全相同**的设计模式（独立 class + dispatcher 编排）。

---

## 背景

### 问题

HydraForge 当前已有完整的 6 层抽象扩展点矩阵（L0 ILLMProviderDecorator → L5 IInteractionBus），但**没有统一的横切功能管理机制**：
1. **业务代码与横切逻辑混合**：每个应用需手动注册 decorator / hook / bus subscriber，重复模板代码
2. **横切功能不可发现**：registry.list_registered() 找不到所有可用的横切能力（仅 business agents）
3. **配置驱动缺失**：缺乏统一的 YAML/JSON 配置格式驱动横切能力装配
4. **多范式无统一抽象**：4 种扩展方式（Decorator / Hook / Composition / Bus）独立 API，缺少共同接口
5. **Agent first-class 不彻底**：横切功能可建模为 Agent 但缺乏统一编排入口

### 解决方案

引入 **PDK Cross-Cutting Pattern** 子模式（类比 PDK Loop Agent）：

```
include/agenticdsl/pdk/cross_cutting/
├── icross_cutting_pattern.h         (统一抽象, 类比 loop_result.h)
├── decorator_pattern.h               (Decorator 范式实现, 类比 react_loop.h)
├── hook_pattern.h                    (Hook 范式实现, 类比 plan_execute_loop.h)
├── composition_pattern.h             (Composition 范式实现, 类比 fork_join_loop.h)
├── bus_pattern.h                     (Event Bus 范式实现)
└── cross_cutting_orchestrator.h      (无状态 dispatcher, 类比 LoopDispatcher)

examples/cross_cutting/dsl/
├── high_security_mode.cc.md          (高安全模式配置, 类比 *.agent.md)
├── cost_optimization_mode.cc.md      (成本优化模式配置)
└── development_mode.cc.md            (开发模式配置)
```

**4 个独立 Pattern class** + **1 个 Orchestrator dispatcher**（无状态，纯分发） + **横切功能 DSL**（类比 Agent DSL）。

### 设计哲学（继承自 PDK Loop Agent）

- **正交分层**：4 Pattern 互相正交，按需组合
- **统一抽象**：`ICrossCuttingPattern` interface（类比 `LoopResult`）
- **无状态 dispatcher**：`CrossCuttingOrchestrator` 不存储业务状态（类比 `LoopDispatcher` 模板）
- **Agent first-class**：Composition Pattern 通过 `IAgentRegistry` 注入 Agent（ADR-0082 ✅）
- **fail-safe 默认**：复用 `HookErrorPolicy` (FailClosed/FailOpen, ADR-0069 🟡 Partial)
- **零业务代码侵入**：横切关注点通过 Pattern apply 注入，business code 无需感知
- **DSL 实例化**：`*.cc.md` 配置文件（类比 `*.agent.md`），YAML 格式

---

## 决策

### 决策 1 — 4 范式独立 PDK Pattern class

**V1 边界**: 仅实现 4 个核心 Pattern，**新增第 5 种范式只需新增 1 个 Pattern class + 注册到 Orchestrator**。

```cpp
// include/agenticdsl/pdk/cross_cutting/icross_cutting_pattern.h
namespace hydraforge::pdk {

class ICrossCuttingPattern {
public:
    virtual ~ICrossCuttingPattern() = default;
    virtual const std::string& name() const = 0;  // "decorator-v1" / "hook-v1" / etc.
    virtual void apply(const nlohmann::json& pattern_config,
                      CrossCuttingContext& ctx) = 0;
};

struct CrossCuttingContext {
    agenticdsl::IAgentRegistry* agent_registry;
    agenticdsl::IAgentHookRegistry* agent_hook_registry;
    agenticdsl::IToolHookRegistry* tool_hook_registry;
    agenticdsl::IAgentComposition* agent_composition;   // L3 编排 (Oracle H1: 真实 API)
    agenticdsl::IApprovalHandler* approval_handler;     // L4 通道 (Oracle M2)
    agenticdsl::IInteractionBus* bus;
    // Oracle B1: L0 通道使用 set_llm_provider 回调替代虚构的
    // ILLMProvider** 槽位 (engine.h 仅暴露 set_llm_provider API)。
    // 用户构造 Orchestrator 时绑定到 DSLEngine::set_llm_provider。
    std::function<void(std::unique_ptr<agenticdsl::ILLMProvider>)> set_llm_provider;
};

namespace cross_cutting_pattern {
    constexpr const char* Decorator = "decorator-v1";
    constexpr const char* Hook = "hook-v1";
    constexpr const char* Composition = "composition-v1";
    constexpr const char* Bus = "bus-v1";
}

}  // namespace hydraforge::pdk
```

**真实 API 引用（Oracle H1）** — 本 ADR 依赖的 L3 契约，实施时以实际头文件为准：
- `include/agenticdsl/contract/iagent_registry.h`（ADR-0082 ✅ Approved）— `AgentFactory` = `std::function<std::unique_ptr<IAgent>(const AgentConfig&)>`；`AgentConfig` = `struct { std::string instance_id; }`；API：`register_agent(string_id, factory)`（重复注册返回 false 不抛）/ `create(string_id, config)`（未注册 → nullptr）/ `unregister` / `is_registered` / `list_registered` / `size`。**无** `resolve()` / `list()` / `std::optional<IAgent>`（虚构方法已删除）
- `include/agenticdsl/contract/iagent_composition.h`（ADR-0060 ✅ Approved）— `call(id, args, timeout)` / `delegate(id, task, priority)` / `call_async(id, args, callback, timeout)` / `stream(id, args)`（Phase 2 占位，抛 `logic_error`）
- `include/agenticdsl/policy/iapproval_handler.h`（ADR-0031 ✅）— `process_request(const ToolMetadata&, const ToolCallContext&, const ToolPreview&) -> bool`（Oracle M2）
- `include/agenticdsl/contract/itool_hook_registry.h`（ADR-0069 🟡 Partial）— `HookErrorPolicy` / `PreHookResult` / `register_pre_hook(tool_glob, PreHook, priority, policy)`
- `include/agenticdsl/contract/iagent_hook_registry.h`（ADR-0081 ✅ Approved）— `AgentPreHook` = `std::function<AgentPreHookResult(const IAgent&, const std::string&)>`

**理由**:
- 4 范式（Decorator / Hook / Composition / Bus）覆盖现有 6 层抽象扩展点的所有应用场景
- 独立 class 实现避免 god class（类比 React/PlanExecute/ForkJoin 独立 class）
- 统一抽象接口便于测试 + 扩展（V2 新增第 5 种范式无需修改 Orchestrator）

### 决策 2 — `CrossCuttingOrchestrator` 无状态 dispatcher

**关键设计点**：
- **无状态**：Orchestrator 只持有基础设施引用 + pattern 集合，**不存储业务状态**
- **运行时分发**：基于 JSON 配置动态选择 pattern（vs `LoopDispatcher` 编译期模板特化）
- **扩展点**：`register_pattern()` 方法允许注册自定义 pattern（V2 扩展）
- **不修改既有契约**：Orchestrator 仅依赖既有 6 层抽象（L0-L5），零修改

```cpp
// include/agenticdsl/pdk/cross_cutting/cross_cutting_orchestrator.h
class CrossCuttingOrchestrator {
public:
    // Oracle M7: patterns 可选注入, 默认注册 4 个内置 pattern (向后兼容)。
    CrossCuttingOrchestrator(agenticdsl::IAgentRegistry& agent_reg,
                              agenticdsl::IAgentHookRegistry& agent_hook_reg,
                              agenticdsl::IToolHookRegistry& tool_hook_reg,
                              agenticdsl::IInteractionBus& bus,
                              agenticdsl::IApprovalHandler* approval_handler = nullptr,   // L4 通道 (Oracle M2)
                              std::function<void(std::unique_ptr<agenticdsl::ILLMProvider>)>
                                  set_llm_provider = nullptr,                              // L0 通道 (Oracle B1)
                              std::vector<std::unique_ptr<ICrossCuttingPattern>> patterns = {});  // Oracle M7
    
    void dispatch(const nlohmann::json& cross_cutting_config);  // 主入口
    
    void register_pattern(std::unique_ptr<ICrossCuttingPattern> pattern);
    
private:
    CrossCuttingContext ctx_;
    std::unordered_map<std::string, std::unique_ptr<ICrossCuttingPattern>> patterns_;
};
```

> **Oracle M7 说明**: `patterns` 参数为空时默认注册 4 个内置 pattern（Decorator/Hook/Composition/Bus），
> 保持向后兼容；调用方可注入自定义 pattern 集合覆盖默认。**硬编码内置 pattern 是 V1 简化**，
> V2 可改为静态工厂（`CrossCuttingPatternFactory::create_defaults()`）或完全配置驱动。

**关键差异 vs LoopDispatcher**:
| 维度 | LoopDispatcher | CrossCuttingOrchestrator |
|------|-----------------|---------------------------|
| 分发时机 | 编译期（模板特化）| 运行期（JSON 配置）|
| 状态 | 无（纯模板）| 无（仅持有引用）|
| 注册时机 | 编译时 `template<>` | 运行时 `register_pattern()` |
| 错误处理 | 编译错误 | 未知 pattern **FailOpen**（记 warning + 跳过, Oracle M1）；throw 仅限 schema 非法 |

**理由**:
- 配置驱动 vs 编译期类型（横切功能运行时按需启用）
- 与 LoopDispatcher 设计哲学保持一致（独立 dispatcher + 独立 class）
- V2 扩展点 `register_pattern()` 允许第三方自定义

### 决策 3 — 4 Pattern 实现职责（每 Pattern 单一关注点）

**Pattern 1: DecoratorPattern**（L0 ILLMProviderDecorator 注入）
- **职责**: 修改 `ILLMProvider` 链（添加 cost tracking / rate limit / PII scrub 等）
- **依赖**: `set_llm_provider` 回调（Oracle B1：绑定到 `DSLEngine::set_llm_provider`，非虚构 `ILLMProvider**` 槽位）
- **V1 不实现**: 工厂模式 + 装饰器链自动构造（V2 deferred；链构造委托 `DecoratorFactory::create_chain`，类比 `wrap_chain` 静态工厂）

**Pattern 2: HookPattern**（L1 ToolHook + L2 AgentHook + L4 Approval 注册）
- **职责**: 注册 pre/post hooks 到 ToolHookRegistry / AgentHookRegistry
- **依赖**: `IToolHookRegistry*` + `IAgentHookRegistry*`
- **配置粒度**: target (tool/agent/**approval**), glob, priority, policy
- **Oracle M2**: 新增 `target: approval` 类型 —— 通过 `ctx.approval_handler->process_request(...)` 走 L4 审批通道（依赖 ADR-0031 `IApprovalHandler` ✅）

**Pattern 3: CompositionPattern**（L3 AgentRegistry 注入）
- **职责**: 通过 `IAgentRegistry::create()` 实例化横切功能 Agent，注入到目标 registry
- **依赖**: `IAgentRegistry*` + `IAgentHookRegistry*`（自动注入为 hook）
- **真实 API（Oracle H1/M3）**: 仅 `register_agent` + `create`（无 `resolve/list`）；`AgentConfig` 仅含 `instance_id`；未注册 string_id → `create()` 返回 nullptr（FailOpen 跳过）
- **V1 不实现**: 跨 Agent 通信编排（V2 deferred）

**Pattern 4: BusPattern**（L5 IInteractionBus 订阅）
- **职责**: 订阅 IInteractionBus 主题，将事件转发到 handler
- **依赖**: `IInteractionBus*`
- **V1 不实现**: 复杂事件过滤（glob regex 等 V2 deferred）

**理由**:
- 每 Pattern 单一关注点（SRP），独立测试
- 与既有 6 层抽象扩展点一一对应（L0/L1/L2/L3/L5）
- L4 (Approval) 不引入 Pattern（V1 范围内横切能力不需独立 Pattern）

### 决策 4 — 横切功能 DSL 格式（YAML）

**位置**: `examples/cross_cutting/dsl/*.cc.md`

```yaml
# examples/cross_cutting/dsl/high_security_mode.cc.md
### AgenticDSL `/__meta__`
version: "1.0"
mode: high_security
description: "Enable strict privacy + audit + approval"

### AgenticDSL `/cross_cutting`
patterns:
  - type: decorator-v1
    config:
      decorators: ["CostTracking", "Compliance", "PII-Scrub"]  # Oracle B2: 链深 ≤4 含 inner (Retry 为自定义 decorator)
  - type: hook-v1
    config:
      hooks:
        - target: approval       # Oracle M2: L4 审批通道 (approval_handler->process_request)
          glob: "L3_*"
          type: pre
          priority: 1000
          policy: FailClosed
        - target: agent
          glob: "react-loop/*"
          type: pre
          priority: 500
          policy: FailClosed
          handler: privacy-policy-v1
  - type: composition-v1
    config:
      agents:
        - name: privacy-policy-v1
          config:
            instance_id: "privacy-main"   # Oracle M3: 完整 AgentConfig (instance_id 可选, 空则 create() 自动生成)
          scope: "react-loop/*"
  - type: bus-v1
    config:
      subscriptions: ["mutation.committed"]
      handler: external-siem-adapter-v1
```

**Oracle M6 — DSL schema 校验**: 加载 `/cross_cutting` 段时使用 `cross_cutting_schema.json`
（V1 实施阶段定义），复用 ADR-0073 nlohmann JSON Schema 校验器
（`include/agenticdsl/tools/tool_schema_validator.h`，JSON Schema 2020-12 最小子集：
type/properties/required/items/enum）做**结构校验**——`patterns[].type` 必须是
`decorator-v1` / `hook-v1` / `composition-v1` / `bus-v1` 之一，`config` 字段按各 pattern
声明。否则 YAML `type` 字段拼写错误（Oracle H3）运行时才以
FailOpen warning 暴露，难以在加载期发现。

**理由**:
- YAML 格式与 Agent DSL（`*.agent.md`）一致（ADR-0043 命名约定）
- 类比 `examples/pdk_chat_demo/dsl/*.agent.md` 实例化模式
- V1 仅 4 字段（patterns/type/config/global_meta），后续可扩展
- schema 校验复用 ADR-0073 既有校验器（零新依赖）

### 决策 5 — V1 不强制 Meta-Agent 自管理

**重要修正（vs v1.0 工作文档）**：
- v1.0 提出 `CrossCuttingMetaAgent` 作为统一入口
- v1.2 修正为**可选高级特性**（V2 deferred）

**理由**:
- **PDK 一致性**：Loop Dispatcher 没有"MetaLoop"集中决策，Orchestrator 也不应有"MetaOrchestrator"
- **SRP 原则**：Orchestrator 是无状态 dispatcher，MetaAgent 是高级编排（职责分离）
- **使用场景窄**：仅自适应系统场景需要，多数应用直接用 Config 即可
- **测试性**：Orchestrator 独立可测；MetaAgent 可后置

### 决策 6 — V1 边界（强制）

**V1 范围内**:
- ✅ 4 PDK Pattern class（Decorator / Hook / Composition / Bus）
- ✅ `ICrossCuttingPattern` 统一抽象
- ✅ `CrossCuttingOrchestrator` dispatcher
- ✅ 横切功能 DSL 格式 + 加载器
- ✅ ≥3 DSL examples（high_security / cost_optimization / development）
- ✅ ≥10 测试 cases（覆盖 4 Pattern + Orchestrator + DSL 加载）

**V1 不做（明确边界）**:
- ❌ 横切功能 Agent 实现（privacy-policy / metrics-collector / siem-adapter 等）— V2
- ❌ Meta-Agent 自管理 — V2
- ❌ Hot-Reload 反向取消（disable pattern）— V2
- ❌ 跨 Pattern 依赖编排 — V2
- ❌ 横切功能 marketplace — V2
- ❌ 修改既有 6 层抽象扩展点（L0-L5）— V1 零修改
- ❌ 修改既有 27+ 事件主题（ADR-0068）— V1 零修改
- ❌ 修改既有 HookErrorPolicy — V1 复用既有

---

## 关键不变量

1. **既有契约零修改**: `ICrossCuttingPattern` / Orchestrator 仅**依赖**既有 6 层抽象（L0-L5）+ IAgentRegistry + IAgentHookRegistry + IToolHookRegistry + IInteractionBus，不修改其接口
2. **Orchestrator 无状态**: 不存储业务配置 / state，所有 state 来自 DSL 加载器
3. **Pattern 单一关注点**: 每个 Pattern 仅处理一种范式，互不耦合
4. **fail-safe 默认**: HookPattern 复用既有 `HookErrorPolicy` (ADR-0069)；Orchestrator **不阻断主流程** —— 未知 pattern / pattern apply 异常均按 FailOpen（记 warning + 跳过），throw 仅限 schema 非法（Oracle M1 强化）
5. **PDK Loop 一致性**: 4 Pattern 独立 class + Orchestrator 编排 = 与 3 Loop + LoopDispatcher 完全对等
6. **DSL 实例化**: `*.cc.md` 配置文件类比 `*.agent.md`，YAML 格式
7. **命名空间卫生**: 所有代码样本使用 `agenticdsl::` 限定（Oracle H2）
8. **DSL 字段统一**: 配置字段统一 `type:`（无旧字段名残留，Oracle H3）

---

## 实施路径（OpenSpec change `pdk-cross-cutting-patterns`）

### Phase 0: 抽象 + Orchestrator（估时 0.5 sprint）
- 新增 `include/agenticdsl/pdk/cross_cutting/icross_cutting_pattern.h`
- 新增 `include/agenticdsl/pdk/cross_cutting/cross_cutting_orchestrator.h`
- 实现 Orchestrator class（运行时分发 + register_pattern 扩展点）
- 新增 ≥4 测试 cases（dispatch / register_pattern / 错误处理）

### Phase 1: 4 Pattern 实现（估时 1 sprint）
- `decorator_pattern.h/.cpp` + ≥2 tests
- `hook_pattern.h/.cpp` + ≥3 tests
- `composition_pattern.h/.cpp` + ≥2 tests
- `bus_pattern.h/.cpp` + ≥2 tests

### Phase 2: DSL 加载器 + examples（估时 0.5 sprint）
- `cross_cutting_config.h/.cpp`（YAML 解析 + **`cross_cutting_schema.json` 结构校验, 复用 ADR-0073 校验器, Oracle M6**）
- `examples/cross_cutting/dsl/high_security_mode.cc.md`
- `examples/cross_cutting/dsl/cost_optimization_mode.cc.md`
- `examples/cross_cutting/dsl/development_mode.cc.md`
- ≥2 集成 tests（DSL → Orchestrator 全链路）

### Phase 3: 文档 + ship（估时 0.2 sprint）
- 更新 `docs/architecture/cross-cutting-hooks-architecture-2026-08.md` v1.2
- 更新 cap-map §一 +1（新能力 #29）
- 更新 README §adr/ 表格
- adr_lint + docs_drift_audit 全 PASS
- ctest 全量 0 回归（动态基线）

**总估时: ~2.2 sprint**（与 ADR-0068 ~2 周、ADR-0069 ~2 周 同等级）

---

## 关联

### 父 ADR / 父文档
- `docs/architecture/cross-cutting-hooks-architecture-2026-08.md` v1.2（本文档设计依据；Oracle 评审 H1-H4 + M1-M9 已应用）

### 依赖 ADR（已 ship 或 Approved）
- **ADR-0021** PDK Design ✅ Approved（PDK Plugin 范式）
- **ADR-0068** Event Emission Contract ✅ Approved（27+ 主题）
- **ADR-0069** ToolCoordinator Hook 🟡 Partial（HookErrorPolicy 已 ship，待 HookErrorPolicy amendment）— Oracle M8 状态修正
- **ADR-0073** Tool JSON Schema Contract 🟡 Partial（DSL schema 校验器复用，Oracle M6）
- **ADR-0081** Pre-Step Hook Contract ✅ Approved（IAgentHookRegistry, Agent-scoped）
- **ADR-0082** Agent First-Class Registry ✅ Approved（IAgentRegistry + IAgent）
- **ADR-0083** IEvaluator ✅ Approved + ship（Composition Pattern 复用）
- **ADR-0084** Mutation Governance ✅ Approved + ship（HookPattern 集成示例）
- **ADR-0031** Execution Policy ✅ Approved（IApprovalHandler L4 通道，Oracle M2）

### 不依赖（V1 零耦合）
- T17 SkillCompiler ✅ ship（V1 不集成）
- T15 Trajectory IR ✅ ship（V1 不集成）
- T19 GEPA Phase 2 commit ✅ ship（V1 不集成，仅 Composition Pattern 复用其 Agent 类型）

### OpenSpec change
- `pdk-cross-cutting-patterns`（待创建，V1 实施载体）

---

## 测试要求

### 单元测试（每 Pattern 独立）
- `test_decorator_pattern.cpp` — ≥2 cases（cost tracking / rate limit）
- `test_hook_pattern.cpp` — ≥3 cases（tool pre / agent pre / `target: approval` L4 通道）
- `test_composition_pattern.cpp` — ≥2 cases（Agent 创建 + hook 注入）
- `test_bus_pattern.cpp` — ≥2 cases（单主题订阅 / 多主题订阅）

### Orchestrator 集成测试
- `test_cross_cutting_orchestrator.cpp` — ≥4 cases（dispatch / register_pattern / 错误处理 / 多 Pattern 顺序）
- `test_cross_cutting_dsl.cpp` — ≥2 cases（DSL 加载 + 全链路 dispatch）

### E2E 测试
- `test_cross_cutting_e2e.cpp` — ≥2 cases（high_security_mode + cost_optimization_mode 全链路验证）

### 关键不变量验证（git diff）
- `git diff HEAD -- include/agenticdsl/contract/iinteraction_bus.h` — 0 行
- `git diff HEAD -- include/agenticdsl/contract/itool_hook_registry.h` — 0 行
- `git diff HEAD -- include/agenticdsl/contract/iagent_hook_registry.h` — 0 行
- `git diff HEAD -- include/agenticdsl/contract/iagent_registry.h` — 0 行
- `git diff HEAD -- include/agenticdsl/contract/i_llm_provider_decorator.h` — 0 行
- `git diff HEAD -- include/agenticdsl/contract/iagent_composition.h` — 0 行
- `git diff HEAD -- src/core/engine.h` — 0 行 (Oracle B3: V1 不修改 DSLEngine，通过 set_llm_provider 复用既有 API)
- `git diff HEAD -- docs/adr/adr-0068-event-emission-contract.md` — 0 行

---

## 评审检查清单（自评）

- [x] **命名一致性**: `cross_cutting_pattern::Decorator` 等常量与 ADR-0043 命名约定对齐
- [x] **接口正交性**: 4 Pattern 仅依赖既有 6 层抽象，互不耦合
- [x] **fail-safe 默认**: HookPattern 复用 ADR-0069 HookErrorPolicy；Orchestrator FailOpen（Oracle M1）
- [x] **可测试性**: 每 Pattern 独立单元测试，Orchestrator 集成测试
- [x] **可演化性**: register_pattern() 扩展点允许 V2 新增第 5 种范式
- [x] **Agent first-class**: Composition Pattern 通过 IAgentRegistry 注入（真实 API，Oracle H1）
- [x] **DSL 实例化**: `*.cc.md` 配置类比 `*.agent.md` Agent DSL
- [x] **V1 边界清晰**: 4 Pattern + Orchestrator + DSL；V2 包含 Meta-Agent / marketplace
- [x] **既有契约零修改**: 6 层抽象 + IAgentRegistry + IAgentHookRegistry + IToolHookRegistry + IInteractionBus 均不修改
- [x] **PDK Loop 一致性**: 独立 class + dispatcher 编排与 PDK Loop 完全对等
- [x] **命名空间卫生**: 代码样本统一 `agenticdsl::` 限定（Oracle H2）
- [x] **DSL 字段统一**: `type:` 字段统一（0 个旧字段名残留，Oracle H3）
- [x] **schema 校验**: DSL 加载复用 ADR-0073 校验器（Oracle M6）
- [x] **L4 审批通道**: `target: approval` 通过 `IApprovalHandler`（Oracle M2）
- [x] **真实 AgentConfig**: CompositionPattern 接受完整 `config`（仅含 instance_id，Oracle M3）

---

## 待 Oracle / 架构组评审项

1. **D1-D6 决策是否完整覆盖 v1.2 文档所有设计点**?
2. **CrossCuttingOrchestrator 运行时分发 vs LoopDispatcher 编译期分发是否合理**?
3. **4 Pattern 划分（L4 Approval 作为 HookPattern 的 `target: approval` 分支，非独立 Pattern）是否完整覆盖所有横切场景**?
4. **V1 不实施 MetaAgent 是否合理**?（vs v1.0 强制实施）
5. **`*.cc.md` DSL 格式 schema 校验粒度是否足够**?（复用 ADR-0073 校验器）
6. **V1 实施估时 2.2 sprint 是否合理**?
7. **ADR-0085 状态是否可立即转 Approved**?（Oracle 评审 H1-H4 + M1-M9 已完成，取决于 OpenSpec change 实施决心）

---

## 状态变更

- 当前: 🔍 Proposed（2026-08-28 创建；Oracle 评审 H1-H4 + M1-M9 修正完成）
- 评审准备: 设计依据完整 + 关键不变量清晰 + 测试要求明确 + 真实 API 引用校正
- 待评审: ADR 评审会议 + 与 ADR-0021 (PDK Design) 关联验证

> **注**: 本 ADR 文件于 2026-08-28 创建，承接 `cross-cutting-hooks-architecture-2026-08.md` v1.2 的 §四 设计建议（Oracle 评审修正已同步）。实施载体为 OpenSpec change `pdk-cross-cutting-patterns`（待创建）。
