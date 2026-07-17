# ADR-0061-04: SLM 路由优先（NVIDIA Position Paper 2025）

**日期**: 2026-07-16
**状态**: ✅ Approved (P1, 父 ADR-0061 拆分)
**父 ADR**: [../adr-0061-agent-evolution-and-solidification.md](../adr-0061-agent-evolution-and-solidification.md)

---

## 背景

NVIDIA 2025 position paper 论证：SLM (Small Language Models, 1-3B) 接管 agent 中 80% 常规子任务（parse / 抽取 / summarization），比 70B 模型**便宜 10-30×**，延迟低 5-10×。

HydraForge 已有 `model_router` plugin（C7 ship），需要加 SLM 优先路由策略。

## 决策

### 决策 1 — 路由决策矩阵

```cpp
enum class RoutingStrategy {
    Always_LLM,           // 强制 LLM
    SLM_Preferred,         // SLM 优先，失败 fallback LLM
    Auto,                  // 自动判断（基于任务复杂度）
    Cost_Optimized,        // 成本优先
    Latency_Optimized      // 延迟优先
};
```

### 决策 2 — SLM 候选库

| 模型 | 大小 | 适用 |
|------|------|------|
| Qwen 2.5 1.5B | 1.5B | parse, extract |
| Llama 3.2 1B | 1B | summarization |
| Phi-3.5 mini | 3.8B | code generation |
| Mistral 7B | 7B | fallback (如果 SLM 不足) |

### 决策 3 — 路由规则

```cpp
RoutingDecision route_request(const Request& req) {
    // 1. 任务复杂度评估（基于 prompt 长度 + 工具数）
    auto complexity = estimate_complexity(req);
    
    // 2. 选择模型
    if (complexity < THRESHOLD_LOW && slm_available) {
        return {SLM, "qwen-2.5-1.5b", estimated_cost: 0.0001};
    }
    if (complexity < THRESHOLD_MEDIUM) {
        return {SLM, "llama-3.2-1b", estimated_cost: 0.0002};
    }
    return {LLM, "claude-sonnet", estimated_cost: 0.015};
}
```

### 决策 4 — 与 ADR-0059 跨进程路由整合

跨进程 Agent 调用时，路由策略随 manifest 传递：

```json
{
  "agent_id": "code_review/run",
  "routing": "auto",
  "slm_preference": ["qwen-2.5-1.5b", "llama-3.2-1b"],
  "llm_fallback": "claude-sonnet"
}
```

## 实施

- 文件: `pdk/model_router/src/slm_routing.{h,cpp}`
- 测试: `tests/test_slm_routing.cpp`
- 工作量: 1 week
- 优先级: P1

## 参考

- NVIDIA: https://developer.nvidia.com/blog/how-small-language-models-are-key-to-scalable-agentic-ai/
- ADR-0034 (model_router)
- [ADR-0061-06-trajectory-ir](./adr-0061-06-trajectory-ir.md)