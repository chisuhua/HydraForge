# wave-3-finetune-base-model Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 启动 Wave 3 Model-RSI pilot Phase 1 (per Pre-Wave3 Plan §3 + ADR-0078 🔍 Proposed → ✅ Approved 翻牌). 实施 D1 (基模选型 4 维度评分) + D3 (训练数据准备, ADR-0074 D6 JSONL 加 `source` 字段 + 过滤) + D7 Phase 1 最小版 (Fine-tune provider stub 注册 + config 解析). **D4/D5/D6/D7-Phase 2 + MCP prompts/* 更新 全部延后 Wave 3 Phase 2+ (per improvement draft Non-Goals)**.

**Architecture:**
- ADR-0078 翻牌 (状态行 + Wave 5+ descoped 标签移除 + Phase 1 容量评估段落新增)
- D1 文档性产出 (评分 yaml 持久化, 无需新 test binary; 复用 `tests/test_cost_tracking_decorator` + `tests/test_llm_tool*`)
- D3 Python 脚本 (`scripts/prepare_training_data.py`) + 1 C++ test (`tests/test_training_data_pipeline.cpp`)
- D7 C++ 类 (`src/common/llm/finetune_provider.h/cpp`) + `LLMProviderFactory::register_dynamic` 调用 + 1 C++ test (`tests/test_llm_provider_factory.cpp`)
- OpenSpec change 4 件套 (`openspec/changes/wave-3-finetune-base-model-pilot-phase1/`): `.openspec.yaml` + `proposal.md` + `design.md` + `tasks.md` + `specs/wave-3-finetune-base-model/spec.md`
- TDD 5 步: 先 RED test → 验证 fail → GREEN 实施最小代码 → 验证 pass → defer commit (per AGENTS.md mode #4 atomicity)

**Spec Source:** `openspec/changes/wave-3-finetune-base-model-pilot-phase1/{proposal.md, design.md, tasks.md, specs/wave-3-finetune-base-model/spec.md}`

**Oracle dual-agent review plan (per AGENTS.md mode #11 + AC-9):**
- Pre-implementation Metis + Oracle 并行审查 (templates: Metis `bg_d9744d91` + Oracle `bg_c706862b`)
- Post-implementation Oracle SHIP-with-fixes 复评 (主会话派, prompt 已 save in `.rddf/state/builder/wave-3-finetune-base-model.json::post_impl_review_prompt`)

**Cooling-off override (per user decision):** 用户显式 override Single-Dev 治理范式 24h cooling-off 红线 (T+1h28m from G2 merge `dc12a17`, 满点 2026-09-23T14:30Z). 审计证据记录在 builder-handoff `cooling_off_override_audit` 字段.

---

## File Structure

### Production Code (NEW)

| File | Responsibility |
|---|---|
| `src/common/llm/finetune_provider.h` | `FinetuneBaseModelProvider : ILLMProvider` (Phase 1 stub: 注册 + config 解析 + generate 返回 failure "Phase 2 deferred") |
| `src/common/llm/finetune_provider.cpp` | FinetuneBaseModelProvider 3 虚函数实现 (generate / generate_stream / available_models) — Phase 1 stub 版本, 推理逻辑延后 Phase 2 |
| `src/common/llm/llm_provider_factory.cpp` | `register_dynamic("agenticdsl-llama-3.1-70b-lora-v1", factory_fn)` 入口 — **API 实际名称勘误**: `register_dynamic` (per `src/common/llm/llm_provider_factory.h:33`), 不是 `register_provider` (improvement draft 笔误) |

### Production Code (MODIFY)

| File | Responsibility |
|---|---|
| `docs/adr/adr-0078-finetune-base-model.md` | 状态行 🔍 Proposed → ✅ Approved + Wave 5+ descoped 标签移除 + Phase 1 容量评估段落新增 (引用 Pre-Wave3 Plan §3 + Decision Record §5.1) |

### Docs (NEW)

| File | Responsibility |
|---|---|
| `docs/research/wave-3-base-model-selection.md` | D1 评分 yaml (≥3 候选模型评分表 + 4 过滤条件验证 + 最终选择 + 论证) |
| `openspec/changes/wave-3-finetune-base-model-pilot-phase1/proposal.md` | OpenSpec change proposal (Why / What / How / Acceptance 12 条) |
| `openspec/changes/wave-3-finetune-base-model-pilot-phase1/design.md` | OpenSpec change design (D1/D3/D7 决策点 + D3 schema migration 选项 A/B/C 敲定) |
| `openspec/changes/wave-3-finetune-base-model-pilot-phase1/tasks.md` | OpenSpec change tasks (TDD 5 步 checkbox) |
| `openspec/changes/wave-3-finetune-base-model-pilot-phase1/specs/wave-3-finetune-base-model/spec.md` | OpenSpec change spec (D1/D3/D7 Requirements + Scenarios) |
| `openspec/changes/wave-3-finetune-base-model-pilot-phase1/.openspec.yaml` | OpenSpec metadata |

### Docs (MODIFY)

| File | Responsibility |
|---|---|
| `AGENTS.md` | Recent Changes 条目 (Wave 3 Phase 1 ship 后) |
| `docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md` | §3 摩擦段同步 (摩擦 1 已 resolved, 无新增摩擦) + §5.1 4-Gate 序列 → Wave 3 Phase 1 容量评估段落新增 |
| `.rddf/roadmap/2026-09-21-pre-wave3-to-wave3-execution-plan.md` | Decision Record §5.1 (GO closed → Wave 3 Phase 1 SHIPPED) + §3 摩擦列表更新 |
| `.rddf/improvement-suggestions.md` | (若存在) Wave 3 Phase 1 立项条目添加 |

### Scripts (NEW)

| File | Responsibility |
|---|---|
| `scripts/prepare_training_data.py` | D3 ADR-0074 D6 JSONL 迁移 (加 `source` 字段 = `baseline`/`failure`/`agenticmind`) + 过滤 `parse_valid && task_success` + 输出 `data/wave-3-training-data.jsonl` |

### Tests (NEW)

| File | Responsibility |
|---|---|
| `tests/test_training_data_pipeline.cpp` | D3 验证: 加载 ADR-0074 D6 JSONL → 加 source 字段 → 过滤 → 输出文件存在 + records 数 ≥ 1 (AC-5) |
| `tests/test_llm_provider_factory.cpp` | D7 验证: `LLMProviderFactory::register_dynamic(name, factory_fn)` 注册 → 实例化 → `provider->available_models()` 返回非空 + `provider->generate(...)` 返回 failure "Phase 2 deferred" (AC-6) |

### Build (MODIFY)

| File | Responsibility |
|---|---|
| `src/common/llm/CMakeLists.txt` | 注册 `finetune_provider.cpp` 到 `agenticdsl_common` 静态库 |
| `tests/CMakeLists.txt` | 注册 `test_training_data_pipeline` + `test_llm_provider_factory` 2 个新 binary |

### Archive

| File | Responsibility |
|---|---|
| `openspec/changes/archive/wave-3-finetune-base-model-pilot-phase1-2026-09-23/` | archive 5 文件 (`.openspec.yaml` + `proposal.md` + `design.md` + `tasks.md` + `specs/<name>/spec.md`) — per AGENTS.md Day-5 trap guard |

---

## Implementation Roadmap (from improvement draft Step 1-9)

### Step 1 — ADR-0078 状态翻牌 + Decision Record §3 更新 (0.5h)

- [ ] **Step 1.1:** Modify `docs/adr/adr-0078-finetune-base-model.md` status row: 🔍 Proposed → ✅ Approved
- [ ] **Step 1.2:** Remove "Wave 5+ descoped docs-only" label from ADR-0078 header
- [ ] **Step 1.3:** Add Phase 1 容量评估段落 (引用 Pre-Wave3 Plan §3 + D1/D3/D7 边界)
- [ ] **Step 1.4:** Modify `docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md` §5.1: GO closed → Wave 3 Phase 1 SHIPPED
- [ ] **Step 1.5:** Modify `.rddf/roadmap/2026-09-21-pre-wave3-to-wave3-execution-plan.md` Decision Record §5.1

### Step 2 — OpenSpec change 4 件套创建 + dual-agent review (1h)

- [ ] **Step 2.1:** Create `openspec/changes/wave-3-finetune-base-model-pilot-phase1/.openspec.yaml` (OpenSpec metadata schema)
- [ ] **Step 2.2:** Create `openspec/changes/wave-3-finetune-base-model-pilot-phase1/proposal.md` (Why / What / How / Acceptance 12 条)
- [ ] **Step 2.3:** Create `openspec/changes/wave-3-finetune-base-model-pilot-phase1/design.md` (D1/D3/D7 决策 + D3 schema migration 选项敲定 — **决策点 #1**)
- [ ] **Step 2.4:** Create `openspec/changes/wave-3-finetune-base-model-pilot-phase1/tasks.md` (TDD 5 步 checkbox)
- [ ] **Step 2.5:** Create `openspec/changes/wave-3-finetune-base-model-pilot-phase1/specs/wave-3-finetune-base-model/spec.md` (D1/D3/D7 Requirements + Scenarios)
- [ ] **Step 2.6:** Run `openspec validate wave-3-finetune-base-model-pilot-phase1 --strict` → Expected "Change is valid" (AC-3)
- [ ] **Step 2.7:** Pre-implementation Metis + Oracle 并行审查 (background `task(subagent_type="metis")` + `task(subagent_type="oracle")`) — **决策点 #2: D3 schema migration 选项 (A/B/C)** + **决策点 #3: LLMProviderFactory API 名称 (`register_dynamic` vs `register_provider`)**

### Step 3 — TDD RED: 2 个失败测试 (1h)

- [ ] **Step 3.1:** Create `tests/test_training_data_pipeline.cpp` (RED — 编译因 `scripts/prepare_training_data.py` 未实装 FAIL)
  - Test cases (≥1):
    - `prepare_training_data loads ADR-0074 D6 JSONL and adds source field` (parse_valid=true, source=baseline)
    - `prepare_training_data filters parse_valid && task_success` (过滤后 records 数 ≥ 1)
- [ ] **Step 3.2:** Create `tests/test_llm_provider_factory.cpp` (RED — 编译因 `LLMProviderFactory::register_dynamic` + `FinetuneBaseModelProvider` 未实装 FAIL)
  - Test cases (≥1):
    - `register_dynamic registers fine-tune provider and instantiation returns stub` (available_models() 非空 + generate() 返回 failure "Phase 2 deferred")
- [ ] **Step 3.3:** Run `cmake --build build --target test_training_data_pipeline test_llm_provider_factory` → Expected: 编译失败 (RED)

### Step 4 — GREEN: scripts/prepare_training_data.py + FinetuneBaseModelProvider stub (2h)

- [ ] **Step 4.1:** Create `scripts/prepare_training_data.py` (D3 schema migration 选项敲定后实装 — **决策点 #1** 引用)
  - 加载 `data/adr-0074-d6-baseline.jsonl`
  - 每行加 `source = "baseline"` (默认)
  - 过滤 `parse_valid && task_success`
  - 输出 `data/wave-3-training-data.jsonl`
- [ ] **Step 4.2:** Create `src/common/llm/finetune_provider.h` (Phase 1 stub)
  - `class FinetuneBaseModelProvider : public ILLMProvider`
  - 3 虚函数声明: `generate` / `generate_stream` / `available_models`
  - generate 返回 failure "Phase 2 deferred" (避免 stub 误触发真实推理)
- [ ] **Step 4.3:** Create `src/common/llm/finetune_provider.cpp` (Phase 1 stub 实现)
- [ ] **Step 4.4:** Modify `src/common/llm/llm_provider_factory.cpp`: `register_dynamic("agenticdsl-llama-3.1-70b-lora-v1", factory_fn)` 入口 (实际 API 名称 — **决策点 #3** 引用)
- [ ] **Step 4.5:** Modify `src/common/llm/CMakeLists.txt` 注册新文件
- [ ] **Step 4.6:** Run 2 个测试 → Expected: 编译通过 + 测试 PASS (GREEN)

### Step 5 — D1 评分 yaml 持久化 (1h)

- [ ] **Step 5.1:** Run `tests/test_llm_tool*` (≥1 binary) + `tests/test_cost_tracking_decorator` (cost 数据) — 复用现有 D1 评分基础设施
- [ ] **Step 5.2:** Create `docs/research/wave-3-base-model-selection.md` (≥3 候选模型评分表 + 4 过滤条件验证 + 最终选择 + 论证)
- [ ] **Step 5.3:** 验证 D1 评分 Weighted ≥ 7.5 + 4 过滤条件全过 (per ADR-0078 D1)

### Step 6 — 既有 test 零回归验证 (0.5h)

- [ ] **Step 6.1:** Run focused ctest: `tests/test_training_data_pipeline` + `tests/test_llm_provider_factory` + `tests/test_llm_tool*` + `tests/test_cost_tracking_decorator` + `tests/test_genome_registry` (≥6 binary) → Expected: 全 PASS (AC-7)
- [ ] **Step 6.2:** Run `ctest -N` 计数 → Expected: 新增 2 binary, 无 PDK 跨库影响 (AC-8)

### Step 7 — 1 atomic commit + Day-5 trap guard (0.5h)

- [ ] **Step 7.1:** Run `git status` + `git add` (production + test + docs + scripts) — **defer commit to archive stage** (per AGENTS.md mode #4 atomicity)
- [ ] **Step 7.2:** archive 5 文件: `mkdir -p openspec/changes/archive/wave-3-finetune-base-model-pilot-phase1-2026-09-23 && cp openspec/changes/wave-3-finetune-base-model-pilot-phase1/{.openspec.yaml,proposal.md,design.md,tasks.md,specs/wave-3-finetune-base-model/spec.md} openspec/changes/archive/wave-3-finetune-base-model-pilot-phase1-2026-09-23/` + `git rm` 活跃 change dir
- [ ] **Step 7.3:** Verify Day-5 trap guard: `git ls-files openspec/changes/archive/wave-3-finetune-base-model-pilot-phase1-2026-09-23/` → Expected: 5 文件全在 (`.openspec.yaml` + `proposal.md` + `design.md` + `tasks.md` + `specs/<name>/spec.md`) (AC-10)
- [ ] **Step 7.4:** Run `openspec validate --strict` (with archived change in archive/) → Expected: PASS

### Step 8 — Oracle post-impl SHIP-with-fixes 复评 (1h)

- [ ] **Step 8.1:** 主会话派 Oracle (background `task(subagent_type="oracle", run_in_background=true)`, prompt 引用 `.rddf/state/builder/wave-3-finetune-base-model.json::post_impl_review_prompt`)
- [ ] **Step 8.2:** 等待 `<system-reminder>` notification → collect Oracle verdict via `background_output(task_id="bg_...")`
- [ ] **Step 8.3:** Apply SHIP-with-fixes fixes (per verdict + Critical/Major 排序清单) → 新增 atomic commit (mode #4 atomicity, 不 amend baseline)
- [ ] **Step 8.4:** 验证 fix commit 不破坏既有 test (focused ctest 再次跑)

### Step 9 — merge → main + cleanup worktree + AGENTS.md sync (0.5h)

- [ ] **Step 9.1:** `git worktree remove --force .rddf/wt/wave-3-finetune-base-model/` + `git branch -D feat/wave-3-finetune-base-model`
- [ ] **Step 9.2:** Merge worktree branch to main (`git checkout main && git merge --no-ff feat/wave-3-finetune-base-model -m "merge Wave 3 finetune-base-model Phase 1 per Oracle SHIP verdict"`) — 保留 worktree branch history per AGENTS.md mode #11 step 7
- [ ] **Step 9.3:** Re-build main working tree (`cmake --build build -j$(nproc)`) — worktree merge 不自动 sync main working tree (per AGENTS.md mode #11 教训)
- [ ] **Step 9.4:** Modify `AGENTS.md` Recent Changes 条目 (Wave 3 Phase 1 ship — commit hash + archive path + Oracle verdict + 12 AC 验证)
- [ ] **Step 9.5:** Modify `.rddf/state/builder/wave-3-finetune-base-model.json::post_impl_execute_summary` + `post_impl_review_outcome` (主会话补, per AGENTS.md mode #11 step 8)

---

## 决策点 (P1 plan 阶段必敲定)

### 决策点 #1: D3 schema migration 选项 (A/B/C)

per improvement draft §"What Changes" + LLM concern #1:

| 选项 | 描述 | 优 | 劣 |
|---|---|---|---|
| **A** | 改造 `tools/prompt/export_training_data.py` 扩展 schema, 加 `source` + ADR-0078 假设字段 (`dsl_version`/`schema_snapshot_hash`/`stage_1_selected`/`parse_valid`/`task_success`) | 满足 ADR-0078 治理意图 | 需要 schema migration 测试 + backward compat, 复杂度高 |
| **B** | 接受 ADR-0074 V1 实际 schema, Wave 3 微调 ADR-0078 ADR | 简单, 低风险 | 失去 ADR-0078 部分治理价值 |
| **C** | 双 schema 并存: 训练用 ADR-0074 V1, 评估用 ADR-0078 假设字段 | 解耦, 灵活 | 增加 complexity, 需同步维护两套 schema |

**默认推荐**: 选项 A (满足 ADR-0078 治理 + 添加 `source` 字段 + 兼容 ADR-0074 V1 既有数据). **dual-agent review 时再决策**.

### 决策点 #2: D3 schema migration 选项 final (dual-agent review 后)

per Step 2.7 — Metis + Oracle 双审查给出建议, **主会话最终敲定**.

### 决策点 #3: LLMProviderFactory API 名称

per LLM concern #2:
- **实际 API**: `register_dynamic(name, DynamicFactoryFn)` (per `src/common/llm/llm_provider_factory.h:33`)
- **improvement draft AC-6 笔误**: `register_provider`

**默认推荐**: 使用 `register_dynamic` (实际 API), proposal/design/tasks/spec 全部统一为 `register_dynamic`. **dual-agent review 时验证**.

---

## MUST DO (执行红线)

- 工作目录: `.rddf/wt/wave-3-finetune-base-model/` (per `.rddf/wt/` 已 ignore)
- TDD 5 步: 先写 failing test → 验证 fail → 实施最小代码 → 验证 pass → defer commit
- 仅 1 atomic commit per AGENTS.md 模式 #4 (`feat(llm): Wave 3 finetune-base-model pilot phase 1`)
- `lsp_diagnostics` 全部改动文件零错误 (post-Step 4)
- 实施完成后必派 Oracle post-impl `SHIP-with-fixes` 复评 (per user 决策 + mode #11 闭环)
- Day-5 trap 防御: archive 时 `git ls-files` 5 文件验证
- 24h cooling-off 链式合规: Pre-Wave3 (G2 merge dc12a17) ✅ → Wave 3 cooling-off 自 Wave 3 merge 起算 (AC-12)

## MUST NOT DO

- ❌ 不要碰 ADR-0071/0074/0076/0077 主体 (4 个上游 ADR 独立 OpenSpec change)
- ❌ 不要开 new PDK plugin (Fine-tune model 作为现有 ILLMProvider 接口实现)
- ❌ 不要 amend 上次 commit (Atomic commits 不能 amend)
- ❌ 不要在 main 直接修改 (违反 worktree-discipline)
- ❌ 不要实施 D4-D7 (训练方法/评估/回流/serving-Phase 2) — Wave 3 Phase 1 边界
- ❌ 不要引入新外部依赖 (DeepSeek API / HF TRL / PEFT) — 不增加 build complexity
- ❌ 不要在 main 上改动 .rddf/state/builder/*.json (builder state 是 mode #11 pre-flight 字段, worktree 期间不冲突)

---

## NOT-VERIFIED (主会话 post-merge 必补)

- 全量 ctest 252 binaries 零回归 (G1+G2+G3+G4 + Wave 3 Phase 1 都需验证)
- TSan 跑 (机器性能受限跳过, 同 G1/G4 NOT-VERIFIED 项)
- D1 评分 yaml 的实际候选模型 benchmark 数据 (依赖 llm-tool-eval 实时跑, 不在本 change 范围)

---

## Related

- **Improvement**: `.rddf/improvements/wave-3-finetune-base-model.md` (139 lines, 5-segment)
- **Builder handoff**: `.rddf/state/builder/wave-3-finetune-base-model.json` (P0 approved, post_impl_review_prompt saved)
- **Planner handoff**: `.rddf/state/.planner-handoff.json::recommended_route=complex`
- **AGENTS.md**: 模式 #4 (atomicity) + 模式 #11 (Async Worker + Dual Oracle dual-review SHIP-with-fixes cycle)
- **Pre-Wave3 Plan**: `.rddf/roadmap/2026-09-21-pre-wave3-to-wave3-execution-plan.md` §2 (4-Gate 序列) + §3 (Wave 3 立项依据)
- **ADR-0078**: `docs/adr/adr-0078-finetune-base-model.md` (Step 1 翻牌目标)
- **ADR-0074**: V1 实际 schema (D3 数据源) + D6 baseline JSONL export

---

*Plan 版本: v1.0*
*生成日期: 2026-09-22 (cooling-off override 后)*
*执行触发: 主会话完成 P0 approve + 启动 P1 plan gen*
*下一步: P1.5 deps gate → P2 worktree + execute (mode #11 async worker)*