# 自举实施路径方案

**ID**: BOOT-001
**日期**: 2026-05-22
**状态**: 已批准
**关联**: VN-001, IP-001, ARCH-001, OPT-001, ADR-0001

---

## 自举定义

**自举**：通过 AgenticDSL 驱动推理计算图，驱动推理输出，而推理的输出内容的质量可以持续驱动 AgenticDSL 运行时工作。

**自举后目标**：AgenticDSL 运行时提供推理服务，以 MCP 或 OpenAI/Anthropic 兼容接口提供推理 API 服务。服务分层：低质量用本地推理，高质量继续使用云端输出。

核心转变：
```
阶段 0: 硬编码参数 + 本地 HTTP 调用 → 无法调整，质量不足
阶段 1: 云端集成 + DSL 可编程参数 → 确保质量，Agent 手动配置
阶段 2: 质量评估闭环 + 服务分层 → 自动选择云端/本地
阶段 3: 完全自举 + 服务化 → 提供 API 服务，持续自进化
```

---

## 实施路线图

### 阶段 0：云端集成 + 质量保障（1-2 周）

**目标**：建立云端 LLM 集成，确保推理质量，为自举提供可靠基础

#### 任务 0.1：云端 LLM 适配器

**当前问题**：
- `LlamaAdapter` 仅支持 HTTP 调用本地 llama.cpp（`localhost:8080`）
- 无云端 LLM（OpenAI/Claude）支持
- 本地 llama.cpp 质量不足，无法支撑自举

**目标架构**：
```
AgenticDSL Runtime
    │
    ├── CloudLLMAdapter (OpenAI/Claude API)
    │       └── 高质量推理（默认）
    │
    └── LocalLLMAdapter (HTTP → llama.cpp)
            └── 低质量推理（降级/辅助）
```

**实施步骤**：

| 步骤 | 文件 | 改动 |
|------|------|------|
| 1 | `src/common/llm/cloud_llm_adapter.h` | 新增云端 LLM 适配器接口 |
| 2 | `src/common/llm/openai_adapter.cpp` | 实现 OpenAI API 调用 |
| 3 | `src/common/llm/anthropic_adapter.cpp` | 实现 Anthropic API 调用 |
| 4 | `src/common/llm/llm_router.h` | 新增推理路由器 |
| 5 | `src/common/tools/registry.cpp` | 注册云端推理工具 |
| 6 | 配置 | 支持 API key 和 endpoint 配置 |

**关键代码**：

```cpp
// cloud_llm_adapter.h
class CloudLLMAdapter {
public:
    struct Config {
        std::string provider;           // "openai" / "anthropic"
        std::string api_key;
        std::string base_url;           // 可选，用于自定义 endpoint
        std::string model;              // "gpt-4", "claude-3-opus", etc.
        
        // 采样参数
        float temperature = 0.7f;
        float top_p = 0.9f;
        int max_tokens = 512;
        std::vector<std::string> stop_tokens;
    };
    
    virtual ~CloudLLMAdapter() = default;
    
    virtual bool initialize(const Config& config) = 0;
    virtual std::string generate(const std::string& prompt, 
                                  const Config& override_config) = 0;
    virtual std::string generate_stream(const std::string& prompt,
                                         std::function<void(const std::string&)> on_token,
                                         const Config& override_config) = 0;
};

// llm_router.h
class LLMRouter {
public:
    enum class Backend {
        CLOUD,      // 云端 LLM（高质量）
        LOCAL,      // 本地 llama.cpp（低质量）
        AUTO        // 自动选择
    };
    
    struct RoutingDecision {
        Backend backend;
        std::string reason;     // 选择原因
        float quality_score;    // 预期质量分数
        float cost_estimate;    // 预估成本
    };
    
    // 根据任务特征选择后端
    RoutingDecision route(const TaskProfile& profile);
    
    // 执行推理
    std::string generate(const std::string& prompt, 
                         const RoutingDecision& decision);
    
private:
    std::unique_ptr<CloudLLMAdapter> cloud_adapter_;
    std::unique_ptr<LocalLLMAdapter> local_adapter_;
};
```

**验证标准**：
- [ ] 云端 LLM 调用成功（OpenAI/Claude）
- [ ] 本地 llama.cpp 调用保持兼容
- [ ] 路由器能正确选择后端
- [ ] 配置加载正常（API key、model 等）

---

#### 任务 0.2：云端推理工具注册

**目标**：注册云端 LLM 推理工具到 ToolRegistry

**工具清单**：

```cpp
// engine.cpp 初始化
void DSLEngine::register_cloud_inference_tools() {
    auto& registry = ToolRegistry::instance();
    
    // 云端推理 - 默认高质量
    registry.register_tool("inference.cloud_generate", 
        [](const Args& args) {
            CloudLLMAdapter::Config config;
            config.provider = args.at("provider");  // "openai" / "anthropic"
            config.model = args.at("model");        // "gpt-4", "claude-3-opus"
            config.temperature = std::stof(args.at("temperature"));
            config.top_p = std::stof(args.at("top_p"));
            config.max_tokens = std::stoi(args.at("max_tokens"));
            
            std::string prompt = args.at("prompt");
            std::string text = cloud_adapter_->generate(prompt, config);
            
            return json{
                {"text", text},
                {"provider", config.provider},
                {"model", config.model},
                {"status", "ok"}
            };
        });
    
    // 本地推理 - 降级/辅助
    registry.register_tool("inference.local_generate",
        [](const Args& args) {
            LocalLLMAdapter::Config config;
            config.temperature = std::stof(args.at("temperature"));
            config.top_p = std::stof(args.at("top_p"));
            config.max_tokens = std::stoi(args.at("max_tokens"));
            
            std::string prompt = args.at("prompt");
            std::string text = local_adapter_->generate(prompt, config);
            
            return json{
                {"text", text},
                {"backend", "local"},
                {"status", "ok"}
            };
        });
    
    // 推理路由 - 根据任务特征选择后端
    registry.register_tool("inference.route",
        [](const Args& args) {
            TaskProfile profile;
            profile.task_type = args.at("task_type");      // "code", "chat", "creative"
            profile.complexity = std::stof(args.at("complexity"));  // 0.0 - 1.0
            profile.quality_requirement = std::stof(args.at("quality_requirement"));  // 0.0 - 1.0
            profile.latency_requirement_ms = std::stoi(args.at("latency_requirement_ms"));
            profile.privacy_sensitive = args.at("privacy_sensitive") == "true";
            
            auto decision = llm_router_->route(profile);
            
            return json{
                {"backend", decision.backend == LLMRouter::Backend::CLOUD ? "cloud" : "local"},
                {"reason", decision.reason},
                {"quality_score", decision.quality_score},
                {"cost_estimate", decision.cost_estimate}
            };
        });
    
    // 质量评估 - 快速规则评估
    registry.register_tool("inference.quality_eval",
        [](const Args& args) {
            std::string output = args.at("output");
            std::string expected_format = args.at("expected_format");  // "json", "code", "text"
            
            QualityMetrics metrics;
            metrics.format_valid = validate_format(output, expected_format);
            metrics.length_score = std::min(1.0f, output.length() / 1000.0f);
            metrics.has_errors = contains_error_keywords(output);
            
            return json{
                {"format_valid", metrics.format_valid},
                {"length_score", metrics.length_score},
                {"has_errors", metrics.has_errors},
                {"overall_score", (metrics.format_valid + metrics.length_score + !metrics.has_errors) / 3.0f}
            };
        });
}
```

**验证标准**：
- [ ] 所有工具可通过 DSL tool_call 调用
- [ ] 参数传递正确
- [ ] 错误处理完善

---

#### 任务 0.3：完善推理标准库子图

**已完成**：engine.md, model.md, session.md

**待实现**：

```yaml
# lib/inference/sampling.md
signature: "(temperature: float, top_p: float, top_k: int, repeat_penalty: float) -> (config: json)"

## /config
  type: tool_call
  tool: inference.sampling_config
  arguments:
    temperature: "{{ inputs.temperature }}"
    top_p: "{{ inputs.top_p }}"
    top_k: "{{ inputs.top_k }}"
    repeat_penalty: "{{ inputs.repeat_penalty }}"
  output_keys: ["status"]

# lib/inference/kv_cache.md
signature: "(strategy: string, max_pages: int) -> (config: json)"

## /config
  type: tool_call
  tool: inference.kv_cache_config
  arguments:
    strategy: "{{ inputs.strategy }}"
    max_pages: "{{ inputs.max_pages }}"
  output_keys: ["status"]

# lib/inference/memory.md
signature: "(gpu_layers: int, use_mmap: bool, use_mlock: bool) -> (config: json)"

## /config
  type: tool_call
  tool: inference.memory_config
  arguments:
    gpu_layers: "{{ inputs.gpu_layers }}"
    use_mmap: "{{ inputs.use_mmap }}"
    use_mlock: "{{ inputs.use_mlock }}"
  output_keys: ["status"]
```

**验证标准**：
- [ ] 所有子图可通过 dsl_call 调用
- [ ] 参数正确传递给底层工具
- [ ] 错误处理完善

---

### 阶段 1：质量评估闭环 + 服务分层（2-3 周）

**目标**：建立质量评估机制，实现云端/本地服务分层

#### 任务 1.1：质量评估节点

**目标**：在 DSL 中内嵌质量评估能力

```yaml
# lib/inference/quality_eval.md
signature: "(output: string, expected_format: string, quality_threshold: float) -> (pass: bool, score: float, feedback: string)"

## /eval_format
  type: tool_call
  tool: inference.quality_eval
  arguments:
    output: "{{ inputs.output }}"
    expected_format: "{{ inputs.expected_format }}"
  output_keys: ["format_valid", "length_score", "has_errors"]

## /eval_content
  type: tool_call
  tool: llm_evaluate
  arguments:
    output: "{{ inputs.output }}"
    criteria: "accuracy, completeness, coherence"
  output_keys: ["content_score", "feedback"]

## /decision
  type: switch
  input: "{{ overall_score }}"
  condition: "{{ overall_score >= inputs.quality_threshold }}"
  cases:
    true: "/pass"
    false: "/fail"

## /pass
  type: assign
  assign:
    pass: true
    score: "{{ overall_score }}"
    feedback: "{{ feedback }}"

## /fail
  type: assign
  assign:
    pass: false
    score: "{{ overall_score }}"
    feedback: "{{ feedback }}"
    suggestion: "{{ suggestion }}"
```

**验证标准**：
- [ ] 质量评估节点可通过 dsl_call 调用
- [ ] 评估结果准确（与人工评估一致性 > 80%）
- [ ] 反馈信息可用于策略调整

---

#### 任务 1.2：服务分层路由

**目标**：实现云端/本地自动路由

```yaml
# lib/inference/router.md
signature: "(task_type: string, complexity: float, quality_requirement: float, latency_requirement_ms: int, privacy_sensitive: bool) -> (backend: string, reason: string)"

## /analyze
  type: tool_call
  tool: inference.route
  arguments:
    task_type: "{{ inputs.task_type }}"
    complexity: "{{ inputs.complexity }}"
    quality_requirement: "{{ inputs.quality_requirement }}"
    latency_requirement_ms: "{{ inputs.latency_requirement_ms }}"
    privacy_sensitive: "{{ inputs.privacy_sensitive }}"
  output_keys: ["backend", "reason", "quality_score", "cost_estimate"]

## /execute_cloud
  type: tool_call
  tool: inference.cloud_generate
  arguments:
    prompt: "{{ inputs.prompt }}"
    provider: "openai"
    model: "gpt-4"
    temperature: 0.7
    max_tokens: "{{ inputs.max_tokens }}"
  output_keys: ["text", "quality_score"]
  condition: "{{ backend == 'cloud' }}"

## /execute_local
  type: tool_call
  tool: inference.local_generate
  arguments:
    prompt: "{{ inputs.prompt }}"
    temperature: 0.7
    max_tokens: "{{ inputs.max_tokens }}"
  output_keys: ["text", "quality_score"]
  condition: "{{ backend == 'local' }}"

## /quality_check
  type: dsl_call
  subgraph: "/lib/inference/quality_eval"
  inputs:
    output: "{{ text }}"
    expected_format: "{{ inputs.expected_format }}"
    quality_threshold: "{{ inputs.quality_requirement }}"
  output_keys: ["pass", "score", "feedback"]

## /fallback
  type: tool_call
  tool: inference.cloud_generate
  arguments:
    prompt: "{{ inputs.prompt }}"
    provider: "openai"
    model: "gpt-4"
  output_keys: ["text", "quality_score"]
  condition: "{{ !pass && backend == 'local' }}"
```

**验证标准**：
- [ ] 路由决策准确（符合预期分层标准）
- [ ] 本地输出质量不达标时自动回退云端
- [ ] 性能统计记录完整

---

### 阶段 2：反馈闭环 + 自适应优化（3-4 周）

**目标**：建立质量反馈闭环，实现自适应优化

#### 任务 2.1：质量反馈基础设施

```cpp
// src/modules/budget/quality_feedback_controller.h
class QualityFeedbackController {
public:
    struct QualityMetrics {
        float format_score;        // 格式正确性
        float content_score;       // 内容质量
        float latency_score;       // 延迟表现
        float cost_score;          // 成本效率
        float overall_score;       // 综合评分
    };
    
    struct FeedbackRecord {
        std::string task_id;
        std::string backend;       // "cloud" / "local"
        QualityMetrics metrics;
        std::string strategy;      // 使用的策略
        nlohmann::json context;    // 任务上下文
    };
    
    void record_feedback(const FeedbackRecord& record);
    QualityMetrics get_average_quality(const std::string& backend, time_range_t range);
    
    // 自适应建议
    struct AdaptationSuggestion {
        std::string current_backend;
        std::string suggested_backend;
        std::string reason;
        float expected_improvement;
    };
    std::vector<AdaptationSuggestion> generate_suggestions();
    
private:
    std::deque<FeedbackRecord> feedback_history_;
    std::map<std::string, std::vector<QualityMetrics>> backend_performance_;
};
```

**验证标准**：
- [ ] 所有推理请求记录质量反馈
- [ ] 可查询历史质量数据
- [ ] 生成合理的优化建议

---

#### 任务 2.2：自适应优化循环

```yaml
# examples/adaptive_optimize.agent.md
## /main/initialize
  type: assign
  assign:
    current_backend: "cloud"
    current_strategy: "default"
    quality_threshold: 0.8
    iteration: 0
  next: ["/main/optimize_loop"]

## /main/optimize_loop
  type: loop
  condition: "{{ iteration < 100 }}"
  body:
    ## /route
      type: dsl_call
      subgraph: "/lib/inference/router"
      inputs:
        task_type: "{{ inputs.task_type }}"
        complexity: "{{ inputs.complexity }}"
        quality_requirement: "{{ quality_threshold }}"
      output_keys: ["backend", "reason"]

    ## /generate
      type: tool_call
      tool: "{{ backend == 'cloud' ? 'inference.cloud_generate' : 'inference.local_generate' }}"
      arguments:
        prompt: "{{ inputs.prompt }}"
        max_tokens: "{{ inputs.max_tokens }}"
      output_keys: ["text", "latency_ms"]

    ## /quality_check
      type: dsl_call
      subgraph: "/lib/inference/quality_eval"
      inputs:
        output: "{{ text }}"
        expected_format: "{{ inputs.expected_format }}"
        quality_threshold: "{{ quality_threshold }}"
      output_keys: ["pass", "score", "feedback"]

    ## /record_feedback
      type: tool_call
      tool: quality_feedback.record
      arguments:
        backend: "{{ backend }}"
        quality_score: "{{ score }}"
        latency_ms: "{{ latency_ms }}"
      output_keys: ["status"]

    ## /adapt
      type: switch
      input: "{{ pass }}"
      cases:
        true: "/adapt/maintain"
        false: "/adapt/adjust"

    ## /adapt/maintain
      type: assign
      assign:
        current_backend: "{{ backend }}"

    ## /adapt/adjust
      type: switch
      input: "{{ backend }}"
      cases:
        local: "/adapt/switch_to_cloud"
        cloud: "/adapt/lower_threshold"

    ## /adapt/switch_to_cloud
      type: assign
      assign:
        current_backend: "cloud"
        quality_threshold: "{{ min(quality_threshold + 0.05, 0.95) }}"

    ## /adapt/lower_threshold
      type: assign
      assign:
        quality_threshold: "{{ max(quality_threshold - 0.05, 0.5) }}"

    ## /increment
      type: assign
      assign:
        iteration: "{{ iteration + 1 }}"

## /main/report
  type: tool_call
  tool: quality_feedback.report
  arguments:
    range: "last_100"
  output_keys: ["cloud_avg_quality", "local_avg_quality", "optimal_ratio"]
```

**验证标准**：
- [ ] 自适应优化收敛（100 轮内找到较优配置）
- [ ] 质量提升可量化
- [ ] 优化过程可解释

---

### 阶段 3：服务化 + 完全自举（4-6 周）

**目标**：提供推理 API 服务，Agent 能够自主发现新的优化策略

#### 任务 3.1：推理 API 服务

**目标**：提供 MCP 或 OpenAI/Anthropic 兼容的推理 API

```cpp
// src/api/inference_server.h
class InferenceServer {
public:
    struct Config {
        std::string host = "0.0.0.0";
        int port = 8080;
        bool enable_mcp = true;
        bool enable_openai_compatible = true;
    };
    
    bool start(const Config& config);
    void stop();
    
    // MCP 接口
    void handle_mcp_request(const json& request, json& response);
    
    // OpenAI 兼容接口
    void handle_chat_completions(const json& request, json& response);
    void handle_embeddings(const json& request, json& response);
    
private:
    std::unique_ptr<LLMRouter> router_;
    std::unique_ptr<QualityFeedbackController> feedback_controller_;
};
```

**服务分层实现**：

```yaml
# api/chat_completions.agent.md
signature: "(messages: list, model: string, quality_tier: string) -> (response: string)"

## /route
  type: tool_call
  tool: inference.route
  arguments:
    task_type: "chat"
    complexity: "{{ estimate_complexity(messages) }}"
    quality_requirement: "{{ quality_tier == 'high' ? 0.95 : 0.8 }}"
  output_keys: ["backend", "reason"]

## /generate
  type: tool_call
  tool: "{{ backend == 'cloud' ? 'inference.cloud_generate' : 'inference.local_generate' }}"
  arguments:
    prompt: "{{ format_messages(messages) }}"
    model: "{{ model }}"
  output_keys: ["text", "quality_score"]

## /quality_check
  type: dsl_call
  subgraph: "/lib/inference/quality_eval"
  inputs:
    output: "{{ text }}"
    quality_threshold: "{{ quality_tier == 'high' ? 0.95 : 0.8 }}"
  output_keys: ["pass", "score"]

## /fallback
  type: tool_call
  tool: inference.cloud_generate
  arguments:
    prompt: "{{ format_messages(messages) }}"
    model: "gpt-4"
  output_keys: ["text", "quality_score"]
  condition: "{{ !pass && backend == 'local' }}"

## /response
  type: assign
  assign:
    response: "{{ text }}"
    backend_used: "{{ backend }}"
    quality_score: "{{ score }}"
```

**验证标准**：
- [ ] API 服务可启动并响应请求
- [ ] MCP 接口符合协议规范
- [ ] OpenAI 兼容接口通过标准测试
- [ ] 服务分层正常工作（高质量→云端，低质量→本地）

---

#### 任务 3.2：元学习优化

**目标**：Agent 自主发现新的优化策略

```python
# 伪代码：Agent 学习优化策略
class MetaOptimizer:
    def __init__(self):
        self.strategy_embeddings = {}
        self.performance_history = []
    
    def encode_task(self, prompt_features):
        """将任务特征编码为向量"""
        return embedding_model.encode(prompt_features)
    
    def find_similar_tasks(self, task_embedding, k=5):
        """找到历史上相似的任务"""
        return nearest_neighbors(self.performance_history, task_embedding, k)
    
    def recommend_strategy(self, task_embedding):
        """基于相似任务推荐策略"""
        similar = self.find_similar_tasks(task_embedding)
        best = max(similar, key=lambda x: x.performance_score)
        return best.strategy
    
    def update(self, task_embedding, strategy, performance):
        """更新知识库"""
        self.performance_history.append({
            'task': task_embedding,
            'strategy': strategy,
            'performance': performance
        })
```

**验证标准**：
- [ ] Agent 能发现新的策略组合
- [ ] 新策略性能优于基线
- [ ] 优化过程可解释

---

## 关键里程碑

| 里程碑 | 时间 | 交付物 | 成功标准 |
|--------|------|--------|---------|
| **M0: 云端集成** | 第 1 周 | CloudLLMAdapter + LLMRouter | 云端调用成功，路由正常 |
| **M1: 工具注册** | 第 1-2 周 | inference.cloud_generate, inference.local_generate, inference.route | 所有工具可调用 |
| **M2: 质量评估** | 第 2 周 | quality_eval.md + 评估节点 | 评估准确率 > 80% |
| **M3: 服务分层** | 第 3 周 | router.md + 自动回退机制 | 分层标准符合预期 |
| **M4: 反馈闭环** | 第 3-4 周 | QualityFeedbackController | 自适应优化收敛 |
| **M5: API 服务** | 第 4-6 周 | InferenceServer (MCP + OpenAI) | API 服务可对外提供 |
| **M6: 完全自举** | 第 6-10 周 | MetaOptimizer | Agent 自主发现新策略 |

---

## 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| 云端 API 不稳定/限流 | 高 | 实现重试机制和降级策略 |
| 本地 llama.cpp 质量始终不足 | 高 | 明确本地定位为"低成本/离线"，不追求超越云端 |
| 成本过高 | 中 | 服务分层，低质量任务强制本地 |
| Agent 策略选择错误 | 中 | 添加安全边界，允许人工覆盖 |
| 多后端支持复杂度 | 高 | 先专注 OpenAI + llama.cpp，后期扩展 |
| 内存泄漏 | 高 | 严格 RAII，使用智能指针 |

---

## 与现有文档的关系

| 文档 | 关系 |
|------|------|
| [VN-001: 自举愿景](../vision/01-self-bootstrapping-vision.md) | 本文是愿景的具体实施计划 |
| [IP-001: 实施路线图](../implementation-roadmap/01-roadmap.md) | 本文补充推理优化专项计划 |
| [ARCH-001: 总体推理架构](../architecture/inference-architecture.md) | 本文是架构的实施路径 |
| [OPT-001: 优化方向方案](../optimization/inference-optimization-strategies.md) | 本文是优化方案的实施计划 |
| [RES-001: 推理引擎调研报告](../research/inference-engine-research.md) | 本文的技术依据 |

---

## 下一步行动

1. **立即开始**：任务 0.1（云端 LLM 适配器）
2. **本周完成**：CloudLLMAdapter + LLMRouter 实现
3. **下周开始**：云端推理工具注册和质量评估节点
4. **持续进行**：服务分层测试和基准建立

---

## 附录：快速启动指南

### 如何添加新的推理工具

```cpp
// 1. 在 llama_adapter.h 中添加方法声明
class LlamaAdapter {
public:
    void new_feature(const Config& config);
};

// 2. 在 llama_adapter.cpp 中实现
void LlamaAdapter::new_feature(const Config& config) {
    // 调用 llama.cpp C API
    llama_new_feature(ctx_, config.param);
}

// 3. 在 registry.cpp 中注册工具
registry.register_tool("inference.new_feature",
    [](const Args& args) {
        LlamaAdapter::Config config;
        // 解析参数
        llama_adapter_.new_feature(config);
        return json{{"status", "ok"}};
    });

// 4. 创建子图
// lib/inference/new_feature.md
signature: "(param: type) -> (result: type)"
## /execute
  type: tool_call
  tool: inference.new_feature
  arguments:
    param: "{{ inputs.param }}"
```

### 如何测试推理优化效果

```bash
# 1. 建立性能基线
./build/benchmark --baseline --config=default.json

# 2. 应用优化策略
./build/benchmark --strategy=high_throughput --config=optimized.json

# 3. 对比结果
./build/benchmark --compare baseline.json optimized.json
```
