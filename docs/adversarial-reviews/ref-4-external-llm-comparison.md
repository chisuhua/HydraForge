# 参考报告 4: 7 个主流推理引擎 Plugin 抽象对比调研

> **来源**: librarian agent (bg_ee40d227, 2m34s)
> **关联 session**: `ses_0cb0f8694ffec5qaKlCnGlh7r0`
> **调查范围**: vLLM / SGLang / llama.cpp / TensorRT-LLM / TGI / LMDeploy / lit-gpt
> **日期**: 2026-07-06

---

## 1. 主流推理后端抽象模式对比

| 项目 | 后端抽象层 | Sampler 接口 | Batching 内置 | Plugin 系统 | 抽象评分 |
|------|-----------|-------------|:------------:|-----------|:--------:|
| **vLLM** | `LLMEngine`/`AsyncLLMEngine` + `ExecutorBase` 多进程 | `SamplingParams` 扁平 dataclass, 无虚接口| 内置 Continuous Batching | Python `entry_points` | ⭐⭐⭐⭐⭐ |
| **SGLang** | `Engine`→`Scheduler`→`ModelRunner` 4 进程 ZMQ | `SamplingBatchInfo` 批量类, 无独立接口 | 内置 RadixAttention | 无暴露的 plugin 系统 | ⭐⭐⭐⭐ |
| **llama.cpp** | 单进程, 无后端抽象 (switch-case 40+ 模型) | `llama_sampler_i` C 虚表 + chain | 无 (单序列) | 无 plugin 系统 | ⭐⭐⭐ |
| **TRT-LLM** | 4 层 (PyExecutor/Scheduler/ModelEngine/Decoder) + 3 后端 | `Sampler` 独立类 | 内置 Overlap Scheduler | SchedulerPolicy 可自定义 | ⭐⭐⭐⭐ |
| **TGI** | Rust Router + Python Server + 4 后端 | 无独立接口 | 内置 (Rust) | Multi-backend gRPC | ⭐⭐⭐ |
| **LMDeploy** | TurboMind (C++) + PyTorch 双路线 | 无独立 (重构中移除) | Persistent Batch | 无 | ⭐⭐ |
| **lit-gpt** | "No abstractions" 设计哲学 | 无 | 无 | 无 | ⭐ |

---

## 2. vLLM 架构深度解析 (重点参考)

### 2.1 V0 → V1 架构重构 (2025-01)

关键演进 (vLLM blog, 2025-01-27):
- V0: `LLMEngine` + `AsyncLLMEngine` 同进程, GIL 竞争严重
- V0.6.0: 分离 API server 和 inference engine 到两个进程 (ZMQ 通信), 2.7x throughput
- V1: 重写 scheduler/KV cache/worker/sampler/API server 五层, 保留 models/GPU kernels

**与 HydraForge 的关联**: 架构重构是正常的、健康的演进, 不必一次到位。

### 2.2 vLLM V1 移除的特性 (抽象失败的证据)

| 移除的特性 | 原因 | 影响 |
|-----------|------|:----:|
| `best_of` sampling (RFC #13361) | 使用量低, 架构复杂度高 | 🔴 移除 |
| Per-Request Logits Processors (RFC #13360) | V1 架构简化 | 🔴 移除 |
| GPU↔CPU KV Cache Swapping | 前缀缓存使 swapping 不再需要 | 🔴 移除 |
| Virtual Engine (PR #37195) | V1/V2 始终为 0 | 🔴 废弃 |

**模式**: 每个被移除的抽象都是"先添加, 后发现不需要, 再移除"的路径。这验证了 Sandi Metz 的原则: **抽象应该从现实需求中提取, 而非为假设的需求设计。**

### 2.3 SamplingParams: 扁平 dataclass 不是虚接口

vLLM 使用 **140+ 字段的扁平 Python dataclass**, 没有虚接口、没有继承:

```python
@dataclass
class SamplingParams:
    temperature: float = 1.0
    top_p: float = 1.0
    top_k: int = -1
    min_p: float = 0.0
    # ... 100+ 字段
```

**与 HydraForge B2 SamplerStrategy 对比**:
| 维度 | vLLM | HydraForge SamplerStrategy |
|------|------|--------------------------|
| 设计模式 | 数据驱动 (dataclass 字段) | 行为驱动 (虚接口继承) |
| 扩展方式 | 增删字段 | 继承子类 |
| 性能 | 无虚函数开销 | 虚函数调用 (影响小但存在) |
| 可组合性 | 字段组合 | chain/继承组合 |

---

## 3. SamplerStrategy 接口设计参考

### llama.cpp `llama_sampler_i` (最接近 HydraForge 的设计)

llama.cpp 在 PR #9294 (2024-09) 重构时创建了 C 虚表接口, 2026 年扩展了 GPU backend:

```c
struct llama_sampler_i {
    void (*reset)   (struct llama_sampler * smpl);
    void (*accept)  (struct llama_sampler * smpl, llama_token token);
    void (*apply)   (struct llama_sampler * smpl, llama_token_data_array * cur_p);
    void (*destroy) (struct llama_sampler * smpl);
    // 2026 新增 GPU backend sampling:
    void (*init_ggml)(...);
    void (*apply_ggml)(...);
};
```

**Chain 模式**:
```c
llama_sampler * chain = llama_sampler_chain_init(sparams);
llama_sampler_chain_add(chain, llama_sampler_init_top_k(k));
llama_sampler_chain_add(chain, llama_sampler_init_top_p(p, 1));
```

### 关键对比表

| 维度 | llama.cpp `llama_sampler_i` | HydraForge B2 `SamplerStrategy` |
|------|---------------------------|-------------------------------|
| 接口类型 | C 虚表 (函数指针) | C++ 虚类 |
| 组合方式 | Chain (pipeline) | 单虚方法 |
| GPU offload | 支持 (backend_init/apply_ggml) | 未指定 |
| 历史状态 | 通过 `smpl->ctx` 持有时序状态 | 无状态 |
| 成功年限 | **2 年** (2024-09 → 2026-07) | 从未 ship |

---

## 4. BatchingQueue 实现可行性

### 4.1 零项目先例

| 项目 | Batching 抽象 | 有独立 BatchingQueue? |
|------|--------------|:-------------------:|
| vLLM V1 | Scheduler.schedule() 返回 ScheduleOutput | ❌ 无 |
| SGLang | Scheduler.get_next_batch_to_run() | ❌ 无 |
| llama.cpp | 无 batching (单序列) | ❌ 无 |
| TRT-LLM | CapacityScheduler + MicroBatchScheduler 内建 | ❌ 无 |
| TGI | Rust router 内建 | ❌ 无 |
| LMDeploy | PersistentBatch 内嵌 | ❌ 无 |

**共识**: batching 完全内嵌在 Scheduler, 不被暴露为独立抽象接口。

### 4.2 0 并发用户场景分析

HydraForge B2 提议的 BatchingQueue 5 方法 (submit/flush/cancel/wait_id/size), 在单用户串行场景下:

| 方法 | 退化行为 |
|------|---------|
| submit | 退化为 `enqueue_single` |
| flush | 退化为 `execute_single + return` |
| cancel | **退化: 无排队请求可取消** |
| wait_id | 立即返回 |
| size | 始终返回 0 或 1 |

**结论**: 5 方法中 3 个 (flush/cancel/wait_id) 在 0 并发场景下退化为空操作。

---

## 5. 三层架构先例

### vLLM 的隐式三层

| 层 | vLLM 对应 | 说明 |
|---|----------|------|
| 架构层 (Schema) | `VllmConfig` + `WorkerBase` + `SamplingParams` | 定义契约 (dataclass/ABC) |
| PDK Plugin | `entry_points["vllm.general_plugins"]` | 独立 PyPI 包 |
| 第三方贡献 | `vllm-project/bart-plugin` (独立仓库) | 双 repo 策略 |

### TRT-LLM 的明确三层

| 层 | 实现 | 弹性 |
|---|------|------|
| 架构层 | `SchedulerConfig` / `ExecutorConfig` / `SchedulerPolicyBase` | C++ API + Python ABC |
| 自定义实现 | `SchedulerPolicyBase` 子类 | Python 继承 |
| 第三方后端 | `LLM(backend="_autodeploy")` | 后端选择切换 |

---

## 6. 给 B2 实施的具体建议

### ✅ 保留

1. **C13 schema 定义层**: llama.cpp 的 `llama_sampler_i` 证明 C 层虚接口在 2 年内成功扩展
2. **Plugin 注册模式**: 使用简单 dict + decorator 模式 (vs 复杂 registry 框架)
3. **SamplerStrategy `apply()` 单一虚方法**: 有先例证明可持续

### ❌ 推迟/简化

1. **BatchingQueue (C15)**: 零项目先例, 5 方法中 3 个退化。改为内联或 SchedulerPolicy 单方法模式
2. **Engine/Model 子图**: 从 7 个子图精简到 3 个核心 (load/generate/unload)。vLLM WorkerBase 经过 2 年迭代才稳定

### 🔄 可借鉴的特定设计

1. **llama.cpp 的 `backing_sampling` (GPU offload) 扩展模式**: 以 `_ggml` 后缀方法添加, 不破坏向后兼容
2. **vLLM V1 的配置统一策略**: 单一 `VllmConfig` 配置对象, 避免每次新功能改构造函数签名
3. **TRT-LLM 的 `SchedulerPolicy` 抽象**: 继承 ABC + 1 个 `schedule()` 方法 — BatchingQueue 的**正确替代**