# Design: Wave 3 Fine-tune Base Model Pilot Phase 1

> **关联 proposal**: [proposal.md](./proposal.md)
> **关联 spec**: [wave-3-finetune-base-model](./specs/wave-3-finetune-base-model/spec.md)
> **创建**: 2026-09-23
> **依据**: ADR-0078 (翻牌) + improvement draft `wave-3-finetune-base-model.md` (5-segment) + Pre-Wave3 Plan §3 + builder-handoff `llm_preflight::concerns`

---

## D1. 基模选型 — 4 维度评分框架落地 (文档性)

### 决策 D1-1: 评分基础设施复用

复用既有 `tests/test_llm_tool*` (评分) + `tests/test_cost_tracking_decorator` (cost 数据) — 不新增 test binary (D1 是文档性产出).

### 决策 D1-2: 评分 yaml 持久化格式

`docs/research/wave-3-base-model-selection.md` 内嵌 yaml block:
- `candidates:` 列表 (≥3 候选模型, 每项 4 维度 scores + weighted_score)
- `filters:` 4 过滤条件验证结果
- `selection:` 最终选择 + 论证

### 决策 D1-3: 评分数据来源

- Capability: llm-tool-eval 结果 (ADR-0074 D3 baseline 已有 3 模型 persona 数据) + 公开 benchmark
- Latency: llm-tool-eval 延迟数据 + 公开 API 文档
- Cost: cost_tracking_decorator 记录 + 公开定价
- Openness: 模型权重开放度 + 本地部署能力 + vendor lock-in 评估
- **NOT-VERIFIED**: 真实 benchmark 数据依赖 llm-tool-eval 实时跑, 不在本 change 范围 (plan NOT-VERIFIED 登记)

---

## D3. 训练数据准备 — ADR-0074 D6 JSONL 迁移

### 背景: ADR-0074 V1 实际 schema vs ADR-0078 假设字段不匹配

- **ADR-0074 V1 实际产出** (`tools/prompt/export_training_data.py`): 每行 `{prompt, response, reward, metadata{task_id, domain, difficulty}}`
- **ADR-0078 假设字段**: `record_id, timestamp, model, prompt_version, user_input, dsl_output, stage_1_selected, parse_valid, task_success, error_code, context{dsl_version, schema_snapshot_hash, ...}, tags, difficulty, source`

### 决策 D3-1 (决策点 #1/#2): D3 schema migration 选项 (A/B/C)

| 选项 | 描述 | 优 | 劣 |
|---|---|---|---|
| **A** | 改造 `tools/prompt/export_training_data.py` 扩展 schema, 加 `source` + ADR-0078 假设字段 | 满足 ADR-0078 治理意图 | 需要 schema migration 测试 + backward compat, 复杂度高 |
| **B** | 接受 ADR-0074 V1 实际 schema, Wave 3 微调 ADR-0078 ADR | 简单, 低风险 | 失去 ADR-0078 部分治理价值 |
| **C** | 双 schema 并存: 训练用 ADR-0074 V1, 评估用 ADR-0078 假设字段 | 解耦, 灵活 | 增加 complexity, 需同步维护两套 schema |

**敲定**: 待 dual-agent review 阶段 (Step 2.7)。设计默认推荐 **选项 A** (满足 ADR-0078 治理 + 添加 `source` 字段 + 兼容 ADR-0074 V1 既有数据), 但实施策略**不直接改 export_training_data.py** (避免破坏 T21 既有 golden-export 测试 `jsonl_export_schema_compliance`), 而是**新建独立迁移脚本** `scripts/prepare_training_data.py` 消费 ADR-0074 D6 JSONL 作为输入 → 加 `source` + 过滤 → 输出新文件. 该策略满足选项 A 的治理意图 (训练数据带 source 字段) 同时零破坏既有管线.

### 决策 D3-2: 迁移脚本 CLI 契约

```bash
python3 scripts/prepare_training_data.py \
  --input data/adr-0074-d6-baseline.jsonl \
  --output data/wave-3-training-data.jsonl \
  --source baseline
```

- `--input` (default `data/adr-0074-d6-baseline.jsonl`): ADR-0074 D6 JSONL 输入
- `--output` (default `data/wave-3-training-data.jsonl`): 输出
- `--source` (default `baseline`, 可选 `failure`/`agenticmind`): source 字段值
- 每行加 `source` 字段 (幂等: 已存在则不覆盖)
- 过滤 `parse_valid == true && task_success == true` (缺失字段视为 true — 兼容 ADR-0074 V1 实际 schema `{prompt, response, reward, metadata}`)
- 输出文件存在 + records ≥ 1

### 决策 D3-3: `source` 字段语义

| source | 数据源 | Phase 1 实现 |
|---|---|---|
| `baseline` | ADR-0074 D6 baseline JSONL | ✅ 本 change |
| `failure` | ADR-0074 D7 失败事件 | Phase 2 (D7 事件主题注册已 ship, 数据回收延后) |
| `agenticmind` | AgenticMind 回流 | Phase 2 (依赖 AgenticMind ship) |

### 决策 D3-4: 测试策略

`tests/test_training_data_pipeline.cpp` (Catch2, `std::system("python3 ...")` 模式同 test_prompt_evidence_gate):
- Case 1: `prepare_training_data loads ADR-0074 D6 JSONL and adds source field` — 输入 fixture (临时文件) → 输出存在 + 每行含 `source=baseline`
- Case 2: `prepare_training_data filters parse_valid && task_success` — 过滤后 records 数 ≥ 1 且全部满足条件
- CMake: WORKING_DIRECTORY 需 repo root (python 脚本相对路径), 同 `test_prompt_evidence_gate` 模式

---

## D7. Serving 集成 Phase 1 最小版 — FinetuneBaseModelProvider stub

### 背景: ILLMProvider v2 接口 (C16 ship, ADR-0042)

```cpp
class ILLMProvider {
  virtual ~ILLMProvider() = default;
  virtual Result<GenerationResult, LLMError> generate(const GenerationRequest&, std::stop_token) = 0;
  virtual std::unique_ptr<IGenerationStream> generate_stream(const GenerationRequest&, std::stop_token) = 0;
  virtual std::vector<ModelInfo> available_models() const = 0;
};
```

### 决策 D7-1 (决策点 #3): LLMProviderFactory API 名称勘误

- **实际 API**: `LLMProviderFactory::register_dynamic(name, DynamicFactoryFn)` (per `src/common/llm/llm_provider_factory.h:33`)
- **improvement draft AC-6 笔误**: `register_provider`
- **敲定**: 使用 `register_dynamic` (实际 API), 本 change 所有 artifacts 统一
- **NOTE**: `DynamicFactoryFn = std::function<std::unique_ptr<ILLMProvider>(const LLMConfig&)>` — LLMConfig 非 json (改善 draft 旧签名 `[](const json&)` 已废弃)

### 决策 D7-2: FinetuneBaseModelProvider stub 行为 (Phase 1 最小版)

```cpp
class FinetuneBaseModelProvider : public ILLMProvider {
 public:
  explicit FinetuneBaseModelProvider(LLMConfig config);
  // generate: 返回 failure "Phase 2 deferred" (避免 stub 误触发真实推理)
  // generate_stream: 返回 nullptr (Phase 1 无流式)
  // available_models: 返回 {agenticdsl-llama-3.1-70b-lora-v1} (非空, AC-6 要求)
};
```

- **设计原则**: stub 必须**明确失败**而非静默空响应 (fail-fast, 防"看起来成功但没数据")
- `available_models()` 返回非空: 让路由/工具能发现该模型 (ModelInfo: name=`agenticdsl-llama-3.1-70b-lora-v1`, capabilities={Chat, ToolUse}, provider=`finetune`)
- `generate()` 返回 `LLMError{Code::Unknown, "Phase 2 deferred: finetune model inference not yet implemented"}`

### 决策 D7-3: register_dynamic 注册位置

`src/common/llm/llm_provider_factory.cpp` 构造函数内注册 (每次 factory 构造自动可用):

```cpp
LLMProviderFactory::LLMProviderFactory() {
  // 自注册 fine-tune provider (Phase 1 stub) — register_dynamic 是公开成员函数,
  // 构造函数内自调用合法 (dynamic_mutex_ 已初始化, is_reserved_backend 检查通过)
  register_dynamic("agenticdsl-llama-3.1-70b-lora-v1",
                   [](const LLMConfig& config) {
                     return std::make_unique<FinetuneBaseModelProvider>(config);
                   });
}
```

**调用方创建 provider 的正确语法** (非 aggregate init, LLMConfig 是普通 struct):

```cpp
LLMConfig config;
config.provider = "agenticdsl-llama-3.1-70b-lora-v1";
auto provider = factory.create(config);  // 经 dynamic_factories_ 路由到 fine-tune stub
```

**注册文件位置勘误**: `src/common/llm/CMakeLists.txt` **不存在** — `finetune_provider.cpp` 必须注册到**根 `CMakeLists.txt`** 的 `agenticdsl_common` 源列表 (line ~79-97, 与 cloud_adapter.cpp / llm_provider_factory.cpp 同组)。

### 决策 D7-4: 测试策略

`tests/test_llm_provider_factory.cpp` (Catch2):
- Case 1: `register_dynamic registers fine-tune provider and instantiation returns stub`
  - `factory.has_dynamic("agenticdsl-llama-3.1-70b-lora-v1")` == true (构造自动注册)
  - `factory.create({.provider = "agenticdsl-llama-3.1-70b-lora-v1"})` → 非空
  - `available_models()` 非空
  - `generate(...)` 返回 failure, message 含 "Phase 2 deferred"

---

## 决策记录

| # | 决策 | 选项 | 敲定 |
|---|------|------|------|
| D1-1 | 评分基础设施复用 | 复用 test_llm_tool* + cost_tracking | ✅ |
| D1-2 | 评分 yaml 持久化 | docs/research/wave-3-base-model-selection.md 内嵌 yaml | ✅ |
| D3-1 | D3 schema migration 选项 | A (新建独立迁移脚本, 不破坏 T21) | ⏳ 决策点 #1/#2 (dual-agent review 后敲定) |
| D3-2 | 迁移脚本 CLI 契约 | --input/--output/--source | ✅ |
| D3-3 | source 字段语义 | baseline/failure/agenticmind | ✅ |
| D7-1 | LLMProviderFactory API 名称 | **register_dynamic** (实际 API) | ⏳ 决策点 #3 (dual-agent review 验证) |
| D7-2 | Stub 行为 | fail-fast: generate → failure "Phase 2 deferred" | ✅ |
| D7-3 | 注册位置 | factory 构造函数内 | ✅ |

---

*Design 版本: v1.0*
*创建日期: 2026-09-23*
*状态: ACTIVE (Wave 3 Phase 1 Pilot)*
