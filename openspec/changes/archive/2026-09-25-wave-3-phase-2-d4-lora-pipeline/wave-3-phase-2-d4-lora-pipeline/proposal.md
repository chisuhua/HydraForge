# Wave 3 Phase 2 D4 — LoRA Training Pipeline — Proposal

> **Change Slug**: `wave-3-phase-2-d4-lora-pipeline`
> **Status**: 🔍 Proposed (2026-09-25)
> **Created**: 2026-09-25
> **Cooling-off**: 2026-09-25 → 2026-09-26 (24h per AGENTS.md AC-12 chain rule)
> **依赖**: `wave-3-finetune-base-model-pilot-phase1-2026-09-23` (Phase 1 ✅ shipped)
> **Wave 3 cooling-off**: Phase 1 merge `f0a5c4b` 2026-09-23T05:35Z → EXPIRED 2026-09-24T05:35Z ✅
> **关联 SoT**: `docs/architecture/rsi-architecture-2026-09.md` §十一.6
> **关联 ADR**: ADR-0078 (finetune-base-model) v1.0 ✅

---

## Why (动机)

### 当前问题

1. **Phase 1 D7 stub 不够**: `Wave 3 Phase 1 finetune-base-model-pilot-phase1-2026-09-23` 已 ship（commit `f0a5c4b`），含：
   - 5 个 base model 候选评分（D1 评分 yaml，Mi1 fix 校准后正确）
   - `LLMProviderFactory` ctor 自注册 `agenticdsl-llama-3.1-70b-lora-v1` (C1 fix)
   - **D7 stub provider**：仅注册未实现真实训练

2. **Phase 2 D4 缺口**: Pre-Wave3 Plan §3 定义 Phase 2 D4 = "LoRA 训练管线完整化"。当前 Phase 1 D7 是 stub，需要替换为真实 LoRA 训练实现。

3. **Wave 3 momentum 需维持**: Wave 3 cooling-off 已满 (2026-09-24T05:35Z EXPIRED)，Phase 2 立项已可启动。

### 现状证据

- **`openspec/changes/archive/wave-3-finetune-base-model-pilot-phase1-2026-09-23/`**: Phase 1 完整 ship (3 commits: impl 97a2abb + SHIP-with-fixes 232eb13 + merge f0a5c4b)
- **`docs/architecture/rsi-architecture-2026-09.md` §十一.6**: Wave 3 Phase 2 D4-D7 4 阶段规划
- **ADR-0078 §Phase 1 D7**: stub provider 已注册 (`agenticdsl-llama-3.1-70b-lora-v1`)
- **Pre-Wave3 Plan** §3 (`.rddf/roadmap/2026-09-21-pre-wave3-to-wave3-execution-plan.md`): D4-D7 范围
- **Oracle DECISION C 推荐** (`ses_f2b923412ffeTFfBdDqQFOMdT9` 2026-09-25):
  - 启动 rdd-arch → rdd-planner（纯文档），触发独立 24h cooling-off
  - 范围建议：D4-D7 不要一个 change 全包。先立 D4（LoRA 训练管线）为独立 change

---

## What Changes（实施内容）

### 1. 替换 Phase 1 D7 stub 为真实 LoRA 训练实现

#### 1.1 LoRA 训练管线核心

```cpp
// src/modules/training/lora_trainer.cpp
class LoRATrainer {
public:
    struct Config {
        std::string base_model_path;     // e.g., "agenticdsl-llama-3.1-70b-lora-v1"
        std::string output_dir;          // e.g., "/tmp/lora-output-<uuid>"
        int rank = 8;                    // LoRA rank (hyperparameter)
        float alpha = 16.0f;             // LoRA alpha
        float learning_rate = 1e-4f;
        int batch_size = 4;
        int max_iterations = 1000;
    };

    struct Result {
        std::filesystem::path adapter_path;
        std::vector<float> loss_history;
        agenticdsl::Result<TrainedAdapter, TrainingError> finalize();
    };

    Result train(
        const Config& config,
        const std::filesystem::path& training_data_jsonl,  // SessionWriter output
        std::stop_token cancellation_token = {});
};
```

#### 1.2 Adapter 持久化（复用 IGenomeRegistry）

```cpp
// LoRA adapter → Genome format → IGenomeRegistry::fork()
// Per ADR-0078 Phase 1 D7 + Wave 3 24h commit Phase 1 integration
auto genome_registry = FilesystemGenomeRegistry(registry_path);
auto adapter_genome = lora_trainer_result.to_genome_spec();
auto forked = genome_registry.fork(
    "lora-adapter-v1",
    parent_version,
    adapter_genome);
```

#### 1.3 评测自动挂钩

```cpp
// 训练完成后自动 IEvaluator V2 BehavioralEquivalence 评估
auto trained_adapter = registry.load("lora-adapter-v1", forked.version());
auto eval_result = agenticdsl::IEvaluator::BehavioralEquivalence::compare(
    baseline_response,
    trained_adapter_response);
```

### 2. 测试覆盖

#### 2.1 Unit tests (~5 cases)

- LoRA 矩阵初始化（rank=8, alpha=16, base_model_dim=4096 → matrix shape [4096, 8]）
- 梯度流（前向 + 反向 + 检查 weight 更新）
- Adapter 序列化（JSON ↔ Genome spec roundtrip）
- 训练 loss 收敛（100 iter synthetic data, loss < threshold）
- Adapter 持久化（fork → load → 字段一致）

#### 2.2 Integration test (~3 cases)

- Stub base → D4 LoRA 训练 → trained adapter → IEvaluator V2 评估 → attribution_verdict 分布
- 训练数据从 SessionWriter JSONL 加载（per Phase 1 IDistillationWriter path）
- CUDA 11.x dispatch（CPU fallback for sandbox without GPU）

#### 2.3 Real-LLM tests (skip-guarded, CI skip)

- `[realllm]` tag 6 cases:
  - 真实 Llama.cpp 加载 `agenticdsl-llama-3.1-70b-lora-v1` base
  - 真实 LoRA 训练 50 iter on synthetic data
  - 真实 DeepSeek adapter eval (per ADR-0078)
- CI 默认 `HYDRAFORGE_SKIP_REAL_LLM=1` skip
- 本地 `DEEPSEEK_API_KEY` + CUDA 11.x GPU 时启用

### 3. CUDA 11.x 基础设施

- `src/modules/training/CMakeLists.txt`: 检测 CUDA 11.x
- CPU fallback: 当 CUDA 不可用时自动降级（per `ext/taskflow` 模式）
- No new contract layer（N2 hard-block maintained）

---

## Impact (影响评估)

| 维度 | 评估 |
|------|------|
| N1 (main.cpp zero diff) | ✅ 保持（不修改 examples/pdk_chat_demo/main.cpp） |
| N2 (include/ zero diff) | ✅ 保持（不引入新 public API） |
| N5 (Contract layer freeze) | ✅ 保持（LoRA adapter 复用 IGenomeRegistry） |
| 新增 LOC | ~500 LOC（LoRA 训练核心） + ~50 LOC（adapter 持久化集成） + ~200 LOC（测试） |
| 构建 | CUDA 11.x optional（自动降级 CPU） |
| API | 外部 API 不变（Phase 1 D7 stub API 兼容） |
| 风险 | 中（GPU 依赖；CI 无 GPU skip，local 需真 GPU 验证） |

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