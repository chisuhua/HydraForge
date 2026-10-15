# harness-mutation-commit-real-llm

> **来源**: 2026-10-06 全量回归测试完备性审查 (AGENTS.md Pattern #10 hygiene 提案, G4 盲点) + 2026-10-14 dual-agent 调研 (Oracle `ses_ec5f38a92ffeLuZb4aIXmev0j3` + Metis `ses_ec5f36ffaffeT3KEsoFqAUPq43`)
> **生成**: 2026-10-06 via completeness-audit, revised 2026-10-15 per rdd-builder P0 case 4
> **rdd-builder P0 状态**: case 4 revise → 2026-10-15 重写 5-segment 格式 (per `add-improve` 5-segment 标准)

**优先级**: 🔴 P0 | **阶段**: post-6c (real LLM 验证层补全)
**分类**: 真实 LLM 验证盲点 (debt-tracking, 补 test coverage)
**主题**: Harness mutation commit + IGenomeRegistry persist 真实 LLM 端到端 (Genome wiring 闭环第 7 环实证)
**状态**: pending (Genome wiring ship 后真实 LLM 闭环未验证, 触发条件已满足: 用户要求 2026-10-14)
**关联**:
- `openspec/changes/archive/genome-wiring-harness-rsi-gepa-2026-09-22/` (闭环第 7 环 ship)
- `tests/test_harness_rsi_pilot.cpp` (22/22 mock, 复用 baseline)
- `tests/test_genome_registry.cpp` (13/13 mock, 复用 baseline)
- `tests/test_harness_mutation_proposal_real_llm.cpp` (1/1 real, 仅 smoke 现有)
- `.rddf/audits/2026-10-14-p0-proposals-dual-agent-review.md` (Phase C 调研产物)
- `.rddf/state/.arch-handoff.json` (6 pre_issues 记录)

## Why

### 真实闭环缺口

Genome wiring 闭环第 7 环"版本提交/发布"端到端 ship (2026-09-22), 但真实 LLM 验证仅覆盖"突变提案"环节 (smoke), 未覆盖完整路径:

```
LLM 生成 prompt_delta
  → MutationGate Gate 0-3 评分
    → IGenomeRegistry fork() persist
      → apply_harness_mutation 内存应用
        → genome.committed 事件 + walk_ancestors 关联
```

### Harness 子系统缺口 (per `harness-architecture-2026-09.md` §十二 H1-H6)

- H1 mutation 提案生成: ✅ real LLM smoke
- H2 mutation 审批 (MutationGate): ⚠️ mock only
- H3 mutation commit + IGenomeRegistry 持久化: ⚠️ mock only
- H4 evolution.readiness.denied: ⚠️ mock only
- H5 反作弊 R9: ⚠️ mock only (见 G5, 独立 change 范围)
- H6 4-tier gate 反向校验 + drop_ratio: ⚠️ mock only

按 AGENTS.md §Reverse Indicator Rule R3 元指标实证要求, 当前"提案生成"环节真实 LLM 验证, **真实 LLM 端到端 commit → persist → reload → re-eval 链路未验证**. 自进化 v1.5 9 段闭环 R3 声称不完整.

### 触发条件 (任一满足)

- ✅ **用户要求** (本次会话提问即触发条件 1)
- ✅ 自进化 v1.5 9 段闭环 §R3 元指标需要实证
- ⏸ Wave 3 Phase 2 D4 LoRA 训练管线启动需要 harness 真实 LLM 闭环数据

### dual-agent 调研强交叉命中 (2026-10-14)

**Oracle + Metis 独立识别**:
- A: 实现路径清晰, 前置依赖全 ship (Genome wiring + must_realllm helper)
- B: "Gate 0-3 真实 LLM 评分" 是误称 — gate 是确定性规则, LLM 只参与提案生成, Case 1/3 描述需 redesign
- C: Case 4 drop_ratio 在 L2 1-2 contexts 粒度物理不可行 (per 2026-10-06 实证, 0/50/100% 二值, 5% 阈值无法稳定构造)
- D: Gate 0-3 是确定性规则 (ToolMetadata 白名单 / 依赖图 / Schema 验证), 非 LLM 判定

**独占发现**:
- E (Metis): env_vars secrets redaction 缺口 (与本 change 正交, 已立 B2)
- F (Metis): kStreamChunkSize=64 UTF-8 切片 bug (与本 change 正交, 已立 B3)
- G (Oracle): sink pointer RAII 生命周期 (与本 change 正交, 已立 B4)
- H (Oracle): hermetic HOME 强制 (per 2026-09-21 C1 genome.key fresh-deploy 教训, **本 change 实施前置**)

## What Changes

### 范围 (In Scope)

**新增 1 个 test binary** (test only, 0 源代码改动):
- `tests/test_harness_mutation_commit_real_llm.cpp` (must_realllm label per 2026-09-29 ship 机制)
- **4 cases 全部 redesign (per dual-agent 共识)**:
  - **Case 1 (commit 成功路径)**: 真实 LLM 生成 mutation proposal → Gate 0-3 确定性规则通过 → `IGenomeRegistry::fork()` persist → `apply_harness_mutation` 内存应用 → **`genome.committed` 事件** 触发 + `walk_ancestors` 验证 (state consistency assertion, 非固定路径)
  - **Case 2 (commit 事件流)**: 真实 LLM mutation commit → `genome.committed` 事件经 InMemoryBus 派发 → `walk_ancestors` 关联新版本 + 父版本 (per `test_harness_rsi_pilot.cpp:696` mock baseline)
  - **Case 3 (gate denied 回滚)**: **确定性非法输入构造** (缺必需字段的 proposal 直接喂给 Gate, 非依赖 LLM 输出) → gate denied → `IGenomeRegistry` 零状态变更 (per 2026-10-15 dual-agent 共识, 真实 LLM 输出不可控)
  - **Case 4 (descope)**: 原提案 "drop_ratio ∈ 合理范围" 物理不可行 → **descope 或改 ≥3 contexts** (per 2026-10-06 `l2-evolution-real-llm-provider` C4 deferred ack, 1-2 contexts 粒度无法稳定构造 5% 阈值)

**辅助 fixture**:
- Recording Provider 守卫 (per AGENTS.md §5, 真实 LLM 行为录制 + Mock 回放, CI `HYDRAFORGE_SKIP_REAL_LLM=1` 下零防护)
- hermetic HOME fixture (per 2026-09-21 C1 genome.key 教训, 隔离 `~/.hydraforge/genome.key` 宿主状态)
- InMemoryBus `wait_for_drain()` (per AGENTS.md Pattern #9, 避免事件派发 race)

**依赖 (前置 ship)**:
- `tests/test_helpers/real_llm_env.h` `must_require_real_llm_env()` helper (2026-09-29 ship)
- `tests/test_harness_rsi_pilot.cpp` baseline 复用 (22/22 mock 不变)
- `tests/test_genome_registry.cpp` baseline 复用 (13/13 mock 不变)
- `openspec/changes/archive/genome-wiring-harness-rsi-gepa-2026-09-22/` (闭环第 7 环, 全 ship)

### 范围外 (Out of Scope)

- 源代码改动 (Harness / IGenomeRegistry / MutationGate 实现层全 ship, 0 改动)
- H5 反作弊 R9 (独立 `rsi-anti-cheat-real-llm` change 范围)
- Wave 3 Phase 2 D4 LoRA 训练管线 (待 D4 立项)
- 工具内部 secrets 处理 (per 工具 own 协议)
- Performance 优化 (本 change 仅补测试覆盖, 不优化路径)

## Acceptance

- [ ] AC-1: `tests/test_harness_mutation_commit_real_llm.cpp` 新建并入 CTest (per `tests/CMakeLists.txt::add_catch_test`)
- [ ] AC-2: `LABELS "must_realllm"` (per 2026-09-29 CTest label 机制, CI 强制验证)
- [ ] AC-3: 4 cases 全部 PASS at real LLM (with `DEEPSEEK_API_KEY` set)
- [ ] AC-4: 4 cases 全部 `SUCCEED` (or `WARN` + reason) at `HYDRAFORGE_SKIP_REAL_LLM=1` (per AGENTS.md §3, 短路不 FAIL)
- [ ] AC-5: Recording Provider 配对实现 (per AGENTS.md §5 守卫)
- [ ] AC-6: hermetic HOME fixture 实现 (per 2026-09-21 C1 教训)
- [ ] AC-7: Case 3 deterministic 构造 (非法 input 喂 gate, 非 LLM 输出依赖) 通过 ≥3 iter 100% consistency
- [ ] AC-8: 既有 `tests/test_harness_rsi_pilot.cpp` 22 cases 零回归
- [ ] AC-9: 既有 `tests/test_genome_registry.cpp` 13 cases 零回归
- [ ] AC-10: 既有 `tests/test_harness_mutation_proposal_real_llm.cpp` 1 case 零回归
- [ ] AC-11: `ctest --test-dir build -L must_realllm` 0 失败 (本地有 key)
- [ ] AC-12: TSan / ASan 0 新增 error (尤其 UAF / race 路径)
- [ ] AC-13: ctest `--rerun-failed` 验证 flaky < 5% (per 2026-09-12 经验)
- [ ] AC-14: AGENTS.md §Recent Changes 段登记本 ship (含 R3 元指标实证 claim)

## Capabilities

### New Capabilities

- `harness-real-llm-mutation-commit`: Harness mutation commit 真实 LLM 端到端验证 (R3 元指标实证)
- `genome-registry-relistic-state-consistency`: IGenomeRegistry 状态一致性真实 LLM 验证 (state_consistency_assertion, 非固定路径)
- `gate-deterministic-deny-construct`: MutationGate denied 路径确定性测试构造 (无 LLM 输出依赖)

### Modified Capabilities

- `agile-self-evolution-arch::real-llm-validation-coverage`: 真实 LLM 验证覆盖率 (从 H1-only → H1+H2+H3+H4 完整)
- `harness-architecture::R3-meta-evidence`: 自进化 v1.5 §R3 元指标实证 (从 partial mock → 端到端 real LLM)
- `drop-ratio-redline`: R8.1 红线 5% 触发验证 (per 2026-10-06 C4 deferred ack, 1-2 contexts 物理不可行, descope)

## Impact

**Production code (0 files)**: 无 src/ 改动, 纯 test 覆盖补全

**Test (1 file)**:
- `tests/test_harness_mutation_commit_real_llm.cpp` (新增, ~300-500 lines, 4 cases + Recording Provider fixture + hermetic HOME fixture)

**Test fixture helpers (复用既有, 0 新增)**:
- `tests/test_helpers/real_llm_env.h` (`must_require_real_llm_env` 已 ship)
- `src/core/genome/registry_filesystem.cpp` (`IGenomeRegistry::fork/load/walk_ancestors` 已 ship, 复用)
- `src/modules/cognitive/harness_rsi.cpp` (`apply_harness_mutation` 已 ship, 复用)

**CMake (1 file)**:
- `tests/CMakeLists.txt` 新增 `add_catch_test(test_harness_mutation_commit_real_llm ...)` + `LABELS "must_realllm"` (per 2026-09-29 ship 机制)

**Spec / docs (2 files)**:
- `openspec/specs/.../spec.md` 新增 R3 元指标实证验收场景 (per L2 spec §R3 引用)
- `AGENTS.md §Recent Changes` 本 ship 登记 (含 R3 元指标实证 claim + drop_ratio 物理不可行 ack)

**Non-goals**:
- 不动 0 源代码 (Harness / IGenomeRegistry / MutationGate 实现层)
- 不实现反作弊 R9 真实 LLM (独立 G5 change 范围)
- 不触发 Wave 3 Phase 2 D4 LoRA 启动 (待 D4 立项)
- 不新增 IToolRegistry streaming API (per B1 真流式 V2 独立范围)

## Cooling-Off

**24h cooling-off** (per AGENTS.md Single-Dev Mode default): rdd-builder P0 创建 OpenSpec change artifacts 后 24h, user 确认 ship.

**Override 机制**: 若用户判断紧急 (本次 R3 元指标需求), 可走 HARD pause override (per Wave 3 G2 precedent), 风险由用户承担, 审计记录在 builder-handoff-v1.5.json.

## Status

📋 **rdd-builder P0 case 4 revise → 2026-10-15 5-segment 格式重写完成**. 待 rdd-builder P0 重决策 case 1 (approve) → P1 plan gen → P1.5 deps → P2 execute (in worktree per ADR-0048) → P2.5 Oracle SHIP-with-fixes review → P3 archive + verifier.

## Status: 🔴 OPEN (2026-10-15 audit verification, P2 实施待 24h cooling-off 满)

**Status 段补全** (per Pattern #10 hygiene):

**5-维判定**:
- 维度 1 self-declared: header 有 Status 段 "📋 rdd-builder P0 case 4 revise ...", 但格式非标准
- 维度 2 archive: ✅ `openspec/changes/2026-10-15-harness-mutation-commit-real-llm/` (5-file integrity, per commit `dbd01d6` 关联)
- 维度 3 commit: ✅ commit `dbd01d6` (R13.4 bug fixes OpenSpec 关联) + 4 commits (fc5749f + 04c653a + 7af59b7 + 0477ead) per 早 session
- 维度 4 ADR: 关联 ADR-0068 (event emission) + harness-arch v1.0 §十一 (genome 持久化)
- 维度 5 fallback: P2 实施 7af59b7 + SHIP-with-fixes 0477ead (per Oracle M1 M3 M4 + 6 Minor) + 4/4 cases PASS 35 assertions

**判定依据**:
- 本提案对应 OpenSpec change `2026-10-15-harness-mutation-commit-real-llm` (per 早 session 实施)
- P2 实施 ship (commit 7af59b7) + Oracle SHIP-with-fixes (commit 0477ead, 4 Major + 6 Minor 全应用)
- 4/4 cases PASS (35 assertions, 0.01s deterministic, per Oracle verdict ses_ec25297b2)
- 待 P3 archive + verifier retry loop (max 3 per ADR-0034)
- 必须_realllm label 已 M1 路径 A 移除 (Recording Provider 0 实例化问题, per 早 session Pattern #13 沉淀)

**判定结论**: 🔴 OPEN (P2 ship 已完, P3 archive 待 user 决策, P4 verifier retry loop 待)

**Pattern 沉淀**: 早 session 2026-10-15 完整 rdd-arch → rdd-planner → rdd-builder P0-P2-P2.5 ship 闭环, P3 archive 待下会话完成.
