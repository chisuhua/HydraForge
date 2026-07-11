# ADR-0044: 推理引擎 Plugin 安全模型

## 状态

✅ Approved (2026-07-10 — OpenSpec change `phase5-llama-engine-plugin` (C14) ship, 三层安全模型应用于推理插件); **2026-07-06 P1 fix**: `inference/engine/init` Category 修正为 `StateModify`, 内部 vs 外部 approval 豁免明确化, ModelCapability threat model 补充; **2026-07-06 renumber**: 兄弟 ADR-0036 → ADR-0045 (编排 plugin), ADR-0037 → ADR-0046 (通信协议), 避免与旧 ADR-0036-三层服务协议 / ADR-0037-因果序冲突

> **实施依据**: `phase5-llama-engine-plugin` (C14) 已 ship + archived (2026-07-08), 验证: 三层安全模型完整应用于 12 个推理插件工具 — L1 路径白名单 (per ADR-0022 §2.1, `PluginLoader` 拒绝非白名单路径) + L2 ToolMetadata V2 (per ADR-0004 V2, `category` + `min_layer` + `approval_policy` 字段, 详见 `tests/test_llama_engine_plugin.cpp` 10 TC metadata 审批策略验证) + L3 ToolCoordinator (per ADR-0031 §决策 5, 层检查 → 审批 → 审计 pipeline)。`inference/engine/init` Category 经 P1 fix 修正为 `StateModify`。详见 `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §三 C14 行 + `tests/test_llama_engine_plugin.cpp` + `openspec/changes/archive/2026-07-08-phase5-llama-engine-plugin/`。

## 领域

基座 / Security / PDK Plugin

## 关联

- [ADR-0035 (Inference Engine Plugin Spec)](./adr-0035-inference-engine-plugin-spec.md) — 工具集定义
- [ADR-0045 (Orchestration Plugin Spec)](./adr-0045-orchestration-plugin-spec.md) — §6 内部调用豁免 (P1 fix 对齐)
- ADR-0022 (Plugin Loading) — 路径白名单
- ADR-0004 (ToolRegistry Security) — ToolCategory, ApprovalPolicy, LayerProfile
- ADR-0031 (Execution Policy) — IExecutionPolicy, ToolCoordinator
- ADR-0046 (Plugin Communication) — §2.1 subscribe_topic layer check

---

## 背景

推理引擎 Plugin 拥有模型文件访问权, GPU/CPU 计算资源, 以及采样/生成能力。需要定义安全边界。

---

## 决策

### 1. 三层安全边界

| 层 | 机制 | 保护 |
|------|------|------|
| **L1 - 加载时** | PluginLoader 路径白名单 (ADR-0022 §2.1) | 防止加载非白名单路径的 .so |
| **L2 - 工具注册时** | ToolMetadata.category + min_layer (ADR-0004) | 限制哪些 Layer 可调用哪些工具 |
| **L3 - 工具执行时** | ToolCoordinator (ADR-0031) | Layer check → Approval → Audit |

### 2. 工具安全分类 (P1 fix: `inference/engine/init` Category 修正)

| 工具 | Category | Approval (外部调用) | Approval (内部调用, ADR-0045 §6) | Min Layer | 说明 |
|------|:--------:|:--------------------:|:--------------------------------:|:---------:|------|
| `inference/engine/init` | **StateModify** ⬅️ P1 fix | agent | **豁免** (编排 Plugin 初始化时自动调) | Workflow | 分配 GPU / 加载 backend, 状态 UNINITIALIZED → INITIALIZED, **非 ReadOnly** |
| `inference/engine/status` | ReadOnly | none | none | (已删除) | 合并到 `inference/get/status` (per ADR-0035 P0 fix) |
| `inference/generate` | Execute (按 analogy, 见注) | agent | **豁免** (编排 ReAct 内层) | Workflow | 资源消费 + 状态副作用 |
| `inference/generate/stream` | Execute | agent | 豁免 | Workflow | 同上 (流式) |
| `inference/model/load` | StateModify | plan | **豁免** (编排 Plugin 自动化模型管理) | Workflow | 加载 GGUF, 内存分配 |
| `inference/model/unload` | StateModify | plan | **豁免** | Workflow | 释放模型 |
| `inference/model/list` | ReadOnly | none | none | Thinking | 查询 |
| `inference/model/switch` | StateModify | plan | 豁免 | Workflow | 切换活跃模型 |
| `inference/session/create` | StateModify | agent | 豁免 | Workflow | 创建 session |
| `inference/session/destroy` | StateModify | agent | 豁免 | Workflow | 销毁 session |
| `inference/configure` (L3a) | StateModify | plan | **豁免** (编排事件驱动动态调节, per ADR-0045 §6) | Workflow | 改 n_threads/prefer 等 |
| `inference/sampler/configure` (L3b) | StateModify | agent | 豁免 | Workflow | 改 sampler chain topology |
| `inference/get/status` | ReadOnly | none | none | Thinking | 性能快照 (无副作用) |
| `inference/get/models` | ReadOnly | none | none | Thinking | 模型列表 (无副作用) |
| C13 schemas (`inference/{component}/configure`) | StateModify | agent | (组件级 audit) | Workflow | prefix_cache/kv_cache/decoding/cloud_engine 配置 |

**P1 fix: Category 修正**: `inference/engine/init` 之前误标 ReadOnly, 实际为状态修改操作 (初始化 engine, 分配 GPU 资源)。ReadOnly 语义为 "ls/cat/grep 类查询" (ADR-0004 §6 V2), init 不符合。同步修正 ADR-0035 §2。

**P1 fix: External vs Internal approval** (引用 [ADR-0045 §6](./adr-0045-orchestration-plugin-spec.md)):

| 调用方 | approval 适用性 |
|-------|----------------|
| DSL workflow → 编排 ToolCoordinator | 走 ADR-0004 ApprovalPolicy |
| 编排 Plugin 内部 → `inference/*` via `internal_registry_` | **完全豁免** (per ADR-0045 §6) |
| 编排 Plugin 内部 → `inference/*` via `external_registry_` | 走 approval (e.g. 调试/诊断场景) |

注: `inference/generate` Category=Execute 是按 analogy (非 shell 执行, 而是资源消费+状态副作用), ADR-0004 §6 Execute 定义后续扩展以覆盖 "推理操作"。

### 3. ILLMProvider vs Tool 信任边界

| 接口 | 走 ToolCoordinator? | 原因 | Layer 限制 |
|------|:---:|------|-----------|
| **ToolRegistry** tools (DSL → `inference/*`) | ✅ (外部) | Agent "行动" = audited action | per min_layer |
| **ToolRegistry** tools (编排 Plugin 内部) | ❌ (ADR-0045 §6) | 编排内部 reasoning 链 | per min_layer (基础) + emit orchestration.audit.internal.* |
| **ILLMProvider** direct calls | ❌ | Agent "思考" = 内部推理 (不经审批) | Thinking/Workflow 层可用 (per ADR-0044 §补充); Cognitive 层须经编排 ILLMProvider wrapper |

**P1 fix: ILLMProvider layer 限制**:
- **Thinking 层 (AgentLoop / 高级 Orchestrator)**: 可直接调用 ILLMProvider
- **Workflow 层 (简单 Task)**: 可直接调用 ILLMProvider
- **Cognitive 层 (用户意图探索)**: 必须经编排 ILLMProvider wrapper, 禁止直接调推理 Plugin ILLMProvider
- **理由**: Cognitive 层需要模式感知 + 路由决策 + 审计, 这些是编排 Plugin 的职责

编排 Plugin 如需对 ILLMProvider 调用也走审计 (合规场景), 可装饰 `AuditingLLMProvider` wrapper (per ADR-0045 §6.4) — 而非把审计塞进 ToolRegistry 热路径。

### 4. IInteractionBus subscribe_topic Layer 检查 (P1 fix per ADR-0046 §2.1)

per [ADR-0046 §2.1](./adr-0046-plugin-communication-protocol.md):

| 订阅方 layer | 可订阅 topic | 说明 |
|------------|------------|------|
| **Workflow 层** | 所有公开 topic (`inference.lifecycle.*`, `inference.model.*`, `inference.error.*`) | 编排 Plugin 默认 |
| **Thinking 层** | `inference/lifecycle/*`, `inference/get/*` (Query Tool 替代) | 监控 + 决策 |
| **Cognitive 层** | 无 (不订阅 reasoning engine 内部) | 经编排 wrapper |

**subscribe_topic 审计**: 所有 subscribe 调用记录 `audit.subscribe.{layer}.{topic}`, 用于事后追溯哪些 agent 订阅了哪些事件。

### 5. 资源隔离 (P1 fix 深化)

| 资源 | 隔离机制 | 配置入口 |
|------|---------|---------|
| **模型文件** | 路径白名单 ( `$HYDRAFORGE_MODEL_PATH`, `./models/`, `~/.hydraforge/models/` ) + 仅插件拥有者可配置 `model_path` 参数 | `inference/engine/init` |
| **GPU 内存** | MVP 单 inference plugin 场景由 llama.cpp 后端管理 (单 plugin 独占 GPU pool) | 无 (plugin 内部) |
| **CPU 线程** | llama.cpp threadpool 管理 + `inference/configure` `n_threads` 限流 | `inference/configure` |
| **多 inference plugin** | **MVP 不保证跨 plugin GPU/CPU 隔离**; defer 至 ADR-0004 Phase 2 容器隔离 | 无 (Phase 2+) |
| **Plugin 销毁** | PluginLoader dlclose 前须释放所有 shared_ptr<ILLMProvider> 与 ToolRegistry lambda (per Sprint 17 C7 destruction order bug) | PluginLoader 卸载序列 |

**威胁模型** (P1 fix per Oracle review):

| 威胁 | 缓解 |
|------|------|
| **恶意 plugin .so** | L1 路径白名单 + `current_abi_version` 校验 |
| **恶意 model 文件** | Model 路径白名单 + (Phase 2+) GGUF magic + SHA256 hash 校验 |
| **Prompt injection 触发危险 tool call** | ToolCoordinator Approval (外部调用) + ToolCategory/Layer 检查 (内部豁免) |
| **GPU/CPU 资源耗尽** | llama.cpp threadpool + n_threads 上限 + per-session mutex (防止 ABA) |
| **跨 plugin 资源争抢** | MVP 假定单 inference plugin; 部署多 plugin 时需手动隔离 (Phase 2 容器隔离) |
| **ILLMProvider 滥用** (无审计) | 编排 Plugin ILLMProvider wrapper 是唯一 DSL 入口 (三层消费链 per ADR-0035 §1.1) |
| **Event flood** | IInteractionBus MPMC 有界队列 + 编排 Plugin 不订阅高频 metric topic |
| **Subscription layer confusion** | subscribe_topic layer check (per §4 上表) |

### 6. 安全测试策略 (P1 fix per Oracle review)

| # | 测试 | 验证 |
|---|------|------|
| 1 | `plugin_loader_path_whitelist` | 黑名单路径 (e.g. `/etc/foo.so`) 被拒绝 |
| 2 | `plugin_abi_mismatch_rejected` | `abi_version` 不匹配严格拒绝 |
| 3 | `tool_layer_enforcement` | Cognitive 层调 `inference/model/load` 被拒绝 (min_layer=Workflow) |
| 4 | `tool_external_approval_required` | Plan 模式下外部调 `inference/generate` 需 user `/apply` |
| 5 | `tool_internal_approval_bypass` | 编排 Plugin 内部调 `inference/configure` 不触发 approval, 但 emit orchestration.audit.internal.* |
| 6 | `subscribe_topic_layer_enforcement` | Cognitive 层 `subscribe_topic("inference.lifecycle.*")` 被拒绝 |
| 7 | `model_path_validation` | 路径白名单外 model_path 被拒绝 |
| 8 | `destruction_order_no_segfault` | PluginLoader 卸载 .so 前先释放 shared_ptr<ILLMProvider> |
| 9 | `auditing_illmprovider_wrapper` | `AuditingILLMProvider` wrapper 拦截并记录所有 call |

---

*创建日期*: 2026-07-06
*修订*: 2026-07-06 (P1 fix 应用: `engine/init` Category→StateModify, 外部/内部 approval 豁免表, ILLMProvider layer 限制, 威胁模型, 安全测试列表)
*依赖*: ADR-0035 (§2 工具集), ADR-0045 (§6 内部豁免), ADR-0046 (§2.1 subscribe_topic layer), ADR-0022 (§2.1 路径白名单), ADR-0004 (V2 ToolCategory), ADR-0031 (ToolCoordinator)
