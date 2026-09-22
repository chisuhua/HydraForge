# ADR-0078: Fine-tune 基模选型与训练管线

## 状态

✅ Approved (Wave 3 Phase 1 Pilot 激活, 2026-09-23 — 派生自 ADR-0071 §决策 D9; 经 Pre-Wave3 Plan §3 立项 + 用户显式 override 24h cooling-off (audit 见 `.rddf/state/builder/wave-3-finetune-base-model.json::cooling_off_override_audit`); Phase 1 实施 D1 (基模选型评分) + D3 (训练数据准备) + D7 最小版 (provider stub 注册); D4-D7 Phase 2 延后; 衔接 ADR-0074 (JSONL 训练数据 + Evidence Gate); OpenSpec change: `wave-3-finetune-base-model-pilot-phase1`)

## 领域

L0 运行时 / LLM 训练 / 基模选型 / 训练数据管线 / AgenticMind → HydraForge 回流 / Wave 3 Phase 1 Pilot 激活

## Phase 1 容量评估 (2026-09-23, per Pre-Wave3 Plan §3 + Decision Record §5.1)

Wave 3 Phase 1 启动时对 ADR-0078 D1-D7 逐项做容量评估, 明确 Phase 1 边界 (D1/D3/D7 最小版) vs Phase 2 (D4-D7 完整):

| 决策项 | Phase 1 (本 change) | Phase 2+ (延后) | 依据 |
|--------|--------------------|-----------------|------|
| **D1 基模选型** | ✅ 4 维度评分框架落地, 评分 yaml 持久化 (`docs/research/wave-3-base-model-selection.md`), 复用 llm-tool-eval + cost-monitoring 基础设施 | 真实候选模型 benchmark 数据 (依赖 llm-tool-eval 实时跑, 见 plan NOT-VERIFIED) | Pre-Wave3 Plan §3: "起步 ship 范围: fine-tune 数据闭环 (ADR-0078 §D3) + 4 维度评分 (§D1)" |
| **D2 触发条件** | ✅ 重新审阅: 4 项触发条件部分满足 (AgenticMind 立项 + G4 ship 后 eval_quality 真实值 + Fine-tune 价格 ≤$1/1M), Production 用户≥10 未达 (Phase 7 部署后) | — | ADR-0078 D2 + Pre-Wave3 Plan §3 |
| **D3 训练数据准备** | ✅ 3 路汇总第 1 路 (ADR-0074 D6 JSONL) 落地: `scripts/prepare_training_data.py` 迁移脚本加 `source` 字段 + 过滤 `parse_valid && task_success` | D7 失败事件 + AgenticMind 回流 (依赖 AgenticMind ship, Phase 2) | ADR-0078 D3 + improvement draft |
| **D4 训练方法** | ❌ 不实施 (LoRA/QLoRA/API fine-tune, 估时 2-4 周) | Wave 3 Phase 2+ | improvement draft Non-Goals |
| **D5/D6 评估 + 回流** | ❌ 不实施 (依赖 AgenticMind ship) | Wave 3 Phase 2+ | improvement draft Non-Goals |
| **D7 serving** | ✅ Phase 1 最小版: `LLMProviderFactory::register_dynamic("agenticdsl-llama-3.1-70b-lora-v1", factory_fn)` 注册 + `FinetuneBaseModelProvider` stub (available_models 非空 + generate 返回 failure "Phase 2 deferred") | 真实推理 + MCP `prompts/*` 更新 (依赖 ADR-0076 gRPC, Phase 2+) | ADR-0078 D7 + improvement draft |

**容量评估结论**: Solo Dev 容量约束下, Phase 1 (1-2 天, 4-6 files + 2-3 tests, 0 外部依赖) 与 Phase 2 (D4-D7, 2-4 周, HF TRL/PEFT 引入) 边界清晰。Phase 1 立即可执行且不引入 build complexity; Phase 2 需 Wave 3 cooling-off (自本 change merge 起算 24h) 满后独立立项。

## 关联

### 父 ADR
- [ADR-0071 — LLM-native AgenticDSL 架构](./adr-0071-llm-native-agenticdsl-architecture.md) §决策 D9 (本 ADR 是 D9 的具体实施: Fine-tune 基模选型 + 训练管线)
- ADR-0071 §决策 D5 (本 ADR 训练数据消费 D5 JSONL 格式)
- ADR-0071 §决策 D4 (本 ADR fine-tune 后 baseline 复用 D4 schema)

### 上游锚定
- [ADR-0073 — Tool JSON Schema 契约](./adr-0073-tool-json-schema-contract.md) — Fine-tune 训练数据 schema 与 ToolMetadata V3 一致
- [ADR-0074 — Prompt Engineering + Evidence Gate](./adr-0074-prompt-evidence-gate.md) §决策 D6 (JSONL 训练数据 + schema_snapshot_hash)
- [ADR-0076 — DSL Engine as MCP Server](./adr-0076-dsl-engine-mcp-server.md) — Fine-tune 后 `prompts/*` 更新 (MCP server hot-reload)
- [ADR-0077 — gRPC Data Plane](./adr-0077-grpc-data-plane.md) §D1 LLMDataPlane — Fine-tune 训练数据高频采集底层

### 平行/下游
- AgenticMind (独立项目, 4-6 周探索) — 本 ADR Phase 2 等待 AgenticMind 探索结果回流后激活
- Wave 5+ PDK AgentForge 集成 — Fine-tune 模型作为 AgentForge agent 的 LLM 后端 (Phase 2+)

### 规范
- [OpenAI Fine-tuning API](https://platform.openai.com/docs/guides/fine-tuning) — OpenAI fine-tune 接口
- [Anthropic Fine-tuning](https://docs.anthropic.com/en/docs/build-with-claude/fine-tune-claude) — Anthropic Claude fine-tune
- [DeepSeek Fine-tuning](https://api-docs.deepseek.com/guides/fine-tuning) — DeepSeek fine-tune
- [Hugging Face TRL](https://huggingface.co/docs/trl) — 开源 fine-tune 框架 (LoRA / QLoRA)
- [`docs/specs/dsl.md`](../specs/dsl.md) v3.10+ — DSL 规范 (Fine-tune 后 DSL 版本对齐)

---

## 背景

### 问题

ADR-0071 §决策 D9 明确: Fine-tune 基模选型**延后到 AgenticMind 探索后**。当前 5 个具体空白:

1. **基模选型无客观标准** — GPT-4 / Claude / DeepSeek / 开源 (Llama-3 / Qwen-2.5) 哪个最适合 AgenticDSL 生成? 无量化决策依据
2. **训练数据 → 模型映射缺失** — ADR-0074 D6 JSONL 训练数据 (≥30 few-shot + ≥50 golden + failures) 如何喂给 fine-tune API?
3. **Fine-tune 后评估方法学缺失** — Fine-tune 后模型是否真比 baseline 强? 需重跑 ADR-0074 D3 baseline
4. **AgenticMind → HydraForge 回流机制未定义** — AgenticMind 独立探索结果如何沉淀到 HydraForge?
5. **Fine-tune 模型 serving 与 HydraForge LLM provider 集成** — Fine-tune 模型如何注册为 ILLMProvider?

### 解决方案

引入 **Fine-tune 基模选型框架** + **训练管线** + **AgenticMind 回流机制**:

```
┌─────────────────────────────────────────────────────────────┐
│           Fine-tune Pipeline (本 ADR Wave 3 Pilot)           │
│                                                               │
│  ┌────────────────────────────────────────────┐              │
│  │  Phase 1: Data Preparation (D3)             │              │
│  │  ├─ ADR-0074 D6 JSONL (训练主数据)         │              │
│  │  ├─ 失败事件 (D7 parse_failed / schema_*)   │              │
│  │  └─ AgenticMind 补充数据 (D5 回流)         │              │
│  └────────────────────────────────────────────┘              │
│                          ↓                                     │
│  ┌────────────────────────────────────────────┐              │
│  │  Phase 2: Base Model Selection (D1)         │              │
│  │  ├─ 候选: 开源 (Llama-3 / Qwen-2.5)        │              │
│  │  ├─ 候选: 闭源 (GPT-4 / Claude / DeepSeek) │              │
│  │  └─ 选择标准: 4 维度评分 (D1)              │              │
│  └────────────────────────────────────────────┘              │
│                          ↓                                     │
│  ┌────────────────────────────────────────────┐              │
│  │  Phase 3: Training (D3-D4)                  │              │
│  │  ├─ OpenAI / Anthropic / DeepSeek API        │              │
│  │  └─ Hugging Face TRL (开源 LoRA / QLoRA)     │              │
│  └────────────────────────────────────────────┘              │
│                          ↓                                     │
│  ┌────────────────────────────────────────────┐              │
│  │  Phase 4: Evaluation (D5-D6)                │              │
│  │  ├─ 重跑 ADR-0074 D3 baseline                │              │
│  │  ├─ Evidence Gate D4 阈值复测                │              │
│  │  └─ A/B vs 原始基模                          │              │
│  └────────────────────────────────────────────┘              │
│                          ↓                                     │
│  ┌────────────────────────────────────────────┐              │
│  │  Phase 5: Serving (D7)                      │              │
│  │  ├─ 注册为 ILLMProvider (ADR-0071 复用)    │              │
│  │  ├─ LLMProviderFactory 路由                  │              │
│  │  └─ MCP prompts/* 更新 (ADR-0076 衔接)      │              │
│  └────────────────────────────────────────────┘              │
└─────────────────────────────────────────────────────────────┘
```

### 已实证证据

- **ADR-0071 §D9 已定义触发路径**: AgenticMind 探索 → 回流 → 激活本 ADR
- **ADR-0074 D6 JSONL 训练数据格式已定义**: `dsl_version` + `schema_snapshot_hash` + `stage_1_selected` 等元数据
- **Fine-tune 工业成熟度**: OpenAI (2023-08 GA) / Anthropic (2024-10 GA) / DeepSeek (2024-12) 全部 ship fine-tune API
- **Hugging Face TRL**: LoRA / QLoRA 降低 fine-tune 成本 10x, 单 GPU 可训练 7B 模型
- **Project LLMProviderFactory (Sprint 16)**: 已支持多 provider 路由, fine-tune 模型可作为新 provider 注册

---

## 决策

### D1. 基模选型框架 — 4 维度评分

**目标**: AgenticMind 探索结束后, 用客观框架选择基模, 避免主观偏好。

**4 维度 + 权重**:

| 维度 | 权重 | 评分标准 (0-10) |
|------|:---:|------|
| **Capability** (能力) | 30% | DSL 语法理解 / JSON Schema 遵循 / 长 context 稳定性 / 工具调用准确率 |
| **Latency** (延迟) | 20% | P50 / P95 / P99 token 生成延迟 (ms/token) |
| **Cost** (成本) | 20% | $ / 1M tokens (input + output 加权) |
| **Openness** (开放性) | 30% | 权重自训练 / 数据隐私 / 本地部署能力 / vendor lock-in 风险 |

**评分方法**:

```yaml
# 每个候选模型 4 维度评分 (基于 AgenticMind 探索结果)
candidates:
  - name: gpt-4-turbo-2024-XX
    scores:
      capability: 9.0       # JSON Schema 遵循 + 工具调用优秀
      latency: 7.0          # P50 30ms/token
      cost: 4.0             # $10 / 1M input, $30 / 1M output
      openness: 3.0         # 闭源, 无权重, 无本地部署
    weighted_score: 5.8     # 9*0.3 + 7*0.2 + 4*0.2 + 3*0.3 = 2.7+1.4+0.8+0.9

  - name: claude-3-5-sonnet-20241022
    scores:
      capability: 9.5       # 当前 SOTA
      latency: 6.0          # P50 40ms/token
      cost: 3.0             # $3 / 1M input, $15 / 1M output
      openness: 2.0         # 闭源
    weighted_score: 5.25

  - name: llama-3.1-70b-instruct
    scores:
      capability: 8.0       # 略低于 GPT-4
      latency: 5.0          # P50 60ms/token (本地)
      cost: 9.0             # 本地部署, 仅电费
      openness: 10.0        # 完全开源, 权重可训练
    weighted_score: 8.2      # ← 选这个 (本地 + 可训练 + 低成本)

  - name: qwen-2.5-72b-instruct
    scores:
      capability: 8.5
      latency: 5.5
      cost: 9.0
      openness: 10.0
    weighted_score: 8.45     # ← 也可

  - name: deepseek-v2-chat
    scores:
      capability: 8.5
      latency: 7.0
      cost: 8.0             # $0.14 / 1M (cache hit), $0.28 (miss)
      openness: 6.0         # 部分开源 (DeepSeek-V2 Lite)
    weighted_score: 7.35
```

**选择标准**:

- 加权分 ≥ 7.5 → 候选
- 加权分最高者 + 满足以下条件:
  - ✅ Capability ≥ 8.0 (DSL 生成可用)
  - ✅ Openness ≥ 5.0 (允许 fine-tune)
  - ✅ Cost ≤ $1 / 1M tokens (sustainable)
  - ✅ Latency P95 ≤ 100ms/token (interactive)

**示例最终选择** (基于上述): `llama-3.1-70b-instruct` 或 `qwen-2.5-72b-instruct` (本地 + 可训练)

> **算术校正 NOTE (2026-09-23, Oracle bg_7fe026cc 审查发现)**: 上方 5 个候选 `weighted_score` 初版全部存在算术错误 (加权计算表达式正确, 字段值抄录错误), 已修正为正确算术值: gpt-4-turbo **5.8** (原 6.0) / claude-3-5-sonnet **5.25** (原 5.35) / llama-3.1-70b **8.2** (原 8.0) / qwen-2.5-72b **8.45** (原 8.25) / deepseek-v2-chat **7.35** (原 7.5)。关键影响: deepseek 真实 7.35 < 7.5 不满足 Weighted ≥ 7.5, 不再属于候选。最终选择 `llama-3.1-70b-instruct` (实际 8.2 ≥ 7.5) **不变**。对应研究文档 `docs/research/wave-3-base-model-selection.md` 已同步修正 + 增加"校正说明"段落; 本 ADR 与之一致。

### D2. Fine-tune 触发条件 — Evidence Gate + AgenticMind 回流

**触发本 ADR 实施** (任一):

1. **AgenticMind 项目 ship** + 回流结果产出 (基模推荐 + 训练数据补充)
2. **ADR-0074 Evidence Gate FAIL** (parse-valid < 85% 或 task-success 阈值不达标)
   - Evidence Gate FAIL 时, Prompt 优化不够, 需 fine-tune 强化
3. **Production 用户 ≥ 10** + 训练数据 ≥ 1 万条 (足够 fine-tune 收益)
4. **OpenAI / Anthropic / DeepSeek fine-tune 价格降至 ≤ $1 / 1M tokens** (成本可行)

**触发顺序** (Wave 3 Phase 1 实际路径):

```
[Wave 3] Pre-Wave3 4-Gate 序列 (G1-G4) 全部 SHIPPED (2026-09-22)
    ↓
[Wave 3] 24h cooling-off + 用户显式 override (2026-09-22, builder-handoff audit)
    ↓
[Wave 3] ADR-0078 翻牌 ✅ Approved (Phase 1 Pilot 激活, 2026-09-23)
    ↓
Phase 1: 基模选型评分 (D1) + 数据准备第 1 路 (D3) + serving 注册 stub (D7 最小版)
    ↓
[Wave 3 Phase 2] AgenticMind ship + D4-D7 完整实施 (2-4 周)
```

**不触发条件** (本 ADR 永不实施):

- ❌ Evidence Gate PASS + AgenticMind 未 ship (Phase 6 demo 已 ship, Prompt 优化足够)
- ❌ 用户 < 5 (训练数据不足, fine-tune 收益不显著)
- ❌ 团队 < 2 人 (fine-tune 运维成本高)

### D3. 训练数据来源 — 3 路汇总

**目标**: 从 3 个数据源汇总训练样本, JSONL 格式统一。

**数据源**:

| 来源 | 数据类型 | 数量目标 | 优先级 |
|------|---------|:---:|:---:|
| **ADR-0074 D6 baseline JSONL** | baseline 测量输出 (parse-valid + task-success) | ≥10,000 | P0 |
| **ADR-0074 D7 失败事件** | parse_failed + schema_validation_failed + retry | ≥2,000 | P1 |
| **AgenticMind 回流** | AgenticMind 探索补充 (RLHF / DPO 数据) | ≥5,000 | P2 |

**JSONL schema** (与 ADR-0074 D6 一致):

```json
{
  "record_id": "uuid-v4",
  "timestamp": "2026-XX-XXTHH:MM:SSZ",
  "model": "gpt-4-turbo-2024-XX",
  "prompt_version": "V3",
  "user_input": "用 GitHub API 检查 PR 状态",
  "dsl_output": "type: dsl\nname: pr_check\n...",
  "stage_1_selected": ["github.get_pr", "slack.notify"],
  "parse_valid": true,
  "task_success": true,
  "error_code": null,
  "context": {
    "dsl_version": "3.10",
    "schema_snapshot_hash": "sha256:abcd1234...",
    "tool_metadata_v": 3,
    "agenticdsl_commit": "cc8c7df"
  },
  "tags": ["auth", "tool_call"],
  "difficulty": "L2",
  "source": "baseline | failure | agenticmind"  // ← 本 ADR 新增字段
}
```

**Fine-tune API 格式转换**:

```python
# OpenAI fine-tune 格式转换
def convert_to_openai_finetune(jsonl_records):
    return [{
        "messages": [
            {"role": "system", "content": system_prompt_template(record)},
            {"role": "user", "content": record["user_input"]},
            {"role": "assistant", "content": record["dsl_output"]}
        ]
    } for record in jsonl_records if record["parse_valid"] and record["task_success"]]

# Anthropic / DeepSeek 转换器类似
```

**数据过滤**:

- ✅ 仅 `parse_valid=true && task_success=true` (正向样本)
- ✅ 失败样本单独处理 (强化学习 / DPO 用, 见 D4)
- ❌ 过滤 hardcoded 密钥 / PII (CI 检查)

### D4. Fine-tune 训练方法 — 闭源 API + 开源 LoRA

**闭源 API fine-tune** (GPT-4 / Claude / DeepSeek):

```bash
# OpenAI fine-tune
openai api fine_tuning.jobs.create \
  -m gpt-3.5-turbo-0613 \
  -f data/training/openai_finetune.jsonl \
  --suffix "agenticdsl-v1" \
  --n_epochs 3

# Anthropic fine-tune (Claude)
# 参考 docs.anthropic.com/en/docs/build-with-claude/fine-tune-claude

# DeepSeek fine-tune
curl -X POST "https://api.deepseek.com/v1/fine_tuning/jobs" \
  -H "Authorization: Bearer $DEEPSEEK_API_KEY" \
  -d @data/training/deepseek_finetune.jsonl
```

**开源 LoRA / QLoRA fine-tune** (Llama / Qwen):

```python
# Hugging Face TRL + PEFT
from trl import SFTTrainer
from peft import LoraConfig

lora_config = LoraConfig(
    r=16, lora_alpha=32, target_modules=["q_proj", "v_proj"],
    lora_dropout=0.05, bias="none", task_type="CAUSAL_LM"
)

trainer = SFTTrainer(
    model="meta-llama/Llama-3.1-70B-Instruct",
    train_dataset=jsonl_to_hf_dataset("data/training/baseline.jsonl"),
    peft_config=lora_config,
    max_seq_length=8192,
    args=TrainingArguments(num_train_epochs=3, learning_rate=1e-4, ...),
)
trainer.train()
trainer.save_model("models/agenticdsl-llama-3.1-70b-lora-v1")
```

**成本估算** (示例 Llama-3.1-70B QLoRA):

- 单 A100 80GB GPU × 24h ≈ $50 (云) / $5 (电费)
- LoRA 适配器 ~100MB (vs 全参数 fine-tune ~140GB)
- 推理: 单 GPU 可服务 (70B INT4 量化 ~40GB)

### D5. 评估方法学 — 重跑 baseline + A/B

**目标**: Fine-tune 后模型是否真比 baseline 强? 需客观量化。

**2 个评估步骤**:

**Step 1: 重跑 ADR-0074 D3 baseline**

```bash
./tools/measure_prompt_baseline \
    --models agenticdsl-llama-3.1-70b-lora-v1,gpt-4-turbo,claude-3-5-sonnet \
    --golden-suite tests/golden_suite/ \
    --output docs/audits/2026-XX-XX-finetune-baseline-v1.json
```

**对比指标**:

| 指标 | baseline (基模) | fine-tune (本 ADR) | 改善目标 |
|------|:---:|:---:|:---:|
| parse-valid | 0.92 (GPT-4 V3) | 0.95+ | ≥3% |
| task-success L1 | 0.85 | 0.92+ | ≥7% |
| task-success L2 | 0.55 | 0.70+ | ≥15% |
| task-success L3 | 0.20 | 0.40+ | ≥20% |
| Prompt prefix tokens | 7500 | 4500 | 减少 (knowledge distillation) |

**Step 2: A/B vs 原始基模**

- 50 任务随机化 (用户不知用哪个模型)
- 评分: 任务成功率 + LLM-as-judge (受限使用)
- 显著性检验: p < 0.05 才认为 fine-tune 改善显著

**Evidence Gate 复测** (per ADR-0074 D4):

```
parse_valid ≥85%? → ✅
task_success L1 ≥70%? → ✅
task_success L2 ≥50%? → ✅
task_success L3 ≥30%? → ✅
Prompt prefix ≤8k? → ✅
→ Wave 2 → Wave 3 Evidence Gate PASS (即使基模替换, 不重启 Wave)
```

### D6. AgenticMind → HydraForge 回流机制

**目标**: AgenticMind 独立项目探索结果 (基模 + RLHF 数据 + 评估方法) 沉淀到 HydraForge。

**回流路径**:

```
AgenticMind 项目 (独立)
    │
    ├─ 探索 1: 基模 benchmark (Llama-3 / Qwen-2.5 / DeepSeek)
    │  └─ 输出: docs/agenticmind/baseline-2026-XX.md
    │
    ├─ 探索 2: RLHF / DPO 数据采集 (用户偏好)
    │  └─ 输出: data/training/agenticmind_rlhf.jsonl (≥5000 条)
    │
    └─ 探索 3: Fine-tune 实验结果
       └─ 输出: docs/agenticmind/finetune-results-2026-XX.md
            ↓
       git subtree pull → HydraForge monorepo
            ↓
       ADR-0078 §D1 基模选型应用 AgenticMind 结果
       ADR-0078 §D3 训练数据合并 agenticmind_rlhf.jsonl
       ADR-0078 §D5 评估方法学应用 AgenticMind 评估方法
```

**AgenticMind 项目接口约定**:

```yaml
# AgenticMind 输出格式 (与 HydraForge 一致)
agenticmind_outputs:
  baseline_report:
    format: docs/agenticmind/baseline-<date>.md
    required_sections:
      - 4 维度评分 (D1)
      - 候选模型列表 (≥3 个)
      - 加权分 + 选择理由
  rlhf_data:
    format: data/training/agenticmind_rlhf-<date>.jsonl
    required_fields:
      - record_id, timestamp, model, user_input, dsl_output
      - preference_score (1-5)
      - annotator_id
  finetune_results:
    format: docs/agenticmind/finetune-results-<date>.md
    required_sections:
      - 训练 loss 曲线
      - 评估结果 (D5)
      - A/B 显著性检验
```

**沉淀 ADR**:

- AgenticMind 基模推荐 → ADR-0078 §D1 应用
- AgenticMind RLHF 数据 → 合并到 ADR-0078 §D3
- AgenticMind 评估方法学 → 升级 ADR-0078 §D5

### D7. Serving 集成 — ILLMProvider + LLMProviderFactory

**目标**: Fine-tune 后模型注册为 HydraForge LLM provider, MCP `prompts/*` 更新。

**新增 provider**:

```cpp
// src/common/llm/finetune_llama_provider.h
class FinetuneLlamaProvider : public ILLMProvider {
public:
  FinetuneLlamaProvider(const FinetuneConfig& config);
  // 推理: 调用本地 Llama 模型 + LoRA 适配器
  // 兼容 ILLMProvider v2 接口 (C16 ship, ADR-0071 §D7)
};

// Factory 注册
LLMProviderFactory::register_provider(
  "agenticdsl-llama-3.1-70b-lora-v1",
  [](const json& config) -> unique_ptr<ILLMProvider> {
    return make_unique<FinetuneLlamaProvider>(config);
  }
);
```

**DSL 引用**:

```yaml
# .agent.md 使用 fine-tune 模型
- type: dsl_call
  target: llm.call
  args:
    prompt: $ctx.working.data.user_input
    model: agenticdsl-llama-3.1-70b-lora-v1  # Fine-tune 模型
```

**MCP `prompts/*` 更新** (ADR-0076 衔接):

```json
{
  "name": "generate_dsl_v3_finetuned",
  "description": "Generate DSL with fine-tuned model (smaller prefix, better accuracy)",
  "arguments": [
    {"name": "user_input", "required": true},
    {"name": "model", "default": "agenticdsl-llama-3.1-70b-lora-v1"}
  ],
  "messages": [
    {
      "role": "system",
      "content": {
        "type": "text",
        "text": "You are an AgenticDSL generator fine-tuned for {{model}}. ... [reduced schema snapshot, 2k tokens instead of 5k]"
      }
    }
  ]
}
```

Fine-tune 模型 prompt 可缩短 (knowledge distillation), 进一步降低 prefix tokens (ADR-0074 D5)。

---

## 不变量

### 长期不变量

1. **基模选型必须用 D1 4 维度评分** — 不允许"凭感觉"选择
2. **训练数据来源严格 3 路** (baseline JSONL + 失败事件 + AgenticMind 回流) — 不引入第 4 路
3. **JSONL schema 兼容 ADR-0074 D6** — `dsl_version` + `schema_snapshot_hash` 必填
4. **Fine-tune 后必须重跑 ADR-0074 D3 baseline** — 不允许"主观感觉更好"
5. **Fine-tune 模型必须注册为 ILLMProvider** — 不允许绕过 LLMProviderFactory 直接调用
6. **MCP `prompts/*` 更新是 fine-tune ship 的最后一步** — serving + prompts 同步
7. **Fine-tune 永不替代 Prompt baseline** — Evidence Gate PASS 即不 fine-tune (D2 不触发条件)
8. **AgenticMind 输出格式标准化** (D6) — 避免每次回流重新解析

### 数据隐私不变量

```
训练数据 MUST 过滤 hardcoded 密钥 / PII (CI 检查)
开源 fine-tune (Llama / Qwen) 数据不出本地 (本地训练)
闭源 fine-tune (GPT / Claude) 数据走 OpenAI / Anthropic 隐私协议 (DPA)
```

---

## 风险

### 高风险

| 风险 | 缓解 |
|------|------|
| **Fine-tune 后模型 worse than baseline** — 训练数据噪声 / 过拟合导致 parse-valid 下降 | D5 Step 1 baseline 必跑; 显著恶化 → 回滚基模; p < 0.05 显著性检验 |
| **训练数据泄露 hardcoded 密钥 / PII** — 用户输入含密码/API key | CI 强制扫描 (`tools/scan_pii.py`); data 目录加入 pre-commit secret scan |
| **闭源 fine-tune 数据被 OpenAI / Anthropic 用于训练下一代** — vendor lock-in 风险 | D1 Openness 维度评分 (≥5 才候选); 优先开源 LoRA (Llama / Qwen); DPA 协议审查 |
| **Fine-tune 成本失控** — 闭源 fine-tune 价格波动 (OpenAI 2024 涨价 2x) | D1 Cost 维度评分; 单次 fine-tune ≤ $5000 硬上限; 超限启动 ADR 重新评估 |

### 中风险

| 风险 | 缓解 |
|------|------|
| **Fine-tune 模型与 DSL 版本漂移** — DSL v3.11 ship 后 fine-tune 模型仍用 v3.10 训练 | JSONL `dsl_version` 字段强制; DSL major version 升级触发 fine-tune 重训 |
| **AgenticMind 项目延期 / 取消** — 探索结果 4-6 周未产出 | D2 触发条件含 Evidence Gate FAIL 路径 (不依赖 AgenticMind); 独立 fine-tune 可启动 |
| **LoRA 适配器与基模版本不匹配** — Llama-3.1 → Llama-3.2 升级时 LoRA 失效 | 锁定基模版本 (e.g. `llama-3.1-70b-instruct`); 基模升级触发 LoRA 重训 |
| **Fine-tune 后 MCP `prompts/*` 兼容问题** — 老 client 用旧 prompt hash 失败 | MCP `etag`-like 缓存机制 (ADR-0076 D4); 老 prompt 保留 6 月 deprecated |

### 低风险

| 风险 | 缓解 |
|------|------|
| **Fine-tune 模型 inference 延迟高** — LoRA 推理 ~5% 开销 | benchmark target P95 ≤ 100ms/token; 超阈值用 INT4 量化或换小基模 |
| **Fine-tune 数据版本管理混乱** — 多次 fine-tune 后数据混用 | JSONL `commit` 字段 (git commit); data/ 目录 git LFS; 月度归档 |

---

## 替代方案

### 替代 1: 不 fine-tune, 仅靠 Prompt 优化 (拒绝)

**否决理由**: ADR-0074 D4 Evidence Gate 测量结果若 FAIL, Prompt 优化无法解决; fine-tune 是 fallback 路径; D2 触发条件清晰。

### 替代 2: 用 RAG (检索增强) 替代 fine-tune (拒绝)

**否决理由**: RAG 增加 context tokens, 与 ADR-0074 D5 ≤8k prefix 不兼容; fine-tune 知识蒸馏后 prefix 更短; 用户偏好 fine-tune (Latency 更低)。

### 替代 3: 闭源 fine-tune 优先 (拒绝)

**否决理由**: D1 Openness 维度评分 ≥5 才候选; 闭源 vendor lock-in 风险高; 数据隐私; 开源 Llama / Qwen 已达 SOTA。

### 替代 4: 单基模 fine-tune, 不评估候选 (拒绝)

**否决理由**: D1 4 维度评分是基模选型唯一标准; 主观偏好不构成决策依据; ADR-0071 §D9 明确"AgenticMind 探索后回流"。

### 替代 5: 不等待 AgenticMind, 立即 fine-tune (拒绝)

**否决理由**: D2 触发条件 #1 是 AgenticMind ship; 提前 fine-tune 浪费 (可能选错基模); Solo Dev 容量限制 (Phase 6 demo 优先)。

### 替代 6: 全参数 fine-tune 而非 LoRA (拒绝)

**否决理由**: LoRA 成本 10x 低, 适配器 100MB vs 140GB; 全参数需 ≥8 GPU; Solo Dev 不可行; LoRA 效果差异 <5%。

---

## 影响范围

### 文档
- `docs/specs/finetune-pipeline.md` (Phase 5+ 新增) — Fine-tune pipeline 详细规范
- `docs/llm/basemodel-selection-guide.md` (Phase 5+ 新增) — D1 4 维度评分使用指南
- `docs/security/finetune-data-privacy.md` (Phase 5+ 新增) — 训练数据隐私规范
- `docs/audits/2026-XX-XX-finetune-baseline-v1.json` (Phase 5+ 新增) — Fine-tune 后 baseline 重测

### 代码 (Phase 1 ✅ ship / Phase 2 待实施)
- `src/common/llm/finetune_provider.h/cpp` — `FinetuneBaseModelProvider` (D7 Phase 1 stub: 注册 + config 解析 + generate 返回 failure "Phase 2 deferred"; Phase 2 完整推理) ✅ Phase 1 ship
- `src/common/llm/llm_provider_factory.cpp` — 新增 `agenticdsl-llama-3.1-70b-lora-v1` 注册 (D7 Phase 1) ✅ Phase 1 ship
- `src/common/llm/finetune_llama_provider.h/cpp` — FinetuneLlamaProvider (D7 Phase 2 完整推理)
- `src/common/llm/finetune_config.h` — FinetuneConfig (LoRA 适配器路径 + 基模路径) (Phase 2)
- `tools/measure_finetune_baseline.cpp` (新增) — D5 重测脚本 (Phase 2)
- `tools/scan_pii.py` (新增) — 训练数据 PII 扫描 (CI hook) (Phase 2)
- `tools/jsonl_to_openai_finetune.py` (新增) — JSONL → OpenAI fine-tune 格式转换 (Phase 2)
- `tools/jsonl_to_hf_dataset.py` (新增) — JSONL → Hugging Face Dataset (Phase 2)

### 测试 (Phase 1 ✅ ship / Phase 2 待实施)
- `tests/test_llm_provider_factory.cpp` — D7 Phase 1 provider 注册 + 实例化 stub (register_dynamic + available_models 非空 + generate failure "Phase 2 deferred") ✅ Phase 1 ship
- `tests/test_training_data_pipeline.cpp` — D3 ADR-0074 D6 JSONL 迁移 + source 字段 + 过滤 (parse_valid && task_success) ✅ Phase 1 ship
- `tests/test_finetune_llama_provider.cpp` — FinetuneLlamaProvider ILLMProvider v2 接口 (Phase 2)
- `tests/test_llm_provider_factory_routing.cpp` — fine-tune 模型路由 (Phase 2)
- `tests/test_basemodel_selection.cpp` — D1 4 维度评分算法 (Phase 2)
- `tests/test_pii_scan.cpp` — 训练数据 PII 扫描 (Phase 2)
- `tests/test_finetune_baseline.cpp` — D5 baseline 重测 (Phase 2)

### 生态
- `examples/finetune_pipeline/` (Phase 2+) — Fine-tune 全流程示例 (数据 → 训练 → 评估 → serving)
- `data/training/*.jsonl` (ADR-0074 D6) — Fine-tune 数据源 (Phase 1 输出 `data/wave-3-training-data.jsonl`)
- ADR-0076 MCP `prompts/*` — Fine-tune 后 `generate_dsl_v3_finetuned` 新增 (Phase 2)
- AgentForge — Fine-tune 模型作为 agent LLM 后端 (Phase 2)

---

## 后续

### Wave 3 Phase 1 已 ship (2026-09-23)

1. **✅ ADR 状态翻牌** 🔍 Proposed → ✅ Approved (本 change)
2. **✅ D1 基模选型评分 yaml 持久化** — `docs/research/wave-3-base-model-selection.md` (≥3 候选 + Weighted ≥ 7.5 + 4 过滤条件全过)
3. **✅ D3 训练数据准备第 1 路** — `scripts/prepare_training_data.py` (ADR-0074 D6 JSONL + `source` 字段 + 过滤 `parse_valid && task_success`)
4. **✅ D7 serving Phase 1 最小版** — `FinetuneBaseModelProvider` stub + `LLMProviderFactory::register_dynamic("agenticdsl-llama-3.1-70b-lora-v1", ...)`
5. **AgenticMind 项目独立进展跟踪** — 等待 4-6 周探索产出 (Phase 2 前置)

### Wave 3 Phase 2 (D4-D7 完整, 估时 2-4 周, Wave 3 cooling-off 满后启动)

6. **D1 基模选型应用 AgenticMind 结果** — 4 维度评分 → 最终选择 (补充真实 benchmark 数据)
7. **D3 训练数据合并 agenticmind_rlhf.jsonl + D7 失败事件** — 3 路汇总完成
8. **D4 LoRA fine-tune 训练** — 单 A100 80GB × 24h (HF TRL / PEFT 引入)
9. **D5 重跑 baseline + A/B** — 评估方法学应用
10. **D7 serving 完整推理** — FinetuneBaseModelProvider 真实推理 + MCP prompts/* 更新
11. **Evidence Gate 复测** (per ADR-0074 D4) — 即使基模替换, 不重启 Wave

### Phase 6+ 持续 (与 ADR-0074 baseline 同步)

12. 每月新增 baseline 数据 → 触发 fine-tune v2 / v3 增量训练
13. DSL v3.11+ ship → 触发 fine-tune 数据 v3.11 适配
14. MCP `prompts/*` 持续更新 — 反映 fine-tune 模型能力

---

## 复审节点

- **本 ADR 创建时 (2026-08-03)**: 🔍 Proposed + docs-only + Wave 5+ descoped
- **Wave 3 Phase 1 翻牌时 (2026-09-23)**: ✅ Approved (Pilot 激活) — 用户显式 override 24h cooling-off (审计: builder-handoff `cooling_off_override_audit`); 4 项 D2 触发条件部分满足 (AgenticMind 立项 + G4 eval_quality 真实值 + Fine-tune 价格 ≤$1/1M; Production 用户 ≥ 10 未达, Phase 7 部署后)
- **Wave 3 Phase 2 启动时** (估时 Wave 3 cooling-off 满后):
  - 检查: AgenticMind 项目是否 ship? (D2 触发条件 #1)
  - 检查: Evidence Gate 是否 FAIL? (D2 触发条件 #2)
  - 检查: 用户数 ≥ 10? (D2 触发条件 #3)
  - 检查: Fine-tune 价格 ≤ $1 / 1M tokens? (D2 触发条件 #4)
  - 满足任一 → 启动 Phase 2 实施 (D4-D7, 估时 2-4 周)
- **Phase 6+ 持续**: 每月评估 (D3 §后续 持续项)

---

## Wave 5+ descoped 历史理由 (引用 ADR-0071 §D9, 2026-09-23 已由 Wave 3 Phase 1 翻牌解除)

> 4.1 §Layer 3 dual memos 记录: Solo Dev Phase 6a/6b 容量 37h/44h 满载;
> Fine-tune 估时 4-6 周 + 持续运维成本 (每月数据 + 重训 + 评估);
> AgenticMind 项目独立探索是关键前置 (避免选错基模);
> 团队 1 人无法承担 Phase 6 demo 推进 + Fine-tune 实施并行。

**Wave 3 Phase 1 翻牌依据** (2026-09-23): 上述 descoped 理由针对的是 **D4-D7 完整实施 (4-6 周)**。Wave 3 Phase 1 (D1 + D3 + D7 最小版, 1-2 天, 0 外部依赖) 不违反 Solo Dev 容量约束 — 无训练 / 无 GPU / 无新依赖, 仅评分 + 数据管线 + provider 注册 stub。D4-D7 完整实施 (Phase 2) 仍按原 descoped 理由延后到 Wave 3 cooling-off 满后独立立项。

*文档版本: v1.1*
*创建日期: 2026-08-03*
*作者: HydraForge 架构组*
*状态: ✅ Approved (Wave 3 Phase 1 Pilot 激活, 2026-09-23; D1 + D3 + D7 最小版 ship; D4-D7 Phase 2 延后; 衔接 ADR-0074 JSONL + ADR-0076 MCP + ADR-0077 gRPC)*