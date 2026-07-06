# Adversarial Review 综合报告 — Phase 5 B2 (C13/C14/C15)

> **审查日期**: 2026-07-06
> **审查者**: 4 个并行调研 (3 explore + 1 librarian)
> **总审查时长**: ~10 分钟
> **核心发现**: **3 个 B2 changes 包含 2 处明确过早抽象 + 1 处隐藏 breaking change**
> **关联文件集入口**: `README.md` (本文目录)

---

## 1. 架构审查结论概要

### 1.1 三 Change 整体评级

| Change | Proposal 声称 | 审查后评级 | 最终建议 |
|--------|-------------|:----------:|---------|
| **C13** `phase5-b2-arch-schemas` | 4 个 .md schema + SamplerStrategy 头文件 | ⚠️ **需精简** | 保留 4 个 schema, **删除 SamplerStrategy 接口**(过早抽象) |
| **C14** `phase5-llama-engine-plugin` | engine/model 实现 + SamplerStrategy reference | ⚠️ **需调整** | 保留,但工具名统一为 `inference.*`,删除 DSLEngine 默认注入 |
| **C15** `phase5-batching-queue-plugin` | BatchingQueue 接口 + 贡献流程 | ❌ **需推迟** | 仅创建 batching.md schema(40 行), 推迟 BatchingQueue |

### 1.2 方案对比

| 方案 | 内容 | 工作量 | 技术债增量 | 何时选择 |
|------|------|:-----:|:----------:|---------|
| **A 维持现状** | 三 change 逐 ship | 2-3.5 天 | **高** (SamplerStrategy 无多样性 + BatchingQueue 0 实现支持 + 工具命名空间 break) | "完整性"追求 |
| **B 合并 + 裁剪** ⭐ | C13 4 schema (无 SamplerStrategy) + C14 engine/model (无注入, 统一 `inference.*`) + C15 仅 schema | **1.5-2 天** | 低 | **推荐** |
| **C 推迟** | C13 仅 schema, C14/C15 暂不启动 | 0.5 天 | 无 | "最小可行 + 需求驱动" |
| **D B2 重新拆分** | B2-schema(.md) + B2-impl(内联实现) 两层 | 1.5-2.5 天 | 未来重构代价 | "先做对、再抽象" |

---

## 2. 五大 Adversarial 发现 (按严重度)

### 🔴 P1: BatchingQueue 没有市场先例

| 维度 | 发现 |
|------|------|
| **结论** | **C15 BatchingQueue 5 方法接口不应该 ship** |
| **证据** | 7 个主流推理项目 (vLLM/SGLang/llama.cpp/TRT-LLM/TGI/LMDeploy/lit-gpt) — **零项目有独立 BatchingQueue 接口** |
| **共识** | 主流设计将 batching 完全内嵌到 Scheduler 而非暴露抽象接口。LlamaBatchingQueue 自己也声明 `supports_batching=false`、`cancel()` 始终 false、`max_concurrent=1` |
| **参考文件** | `ref-4-external-llm-comparison.md` §4 BatchingQueue 实现可行性 |
| **建议替代** | 参照 TRT-LLM SchedulerPolicy 抽象 (单 `schedule()` 方法),或直接内联到 Engine |

### 🔴 P1: SamplerStrategy 是无多样性需求接口

| 维度 | 发现 |
|------|------|
| **结论** | **C13 的 SamplerStrategy 接口应该删除或降级** |
| **证据** | C13 proposal 第 92-109 行: 3 虚函数。C14 LlamaSampler: `supports()` 对全部 5 种采样器返回 true, `warning_if_unsupported()` 始终空字符串, `normalize()` 仅 clamp |
| **市场验证** | vLLM V1 **明确删除**了早期 logits processor 抽象 (改用全局 processor) — 证明"先添加后删除"是行业教训 |
| **建议替代** | decoding.md 的 sampler 字段保持 5 种字符串选择 (`temperature|greedy|mirostat_v1|mirostat_v2|typical_p`), 等真实第二个实现出现时再提取接口 |

### 🟡 P2: 工具命名空间变更不是 zero breaking change

| 维度 | 发现 |
|------|------|
| **结论** | C14 proposal 声称 "API 兼容性: 零 breaking change",**但工具名实际改变了** |
| **证据** | 现有 `lib/inference/engine.md` 占位文件已定义 `tool: inference.engine_init`。C14 proposal 第 186 行改为 `tool: llama_engine/init`。命名空间从 `inference.` 点号 → `llama_engine/` 斜杠 **双轴变更** |
| **影响** | 任何依赖占位文件的工作流 (init_engine → model_load → 等) 在 C14 ship 时断裂 |
| **建议** | C14 应统一为 `inference.*` 命名空间 (保持占位文件兼容性) |

### 🟡 P2: 起点假设错了 — 不是 7/7 ship, 而是 1/7

| 维度 | 发现 |
|------|------|
| **结论** | B2 不是"扩展", 而是从 1/7 开始填充到 7/7 |
| **证据** | `lib/inference/` 目录: `session.md` (✅ 107 行) + `engine.md` (占位 101 行) + `model.md` (占位 108 行)。prefix_cache/kv_cache/decoding/batching/cloud_engine 全部缺失 |
| **后果** | master plan 多处声称"3/7 ship" / "7/7 子图覆盖率"已被 Oracle 修正, 但 B2 实施仍未启动 |
| **建议** | C13 估时 0.5-1 天实际约 2-3h(纯 .md),C14 需考虑 llama.cpp 原生集成缺失 |

### 🟡 P2: C7 Model Router 范式有 3 处不可直接复用

| 维度 | 发现 |
|------|------|
| **差异 1 (状态模型)** | C7 IModelRouter 是 stateless 纯函数 (2 虚方法); B2 engine 需要 long-lived ModelHandle, batching 需要并发队列 |
| **差异 2 (依赖链接)** | C7 plugin 只链接 `hydraforge_pdk` (header-only); B2 engine 需 `target_link_libraries(... PRIVATE llama)` |
| **差异 3 (生命周期)** | C7 plugin 加载一次注册即可; B2 engine 需要 `load → generate → unload` 钩子 |

---

## 3. 三 Change 重新评估详情

### C13: phase5-b2-arch-schemas (32 tasks → 精简后 ~28 tasks)

| 当前状态 | 审查后建议 |
|---------|---------|
| 4 个 .md schema (prefix_cache/kv_cache/decoding/cloud_engine) | ✅ 保留, 可立即 ship (纯 .md, 零 C++) |
| SamplerStrategy 接口声明 (3 虚函数, 60 行头文件) | ❌ **删除** — 单一实现且 `supports()` 永远 true |
| Doxygen + 文档同步 | ✅ 保留 |
| 验证 + Git + archive | ✅ 保留 |

### C14: phase5-llama-engine-plugin (51 tasks → 精简后 ~45 tasks)

| 当前状态 | 审查后建议 |
|---------|---------|
| `pdk/llama_engine/` plugin 骨架 (复用 C7 范式) | ✅ 保留 |
| 8 个工具注册 (engine_init/generate/stream 等) | ✅ 保留, 但命名空间 **统一为 `inference.*`** |
| SamplerStrategy reference impl (LlamaSampler) | ✅ 保留, 但作为 `llama_sampler.cpp` 内联实现, 不暴露 PDK 接口 |
| DSLEngine 默认 plugin 注入 (dlopen + fallback) | ❌ **删除** — 改用显式 `DSLEngine::load_plugin(name)` 方法 |
| 测试 (8 个 TEST_CASE) | ✅ 保留 |

### C15: phase5-batching-queue-plugin (46 tasks → 精简后 ~5 tasks)

| 当前状态 | 审查后建议 |
|---------|---------|
| BatchingQueue 5 方法接口 (submit/flush/cancel/wait_id/size) | ❌ **推迟** — 0 项目先例, 5 方法中 3 个在单用户场景下退化 |
| LlamaBatchingQueue reference impl (FIFO fallback) | ❌ **推迟** — `cancel()` 始终 false, `supports_batching=false` 的接口是矛盾 |
| `lib/inference/batching.md` schema (40 行) | ✅ **保留**, 仅创建 schema (纯 .md) |
| 贡献流程 (PR 模板 + ADR 模板 + semver ABI policy, ~340 行) | ❌ **推迟** — 0 第三方贡献者时写贡献流程是空文档 |
| 测试 (3 个 TEST_CASE) | ❌ 相应推迟 |

---

## 4. 外部参考对比 (关键发现)

### SamplerStrategy 有市场先例

| 来源 | 设计 | 可比性 |
|------|------|--------|
| **llama.cpp** `llama_sampler_i` (C 虚表, 4→8 方法) | chain 模式 + GPU offload 扩展 | ✅ **高度可比** — 2 年成功演进 |
| **TRT-LLM** `Sampler` 独立类 | C++ 绑定 + Python 可定制 | ✅ 中度可比 |
| **vLLM** `SamplingParams` (140 字段 dataclass) | 无虚接口, 参数驱动, V1 删除了 logits processor | ❌ 低度可比 |

**结论**: SamplerStrategy 有参考价值, 但当前阶段 (单一实现) 不需要接口化。llama.cpp 是在有 5+ 种 sampler 实现后才提取接口的。

### BatchingQueue 零先例

| 来源 | Batching 设计 | 有无独立接口 |
|------|-------------|:----------:|
| vLLM | 内嵌 Scheduler::schedule() | ❌ |
| SGLang | 内嵌 Scheduler::get_next_batch_to_run() | ❌ |
| llama.cpp | 无 batching (单序列) | ❌ |
| TRT-LLM | 内嵌 CapacityScheduler / MicroBatchScheduler | ❌ |
| TGI | Rust router 内建 | ❌ |
| LMDeploy | PersistentBatch 内嵌 | ❌ |

**结论**: **零项目有独立 BatchingQueue 抽象接口**。建议参照 TRT-LLM 的单方法 `SchedulerPolicy::schedule()` 抽象, 或直接内联到 Engine。

---

## 5. 推荐路径

### ⭐ 方案 B (合并 + 裁剪)

**理由**:
1. 节省 0.5-1.5 天 vs 方案 A (避免 BatchingQueue + SamplerStrategy 的过度工程)
2. 避免 3 个抽象死接口 (SamplerStrategy/BatchingQueue/BatchingQueue 工厂)
3. 零 breaking change (统一 `inference.*` 命名空间保持与占位文件一致)
4. 保持扩展性 (未来真实需求出现时再提取接口)

**执行步骤**:
1. 修正 C13 tasks.md: 删除 SamplerStrategy 任务, 保留 4 个 .md schema + 文档同步
2. 更新 C14 proposal: 工具名统一为 `inference.*`, 删除 DSLEngine 默认注入
3. 重写 C15 tasks.md: 仅创建 `lib/inference/batching.md` schema (40 行)
4. 贡献流程文档全部移除

---

## 6. 决策点 (供新 Session 快速启动)

| # | 决策 | 选项 A | 选项 B | 选项 C |
|---|------|--------|--------|--------|
| D1 | SamplerStrategy 接口 | 删除 | 保留 | — |
| D2 | BatchingQueue | 推迟 | 保留 | — |
| D3 | C14 工具命名空间 | `inference.*` | `llama_engine/` | — |
| D4 | 优先级排序 | 先 B2 | 先 TSan | 并行 |