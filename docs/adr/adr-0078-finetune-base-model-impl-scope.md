# ADR-0078 实现范围审计 (Implementation Scope Audit)

## 状态

**🟡 Partial（Wave 3 Phase 1 Pilot ship, Phase 2 future）** (2026-10-17) — 本文件是 ADR-0078 实施范围审计，记录 Wave 3 Phase 1 Pilot 实际实施边界（D1 基模选型评分 + D3 训练数据准备 + D7 serving stub）+ Phase 2 延后项（D4-D7 完整推理 + 训练 + serving）。状态变更历史见头部备注段落。

> **🟡 Partial（Wave 3 Phase 1 Pilot ship, Phase 2 future）** (2026-10-17) — 本文件由 docs_drift_audit.py Scenario 4 工具建议创建。
> 状态：Wave 3 Phase 1 ship ✅（commits `97a2abb` impl + `232eb13` SHIP-with-fixes + merge `f0a5c4b`，2026-09-23）；Phase 2 启动前提 = Wave 3 cooling-off 满 24h（已过 2026-09-24T05:35Z）+ AgenticMind 真实 LLM 探索产出 + 用户决策启动。
> **本审计不修改任何 C++ 或测试源代码。**

## ADR 描述（引用 ADR-0078 原文要点）

ADR-0078 (Fine-tune 基模选型与训练管线) 描述：
- **D1**: 基模选型评分（4 维度：能力/成本/适配度/可控性）
- **D2**: 训练数据 Schema（ADR-0074 D6 JSONL + `source` 字段 + 过滤 `parse_valid && task_success`）
- **D3**: 训练数据准备第 1 路（`scripts/prepare_training_data.py`）
- **D4**: LoRA fine-tune 训练（HF TRL / PEFT）
- **D5**: Baseline 重测（per ADR-0074 D4 Evidence Gate）
- **D6**: 训练数据 PII 扫描（CI hook）
- **D7**: Serving 集成 — `FinetuneBaseModelProvider` 注册 + LLMProviderFactory

## 代码实际状态（grep 验证 2026-10-17）

### 验证命令

```bash
# D1: 基模选型评分持久化
rg -n "weighted_score|passes_all_filters" docs/research/wave-3-base-model-selection.md
# → 5 候选模型 yaml 评分 (gpt-4 / claude-3 / llama-3-70b / qwen-2.5-72b / deepseek-v2-chat)
# → 最终选择 llama-3.1-70b-lora-v1 (8.2 ≥ 7.5 ✅)

# D3: 训练数据准备脚本
ls scripts/prepare_training_data.py
# → 存在, ADR-0074 D6 JSONL + source 字段 + 过滤 parse_valid && task_success ✅

# D7 Phase 1 stub
rg -n "FinetuneBaseModelProvider|agenticdsl-llama-3.1-70b-lora-v1" src
# → src/common/llm/finetune_provider.h:25 class FinetuneBaseModelProvider : public ILLMProvider
# → src/common/llm/finetune_provider.cpp:22 generate → failure "Phase 2 deferred" (fail-fast)
# → src/common/llm/llm_provider_factory.cpp:52 register_dynamic 自注册 (Phased 1 minimal)

# D4 LoRA training: NOT implemented
rg -rn "LoRA|PEFT|trl\." src scripts
# → 0 matches — Phase 2 deferred

# D5 baseline 重测: NOT implemented
ls tools/measure_finetune_baseline.cpp
# → No such file

# D6 PII 扫描: NOT implemented
ls tools/scan_pii.py
# → No such file
```

### 实际状态

| ADR-0078 决策 | 状态 | 证据 |
|---|---|---|
| **D1**: 基模选型评分（4 维度） | ✅ **Phase 1 ship** | `docs/research/wave-3-base-model-selection.md` — 5 候选模型 yaml 评分 (gpt-4 / claude-3 / llama-3-70b / qwen-2.5-72b / deepseek-v2-chat)，每候选 4 维度评分（能力/成本/适配度/可控性）+ Weighted ≥ 7.5 过滤 + 4 过滤条件表 + 最终选择 llama-3.1-70b-lora-v1（8.2 ≥ 7.5 ✅）。ORACLE SHIP-with-fixes `bg_7fe026cc` M1 算术校正（5 weighted_score 全部校准 + deepseek false-positive 修正）。**注意**：评分 yaml 是 D1 评分持久化（Phase 1），D1 基模**实际选择**待 Phase 2 补 AgenticMind 真实 benchmark 数据 |
| **D2**: 训练数据 Schema（JSONL + source + 过滤） | ✅ **Phase 1 ship** | `scripts/prepare_training_data.py` — ADR-0074 D6 JSONL 格式 + `source` 字段 (`wave3_pilot_phase1` / `wave3_pilot_phase2`) + 过滤 `parse_valid && task_success`；数据流：原始轨迹 → 过滤 → 去重 → JSONL 输出 |
| **D3**: 训练数据准备第 1 路 | ✅ **Phase 1 ship** | 同 D2 脚本。Wave 3 Phase 2 启动后会扩第 2/3 路（agenticmind_rlhf.jsonl + D7 失败事件） |
| **D4**: LoRA fine-tune 训练 | ❌ **Phase 2 deferred** | 0 grep 命中 — HF TRL / PEFT 依赖未引入；Wave 3 Phase 2 启动引入 (单 A100 80GB × 24h 估时) |
| **D5**: Baseline 重测 | ❌ **Phase 2 deferred** | `tools/measure_finetune_baseline.cpp` 不存在 — Phase 2 实施 + A/B 测试 |
| **D6**: 训练数据 PII 扫描 | ❌ **Phase 2 deferred** | `tools/scan_pii.py` 不存在 — CI hook 接入留 Phase 2 |
| **D7 Phase 1 stub**: Serving 注册 + 失败快速 | ✅ **Phase 1 ship** | `src/common/llm/finetune_provider.h:25-47` `FinetuneBaseModelProvider : public ILLMProvider`; `src/common/llm/finetune_provider.cpp:22-43` 三虚函数: `generate` → `failure "Phase 2 deferred"` (fail-fast, 避免静默空响应); `generate_stream` → `nullptr` (Phase 1 无流式); `available_models` → `{kModelName}` 非空含 `Chat + ToolUse` capability (AC-6); `src/common/llm/llm_provider_factory.cpp:52-56` 构造函数自注册 `agenticdsl-llama-3.1-70b-lora-v1` |
| **D7 Phase 2 完整推理**: 真实 LoRA 推理 | ❌ **Phase 2 deferred** | ADR-0078 §D7 描述的 `FinetuneLlamaProvider` 类 (call local Llama + LoRA adapter) 仍未实施；Phase 1 用 `FinetuneBaseModelProvider` stub 替代；Phase 2 替换为实际推理 |
| **MCP `prompts/*` 更新**: `generate_dsl_v3_finetuned` | ❌ **Phase 2 deferred** | ADR-0076 / docs/active-status.md:601 Phase 2 待启动 |

## 测试覆盖（Phase 1 ship）

| 测试 binary | 覆盖范围 | Phase 1 状态 |
|---|---|---|
| `tests/test_provider_factory.cpp` | D7 stub 注册 + 6 处动态注册断言 (含 `agenticdsl-llama-3.1-70b-lora-v1` baseline size +1) | ✅ Phase 1 ship, 12/12 cases PASS |
| `tests/test_provider_factory_concurrent.cpp` | D7 stub 并发线程安全 | ✅ Phase 1 ship |
| `tests/test_provider_register_dynamic_tool.cpp` | D7 stub 与其他 dynamic 注册共存 | ✅ Phase 1 ship |
| `tests/test_llm_provider_factory.cpp` | D7 stub 与 mock/cloud/llama 三 factory 共存 | ✅ Phase 1 ship |
| `tests/test_llm_provider_factory_decorator.cpp` | D7 stub 装饰器链 (decorator pattern) | ✅ Phase 1 ship |
| `tests/test_llm_tool.cpp` | D7 stub 与 LLM tool 集成 | ✅ Phase 1 ship |
| `tests/test_cost_tracking_decorator.cpp` | D7 stub cost tracking 装饰器 (Phase 1 stub generate 必失败, cost 由空调用计) | ✅ Phase 1 ship |
| `tests/test_training_data_pipeline.cpp` | D2/D3 JSONL + source + 过滤 parse_valid && task_success | ✅ Phase 1 ship |
| `tests/test_genome_registry.cpp` | 自进化闭环第 7 环 (G4 genome-wiring-harness-rsi-gepa ship) | ✅ Phase 1 ship |
| `tests/test_distillation_capture_training_real_llm.cpp` | must_realllm 真 LLM distillation 数据 → 训练数据流 (R13.4 redaction 守卫) | ✅ Phase 1 ship |
| **Phase 2 测试**: `test_finetune_llama_provider.cpp` / `test_llm_provider_factory_routing.cpp` / `test_basemodel_selection.cpp` / `test_pii_scan.cpp` / `test_finetune_baseline.cpp` | ❌ **Phase 2 deferred** |

## Phase 1 ship 验收 (per OpenSpec change `wave-3-finetune-base-model-pilot-phase1`)

- **Wave 3 merge `f0a5c4b` (2026-09-23T05:35Z)** — Sisyphus-Junior async worker `bg_8cb82c2c` (1h58m) 实施 + Oracle SHIP-with-fixes 复评 `bg_7fe026cc` (49m) 完整 ship 3 commits on worktree + merge
- **Oracle verdict**: 1 Critical (LLMProviderFactory 自注册污染 dynamic_names 6 处断言) + 1 Major (D1 评分算术错 deepseek false-positive) + 4 Minor 全部 SHIP-with-fixes 应用
- **focused ctest 9/9 PASS** (2.23s): test_provider_factory (含 6 处修正断言) + test_provider_factory_concurrent + test_provider_register_dynamic_tool + test_training_data_pipeline + test_llm_provider_factory + test_llm_tool + test_cost_tracking_decorator + test_genome_registry + test_llm_provider_factory_decorator
- **12 AC 验证 ALL PASS** (AC-1 ADR-0078 翻牌 / AC-2 4 件套 / AC-3 openspec validate / AC-4 D1 评分 / AC-5 D3 脚本 / AC-6 D7 stub / AC-7 既有 test 零回归 / AC-8 ctest 计数 / AC-9 atomic commit + Oracle / AC-10 Day-5 archive / AC-11 AGENTS.md + ADR / AC-12 cooling-off)
- **drop_ratio = 0%** (纯 stub + 工具 + 文档, 无 production behavior 变化)

## Phase 2 启动条件 (Wave 3 cooling-off 满 + AgenticMind + 用户决策)

1. **Wave 3 cooling-off 满 24h**: merge `f0a5c4b` 2026-09-23T05:35Z + 24h = 2026-09-24T05:35Z 已过期 ✅
2. **AgenticMind 真实 LLM 探索产出**: 4-6 周探索窗口 (per ADR-0078 §D2 触发条件 #1)
3. **用户决策启动 Phase 2**: 当前主会话/未来 session 决策 D4-D7 完整优先级 + 资源评估
4. **D4-D7 工作量估算**: 2-4 周 (单 Sprint 不可完成, 需独立 OpenSpec change `wave-3-phase-2-d4-lora-pipeline` 立项, 已 ship 5 文件 per 2026-09-25 DECISION C, 24h cooling-off 已过)

## 后续 follow-up

- **`docs/adr/adr-0078-finetune-base-model-impl-scope.md`** (本文件) — Phase 1 ship 实施范围审计, 供 docs_drift_audit.py Scenario 4 关闭 ✅
- **OpenSpec change `2026-09-25-wave-3-phase-2-d4-lora-pipeline`** — Phase 2 D4 LoRA 训练管线立项载体 (5 文件 + 24h cooling-off 满 + 已 archive per Day-5 4-file integrity)
- **`.rddf/improvements/` follow-up tracker** — Wave 3 Phase 2 启动入口, 见 adr-0078-finetune-base-model 提案状态

## 参考

- `docs/adr/adr-0078-finetune-base-model.md` — 主 ADR (🔍 Proposed → ✅ Approved 2026-09-23)
- `openspec/changes/archive/wave-3-finetune-base-model-pilot-phase1/` — Phase 1 OpenSpec change archive (Day-5 4-file integrity)
- `openspec/changes/archive/2026-09-25-wave-3-phase-2-d4-lora-pipeline/` — Phase 2 D4 LoRA 管线立项 archive
- `docs/research/wave-3-base-model-selection.md` — D1 5 候选模型 yaml 评分持久化
- `scripts/prepare_training_data.py` — D3 训练数据准备脚本
- `src/common/llm/finetune_provider.h/cpp` — D7 Phase 1 stub 实现
- `src/common/llm/llm_provider_factory.cpp:52-56` — D7 stub 注册
- `tests/test_provider_factory.cpp` (12 cases) + 8 其他测试 binary — Phase 1 测试覆盖
- AGENTS.md §Sprint 36 (Wave 3 Phase 1 ship 详细记录) + §Pattern #11 (Async Worker + Dual Oracle SHIP-with-fixes cycle)