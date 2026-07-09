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

### 2. LlamaAdapterProvider 退役路径 (P0 fix 触发条件明确; 2026-07-09 修订 per OpenSpec change `phase5-illmprovider-call-chain-v2` Task 7.6 + Decision 4)

| 阶段 | 触发条件 | 措施 |
|:----:|---------|------|
| **Phase 1** | 本 ADR Approved 即生效 | `[[deprecated]]` 标记 `LlamaAdapterProvider` + `LlamaAdapter` (底层 HTTP 包装), 推荐迁移到推理 Plugin |
| **Phase 2** | 推理 Plugin (`pdk/llama_engine/`) ✅ Approved + 1 release cycle 后 | **remap** `"local"` → `llama_engine` Plugin / `pdk/llama_engine/` (per ADR-0042 §2 修订; 用户配置零改动, factory 内部路由切换, `LlamaAdapterProvider` 不再被默认实例化) |
| **Phase 3** | Phase 2 完成后**再 2 个 minor release cycles** (估算 ~6-12 个月, per 项目 release cadence; 原 "Telemetry 30 天零实例化" trigger 不可执行 — 当前仓库无 telemetry 基础设施) | 删除 `LlamaAdapterProvider` + `LlamaAdapter` (底层 HTTP 包装) 实现 |

**Phase 2/3 时间线明确定义** (消除歧义):

| Phase | 触发时间 | 同步信号 |
|---|---|---|
| **Phase 1** | 本 ADR Approved 后立即 | 当前 OpenSpec change `phase5-illmprovider-call-chain-v2` ship |
| **Phase 2** | 推理 Plugin (`pdk/llama_engine/`) ✅ Approved + **下一个 minor release** (per ADR-0042 §2 trigger) | 跟踪信号: `docs/adr/adr-0035-inference-engine-plugin-spec.md` 状态从 🔍 Proposed → ✅ Approved + `openspec/changes/phase5-llama-engine-plugin/` archived |
| **Phase 3** | Phase 2 ship 后**再 2 个 minor release** (估算 ~6-12 个月, per 项目 release cadence) | 跟踪信号: `git log --grep="v0\."` 计数 + OpenSpec change `phase5-illmprovider-call-chain-v2` 标记为 Phase 3 ready |

**注意**:
- "release cycle" 指 HydraForge minor release (估算 3-6 个月一次), 非 minor commit 或 patch release
- Phase 2 实际接入点已经在本 change 中预留 (`Task 5.12c`), 无需新 OpenSpec change
- Phase 3 删除 `LlamaAdapterProvider` + `LlamaAdapter` 需要新建独立 OpenSpec change (2027+)

**Deprecate 范围 (2026-07-09 修订)**: 同时 deprecate `LlamaAdapterProvider` (ILLMProvider 适配器) + `LlamaAdapter` (底层 llama.cpp HTTP 包装), 关闭 escape hatch (避免用户直接 `new LlamaAdapter` 绕过 factory)。

**`[[deprecated]]` 标注** (非 breaking):

```cpp
// src/common/llm/llama_adapter_provider.h
class [[deprecated("Use pdk/llama_engine/ plugin instead, see ADR-0042 §2")]]
    LlamaAdapterProvider : public ILLMProvider {
  // ...
};

// src/common/llm/llama_adapter.h
class [[deprecated("LlamaAdapter is deprecated; use pdk/llama_engine/ plugin, see ADR-0042 §2")]]
    LlamaAdapter {
  // ...
};
```

### 3. ILLMProvider 接口保持不变 + 三层消费链引用

ADR-0001 接口不需要修改。推理 Plugin 实现 `generate()`, `generate_stream()`, `available_models()`。

**关键澄清** (P0 fix per ADR-0035 §1.1): 推理 Plugin 的 ILLMProvider 是**内部接口**, 仅编排 Plugin 的 ILLMProvider 实现委托给它。DSLEngine/SimpleCognitiveOrchestrator 直接消费的是编排 Plugin 的 ILLMProvider。详见 [ADR-0035 §1.1](./adr-0035-inference-engine-plugin-spec.md) 三层消费链。

### 4. 远程 llama-server 用例处理 (P1 fix @Oracle review) + Cloud Plugin 化 (2026-07-09 修订, per ADR-0042 §4 修订 + OpenSpec change `phase5-illmprovider-call-chain-v2` Decision 3)

**LlamaAdapterProvider 删除后的远程推理用例**: 由独立的 HTTP ILLMProvider 承担 (推测命名 `RemoteLlamaProvider` 或 `HttpOpenAILLMProvider`), 通过 `IProviderFactory` `remote` / `openai` provider 类型路由, 已在 [ADR-0005 (LLM 后端配置与工厂)](./adr-0005-llm-backend-config-factory.md) §3 基础设施中支持。

**In-process 本地推理 (新)**: 通过 Inference Plugin 的 ILLMProvider, 由 DSLEngine 从 PluginLoader 动态注入。

**Cloud 后端 plugin 化 (2026-07-09 新增)**: 推翻原 "cloud 留 HTTP 客户端在核心" 决议。CloudLLMAdapter 从 `src/common/llm/` 移至 `pdk/cloud/` 作为 first-party plugin, 所有 backend (cloud + local) 统一走 PDK plugin 机制。理由: (1) 5 年视角下统一机制 vs 两套机制, 前者总成本 < 后者; (2) cloud 路径补全 lifecycle hooks (`pdk_plugin_init/fini`) 支持连接池、key rotation; (3) 内部 gateway 代理、自定义 auth 等部署需求可由第三方 plugin 满足; (4) 与推理 plugin 共享 ABI, 架构对称。详见 [ADR-0042 §4 + OpenSpec change `phase5-illmprovider-call-chain-v2` Design Decision 3](./adr-0042-illmprovider-evolution-path.md)。

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
