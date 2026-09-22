# Proposal: Wave 3 Fine-tune Base Model Pilot Phase 1

> **STATUS**: ACTIVE (2026-09-23)
> **触发**: Pre-Wave3 Plan §3 (G1-G4 4-Gate 序列全部 SHIPPED 2026-09-22) + ADR-0078 🔍 Proposed → ✅ Approved 翻牌 + 用户显式 override 24h cooling-off (audit: builder-handoff `cooling_off_override_audit`)
> **范围**: Phase 1 边界 — D1 (基模选型 4 维度评分) + D3 (训练数据准备, ADR-0074 D6 JSONL 加 `source` 字段) + D7 Phase 1 最小版 (Fine-tune provider stub 注册 + config 解析). D4-D7 完整实施延后 Wave 3 Phase 2+.
> **估时**: 1-2 天 (Phase 1 边界)
> **关联 ADR**: [ADR-0078](../adr/adr-0078-finetune-base-model.md) (翻牌目标) + ADR-0074 (D6 JSONL 数据源, 只读) + ADR-0071 §D9 (派生)

---

## Why（背景概要）

Pre-Wave3 Plan §3 明确 Wave 3 立项依据: G1+G3+G4+G2 4-Gate 序列全部 SHIPPED (2026-09-22) 后, ADR-0078 Model-RSI pilot 是 HydraForge 闭环自进化的下一步 — 把自进化"变异→评估→决策→提交"产出的数据作为训练数据回流, 实现"自进化→自我训练"正循环。

**3 项 Wave 3 价值**:
1. **闭环第 7 环配套完整**: G4 ship 后 `genome.committed` / `genome.persist_failed` 事件现成, `evolution.readiness.denied` 事件 `eval_quality` 真实值 (G2 ship) — 训练数据来源 D3 现成
2. **D2 触发条件部分满足**: AgenticMind 立项 + Fine-tune 价格 ≤$1/1M + G4 事件真实值; Production 用户 ≥10 未达 (Phase 7 部署后)
3. **D1 4 维度评分框架就绪**: llm-tool-eval + cost-monitoring 已 ship, 可量化评分候选基模

**Wave 3 不做的话**: 自进化闭环仅"变异→评估→决策"通, 缺"决策→训练→更强基模"正反馈; Fine-tune 模型注册为 ILLMProvider 是 MCP server + gRPC data plane 的前置, 阻断下游。

## What Changes

- **ADR-0078 状态翻牌**: 🔍 Proposed (Wave 5+ descoped) → ✅ Approved (Wave 3 Phase 1 Pilot 激活). Wave 5+ descoped 标签移除, 加 Phase 1 容量评估段落.
- **D1 基模选型实施**: 4 维度 (Capability/Latency/Cost/Openness) 评分, ≥3 候选模型, Weighted ≥ 7.5 + 4 过滤条件全过, 评分 yaml 持久化 (`docs/research/wave-3-base-model-selection.md`).
- **D3 训练数据准备**: `scripts/prepare_training_data.py` — ADR-0074 D6 JSONL 加 `source` 字段 (`baseline`/`failure`/`agenticmind`) + 过滤 `parse_valid && task_success` + 输出 `data/wave-3-training-data.jsonl`. 新增 `tests/test_training_data_pipeline.cpp`.
- **D7 serving Phase 1 最小版**: `FinetuneBaseModelProvider : ILLMProvider` stub (注册 + config 解析, generate 返回 failure "Phase 2 deferred") + `LLMProviderFactory::register_dynamic("agenticdsl-llama-3.1-70b-lora-v1", factory_fn)` 入口. 新增 `tests/test_llm_provider_factory.cpp`.
- **Decision Record 更新**: `docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md` §5.1 + `.rddf/roadmap/2026-09-21-pre-wave3-to-wave3-execution-plan.md` Decision Record §5.1 → Wave 3 Phase 1 SHIPPED.

**Non-Goals (Wave 3 Phase 2+)**:
- ❌ D4 训练方法 (LoRA/QLoRA/API fine-tune) — 延后 Phase 2 (估时 2-4 周)
- ❌ D5/D6 评估方法学 + AgenticMind 回流 — 延后 Phase 2 (依赖 AgenticMind ship)
- ❌ D7 MCP `prompts/*` 更新 + 真实推理 — 延后 Phase 2 (依赖 ADR-0076 gRPC)
- ❌ ADR-0071/0074/0076/0077 改动 — 4 个上游 ADR 各自独立 OpenSpec change
- ❌ 新 PDK plugin — Fine-tune model 作为现有 ILLMProvider 接口实现
- ❌ 新外部依赖 (DeepSeek API / HF TRL / PEFT) — 不增加 build complexity

## How（技术方案）

### D1 基模选型评分 (文档性, 复用现有基础设施)

- 复用 `tests/test_llm_tool*` + `tests/test_cost_tracking_decorator` 评分/成本基础设施 (Step 5.1)
- 评分 yaml 持久化到 `docs/research/wave-3-base-model-selection.md` (Step 5.2)
- 4 维度权重: Capability 30% / Latency 20% / Cost 20% / Openness 30% (per ADR-0078 D1)
- 选择标准: Weighted ≥ 7.5 + Capability ≥ 8.0 + Openness ≥ 5.0 + Cost ≤ $1/1M + Latency P95 ≤ 100ms/token

### D3 训练数据准备 (D3 schema migration 选项敲定 — 决策点 #1/#2)

**D3 schema migration 选项 A/B/C (per improvement draft + LLM concern #1)**:

| 选项 | 描述 | 优 | 劣 |
|---|---|---|---|
| **A** | 改造 `tools/prompt/export_training_data.py` 扩展 schema, 加 `source` + ADR-0078 假设字段 | 满足 ADR-0078 治理意图 | 需要 schema migration 测试 + backward compat, 复杂度高 |
| **B** | 接受 ADR-0074 V1 实际 schema, Wave 3 微调 ADR-0078 ADR | 简单, 低风险 | 失去 ADR-0078 部分治理价值 |
| **C** | 双 schema 并存: 训练用 ADR-0074 V1, 评估用 ADR-0078 假设字段 | 解耦, 灵活 | 增加 complexity, 需同步维护两套 schema |

**决策**: 待 dual-agent review 敲定 (Step 2.7)。新增 `source` 字段 (`baseline`/`failure`/`agenticmind`) 跨选项通用。

### D7 serving Phase 1 最小版 (API 名称勘误 — 决策点 #3)

- **实际 API**: `LLMProviderFactory::register_dynamic(name, DynamicFactoryFn)` (per `src/common/llm/llm_provider_factory.h:33`)
- **improvement draft AC-6 笔误**: `register_provider`
- **决策**: 使用 `register_dynamic` (实际 API), proposal/design/tasks/spec 全部统一. Factory fn 签名 `std::function<std::unique_ptr<ILLMProvider>(const LLMConfig&)>` (LLMConfig 非 json).

### 文件清单

| 文件 | 变更 |
|---|---|
| `src/common/llm/finetune_provider.h/cpp` | NEW — FinetuneBaseModelProvider stub |
| `src/common/llm/llm_provider_factory.cpp` | MODIFY — register_dynamic 入口 |
| `src/common/llm/CMakeLists.txt` (根 CMakeLists.txt) | MODIFY — 注册 finetune_provider.cpp |
| `scripts/prepare_training_data.py` | NEW — D3 迁移 + 过滤脚本 |
| `docs/adr/adr-0078-finetune-base-model.md` | MODIFY — 状态翻牌 + 容量评估段落 |
| `docs/research/wave-3-base-model-selection.md` | NEW — D1 评分 yaml |
| `docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md` | MODIFY — §5.1 |
| `.rddf/roadmap/2026-09-21-pre-wave3-to-wave3-execution-plan.md` | MODIFY — Decision Record §5.1 |
| `tests/test_training_data_pipeline.cpp` | NEW — D3 测试 |
| `tests/test_llm_provider_factory.cpp` | NEW — D7 测试 |
| `openspec/changes/wave-3-finetune-base-model-pilot-phase1/` | NEW — 本 change 5 文件 |

## Acceptance

- [ ] AC-1: ADR-0078 状态从 🔍 Proposed → ✅ Approved (per Pre-Wave3 Plan §3 + Decision Record §5.1 更新)
- [ ] AC-2: `openspec/changes/wave-3-finetune-base-model-pilot-phase1/` 5 文件完整 (.openspec.yaml + proposal.md + design.md + tasks.md + specs/*/spec.md)
- [ ] AC-3: `openspec validate wave-3-finetune-base-model-pilot-phase1 --strict` → "Change is valid"
- [ ] AC-4: D1 基模选型 — 至少 3 个候选模型评分 + 1 个最终选择 + 评分 yaml 持久化 (`docs/research/wave-3-base-model-selection.md`)
- [ ] AC-5: D3 训练数据准备 — ADR-0074 D6 JSONL 加 `source` 字段迁移脚本 + `tests/test_training_data_pipeline.cpp` ≥ 1 case (过滤 `parse_valid && task_success`)
- [ ] AC-6: D7 serving 集成 (Phase 1 最小版) — `LLMProviderFactory::register_dynamic` 新增 fine-tune provider + `tests/test_llm_provider_factory.cpp` ≥ 1 case (注册 + 实例化返回 stub)
- [ ] AC-7: 既有 `tests/test_llm_tool*` + `tests/test_cost_tracking_decorator` + `tests/test_genome_registry` 零回归 (≥6 个 binary PASS)
- [ ] AC-8: 全量 `ctest -N` 计数 = expected (新增 2 binary: test_training_data_pipeline + test_llm_provider_factory, 无 PDK 跨库影响)
- [ ] AC-9: 1 atomic commit per AGENTS.md 模式 #4 + Oracle post-impl `SHIP-with-fixes` 复评通过 (主会话派)
- [ ] AC-10: archive 时 `git ls-files openspec/changes/archive/wave-3-finetune-base-model-pilot-phase1-2026-09-23/` 验证 5 文件全在 (防 AGENTS.md Day-5 lesson 陷阱)
- [ ] AC-11: AGENTS.md Recent Changes + ADR-0078 状态翻牌 + Decision Record 同步
- [ ] AC-12: 24h cooling-off 链式合规 — Wave 3 cooling-off 自 Wave 3 merge 起算 (自 G2 merge `dc12a17` 链式)

---

*Proposal 版本: v1.0*
*创建日期: 2026-09-23*
*状态: ACTIVE (Wave 3 Phase 1 Pilot)*
