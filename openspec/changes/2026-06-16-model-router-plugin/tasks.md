# Tasks: ModelRouter Plugin Stub (Sprint 0)

> **关联**: proposal.md + design.md + specs/model-router-plugin/spec.md
> **状态**: ✅ 全部完成 (2026-06-16 W1D3 提前 1 天收官, 原计划 W1D4 启动)

## S0.T1 Runtime 端 (0.2d) ✅

- [x] S0.T1.1 读 `src/common/llm/llm_types.h` 现有结构, 确认无破坏性修改面
- [x] S0.T1.2 添加 `enum class ModelCapability { Chat, Completion, Embedding, ToolUse, Vision }`
- [x] S0.T1.3 添加 `struct ModelInfo { name, capabilities, context_window, provider }`
- [x] S0.T1.4 添加 `virtual std::vector<ModelInfo> available_models() const` 默认空实现
- [x] S0.T1.5 验证: 25/25 ctest 零回归 (7.20s)

## S0.T2 Plugin 端 examples (0.5d) ✅

- [x] S0.T2.1 修改 `src/common/llm/mock_provider.h` 添加 `available_models() const override` 声明
- [x] S0.T2.2 修改 `src/common/llm/mock_provider.cpp` 实现返回 1 个 mock 模型
- [x] S0.T2.3 新建 `examples/phase1_model_router_plugin/main.cpp` 含 `ModelRouterPolicy` 类
- [x] S0.T2.4 新建 `examples/phase1_model_router_plugin/CMakeLists.txt`
- [x] S0.T2.5 新建 `examples/phase1_plugin_demo/main.cpp` 端到端 demo
- [x] S0.T2.6 新建 `examples/phase1_plugin_demo/CMakeLists.txt`
- [x] S0.T2.7 修改根 `CMakeLists.txt` 添加 2 个 `add_subdirectory`
- [x] S0.T2.8 验证: 2 个 examples `--mock` 输出 `routed to model: mock-llm-v1`

## S0.T3 单元测试 (0.2d) ✅

- [x] S0.T3.1 新建 `tests/test_model_router_policy.cpp` 含 5 个 TEST_CASE
  - [x] `available_models() returns non-empty`
  - [x] `ModelRouterPolicy.route() selects first Chat-capable model`
  - [x] `ModelRouterPolicy throws on empty model list`
  - [x] `ModelRouterPolicy throws when no Chat-capable model`
  - [x] `ModelRouterPolicy route decision includes trace_id`
- [x] S0.T3.2 验证: 5/5 new test cases PASS (13 assertions)
- [x] S0.T3.3 验证: 26/26 ctest 全量 PASS (4.56s)

## S0.T4 文档 + 收官 (0.1d) ✅

- [x] S0.T4.1 填充 OpenSpec 4 件套 (proposal + design + tasks + specs)
- [x] S0.T4.2 更新 `docs/roadmap-status.md` Sprint 0 状态行 (待 T5 commit 前)
- [x] S0.T4.3 commit `feat(plugin): add ModelRouter Plugin Stub per ADR-0022 (Sprint 0, K1)`

## Sprint 0 收官验收

- [x] 26/26 ctest PASS (含 5 个新 test cases)
- [x] `examples/phase1_model_router_plugin --mock` 跑通
- [x] `examples/phase1_plugin_demo --mock` 跑通
- [x] OpenSpec 4 件套完整
- [x] commit message 格式合规
- [x] 提前 1 天收官 (W1D3 vs 原计划 W1D4)
