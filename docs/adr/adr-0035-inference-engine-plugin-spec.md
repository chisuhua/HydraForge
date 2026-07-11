# ADR-0035: 推理引擎 PDK Plugin 规范

## 状态

✅ Approved (2026-07-10 — OpenSpec change `phase5-llama-engine-plugin` (C14) ship); **2026-07-06 renumber**: 兄弟 ADR-0036 → ADR-0045 (编排 plugin), ADR-0037 → ADR-0046 (通信协议), 避免与旧 ADR-0036-三层服务协议 / ADR-0037-因果序冲突

> **实施依据**: `phase5-llama-engine-plugin` (C14) 已 ship + archived (2026-07-08), 验证: `pdk/llama_engine/` 12 个推理工具注册 (`inference/engine/{init,generate,stream,status}` × 4 + `inference/model/{load,unload,list,switch}` × 4 + C13 4 个 schema 工具 `prefix_cache/kv_cache/decoding/cloud_engine` `.configure`) + DSLEngine `load_plugin()` 显式 API + PluginLoader 5 符号查找 + 65/65 ctest + 0 回归。详见 `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §三 C14 行 + `docs/active-status.md` §五 2026-07-08 行 + `openspec/changes/archive/2026-07-08-phase5-llama-engine-plugin/`。

## 领域

基座 / LLM / PDK Plugin

## 关联

- ADR-0021 (PDK Design) — Plugin SDK 模式, static linking, test stubs
- ADR-0022 (Plugin Loading) — dlopen 加载, pdk_plugin_info, pdk_register_tools
- ADR-0034 (Model Router) — 无状态路由插件范式 (C7), slash 命名先例
- ADR-0001 (ILLMProvider Streaming) — ILLMProvider 流式接口, IGenerationStream
- ADR-0004 (ToolRegistry Security) — ToolMetadata V2, ToolCategory, ApprovalPolicy
- ADR-0031 (Execution Policy) — IExecutionPolicy, ToolCoordinator, approval pipeline
- ADR-0046 (Plugin Communication Protocol) — 四通道通信架构

---

## 背景

### 问题

HydraForge 当前缺少**本地推理引擎**的标准化 PDK Plugin 规范：

| 维度 | 现状 | 问题 |
|------|------|------|
| **本地推理** | LlamaAdapterProvider → HTTP 转发到外部 llama-server | 非原生 llama.cpp 集成, 性能损耗, 无 Triton 后端 |
| **Plugin 接口** | 仅 ADR-0034 无状态路由模式 | 推理引擎是有状态长生命周期 plugin, 需要新范式 |
| **配置控制** | 硬编码 Config, 无运行时动态调节 | 编排层无法按负载调整线程/采样策略 |
| **性能可见性** | 黑盒 — 编排层不知 KV cache 使用率、GPU 内存 | 无法做智能调度决策 |

### 目标

为 AgenticLlama (llama.cpp fork) 封装为标准化 PDK Plugin，定义其：
1. 注册的工具集 (ToolRegistry)
2. 暴露的 ILLMProvider 接口
3. 生命周期状态机
4. 配置 Schema
5. 流式推理桥接

### 架构定位

```
HydraForge Framework
├── IToolRegistry
├── IInteractionBus
├── PluginLoader
│
├── [推理引擎 Plugin] ← 本文档定义
│   ├── Tools: inference/* (DSL workflows 调用)
│   ├── ILLMProvider (C++ consumers 直接调用)
│   ├── 内部状态: llama_model*, llama_context*, sampler_chain*
│   └── 性能指标: t/s, KV cache, GPU mem
│
└── [编排 Plugin] ← ADR-0045 定义
    ├── 策略决策 (模型选择, 路由, ReAct 循环)
    ├── 调用 inference/* tools → 推理引擎
    └── 订阅 inference.* events → 状态监控
```

---

## 决策

### 1. 双接口暴露: Tools + ILLMProvider (Oracle 推荐 Option C)

**关键决策**: 推理引擎 Plugin **同时**实现 PDK ToolRegistry 工具注册 + ILLMProvider 接口。

#### 1.1 Dual Consumer Model (Oracle 裁决, 2026-07-06; 重画 2026-07-09 per OpenSpec change `phase5-illmprovider-call-chain-v2` Task 7.2)

> **修订说明 (2026-07-09, per OpenSpec change `phase5-illmprovider-call-chain-v2` Decision 1)**: 原三层消费链图被本 Dual Consumer Model 替换。`OrchestrationILLMProvider::generate()` 内部从 `call_tool("inference/generate", ...)` 改为**直连** `inference_provider_->generate()` (共享 `shared_ptr<ILLMProvider>`)。Agent 循环 (ReAct/PlanExecute/ForkJoin) 通过 `engine_->get_llm_provider()` 获取 raw ILLMProvider*, **绕开编排包装**。LLM "thought" 不再经 ToolCoordinator audit pipeline (修复 ADR-0031 §决策 5 语义误用)。

```
┌────────────────── Orchestration Plugin ──────────────────┐
│                                                           │
│  ┌────────────────────────────┐  ┌─────────────────────┐ │
│  │ OrchestrationILLMProvider  │  │ Agent Loops         │ │
│  │ (路由 + 会话管理)            │  │ (ReAct/PlanExec/    │ │
│  │                            │  │  ForkJoin)          │ │
│  │ 消费者: DSLEngine/          │  │                     │ │
│  │          NodeExecutor       │  │ 消费者: 循环自身      │ │
│  │                            │  │                     │ │
│  │ 直连推理 (no Tool dispatch)│  │ 直连推理(同样无包装) │ │
│  └──────────┬─────────────────┘  └──────────┬──────────┘ │
│             │                                 │            │
│             ▼                                 ▼            │
│        ┌──────────────────────────────────────────────┐    │
│        │  推理 Plugin ILLMProvider                   │    │
│        └────────────────────┬─────────────────────────┘    │
└────────────────────────────┼───────────────────────────────┘
                             ▼
                       llama_decode()
```

**关键洞察 (修订后)**: 推理 Plugin 的 ILLMProvider 是**内部接口** (仅编排 Plugin + Agent 循环使用),不直接暴露给 DSLEngine/SimpleCognitiveOrchestrator 的对外消费路径。**编排层 ILLMProvider 真实价值** = 路由 + 会话管理 (ADR-0045 §2.2),不依赖 Tool dispatch 即可实现。

| 接口 | 暴露给 | 语义 | 走审批? |
|------|--------|------|:------:|
| **`inference/*` Tools** | DSL workflow 节点 (经 ToolCoordinator) | Agent "行动" (audited tool call) | ✅ ToolCoordinator |
| **ILLMProvider (推理 Plugin)** | 编排 Plugin + Agent 循环 (C++ 直接调用, 不经 ToolRegistry) | Agent "思考" (LLM reasoning) | ❌ 不经审批 |

**依据**: ReAct 循环中 LLM 生成 thought 是 agent 的推理内层, ToolCoordinator 审批的是 agent 对外调用的 tool。把 LLM 生成塞进 tool approval 等于语义误用 (违反 ADR-0031 §决策 5 设计意图)。Oracle 实证 (代码 trace 2026-07-06): Tool dispatch 开销 ~5μs vs llama_decode ~200ms = 0.0025%, 性能不是争论点; 但 LLM thought 经 ToolCoordinator audit pipeline 是**语义误用** (ADR-0031 §决策 5)。

**与原三层链的差异**:
- 原方案: DSLEngine → 编排 ILLMProvider → `inference/generate` tool (经内部 registry + ToolCoordinator 豁免)
- 新方案 (Dual Consumer Model): DSLEngine → 编排 ILLMProvider (直连推理 Provider) → llama_decode(); Agent 循环平行路径直连推理 Provider (绕开编排包装)

#### 1.2 Plugin 工厂符号规范 (P0 fix @Oracle review, namespace 统一 @P1 review)

```cpp
// inference_plugin.cpp
// P1 fix: namespace 统一为 ::agenticdsl:: (对齐代码库 AGENTS.md + ADR-0022 §1.1)
extern "C" std::shared_ptr<::agenticdsl::ILLMProvider> pdk_create_llm_provider() {
  return std::make_shared<LlamaInferenceProvider>(...);
}
```

**符号约定** (在 ADR-0022 §1.1 同步追加):
- **第三个符号**: `extern "C" std::shared_ptr<::agenticdsl::ILLMProvider> pdk_create_llm_provider()`
- shared_ptr 而非 raw pointer: 跨 .so 边界安全, 与 C++ RAII 一致
- 与 ADR-0001 ILLMProvider 接口完全兼容, 消费者零修改
- PluginLoader 加载 .so 后查找此符号, DSLEngine 持有返回的 shared_ptr
- PluginLoader 卸载 .so 前, 必须释放所有 shared_ptr (析构顺序 — 参考 Sprint 17 C7 destruction order bug)
- **Namespace 统一**: `agenticdsl::` for runtime contract types (ILLMProvider, IToolRegistry, IExecutionPolicy); `hydraforge::` for PDK-specific types (PluginInfo). P1 review 同步修正 [ADR-0042 §1](./adr-0042-illmprovider-evolution-path.md).

### 2. 工具集命名与注册 (遵循 ADR-0034 slash 命名先例)

全部使用 slash 分隔符, 注册到 IToolRegistry:

| 工具名 | ToolCategory | ApprovalPolicy | Min Layer | 功能 |
|--------|:-----------:|:--------------:|:---------:|------|
| `inference/engine/init` | **StateModify** ⬅️ P1 fix | agent | Workflow | 初始化推理后端 (静态配置; 状态 UNINITIALIZED → INITIALIZED) |
| `inference/model/load` | StateModify | plan | Workflow | 加载 GGUF 模型 |
| `inference/model/unload` | StateModify | plan | Workflow | 卸载模型 |
| `inference/model/list` | ReadOnly | none | Thinking | 列出已加载模型 |
| `inference/model/switch` | StateModify | plan | Workflow | 切换活跃模型 |
| `inference/session/create` | StateModify | agent | Workflow | 创建推理会话 |
| `inference/session/destroy` | StateModify | agent | Workflow | 销毁推理会话 |
| `inference/generate` | Execute (按 analogy, 详见 ADR-0044 §2 注) | agent | Workflow | 同步文本生成 |
| `inference/generate/stream` | Execute | agent | Workflow | 流式文本生成 |
| `inference/configure` (L3a) | StateModify | plan | Workflow | 动态调节数值参数 (线程/偏好) |
| `inference/sampler/configure` (L3b) | StateModify | agent | Workflow | 配置采样策略 (chain topology) |
| `inference/get/status` | ReadOnly | none | Thinking | 获取性能指标快照 (atomic snapshot, schema per ADR-0039) |
| `inference/get/models` | ReadOnly | none | Thinking | 获取模型能力列表 (mirror ILLMProvider.available_models() per ADR-0034) |

> 备选 C13 schema tools (prefix_cache, kv_cache, decoding, cloud_engine): 遵循相同命名模式 `inference/{component}/configure`，详见 ADR-0038。

### 3. 生命周期状态机

```
UNINITIALIZED ──→ inference/engine/init ──→ INITIALIZED
                                                │
                                   inference/model/load
                                                │
                                                ▼
                                           MODEL_LOADED
                                                │
                                inference/session/create
                                                │
                                                ▼
                                          SESSION_ACTIVE
                                           │          │
                           inference/generate    inference/configure
                                           │          │
                                           ▼          ▼
                                        GENERATING   (参数更新)
                                           │
                              (完成/error/abort)
                                           │
                                           ▼
                                      SESSION_ACTIVE
                                           │
                              inference/session/destroy
                                           │
                                           ▼
                                       INITIALIZED
                                           │
                              inference/model/unload
                                           │
                                           ▼
                                       INITIALIZED
```

**`inference/model/switch` 行为**: GENERATING 状态下拒绝 (返回 ToolResult::error(ErrorCode::ResourceExhausted, "engine busy, retry later"))。SESSION_ACTIVE 状态允许切换, transition 到新 model 上下文重建。
```

**状态转换事件** (通过 IInteractionBus emit, P1 fix 用 dot 分隔符对齐 [ADR-0046 §2](./adr-0046-plugin-communication-protocol.md)):
- `inference.lifecycle.{state}` — 每次状态变更时 emit (idle, running, model_loaded, model_unloaded, session_active, context_overflow 等)
- `inference.model.{action}` — 模型生命周期 (loaded, unloaded, switched)
- `inference.error.{code}` — 推理错误 (oom, network, cancelled, context_overflow) + retryable 标记

### 4. 配置参数分层

> **Deferred 注记 (2026-07-09, per OpenSpec change `phase5-illmprovider-call-chain-v2` Task 7.4 + Adversarial Review 2026-07-06)**: `SamplerStrategy` 接口 (作为独立 PDK 抽象 `include/agenticdsl/pdk/sampler_strategy.h`) **deferred 到 Phase 6+**。当前推理 Plugin 内部采样器 clamp 逻辑 (`temperature = std::clamp(temperature, 0.0f, 2.0f)` 等) 内联到 `inference/engine/generate` 工具实现内部, 不提取为独立接口。提取时机: 出现第二个推理后端 (e.g., vLLM/SGLang plugin) 时, 按 ADR-0034 "核心保留契约 + 算法 plugin 化" 范式重新评估。SamplerStrategy 相关测试用例 (`sampler_chain_compose` test #15) 保留在 §8 测试表中, 作为 Phase 6+ 启用时的覆盖基线。

| 分层 | 粒度 | 配置入口 | 示例参数 |
|------|:----:|---------|---------|
| **L1 - 静态/模型加载** | per-model-load | `inference/engine/init` | n_gpu_layers, split_mode, use_mmap, devices, tensor_split |
| **L2 - 静态/会话创建** | per-session | `inference/session/create` | n_ctx, n_batch, n_ubatch, flash_attn, type_k, type_v, offload_kqv, rope_scaling |
| **L3a - 动态/运行时** | any time (即时/next-request 生效) | `inference/configure` | n_threads, n_threads_batch, default_temperature, default_top_k, default_top_p, prefer |
| **L3b - 采样策略** (SamplerStrategy 接口 deferred 到 Phase 6+) | per-session (需重建 sampler chain) | `inference/sampler/configure` | sampler chain 类型组合 (greedy/temperature/top_k/mirostat/grammar) |
| **L4 - 每请求** | per-generate | `inference/generate` 入参 | temperature, top_k, top_p, min_p, seed, max_tokens, stop, grammar, penalties |

**L3a 与 L3b 的区别**:
- L3a = 采样**参数** (温度/top_k/top_p 等数值) — 即时或 next-request 生效
- L3b = 采样**策略** (chain composition, sampler 类型) — 需重建 sampler chain, 通常 per-session

**L3a 动态配置返回值**:
```json
{
  "applied": {"n_threads": true},
  "requires_restart": {"n_gpu_layers": false},
  "current": {"n_threads": 8}
}
```

详见 ADR-0038 (动态配置接口)。

### 5. 流式推理桥接

`inference/generate/stream` 内部架构:

```
inference/generate/stream tool call
  │
  ▼
Inference Plugin 内部: LlamaGenerationStream (implements IGenerationStream)
  │
  ├── 后台线程: llama_decode() loop + sampler
  │   ├── llama_decode(ctx, batch)  → logits
  │   ├── llama_sampler_sample(smpl, ctx, -1) → token
  │   ├── llama_token_to_piece(vocab, token, ...) → text chunk
  │   └── buffer.push(chunk); cv.notify_one()
  │
  └── 消费者线程: IGenerationStream::next(stop_token)
      ├── cv.wait() → buffer.pop() → return chunk
      └── stop_token.stop_requested() → llama_set_abort_callback(ctx, λ)
```

**关键桥接**:
- `std::stop_token` → `llama_set_abort_callback(ctx, callback)` 用于取消
- `next()` 阻塞等价于 `cv_.wait()` 等待后台 decode 线程
- chunk 大小: char 级 (每个 token 一次 next()) 或可配置聚合

#### 5.1 线程模型与隔离 (P0 fix @Oracle review)

**每个 `inference/generate/stream` 调用对应一个独立后台线程** (jthread RAII), 与 session 一一对应。Session 间**不共享** `llama_context*` 或 `sampler_chain*`。

| 并发场景 | 隔离机制 |
|---------|---------|
| **多个 session 并发 stream** | 每个 session 独立 jthread + 独立 llama_context (per-session lock) |
| **同一 session 多次 stream 调用** | 拒绝 (ToolResult::error(ErrorCode::ResourceExhausted)) |
| **跨 session generate + stream** | Session-level mutex 序列化 (per-session, 不影响其他 session) |
| **abort_callback 桥接** | `llama_set_abort_callback(ctx, callback, userdata)` 在 stream 创建时设置, callback 检查 stop_token |

**锁粒度**: per-session `std::mutex`, 防止同一 `llama_context*` 并发调用 (llama.cpp 不是线程安全的, 单 context 内)。跨 session 不互相阻塞。

**资源管理**: jthread RAII 自动 join, stream 析构时等待后台线程结束 (设置 1s 超时, 超时则 leak 并 emit `inference/error` 警告)。

### 6. 性能指标暴露 (Hybrid 模式, Oracle 推荐)

| 通道 | 承载内容 | 频率限制 |
|------|---------|:--------:|
| **EventBus** (`inference/lifecycle/*`) | 状态转换 (idle→running, model_loaded, context_overflow) | 每推理 1-2 次 |
| **ToolRegistry** (`inference/get/status`) | 当前快照 (t/s, KV cache%, GPU mem) | 消费者按需拉取 |

**不通过 EventBus 推送高频指标** (1000+ t/s 会触发 Sprint 12 bridge 背压)。

`inference/get/status` 返回 schema (atomic 快照):
```json
{
  "engine": {"uptime_seconds": 3600, "backend": "triton", "backend_version": "CUDA 12.4, SM 89"},
  "device": {"name": "NVIDIA RTX 4090", "total_mem_mb": 24576, "free_mem_mb": 16384},
  "models": [{"model_id": "qwen3-7b", "n_params": 7000000000, "n_ctx": 8192,
              "kv_cache_used_pct": 0.35, "avg_tg_tok_s": 45.2}],
  "sessions": [{"session_id": "ses_001", "model_id": "qwen3-7b", "state": "idle"}],
  "performance": {"avg_tg_tok_s": 45.2, "avg_pp_tok_s": 3200, "p50_latency_ms": 22, "p99_latency_ms": 45}
}
```

详见 ADR-0039 (性能元数据契约)。

### 7. llama.cpp 错误 → LLMError/ErrorCode 映射 (P1 fix @Oracle review)

| llama.cpp 场景 | LLMError::Code (ADR-0001) | ErrorCode (ADR-0023 P2) | retryable() |
|---|:---:|:---:|:---:|
| `llama_decode() < 0` (invalid input) | `InvalidRequest` | `Abort` | false |
| `ctx overflow` (n_tokens > n_ctx) | `ContextOverflow` | `ResourceExhausted` | false |
| GPU OOM | `Unknown` | `ResourceExhausted` | false |
| `llama_set_abort_callback` 触发 (用户取消) | `Cancelled` | `Abort` | false |
| IO/mmap 失败 (模型文件) | `ServerError` | `Retry` | true |
| Sampler 异常 (invalid params) | `InvalidRequest` | `ResourceExhausted` | false |
| llama_kv_cache 碎片 (defrag) | `ServerError` | `Skip` | false |
| `llama_tokenize` 失败 | `InvalidRequest` | `Abort` | false |
| 模型 corrupt / GGUF magic 不匹配 | `ServerError` | `ResourceExhausted` | true |

**双重映射**: 通过 `inference/*` tools 调用时, llm_error_to_error_code() (Sprint 18 D-4 已 ship) 在 ToolCoordinator 层完成。

---

### 8. 测试策略 (P1 fix @Oracle review)

参考 ADR-0022 5 ctest case 标准, Inference Plugin 测试列表:

| # | 测试名 | 覆盖 |
|---|--------|------|
| 1 | `plugin_pod_layout` | `pdk_plugin_info` POD 字节布局正确 |
| 2 | `plugin_abi_validate` | `pdk_create_llm_provider` ABI 兼容性 |
| 3 | `plugin_lifecycle_load_unload` | PluginLoader dlopen + dlclose (含 shared_ptr 析构顺序) |
| 4 | `plugin_register_tools` | 13 个工具全部注册到 MockToolRegistry |
| 5 | `plugin_get_llm_provider` | 工厂符号调用返回非 null shared_ptr |
| 6 | `engine_init_lifecycle` | UNINITIALIZED → INITIALIZED 状态机 + event emit |
| 7 | `model_load_list_unload` | GGUF 加载 + 列表 + 卸载 |
| 8 | `session_create_destroy` | session 生命周期 + per-session mutex |
| 9 | `generate_sync` | 同步生成返回 ToolResult envelope |
| 10 | `generate_stream_basic` | 流式生成 8 token 全 char 级 chunks |
| 11 | `generate_stream_cancel` | std::stop_token → llama_set_abort_callback 桥接 |
| 12 | `generate_stream_concurrent` | 多 session 并发 stream 隔离 |
| 13 | `configure_immediate_thread_change` | `n_threads` 即时生效 |
| 14 | `configure_restart_required_n_gpu_layers` | 需要重启的参数正确返回 |
| 15 | `sampler_chain_compose` | `inference/sampler/configure` 多种 chain 组合 |
| 16 | `get_status_atomic_snapshot` | 多字段一次性读, 无拼装错位 |
| 17 | `get_models_mirror_illmprovider` | `available_models()` 与 `inference/get/models` 数据一致 |
| 18 | `error_mapping_llama_to_envelope` | 9 种 llama.cpp 错误场景 → ToolResult.error_code |
| 19 | `config_layer_through_tool_coordinator` | 编排 Plugin 内部调用 `inference/configure` 走 ToolCoordinator |
| 20 | `plugin_dependency_order` | Inference Plugin 必须先于 Orchestration Plugin 加载 |
| 21 | `event_backpressure_safe` | 1000+ t/s 推理时不通过 EventBus 推 metrics (验证 hybrid 设计) |

**集成测试** (`tests/test_inference_orchestration_integration.cpp`):
- 编排 Plugin + 推理 Plugin 加载顺序与生命周期协同
- ILLMProvider 三层链 (DSLEngine → 编排 → 推理 → llama.cpp)
- generate_stream 并发 + abort 在多 session 场景的正确性

---

## 替代方案

### Option A: 纯 Tool 模式 (仅注册 tools, 不实现 ILLMProvider)

**被拒绝理由**: ReAct 循环中 LLM 生成 (agent "思考") 会错误地经过 ToolCoordinator approval → audit pipeline。语义误用 + 热路径性能损耗。

### Option B: 纯 ILLMProvider 模式 (不注册 tools)

**被拒绝理由**: DSL workflow 无法调用推理引擎 (tool_call 节点无目标), 编排 Plugin 失去工具层编排能力。

### Option C (采用): 双接口 (Tools + ILLMProvider) — 见 §1.1 三层消费链

---

## 风险

| 风险 | 缓解措施 |
|------|---------|
| **PluginLoader 析构顺序** | DSLEngine 用 `shared_ptr<ILLMProvider>` 持有 (per §1.2), 必须在 PluginLoader dlclose 前释放。参考 Sprint 17 C7 destruction order bug |
| **ILLMProvider 工厂符号 ABI** | `shared_ptr` 返回值跨 .so 边界需 ABI 兼容 (libc++/libstdc++ ABI 等)。可通过 CMake 强制 `target_compile_features cxx_std_20` + same STL 限制 |
| **两个接口的状态一致性** | per-session mutex (§5.1) 防止同一 `llama_context*` 并发调用; generate 与 ILLMProvider 直连共享同一资源 |
| **`inference/model/switch` 拒绝行为破坏 workflow** | 通过 `tool.audit.denied` event 通知编排 Plugin, agent 重试或换模型 |
| **插件版本不匹配** | PluginInfo.abi_version 仅管 Plugin↔Runtime, Plugin↔Plugin 间通过 PluginInfo.capabilities 字符串协议 (e.g., "inference.engine.v1") 检查 |

---

*创建日期*: 2026-07-06
*Oracle 审查*: ses_0ca3dce4fffeck5vmAQMs6R94m (三关键问题建议)
*依赖*: ADR-0046 (Plugin Communication Protocol), ADR-0038 (Dynamic Config Interface)
