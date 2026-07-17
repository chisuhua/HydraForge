# ADR-0061-06: AgentIR-style Trajectory IR 升级 ParsedGraph

**日期**: 2026-07-16
**状态**: ✅ Approved (P1, 父 ADR-0061 拆分)
**父 ADR**: [../adr-0061-agent-evolution-and-solidification.md](../adr-0061-agent-evolution-and-solidification.md)

---

## 背景

AgentIR ("LLVM for agent traces") 设计 LLVM/MLIR-style 多级 IR（RawIR → ParsedIR → Canonical），5 个 framework frontends + 用户 YAML DSL，pass pipeline，backends 落 SFT/RL/eval/observability。

HydraForge 现有 `ParsedGraph` 结构需要升级为多级 IR，以支持跨框架 trace 兼容 + pass pipeline + 多 backend 输出。

## 决策

### 决策 1 — 三级 IR

```
RawIR      →  Parser  →  ParsedIR  →  Canonicalizer  →  CanonicalIR
                                                                │
                                                                ▼
                          ┌─────────────────┬─────────────────┬──────────────┐
                          ▼                 ▼                 ▼              ▼
                       SFT 数据          RL 训练          评测数据        可观测性
```

| 级 | 形式 | 用途 |
|---|------|------|
| **RawIR** | 文本（接近 DSL） | 来源 trace / DSL 文件 |
| **ParsedIR** | 结构化 JSON/Protobuf | 标准化 schema |
| **CanonicalIR** | 规范化 JSON | pass pipeline 输入 |

### 决策 2 — Pass Pipeline

```cpp
class PassPipeline {
public:
    // 优化 pass
    std::unique_ptr<CanonicalIR> run(const CanonicalIR& input);
    
    // 内置 pass
    void add_pass(ConstantFoldingPass);
    void add_pass(DeadCodeEliminationPass);
    void add_pass(BudgetAwarePruningPass);
    void add_pass(ProfileGuidedOptimizationPass);  // Sprint 18 P1.T3 集成
};
```

### 决策 3 — Frontends（5 个 framework 兼容）

| Frontend | 来源 |
|---------|------|
| HydraForge DSL | .agent.md |
| LangGraph | Python dict |
| CrewAI | Agent YAML |
| AutoGen | Python class |
| OpenAI SDK | function calling JSON |

### 决策 4 — Backends（4 个输出）

| Backend | 用途 |
|---------|------|
| SFT 数据 | LLM 训练数据生成 |
| RL 训练 | reward model 训练数据 |
| 评测数据 | benchmark generation |
| 可观测性 | OpenTelemetry export |

## 实施

- 文件: `include/agenticdsl/ir/`, `src/modules/ir/`
- 工作量: 3 weeks
- 优先级: P1

## 参考

- AgentIR: github.com/WhitzardAgent/agentir
- LLVM/MLIR: https://llvm.org/
- Sprint 18 P1.T3 (execution_session.cpp move)
- [ADR-0061-07-paste-speculation](./adr-0061-07-paste-speculation.md)