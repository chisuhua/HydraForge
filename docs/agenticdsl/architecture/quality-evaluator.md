# 质量评估节点设计

**ID**: QUALITY-001
**日期**: 2026-05-22
**状态**: 已批准
**关联**: BOOT-001, VN-001, ROUTER-001, ADR-0008

---

## 概述

质量评估节点是 AgenticDSL 自举闭环的核心组件，负责评估推理输出的质量，并将反馈传递给运行时以驱动策略调整。

## 架构

```
推理输出
    │
    ├── 快速规则评估（DSL 内嵌）
    │       ├── 格式检查
    │       ├── 长度检查
    │       └── 错误关键词检查
    │
    └── 深度质量评估（云端老师模型）
            ├── 内容准确性
            ├── 完整性
            └── 连贯性
```

## 评估维度

### 快速规则评估

| 维度 | 方法 | 权重 |
|------|------|------|
| **格式正确性** | 检查是否符合预期格式（JSON/XML/代码等） | 0.3 |
| **长度合理性** | 检查输出长度是否在合理范围内 | 0.2 |
| **错误检测** | 检查是否包含错误关键词（error/fail/exception 等） | 0.3 |
| **结构完整性** | 检查必要字段是否都存在 | 0.2 |

### 深度质量评估

| 维度 | 方法 | 权重 |
|------|------|------|
| **内容准确性** | 云端 LLM 评估内容是否正确 | 0.4 |
| **完整性** | 是否回答了所有问题 | 0.3 |
| **连贯性** | 逻辑是否连贯 | 0.2 |
| **创造性** | 是否有创造性（创意写作任务） | 0.1 |

## 实现

### C++ 接口

```cpp
class QualityEvaluator {
public:
    struct Metrics {
        float format_score = 0.0f;      // 格式正确性
        float length_score = 0.0f;      // 长度合理性
        float error_score = 0.0f;       // 错误检测（0=有错误，1=无错误）
        float structure_score = 0.0f;   // 结构完整性
        float content_score = 0.0f;     // 内容准确性
        float completeness_score = 0.0f; // 完整性
        float coherence_score = 0.0f;   // 连贯性
        float overall_score = 0.0f;     // 综合评分
    };
    
    struct Config {
        float quality_threshold = 0.8f;  // 质量阈值
        bool enable_deep_eval = true;    // 是否启用深度评估
        std::string expected_format;     // 预期格式
    };
    
    // 快速规则评估
    Metrics quick_evaluate(const std::string& output, const Config& config);
    
    // 深度质量评估（调用云端 LLM）
    Metrics deep_evaluate(const std::string& output, 
                          const std::string& prompt,
                          const Config& config);
    
    // 综合评估
    Metrics evaluate(const std::string& output,
                     const std::string& prompt,
                     const Config& config);
    
private:
    std::unique_ptr<CloudLLMAdapter> cloud_adapter_;
    
    float evaluate_format(const std::string& output, const std::string& expected_format);
    float evaluate_length(const std::string& output);
    float evaluate_errors(const std::string& output);
    float evaluate_structure(const std::string& output, const std::string& expected_format);
};
```

### DSL 接口

```yaml
# lib/inference/quality_eval.md
signature: "(output: string, prompt: string, expected_format: string, quality_threshold: float) -> (pass: bool, score: float, feedback: string)"

## /quick_eval
  type: tool_call
  tool: inference.quality_eval
  arguments:
    output: "{{ inputs.output }}"
    expected_format: "{{ inputs.expected_format }}"
  output_keys: ["format_score", "length_score", "error_score", "structure_score"]

## /calculate_quick_score
  type: assign
  assign:
    quick_score: "{{ (format_score + length_score + error_score + structure_score) / 4.0 }}"

## /deep_eval
  type: tool_call
  tool: llm_evaluate
  arguments:
    output: "{{ inputs.output }}"
    prompt: "{{ inputs.prompt }}"
    criteria: "accuracy, completeness, coherence"
  output_keys: ["content_score", "completeness_score", "coherence_score"]
  condition: "{{ quick_score >= 0.5 }}"  # 快速评估通过才进行深度评估

## /calculate_overall_score
  type: assign
  assign:
    overall_score: "{{ quick_score * 0.4 + (content_score + completeness_score + coherence_score) / 3.0 * 0.6 }}"

## /decision
  type: switch
  input: "{{ overall_score >= inputs.quality_threshold }}"
  cases:
    true: "/pass"
    false: "/fail"

## /pass
  type: assign
  assign:
    pass: true
    score: "{{ overall_score }}"
    feedback: "质量达标"

## /fail
  type: assign
  assign:
    pass: false
    score: "{{ overall_score }}"
    feedback: "{{ generate_feedback(format_score, length_score, error_score, content_score) }}"
    suggestion: "{{ generate_suggestion(overall_score, inputs.quality_threshold) }}"
```

## 反馈生成

### 规则生成

```cpp
std::string QualityEvaluator::generate_feedback(const Metrics& metrics) {
    std::vector<std::string> issues;
    
    if (metrics.format_score < 0.8) {
        issues.push_back("格式不正确");
    }
    if (metrics.length_score < 0.5) {
        issues.push_back("输出过短或过长");
    }
    if (metrics.error_score < 1.0) {
        issues.push_back("包含错误信息");
    }
    if (metrics.content_score < 0.7) {
        issues.push_back("内容不准确");
    }
    
    if (issues.empty()) {
        return "质量达标";
    }
    
    return "问题：" + join(issues, "，");
}

std::string QualityEvaluator::generate_suggestion(float score, float threshold) {
    if (score < threshold * 0.5) {
        return "建议：切换到云端 LLM 获取更高质量输出";
    } else if (score < threshold * 0.8) {
        return "建议：调整采样参数（降低 temperature）";
    } else {
        return "建议：微调提示词模板";
    }
}
```

## 与自举闭环集成

```
推理计算图 → 推理输出 → 质量评估节点 → 反馈
                                      │
                                      ▼
                              ┌──────────────┐
                              │ 质量达标？    │
                              └──────┬───────┘
                                     │
                    ┌────────────────┼────────────────┐
                    ▼                ▼                ▼
                 是               否（本地）        否（云端）
                    │                │                │
                    ▼                ▼                ▼
               继续使用          回退云端          调整参数
               当前策略          重新推理          重试
```

## 配置

```json
{
  "quality_eval": {
    "default_threshold": 0.8,
    "quick_eval_weight": 0.4,
    "deep_eval_weight": 0.6,
    "enable_deep_eval": true,
    "deep_eval_provider": "openai",
    "deep_eval_model": "gpt-4",
    "format_checks": {
      "json": {
        "required_fields": [],
        "schema": null
      },
      "code": {
        "language": "auto",
        "syntax_check": true
      }
    }
  }
}
```

## 验证标准

- [ ] 质量评估节点可通过 dsl_call 调用
- [ ] 快速评估延迟 < 10ms
- [ ] 深度评估准确率 > 85%（与人工评估一致性）
- [ ] 反馈信息可用于策略调整
- [ ] 评估结果可解释

## 关联文档

| 文档 | 关系 |
|------|------|
| [BOOT-001: 自举实施路径](../implementation/self-bootstrapping-path.md) | 本文是实施路径的阶段 1 核心组件 |
| [VN-001: 自举愿景](../vision/01-self-bootstrapping-vision.md) | 质量评估闭环的具体实现 |
| [ROUTER-001: 推理路由器](inference-router.md) | 质量评估结果驱动路由决策 |
