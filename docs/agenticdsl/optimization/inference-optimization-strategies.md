# 推理优化方向方案

**ID**: OPT-001
**日期**: 2026-05-22
**状态**: 已批准
**关联**: ADR-006, ARCH-001, RES-001, ADR-0001

---

## 概述

本文档定义 AgenticDSL 的推理优化方向，涵盖 6 个核心优化维度。每个维度包含：
- **优化目标**：明确要解决的问题
- **可调参数**：DSL 可控制的参数
- **子图实现**：标准库子图设计
- **策略建议**：Agent 如何选择参数
- **预期收益**：量化优化效果

---

## 优化维度 1：采样参数优化

### 目标
根据任务类型选择最佳采样策略，平衡生成质量与速度。

### 可调参数

| 参数 | 范围 | 影响 | 适用场景 |
|------|------|------|---------|
| `temperature` | 0.0 - 2.0 | 随机性 | 低=确定性，高=创意 |
| `top_p` | 0.0 - 1.0 | nucleus sampling | 控制候选 token 范围 |
| `top_k` | 1 - 100 | top-k sampling | 限制候选数量 |
| `min_p` | 0.0 - 1.0 | 最小概率阈值 | 过滤低概率 token |
| `repeat_penalty` | 1.0 - 2.0 | 重复惩罚 | 减少重复生成 |
| `frequency_penalty` | -2.0 - 2.0 | 频率惩罚 | 降低高频词概率 |
| `presence_penalty` | -2.0 - 2.0 | 存在惩罚 | 鼓励新话题 |

### 子图实现

```yaml
# lib/inference/sampling.md
signature: "(task_type: string, creativity: float) -> (config: json)"

## /select_strategy
  type: switch
  input: "{{ inputs.task_type }}"
  cases:
    code: "/config/code"
    chat: "/config/chat"
    creative: "/config/creative"
    analysis: "/config/analysis"

## /config/code
  type: assign
  assign:
    temperature: 0.2
    top_p: 0.95
    top_k: 40
    repeat_penalty: 1.2
    frequency_penalty: 0.2

## /config/chat
  type: assign
  assign:
    temperature: 0.7
    top_p: 0.9
    top_k: 50
    repeat_penalty: 1.1

## /config/creative
  type: assign
  assign:
    temperature: 1.2
    top_p: 0.95
    top_k: 100
    repeat_penalty: 1.0
```

### 策略建议

| 任务类型 | 推荐参数 | 理由 |
|---------|---------|------|
| **代码生成** | T=0.2, top_p=0.95, repeat=1.2 | 确定性输出，强重复惩罚减少重复代码 |
| **技术问答** | T=0.5, top_p=0.9 | 平衡准确性和自然度 |
| **创意写作** | T=1.2, top_p=0.95, top_k=100 | 高随机性，多样化输出 |
| **数据分析** | T=0.3, top_p=0.95 | 低随机性，确保数据准确性 |
| **对话聊天** | T=0.7, top_p=0.9 | 自然流畅，适度随机 |

### 预期收益
- 代码生成：重复率降低 30-50%
- 创意任务：输出多样性提升 2-3x
- 整体：用户满意度提升（主观）

---

## 优化维度 2：KV-Cache 优化

### 目标
最大化 KV cache 命中率，减少重复计算，降低内存使用。

### 可调参数

| 参数 | 范围 | 影响 |
|------|------|------|
| `strategy` | "fifo" / "lru" / "adaptive" | 淘汰策略 |
| `page_size` | 16 - 128 | 每页 token 数 |
| `max_pages` | 256 - 8192 | 最大页数 |
| `enable_prefix_caching` | true / false | 前缀缓存开关 |

### 子图实现

```yaml
# lib/inference/kv_cache.md
signature: "(workload_type: string, memory_budget_mb: int) -> (config: json)"

## /calculate_pages
  type: assign
  assign:
    max_pages: "{{ inputs.memory_budget_mb * 1024 * 1024 / (page_size * head_dim * num_layers * bytes_per_param) }}"

## /select_strategy
  type: switch
  input: "{{ inputs.workload_type }}"
  cases:
    chat: "/config/chat"
    batch: "/config/batch"
    long_context: "/config/long"

## /config/chat
  type: assign
  assign:
    strategy: "lru"
    page_size: 16
    enable_prefix_caching: true  # 多轮对话受益大

## /config/batch
  type: assign
  assign:
    strategy: "fifo"
    page_size: 32  # 大 page 减少碎片
    enable_prefix_caching: false  # batch 场景前缀重复少

## /config/long
  type: assign
  assign:
    strategy: "adaptive"
    page_size: 64  # 超大 page 适合长序列
    enable_prefix_caching: true
```

### 策略建议

| 场景 | 推荐策略 | 理由 |
|------|---------|------|
| **多轮对话** | LRU + Prefix Caching | 最近使用保留，系统 prompt 缓存 |
| **批量处理** | FIFO + 大 Page | 顺序访问，减少碎片 |
| **长文档** | Adaptive + 超大 Page | 自适应调整，减少页表开销 |
| **内存受限** | LRU + 小 Page | 细粒度淘汰，最大化利用率 |

### 预期收益
- Prefix Caching：多轮对话首 token 延迟降低 50-70%
- LRU vs FIFO：缓存命中率提升 20-30%
- 大 Page：长序列吞吐量提升 15-25%

---

## 优化维度 3：动态 Batching

### 目标
最大化 GPU 利用率，平衡吞吐量和延迟。

### 可调参数

| 参数 | 范围 | 影响 |
|------|------|------|
| `max_batch_size` | 1 - 256 | 最大并发请求数 |
| `scheduling` | "fcfs" / "priority" / "shortest" | 调度策略 |
| `timeout_ms` | 1 - 1000 | 等待超时 |
| `max_tokens_per_batch` | 256 - 16384 | 每批最大 token 数 |

### 子图实现

```yaml
# lib/inference/batching.md
signature: "(latency_requirement_ms: int, throughput_priority: bool) -> (config: json)"

## /select_strategy
  type: switch
  input: "{{ inputs.latency_requirement_ms < 100 }}"
  cases:
    "true": "/config/low_latency"
    "false": "/config/high_throughput"

## /config/low_latency
  type: assign
  assign:
    max_batch_size: 1        # 不 batch，立即响应
    scheduling: "priority"   # 高优先级优先
    timeout_ms: 1            # 不等待

## /config/high_throughput
  type: assign
  assign:
    max_batch_size: 32       # 大 batch
    scheduling: "fcfs"       # 公平调度
    timeout_ms: 50           # 等待 50ms 凑 batch
```

### 策略建议

| 场景 | 推荐配置 | 理由 |
|------|---------|------|
| **实时交互** | batch=1, priority | 最低延迟 |
| **批量处理** | batch=32, fcfs, timeout=50ms | 最大吞吐 |
| **混合负载** | batch=8, priority, timeout=10ms | 平衡 |
| **API 服务** | batch=16, fcfs, timeout=20ms | 高吞吐 + 可接受延迟 |

### 预期收益
- 大 batch：吞吐量提升 5-10x
- 动态调度：GPU 利用率从 30% → 80%+
- 低延迟模式：P99 延迟 < 100ms

---

## 优化维度 4：解码策略优化

### 目标
根据速度/质量需求选择最佳解码策略。

### 可调参数

| 参数 | 范围 | 影响 |
|------|------|------|
| `strategy` | "greedy" / "sampling" / "speculative" / "beam" | 解码策略 |
| `draft_model` | string | 投机解码草稿模型路径 |
| `n_speculate` | 1 - 10 | 每次推测 token 数 |
| `beam_width` | 1 - 8 | Beam search 宽度 |

### 子图实现

```yaml
# lib/inference/decoding.md
signature: "(speed_priority: bool, quality_requirement: string) -> (config: json)"

## /select_strategy
  type: switch
  input: "{{ inputs.speed_priority }}"
  cases:
    "true": "/config/fast"
    "false": "/config/quality"

## /config/fast
  type: switch
  input: "{{ inputs.quality_requirement }}"
  cases:
    high: "/config/speculative"
    medium: "/config/greedy"
    low: "/config/greedy"

## /config/quality
  type: assign
  assign:
    strategy: "sampling"
    temperature: 0.7
    top_p: 0.9

## /config/greedy
  type: assign
  assign:
    strategy: "greedy"  # 最快，确定性

## /config/speculative
  type: assign
  assign:
    strategy: "speculative"
    draft_model: "/models/draft-1b.gguf"
    n_speculate: 5
```

### 策略建议

| 场景 | 推荐策略 | 速度 | 质量 |
|------|---------|------|------|
| **实时生成** | Greedy | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ |
| **通用任务** | Sampling | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ |
| **高质量** | Beam Search | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| **极速** | Speculative | ⭐⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ |

### 预期收益
- Greedy：1.5x 速度提升（无采样开销）
- Speculative：2-3x 速度提升（草稿模型加速）
- Beam Search：质量提升 10-20%（BLEU 分数）

---

## 优化维度 5：内存优化

### 目标
在有限 GPU 内存下最大化模型能力和吞吐量。

### 可调参数

| 参数 | 范围 | 影响 |
|------|------|------|
| `gpu_layers` | 0 - 999 | GPU 层数（0=全 CPU） |
| `use_mmap` | true / false | 内存映射加载 |
| `use_mlock` | true / false | 锁定内存防交换 |
| `gpu_memory_limit_mb` | 512 - 81920 | GPU 内存上限 |
| `cpu_offload` | true / false | KV cache 卸载到 CPU |

### 子图实现

```yaml
# lib/inference/memory.md
signature: "(available_gpu_mb: int, model_size_mb: int, priority: string) -> (config: json)"

## /calculate_layers
  type: assign
  assign:
    gpu_layers: "{{ min(999, inputs.available_gpu_mb / (inputs.model_size_mb / num_layers)) }}"

## /select_strategy
  type: switch
  input: "{{ inputs.priority }}"
  cases:
    speed: "/config/speed"
    memory: "/config/memory"
    balanced: "/config/balanced"

## /config/speed
  type: assign
  assign:
    gpu_layers: "{{ max_gpu_layers }}"  # 尽可能多的 GPU 层
    use_mmap: false                     # 直接加载，更快
    use_mlock: true                     # 锁定内存
    cpu_offload: false

## /config/memory
  type: assign
  assign:
    gpu_layers: 20                      # 仅关键层在 GPU
    use_mmap: true                      # 内存映射，省内存
    use_mlock: false
    cpu_offload: true                   # KV cache 在 CPU

## /config/balanced
  type: assign
  assign:
    gpu_layers: "{{ calculated_layers }}"
    use_mmap: true
    use_mlock: false
    cpu_offload: false
```

### 策略建议

| 硬件配置 | 推荐策略 | 理由 |
|---------|---------|------|
| **高端 GPU (A100 80GB)** | Speed | 全 GPU，最大性能 |
| **中端 GPU (RTX 4090 24GB)** | Balanced | 大部分层在 GPU |
| **低端 GPU (RTX 3060 12GB)** | Memory | 关键层在 GPU，其余 CPU |
| **纯 CPU** | Memory | 全部 CPU，mmap 加载 |

### 预期收益
- GPU 层数优化：速度提升 10-50x（vs 纯 CPU）
- mmap：内存使用降低 30-50%
- CPU offload：支持 2-4x 更长上下文

---

## 优化维度 6：流式生成优化

### 目标
实现低延迟的 token-by-token 流式输出。

### 可调参数

| 参数 | 范围 | 影响 |
|------|------|------|
| `mode` | "next" / "continue" / "stop" | 流式模式 |
| `chunk_size` | 1 - 10 | 每批返回 token 数 |
| `yield_interval_ms` | 1 - 100 | 产生间隔 |

### 子图实现

```yaml
# lib/inference/stream.md
signature: "(interactive: bool, display_mode: string) -> (config: json)"

## /select_mode
  type: switch
  input: "{{ inputs.interactive }}"
  cases:
    "true": "/config/interactive"
    "false": "/config/batch"

## /config/interactive
  type: assign
  assign:
    mode: "next"
    chunk_size: 1          # 每个 token 立即返回
    yield_interval_ms: 1   # 最小延迟

## /config/batch
  type: assign
  assign:
    mode: "continue"
    chunk_size: 5          # 每 5 个 token 返回一次
    yield_interval_ms: 20  # 20ms 间隔
```

### 策略建议

| 场景 | 推荐配置 | 理由 |
|------|---------|------|
| **打字机效果** | chunk=1, interval=1ms | 每个 token 立即显示 |
| **平滑输出** | chunk=3, interval=10ms | 减少 UI 刷新开销 |
| **批量流式** | chunk=10, interval=50ms | 降低网络开销 |

### 预期收益
- 首 token 延迟：< 50ms（优化后）
- 用户体验：实时感提升显著

---

## 优化组合策略

### 预定义组合模板

```yaml
# lib/inference/strategies/high_throughput.md
## 高吞吐组合（批量 API 场景）
  type: dsl_call
  subgraph: "/lib/inference/memory"
  inputs:
    priority: "speed"

  type: dsl_call
  subgraph: "/lib/inference/kv_cache"
  inputs:
    strategy: "lru"
    max_pages: 4096

  type: dsl_call
  subgraph: "/lib/inference/batching"
  inputs:
    max_batch_size: 32
    scheduling: "fcfs"
    timeout_ms: 50

  type: dsl_call
  subgraph: "/lib/inference/decoding"
  inputs:
    strategy: "greedy"

# lib/inference/strategies/interactive_chat.md
## 交互式聊天组合（低延迟场景）
  type: dsl_call
  subgraph: "/lib/inference/memory"
  inputs:
    priority: "speed"

  type: dsl_call
  subgraph: "/lib/inference/kv_cache"
  inputs:
    strategy: "lru"
    enable_prefix_caching: true

  type: dsl_call
  subgraph: "/lib/inference/batching"
  inputs:
    max_batch_size: 1
    scheduling: "priority"

  type: dsl_call
  subgraph: "/lib/inference/sampling"
  inputs:
    task_type: "chat"

  type: dsl_call
  subgraph: "/lib/inference/stream"
  inputs:
    interactive: true
```

---

## Agent 自适应优化策略

### 优化决策树

```
输入特征分析
├── 任务类型
│   ├── 代码 → 低 temperature, greedy, 强重复惩罚
│   ├── 创意 → 高 temperature, sampling
│   └── 分析 → 中 temperature, top_p=0.95
│
├── 延迟要求
│   ├── < 100ms → batch=1, stream, greedy
│   ├── 100-500ms → batch=8, sampling
│   └── > 500ms → batch=32, beam search
│
├── 吞吐量要求
│   ├── 高 → 大 batch, fcfs, greedy
│   └── 低 → 小 batch, priority, sampling
│
├── 内存限制
│   ├── < 8GB → 减少 gpu_layers, mmap, cpu_offload
│   ├── 8-24GB → 平衡配置
│   └── > 24GB → 全 GPU, mlock
│
└── 上下文长度
    ├── < 4K → 标准配置
    ├── 4K-32K → 大 page, prefix cache
    └── > 32K → 超大 page, adaptive, cpu_offload
```

### 自动调优循环

```yaml
## /main/auto_tune
  type: loop
  condition: "{{ iteration < max_iterations }}"
  body:
    ## /generate
      type: dsl_call
      subgraph: "/lib/inference/session"
      inputs:
        config: "{{ current_config }}"
      output_keys: ["result", "stats"]

    ## /evaluate
      type: tool_call
      tool: evaluate_performance
      arguments:
        stats: "{{ stats }}"
        targets: "{{ performance_targets }}"
      output_keys: ["score", "bottleneck"]

    ## /adjust
      type: switch
      input: "{{ bottleneck }}"
      cases:
        latency: "/increase_batch"
        memory: "/reduce_gpu_layers"
        quality: "/improve_sampling"
        throughput: "/optimize_batching"
```

---

## 实施优先级

| 优化维度 | 优先级 | 工作量 | 预期收益 |
|---------|--------|--------|---------|
| 采样参数优化 | P0 | 1 天 | 质量提升显著 |
| KV-Cache 优化 | P1 | 2-3 天 | 延迟降低 50%+ |
| 动态 Batching | P1 | 2-3 天 | 吞吐提升 5-10x |
| 解码策略优化 | P2 | 3-5 天 | 速度提升 2-3x |
| 内存优化 | P1 | 2 天 | 支持更大模型 |
| 流式生成 | P2 | 2-3 天 | 用户体验提升 |

---

## 关联文档

| 文档 | 关系 |
|------|------|
| [ARCH-001: 总体推理架构](../architecture/inference-architecture.md) | 本文优化方案的上层架构 |
| [RES-001: 推理引擎调研报告](../research/inference-engine-research.md) | 本文的技术依据 |
| [ADR-006: 推理标准库接口设计](../inference-stdlib/01-interface-design.md) | 子图接口规范 |
| [IP-001: 实施路线图](../implementation-roadmap/01-roadmap.md) | 实施计划 |
