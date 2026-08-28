# Design: pdk-cross-cutting-patterns

## Context

ADR-0085 Cross-Cutting Pattern PDK (🔍 Proposed → ✅ Approved after ship) 定义了 4 PDK Pattern + CrossCuttingOrchestrator + ICrossCuttingPattern 抽象 + 横切功能 DSL。本 change 是 ADR-0085 的 V1 实施载体。

**所有前置已 ship**:
- 6 层抽象扩展点全部 ship（L0-L5）
- ADR-0068 v1.6 附录 A（27+ 主题）
- T21 payload redact 已 ship（hash-only PII defense 范式参考）
- HookErrorPolicy (FailClosed/FailOpen)
- Engine.h:130 set_llm_provider API（DecoratorPattern 注入通道）
- 既有 190+ tests 全部 PASS

## Scope Boundaries

### 范围 IN
- 5 个新 .h 文件（抽象 + 4 Pattern + Orchestrator）+ 5 个新 .cpp
- 1 个新 CMakeLists.txt + 根 CMakeLists.txt 追加 add_subdirectory
- DSL 加载器（YAML 解析 + schema 校验）
- 3 个 DSL examples（high_security / cost_optimization / development）
- ≥ 18 个新测试 cases（7 个测试文件）
- 文档同步（architecture v1.3 + ADR-0085 ✅ Approved + cap-map + active-status）

### 范围 OUT
- 既有契约 0 修改（Oracle B3 关键不变量）
- Meta-Agent 自管理（V2）
- Hot-Reload 反向取消（V2）
- 横切功能 Marketplace（V2）
- AppendOnlyEventLog 集成（V2）
- 真实 LLM tokenizer 替换 chars/4（V2）
- 跨 Pattern 依赖编排（V2）

## Design Decisions

### D1 — 与 PDK Loop Agent 完全对等

类比 `LoopDispatcher<LoopType>` 编译期分发，`CrossCuttingOrchestrator` 运行期分发。两文档严格 1:1 对应（cross-cutting-architecture v1.2 §5.0）。

### D2 — 真实 6 层抽象 API（Oracle H1 修正后）

**禁止使用虚构 API**（`resolve()`/`list()`/`std::optional<IAgent>` 已删除）：
- `IAgentRegistry::register_agent(string_id, factory)` + `create(string_id, config)`
- `IAgentComposition::call/delegate/call_async/stream`
- `ILLMProviderDecorator::wrap_chain()` 链深 ≤4
- `EventBuilder::meta(trace_id)` + `.args()` + `.ok(bool)` + `.error_code()`

### D3 — set_llm_provider 回调替代 slot（Oracle B1 修正后）

**禁止 `ILLMProvider** llm_provider_slot`**（engine.h:130 仅有 `set_llm_provider` API）：
- `CrossCuttingContext.set_llm_provider` = `std::function<void(unique_ptr<ILLMProvider>)>`
- 用户构造 Orchestrator 时绑定到 DSLEngine::set_llm_provider
- Orchestrator 零触碰 engine.h

### D4 — HookErrorPolicy 复用（ADR-0069 ✅ Approved）

- DSL `policy: "FailClosed"` 解析为 `HookErrorPolicy::FailClosed`
- 3 target: tool (L1) / agent (L2) / approval (L4)
- L4 Approval 通道：调用 `approval_handler->process_request()`

### D5 — FailOpen 默认（Oracle M1 修正后）

- 未知 pattern → log warning + skip（不抛）
- schema 非法 → throw `std::invalid_argument`
- Pattern apply 异常 → try-catch + log + 继续

### D6 — V1 简化（V1 硬编码默认注册）

- Orchestrator 构造默认注册 4 内置 Pattern（V1 简化）
- `register_pattern()` 扩展点允许 V2 自定义
- DSL 加载器 V1 简化: 直接逐行 YAML 解析（V2: 接 nlohmann JSON Schema 完整校验器）

### D7 — 命名空间卫生（Oracle H2 修正后）

- 新代码全部 `hydraforge::pdk::*` 命名空间
- 既有类型全部 `agenticdsl::*` 限定
- 无裸 `IAgentRegistry` / `ILLMProvider` 等

### D8 — 字段统一为 `type:`（Oracle H3 修正后）

- 全文档统一 `type:`（删除 `pattern_type:` 出现）

## Risks

| 风险 | 缓解 |
|---|---|
| 链深 >4 异常（Oracle B2）| wrap_chain 硬约束验证 + 测试 |
| 既有契约意外修改（Oracle B3）| 10 个文件 git diff 0 行强制验证 |
| Pattern apply 异常阻断主流程（不变量 4）| try-catch + log + continue |
| DSL YAML 解析错误暴露给用户 | schema 校验 + throw invalid_argument |

## Verification Gates

- ✅ ≥ 18 cases test_cross_cutting_* PASS
- ✅ 10 个既有契约文件 0 diff
- ✅ adr_lint 83 ADR PASS
- ✅ docs_drift_audit 0 NEW CRITICAL
- ✅ openspec validate --strict PASS
- ✅ ctest 全量 0 回归（动态基线，禁止硬编码）
- ✅ ADR-0085 状态 ✅ Approved
- ✅ cap-map §一 +1 新能力 #29
- ✅ 3 DSL examples 真实存在

## Dependencies

### 满足
- ✅ ADR-0085 v1.2 文档（设计依据）
- ✅ 6 层抽象全部 ship
- ✅ Engine.h set_llm_provider API
- ✅ HookErrorPolicy
- ✅ EventBuilder
- ✅ nlohmann::json
- ✅ IApprovalHandler (ADR-0031 ✅ ship)

### 不依赖
- MetaAgent（V2）
- 横切功能 Marketplace（V2）

## Out of Scope (V2 deferred)

- Meta-Agent 自管理
- Hot-Reload 反向取消
- 横切功能 Marketplace
- 跨 Pattern 依赖编排
- AppendOnlyEventLog 集成
- 真实 LLM tokenizer
- nlohmann JSON Schema 完整校验器（V1 简化字段必填校验）

## Success Criteria

- ADR-0085 ✅ Approved + ship 证据完整
- 4 Pattern + Orchestrator + DSL 加载器 全部 ship
- ≥ 18 cases PASS
- ctest 全量 0 回归
- 3 DSL examples 真实存在
- 10 个既有契约文件 0 diff（Oracle B3 关键不变量）
- OpenSpec archive 完成