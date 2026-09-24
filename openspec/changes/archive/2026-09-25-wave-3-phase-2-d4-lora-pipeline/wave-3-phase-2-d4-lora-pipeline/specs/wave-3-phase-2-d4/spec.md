# wave-3-phase-2-d4-lora-pipeline — Spec

> **Status**: 🔍 Proposed Spec (PLACEHOLDER — 待 rdd-builder 实施阶段细化)
> **关联 Proposal**: [`../../proposal.md`](../../proposal.md)
> **关联 Design**: [`../../design.md`](../../design.md)

---

## Purpose

本规范定义 Wave 3 Phase 2 D4 LoRA 训练管线的功能需求。Phase 1 D7 stub 已 ship（commit `f0a5c4b`），本 change 将其替换为真实训练实现。

**核心命题**: LoRA 训练 → adapter 持久化 → IEvaluator V2 自动评估 → attribution_verdict 分布合理，是 Wave 3 Model-RSI 闭环的核心一环。

---

## ADDED Requirements

### Requirement: real-lora-training-pipeline

LoRA 训练管线 MUST 提供真实低秩适应训练实现（非 stub）：

- MUST 接受 Config（base_model_path, output_dir, rank=8, alpha=16, learning_rate=1e-4, batch_size=4, max_iterations=1000）
- MUST 加载训练数据（SessionWriter JSONL path）
- MUST 应用 LoRA 低秩矩阵 [base_dim, rank] × [rank, base_dim]
- MUST 计算 loss（cross-entropy per token）
- MUST 在 100 iter synthetic data 上 loss < 1e-3（收敛验证）
- MUST 支持 CUDA 11.x GPU path（cuda_dispatch）
- MUST 支持 CPU fallback（sandbox/CI 无 GPU 时自动启用）

#### Scenario: training-loss-converges

- **WHEN** LoRATrainer::train(Config{rank=8, alpha=16, base_model_dim=4096, max_iterations=100}) is called with synthetic JSONL data
- **THEN** loss after 100 iterations MUST be < 1e-3
- **AND** loss_history vector MUST have 100 entries showing monotonic decrease trend

#### Scenario: cuda-fallback-cpu

- **WHEN** AGENTICDSL_HAS_CUDA is not defined (no GPU available)
- **THEN** cuda_dispatch MUST route to CPU path
- **AND** training MUST complete with same loss convergence criteria

### Requirement: adapter-persistence-igenome-registry

训练产物 adapter MUST 持久化到 IGenomeRegistry（不引入新 contract）：

- MUST 调用 `registry.fork("lora-adapter-v1", parent_version, adapter_genome)`
- MUST 序列化 rank + alpha + weights_A + weights_B + training_data_hash 到 Genome spec
- MUST load roundtrip 一致（fork → load → 字段 byte-equal）

#### Scenario: adapter-fork-load-roundtrip

- **WHEN** LoRATrainer finalizes training → adapter_genome spec is created
- **THEN** `registry.fork("lora-adapter-v1", parent_version, adapter_genome)` MUST succeed
- **AND** `registry.load("lora-adapter-v1", forked.version())` MUST return byte-equal adapter
- **AND** weights_A + weights_B MUST roundtrip without precision loss

### Requirement: evaluator-v2-auto-hook

训练完成后 MUST 自动调用 IEvaluator V2 BehavioralEquivalence 评估：

- MUST 计算 baseline_response vs trained_adapter_response 的 attribution_verdict
- MUST 输出 attribution_verdict 分布（Attributed / Confounded / Insufficient / NotAttempted 占比）
- MUST 触发 trained_adapter_response re-generation via ChatSession 11-param ctor (per L2 spec)

#### Scenario: evaluator-attribution-distribution

- **WHEN** LoRATrainer finalizes → trained_adapter is loaded
- **THEN** IEvaluator::BehavioralEquivalence::compare(baseline_response, trained_adapter_response) MUST return verdict
- **AND** Attributed ratio MUST be ≥80% (per Pre-Wave3 Plan §3 D4 acceptance)
- **AND** Confounded + Insufficient combined MUST be ≤20%

### Requirement: training-data-from-session-writer

训练数据 MUST 来自 IDistillationWriter SessionWriter JSONL 输出：

- MUST 加载 SessionWriter JSONL path
- MUST 解析 session turn records（user_input + assistant_response pairs）
- MUST 过滤无效 / 重复 / 截断记录

#### Scenario: load-session-writer-jsonl

- **WHEN** SessionWriter writes `{session_id, turn_input, response, meta}` JSONL
- **THEN** LoRATrainer MUST load this as training pairs (turn_input → response)
- **AND** MUST skip records with missing context_id or turn_input

---

## Cross-Doc References

| 文档 | 更新内容 |
|------|---------|
| [`docs/architecture/rsi-architecture-2026-09.md`](../../../architecture/rsi-architecture-2026-09.md) §十一.6 | +1 row "Phase 2 D4 ✅ ship" |
| [`docs/adr/adr-0078-finetune-base-model.md`](../../../adr/adr-0078-finetune-base-model.md) | Phase 1 → Phase 2 status flip |
| [`AGENTS.md`](../../../AGENTS.md) Recent Changes | +1 entry (D4 ship) |
| [`docs/roadmap/2026-09-21-pre-wave3-to-wave3-execution-plan.md`](../../../roadmap/2026-09-21-pre-wave3-to-wave3-execution-plan.md) §3 | D4 status: proposed → shipped |
| [`openspec/changes/l2-evolution-deferred-follow-up/`](../l2-evolution-deferred-follow-up/) | Phase 3 cross-reference |