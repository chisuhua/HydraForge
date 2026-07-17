# ADR-0061-07: PASTE-style 推测执行

**日期**: 2026-07-16
**状态**: 🟡 Proposed (P2, v2 候选)
**父 ADR**: [../adr-0061-agent-evolution-and-solidification.md](../adr-0061-agent-evolution-and-solidification.md)

---

## 背景

Microsoft Research PASTE 2026 (arXiv:2603.18897)：发现 agent 工具调用模式稳定，从历史 trace 推断未来调用并 speculatively 执行；平均任务时长 ↓48.5%，工具吞吐 1.8×。

## 决策

### 决策 1 — Speculative Tool Fork

在 ExecutionSession 层加 speculative fork：

```cpp
class ExecutionSession {
    // 历史模式挖掘
    Pattern mine_pattern(const std::vector<TraceRecord>& history);
    
    // 推测预执行
    void speculative_tool_fork(const ToolCall& current, const Pattern& p);
    
    // 验证后 commit 或 rollback
    void commit_or_rollback(const ToolCall& call);
};
```

### 决策 2 — Pattern Mining

- 输入：过去 N 次执行的 TraceRecord
- 输出：高频工具调用模式（如 "code_search → read_file → summarize"）
- 存储：ExecutionSession 内部 PatternCache

### 决策 3 — Speculation 步数

- v2 默认 k=2（前瞻 2 步）
- Cost-Aware Speculation (CASE 2026 arXiv:2606.07846) 提供动态调整

## 实施

- 依赖: [ADR-0061-06-trajectory-ir](./adr-0061-06-trajectory-ir.md)
- 工作量: 4 weeks
- 优先级: P2

## 参考

- PASTE: arXiv:2603.18897
- Speculative Actions: arXiv:2510.04371
- DSP: arXiv:2509.01920
- CASE: arXiv:2606.07846
- Sprint 18 (execution_session.cpp)