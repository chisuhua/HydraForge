# 推理引擎与编排 Plugin 双插件通信架构总览

> **状态**: ✅ 架构方案已定 (2026-07-06)
> **Oracle 审查**: ses_0ca3dce4fffeck5vmAQMs6R94m — 三关键问题 (ILLMProvider/EventBus/naming) 决议完成
> **ADR 清单**: 8 个新建 ADR (0035-0044) + 3 个现有 ADR 更新

---

## 1. 架构全景

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         HydraForge Framework                            │
│                                                                         │
│  ┌── PluginLoader ── IToolRegistry ── IInteractionBus ── DSLEngine ──┐ │
│  │                                                                     │ │
│  │  ┌─────────────────────────┐      ┌────────────────────────────┐   │ │
│  │  │ 编排 PDK Plugin         │      │ 推理引擎 PDK Plugin         │   │ │
│  │  │ (ADR-0045)              │      │ (ADR-0035, AgenticLlama)   │   │ │
│  │  │                         │      │                            │   │ │
│  │  │ ┌─ ILLMProvider ───┐   │  ① Tool  │ ┌─ Tools (inference/*) ─┐  │   │ │
│  │  │ │ (wrap tools)      │───┼─────────►│ │ inference/generate    │  │   │ │
│  │  │ │ Agent Loops       │   │  ③ Config│ │ inference/configure   │  │   │ │
│  │  │ │  - React          │───┼─────────►│ │ inference/get/status  │  │   │ │
│  │  │ │  - PlanExecute    │   │  ④ Query │ └───────────────────────┘  │   │ │
│  │  │ │  - ForkJoin       │   │◄─────────┤                            │   │ │
│  │  │ └───────────────────┘   │         │ ┌─ ILLMProvider (direct) ─┐ │   │ │
│  │  │                         │  ② Event │ │ generate/stream/query   │ │   │ │
│  │  │ ┌─ Monitor ─────────┐   │◄─────────┤ └─────────────────────────┘ │   │ │
│  │  │ │ subscribe events   │   │         │                            │   │ │
│  │  │ │ dynamic adjust     │   │         │ ┌─ Private State ─────────┐ │   │ │
│  │  │ └───────────────────┘   │         │ │ llama_model/context*      │ │   │ │
│  │  └─────────────────────────┘      │ │ KV cache, sampler chain   │ │   │ │
│  │                                    │ └───────────────────────────┘ │   │ │
│  │                                    └────────────────────────────┘   │ │
│  └─────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────┘
```

## 2. 四通道通信协议 (ADR-0046)

| 通道 | 基础设施 | 方向 | 同步性 | 用途 |
|------|---------|:---:|:---:|------|
| ① **Tool Layer** | IToolRegistry.call_tool | 编排→推理 | 同步 | 推理操作 (generate, model_load) |
| ② **Event Layer** | IInteractionBus emit/subscribe | 推理→编排 | 异步 | 生命周期通知 (state change, error) |
| ③ **Config Layer** | IToolRegistry (`inference/configure`) | 编排→推理 | 同步 | 动态参数调节 (n_threads, prefer) |
| ④ **Query Layer** | IToolRegistry (`inference/get/*`) | 编排→推理 | 同步 | 状态/性能查询 (status, models) |

**核心原则**: 所有通道复用现有 HydraForge 基础设施, **零框架改动** (符合 ADR-0021 P3)。

## 3. 关键架构决策 (Oracle 裁决)

| 决策 | 选择 | 理由 |
|------|:----:|------|
| **ILLMProvider vs Tool** | Option C — Inference Plugin 同时实现两者 | Tools 给 DSL (audited actions), ILLMProvider 给 C++ consumers (agent thinking, 不经 approval) |
| **Metrics 推送** | Option C — Hybrid (EventBus 仅 lifecycle, query 拉 performance) | 高频指标 (1000+ t/s) 走 EventBus 会触发背压; 编排按需查询更安全 |
| **工具命名** | Option A — 统一 slash (`inference/generate`) | ADR-0034 `model_router/cost` 是先例; dot 保留给 C++ methods |

## 4. ADR 文档索引

### 新建 ADR (Phase 5 B2 - 推理/编排双 Plugin)

| ADR | 标题 | 优先级 | 大小 | 状态 |
|-----|------|:------:|:---:|:----:|
| [ADR-0035](adr/adr-0035-inference-engine-plugin-spec.md) | 推理引擎 PDK Plugin 规范 | **P0** | M | 🔍 Proposed |
| [ADR-0045](adr/adr-0045-orchestration-plugin-spec.md) | 编排 PDK Plugin 规范 | **P0** | M | 🔍 Proposed |
| [ADR-0046](adr/adr-0046-plugin-communication-protocol.md) | PDK 插件间通信协议 | **P0** | M | 🔍 Proposed |
| [ADR-0038](adr/adr-0038-dynamic-config-interface.md) | 推理引擎动态配置接口 | P1 | S | 🔍 Proposed |
| [ADR-0039](adr/adr-0039-performance-metadata-contract.md) | 推理引擎性能元数据契约 | P1 | S | 🔍 Proposed |
| [ADR-0040](adr/adr-0040-inference-plugin-build-strategy.md) | 推理引擎 Plugin 构建与交付策略 | P1 | S | 🔍 Proposed |
| [ADR-0042](adr/adr-0042-illmprovider-evolution-path.md) | ILLMProvider 演进路径 | P1 | S | 🔍 Proposed |
| [ADR-0044](adr/adr-0044-inference-plugin-security-model.md) | 推理引擎 Plugin 安全模型 | P1 | S | 🔍 Proposed |

### 待建 ADR (P2)

| ADR | 标题 | 说明 |
|-----|------|------|
| ADR-0041 | PluginLoader 生命周期扩展 | pdk_plugin_init/fini 钩子 (等有 2+ 有状态 Plugin 后再标准化) |
| ADR-0043 | PDK 工具命名约定 | 规范化 slash 分层 + 命名冲突解决 (P2, 目前 ADR-0046/0034 已覆盖核心约定) |

### 已更新 ADR

| ADR | 更新内容 |
|-----|---------|
| [ADR-0034](adr/plugin/adr-0034-model-router.md) | 追加 §命名约定 — PDK tool names use slash delimiter |
| [ADR-0022](adr/adr-0022-plugin-loading.md) | 追加 §生命周期扩展对齐 — pdk_plugin_init/fini 由 ADR-0041 承接 |
| [lib/inference/*.md](lib/inference/) | 工具引用重命名 dot→slash: `inference.engine_init` → `inference/engine/init` |

## 5. 实施顺序

```
Phase 0 — Review ADR (1-2d)
  ├── ADR-0035 + ADR-0045 + ADR-0046 cross-review
  ├── ADR alignment check (vs ADR-0021/0022/0031/0034/0001)
  └── Oracle approval
        │
Phase 1 — Core Implementation (3-5d)
  ├── Inference Plugin skeleton (.so + pdk_register_tools + pdk_create_llm_provider)
  ├── AgenticLlama C API wrapper layer
  ├── Streaming bridge (IGenerationStream adapter)
  ├── Config layer (inference/configure)
  └── Query layer (inference/get/status, inference/get/models)
        │
Phase 2 — Orchestration (2-3d)
  ├── Orchestration Plugin skeleton
  ├── ILLMProvider wrapper (calls inference tools)
  ├── Event subscriptions (inference/lifecycle/*)
  └── Agent loop integration (React/PlanExecute/ForkJoin)
        │
Phase 3 — Polish (1-2d)
  ├── Unit tests (plugin + integration)
  ├── DSL workflow integration
  ├── LlamaAdapter [[deprecated]] migration
  └── Security hardening (whitelist, layer checks)
```

## 6. 设计原则

| 原则 | 来源 | 说明 |
|------|------|------|
| **AgenticLlama 内部不可见** | 用户要求 | Triton backend dispatch, llama.cpp internals 不暴露给 HydraForge core |
| **性能非黑盒** | 用户要求 | KV cache%, t/s, GPU mem 通过 get_status 暴露给编排 Plugin |
| **零框架改动** | ADR-0021 P3 | 所有通道复用 ToolRegistry + EventBus, 不新增 framework interface |
| **ReAct 内层不经审批** | Oracle 裁决 | Agent "思考" (ILLMProvider) vs "行动" (ToolRegistry) 分离 |
| **低频事件, 高频查询** | Oracle 裁决 | EventBus → lifecycle events; get_status → performance metrics |
| **统一 slash 命名** | ADR-0034 先例 | `inference/generate`, `model_router/cost`, `orchestration/route` |

---

*创建日期*: 2026-07-06
*Oracle 审查*: ses_0ca3dce4fffeck5vmAQMs6R94m
*关联*: docs/adversarial-reviews/main-report.md (B2 原始 Adversarial Review)