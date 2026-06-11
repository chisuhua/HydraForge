# 推理路由器设计

**ID**: ROUTER-001
**日期**: 2026-05-22
**状态**: 已批准
**关联**: BOOT-001, VN-001, ARCH-001, ADR-0001

---

## 概述

推理路由器是 AgenticDSL 自举架构的核心组件，负责根据任务特征自动选择云端 LLM（高质量）或本地 llama.cpp（低质量）作为推理后端。

## 架构

```
AgenticDSL Runtime
    │
    ├── LLMRouter
    │       ├── TaskAnalyzer（任务特征分析）
    │       ├── RoutingEngine（路由决策引擎）
    │       └── FallbackManager（回退管理）
    │
    ├── CloudLLMAdapter（OpenAI/Anthropic）
    │       └── 高质量推理
    │
    └── LocalLLMAdapter（llama.cpp HTTP）
            └── 低质量推理
```

## 路由决策

### 输入特征

| 特征 | 类型 | 说明 |
|------|------|------|
| `task_type` | string | 任务类型：code/chat/creative/analysis |
| `complexity` | float | 复杂度：0.0-1.0 |
| `quality_requirement` | float | 质量要求：0.0-1.0 |
| `latency_requirement_ms` | int | 延迟要求（毫秒） |
| `privacy_sensitive` | bool | 是否隐私敏感 |
| `cost_sensitive` | bool | 是否成本敏感 |

### 路由规则

```yaml
# 强制云端
- condition: "task_type == 'code' || task_type == 'creative'"
  backend: cloud
  reason: "代码生成和创意写作需要高质量"

- condition: "quality_requirement >= 0.9"
  backend: cloud
  reason: "高质量要求"

- condition: "complexity >= 0.8"
  backend: cloud
  reason: "复杂任务"

# 强制本地
- condition: "privacy_sensitive == true"
  backend: local
  reason: "隐私敏感，必须本地处理"

- condition: "cost_sensitive == true && quality_requirement <= 0.7"
  backend: local
  reason: "成本敏感且质量要求不高"

- condition: "latency_requirement_ms <= 500"
  backend: local
  reason: "低延迟要求"

# 自动选择
- condition: "default"
  backend: auto
  reason: "根据历史性能自动选择"
```

### 自动选择策略

```cpp
class AutoRoutingStrategy {
public:
    Backend select(const TaskProfile& profile) {
        // 查询历史性能数据
        auto cloud_perf = history_.get_average_performance("cloud", profile.task_type);
        auto local_perf = history_.get_average_performance("local", profile.task_type);
        
        // 计算预期质量分数
        float cloud_score = cloud_perf.quality * profile.quality_requirement;
        float local_score = local_perf.quality * (1.0 - profile.quality_requirement);
        
        // 考虑延迟和成本
        float cloud_cost = cloud_perf.latency_ms * cloud_perf.cost_per_request;
        float local_cost = local_perf.latency_ms * local_perf.cost_per_request;
        
        // 综合评分
        float cloud_total = cloud_score * 0.6 + (1.0 / cloud_cost) * 0.4;
        float local_total = local_score * 0.6 + (1.0 / local_cost) * 0.4;
        
        return cloud_total > local_total ? Backend::CLOUD : Backend::LOCAL;
    }
};
```

## 回退机制

### 质量不达标回退

```yaml
# 本地推理质量不达标时回退云端
## /generate_local
  type: tool_call
  tool: inference.local_generate
  arguments:
    prompt: "{{ inputs.prompt }}"
  output_keys: ["text", "quality_score"]

## /quality_check
  type: dsl_call
  subgraph: "/lib/inference/quality_eval"
  inputs:
    output: "{{ text }}"
    quality_threshold: "{{ inputs.quality_requirement }}"
  output_keys: ["pass", "score"]

## /fallback_cloud
  type: tool_call
  tool: inference.cloud_generate
  arguments:
    prompt: "{{ inputs.prompt }}"
    provider: "openai"
    model: "gpt-4"
  output_keys: ["text", "quality_score"]
  condition: "{{ !pass }}"
```

### 异常回退

```cpp
class FallbackManager {
public:
    std::string generate_with_fallback(const std::string& prompt, 
                                        const RoutingDecision& decision) {
        try {
            if (decision.backend == Backend::LOCAL) {
                auto result = local_adapter_->generate(prompt);
                
                // 质量检查
                auto quality = evaluate_quality(result);
                if (quality.score < decision.quality_threshold) {
                    // 回退云端
                    return cloud_adapter_->generate(prompt);
                }
                return result;
            } else {
                return cloud_adapter_->generate(prompt);
            }
        } catch (const std::exception& e) {
            // 异常时回退云端
            return cloud_adapter_->generate(prompt);
        }
    }
};
```

## 接口设计

### C++ 接口

```cpp
class LLMRouter {
public:
    enum class Backend {
        CLOUD,
        LOCAL,
        AUTO
    };
    
    struct TaskProfile {
        std::string task_type;
        float complexity = 0.5f;
        float quality_requirement = 0.8f;
        int latency_requirement_ms = 2000;
        bool privacy_sensitive = false;
        bool cost_sensitive = false;
    };
    
    struct RoutingDecision {
        Backend backend;
        std::string reason;
        float quality_score;
        float cost_estimate;
        float latency_estimate_ms;
    };
    
    // 路由决策
    RoutingDecision route(const TaskProfile& profile);
    
    // 执行推理（含回退）
    std::string generate(const std::string& prompt, 
                         const TaskProfile& profile);
    
    // 注册性能数据
    void record_performance(const std::string& backend,
                           const TaskProfile& profile,
                           float quality_score,
                           float latency_ms,
                           float cost);
    
private:
    std::unique_ptr<CloudLLMAdapter> cloud_adapter_;
    std::unique_ptr<LocalLLMAdapter> local_adapter_;
    std::unique_ptr<AutoRoutingStrategy> auto_strategy_;
    std::unique_ptr<FallbackManager> fallback_manager_;
    PerformanceHistory history_;
};
```

### DSL 接口

```yaml
# lib/inference/router.md
signature: "(task_type: string, complexity: float, quality_requirement: float, latency_requirement_ms: int, privacy_sensitive: bool, cost_sensitive: bool) -> (backend: string, reason: string, quality_score: float, cost_estimate: float)"

## /analyze
  type: tool_call
  tool: inference.route
  arguments:
    task_type: "{{ inputs.task_type }}"
    complexity: "{{ inputs.complexity }}"
    quality_requirement: "{{ inputs.quality_requirement }}"
    latency_requirement_ms: "{{ inputs.latency_requirement_ms }}"
    privacy_sensitive: "{{ inputs.privacy_sensitive }}"
    cost_sensitive: "{{ inputs.cost_sensitive }}"
  output_keys: ["backend", "reason", "quality_score", "cost_estimate"]

## /execute
  type: switch
  input: "{{ backend }}"
  cases:
    cloud: "/execute_cloud"
    local: "/execute_local"

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

## /execute_local
  type: tool_call
  tool: inference.local_generate
  arguments:
    prompt: "{{ inputs.prompt }}"
    temperature: 0.7
    max_tokens: "{{ inputs.max_tokens }}"
  output_keys: ["text", "quality_score"]

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

## /response
  type: assign
  assign:
    text: "{{ text }}"
    backend_used: "{{ backend }}"
    quality_score: "{{ score }}"
    reason: "{{ reason }}"
```

## 配置

```json
{
  "router": {
    "default_backend": "auto",
    "quality_threshold": 0.8,
    "fallback_enabled": true,
    "routing_rules": [
      {
        "name": "code_generation",
        "condition": "task_type == 'code'",
        "backend": "cloud",
        "priority": 100
      },
      {
        "name": "privacy_sensitive",
        "condition": "privacy_sensitive == true",
        "backend": "local",
        "priority": 200
      }
    ],
    "auto_routing": {
      "history_window": 100,
      "quality_weight": 0.6,
      "cost_weight": 0.2,
      "latency_weight": 0.2
    }
  }
}
```

## 验证标准

- [ ] 路由决策准确（符合预期分层标准）
- [ ] 本地输出质量不达标时自动回退云端
- [ ] 异常时自动回退云端
- [ ] 性能统计记录完整
- [ ] 自动选择策略收敛（100 轮内找到较优配置）

## 关联文档

| 文档 | 关系 |
|------|------|
| [BOOT-001: 自举实施路径](../implementation/self-bootstrapping-path.md) | 本文是实施路径的阶段 0 核心组件 |
| [VN-001: 自举愿景](../vision/01-self-bootstrapping-vision.md) | 服务分层的具体实现 |
| [ARCH-001: 总体推理架构](../architecture/inference-architecture.md) | 本文是架构的核心组件 |
