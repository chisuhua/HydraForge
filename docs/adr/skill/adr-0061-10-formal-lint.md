# ADR-0061-10: λ_A-style Config 结构完整性检查

**日期**: 2026-07-16
**状态**: 🔍 Proposed (P2, v2 候选)
**父 ADR**: [../adr-0061-agent-evolution-and-solidification.md](../adr-0061-agent-evolution-and-solidification.md)

---

## 背景

λ_A (arXiv:2604.11767) 给 5 个主流 framework (LangGraph / CrewAI / AutoGen / OpenAI SDK / Dify) 一阶抽象，Coq 1519 行 42 定理 0 Admitted。**835 真实 GitHub config 中 94.1% 在 λ_A 语义上结构不完整**。

## 决策

### 决策 1 — HydraForge Agent Config Lint

- 输入：pdk_manifest.json + AgentDescriptor + .agent.md
- 输出：结构完整性报告（缺哪些必需字段 + Coq-style 定理证明）

### 决策 2 — 必查项

| 检查项 | 来源 |
|--------|------|
| `entry_tool` 必须在 `provided_tools` 中 | ADR-0053 |
| `input_schema` 与 `output_schema` 兼容 | ADR-0058 |
| Skill 形态 `requires_isolation = true` | ADR-0055 |
| Capability tags 必填 | ADR-0054 |
| `activation_events` 引用存在的工具/Agent | ADR-0057 |
| Wasm 形态 `allowed_host_functions` 白名单 | ADR-0056 |

### 决策 3 — 集成方式

```cpp
class AgentConfigLinter {
    std::vector<LintError> lint(const pdk_manifest& m);
    bool is_complete(const pdk_manifest& m);  // 100% 通过
};
```

CI 阶段强制运行 `is_complete() == true` 才允许 ship。

## 实施

- 依赖: 所有前置 ADR (0052-0058)
- 工作量: 2 weeks
- 优先级: P2

## 参考

- λ_A: arXiv:2604.11767
- Lean4Agent: arXiv:2606.06523
- Oroboro: arXiv:2509.20364