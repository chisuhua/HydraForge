# Proposal: ADR-0034 — IModelRouter (PDK 示例 Plugin)

> **STATUS: PLACEHOLDER** ⚠️
> **本 change 详细 design/spec/tasks 待 Sprint 16 启动前填充**
> **触发条件**: 无硬依赖 (PDK 已 ship, 独立启动)
> **关联 ADR**: docs/adr/plugin/adr-0034-model-router.md (🔍 Proposed, plugin-candidate)
> **追溯范围**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C7

## Why

ADR-0034 IModelRouter 当前状态 🔍 Proposed (plugin-candidate), 文档位于 `docs/adr/plugin/`. 设计目标: 通过 Plugin SDK 加载第三方模型路由策略, 引擎侧仅保留接口契约.

PDK (ADR-0021) 已 ship (Sprint 4 2026-06-19). PluginLoader (ADR-0022) 已 ship (Sprint 5 2026-06-24). phase1_plugin_demo 已 ship 3 modes (Sprint 5 S5.T3).

本 change 实施 ADR-0034 作为 PDK 首个**非示例** plugin (K3 范畴), 验证:
- Runtime 数据抽象 (`ModelCapability` + `ILLMProvider::available_models()`)
- Plugin 智能体决策 (`DefaultModelRouterPolicy`)
- DECLARE_TOOL 宏 (C6 升级后)
- PluginLoader 加载 (Sprint 5 已 ship)

不解决此问题: (a) ADR-0034 长期 🔍 Proposed; (b) Phase 4 模型路由无法启动; (c) 第三方 plugin 生态缺示范; (d) 成本/质量/延迟路由无标准实现.

## What Changes (待 Sprint 16 启动前详细制定)

### 1. IModelRouter 接口定义 (Sprint 17 Day 1-2)

1. `class IModelRouter` (Plugin 侧实现):
   - `virtual ModelCapability select_model(const RoutingContext&) const = 0;`
   - `virtual std::vector<ModelCapability> available_models() const = 0;`

2. `struct RoutingContext`:
   - `task_type: string` (e.g. "code", "math", "creative")
   - `min_quality: float` (0.0-1.0)
   - `max_cost: double` (USD)
   - `max_latency_ms: int`

3. `struct ModelCapability` (Runtime 数据, 已部分设计):
   - `model_id: string`
   - `model_name: string`
   - `n_ctx: int`
   - `max_tokens: int`
   - `supports_streaming: bool`
   - `supports_function_call: bool`
   - `per_token_cost: float` (USD)
   - `tags: vector<string>`

### 2. Runtime 数据抽象 (Sprint 17 Day 3-4)

1. `ILLMProvider::available_models()` 默认实现 (返回空)
2. LLMConfig 扩展 (config file 加载多模型)
3. MockLLMProvider / OpenAI / Anthropic provider 各自实现 available_models()

### 3. DefaultModelRouter plugin 实施 (Sprint 17 Day 5-7)

1. PDK plugin 实施:
   - `ModelRouterPlugin` (plugin 入口, 注册为 PDK plugin)
   - `DefaultModelRouterPolicy` (决策: 成本路由)
   - `QualityModelRouterPolicy` (决策: 质量路由)
   - `LatencyModelRouterPolicy` (决策: 延迟路由)
   - `ModelRegistry` 工具 (DECLARE_TOOL 暴露 `query_model()`)

2. 3 种路由策略示例 (切换 plugin 决策)

### 4. 集成验证 (Sprint 17 Day 8-9)

1. `examples/phase1_model_router_plugin --mock`:
   - 加载 plugin
   - 演示 3 种路由策略
2. 集成测试 (Sprint 5 PluginLoader 验证)
3. 双仓库同步 (sync-pdk.sh)

## Capabilities (待详细制定)

### ADDED Requirements (placeholder)

- `model-router-plugin-iinterface`: `IModelRouter` 接口 MUST 完整定义 (Plugin 侧)
- `model-router-runtime-data-abstract`: Runtime MUST 提供 `ModelCapability` + `available_models()` 数据抽象
- `model-router-default-policy`: `DefaultModelRouterPolicy` MUST 可用 (成本路由)
- `model-router-quality-policy`: `QualityModelRouterPolicy` MUST 可用 (质量路由)
- `model-router-latency-policy`: `LatencyModelRouterPolicy` MUST 可用 (延迟路由)
- `model-router-plugin-pdk-integration`: PDK plugin MUST 通过 PluginLoader 加载 (Sprint 5 集成)

## Impact (待 Sprint 16 启动前评估)

**预期修改文件**:
- `include/agenticdsl/pdk/model_router_plugin.h` (新建)
- `pdk/model_router/` (新建, PDK 独立 plugin 目录)
- `include/agenticdsl/llm/model_capability.h` (新建)
- `src/common/llm/llm_types.h` (扩展 `ILLMProvider::available_models()`)
- `src/common/llm/mock_provider.cpp` (实现 available_models)
- `examples/phase1_model_router_plugin/main.cpp` (新建)
- `tests/test_model_router_plugin.cpp` (新建)

**API 兼容性**:
- `ILLMProvider::available_models()` 默认实现返回空, 现有 provider 不破坏
- `IModelRouter` 是新接口, 增量添加

## Non-goals (placeholder)

- **不重写** Phase 4.5 清理 (C8 范围)
- **不实现** Fleet 模式 16 路并行 (依赖 C2 ADR-0030 V2)
- **不修改** CognitiveWorker / DomainWorkerPool

## Estimated Effort (placeholder)

**总计**: 1-2 周 (Sprint 17 主体)

**前置依赖**: 无硬依赖 (PDK 已 ship, PluginLoader 已 ship)
**后续依赖**: 无

## 详细制定 TODO (待 Sprint 16 启动前执行)

- [ ] 1. 业务确认: 3 种路由策略的实际使用场景
- [ ] 2. 决策: plugin 内置多策略 vs 单策略多 plugin
- [ ] 3. 评估: 双仓库同步机制 (sync-pdk.sh 已就绪)
- [ ] 4. 写本 change proposal.md (What Changes 详细化)
- [ ] 5. 写 design.md (5 个 Decision: IModelRouter 接口 / RoutingContext 字段 / 3 策略算法 / Plugin 注册 / 双仓库同步)
- [ ] 6. 写 tasks.md (10-15 sections, 30-50 tasks)
- [ ] 7. 写 specs/model-router-plugin/spec.md (5-8 ADDED Requirements)
- [ ] 8. 移除所有 "PLACEHOLDER" 标记, 更新 STATUS 行
- [ ] 9. `openspec validate 2026-06-26-adr-0034-model-router-plugin` exit 0
- [ ] 10. 更新 master plan C7 状态: ⚪ placeholder → 🟡 active
- [ ] 11. 启动 Sprint 17 实施
