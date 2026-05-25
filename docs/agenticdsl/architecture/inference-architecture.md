# 总体推理架构设计

**ID**: ARCH-001
**日期**: 2026-05-22
**状态**: 已批准
**关联**: ADR-006, RES-001, VN-001, ADR-0001

---

## 架构愿景

AgenticDSL 的推理架构目标是实现 **"LLM 驱动 LLM 优化"** 的自举闭环：

```
Agent (LLM) → DSL 工作流 → 推理子图 → 优化策略 → 推理引擎 → 性能反馈 → Agent
```

---

## 总体架构

### 分层架构图

```
┌─────────────────────────────────────────────────────────────┐
│ Layer 4: Agent 工作流层 (.agent.md)                          │
│  - 业务逻辑编排                                               │
│  - 根据输入特征选择推理策略                                    │
│  - 示例: code_review.agent.md, creative_writing.agent.md      │
└─────────────────────────────────────────────────────────────┘
                              │ dsl_call
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ Layer 3: 推理标准库子图 (lib/inference/*.md)                  │
│  - 可组合的推理策略单元                                       │
│  - engine.md, model.md, session.md, kv_cache.md, ...         │
│  - 每个子图封装一个优化维度                                    │
└─────────────────────────────────────────────────────────────┘
                              │ tool_call
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ Layer 2: 工具抽象层 (ToolRegistry)                            │
│  - inference.engine_init, inference.model_load, ...          │
│  - 统一接口，屏蔽底层引擎差异                                  │
│  - 支持 llama.cpp / vLLM / SGLang 多后端                     │
└─────────────────────────────────────────────────────────────┘
                              │
              ┌───────────────┼───────────────┐
              ▼               ▼               ▼
┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐
│ llama.cpp 后端   │ │ vLLM 后端        │ │ SGLang 后端      │
│ (C++ 原生)       │ │ (Python 桥接)    │ │ (Python 桥接)    │
│ 默认后端         │ │ 高性能场景       │ │ 自动缓存场景     │
└─────────────────┘ └─────────────────┘ └─────────────────┘
```

### 核心设计原则

1. **策略与实现分离**
   - 子图定义"做什么优化"（策略）
   - 工具层定义"怎么实现"（实现）
   - 同一策略可在不同后端有不同实现

2. **组合优于继承**
   - 小粒度子图可组合为大策略
   - 示例：`high_throughput = kv_cache(lru) + batching(16) + decoding(greedy)`

3. **运行时自适应**
   - Agent 可根据反馈动态调整策略
   - 支持 A/B 测试不同策略组合

---

## 核心组件

### 1. 推理子图库 (lib/inference/)

#### 已实现的子图（第一批）

| 子图 | 功能 | 暴露参数 | 状态 |
|------|------|---------|------|
| `engine.md` | 引擎生命周期 | device, gpu_layers, memory_limit | ✅ 已实现 |
| `model.md` | 模型管理 | path, quant, ctx_size | ✅ 已实现 |
| `session.md` | 推理会话聚合 | temperature, top_p, max_tokens, stop_tokens | ✅ 已实现 |

#### 计划实现的子图（第二、三批）

| 子图 | 功能 | 暴露参数 | 依赖 |
|------|------|---------|------|
| `kv_cache.md` | KV-cache 策略 | strategy, page_size, max_pages | Lazy ModuleState |
| `prefix_cache.md` | 前缀缓存 | enabled, algorithm | 无 |
| `batching.md` | 动态 batching | max_batch_size, scheduling, timeout | 无 |
| `decoding.md` | 解码策略 | strategy, n_speculate, draft_model | 无 |
| `memory.md` | 内存优化 | use_mmap, use_mlock, gpu_memory_limit | 无 |
| `sampling.md` | 采样参数 | temperature, top_p, top_k, min_p, penalties | 无 |
| `stream.md` | 流式生成 | mode, chunk_size | YIELD 节点 |

### 2. 工具抽象层

#### 工具命名规范

```
inference.{component}.{action}

示例：
- inference.engine_init      → 初始化引擎
- inference.model_load       → 加载模型
- inference.kv_cache_config  → 配置 KV cache
- inference.batch_config     → 配置 batching
- inference.decode_config    → 配置解码策略
- inference.generate         → 执行生成
- inference.generate_stream  → 流式生成
- inference.get_stats        → 获取性能统计
```

#### 多后端支持

```cpp
// 工具注册时指定后端
class InferenceTool : public Tool {
    BackendType backend_;  // LLAMA_CPP / VLLM / SGLANG
    
    nlohmann::json execute(const Args& args) override {
        switch (backend_) {
            case LLAMA_CPP: return execute_llama(args);
            case VLLM:      return execute_vllm(args);
            case SGLANG:    return execute_sglang(args);
        }
    }
};
```

### 3. 策略模板库

#### 预定义策略模板

```yaml
# lib/inference/strategies/high_throughput.md
## 高吞吐策略（适合批量处理）
  type: dsl_call
  subgraph: "/lib/inference/kv_cache"
  inputs:
    strategy: "lru"
    max_pages: 4096

  type: dsl_call
  subgraph: "/lib/inference/batching"
  inputs:
    max_batch_size: 32
    scheduling: "oldest_first"

  type: dsl_call
  subgraph: "/lib/inference/decoding"
  inputs:
    strategy: "greedy"  # 确定性输出，最快

# lib/inference/strategies/low_latency.md
## 低延迟策略（适合交互式）
  type: dsl_call
  subgraph: "/lib/inference/kv_cache"
  inputs:
    strategy: "fifo"
    max_pages: 1024

  type: dsl_call
  subgraph: "/lib/inference/batching"
  inputs:
    max_batch_size: 1  # 不 batch，立即响应
    scheduling: "priority"

  type: dsl_call
  subgraph: "/lib/inference/decoding"
  inputs:
    strategy: "sampling"
    temperature: 0.7

# lib/inference/strategies/memory_efficient.md
## 内存高效策略（适合长上下文）
  type: dsl_call
  subgraph: "/lib/inference/memory"
  inputs:
    use_mmap: true
    use_mlock: false
    gpu_memory_limit: 4096

  type: dsl_call
  subgraph: "/lib/inference/kv_cache"
  inputs:
    strategy: "adaptive"
    page_size: 32  # 大 page，减少碎片
```

---

## 数据流

### 推理请求生命周期

```
1. Agent 生成工作流
   └─ 根据输入类型选择策略模板

2. 工作流调用推理子图
   └─ session.md → 配置 temperature, top_p, max_tokens

3. 子图调用工具层
   └─ inference.session_create → 创建推理会话

4. 工具层调用后端
   └─ llama.cpp: llama_new_context_with_model()

5. 执行推理
   └─ llama_decode() + llama_sampler_sample()

6. 返回结果 + 性能统计
   └─ tokens_generated, t_eval_ms, memory_usage

7. Agent 根据反馈调整策略
   └─ 如果内存不足 → 切换到 memory_efficient 策略
   └─ 如果延迟高 → 切换到 low_latency 策略
```

### 性能反馈闭环

```yaml
## /main/optimize_loop
  type: dsl_call
  subgraph: "/lib/inference/session"
  inputs:
    strategy: "{{ selected_strategy }}"
  output_keys: ["result", "stats"]

## /main/analyze
  type: tool_call
  tool: analyze_performance
  arguments:
    stats: "{{ stats }}"
    target_latency_ms: 100
    target_memory_mb: 4096
  output_keys: ["recommendation"]

## /main/adapt
  type: assign
  assign:
    selected_strategy: "{{ recommendation.next_strategy }}"
  next:
    - condition: "{{ recommendation.need_adjust }}"
      target: "/main/optimize_loop"  # 循环优化
    - condition: "true"
      target: "/main/done"
```

---

## 扩展点

### 1. 添加新后端

要实现 vLLM 后端：

```cpp
// src/common/tools/inference/vllm_backend.cpp
class VLLMBackend : public InferenceBackend {
public:
    nlohmann::json engine_init(const Args& args) override {
        // 调用 vLLM Python API（通过 pybind11 或 subprocess）
        // 或启动 vLLM server 进程
    }
    
    nlohmann::json generate(const Args& args) override {
        // 调用 vLLM LLMEngine.generate()
    }
};
```

### 2. 添加新优化维度

添加量化策略子图：

```yaml
# lib/inference/quantization.md
signature: "(model_path: string, target_format: string) -> (quantized_path: string)"

## /quantize
  type: tool_call
  tool: inference.quantize_model
  arguments:
    model_path: "{{ inputs.model_path }}"
    format: "{{ inputs.target_format }}"  # Q4_0, Q4_1, Q8_0, F16
```

### 3. 添加新策略模板

```yaml
# lib/inference/strategies/code_generation.md
## 代码生成专用策略
  type: dsl_call
  subgraph: "/lib/inference/sampling"
  inputs:
    temperature: 0.2      # 低温度，确定性
    top_p: 0.95
    repeat_penalty: 1.2   # 强重复惩罚

  type: dsl_call
  subgraph: "/lib/inference/decoding"
  inputs:
    strategy: "greedy"    # 贪婪解码
```

---

## 与现有系统的集成

### 与 ToolRegistry 的集成

```cpp
// engine.cpp 初始化时注册推理工具
void DSLEngine::register_inference_tools() {
    auto& registry = ToolRegistry::instance();
    
    // 注册 llama.cpp 后端工具
    registry.register_tool("inference.engine_init", 
        [](const Args& args) { return llama_backend_.engine_init(args); });
    
    registry.register_tool("inference.model_load",
        [](const Args& args) { return llama_backend_.model_load(args); });
    
    // ... 其他工具
}
```

### 与 ExecutionSession 的集成

```cpp
// execution_session.h
class ExecutionSession {
    // ... 现有成员 ...
    
    // 推理会话状态（Lazy init）
    std::map<std::string, nlohmann::json> inference_sessions_;
    
    // 性能统计
    struct InferenceStats {
        int total_tokens = 0;
        float avg_latency_ms = 0.0f;
        float peak_memory_mb = 0.0f;
    };
    std::optional<InferenceStats> inference_stats_;
};
```

---

## 实施状态

| 组件 | 状态 | 优先级 |
|------|------|--------|
| 第一批子图（engine/model/session） | ✅ 已实现 | P0 |
| 工具抽象层（llama.cpp 后端） | ⚠️ 部分实现（HTTP 调用） | P0 |
| 第二批子图（kv_cache/prefix_cache/batching/decoding） | ⏳ 待实现 | P1 |
| 策略模板库 | ⏳ 待实现 | P2 |
| 多后端支持（vLLM/SGLang） | ⏳ 待实现 | P3 |
| 性能反馈闭环 | ⏳ 待实现 | P2 |
| 从 HTTP 切换到 C API | ⏳ 待实现 | P0 |

---

## 关联文档

| 文档 | 关系 |
|------|------|
| [RES-001: 推理引擎调研报告](../research/inference-engine-research.md) | 本文架构设计的技术依据 |
| [ADR-006: 推理标准库接口设计](../inference-stdlib/01-interface-design.md) | 子图接口规范 |
| [IP-001: 实施路线图](../implementation-roadmap/01-roadmap.md) | 实施计划 |
| [VN-001: 自举愿景](../vision/01-self-bootstrapping-vision.md) | 架构目标 |
| [lib/inference/engine.md](../../../lib/inference/engine.md) | 参考实现 |
