# 参考报告 1: B2 三个 OpenSpec Change 架构边界深度审查

> **来源**: explore agent (bg_0a74e604, 2m36s)
> **关联 session**: `ses_0cb1027ccffeN7BmCaOQTpQl1Y`
> **审查范围**: `phase5-b2-arch-schemas` (C13) + `phase5-llama-engine-plugin` (C14) + `phase5-batching-queue-plugin` (C15)
> **日期**: 2026-07-06

---

## 1. 三个 change 架构边界分析

### 1.1 边界划分

基于 Oracle 架构反思 (`ses_0ce717ac4ffejvLa2We0gzbuds`) 的三层模型:

| 类型 | 归属 Change | 合理性 |
|------|------------|--------|
| 配置参数 (prefix_cache/kv_cache/decoding 标准部分) | **架构层 schema** → C13 | ✅ 合理 |
| 可插拔实现 (engine/model/batching) | **PDK plugin** → C14/C15 | ✅ 合理 |
| 高级采样算法 (mirostat_v2/typical_p) | 架构层 + 未来 hook → C13 声明接口, C14 出 reference | ⚠️ 过早抽象 |

### 1.2 7 个 B2 子图的归属

| B2 # | 子图 | Change | 当前状态 | 合理性 |
|------|------|--------|---------|--------|
| B2.0 | session.md | 已有 (C12 前) | ✅ ship, 107 行 | ✅ |
| B2.1 | engine.md | C14 | 占位 102 行 | ✅ |
| B2.2 | model.md | C14 | 占位 109 行 | ✅ |
| B2.3 | prefix_cache.md | C13 | ❌ 不存在 | ✅ |
| B2.4 | kv_cache.md | C13 | ❌ 不存在 | ✅ |
| B2.5 | decoding.md | C13 + SamplerStrategy hook | ❌ 不存在 | ⚠️ 见 §1.3 |
| B2.6 | batching.md | C15 | ❌ 不存在 | ❌ 见 §1.4 |
| B2.7 | cloud_engine.md | C13 | ❌ 不存在 | ✅ Stage 2 占位 |

### 1.3 SamplerStrategy 接口过早抽象

**结论**: 是的，过早抽象。证据:

- SamplerStrategy 在 C13 proposal 第 90-110 行声明，但唯一实现是 C14 的 LlamaSampler (proposal 第 137-167 行)
- LlamaSampler 的 `supports()` 对全部 5 种采样器**全部返回 true** (C14 第 142-148 行)
- `warning_if_unsupported()` 始终返回空字符串 (C14 第 151 行)
- `normalize()` 仅做 clamp (C14 第 155-160 行)
- **三个虚函数零多样性需求**

与 model_router 对比: IModelRouter 有 3 个策略实现 (cost/quality/latency), `route()` 逻辑完全不同。SamplerStrategy 的 3 个虚函数在仅有一个实现时等于白噪声。

### 1.4 BatchingQueue 过早抽象

**结论**: C15 的 BatchingQueue 抽象过度。证据:

- LlamaBatchingQueue 的 `cancel()` 始终返回 false (C15 proposal 第 117-119 行)
- `supports_batching` 元数据始终为 false (C15 第 182-189 行)
- `max_concurrent` 始终为 1
- 5 个虚方法全部只有一个实现，且该实现声明自己不做真正 batching
- 提案自己承认 (C15 第 16 行): "llama.cpp 无 batching"

### 1.5 工具/policy/sampler 嵌入方式

| Change | 选定的嵌入方式 | 评价 |
|--------|--------------|------|
| C13 prefix_cache | schema-only .md → `prefix_cache.configure` 委托给 engine plugin | ✅ |
| C13 kv_cache | schema-only .md → `kv_cache.configure` | ✅ |
| C13 decoding | schema .md + SamplerStrategy 接口声明 | ❌ 过早抽象 |
| C14 engine/model | 8 个工具注册到 IToolRegistry | ✅ 复用 model_router 模式 |
| C15 BatchingQueue | 5 虚方法接口 + `extern "C" BatchingQueue* create_batching_queue()` | ❌ 零市场先例 |
| C15 contribution flow | PR 模板 + ADR 模板 + semver ABI policy | ❌ 零贡献者时过度 |

---

## 2. 依赖链正确性

### 2.1 C13 → C14 依赖

**Claimed**: C14 依赖于 C13 (C13 tasks.md 第 105 行)
**Reality**: 依赖很弱。C13 核心产出 (4 个 .md schema) 与 C14 的 plugin 工具注册完全正交。唯一真正依赖是 `include/agenticdsl/pdk/sampler_strategy.h` 接口声明 — C14 需该接口实现 LlamaSampler。

**消除方法**: 如果 C13 不声明 SamplerStrategy 接口 (改为 C14 在 `llama_sampler.cpp` 内部实现), C13 和 C14 完全可并行。

### 2.2 C14 → C15 依赖

**Claimed**: C15 依赖于 C14 (C15 tasks.md 第 8 行)
**Reality**: 依赖较强。C15 的 LlamaBatchingQueue 实现位于 `pdk/llama_engine/src/llama_batching.cpp` (C15 proposal 第 79-150 行), 复用 C14 的 `llama_engine_` 方法。

**拆分建议**: 可将 C15 拆为 C15a (BatchingQueue 接口 + 贡献流程, 0 依赖) 和 C15b (LlamaBatchingQueue impl, 依赖 C14)。

### 2.3 可并行化的部分

| 内容 | 可并行化 | 当前阻塞 |
|------|---------|---------|
| C13 4 个 .md schema | ✅ 完全独立 | 不阻塞 |
| C13 SamplerStrategy 接口 | ✅ 可延迟到 C14 | 唯一依赖点 |
| C14 plugin 骨架 + engine/model 实现 | ✅ 独立 | 无 |
| C15 贡献流程文档 | ✅ 完全独立 | 无 |
| C15 BatchingQueue 接口 | ✅ 完全独立 | 无 |
| C15 LlamaBatchingQueue impl | ❌ 强依赖 C14 | C14 的 llama_engine.cpp |
| C15 lib/inference/batching.md | ✅ 可独立创建 | 无 |

---

## 3. Adversarial Review Top 5 风险

### P1: SamplerStrategy 接口无多样性需求

- **风险**: C13 声明 3 虚函数, 但唯一实现中 `supports()` 永远 true, `warning()` 永远空
- **影响**: PDK 接口膨胀 25% (8→10 头文件), 无实际收益
- **证据**: C13 proposal 第 92-109 行; C14 proposal 第 142-148/151 行

### P1: LlamaBatchingQueue 声明"不支持 batching"

- **风险**: 5 方法抽象的唯一实现声明 `supports_batching=false`, 整个抽象层的意义在于等待"vLLM/SGLang 贡献者"——而零贡献者
- **影响**: 发布"声明自己不能用"的接口, 损害 PDK 声誉
- **证据**: C15 proposal 第 117-119/182-189 行; C15 spec 第 57-60 行

### P2: DSLEngine 默认注入 + fallback

- **风险**: 每次 DSLEngine 构造都尝试 dlopen (即使不需要推理的场景); 双路径 (成功/失败) 需测试覆盖; 现有 64 测试全部走 fallback, 注入逻辑未被测试覆盖
- **影响**: 30 行的构造逻辑几乎不被真实测试覆盖, 成为"信任债务"
- **证据**: C14 proposal 第 206-220 行; C14 tasks 第 88-95 行

### P2: 贡献流程文档为零贡献者设计

- **风险**: 520 行新增中 340 行(65%)是第三方贡献流程文档。项目当前 0 第三方贡献者、0 第三方 plugin
- **影响**: 6 月内几乎不会被使用, 届时时需重新 review
- **证据**: C15 proposal 第 196-257/289-305 行; C15 spec 第 113-130 行

### P2: engine.md + model.md 占位文件与 C14 升级方案矛盾

- **风险**: 现有占位文件使用 `inference.engine_init`, C14 改用 `llama_engine/init`。工具名从点号改为斜杠, 命名空间双轴变更
- **影响**: 不是零 breaking change
- **证据**: `lib/inference/engine.md` 第 13 行; C14 proposal 第 186/205 行

---

## 4. 替代架构方案

### 方案 A (维持现状)
- 内容: 按当前 proposal, 依次 ship C13 → C14 → C15
- 工作量: 2-3.5 天
- 技术债增量: 高 (SamplerStrategy + BatchingQueue + namespace break)

### 方案 B (合并 + 裁剪) ⭐
- 内容: C13 ship 4 schema(无 SamplerStrategy) + C14 ship engine/model(无 DSLEngine 默认注入, 统一 `inference.*`) + C15 仅 batching.md schema
- 工作量: 1.5-2 天
- 技术债增量: 极少

### 方案 C (推迟)
- 内容: 只做 C13 4 个 .md schema, C14/C15 暂不实施
- 工作量: 0.5 天
- 技术债增量: 无

### 方案 D (B2 重新拆分)
- 内容: B2-schema (.md 所有) + B2-impl (engine/model 内联实现) 两层
- 工作量: 1.5-2.5 天
- 技术债增量: 未来提取接口时需重构, 但接口会基于真实需求