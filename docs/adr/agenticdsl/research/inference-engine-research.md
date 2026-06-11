# 推理引擎深度调研报告

**ID**: RES-001
**日期**: 2026-05-22
**状态**: 已批准
**关联**: ADR-006, IP-001, VN-001
**调研范围**: vLLM, SGLang, llama.cpp 低层 API

---

## 执行摘要

本次调研深入分析了三大主流推理引擎（vLLM、SGLang、llama.cpp）的低层 API 和优化能力，旨在为 AgenticDSL 的推理标准库设计提供技术依据。核心发现：

1. **llama.cpp** 提供丰富的 C API，但 AgenticDSL 当前仅通过 HTTP 调用，无法访问低层优化接口
2. **vLLM** 的 PagedAttention 和动态调度是可配置优化的最佳实践
3. **SGLang** 的 RadixAttention 为自动前缀缓存提供了创新方案
4. **AgenticDSL 当前差距**：仅暴露 temperature/max_tokens，缺失 KV-cache、batching、解码策略等关键优化维度

---

## 一、llama.cpp 低层 API 分析

### 1.1 当前 AgenticDSL 集成状态

AgenticDSL 当前的 `LlamaAdapter` 通过 **HTTP API** 调用 llama.cpp 服务器：

```cpp
// llama_adapter.cpp (当前实现)
std::string LlamaAdapter::generate(const std::string& prompt) {
    HttpLLMAdapter http_adapter(LLMConfig{
        config_.api_url,      // http://localhost:8080
        config_.api_endpoint, // /v1/chat/completions
        config_.temperature,
        config_.n_predict,    // 映射为 max_tokens
        // ... 其他参数通过 HTTP JSON 传递
    });
    return http_adapter.generate(prompt, {}).text;
}
```

**问题**：HTTP 调用增加了 10-50ms 延迟，且无法访问 llama.cpp 的 C API 优化接口。

### 1.2 llama.cpp C API 能力矩阵

| API 类别 | 关键函数 | 可调优参数 | AgenticDSL 暴露状态 |
|---------|---------|-----------|-------------------|
| **后端初始化** | `llama_init_backend()` | `numa`, `ggml_backend` | ❌ 未暴露 |
| **模型加载** | `llama_load_model_from_file()` | `n_gpu_layers`, `split_mode`, `use_mmap`, `use_mlock`, `vocab_only` | ❌ 未暴露 |
| **上下文创建** | `llama_new_context_with_model()` | `n_ctx`, `n_batch`, `n_ubatch`, `n_threads`, `n_threads_batch`, `flash_attn`, `type_k`, `type_v` | ⚠️ 仅 n_ctx |
| **KV-cache 控制** | `llama_kv_cache_clear()`, `llama_kv_cache_seq_rm()`, `llama_kv_cache_seq_cp()` | 序列级操作 | ❌ 未暴露 |
| **采样配置** | `llama_sampler_chain_init()`, `llama_sampler_add_greedy()`, `llama_sampler_add_top_k()`, `llama_sampler_add_top_p()`, `llama_sampler_add_temp()`, `llama_sampler_add_penalties()` | `temperature`, `top_p`, `top_k`, `min_p`, `repeat_penalty`, `frequency_penalty`, `presence_penalty` | ⚠️ 仅 temperature |
| **批量解码** | `llama_decode()` 接受 `llama_batch` | `n_tokens`, `token`, `pos`, `n_seq_id`, `seq_id`, `logits` | ❌ 未暴露 |
| **性能查询** | `llama_get_timings()` | `t_start_ms`, `t_load_ms`, `t_sample_ms`, `t_p_eval_ms`, `t_eval_ms` | ❌ 未暴露 |
| **量化格式** | 模型加载时指定 `ggml_type` | `Q4_0`, `Q4_1`, `Q5_0`, `Q5_1`, `Q8_0`, `F16` | ❌ 未暴露 |

### 1.3 关键发现

**发现 1：llama.cpp 的采样器链是高度可配置的**

```c
// 示例：构建自定义采样策略
llama_sampler* smpl = llama_sampler_chain_init({});
llama_sampler_chain_add(smpl, llama_sampler_init_temp(0.7f));        // 温度
llama_sampler_chain_add(smpl, llama_sampler_init_top_p(0.9f, 1));    // top-p
llama_sampler_chain_add(smpl, llama_sampler_init_top_k(40));         // top-k
llama_sampler_chain_add(smpl, llama_sampler_init_penalties(
    64, 1.1f, 0.0f, 0.0f)); // 重复惩罚
```

**发现 2：KV cache 支持序列级操作**

```c
// 清除特定序列的 KV cache
llama_kv_cache_seq_rm(ctx, seq_id, p0, p1);

// 复制序列（用于 speculative decoding）
llama_kv_cache_seq_cp(ctx, seq_id_src, seq_id_dst, p0, p1);
```

**发现 3：批量解码支持多序列并行**

```c
llama_batch batch = llama_batch_init(n_tokens, 0, n_seq_max);
// 填充多个序列的 token
llama_decode(ctx, batch);
```

---

## 二、vLLM 低层 API 分析

### 2.1 架构组件

```
LLMEngine / AsyncLLMEngine
├── Scheduler (调度器)
│   ├── Policy: FCFS / Priority
│   └── 控制: max_num_seqs, max_num_batched_tokens
├── Worker (工作进程)
│   └── ModelRunner
├── CacheEngine (缓存引擎)
│   ├── PagedAttention
│   └── Prefix Caching
└── ModelRunner (模型执行器)
    └── 执行实际推理
```

### 2.2 可配置参数矩阵

| 组件 | 配置类 | 关键参数 | 优化影响 |
|------|--------|---------|---------|
| **引擎** | `EngineArgs` | `model`, `tensor_parallel_size`, `pipeline_parallel_size`, `gpu_memory_utilization`, `max_model_len` | 内存分配、并行度 |
| **KV-cache** | `CacheConfig` | `block_size` (默认 16), `gpu_memory_utilization`, `swap_space`, `enable_prefix_caching`, `cpu_offload_gb` | 缓存命中率、内存使用 |
| **调度** | `SchedulerConfig` | `max_num_seqs` (默认 256), `max_num_batched_tokens`, `scheduling_policy` ("fcfs"/"priority"), `preemption_mode` ("swap"/"recompute") | 吞吐量、延迟 |
| **采样** | `SamplingParams` | `temperature`, `top_p`, `top_k`, `min_p`, `max_tokens`, `stop`, `frequency_penalty`, `presence_penalty`, `repetition_penalty` | 生成质量 |
| **投机解码** | `SpeculativeConfig` | `draft_model`, `num_speculative_tokens`, `speculative_max_model_len` | 2-3x 速度提升 |
| **并行** | `ParallelConfig` | `pipeline_parallel_size`, `tensor_parallel_size` | 多 GPU 扩展 |

### 2.3 关键发现

**发现 1：Scheduler 是可插拔的**

```python
# vLLM 调度策略可配置
scheduler_config = SchedulerConfig(
    max_num_seqs=256,
    max_num_batched_tokens=4096,
    scheduling_policy="fcfs",  # 或 "priority"
    preemption_mode="swap"     # 或 "recompute"
)
```

**发现 2：Prefix Caching 可显著提升多轮对话性能**

```python
# 启用前缀缓存
engine_args = EngineArgs(
    model="meta-llama/Llama-2-7b",
    enable_prefix_caching=True,  # 自动缓存系统 prompt
    gpu_memory_utilization=0.9
)
```

**发现 3：投机解码提供 2-3x 加速**

```python
# 投机解码配置
speculative_config = SpeculativeConfig(
    draft_model="tinyllama/TinyLlama-1.1B",  # 小模型草稿
    num_speculative_tokens=5,                  # 每次推测 5 个 token
)
```

---

## 三、SGLang 低层 API 分析

### 3.1 核心创新：RadixAttention

SGLang 的 **RadixAttention** 是自动 KV cache 复用机制，比 vLLM 的 prefix caching 更高效：

```
RadixAttention (树形结构)
├── 自动识别相同前缀
├── LRU 淘汰策略
└── 无需手动配置
```

### 3.2 Runtime API

```python
import sglang as sgl

# 初始化引擎
llm = sgl.Engine(
    model_path="meta-llama/Llama-2-7b",
    tp_size=1,  # tensor parallel size
)

# 采样参数
sampling_params = {
    "temperature": 0.8,
    "top_p": 0.95,
    "max_new_tokens": 256,
    "stop": ["\n", "###"],
    "regex": "[A-Z]{3}-[0-9]{4}"  # 结构化输出约束
}

# 生成
outputs = llm.generate(
    prompts=["What is the capital of France?"],
    sampling_params=sampling_params
)
```

### 3.3 关键发现

**发现 1：自动前缀缓存无需配置**

SGLang 自动管理 KV cache 复用，无需像 vLLM 那样手动启用 `enable_prefix_caching`。

**发现 2：结构化生成通过 regex 约束**

```python
# 强制输出 JSON 格式
sampling_params = {
    "regex": r'\{"name": "[a-zA-Z]+", "age": [0-9]+\}'
}
```

**发现 3：SGLang 的编程模型是"语言"而非"API"**

SGLang 提供 `@sgl.function` 装饰器定义生成流程，更接近 AgenticDSL 的 subgraph 概念。

---

## 四、三大引擎对比总结

| 维度 | llama.cpp | vLLM | SGLang | 推荐方案 |
|------|-----------|------|--------|---------|
| **部署复杂度** | 低（单文件） | 高（Python 生态） | 中（Python 生态） | llama.cpp |
| **性能优化** | 中 | 高（PagedAttention） | 高（RadixAttention） | vLLM / SGLang |
| **API 可控性** | 高（C API） | 中（Python API） | 中（Python API） | llama.cpp |
| **内存效率** | 中 | 高 | 高 | vLLM / SGLang |
| **批处理** | 手动 | 自动 | 自动 | vLLM / SGLang |
| **前缀缓存** | 无 | 有（需配置） | 有（自动） | SGLang |
| **投机解码** | 无 | 有 | 无 | vLLM |
| **与 C++ 集成** | 原生 | 困难 | 困难 | llama.cpp |
| **适合 AgenticDSL** | ⭐⭐⭐ | ⭐⭐ | ⭐⭐ | **llama.cpp** |

**结论**：AgenticDSL 应**以 llama.cpp 为默认后端**（C++ 原生集成），同时**预留 vLLM/SGLang 的适配接口**（通过工具抽象层）。

---

## 五、AgenticDSL 当前差距分析

### 5.1 能力差距矩阵

| 优化维度 | llama.cpp 支持 | AgenticDSL 当前 | 差距等级 |
|---------|---------------|----------------|---------|
| **推理参数** | temperature, top_p, top_k, min_p, repeat_penalty, frequency_penalty, presence_penalty | temperature, max_tokens | 🔴 大 |
| **KV-cache 策略** | clear, seq_rm, seq_cp, 手动管理 | 无 | 🔴 大 |
| **动态 batching** | llama_batch 支持多序列 | 无 | 🔴 大 |
| **解码策略** | greedy, sampling, beam search | 无（仅默认 sampling） | 🟡 中 |
| **内存优化** | use_mmap, use_mlock, gpu_layers | 无 | 🔴 大 |
| **上下文窗口** | n_ctx 可配置 | 硬编码 2048 | 🟡 中 |
| **线程数** | n_threads, n_threads_batch | 硬编码 4 | 🟡 中 |
| **量化格式** | 加载时选择 | 无 | 🟡 中 |
| **性能监控** | llama_get_timings() | 无 | 🟡 中 |
| **流式生成** | 手动实现 token-by-token | 无 | 🔴 大 |

### 5.2 架构差距

**当前架构**（HTTP 调用）：
```
AgenticDSL → LlamaAdapter → HttpLLMAdapter → HTTP → llama.cpp server
```

**问题**：
1. HTTP 延迟 10-50ms
2. 无法访问 C API
3. 参数通过 JSON 传递，类型转换开销
4. 无法做细粒度优化（KV cache、batching）

**目标架构**（直接库调用）：
```
AgenticDSL → LlamaAdapter → llama.cpp C API → CUDA/GPU
```

---

## 六、关键洞察与建议

### 洞察 1：AgenticDSL 的自举目标不是替换推理引擎，而是编排优化策略

AgenticDSL 不需要重新实现 PagedAttention 或 RadixAttention，而是：
- **暴露** llama.cpp 的优化参数给 DSL
- **编排** 不同优化策略的组合（如 "高吞吐模式" = 大 batch + prefix cache）
- **自适应** 根据 workload 自动选择策略

### 洞察 2：推理子图是"策略模板"而非"实现"

```yaml
# lib/inference/high_throughput.md —— 高吞吐策略模板
## /config
  type: dsl_call
  subgraph: "/lib/inference/kv_cache"
  inputs:
    strategy: "lru"
    max_pages: 2048

## /batch
  type: dsl_call
  subgraph: "/lib/inference/batching"
  inputs:
    max_batch_size: 16
    scheduling: "oldest_first"
```

### 洞察 3：从 HTTP 到 C API 是必要的技术债务偿还

当前 HTTP 调用是原型阶段的临时方案。要实现 LLM 驱动的推理优化，必须：
1. 直接调用 llama.cpp C API
2. 暴露所有可调参数
3. 支持运行时动态调整

---

## 七、调研方法论

| 引擎 | 调研方法 | 数据来源 |
|------|---------|---------|
| llama.cpp | 代码审查 + 官方文档 | `llama.h` 头文件, README.md, examples/main.cpp |
| vLLM | Context7 文档 + 官方文档 | vLLM docs (context7), GitHub 源码 |
| SGLang | Context7 文档 + 官方文档 | SGLang docs (context7), GitHub 源码 |
| AgenticDSL | 代码审查 | `src/common/llm/`, `src/common/tools/` |

---

## 关联文档

| 文档 | 关系 |
|------|------|
| [ADR-006: 推理标准库接口设计](../inference-stdlib/01-interface-design.md) | 本文调研结果直接支持 ADR-006 的工具接口设计 |
| [IP-001: 实施路线图](../implementation-roadmap/01-roadmap.md) | 本文差距分析指导实施优先级 |
| [VN-001: 自举愿景](../vision/01-self-bootstrapping-vision.md) | 本文证明自举的技术可行性 |
| [lib/inference/engine.md](../../../lib/inference/engine.md) | 第一批实现的子图 |
| [lib/inference/session.md](../../../lib/inference/session.md) | 核心自举节点 |
