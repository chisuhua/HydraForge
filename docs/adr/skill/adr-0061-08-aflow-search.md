# ADR-0061-08: AFlow-style MCTS 工作流搜索

**日期**: 2026-07-16
**状态**: ✅ Approved (P2, v2 候选) — **✅ V1 Shipped 2026-08-28** (Sprint 24 T20, OpenSpec change `t20-aflow-mcts`: `include/agenticdsl/cognitive/mcts_workflow_search.h` + `src/modules/cognitive/mcts_workflow_search.cpp` + `tests/test_mcts_workflow_search.cpp` **17 cases / 65 assertions PASS**; 评审通过 2026-08-28; V1 边界: Mock 5 轴模板实例化 (不触发真实 LLM) + UCB1 MCTS (选择/扩展/模拟/反向传播) + IEvaluator V2 (CompositeEvaluator) 奖励 + BehavioralRegressionGate 回归门 + MutationGovernor L1 workflow variants 授权 + 4 个 `mcts.*` 事件发射 (ADR-0068 附录 A v1.5); 既有 5 契约零修改; AFlow 改进 + L2+ mutation variants deferred V2; ctest 动态基线 0 回归)
**父 ADR**: [../adr-0061-agent-evolution-and-solidification.md](../adr-0061-agent-evolution-and-solidification.md)

---

## 背景

AFlow (ICLR 2025 Oral, arXiv:2410.10762)：MCTS 在代码表示的工作流空间内自动发现 workflow，6 个 benchmark 上比手设计 +5.7%、比自动化基线 +19.5%。

## 决策

### 决策 1 — MCTS 状态空间

- 节点：现有 5 轴模板的实例化
- 边：模板组合规则
- 奖励：regression test pass rate

### 决策 2 — 集成方式

```cpp
class MCTSWorkflowSearch {
    std::unique_ptr<WorkflowGraph> search(
        const TaskSpec& spec,
        int max_iterations = 100
    );
};
```

### 决策 3 — 何时使用

- Skill 演化出现稳定 pattern 后触发
- v1 不做（v2 引入）

## 实施

- 依赖: [ADR-0061-03-skill-compiler](./adr-0061-03-skill-compiler.md), [ADR-0061-02-behavioral-regression](./adr-0061-02-behavioral-regression.md)
- 工作量: 4 weeks
- 优先级: P2

## 参考

- AFlow: arXiv:2410.10762 (ICLR 2025 Oral)
- ADAS: arXiv:2408.08435
- DeepWisdom AFlow repo: github.com/FoundationAgents/AFlow