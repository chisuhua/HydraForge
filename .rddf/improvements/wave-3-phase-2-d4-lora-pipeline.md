# Wave 3 Phase 2 D4 — LoRA Training Pipeline

**优先级**: P0 | **阶段**: Wave 3 Phase 2 (D4) | **分类**: Foundation / Training Pipeline
**类型**: feature | **主题**: LoRA 训练方法完整化 (Phase 1 D7 stub → Phase 2 D4 真实化)
**状态**: 🔍 Proposed (2026-09-25)
**生成时间**: 2026-09-25
**触发**: Wave 3 Phase 1 finetune-base-model 已 ship (commit `f0a5c4b`, 2026-09-23) — Wave 3 cooling-off 起点
**Wave 3 cooling-off**: 起点 `f0a5c4b` (2026-09-23T05:35Z) → 满点 2026-09-24T05:35Z → **EXPIRED 2026-09-25**, 可启动 Phase 2
**关联 ADR**: ADR-0078 (finetune-base-model) v1.0 ✅ Approved
**关联 SoT**: `docs/architecture/rsi-architecture-2026-09.md` §十一.6 (Phase 2 D4-D7 规划)
**关联 Wave 3 Phase 1**: `openspec/changes/archive/wave-3-finetune-base-model-pilot-phase1-2026-09-23/`

## Why (动机)

### 当前问题

1. **Wave 3 Phase 1 D7 stub 已 ship 但 Phase 2 缺口**: Phase 1 评估了 5 个 base model 候选 (`weighted_score` 评分) 并选定 `llama-3.1-70b-lora-v1` 候选，但**真实 LoRA 训练管线**只在 Phase 1 D7 stub 阶段（per `dynamic_factories_` 自注册 baseline）。Phase 2 D4 需要把 stub 替换为真实 LoRA 训练实现。

2. **Pre-Wave3 Plan §3 链式规划**: Wave 3 cooling-off 满后需独立立项 Phase 2 D4-D7 各自独立 ship。当前 Phase 1 起点已 ship，Phase 2 应立即跟进以维持 Wave 3 momentum。

3. **AGENTS.md Wave 3 cooling-off 链式规则 (AC-12)**: Wave 3 Phase 2 立项本身触发 24h cooling-off（不能连续 override，per AGENTS.md Pattern #11）。

### 现状证据

- **`openspec/changes/archive/wave-3-finetune-base-model-pilot-phase1-2026-09-23/`**: Phase 1 完整 ship
- **`docs/architecture/rsi-architecture-2026-09.md` §十一.6**: Wave 3 Phase 2 D4-D7 4 阶段规划
- **ADR-0078 §Phase 1 D7**: stub provider 已注册 (`agenticdsl-llama-3.1-70b-lora-v1`)
- **Pre-Wave3 Plan §3 (`.rddf/roadmap/2026-09-21-pre-wave3-to-wave3-execution-plan.md`)**: D4 = LoRA 训练管线完整化
- **Oracle ses_f2b923412ffeTFfBdDqQFOMdT9 DECISION C 推荐**: "启动 rdd-arch → rdd-planner 做 Wave 3 Phase 2 的 improvement 5-segment + proposal（纯文档），这本身触发独立 24h cooling-off" + "范围建议：D4-D7 不要一个 change 全包。先立 D4（LoRA 训练管线）为独立 change"

## What Changes（实施内容）

### In Scope（D4 真实化范围）

- **LoRA 训练管线实现**:
  - 替换 Phase 1 D7 stub provider 为真实 LoRA 训练实现
  - 训练方法：低秩适应 (Low-Rank Adaptation) 应用于 base model `llama-3.1-70b-lora-v1`
  - 训练数据加载 (per `IDistillationWriter` SessionWriter JSONL path)
  - Loss 计算 + 梯度下降（CPU/GPU dispatch via CUDA 11.x）
- **真实 LoRA adapter 持久化** (per `IGenomeRegistry` FilesystemGenomeRegistry 复用)
- **评测挂钩**: 训练完成后自动跑 `IEvaluator V2` BehavioralEquivalence 评估 (per ADR-0086)
- **测试覆盖**:
  - Unit tests: LoRA 矩阵初始化 / 梯度流 / adapter 序列化
  - Integration test: stub base → D4 LoRA → trained adapter → IEvaluator 链路
  - `[realllm]` tag: 真实 DeepSeek + 真实 Llama.cpp 端到端 (skipped in CI)

### Out of Scope（D4 独立，保留给后续 D5/D6/D7）

- **D5**: Full evaluation pipeline（评测指标体系完善 + cross-task consistency matrix）
- **D6**: AgenticMind 集成（回流 distillation 到 R9 体系）
- **D7**: Serving complete（Phase 2 端到端部署）

### Inherited Intelligence (from Phase 1)

- **D1 评分 yaml 修正**: Phase 1 评分算法保留（M1 fix 校准后 5/5 候选分数正确）
- **C1 LLMProviderFactory 自注册**: 替换 D7 stub 时需注意 `dynamic_factories_` 已有 baseline 注册（per Phase 1 C1 fix）

## Acceptance

### D1 LoRA 训练管线实现
- [ ] 真实 LoRA 训练代码（非 stub）
- [ ] Adapter 持久化到 `IGenomeRegistry::fork()`
- [ ] 训练 loss 收敛（unit test fixture 验证 100 iter 后 loss < 阈值）
- [ ] 训练数据从 SessionWriter JSONL 加载

### D2 测试覆盖
- [ ] Unit tests: LoRA 矩阵初始化 + 梯度流 + adapter 序列化
- [ ] Integration test: stub base → LoRA → trained adapter → IEvaluator 链路 PASS
- [ ] `[realllm]` tag tests (CI skip): 真实 DeepSeek + Llama.cpp E2E
- [ ] focused ctest 100% PASS
- [ ] 全量 ctest 247+ 零回归

### D3 评测挂钩
- [ ] 训练完成后自动 `IEvaluator V2` BehavioralEquivalence 评估
- [ ] attribution_verdict 分布合理（≥80% Attributed, ≤20% Confounded/Insufficient）

### D4 Docs drift gate
- [ ] `docs/architecture/rsi-architecture-2026-09.md` §十一.6 +1 row "Phase 2 D4 ✅ ship"
- [ ] AGENTS.md Recent Changes +1 entry
- [ ] `docs_drift_audit.py 0 DRIFT items`
- [ ] `openspec validate --strict "Change is valid"`

## Capabilities

### New Capabilities

- **真实 LoRA 训练**: 替换 Phase 1 D7 stub，完整低秩适应训练管线
- **Adapter 持久化集成**: 与 IGenomeRegistry 复用，避免引入新 contract layer
- **评测自动挂钩**: 训练→评估一键式，无需手动调用

### Modified Capabilities

- **Wave 3 Phase 1 D7 stub**: 替换为真实实现（向后兼容：保持同一 external API）

## Impact

| 维度 | 评估 |
|------|------|
| **新增** | LoRA 训练代码 (~500 LOC) + adapter 持久化集成 (~50 LOC) + 测试 (~200 LOC) |
| **修改** | Phase 1 D7 stub 实现（向后兼容 external API） |
| **测试** | Unit + integration + real-LLM (skip-guarded) |
| **API** | 外部 API 不变（per ADR-0078 v1.0） |
| **构建** | 需要 CUDA 11.x（per Pre-Wave3 Plan §3 D4 假设） |
| **风险** | 中（GPU 基础设施依赖；CI 无 GPU 时 skip，local 需真 GPU 验证） |

## Reverse Indicator (R8 per AGENTS.md)

```
[Reverse Indicator]
+ new_up: LoRA 训练管线真实化 (Phase 1 D7 stub → Phase 2 D4 真实实现)
- old_down: drop_ratio=0% (Phase 1 D7 stub 已被 Phase 2 替换, 旧 stub 行为等价新实现)
- failure_traces: N/A (planning commit, 无代码变更)
- ablation: N/A
- context_ids: N/A (无 ContextRequest 涉及)
```

## 关联文档

- **Wave 3 Phase 1**: `openspec/changes/archive/wave-3-finetune-base-model-pilot-phase1-2026-09-23/`
- **Pre-Wave3 Plan**: `.rddf/roadmap/2026-09-21-pre-wave3-to-wave3-execution-plan.md` §3
- **ADR-0078**: `docs/adr/adr-0078-finetune-base-model.md` v1.0 ✅
- **SoT**: `docs/architecture/rsi-architecture-2026-09.md` §十一.6
- **D7 stub 起点**: `src/modules/cognitive/gepa_loop.cpp` `dynamic_factories_` 自注册
- **Oracle DECISION C 推荐**: ses_f2b923412ffeTFfBdDqQFOMdT9 (2026-09-25)

## 24h cooling-off 触发记录

| 节点 | 时间 | 起算 | 满点 |
|------|------|------|------|
| Wave 3 Phase 1 merge | `f0a5c4b` 2026-09-23T05:35Z | 2026-09-23T05:35Z | 2026-09-24T05:35Z ✅ EXPIRED |
| Wave 3 Phase 2 D4 立项 (本 improvement 创建) | 2026-09-25T(now) | 2026-09-25T(now) | 2026-09-26T(now+24h) |

**冷却期合规**: 本 improvement 触发独立 24h cooling-off，AC-12 链式规则。code 实施不早于 2026-09-26。