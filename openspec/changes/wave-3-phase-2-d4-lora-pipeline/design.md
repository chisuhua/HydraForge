# Wave 3 Phase 2 D4 — LoRA Training Pipeline — Design

> **Change Slug**: `wave-3-phase-2-d4-lora-pipeline`
> **Status**: 🔍 Proposed Design

---

## D1: 架构总览

```
┌─────────────────────────────────────────────────────────────────┐
│                  LoRA Training Pipeline (D4)                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│   ┌──────────────┐     ┌─────────────────┐     ┌──────────┐   │
│   │ SessionWriter │────▶│  LoRATrainer    │────▶│ Genome   │   │
│   │ JSONL Output  │     │  (真实训练)     │     │ Registry │   │
│   │ (Phase 1)    │     │                 │     │ (reuse)  │   │
│   └──────────────┘     └─────────────────┘     └──────────┘   │
│                              │                       │         │
│                              │ CUDA/CPU               │ load     │
│                              ▼ dispatch              │         │
│                       ┌─────────────────┐           │         │
│                       │  base model     │           │         │
│                       │  llama-3.1-70b  │           │         │
│                       │  -lora-v1       │           │         │
│                       └─────────────────┘           ▼         │
│                                                ┌──────────┐    │
│                                                │ Trained  │    │
│                                                │ Adapter  │    │
│                                                └──────────┘    │
│                                                      │         │
│                                                      │ eval    │
│                                                      ▼         │
│                                            ┌─────────────────┐ │
│                                            │ IEvaluator V2   │ │
│                                            │ Behavioral-      │ │
│                                            │ Equivalence      │ │
│                                            │ (per ADR-0086)   │ │
│                                            └─────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

## D2: 文件结构

### 新增文件 (~750 LOC)

```
src/modules/training/
├── CMakeLists.txt                # NEW: CUDA 11.x detection + CPU fallback
├── lora_trainer.h                # NEW: LoRATrainer class + Config + Result
├── lora_trainer.cpp              # NEW: 真实 LoRA 训练 (~400 LOC)
├── lora_matrix.h                 # NEW: 低秩矩阵初始化 + 梯度 (~80 LOC)
├── cuda_dispatch.h               # NEW: CUDA 11.x dispatch (~50 LOC)
├── cuda_dispatch.cpp             # NEW: ~150 LOC
└── adapter_serialize.h           # NEW: Adapter ↔ Genome spec (~70 LOC)

tests/
├── test_lora_trainer.cpp         # NEW: 5 unit tests (~150 LOC)
├── test_lora_integration.cpp     # NEW: 3 integration tests (~120 LOC)
└── test_lora_real_llm.cpp        # NEW: 6 [realllm] skip-guarded (~180 LOC)
```

### 修改文件 (~100 LOC diff)

```
src/modules/cognitive/gepa_loop.cpp   # Phase 1 D7 stub → 真实 D4 LoRA wiring
src/core/genome/registry_filesystem.cpp # adapter 持久化 hook (optional)
examples/pdk_chat_demo_evolution/      # L2 reference example 可选集成 (D7 follow-up)
```

## D3: 关键设计决策

### D3.1 LoRA rank / alpha 默认值

- **rank=8, alpha=16**: 与主流 LoRA 论文 (Microsoft LoRA paper) 一致
- **alpha/rank=2.0**: standard scaling factor
- **可配置**: Config 暴露给调用方，本 change 默认值不变

### D3.2 CUDA / CPU dispatch

```cpp
// cuda_dispatch.cpp
#ifdef AGENTICDSL_HAS_CUDA
    // CUDA 11.x path (GPU)
    return lora_train_cuda(config, data, cancellation);
#else
    // CPU fallback (sandbox/CI no GPU)
    return lora_train_cpu(config, data, cancellation);
#endif
```

**优先级**: CUDA > CPU；无 GPU 时 CPU 自动启用（per `ext/taskflow` pattern）

### D3.3 Adapter 持久化（复用 IGenomeRegistry）

**避免引入新 contract**（N5 hard-block）：

```cpp
// 复用 IGenomeRegistry::fork() + load() (Phase 1 已 ship)
Genome adapter_genome;
adapter_genome.spec["lora_rank"] = rank;
adapter_genome.spec["lora_alpha"] = alpha;
adapter_genome.spec["lora_weights_A"] = weights_A;  // [base_dim, rank]
adapter_genome.spec["lora_weights_B"] = weights_B;  // [rank, base_dim]
adapter_genome.metadata.training_data_hash = sha256(training_data);
auto forked = registry.fork("lora-adapter-v1", parent_version, adapter_genome);
```

### D3.4 IEvaluator V2 自动挂钩

训练完成后自动跑 BehavioralEquivalence：

```cpp
auto eval_result = IEvaluator::BehavioralEquivalence::compare(
    baseline_response,            // Phase 1 已 ship
    trained_adapter_response);    // D4 训练产物
// attribution_verdict 分布合理 (≥80% Attributed, ≤20% Confounded)
```

**why 自动**: Wave 3 Pre-Plan §3 明确要求 "训练 + 评估一键式"，避免手动调用差异。

## D4: 测试策略

### D4.1 Unit tests (~5 cases, mock-based)

- 矩阵初始化 shape 正确
- 梯度流（前向+反向+weight update 一致）
- Adapter 序列化 roundtrip（JSON ↔ Genome spec）
- Loss 收敛（100 iter synthetic data, loss < 1e-3）
- 持久化 fork/load 字段一致

### D4.2 Integration tests (~3 cases, real components)

- End-to-end pipeline：stub base → D4 LoRA → trained adapter → IEvaluator
- SessionWriter JSONL → 训练数据加载
- CUDA dispatch（sandbox 自动 CPU fallback）

### D4.3 [realllm] skip-guarded tests (~6 cases)

- CI 默认 `HYDRAFORGE_SKIP_REAL_LLM=1` skip
- 本地 `DEEPSEEK_API_KEY` + CUDA 11.x GPU 启用
- 真实 Llama.cpp + 真实 DeepSeek 端到端
- **Per AGENTS.md §REAL LLM TESTING**: helper 三态分离 + Recording Provider 守卫

### D4.4 零回归

- focused ctest: `ctest -L training` 100% PASS
- 全量 ctest: ≥247/247 维持（per Phase 1 baseline 211 + L2 39 + Phase 2 D4 ~8）

## D5: 部署 / 风险

### D5.1 GPU 依赖

- **Local dev**: 需 CUDA 11.x GPU（RTX 30 系及以上）
- **CI sandbox**: 无 GPU → CPU fallback → [realllm] skip
- **生产**: Wave 3 Phase 2 D7 serving 部署后才有 GPU cluster

### D5.2 训练数据质量

- Phase 1 IDistillationWriter 输出 SessionWriter JSONL
- D4 训练数据来源：captured training-mode sessions (per `pdk_chat_demo_evolution` capture-mode=Training)
- **风险**: 训练数据不足时 loss 收敛慢 → 需 ≥ 100 session 才稳定收敛

### D5.3 Model 选型继承

- Phase 1 D1 评分 yaml (Mi1 fix 校准后) 选定 `llama-3.1-70b-lora-v1`
- Phase 2 D4 不重新评估（保持 momentum）
- 如果 Phase 2 训练效果不佳 → 后续 D5 evaluation 阶段可触发"重新评估 base model"

## D6: 继承 Intelligence (from Phase 1 + 模式)

- **Phase 1 C1 fix**: `LLMProviderFactory` ctor 自注册 → Phase 2 D4 替换 D7 stub 时**保留**自注册 baseline
- **AGENTS.md Pattern #1 step 4**: 系统性记录同类潜伏站点（训练失败模式记录）
- **AGENTS.md Pattern #4 SHIP-with-fixes**: Stage 1 baseline → Stage 2 Oracle → Stage 3 fix → Stage 4 final
- **AGENTS.md Pattern #11 v4 workflow**: rdd-arch → rdd-planner → rdd-builder → rdd-verifier
- **AGENTS.md Reverse Indicator Rule**: commit message 含 5-field block
- **AGENTS.md AC-12 chain rule**: 24h cooling-off after change creation/approval
- **AGENTS.md Single-Dev mode**: author = reviewer = approver, 需 Oracle 自审

## D7: 与 Phase 2 D5/D6/D7 的边界

| D | 内容 | 立项时机 |
|---|------|---------|
| D4 (本 change) | LoRA 训练管线真实化 | 本 change |
| D5 | Full evaluation pipeline | D4 ship 后独立 change |
| D6 | AgenticMind 集成 (distillation 回流 R9) | D4 + D5 ship 后独立 change |
| D7 | Serving complete (端到端部署) | D4 + D5 + D6 全 ship 后独立 change |

**不打包**: Oracle DECISION C 明确建议"D4-D7 不要一个 change 全包"。

## D8: Cross-Doc 引用

| 文档 | 更新内容 |
|------|---------|
| `docs/architecture/rsi-architecture-2026-09.md` §十一.6 | +1 row "Phase 2 D4 ✅ ship" |
| `docs/adr/adr-0078-finetune-base-model.md` | Phase 1 → Phase 2 状态更新 |
| `AGENTS.md` Recent Changes | +1 entry (D4 ship) |
| `docs/roadmap/2026-09-21-pre-wave3-to-wave3-execution-plan.md` §3 | D4 status: proposed → shipped |
| `openspec/changes/l2-evolution-deferred-follow-up/` | Phase 3 wave3-phase2-d4-d7 cross-reference |