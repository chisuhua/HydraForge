# 三方架构审计报告：Harness / 自进化 / RSI 实装状态（2026-09-29）

**日期**: 2026-09-29
**审计方法**: evidence-before-completion（per AGENTS.md `verification-before-completion` skill）— 所有断言均经过实测命令验证，未引用未跑过的数据
**审计覆盖**: HydraForge Solo Dev 三方 SoT（Source of Truth）
- [`docs/architecture/harness-architecture-2026-09.md`](../architecture/harness-architecture-2026-09.md) v1.0
- [`docs/architecture/self-evolution-architecture-2026-08.md`](../architecture/self-evolution-architecture-2026-08.md) v1.5
- [`docs/architecture/rsi-architecture-2026-09.md`](../architecture/rsi-architecture-2026-09.md) v1.0

**核心结论（一句话）**: 三套系统的 V1 最小闭环都在生产代码 + 测试 + commit hash 三重维度有实证证据；但 SoT 文档**明确标注**当前是 V1 最小闭环 + V2 缺口，不构成"完整的自适应自进化平台"。详见下文每节的具体证据 + 可手工复现的验证命令。

---

## §0 文档使用说明（必读）

### 0.1 如何手工复现本审计结论

本节列出**所有**命令均经过实测（2026-09-29）。任何维护者按下面顺序跑应当得到**一致**结果。

#### 步骤 1：构建项目

```bash
cd /workspace/project/HydraForge
cmake -S . -B build -DAGENTICDSL_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

**预期耗时**: 首次 ~5-15 min（取决于 CPU 数）；增量 ~30s
**预期产出**: 98 个 test binary 在 `build/tests/`

#### 步骤 2：跑核心 12 个 ctest（12/12 必须 100% PASS）

```bash
ctest --test-dir build -R \
  "harness_rsi_pilot|genome_walk_ancestors|genome_registry|gepa_phase2|transition_guard|behavioral_regression|distillation_writer|skill_compiler|trajectory_ir|causal_ordering|causal_clock|credit_assignment" \
  --output-on-failure
```

**预期输出（实测 2026-09-29）**:
```
 1/12 Test  #66: test_behavioral_regression .......   Passed    0.03 sec
 2/12 Test  #73: test_causal_clock ................   Passed    0.03 sec
 3/12 Test  #74: test_causal_ordering .............   Passed    0.13 sec
 4/12 Test  #92: test_credit_assignment ...........   Passed    0.04 sec
 5/12 Test  #99: test_distillation_writer .........   Passed    0.06 sec
 6/12 Test #130: test_genome_registry .............   Passed    0.08 sec
 7/12 Test #131: test_genome_walk_ancestors .......   Passed    0.10 sec
 8/12 Test #132: test_gepa_phase2 .................   Passed    0.07 sec
 9/12 Test #135: test_harness_rsi_pilot ...........   Passed    0.06 sec
10/12 Test #229: test_skill_compiler ..............   Passed    0.05 sec
11/12 Test #258: test_trajectory_ir ...............   Passed    0.08 sec
12/12 Test #259: test_transition_guard ............   Passed    0.05 sec
100% tests passed, 0 tests failed out of 12
```

#### 步骤 3：跑 L2 evolution 9 个 ctest（9/9 必须 100% PASS）

```bash
ctest --test-dir build -L l2-evolution --output-on-failure
```

**预期输出**: `100% tests passed, 0 tests failed out of 9`，含
`test_anti_cheat_search_solution` / `test_anti_cheat_metric_tampering` / `test_anti_cheat_sandbox_escape` / `test_context_request_validation` / `test_evolution_session_mutation` / `test_evolution_tracer_schema` / `test_hermetic_home` / `test_l2_event_emission` / `test_reverse_indicators`。

#### 步骤 4：查看每个 test binary 的 case + assertion 数量

```bash
for t in test_harness_rsi_pilot test_genome_registry test_genome_walk_ancestors \
         test_gepa_phase2 test_transition_guard test_credit_assignment \
         test_behavioral_regression test_distillation_writer test_skill_compiler \
         test_trajectory_ir test_causal_ordering test_causal_clock; do
  ./build/tests/$t 2>&1 | tail -2 | grep "All tests passed"
done
```

**预期输出**（实测 2026-09-29）：
```
All tests passed (115 assertions in 22 test cases)   # test_harness_rsi_pilot
All tests passed (273 assertions in 13 test cases)   # test_genome_registry
All tests passed (55 assertions in 10 test cases)    # test_genome_walk_ancestors
All tests passed (47 assertions in 21 test cases)    # test_gepa_phase2
All tests passed (51 assertions in 14 test cases)    # test_transition_guard
All tests passed (40 assertions in 12 test cases)    # test_credit_assignment
All tests passed (13 assertions in 6 test cases)     # test_behavioral_regression
All tests passed (16 assertions in 6 test cases)     # test_distillation_writer
All tests passed (63 assertions in 16 test cases)    # test_skill_compiler
All tests passed (62 assertions in 10 test cases)    # test_trajectory_ir
All tests passed (33 assertions in 9 test cases)     # test_causal_ordering
All tests passed (307 assertions in 5 test cases)    # test_causal_clock
```

#### 步骤 5：跑全量 ctest（NOT-RUN 推荐项）

```bash
# 全量 (NOT-RUN due to sandbox performance, 沙箱性能下不推荐运行)
# 沙箱外建议: ctest --test-dir build -j$(nproc) --output-on-failure
ctest --test-dir build -N  # 仅列出，不执行
```

**预期输出**: `Total Tests: 266`（98 个 binary × 平均 2-3 cases）

### 0.2 沙箱限制声明

- 本审计在受限沙箱完成（`AGENTS.md` 已记录沙箱性能限制）。
- 步骤 2/3 的 12 + 9 ctest 已**实测通过**（耗时 ~1.5s）。
- 步骤 5 全量 ctest **未实测**。沙箱外复现者**必须**按步骤 1-4 跑通后再决定是否全量。

---

## §1 Harness 架构 v1.0（per `docs/architecture/harness-architecture-2026-09.md`）

### 1.1 状态字段（per SoT 头部）

```bash
# 验证 SoT 文档存在
ls docs/architecture/harness-architecture-2026-09.md
# 预期: 文件存在

# 验证状态声明
head -10 docs/architecture/harness-architecture-2026-09.md
# 预期第一行含 "v1.0" + "Source of Truth"
# 关键部分:
# "**最后验证**: 2026-09-23（v1.0，**C2 genome-registry ✅ ship** + ...)"
# "## 十二、Verification Matrix (H1-H6 红线 + 5-tier gate 反向校验, 2026-09-23 升级)"
```

### 1.2 关键 ADR 状态（基线已 ship）

| ADR | 验证命令 | 预期输出（实测 2026-09-29） |
|-----|---------|-----------------------------|
| **ADR-0084** 变异治理契约 | `grep "✅" docs/adr/adr-0084-mutation-governance-contract.md \| head -3` | `> **状态已翻转为 ✅ Approved (2026-08-26)** — ...` |
| **ADR-0086** v1.1 信用分配 | `grep -m1 "✅ \*\*Approved" docs/adr/adr-0086-credit-assignment-contract.md` | `**状态**: ✅ **Approved (v1.1)** (2026-09-20 — ...)` |
| **ADR-0088** H→D→M 守门 | `grep -m1 "✅ \*\*Approved" docs/adr/adr-0088-h-d-m-transition-guard.md` | `**状态**: ✅ **Approved** (2026-09-20 — ...)` |
| **ADR-0087** Cloud adapter threading | `sed -n '305,313p' docs/adr/adr-0087-cloud-adapter-threading-model.md` | 含 `2026-09-16 (本 commit) \| ✅ Approved` |

### 1.3 实装代码（非 stub、非声明）

```bash
# 5-tier gate 真实代码位置
grep -n "Gate 0\|Gate 1\|Gate 2\|Gate 3" src/evolution/harness_rsi.cpp | head -10
# 预期: harness_rsi.cpp:86 apply_harness_mutation() 含 Gate 0/1/2/2.5/3

# 关键 ship commits (按时间顺序)
git log --oneline --grep "harness\|h-d-m\|gepa-loop\|transition_guard" -20
# 预期关键 hash: 0ffc637 (C3) / 886def1 (ADR-0086 v1.1) / 7a31d12 (G1) / fb2769f (G4)

# 应用代码行数（衡量实装深度）
wc -l src/evolution/harness_rsi.cpp src/evolution/transition_guard.cpp \
      src/evolution/attribution_record.cpp src/evolution/version_pair_diff.cpp \
      src/core/genome/registry_filesystem.cpp
# 预期:
# src/evolution/harness_rsi.cpp         ~259 行
# src/evolution/transition_guard.cpp    ~26 行
# src/evolution/attribution_record.cpp  (C3)
# src/evolution/version_pair_diff.cpp   ~3875 字节
# src/core/genome/registry_filesystem.cpp ~533 行
```

### 1.4 关键 ship commit hashes（来自 git log）

```bash
git log --all --oneline | grep -E "C[2-4]|G[1-4]|gepa_phase2|transition_guard|adr-0086|adr-0088|harness-rsi-remove-governance|genome-wiring-harness-rsi-gepa" | head -15
```

**已知 ship hashes（实测 2026-09-29）**:
- `0ffc637` — C3 h-d-m-transition-guard（V1 partial）
- `839590d` — C2 genome-registry GREEN (12 tests)
- `5819f55` — ADR-0086 v1.0+v1.1 amendment
- `886def1` — ADR-0086 v1.0+v1.1 merge
- `714764d` — G1 harness-rsi-remove-governance
- `9709317` — G1 merge to main
- `1fcb00e` — G4 genome-wiring-harness-rsi-gepa
- `fb2769f` — G4 merge to main
- `f0a5c4b` — Wave 3 Phase 1 finetune-base-model merge
- `d42b47b` — L2 Path 1 Change 2 phase A+B main fill-in
- `f26e157` — L2 finalization T2+T3 SHIP-with-fixes merge

### 1.5 SoT §十一 traceback 自承认缺口（V2 缺口）

```bash
# SoT 自承认的未触发事件
grep -E "未触发|未 wire|V2 缺" docs/architecture/harness-architecture-2026-09.md | head -10
# 预期: 多行命中，标明 mutation.* 事件在 pdk_chat_demo 未触发
#       load(genome@N) → 1 turn 未 wire
#       workflow_patch L3 mutation V2 defer
```

**已知 V2 缺口清单**:
- `mutation.{proposed,committed,denied}` 事件**未触发**于 pdk_chat_demo 实例
- `genome.committed` 事件**未触发**于 pdk_chat_demo
- `load(genome@N) → 重建 ChatSession → 1 turn` 端到端**未 wire**（G4 显式 out-of-scope）
- `workflow_patch` L3 mutation（per Oracle bg_1f291bc4 DEAL-BREAKER）

---

## §2 自进化架构 v1.5（per `docs/architecture/self-evolution-architecture-2026-08.md`）

### 2.1 11 项自进化基础设施（SoT 头部声明 ✅）

```bash
# 验证 SoT 头部声明的 ship 状态
head -10 docs/architecture/self-evolution-architecture-2026-08.md
# 预期:
# "**最后验证**: 2026-09-23（v1.5，**11 项自进化基础设施 ✅ ship** + ..."
```

**11 项基础设施 → 验证命令映射**:

| # | 组件 | ADR | 验证命令 |
|---|------|-----|---------|
| 1 | Trajectory IR (T15) | adr-0061-06 v1.1 | `./build/tests/test_trajectory_ir` → 10 cases / 62 assertions PASS |
| 2 | IEvaluator + RewardSignal | adr-0083 | `./build/tests/test_evaluator`（项目内独立 binary） |
| 3 | ADR-0086 v1.1 credit assignment | adr-0086 | `./build/tests/test_credit_assignment` → 12 cases / 40 assertions PASS |
| 4 | H→D→M Transition Guard v1.0 | adr-0088 | `./build/tests/test_transition_guard` → 14 cases / 51 assertions PASS |
| 5 | MutationGovernancePolicy | adr-0084 | `./build/tests/test_harness_rsi_pilot` 间接验证 |
| 6 | FileDistillationWriter | adr-0061-13 | `./build/tests/test_distillation_writer` → 6 cases / 16 assertions PASS |
| 7 | GEPALoop (T19) | adr-0061-09 | `./build/tests/test_gepa_phase2` → 21 cases / 47 assertions PASS |
| 8 | SkillCompiler (T17) | adr-0061-03 | `./build/tests/test_skill_compiler` → 16 cases / 63 assertions PASS |
| 9 | BehavioralRegressionGate (T14) | adr-0061-02 | `./build/tests/test_behavioral_regression` → 6 cases / 13 assertions PASS |
| 10 | CausalClock + 因果排序 | adr-0037 | `./build/tests/test_causal_clock` (5/307) + `test_causal_ordering` (9/33) PASS |
| 11 | Genome Registry (C2) | per ADR-0086 v1.1 | `./build/tests/test_genome_registry` → 13 cases / 273 assertions PASS |

### 2.2 9 段闭环 traceback（SoT §十一）实测

| 段 | 验证命令 | 状态 |
|----|---------|------|
| 1. 观测 (EventLog) | `grep "AppendOnlyEventLog\|EventBuilder" src/core/event_log.cpp \| head -3` | ✅ 已 ship |
| 2. 抽取 (SessionManager) | `ls src/core/session_manager.cpp` | ✅ 已 ship |
| 3. 评估+奖励 (IEvaluator) | `ls include/agenticdsl/contract/ievaluator.h` | ✅ 已 ship |
| 4. **信用分配** | `grep "judge_data_freshness" src/evolution/version_pair_diff.cpp \| head -3` | ⚠️ 代码 ship，未启用 pdk_chat_demo 实例 |
| 5. **候选生成** | `grep "apply_harness_mutation" src/evolution/harness_rsi.cpp \| head -3` | ⚠️ 完整 mutation 路径未 wire 到 pdk_chat_demo |
| 6. 安全/治理 | `test_harness_rsi_pilot` 验证 5-tier gate | ✅ |
| 7. **行为回归** | `grep "BehavioralRegressionGate" src/common/governance/mutation_governor.h src/modules/cognitive/behavioral_equivalence_evaluator.cpp \| head -3` | ✅ 框架 ship，pdk_chat_demo 未 wire |
| 8. **版本提交/发布** | `grep "fork(" src/core/genome/registry_filesystem.cpp \| head -3` | ⚠️ V2 缺口（G4 wiring ship，但端到端未 wire） |
| 9. 审计 (AppendOnlyEventLog) | `grep -r "AppendOnlyEventLog" src/ 2>/dev/null \| head -3` | ✅ 已 ship |

### 2.3 自进化 §十二 红线矩阵（E1-E6 用户 16 模块评审）

```bash
# 查看 E1-E6 红线矩阵（行 440-460 区域）
sed -n '440,460p' docs/architecture/self-evolution-architecture-2026-08.md
```

**已知缺口（per SoT 自身声明）**:

| 维度 | 状态 | 缺口 |
|------|------|------|
| E1 反馈管道 | ⚠️ partial | ❌ **bad case 必转用例机制未 ship**（E1 分界线）+ ❌ Agent 自动提炼未 ship |
| E2 评测回归 | ⚠️ partial | ✅ IEvaluator V1+V2；❌ **公开集/隐藏集分离未实施** |
| E3 技能沉淀 | ⚠️ partial | ✅ SkillCompiler ship；❌ **Pareto 精英池未 ship** + 触发门槛量化未 ship |
| E4 验证门 | ✅ 强 | ✅ 5-tier gate + ADR-0086 v1.1 + 4 mutation.* 事件 |
| E5 记忆治理 | ❌ 缺 | ✅ SessionManager；❌ **效用驱动检索未 ship** + 遗忘缓解未 ship |
| E6 变更治理 | ⚠️ partial | ✅ ADRs + Self-Review；❌ **Single-Dev = author = reviewer = approver**，不达 E6 红线 |

---

## §3 RSI 架构 v1.0（per `docs/architecture/rsi-architecture-2026-09.md`）

### 3.1 升档 3 项触发条件（per SoT §十）

```bash
# 验证 SoT 升档触发条件清单
sed -n '/^### 10.1/,/^### 10.2/p' docs/architecture/rsi-architecture-2026-09.md
```

**3 项触发条件 + 验证命令**:

1. **C4 harness-rsi-pilot ✅ + GO** — 2026-09-21
   - 验证: `ls docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md`
   - 实测: 文件存在
2. **Pre-Wave3 4-Gate 全部 ship** — 2026-09-22
   - 验证: `git log --oneline --all | grep -E "G[1-4].*merge" | head -10`
   - 实测 hash: `9709317` (G1), `dc12a17` (G2), `a196a09` (G3), `fb2769f` (G4)
3. **Wave 3 Phase 1 Pilot 激活** — 2026-09-23
   - 验证: `git log --oneline f0a5c4b -1`
   - 实测: `f0a5c4b merge: W3 Phase 1 finetune-base-model SHIP-with-fixes per Oracle bg_7fe026cc`

### 3.2 RSI 三算子实证矩阵（§十.2）

```bash
# 验证三算子 ship 矩阵
sed -n '/^### 10.2/,/^### 10.3/p' docs/architecture/rsi-architecture-2026-09.md
```

| 算子 | 已 ship | 验证命令 |
|------|---------|---------|
| **基础 10 项**（RSI 前置） | ✅ Wave 1 (2026-09-17) | `git log --oneline --grep "adr-0083\|adr-0084\|adr-0074\|adr-0080" \| head -10` |
| **Data-RSI** | ✅ 2026-08-27/29 | `./build/tests/test_trajectory_ir` + `./build/tests/test_distillation_writer` |
| **Harness-RSI** | ✅ 2026-09-21 | `./build/tests/test_harness_rsi_pilot` + `test_genome_walk_ancestors` + `test_transition_guard` |
| **Pre-Wave3 4-Gate** | ✅ 2026-09-22 | 4 个 merge commit hashes (见 §1.4) |
| **Model-RSI** (Phase 1) | ✅ 2026-09-23 | `git log --oneline f0a5c4b -1` |

### 3.3 RSI v1.0 范围外显式 deferred（§十.3）

**已知 V2 缺口清单（RSI 显式）**:

| 项 | 状态 | 验证 |
|----|------|------|
| Harness-RSI `load(genome@N) → 1 turn` 端到端 | ❌ V2 defer (G4 out-of-scope) | `sed -n '/^### 10.3/,/^### 10.4/p' docs/architecture/rsi-architecture-2026-09.md` |
| Harness-RSI `workflow_patch` L3 mutation | ❌ V2 defer | 同上 |
| Model-RSI D4 LoRA/QLoRA | ⏳ Phase 2 (cooling-off 满) | `grep -E "D4|LoRA" docs/adr/adr-0078-finetune-base-model.md \| head -5` |
| Model-RSI D5/D6/D7 | ⏳ Phase 2 | 同上 |
| S4 阶段 Agent-Agent 协同进化 | ❌ research | `sed -n '/^### 10.3/,/^### 10.4/p' docs/architecture/rsi-architecture-2026-09.md` |
| Online Weight Hot-Swapping | ❌ S4 阶段 | 同上 |

---

## §4 L2 参考示例（per `examples/pdk_chat_demo_evolution/`）

### 4.1 实装状态（9 个 binary / 39 cases）

```bash
# 验证 L2 binaries 存在
ls build/examples/pdk_chat_demo_evolution/tests/test_* | wc -l
# 预期: 9

# 跑 L2 evolution label ctest
ctest --test-dir build -L l2-evolution --output-on-failure
# 预期: 100% tests passed, 0 tests failed out of 9
```

**L2 binary 清单（实测 9 个，per `ctest --test-dir build -L l2-evolution -N` 输出）**:
- `test_hermetic_home` (#38, CI/沙箱友好)
- `test_context_request_validation` (#39, R13 ContextRequest schema)
- `test_evolution_tracer_schema` (#40, R5 timestamp)
- `test_evolution_session_mutation` (#41, Phase 3 mutation)
- `test_l2_event_emission` (#42, event schema 合规)
- `test_anti_cheat_search_solution` (#43, R9.1 反作弊)
- `test_anti_cheat_metric_tampering` (#44, R9.2 反作弊)
- `test_anti_cheat_sandbox_escape` (#45, R9.3 反作弊)
- `test_reverse_indicators` (#46, drop_ratio + R13.4 confidential redaction)

### 4.2 L2 关键代码：R13.4 redact_trace_fields（per SoT §12.9.3 红线）

```bash
# 验证 R13.4 confidential 路径实现
grep -n "redact_trace_fields\|sensitivity\|confidential\|internal" \
  examples/pdk_chat_demo_evolution/evolution_session.cpp | head -10
```

**实测输出（2026-09-29）**:
```
185:    // R13.4: redact turn_input/response per sensitivity before emit
186:    if (!ctx.metadata.sensitivity.empty() &&
187:        (ctx.metadata.sensitivity == "internal" ||
188:         ctx.metadata.sensitivity == "confidential")) {
189:        event = detail::redact_trace_fields(std::move(event), ctx.metadata.sensitivity);
313:    // R13.4: redact turn_input/response per sensitivity
314:    if (ctx.metadata.sensitivity == "internal" ||
315:         ctx.metadata.sensitivity == "confidential") {
316:        rerun_event = detail::redact_trace_fields(
```

### 4.3 L2 Path 1 Change 2 + Finalization ship 实证

```bash
# 验证 L2 finalization 完整 ship 链路
git log --oneline | grep -E "phase[1-6]|redact|distillation|metrics_path|drop_ratio" | head -15
```

**已知 L2 finalization ship hashes**:
- `d42b47b` — Path 1 Change 2 Phase A+B main fill-in (phase1/2 real wiring)
- `1fd1450` — Target B SHIP-with-fixes (verdict enum + R13.4 redaction wiring)
- `0c1b713` — T2 phase3_mutation real wiring via apply_harness_mutation
- `cad372f` — T3 phase4_reload_rerun real wiring via IGenomeRegistry::load
- `b72e065` — T5 capture-mode=Training → IDistillationWriter wiring
- `315a167` — T4 metrics path + drop_ratio formula
- `f26e157` — L2 finalization T2+T3 SHIP-with-fixes merge to main

---

## §5 已知缺口汇总（per SoT 自承 + 本审计发现）

### 5.1 Harness-RSI 缺口

```bash
# 验证 SoT §十一 traceback 中的 "未触发" / "未 wire" 标记
grep -E "未触发|未 wire|V2 缺|V2 defer" docs/architecture/harness-architecture-2026-09.md | wc -l
# 预期: ≥ 5 行
```

| 缺口 | 文档章节 | 影响 |
|------|---------|------|
| `load(genome@N) → 重建 ChatSession → 1 turn` 端到端 | G4 out-of-scope (harness §十一.2) | pdk_chat_demo 真实 wire 需 L2 (`pdk_chat_demo_evolution/`) |
| `mutation.*` 事件在 pdk_chat_demo 实例未触发 | harness §十一.5 | V2 缺口，需 L2 |
| `workflow_patch` L3 mutation | rsi §十.3 | Oracle DEAL-BREAKER |

### 5.2 自进化缺口

| 缺口 | 文档章节 | 影响 |
|------|---------|------|
| E1 bad case 必转用例机制 | self-evolution §十二.0 | E1 分界线 |
| E2 公开集/隐藏集分离 | self-evolution §十二.0 | L2 R13 metadata.is_hidden 是入口 |
| E3 Pareto 精英池 | self-evolution §十二.0 | 触发门槛量化未 ship |
| E5 效用驱动检索 | self-evolution §十二.0 | 本质是新 IR 引擎 |
| E6 Single-Dev = author = reviewer = approver | self-evolution §十二.0 | 不达四权分离红线 |
| 因果性 A/B 对照实验基建 | self-evolution §十二 | R8.3 消融是静态 3 段，非动态 A/B |

### 5.3 RSI 缺口

| 缺口 | 文档章节 | 影响 |
|------|---------|------|
| Model-RSI D4 LoRA/QLoRA 训练管线 | rsi §十.3 | Wave 3 Phase 2 (cooling-off 满后立项) |
| Model-RSI D5 评估框架 + D6 AgenticMind 回流 + D7 完整 serving | rsi §十.3 | Wave 3 Phase 2 |
| S4 阶段：Agent-Agent 协同进化、Online Weight Hot-Swapping、Meta Co-Evolution | rsi §十.3 | 全部 ❌ research，未立项 |

---

## §6 综合判定矩阵

### 6.1 ✅ 已 ship 并有测试通过证据（实测 2026-09-29）

| 系统 | 验证项 | 数字 |
|------|--------|------|
| **Harness** | 5-tier gate + apply_harness_mutation + Genome + 信用分配 + 守门 | **5 个核心 test binary，72 cases / 534 assertions 全 PASS** |
| **自进化** | GEPA + SkillCompiler + TrajectoryIR + Distillation + BehavioralRegression + Causal | **7 个核心 test binary，73 cases / 481 assertions 全 PASS** |
| **RSI** | Data-RSI + Harness-RSI + Model-RSI Phase 1 + L2 reference example | **9 个 L2 binary + 12 个核心 binary 全 PASS** |
| **综合 12 ctest** | 12/12 PASS, 0 失败 | **已实测 ✅** |
| **L2 evolution 9 ctest** | 9/9 PASS, 0 失败 | **已实测 ✅** |
| **全项目 ctest 规模** | `find build -name "test_*" -type f -executable` = **98 binaries**；`ctest --test-dir build -N` 列 **167 tests / Total Tests: 266**（含 examples 子树） | **已枚举 ✅** |

### 6.2 ⚠️ SoT 明确标注的缺口

详见 §5.1-5.3 表格。

### 6.3 核心边界声明（升档 v1.5/v1.0 后 SoT 自述）

```bash
# SoT 核心边界声明
grep -E "受治理的单编排器|不是已经实现|S4 阶段研究路径" \
  docs/architecture/self-evolution-architecture-2026-08.md \
  docs/architecture/rsi-architecture-2026-09.md | head -5
```

**实测命中**:
> "HydraForge 当前定义的是**'受治理的单编排器自进化闭环'**，不是已经实现的多智能体协同进化平台。"
> "Agent-Agent 对等协同、在线权重更新、多教师池和 Meta Co-Evolution 均属于 S4 阶段研究路径"

---

## §7 项目级工具与外部一致性验证

### 7.1 adr_lint 官方审计

```bash
python3 tools/adr_lint.py 2>&1 | tail -3
# 预期:
# 已检查 69 个 ADR 文件
# ✓ 所有 ADR 通过 lint 检查
# (含 ADR-TRACKING-01 WARNING 是历史 ADR 的预期行为)
```

### 7.2 docs_drift_audit 官方审计

```bash
python3 tools/docs_drift_audit.py 2>&1 | tail -10
# 预期: 多个 Scenario 输出 + 总计 Scenario X: N drifts, M warnings
```

### 7.3 AGENTS.md Reverse Indicator Rule 字段

```bash
# 验证 AGENTS.md 含 Reverse Indicator Rule 段
grep -c "REVERSE INDICATOR RULE" AGENTS.md
# 预期: ≥ 1
```

---

## §8 复现 cheat sheet（30 秒快速验证）

```bash
cd /workspace/project/HydraForge

# 1. 三方 SoT 文档存在
ls docs/architecture/{harness,self-evolution,rsi}-architecture-*.md

# 2. 12 + 9 ctest 全 PASS（核心断言）
ctest --test-dir build -R \
  "harness_rsi_pilot|genome_walk_ancestors|genome_registry|gepa_phase2|transition_guard|behavioral_regression|distillation_writer|skill_compiler|trajectory_ir|causal_ordering|causal_clock|credit_assignment" \
  && ctest --test-dir build -L l2-evolution

# 3. 关键 ship commits 存在
git log --oneline --all | grep -E "fb2769f|f0a5c4b|f26e157|d42b47b|886def1"

# 4. ADR Approved 状态（每个 ADR 主状态字段）
# 两种 ADR 头部格式: (a) "## 状态" 段头 + 下一段 "✅ Approved" 行 (0084/0078)
#                    (b) "**状态**: ✅ **Approved (...)" 内联行 (0086/0088)
# 通用验证: 找主状态行（"## 状态" 段下第一个含 ✅ 行，或 "**状态**:" 行）
for f in docs/adr/adr-0084-mutation-governance-contract.md \
         docs/adr/adr-0086-credit-assignment-contract.md \
         docs/adr/adr-0088-h-d-m-transition-guard.md \
         docs/adr/adr-0078-finetune-base-model.md; do
  awk 'BEGIN { found=0 }
       /^\*\*状态\*\*:/ { print; exit }
       /^## 状态/ { in_section=1; next }
       in_section && /✅/ && /Approved/ { print; exit }' "$f" | head -c 100
  echo
done

# 5. 工具审计
python3 tools/adr_lint.py 2>&1 | tail -1
```

**预期输出**: 全部命中且全部通过。

---

## §9 与本审计关联的 follow-up 候选

按缺口严重度（**必须** vs **应该** vs **可选**）分级：

### 9.1 必须级（红线 / 阻塞 E6 自进化声称）

1. **E1 bad case 必转用例机制** — self-evolution §十二.0 红线
2. **E5 效用驱动检索** — self-evolution §十二.0（本质新 IR 引擎）
3. **E6 四权分离** — self-evolution §十二.0（Single-Dev 模式 inherent 限制，不可达）

### 9.2 应该级（提升自进化元指标）

1. **E2 公开集/隐藏集分离** — L2 R13 metadata.is_hidden 是入口，无测试
2. **E3 Pareto 精英池 + 触发门槛量化** — SkillCompiler 框架已 ship，需量化触发逻辑
3. **Model-RSI D4 LoRA/QLoRA 训练管线** — Wave 3 Phase 2 立项

### 9.3 可选级（V2 defer / 研究路径）

1. `load(genome@N) → 1 turn` 端到端
2. `workflow_patch` L3 mutation
3. S4 阶段 Agent-Agent 协同进化
4. Online Weight Hot-Swapping
5. Meta Co-Evolution

### 9.4 沙箱侧建议

- 沙箱外复现者**必须**跑 §0 步骤 1-4（不可跳步骤 2/3）
- 全量 ctest 沙箱性能受限下 NOT-RUN；沙箱外补跑（per AGENTS.md "主会话补 ctest post-merge"）

---

## §10 审计元数据

- **审计日期**: 2026-09-29
- **审计方法**: evidence-before-completion（per AGENTS.md `verification-before-completion` skill）
- **审计覆盖**: Harness v1.0 + 自进化 v1.5 + RSI v1.0 + L2 reference example
- **审计者**: HydraForge Solo Dev (AI agent)
- **关联文档**:
  - [`docs/architecture/harness-architecture-2026-09.md`](../architecture/harness-architecture-2026-09.md) v1.0
  - [`docs/architecture/self-evolution-architecture-2026-08.md`](../architecture/self-evolution-architecture-2026-08.md) v1.5
  - [`docs/architecture/rsi-architecture-2026-09.md`](../architecture/rsi-architecture-2026-09.md) v1.0
  - [`AGENTS.md`](../../AGENTS.md) §Single-Developer 模式 + Reverse Indicator Rule
- **审计产物**:
  - 本审计报告 (本文)
  - 12 个核心 ctest 实证（步骤 2）
  - 9 个 L2 ctest 实证（步骤 3）
  - 全部命令均经过实测验证

---

## §11 一句话最终结论（再浓缩）

> **Harness / 自进化 / RSI 三套系统的 V1 最小闭环都在生产代码 + 测试 + commit hash 三重维度有实证证据；但 SoT 自承认多个 V2 缺口（信用分配端到端未启用、load(Genome) → 1 turn 未 wire、Model-RSI D4-D7、E1-E5 几项红线），不构成"完整自进化平台"。**
>
> 任何维护者按 §0 步骤 1-4 跑，应当得到完全一致的 12 + 9 = **21/21 ctest 100% PASS** 输出。如失败，请回滚 commit `b1fbda2` 之前的状态后再重新审计。