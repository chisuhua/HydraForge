# model-router-plugin Specification

> **STATUS: ACTIVE** 🟢 — Ready for implementation (6 ADDED Requirements, Oracle Q1-Q4 决策落地)
> **关联 ADR**: docs/adr/plugin/adr-0034-model-router.md (🔍 Proposed, plugin-candidate)
> **Oracle 审查**: 2026-07-02 (锁定 Q1-Q4 决策)

## ADDED Requirements

### Requirement: model-router-interface

`IModelRouter` 接口 MUST 完整定义: `route(RoutingContext, vector<ModelCapability>) → string` + `name()` + `ModelRoutingError` 3 错误码。

`RoutingContext` MUST 包含: `task_type`, `session_id`, `max_tokens`, `budget_remaining`, `required_tags`, `preferred_model`, `is_fleet_mode`。

`ModelCapability`(pdk 侧) MUST 包含: `model_id`, `model_name`, `n_ctx`, `max_tokens`, `supports_streaming`, `supports_function_call`, `per_token_cost`, `avg_latency_ms`, `tags`。

接口位于 `include/agenticdsl/pdk/model_router.h` (PDK 命名空间 `agenticdsl::pdk`), 不在 `src/common/llm/`。

#### Scenario: route 返回有效模型 ID

- **GIVEN** 一个 CostModelRouterPolicy 实现 IModelRouter
- **AND** 候选模型列表包含 `gpt-4` (cost=0.03) 和 `gpt-3.5-turbo` (cost=0.002)
- **AND** RoutingContext 的 `required_tags` = ["general"]
- **WHEN** 调用 `router.route(ctx, candidates)`
- **THEN** 返回 `"gpt-3.5-turbo"` (最便宜+匹配 tag)

#### Scenario: route 抛出 NoViableModel

- **GIVEN** RoutingContext 的 `budget_remaining` = 0.001 (低于所有模型 cost)
- **AND** 候选模型最小 `per_token_cost` = 0.002
- **WHEN** 调用 `router.route(ctx, candidates)`
- **THEN** 抛出 `ModelRoutingError` 且 `error.code == Code::NoViableModel`

#### Scenario: model-router-interface PDK 位置正确

- **GIVEN** `#include <agenticdsl/pdk/model_router.h>`
- **WHEN** 编译 Plugin 代码
- **THEN** 不依赖 `#include "common/llm/llm_types.h"`
- **AND** 所有 IModelRouter 相关类型在 `agenticdsl::pdk` 命名空间

### Requirement: model-router-plugin-entry

每个 Plugin MUST 通过 `pdk_register_tools(IToolRegistry&)` 精确注册 1 个 `model_router/<strategy>` 工具。

Plugin `.so` 必须 export `extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry&)` 符号。

工具名使用分层前缀 `model_router/` 避免命名冲突。

#### Scenario: cost plugin 注册成功

- **GIVEN** 加载 `hydraforge_model_router_cost.so`
- **WHEN** PluginLoader 调用 `pdk_register_tools(registry)`
- **THEN** `registry.has_tool("model_router/cost")` 返回 true
- **AND** `registry.list_tools()` 包含 `model_router/cost` (不包含其他无关注册)

#### Scenario: 三个 Plugin 共存无冲突

- **GIVEN** 依次加载 cost / quality / latency 三个 Plugin
- **WHEN** 检查 `registry.list_tools()`
- **THEN** 包含 `model_router/cost`, `model_router/quality`, `model_router/latency`
- **AND** 三者各自独立, 互不干扰

#### Scenario: tool 调用返回 router handle

- **GIVEN** `model_router/cost` tool 已注册
- **WHEN** 调用 `registry.call_tool("model_router/cost", args)`
- **THEN** 返回 json 包含 `{"model_id": "gpt-3.5-turbo", "router": "cost"}`

### Requirement: cost-strategy-end-to-end

成本路由策略 MUST 在 capability tag 匹配的前提下返回 `per_token_cost` 最低的模型。

`budget_remaining` 超标时 MUST 跳过对应模型, 全模型超标时 throw `NoViableModel`。

#### Scenario: 返回最便宜 tag-matching 模型

- **GIVEN** candidates = [gpt-4 (cost=0.03, tags=["general","code"]), gpt-3.5 (cost=0.002, tags=["general"])]
- **AND** ctx.required_tags = ["general"]
- **AND** ctx.budget_remaining = 不设限
- **WHEN** CostModelRouterPolicy::route(ctx, candidates)
- **THEN** 返回 "gpt-3.5-turbo"

#### Scenario: 跳过超预算模型

- **GIVEN** candidates = [gpt-4 (cost=0.03), gpt-3.5 (cost=0.002), claude-3 (cost=0.015)]
- **AND** ctx.budget_remaining = 0.01
- **WHEN** CostModelRouterPolicy::route(ctx, candidates)
- **THEN** 跳过 gpt-4 (cost=0.03 > 0.01)
- **AND** 返回 "claude-3" (cheapest under budget = 0.015, gpt-3.5 更便宜 0.002 应被选)
- **重审**: gpt-3.5 cost=0.002 < 0.01 budget → 返回 "gpt-3.5-turbo" (绝对最低且未超预算)

#### Scenario: 全超预算抛出 NoViableModel

- **GIVEN** candidates = [gpt-4 (cost=0.03), claude-3 (cost=0.015)]
- **AND** ctx.budget_remaining = 0.01
- **WHEN** CostModelRouterPolicy::route(ctx, candidates)
- **THEN** 抛出 ModelRoutingError(NoViableModel)
- **AND** error.what() 包含 "[NoViableModel]" 前缀

#### Scenario: 无 tag 匹配抛出 NoViableModel

- **GIVEN** candidates 全部 tags=["general"]
- **AND** ctx.required_tags = ["vision"]
- **WHEN** CostModelRouterPolicy::route(ctx, candidates)
- **THEN** 抛出 ModelRoutingError(NoViableModel)

### Requirement: quality-strategy-end-to-end

质量路由策略 MUST 按 `tags` 匹配度排序, 匹配 tag 最多的模型优先。

无 tag 匹配时 MUST fallback 到默认模型 (第一个候选) 而非 throw。

#### Scenario: tag 完全匹配优先

- **GIVEN** gpt-4 tags=["general","reasoning","vision","code"], gpt-3.5 tags=["general"]
- **AND** ctx.required_tags = ["reasoning", "code"]
- **WHEN** QualityModelRouterPolicy::route(ctx, candidates)
- **THEN** 返回 "gpt-4" (匹配 2/2 tags) > gpt-3.5 (匹配 0/0, 0 matches from required tags perspective — 实际上 general match?)

注意: 更精确的匹配度计算: 统计 `required_tags` 中有多少在 model.tags 中。gpt-4 匹配 reasoning+code=2, gpt-3.5 匹配 0 → gpt-4 胜出。

#### Scenario: 部分匹配按分数排序

- **GIVEN** gpt-4 tags=["general","code"], claude-3 tags=["general","reasoning","code"]
- **AND** ctx.required_tags = ["reasoning", "code"]
- **WHEN** QualityModelRouterPolicy::route(ctx, candidates)
- **THEN** 返回 "claude-3" (匹配 reasoning+code=2) > gpt-4 (仅匹配 code=1)

#### Scenario: 无 tag 匹配 fallback 到第一个模型

- **GIVEN** 所有模型 tags=["general"], ctx.required_tags=["vision"]
- **WHEN** QualityModelRouterPolicy::route(ctx, candidates)
- **THEN** 返回 candidates[0].model_id (fallback, 不 throw)
- **AND** 通过 bus emit `tool.audit.warning` (若 bus 注入)

#### Scenario: empty tag 返回最高能力模型

- **GIVEN** ctx.required_tags = [] (无 tag 约束)
- **WHEN** QualityModelRouterPolicy::route(ctx, candidates)
- **THEN** 按 `n_ctx + max_tokens` 总分排序, 返回总分数最高的模型

### Requirement: latency-strategy-end-to-end

延迟路由策略 MUST 在 capability tag 匹配的前提下返回 `avg_latency_ms` 最低的模型。

`max_latency_ms` 超标时 MUST 跳过对应模型, 全模型超 latency budget 时 throw `NoViableModel`。

#### Scenario: 返回最低延迟模型

- **GIVEN** gpt-4 (latency=500ms), gpt-3.5 (latency=200ms), claude-3 (latency=350ms)
- **WHEN** LatencyModelRouterPolicy::route(ctx, candidates)
- **THEN** 返回 "gpt-3.5-turbo"

#### Scenario: 跳过超 latency budget 模型

- **GIVEN** ctx.max_latency = 300 (通过 RoutingContext 的扩展字段或 context args)
- **AND** gpt-4 (latency=500ms), gpt-3.5 (latency=200ms), claude-3 (latency=350ms)
- **WHEN** LatencyModelRouterPolicy::route(ctx, candidates)
- **THEN** 跳过 gpt-4 (500 > 300) 和 claude-3 (350 > 300)
- **AND** 返回 "gpt-3.5-turbo" (200ms ≤ 300)

#### Scenario: 全超 latency budget 抛出 NoViableModel

- **GIVEN** ctx.max_latency = 100
- **AND** 所有模型 latency > 100ms
- **WHEN** LatencyModelRouterPolicy::route(ctx, candidates)
- **THEN** 抛出 ModelRoutingError(NoViableModel)

#### Scenario: capability tag 约束优于延迟

- **GIVEN** gpt-4 (latency=500ms, tags=["vision"]), gpt-3.5 (latency=200ms, tags=["general"])
- **AND** ctx.required_tags = ["vision"]
- **WHEN** LatencyModelRouterPolicy::route(ctx, candidates)
- **THEN** 返回 "gpt-4" (只有它满足 vision tag, 即使延迟更高)

### Requirement: model-registry-tool

`model_router/registry` MUST 提供 `DECLARE_TOOL` 注册的查询工具, 返回 Provider 可用模型列表, 支持按 capability tag 筛选。

#### Scenario: 返回全量模型列表

- **WHEN** 调用 `call_tool("model_router/registry", {})` (无 tag 筛选)
- **THEN** 返回 json array, 长度 = 所有已注册模型数
- **AND** 每个元素含 `model_id`, `model_name`, `n_ctx`, `tags` 字段

#### Scenario: 按 tag 筛选

- **WHEN** 调用 `call_tool("model_router/registry", {{"tag", "vision"}})`
- **THEN** 仅返回 tags 包含 "vision" 的模型
- **AND** 不含无 "vision" tag 的模型

#### Scenario: 无匹配 tag 返回空列表

- **WHEN** 调用 `call_tool("model_router/registry", {{"tag", "quantum"}})`
- **THEN** 返回空 json array `[]`
- **AND** 不 throw 异常

## VERIFICATION

### Build & Test

```bash
cmake --preset tests && make -j$(nproc)   # 编译通过
ctest --output-on-failure                 # ≥ 57+3+4 = 64+ PASS
cmake --preset asan && ctest              # 0 memory error
cmake --preset tsan && ctest              # 0 data race
```

### Ship Gate (C7)

- [ ] `openspec validate 2026-06-26-adr-0034-model-router-plugin` exit 0
- [ ] ctest ≥ 64+ (57 baseline + 4 test_model_router_plugins + 3 plugin-strategy tests)
- [ ] ASan 100% clean
- [ ] TSan 100% clean
- [ ] 3 Plugin .so 构建成功 (cost / quality / latency)
- [ ] `examples/phase1_model_router_plugin --mock` 演示 3 策略
- [ ] `scripts/sync-pdk.sh` 零改动, 同步成功
- [ ] ADR-0034 状态不变 (🔍 Proposed → ✅ Approved 在 C7 ship 时更新)
- [ ] master plan C7 状态 ⚪ 占位 → ✅ shipped