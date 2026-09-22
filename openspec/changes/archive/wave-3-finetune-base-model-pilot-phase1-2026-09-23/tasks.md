# Tasks: Wave 3 Fine-tune Base Model Pilot Phase 1

> **TDD 5 步**: 每任务严格遵循 Write failing test → Verify fail → Implement → Verify pass → Commit (defer commit to archive stage, per AGENTS.md mode #4 atomicity)
> **工作目录**: `.rddf/wt/wave-3-finetune-base-model/` (branch `feat/wave-3-finetune-base-model`)

## Step 1 — ADR-0078 状态翻牌 + Decision Record 更新 (0.5h)

- [x] Step 1.1: Modify `docs/adr/adr-0078-finetune-base-model.md` status row: 🔍 Proposed → ✅ Approved
- [x] Step 1.2: Remove "Wave 5+ descoped docs-only" label from ADR-0078 header
- [x] Step 1.3: Add Phase 1 容量评估段落 (引用 Pre-Wave3 Plan §3 + D1/D3/D7 边界)
- [x] Step 1.4: Modify `docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md` §5.1: GO closed → Wave 3 Phase 1 SHIPPED
- [x] Step 1.5: Modify `.rddf/roadmap/2026-09-21-pre-wave3-to-wave3-execution-plan.md` Decision Record §5.1

## Step 2 — OpenSpec change 4 件套创建 + dual-agent review (1h)

- [x] Step 2.1: Create `openspec/changes/wave-3-finetune-base-model-pilot-phase1/.openspec.yaml`
- [x] Step 2.2: Create `openspec/changes/wave-3-finetune-base-model-pilot-phase1/proposal.md`
- [x] Step 2.3: Create `openspec/changes/wave-3-finetune-base-model-pilot-phase1/design.md`
- [x] Step 2.4: Create `openspec/changes/wave-3-finetune-base-model-pilot-phase1/tasks.md`
- [x] Step 2.5: Create `openspec/changes/wave-3-finetune-base-model-pilot-phase1/specs/wave-3-finetune-base-model/spec.md`
- [x] Step 2.6: Run `openspec validate wave-3-finetune-base-model-pilot-phase1 --strict` → "Change is valid"
- [x] Step 2.7: Pre-implementation Metis + Oracle 并行审查 → 决策点 #1/#2/#3 敲定 (见下方决策点记录表)

## Step 3 — TDD RED: 2 个失败测试 (1h)

- [x] Step 3.1: Create `tests/test_training_data_pipeline.cpp` (RED — 编译/运行因 `scripts/prepare_training_data.py` 未实装 FAIL)
- [x] Step 3.2: Create `tests/test_llm_provider_factory.cpp` (RED — 编译因 `FinetuneBaseModelProvider` + register_dynamic 入口未实装 FAIL)
- [x] Step 3.3: Run `cmake --build build --target test_training_data_pipeline test_llm_provider_factory` → 编译失败 (RED 验证)

## Step 4 — GREEN: scripts/prepare_training_data.py + FinetuneBaseModelProvider stub (2h)

- [x] Step 4.1: Create `scripts/prepare_training_data.py` (D3 schema migration 选项敲定后实装)
- [x] Step 4.2: Create `src/common/llm/finetune_provider.h` (FinetuneBaseModelProvider : ILLMProvider stub)
- [x] Step 4.3: Create `src/common/llm/finetune_provider.cpp` (3 虚函数实现)
- [x] Step 4.4: Modify `src/common/llm/llm_provider_factory.cpp`: register_dynamic 入口 (构造函数内)
- [x] Step 4.5: Modify 根 `CMakeLists.txt`: 注册 `src/common/llm/finetune_provider.cpp` 到 agenticdsl_common
- [x] Step 4.6: Run 2 个测试 → 编译通过 + PASS (GREEN 验证)

## Step 5 — D1 评分 yaml 持久化 (1h)

- [x] Step 5.1: Run `tests/test_llm_tool*` (≥1 binary) + `tests/test_cost_tracking_decorator` (复用 D1 评分基础设施)
- [x] Step 5.2: Create `docs/research/wave-3-base-model-selection.md` (≥3 候选模型评分表 + 4 过滤条件验证 + 最终选择 + 论证)
- [x] Step 5.3: 验证 D1 评分 Weighted ≥ 7.5 + 4 过滤条件全过 (per ADR-0078 D1)

## Step 6 — 既有 test 零回归验证 (0.5h)

- [x] Step 6.1: Run focused ctest: test_training_data_pipeline + test_llm_provider_factory + test_llm_tool* + test_cost_tracking_decorator + test_genome_registry (≥6 binary) → 全 PASS
- [x] Step 6.2: Run `ctest -N` 计数 → 新增 2 binary, 无 PDK 跨库影响

## Step 7 — 1 atomic commit + Day-5 trap guard (0.5h)

- [x] Step 7.1: Run `git status` + `git add` (production + test + docs + scripts + OpenSpec 5 文件)
- [x] Step 7.2: archive 5 文件 → `openspec/changes/archive/wave-3-finetune-base-model-pilot-phase1-2026-09-23/` + `git rm` 活跃 change dir
- [x] Step 7.3: Day-5 trap guard: `git ls-files openspec/changes/archive/wave-3-finetune-base-model-pilot-phase1-2026-09-23/` → 5 文件全在
- [x] Step 7.4: Run `openspec validate --strict` (archived) → PASS
- [x] Step 7.5: 1 atomic commit `feat(llm): Wave 3 finetune-base-model pilot phase 1`

## Step 8-9 (主会话接手, worker 不执行)

- [ ] Step 8: Oracle post-impl SHIP-with-fixes 复评 (主会话派, prompt 引用 builder-handoff `post_impl_review_prompt`)
- [ ] Step 9: merge → main + cleanup worktree + AGENTS.md Recent Changes + builder state post_impl_execute_summary

---

## 决策点记录

| 决策点 | 内容 | 敲定结果 |
|--------|------|---------|
| #1 | D3 schema migration 选项 A/B/C | ✅ **选项 A** — 新建 `scripts/prepare_training_data.py` 独立迁移脚本 (消费 ADR-0074 D6 JSONL 作输入, 不改 export_training_data.py 保护 T21). 双 agent 收敛: Oracle V1 确认 "missing default true" 正确 + Metis C1 已由 spec R4.3 显式文档化 |
| #2 | D3 schema migration 选项 final (dual-agent review 后) | ✅ **选项 A final** (per dual-agent: Oracle bg_949a71f5 SHIP-with-fixes + Metis bg_27aac1d8) — 过滤 `parse_valid && task_success` + "缺失默认 true" 完全兼容 ADR-0074 V1 |
| #3 | LLMProviderFactory API 名称 (register_dynamic vs register_provider) | ✅ **`register_dynamic`** — 实际 API (llm_provider_factory.h:33), 双 agent 确认; improvement draft AC-6 笔误已统一勘误 |

## Pre-implementation Dual-Agent Review 修正 (per AGENTS.md 模式 #8)

**Oracle bg_949a71f5 (SHIP-with-fixes)**:
- 🔴 C1 Critical: `test_training_data_pipeline` 缺 WORKING_DIRECTORY → 已修 (tests/CMakeLists.txt 加 `WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}`)
- 🟠 M1 Major: design.md D7-3 aggregate-init 示例不编译 → 已修 (改 `LLMConfig config; config.provider = ...` + 注明注册文件位置勘误)
- ✅ V1-V5 验证通过 (register_dynamic API / 名称不冲突 / filter 逻辑 / test fixture / LLMConfig 字段)

**Metis bg_27aac1d8 (DEAL-BREAKER → 全部已解决)**:
- C1: D3 schema 字段不匹配 — 由 spec R4.3 显式 "missing default true + ADR-0074 V1 兼容" 解决 (Oracle 独立验证同一结论 = 收敛信号)
- C2: `src/common/llm/CMakeLists.txt` 不存在 → 已明确注册到根 `CMakeLists.txt` agenticdsl_common 源列表 (design.md D7-3 + 本 tasks Step 4.5)
- M1: 选项 A 语义冲突 → design.md D3-1 明确 "新建独立迁移脚本, 不改造 export_training_data.py"
- M2: AC-6 笔误 → 本 change 全部 artifacts 统一 `register_dynamic`

---

*Tasks 版本: v1.0*
*创建日期: 2026-09-23*
*状态: ACTIVE (Wave 3 Phase 1 Pilot)*
