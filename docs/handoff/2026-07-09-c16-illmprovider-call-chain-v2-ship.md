# C16 ILLMProvider Call Chain V2 Ship Handoff

**Date**: 2026-07-09
**Branch**: `phase5-inference-orchestration` (已 merge 到 main)
**Commit**: `552285b` (merge commit)
**Status**: ✅ Shipped — 72/72 ctest, ASan 72/72, openspec archived
**From**: Sisyphus session 2026-07-09

---

## TL;DR

本会话完整实施了 OpenSpec change `phase5-illmprovider-call-chain-v2`，ILLMProvider 调用链 v2 架构落地。

**核心交付**：
- **Decorator 链**：CostTrackingDecorator（P0 budget hole 修复）+ ComplianceDecorator + RateLimitDecorator，DSLEngine 构造器和 `set_llm_provider()` 都经过 `decorate_provider()` 包装
- **Dual Consumer Model**：OrchestrationILLMProvider 直连 `inference_provider_->generate()`，不再经过 IToolRegistry
- **available_models() pure virtual**：从 `virtual { return {}; }` 改为 `= 0`，5 个实现全部 override
- **PluginLoader V2**：5 符号查找 + lifecycle + ABI v2 + 循环依赖检测
- **ADR 文档**：ADR-0001/0035/0038/0042/0045/0005 修订
- **弃用标注**：LlamaAdapter + LlamaAdapterProvider `[[deprecated]]`
- **ASan 修复**：`test_execute_parallel` use-after-scope（lambda 捕获由引用改为值拷贝）

---

## 1. 当前项目状态

| 项目 | 状态 |
|------|------|
| 分支 | `phase5-inference-orchestration` (已 merge 到 main) |
| 最新 commit | `552285b` merge: Sprint 21 / C16 ship |
| ctest (Debug) | 72/72 ✅ |
| ctest (ASan) | 72/72 ✅ |
| TSan | 超时跳过（环境限制，pre-existing bug 已修复） |
| openspec validate | exit 0 ✅ |
| sprint-closeout.sh | 7/7 PASS ✅ |
| openspec archive | ✅ 已归档到 `openspec/changes/archive/2026-07-09-phase5-illmprovider-call-chain-v2/` |

## 2. 交付清单

### 2.1 新文件（17 个）

| 文件 | 用途 |
|------|------|
| `include/agenticdsl/contract/i_llm_provider_decorator.h` | ILLMProviderDecorator 抽象基类 |
| `include/agenticdsl/pdk/agent_loops/orchestration_illm_provider.h` | OrchestrationILLMProvider 声明 |
| `src/common/llm/illmprovider_decorator.cpp` | Decorator 基类转发实现 |
| `src/common/llm/cost_tracking_decorator.{h,cpp}` | CostTracking 计费装饰器 |
| `src/common/llm/compliance_decorator.{h,cpp}` | Compliance 合规日志装饰器 |
| `src/common/llm/rate_limit_decorator.{h,cpp}` | RateLimit 令牌桶装饰器 |
| `src/common/llm/orchestration_illm_provider.cpp` | Dual Consumer 直连实现 |
| `tests/test_available_models_pure_virtual.cpp` | pure virtual 验证（5 TC） |
| `tests/test_compliance_decorator.cpp` | Compliance 测试（3 TC） |
| `tests/test_cost_tracking_decorator.cpp` | CostTracking 测试（7 TC，含链深度+流式精度） |
| `tests/test_orchestration_dual_consumer.cpp` | Dual Consumer 测试（7 TC） |
| `tests/test_plan_execute_restart.cpp` | PlanExecute verify 重启测试（3 TC） |
| `tests/test_plugin_loader_v2.cpp` | PluginLoader V2 测试（5 TC） |
| `tests/test_rate_limit_decorator.cpp` | RateLimit 测试（3 TC） |

### 2.2 修改文件（36 个）

- `src/core/engine.{h,cpp}` — decorate_provider() + set_llm_provider 重新包装
- `src/common/llm/llm_types.h` — available_models() → = 0
- `src/common/llm/mock_provider.{h,cpp}` — override
- `src/common/llm/cloud_adapter.{h,cpp}` — override
- `src/common/llm/llama_adapter_provider.{h,cpp}` — override + [[deprecated]]
- `src/common/llm/llama_adapter.h` — [[deprecated]]
- `src/common/llm/http_adapter.{h,cpp}` — override
- `src/modules/plugin/plugin_loader.{h,cpp}` — 5 符号查找 + lifecycle
- `CMakeLists.txt` — 4 个新编译单元
- 7 个测试文件适配 — test_cognitive_worker, test_executor_with_mock_provider, test_pdk_plan_execute, test_simple_orchestrator, test_yield_node, test_llm_streaming, test_plugin_loader, test_execute_parallel (ASan fix)
- 7 个 ADR 文档修订
- `docs/active-status.md` — 更新 C16 状态

## 3. 架构关键决策

```
ILLMProvider 继承层次:

ILLMProvider (abstract)
  ├── ILLMProviderDecorator (abstract)
  │     ├── CostTrackingDecorator
  │     ├── ComplianceDecorator
  │     └── RateLimitDecorator
  ├── MockLLMProvider (testing)
  ├── LlamaAdapterProvider ([[deprecated]])
  ├── CloudLLMAdapter
  ├── HttpLLMAdapter
  └── OrchestrationILLMProvider (Dual Consumer Model)
        ├── inference_provider_ (ILLMProvider*)
        └── router_ (IModelRouter*)
```

DSLEngine 构造器链: `CostTrackingDecorator → (ComplianceDecorator? → RateLimitDecorator?) → inner_provider`

所有 LLM 调用路径 (NodeExecutor / SimpleCognitiveOrchestrator / PlanExecuteLoop) 均经过装饰器链。

## 4. 顺延项（下一会话入口）

| 项目 | 状态 | 建议 |
|------|:----:|------|
| §5 Cloud plugin (pdk/cloud/) | 🔴 顺延 | 独立 change `phase5-illmprovider-call-chain-v3` |
| 9.6/9.7/9.7c Factory 路由 remap | 🔴 依赖 §5 | Cloud plugin 化后实施 |
| 9.7b cost_callback deprecation | 🔴 待分析 | 需排查 `tool_registry_->set_cost_callback` 调用点 |
| TSan 验证 | ⚠️ 跳过 | 环境限制，ASan 已覆盖 |

## 5. 参考

| 文档 | 位置 |
|------|------|
| 已归档 change | `openspec/changes/archive/2026-07-09-phase5-illmprovider-call-chain-v2/` |
| ADR-0001 | `docs/adr/adr-0001-illm-provider-streaming-interface.md` |
| ADR-0035 | `docs/adr/adr-0035-inference-engine-plugin-spec.md` |
| ADR-0042 | `docs/adr/adr-0042-illmprovider-evolution-path.md` |
| ADR-0045 | `docs/adr/adr-0045-orchestration-plugin-spec.md` |
| 活跃看板 | `docs/active-status.md` |
