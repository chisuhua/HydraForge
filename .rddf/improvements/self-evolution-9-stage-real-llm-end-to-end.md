# self-evolution-9-stage-real-llm-end-to-end

> **来源**: 2026-10-06 全量回归测试完备性审查 (`AGENTS.md §Recent Changes` entry 自识别盲点 G8)
> **生成**: 2026-10-06 via completeness-audit
> **AGENTS.md Pattern**: #10 hygiene (systematic recording of deferred work)

**优先级**: 🟢 P2 | **阶段**: 自由 (phase-n/a) | **分类**: 真实 LLM 验证盲点
**类型**: debt-tracking | **主题**: 自进化 v1.5 9 段闭环完整真实 LLM 端到端
**状态**: pending (段 3+8 真实 LLM 已 ship, 段 1-2 + 4-7 mock only)
**关联**:
- `self-evolution-architecture-2026-08.md` v1.5 §十二 9 段闭环
- `tests/test_gepa_loop_real_llm.cpp` (段 3 ✅)
- `tests/test_distillation_capture_training_real_llm.cpp` (段 8 ✅)

## 背景

自进化 v1.5 9 段闭环真实 LLM 覆盖现状:

| 段 | 名称 | mock | real LLM |
|----|------|------|----------|
| 1 | trace 捕获 | ✅ mock | ❌ |
| 2 | baseline attribution | ✅ mock | ❌ |
| 3 | GEPA Loop | ✅ mock + ✅ real | ✅ (`test_gepa_loop_real_llm`) |
| 4 | mutation apply | ✅ mock | ❌ |
| 5 | reload + rerun | ✅ mock | ❌ |
| 6 | phase5_compare (IEvaluator) | ✅ mock | ❌ |
| 7 | commit (genome.persist) | ✅ mock | ❌ (见 G4) |
| 8 | distillation | ✅ mock + ✅ real | ✅ (`test_distillation_capture_training_real_llm`) |
| 9 | 训练数据回流 (Wave 3 Phase 2 D4) | ⚠️ Phase 2 deferred | ❌ |

## 缺口

段 1-2 + 4-7 真实 LLM 端到端验证未覆盖。其中:
- 段 1-2 (trace + baseline): 与段 7 commit 协同 (见 G4)
- 段 4-5 (mutation apply + reload): 协同 G4 (Harness mutation commit 真实 LLM)
- 段 6 (phase5_compare): 需要 LLM 真实评分能力, 独立

## 触发条件

任何下列条件满足即升级 P0 → 立即立项:
1. 用户要求 "自进化 9 段闭环真实 LLM 端到端"
2. R3 元指标实证要求 ≥ 3 类 ContextRequest (per AGENTS.md §Reverse Indicator Rule)
3. Wave 3 Phase 2 D4 训练数据回流需要完整 trace 流

## 实施建议 (非实施, 待立项 OpenSpec change)

依赖 G1 + G4 + G7 ship 后.

新增 `openspec/changes/2026-XX-self-evolution-9-stage-real-llm/`:
- 新增 `tests/test_self_evolution_9_stage_real_llm.cpp` (must_realllm label)
  - 端到端跑 9 段: trace → baseline → GEPA → mutation apply → reload → phase5_compare → commit → distillation → training data export
  - 真实 LLM 评分 IEvaluator (phase5_compare)
  - 真实 trace JSONL 含 0 confidential leak (G6 协同)
  - 真实 drop_ratio (G7 协同)
  - ≥ 3 类 ContextRequest (code/research/debug, R3 元指标实证)
- 预估 effort: 3-5 天 (依赖 G1+G4+G7)
- token 消耗: ~10-50 LLM calls (高)
- 关键: 这是 Wave 3 Phase 2 D4 训练数据回流的输入数据源, 必须真实 LLM 闭环
