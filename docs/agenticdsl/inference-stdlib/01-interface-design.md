# ADR-006: 推理标准库接口设计

**ID**: ADR-006
**日期**: 2026-05-20
**状态**: 已批准（Oracle 确认，决策已定）
**关联**: ADR-002, ADR-003, IP-003, ADR-0001
**调研依据**: Oracle 架构评估（bg_f7bdeec4, bg_0a82905e）

---

## 上下文

AgenticDSL 的核心自举场景是通过 tool_call 调用 llama.cpp 的推理能力。但 llama.cpp 的功能需要被合理抽象——暴露策略控制面给 DSL，隐藏 CUDA/GPU 细节在工具内部。

## 设计原则

1. **工具是边界** — 工具内部是实现细节（CUDA/GPU/内存管理），对 DSL 不可见
2. **标准库是语言** — `lib/inference/` 每个 .md 文件是一个可组合的语义单元
3. **Agent 是编译器** — Agent 根据 workload 特征选择策略组合
4. **控制面全面暴露** — 所有影响推理性能和效果的参数对外可见

---

## 层次架构

```
Agent Workflow (.agent.md)
        │  invokes subgraphs
        ▼
lib/inference/ (标准库子图)
        │  calls via tool_call
        ▼
ToolRegistry tools (C++, 注册为 "inference.xxx")
        │  wraps llama.cpp API
        ▼
llama.cpp (CUDA/GPU/CPU — 隐藏)
```

---

## 工具接口层（C++ 侧）

需要注册以下工具到 `ToolRegistry`：

```cpp
// 引擎生命周期
register_tool("inference.engine_init",      // 初始化引擎
    args: {device, gpu_layers, memory_limit});
register_tool("inference.engine_ready",     // 等待就绪
    args: {session_id});

// 模型管理
register_tool("inference.model_load",       // 加载模型
    args: {model_path, quantization, context_size});
register_tool("inference.model_unload",     // 卸载
    args: {session_id});

// KV-cache 控制
register_tool("inference.kv_cache_config",  // KV-cache 策略
    args: {strategy, max_pages, page_size});
register_tool("inference.kv_cache_stats",   // 当前状态
    args: {session_id});
register_tool("inference.kv_cache_clear",   // 清空
    args: {session_id});

// Prefix-cache 控制
register_tool("inference.prefix_cache_config",
    args: {enabled, algorithm, max_entries});
register_tool("inference.prefix_cache_stats",
    args: {});

// Batching 控制
register_tool("inference.batch_config",
    args: {max_batch_size, scheduling_policy, timeout_ms});
register_tool("inference.batch_stats",
    args: {session_id});

// 解码控制
register_tool("inference.decode_config",
    args: {strategy, n_speculate, draft_model, temperature, top_p});
register_tool("inference.decode_stats",
    args: {session_id});

// 核心推理
register_tool("inference.generate",          // 完整生成
    args: {session_id, prompt, max_tokens, stop_tokens,
           temperature, top_p, frequency_penalty,
           presence_penalty, repetition_penalty, seed, ...});
register_tool("inference.generate_stream",   // 流式生成（包装 ILLMProvider）
    args: {session_id, prompt, max_tokens, stop_tokens,
           temperature, top_p, frequency_penalty,
           presence_penalty, repetition_penalty, seed, ...});
register_tool("inference.forward_token",     // 单步 forward
    args: {session_id, position});
register_tool("inference.stop_generation",   // 停止
    args: {session_id});

// 监控
register_tool("inference.get_stats",         // 全量统计
    args: {session_id});
register_tool("inference.get_throughput",    // 吞吐量
    args: {});
register_tool("inference.get_memory_usage",  // 内存
    args: {});
```

---

## 标准库子图清单（lib/inference/）

| 文件 | 签名 | 暴露控制参数 | 依赖工具 | MVP 可构建？ |
|------|------|------------|---------|------------|
| `engine.md` | (device, memory) → (engine_id) | device(cuda/cpu), gpu_layers, mem_limit | inference.engine_init | ✅ 现在——零新运行时 |
| `model.md` | (path, quant) → (model_id) | quant(f16/f8/q4), ctx_size | inference.model_load | ✅ 现在——零新运行时 |
| `prefix_cache.md` | (enabled, algo) → (config) | algorithm(rolling_hash/exact) | inference.prefix_cache_config | ✅ 现在——零新运行时 |
| `session.md` | (engine, model, configs) → (session) | 聚合以上所有 | 以上全部 | ✅ 现在——纯 dsl_call 聚合 |
| `kv_cache.md` | (strategy, pages) → (config) | strategy(fifo/lru/adaptive), page_size | inference.kv_cache_config | ✅ 现在——json scope nesting 足够 |
| `batching.md` | (batch_size, policy) → (config) | schedule(oldest_first/shortest), max_batch | inference.batch_config | ✅ 现在——自包含 queue 管理 |
| `decoding.md` | (strategy) → (config) | strategy(greedy/sampling/mtp/speculative) | inference.decode_config | ✅ 现在——json scope nesting 足够 |
| ~~`orchestrate.md`~~ | — | — | — | ❌ 用户 workflow 层组合，非标准库子图 |

---

## 标准库子图示例：kv_cache.md

```yaml
### AgenticDSL `/lib/inference/kv_cache`
# --- BEGIN AgenticDSL ---
graph_type: subgraph
signature: "(strategy: string, max_pages: int, page_size: int) -> (config: json)"

module_state:
  page_table: { type: array, scope: session, init: "[]", fork_behavior: deep_copy }
  current_pos: { type: int, scope: session, init: 0 }

nodes:
  - id: validate_strategy
    type: assert
    condition: "{{inputs.strategy in ['fifo', 'lru', 'lfu', 'adaptive']}}"
    on_failure: "/lib/inference/kv_cache/default_config"
    next: ["/lib/inference/kv_cache/apply"]

  - id: apply
    type: tool_call
    tool: inference.kv_cache_config
    arguments:
      strategy: "{{inputs.strategy}}"
      max_pages: "{{inputs.max_pages}}"
      page_size: "{{inputs.page_size}}"
    output_keys: ["result"]
    next: ["/lib/inference/kv_cache/return"]

  - id: default_config
    type: tool_call
    tool: inference.kv_cache_config
    arguments:
      strategy: "adaptive"
      max_pages: 4096
      page_size: 512
    output_keys: ["result"]
    next: ["/lib/inference/kv_cache/return"]

  - id: return
    type: assign
    assign:
      config: "{{result}}"
    output_keys: ["config"]
    next: ["/end_soft"]
# --- END AgenticDSL ---
```

---

> **⚠️ Oracle 结论：orchestrate.md 不应放在 lib/inference/ 标准库中。**
> 编排（adaptively choosing strategies）是 Agent 工作流层的业务逻辑，不是可组合的原语。
> 标准库提供 7 个积木子图（engine/model/kv_cache/prefix_cache/batching/decoding/session），
> 编排由用户在 workflow 层用 FORK/JOIN/dsl_call 自行组合。
> 以下编排图保留作为"架构参考"，但不作为标准库子图实现。

## 编排参考（orchestrate — 用户 workflow）

Agent 可以通过编排实现自适应策略选择：

nodes:
  - id: analyze
    type: dsl_call
    llm_tool: gpt-4
    prompt: |
      负载: {{inputs.workload}}
      选择最优策略组合:
      - kv_cache: fifo/lru/adaptive?
      - prefix_cache: on/off?
      - batch: 多大?
      - 解码: MTP/speculative?
    output_keys: ["strategy"]
    next: ["/lib/inference/orchestrate/fork_config"]

  - id: fork_config
    type: fork
    branches:
      - /lib/inference/kv_cache
      - /lib/inference/prefix_cache
      - /lib/inference/batching
      - /lib/inference/decoding
    next: ["/lib/inference/orchestrate/launch"]

  - id: launch
    type: dsl_call
    subgraph: "/lib/inference/engine"
    input:
      device: "{{workload.device}}"
      configs: "{{strategy}}"
    output_keys: ["engine"]
    next: ["/lib/inference/orchestrate/monitor"]

  - id: monitor
    type: tool_call
    tool: inference.get_stats
    next: ["/lib/inference/orchestrate/decide"]

  - id: decide
    type: dsl_call
    llm_tool: gpt-4
    prompt: "当前统计: {{stats}}。是否需要调整策略？"
    output_keys: ["decision"]
    next: ["/lib/inference/orchestrate/route"]

  - id: route
    type: switch
    input: "{{decision.action}}"
    cases:
      continue: "/lib/inference/orchestrate/monitor"
      adapt: "/lib/inference/orchestrate/analyze"
```

---

---

## Oracle 评估确认（2026-05-21）

| 决策点 | Oracle 结论 | 影响 |
|-------|------------|------|
| **工具粒度** | ✅ **细粒度**（30+ C++ 工具）+ DSL 子图提供粗粒度包装。两层设计两全其美。 | 保持当前 30+ 工具设计 |
| **控制面边界** | ✅ 基本正确。补充 5 个参数（stop_tokens, frequency/presence penalty, repetition_penalty, min_tokens, seed）。隐藏列表全部保持隐藏。 | 更新工具接口参数 |
| **orchestrate.md** | ✅ **不属于标准库**——编排是用户 workflow 层业务逻辑，不是可组合原语。标准库只提供 7 个积木子图。 | 从 lib/inference/ 移除 |
| **与 ILLMProvider 的关系** | ✅ 新工具应 **wrap and extend** 现有的 `ILLMProvider` 接口。`inference.generate_stream` 调用 `ILLMProvider::generate_stream()`。不需要新建 C++ 抽象层。 | 复用 ADR-0001 流式接口 |
| **子图依赖** | ✅ 7/8 子图可在当前代码库上实现（Lazy ModuleState json scope nesting 已足够）。无需等待 YIELD 或 ModuleState schema。 | 立即开始写 .md 文件 |

**详情见** [02-specification.md](02-specification.md) 中的依赖矩阵验证。

---

## 控制面 vs 实现面

| 暴露给 DSL（标准库参数） | 隐藏在工具内部（不暴露） |
|------------------------|----------------------|
| 设备选择: cuda/cpu | CUDA kernel launch configuration |
| Qunatization: f16/f8/q4 | Tensor core 指令选择 |
| KV-cache 策略: fifo/lru/adaptive | Page table 内部实现 |
| Batch size | GPU memory allocation details |
| MTP speculate steps | Draft model architecture |
| Prefix cache on/off | Hash table 实现 |
| Temperature / top-p | Softmax 实现细节 |
| **stop_tokens** (Oracle 建议补充) | — |
| **frequency_penalty / presence_penalty** (Oracle 建议补充) | — |
| **repetition_penalty** (Oracle 建议补充) | — |
| **min_tokens** (Oracle 建议补充) | — |
| **seed** (Oracle 建议补充) | — |

Oracle 确认：所有隐藏项应继续隐藏。Agent 无法在 CUDA kernel launch / page table 层面做出比引擎更好的决策。

---

## 与自举链路的关系

```
阶段0: 外部 llama.cpp API → 通过 tool_call 调用
       └── 当前状态：通过 HTTP 调用 llama.cpp server
       └── 目标状态：直接调用 llama.cpp C API（消除 HTTP 开销）

阶段1: 推理标准库就位（lib/inference/）
       └── Agent 工作流通过标准库子图控制推理
       └── 已创建：engine.md, model.md, session.md
       └── 待创建：kv_cache.md, prefix_cache.md, batching.md, decoding.md

阶段2: 策略模板化（lib/inference/strategies/）
       └── 预定义优化策略：high_throughput, low_latency, memory_efficient
       └── Agent 根据输入特征自动选择策略

阶段3: 自适应优化（auto_optimize.agent.md）
       └── Agent 根据性能反馈自动调整策略
       └── 建立性能监控和自动调优闭环

阶段4: 完全自举（MetaOptimizer）
       └── Agent 自主发现新的优化策略组合
       └── 基于历史数据学习最优配置
```

**关键转变**：从"硬编码参数"到"DSL 可编程"到"Agent 自动优化"

### 当前架构问题

当前 `LlamaAdapter` 通过 HTTP 调用 llama.cpp server：
```
AgenticDSL → LlamaAdapter → HttpLLMAdapter → HTTP → llama.cpp server
```

**问题**：
1. HTTP 延迟 10-50ms
2. 无法访问 llama.cpp C API（KV cache、batching、采样器链）
3. 参数通过 JSON 传递，类型转换开销

**目标架构**（阶段 0 实施）：
```
AgenticDSL → LlamaAdapter → llama.cpp C API → CUDA/GPU
```

**实施路径**：见 [BOOT-001: 自举实施路径方案](../implementation/self-bootstrapping-path.md)

---

## 关联文档

| 文档 | 关系 |
|------|------|
| [02-specification.md](02-specification.md) | 推理标准库 lib/inference/ 每个子图的详细设计 |
| [docs/adr/adr-0001-illm-provider-streaming-interface.md](../../adr/adr-0001-illm-provider-streaming-interface.md) | 当前 ILLMProvider 流式接口，本文工具接口的设计参考 |
| [docs/adr/adr-0005-llm-backend-config-factory.md](../../adr/adr-0005-llm-backend-config-factory.md) | LLM 后端配置工厂，本文 engine_config 工具的参数来源 |
| [docs/adr/adr-0009-dsl-standard-library.md](../../adr/adr-0009-dsl-standard-library.md) | 标准库规划（reasoning/workflow/tools），推理标准库是其中的扩展方向 |
| [docs/specs/dsl.md](../../specs/dsl.md) | 当前 dsl_call / tool_call 节点规范 — 推理标准库的组合方式 |
| [docs/specs/dsl-lib.md](../../specs/dsl-lib.md) | 当前 DSL 库规范目录结构，推理标准库遵循其路径约定 |
| [VN-001: 自举愿景](../vision/01-self-bootstrapping-vision.md) | 推理标准库是自举链路阶段1/2的核心组件 |
| [RES-001: 推理引擎调研报告](../research/inference-engine-research.md) | vLLM/SGLang/llama.cpp 深度调研，本文工具接口的技术依据 |
| [ARCH-001: 总体推理架构](../architecture/inference-architecture.md) | 本文子图的上层架构设计 |
| [OPT-001: 优化方向方案](../optimization/inference-optimization-strategies.md) | 本文子图的具体优化策略 |
