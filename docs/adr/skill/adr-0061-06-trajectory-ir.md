# ADR-0061-06: AgentIR-style Trajectory IR 升级 ParsedGraph

> ⛔ **Superseded by v1.1 amendment (2026-08-25, Oracle session `ses_fcba5e477ffeG9wEBHVhU64J0o`)**
>
> 本文件 (v1) 已由 [v1.1 amendment](./adr-0061-06-v1-1-amendment-trajectory-ir-decouple.md) 修订：
>
> - **标题**: "升级 ParsedGraph" → "独立序列化视图（不改 ParsedGraph）"
> - **决策 1**: TrajectoryIR = ParsedGraph 升级版 → TrajectoryIR = 独立类（Converter 单向桥接 ParsedGraph）
> - **决策 2**: 三级 IR + V1 列 4 个 Pass (ConstantFoldingPass/DCE/BudgetAwarePruningPass/PGO) → V1 仅 ConstantFoldingPass 占位（其他 3 个推迟 V2）
> - **决策 3**: Framework frontends 5 个 → V1 仅 2 个 (HydraForge DSL + 用户 YAML)
> - **决策 4**: Backends 4 个 (SFT/RL/eval/可观测性) → V1 仅 2 个 (SFT 数据 + OTel spans)
> - **实施估时**: 3 weeks → 2 sprint (T15, 与 ADR-0061-13 集成)
>
> **阅读建议**: 仅 v1.1 amendment 是当前有效设计；v1 文件保留作历史记录。
>
> **Ship 证据 (2026-08-27, T15)**: v1.1 设计已由 OpenSpec change `t15-trajectory-ir` 实施完成 — commits `3ba9f2c` (Phase 0 契约) + `53a0f17` (Phase 1 Converter) + `1fd5c4b` (Phase 2 backends/pass) + `7b24973` (Phase 3 SkillCompiler 集成); `tests/test_trajectory_ir.cpp` 9 cases / 55 assertions PASS; ParsedGraph (`src/core/types/node.h`) 零修改。

**日期**: 2026-07-16
**状态**: ⛔ Superseded (v1.1 amendment 取代，2026-08-25；v1.1 ✅ Approved + Shipped 2026-08-27)
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