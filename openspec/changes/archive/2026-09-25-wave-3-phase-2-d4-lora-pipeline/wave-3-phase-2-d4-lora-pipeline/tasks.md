# Wave 3 Phase 2 D4 — LoRA Training Pipeline — Tasks

> **Status**: 🔍 Proposed (2026-09-25)
> **Cooling-off**: 2026-09-25 → 2026-09-26 (24h per AGENTS.md AC-12 chain rule)
> **关联**: `wave-3-finetune-base-model-pilot-phase1-2026-09-23` (Phase 1 ✅ shipped)

---

## Task Groups (TDD 5 步结构, per AGENTS.md Pattern #11)

### Phase A: Pre-flight (TDD 起步前准备)

#### A1 [GREEN] CUDA 11.x 检测 + CPU fallback 框架 (~1h)
- [ ] `src/modules/training/CMakeLists.txt`: `find_package(CUDA 11.0 REQUIRED)` + `AGENTICDSL_HAS_CUDA` macro
- [ ] `cuda_dispatch.h/cpp`: `#ifdef AGENTICDSL_HAS_CUDA` 分发
- [ ] 验证：sandbox 无 CUDA → 自动 CPU fallback（per `ext/taskflow` pattern）

#### A2 [GREEN] 复现 Phase 1 D7 stub 起点 (~30min)
- [ ] 读 `src/modules/cognitive/gepa_loop.cpp` D7 stub 实现
- [ ] 验证 stub API（外部接口与 Phase 2 D4 一致，避免破坏 ABI）

---

### Phase B: 核心实现（TDD Red-Green-Refactor）

#### B1 [RED] Unit test: LoRA 矩阵初始化 (~30min)
- [ ] `tests/test_lora_trainer.cpp` Case 1: rank=8, alpha=16, base_model_dim=4096 → matrix shape [4096, 8]
- [ ] 验证 FAIL（lora_matrix.h 未实现）

#### B2 [GREEN] lora_matrix.h 实现 (~1h)
- [ ] `src/modules/training/lora_matrix.h`: LoRA matrix A [base_dim, rank] + B [rank, base_dim] init + forward
- [ ] alpha/rank scaling
- [ ] 验证 Case 1 PASS

#### B3 [RED] Unit test: 梯度流 (~30min)
- [ ] Case 2: 前向 + 反向 + weight update 一致
- [ ] 验证 FAIL

#### B4 [GREEN] gradient computation (~1h)
- [ ] `lora_matrix.cpp`: backward (L2) gradient
- [ ] 验证 Case 2 PASS

#### B5 [RED] Unit test: Adapter 序列化 (~30min)
- [ ] Case 3: JSON ↔ Genome spec roundtrip (rank/alpha/weights_A/weights_B/training_data_hash)
- [ ] 验证 FAIL

#### B6 [GREEN] adapter_serialize.h (~1h)
- [ ] `adapter_serialize.h`: `to_genome_spec()` + `from_genome_spec()`
- [ ] 验证 Case 3 PASS

#### B7 [RED] Unit test: Loss 收敛 (~30min)
- [ ] Case 4: 100 iter synthetic data, loss < 1e-3
- [ ] 验证 FAIL

#### B8 [GREEN] LoRATrainer train loop (~2h)
- [ ] `lora_trainer.cpp`: `LoRATrainer::train()` 实现（iteration loop + loss accumulation）
- [ ] `lora_train_cpu()` + `lora_train_cuda()` 分发
- [ ] 验证 Case 4 PASS（100 iter 后 loss < 1e-3）

#### B9 [RED] Unit test: 持久化 fork/load 一致 (~30min)
- [ ] Case 5: fork adapter → load adapter → 字段一致
- [ ] 验证 FAIL

#### B10 [GREEN] adapter persistence hook (~1h)
- [ ] `registry_filesystem.cpp`: `fork("lora-adapter-v1", parent_version, adapter_genome)` 集成
- [ ] 验证 Case 5 PASS

---

### Phase C: Integration tests

#### C1 [RED] Integration test: End-to-end pipeline (~30min)
- [ ] `tests/test_lora_integration.cpp` Case 1: stub base → D4 LoRA 训练 → trained adapter → IEvaluator V2
- [ ] 验证 FAIL

#### C2 [GREEN] IEvaluator V2 自动挂钩 (~1h)
- [ ] `lora_trainer.cpp`: 训练完成后自动调 BehavioralEquivalence
- [ ] attribution_verdict 分布 ≥80% Attributed, ≤20% Confounded/Insufficient
- [ ] 验证 Case 1 PASS

#### C3 [RED] Integration test: SessionWriter JSONL 加载 (~30min)
- [ ] Case 2: IDistillationWriter 输出 → LoRATrainer 训练数据加载
- [ ] 验证 FAIL

#### C4 [GREEN] training data loader (~1h)
- [ ] `lora_trainer.cpp`: `load_training_data(jsonl_path)` 实现
- [ ] 验证 Case 2 PASS

#### C5 [RED] Integration test: CUDA dispatch (~30min)
- [ ] Case 3: CPU fallback 自动启用（无 GPU 环境）
- [ ] 验证 FAIL（dispatch 未实现）

#### C6 [GREEN] CPU fallback dispatch (~30min)
- [ ] `cuda_dispatch.cpp`: CPU path 完整实现
- [ ] 验证 Case 3 PASS

---

### Phase D: Real-LLM skip-guarded tests

#### D1 [GREEN] [realllm] test scaffolding (~1h)
- [ ] `tests/test_lora_real_llm.cpp`: 6 cases scaffolded + skip-guarded
- [ ] Per AGENTS.md §REAL LLM TESTING: `require_real_llm_env()` + `real_llm_env_skipped()` helper 三态分离
- [ ] Recording Provider 守卫 (per Pattern §5)

#### D2-D7 [GREEN] 6 [realllm] cases (~2h)
- [ ] Case 1: 真实 Llama.cpp 加载 `agenticdsl-llama-3.1-70b-lora-v1`
- [ ] Case 2: 真实 LoRA 训练 50 iter on synthetic data
- [ ] Case 3: 真实 DeepSeek adapter eval (per ADR-0078)
- [ ] Case 4-6: Adapter 持久化 + load + IEvaluator
- [ ] CI skip 默认 (`HYDAGR_SKIP_REAL_LLM=1`)

---

### Phase E: Ship & Archive (~30min)

#### E1 [GREEN] Oracle Stage 2 review (~30min)
- [ ] 派 Oracle 后台审查
- [ ] 收集 SHIP / SHIP-with-fixes / BLOCK verdict

#### E2 [GREEN] Stage 3 atomic commit on worktree (如 Major fixes)
- [ ] 不 amend baseline commit
- [ ] 含 `[Reverse Indicator]` 5-field block

#### E3 [GREEN] Stage 4 final Oracle verdict (~30min)
- [ ] 收集 APPROVE / REJECT verdict

#### E4 [GREEN] Archive + SoT sync (~30min)
- [ ] `openspec archive wave-3-phase-2-d4-lora-pipeline --yes`
- [ ] `docs/architecture/rsi-architecture-2026-09.md` §十一.6 +1 row
- [ ] `docs/adr/adr-0078-finetune-base-model.md` Phase 1 → Phase 2
- [ ] `AGENTS.md` Recent Changes +1 entry

---

### Phase F: 零回归验证

#### F1 [GREEN] focused ctest 100% PASS
- [ ] `ctest -L training` 100% PASS

#### F2 [GREEN] 全量 ctest ≥247/247 零回归
- [ ] `ctest --output-on-failure` ≥247/247
- [ ] Per Phase 1 baseline 211 + L2 39 + Phase 2 D4 ~8 = ≥258

---

## Acceptance

- [ ] **D1**: 真实 LoRA 训练代码（非 stub），adapter 持久化，训练 loss 收敛
- [ ] **D2**: Unit + integration + real-LLM (skip-guarded) 全覆盖，focused ctest 100%
- [ ] **D3**: 训练完成后自动 IEvaluator V2 BehavioralEquivalence 评估，attribution_verdict ≥80% Attributed
- [ ] **D4**: `docs/architecture/rsi-architecture-2026-09.md` §十一.6 +1 row, `docs_drift_audit.py 0 DRIFT`, `openspec validate --strict "Change is valid"`

---

## Reverse Indicator (R8 per AGENTS.md)

```
[Reverse Indicator]
+ new_up: LoRA 训练管线真实化 (Phase 1 D7 stub → Phase 2 D4 真实实现)
- old_down: drop_ratio=0% (Phase 1 D7 stub 已被 Phase 2 替换, 旧 stub 行为等价新实现)
- failure_traces: N/A (planning commit, 无代码变更)
- ablation: N/A
- context_ids: N/A (无 ContextRequest 涉及)
```

---

## Reference

- Wave 3 Phase 1: `openspec/changes/archive/wave-3-finetune-base-model-pilot-phase1-2026-09-23/`
- Pre-Wave3 Plan: `.rddf/roadmap/2026-09-21-pre-wave3-to-wave3-execution-plan.md` §3
- ADR-0078: `docs/adr/adr-0078-finetune-base-model.md` v1.0 ✅
- SoT: `docs/architecture/rsi-architecture-2026-09.md` §十一.6
- Oracle DECISION C: ses_f2b923412ffeTFfBdDqQFOMdT9 (2026-09-25)
- AGENTS.md Pattern #11 (v4 workflow), AC-12 chain rule