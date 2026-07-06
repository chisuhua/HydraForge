# ADR-0042: ILLMProvider 演进路径

## 状态

🔍 Proposed (2026-07-06 — 架构方案讨论产出, 待 review); **2026-07-06 renumber**: 兄弟 ADR-0036 → ADR-0045 (编排 plugin), ADR-0037 → ADR-0046 (通信协议), 避免与旧 ADR-0036-三层服务协议 / ADR-0037-因果序冲突

## 领域

基座 / LLM / ILLMProvider

## 关联

- ADR-0001 (ILLMProvider Streaming) — 现有 ILLMProvider 接口
- ADR-0035 (Inference Engine Plugin Spec) — 推理 Plugin 的 ILLMProvider 实现
- ADR-0022 (Plugin Loading) — PluginLoader factory 符号扩展

---

## 背景

现有 LlamaAdapterProvider (HTTP client) 和 ILLMProvider 接口。推理引擎 Plugin (ADR-0035) 替代 LlamaAdapterProvider 成为本地推理的 ILLMProvider 实现。需要定义过渡路径。

---

## 决策

### 1. 推理 Plugin 通过工厂符号暴露 ILLMProvider (P0 fix @Oracle review)

```cpp
// inference_plugin.cpp
// P0 fix: 返回 shared_ptr 而非 raw pointer, namespace 统一为 agenticdsl::
//   (对齐 ADR-0022 §1.1 + ADR-0035 §1.2 + 代码库 AGENTS.md)
extern "C" std::shared_ptr<::agenticdsl::ILLMProvider> pdk_create_llm_provider() {
  return std::make_shared<LlamaInferenceProvider>(...);
}
// P0 fix: 删除 pdk_destroy_llm_provider — shared_ptr RAII 自动管理生命周期
```

PluginLoader 在 `load_so` 后查找此符号, 由 DSLEngine 持有返回的 `shared_ptr<ILLMProvider>`。

**Namespace 统一** (P0 fix): `agenticdsl::ILLMProvider` 而非 `hydraforge::ILLMProvider`。原因:
- 代码库统一 `agenticdsl` namespace (AGENTS.md)
- `hydraforge::` 仅用于 PDK 类型 (`PluginInfo` 等 per ADR-0022 §1.2)
- ADR-0022 §1.1 已声明 `::agenticdsl::ILLMProvider`
- 同步修正 ADR-0035 §1.2（同错误）

### 2. LlamaAdapterProvider 退役路径 (P0 fix 触发条件明确)

| 阶段 | 触发条件 | 措施 |
|:----:|---------|------|
| **Phase 1** | 本 ADR Approved 即生效 | `[[deprecated]]` 标记 LlamaAdapterProvider, 推荐迁移到推理 Plugin |
| **Phase 2** | 推理 Plugin ✅ Approved + 1 release cycle 后 | 从默认 `IProviderFactory` (ADR-0005 §3 映射) 中移除 `"local"` → `LlamaAdapterProvider` 映射 |
| **Phase 3** | Telemetry 显示 30 天内 LlamaAdapterProvider 实例化计数=0 | 删除 `LlamaAdapterProvider` 实现 |

### 3. ILLMProvider 接口保持不变 + 三层消费链引用

ADR-0001 接口不需要修改。推理 Plugin 实现 `generate()`, `generate_stream()`, `available_models()`。

**关键澄清** (P0 fix per ADR-0035 §1.1): 推理 Plugin 的 ILLMProvider 是**内部接口**, 仅编排 Plugin 的 ILLMProvider 实现委托给它。DSLEngine/SimpleCognitiveOrchestrator 直接消费的是编排 Plugin 的 ILLMProvider。详见 [ADR-0035 §1.1](./adr-0035-inference-engine-plugin-spec.md) 三层消费链。

### 4. 远程 llama-server 用例处理 (P1 fix @Oracle review)

**LlamaAdapterProvider 删除后的远程推理用例**: 由独立的 HTTP ILLMProvider 承担 (推测命名 `RemoteLlamaProvider` 或 `HttpOpenAILLMProvider`), 通过 `IProviderFactory` `remote` / `openai` provider 类型路由, 已在 [ADR-0005 (LLM 后端配置与工厂)](./adr-0005-llm-backend-config-factory.md) §3 基础设施中支持。

**In-process 本地推理 (新)**: 通过 Inference Plugin 的 ILLMProvider, 由 DSLEngine 从 PluginLoader 动态注入。

```
DSLEngine::run()
  │
  │ default_provider_ = remote_provider  # ADR-0005 factory default
  │ 或 default_provider_ = inference_plugin_illmprovider  # PluginLoader 注入
  ▼
```

---

### 5. 迁移指南 (P1 fix)

| 用户场景 | 迁移路径 |
|---------|---------|
| **走 IProviderFactory** (`provider: "local"` in config) | 零改动, 默认 transition 到 Phase 2 时切换 provider string (`local` → `agentic_llama` 或 `inference` 任意) |
| **直接 `new LlamaAdapterProvider(...)`** | 改为 `OrcImpl::create_orchestrator(...)` + orchestration_plugin().get_illm_provider(), 代码重构 (提供 deprecation warning +1 release) |
| **CLI/microservice 部署** | 仍可走 remote llama-server (`remote` / `openai`), 不需迁移 |

---

*创建日期*: 2026-07-06
*修订*: 2026-07-06 (P0 fix 应用, factory signature 修正 + namespace 统一 + 阶段触发条件 + 远程用例 + 迁移指南)
*依赖*: ADR-0035 (§1.2 factory sym), ADR-0001 (interface), ADR-0022 (§1.1 sym convention), ADR-0005 (Phase 2 IProviderFactory)
