# Proposal: ModelRouter Plugin Stub (Sprint 0)

> **变更类型**: 真实实现 (Phase 1 Sprint 0 入口)
> **作者**: Sisyphus
> **创建日期**: 2026-06-16
> **追溯范围**: `.omo/plans/phase1-execution.md` §Sprint 0 (K1 决策)
> **关联 ADR**:
> - `docs/adr/plugin/adr-0034-model-router.md` (plugin-candidate, 🔍 Proposed, K1 决策)
> - `docs/adr/adr-0022-plugin-loading.md` (🔍 Proposed, Sprint 5 实施)

## Why

ADR-0034 (`IModelRouter` 模型路由接口) 自 2026-06-09 起标注 ❌ Not Implemented, 归档于 `docs/archive/adr/`。

Phase 1 原始计划将 ModelRouter 视为 **Runtime 层** 组件 (Track 0.2 M4.1-M4.5), 由 `src/common/llm/` 提供 `IModelRouter`/`ModelRouter`/`DefaultModelRouter` 类。

但 K1 决策 (2026-06-16 锁定): **模型选择应由 Domain Plugin 智能体维护**, 而非 Runtime 内置。Runtime 端仅提供 **数据抽象** (哪些模型可用 + 各自能力), 路由决策由 Plugin 实现。

不实施此 change, Phase 1 Sprint 0 无可见交付, 后续 Sprint 4 PDK + Sprint 5 PluginLoader 缺首批 Plugin 示例。

## What Changes

### Runtime 端 (新增, 5 处修改)

- **修改** `src/common/llm/llm_types.h`:
  - 新增 `enum class ModelCapability { Chat, Completion, Embedding, ToolUse, Vision }`
  - 新增 `struct ModelInfo { name, capabilities, context_window, provider }`
  - 新增 `virtual std::vector<ModelInfo> available_models() const` (默认空实现)

- **修改** `src/common/llm/mock_provider.h` + `mock_provider.cpp`:
  - `MockLLMProvider` override `available_models()` 返回 1 个默认模型 (`mock-llm-v1`, Chat + ToolUse, ctx=4096)

### Plugin 端 (新增, 2 个 examples)

- **新建** `examples/phase1_model_router_plugin/`:
  - `main.cpp`: `ModelRouterPolicy` 类 (静态 `route()` 方法, 选第一个 Chat-capable 模型)
  - `CMakeLists.txt`: 独立可执行 (非 .so 加载, **K1 Plugin Stub 而非真 Plugin**)
  - 输出: `routed to model: mock-llm-v1`

- **新建** `examples/phase1_plugin_demo/`:
  - `main.cpp`: 端到端 demo (MockLLMProvider + ModelRouterPolicy 集成 + `generate()` 调用)
  - `CMakeLists.txt`: 独立可执行
  - 输出: 路由决策 + generate() 输出

### 测试端 (新增 1 个测试文件, 5 个 test cases)

- **新建** `tests/test_model_router_policy.cpp`:
  - `available_models()` returns non-empty (MockLLMProvider default)
  - `ModelRouterPolicy.route()` selects first Chat-capable model
  - `ModelRouterPolicy throws on empty model list`
  - `ModelRouterPolicy throws when no Chat-capable model`
  - `ModelRouterPolicy route decision includes trace_id` (Plugin Stub metadata 验证)

### 根 CMakeLists.txt (1 处修改)

- 加 2 个 `add_subdirectory(examples/phase1_*)`

## Non-goals

- ❌ 不实施真实 `.so` Plugin 加载 (依赖 Sprint 4 PDK + Sprint 5 PluginLoader)
- ❌ 不修改 Sprint 4/5 范围 (PDK 头文件 + PluginLoader 实现)
- ❌ 不复活 `IModelRouter`/`ModelRouter`/`DefaultModelRouter` Runtime 类 (K1 决策: Plugin 层替代)
- ❌ 不改 Sprint 1a ToolResult P2-P4 (Sprint 1a 单独 change)

## Success Criteria

- [x] 5/5 new test cases PASS (ctest -R test_model_router_policy)
- [x] 26/26 ctest 全量 PASS (25 旧 + 1 新)
- [x] `examples/phase1_model_router_plugin --mock` 输出含 `routed to model: mock-llm-v1`
- [x] `examples/phase1_plugin_demo --mock` 端到端跑通
- [x] commit message: `feat(plugin): add ModelRouter Plugin Stub per ADR-0022 (Sprint 0, K1)`

## Impact

- **Affected specs**:
  - `docs/specs/architecture.md` (Runtime 数据抽象章节)
  - `docs/adr/plugin/adr-0034-model-router.md` (状态更新, Sprint 0 完成后)
- **Affected code**:
  - `src/common/llm/llm_types.h` (新增 35 行)
  - `src/common/llm/mock_provider.h` (新增 5 行)
  - `src/common/llm/mock_provider.cpp` (新增 11 行)
  - `CMakeLists.txt` (新增 4 行)
  - `examples/phase1_model_router_plugin/` (新建)
  - `examples/phase1_plugin_demo/` (新建)
  - `tests/test_model_router_policy.cpp` (新建)

## 后续 Sprint 衔接

- **Sprint 5** (PluginLoader): 复用 `examples/phase1_model_router_plugin/main.cpp` 的 `ModelRouterPolicy` 逻辑, 迁移到 PDK Plugin, 通过真实 `.so` 加载验证
- **Phase 2** (异步 + EventBus): `available_models()` 扩展为异步, 支持 Provider 热更新
