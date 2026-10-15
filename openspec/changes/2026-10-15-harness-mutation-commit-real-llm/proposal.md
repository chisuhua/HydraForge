## Why

### 真实闭环缺口 (per AGENTS.md §Reverse Indicator Rule R3 元指标实证)

Genome wiring 闭环第 7 环"版本提交/发布"端到端 ship (`openspec/changes/archive/genome-wiring-harness-rsi-gepa-2026-09-22/`, 2026-09-22), 但真实 LLM 验证仅覆盖"突变提案"环节 (smoke test `tests/test_harness_mutation_proposal_real_llm.cpp` 1/1 real), 未覆盖完整路径:

```
LLM 生成 prompt_delta
  → MutationGate Gate 0-3 评分
    → IGenomeRegistry fork() persist
      → apply_harness_mutation 内存应用
        → genome.committed 事件 + walk_ancestors 关联
```

### Harness 子系统缺口 (per `docs/architecture/harness-architecture-2026-09.md` §十二 H1-H6)

| 项 | 状态 | 备注 |
|----|------|------|
| H1 mutation 提案生成 | ✅ real LLM smoke | `test_harness_mutation_proposal_real_llm` 1/1 PASS |
| H2 mutation 审批 (MutationGate) | ⚠️ mock only | 22 cases mock in `test_harness_rsi_pilot` |
| H3 mutation commit + IGenomeRegistry 持久化 | ⚠️ mock only | 13 cases mock in `test_genome_registry` |
| H4 evolution.readiness.denied | ⚠️ mock only | — |
| H5 反作弊 R9 | ⚠️ mock only | 独立 `rsi-anti-cheat-real-llm` change 范围 |
| H6 4-tier gate 反向校验 + drop_ratio | ⚠️ mock only | — |

按 AGENTS.md §Reverse Indicator Rule R3 元指标实证要求, 当前"提案生成"环节真实 LLM 验证, **真实 LLM 端到端 commit → persist → reload → re-eval 链路未验证**. 自进化 v1.5 9 段闭环 R3 声称不完整.

### 触发条件 (任一满足)

- ✅ **用户要求** (本次会话提问即触发条件 1, 2026-10-14)
- ✅ 自进化 v1.5 9 段闭环 §R3 元指标需要实证
- ⏸ Wave 3 Phase 2 D4 LoRA 训练管线启动需要 harness 真实 LLM 闭环数据 (待触发)

### dual-agent 调研强交叉命中 (2026-10-14, per `.rddf/audits/2026-10-14-p0-proposals-dual-agent-review.md`)

**Oracle + Metis 独立识别**:
- A: 实现路径清晰, 前置依赖全 ship (Genome wiring + must_realllm helper)
- B: "Gate 0-3 真实 LLM 评分" 是误称 — gate 是确定性规则 (ToolMetadata 白名单 / 依赖图 / Schema 验证), LLM 只参与提案生成, Case 1/3 描述需 redesign
- C: Case 4 drop_ratio 在 L2 1-2 contexts 粒度物理不可行 (per 2026-10-06 实证, 0/50/100% 二值, 5% 阈值无法稳定构造)
- D: 现有 Case 1/3 依赖固定路径断言, flaky 风险, 需 redesign 为状态一致性断言

**独占发现** (与本 change 正交, 已立为独立 improvement):
- E (Metis): env_vars secrets redaction 缺口 → `adr-0072-d4-env-vars-redaction.md` (Phase B 推进)
- F (Metis): kStreamChunkSize=64 UTF-8 切片 bug → `fix-stream-handle-utf8-chunk-boundary.md` (Phase B 推进)
- G (Oracle): sink pointer RAII 生命周期 → `fix-stream-handle-sink-lifecycle.md` (Phase B 推进)
- H (Oracle): hermetic HOME 强制 (per 2026-09-21 C1 genome.key fresh-deploy 教训) → **本 change 实施前置**

## What Changes

### 范围 (In Scope)

**新增 1 个 test binary** (test only, **0 源代码改动** — Harness / IGenomeRegistry / MutationGate 实现层全 ship):

- `tests/test_harness_mutation_commit_real_llm.cpp` (must_realllm label per 2026-09-29 ship 机制)
- **4 cases 全部 redesign** (per dual-agent 共识):

#### Case 1 (commit 成功路径, state consistency assertion)
- 真实 LLM 生成 mutation proposal → Gate 0-3 确定性规则通过 → `IGenomeRegistry::fork()` persist → `apply_harness_mutation` 内存应用 → `genome.committed` 事件触发
- **断言 (per AGENTS.md Pattern #3 分层)**: **state consistency** 而非固定路径
  - `IGenomeRegistry::load(commit_result.genome_version)` 返回的 Genome 与 commit 前内存一致 (version chain + spec.harness 逐字段)
  - `walk_ancestors` 链含父版本 + 新版本
  - `genome.committed` 事件 payload 含 `genome_version` 字段
- **不**断言 Gate 评分具体值, **不**断言 commit 后某字段等于某值 (LLM 输出不可控)

#### Case 2 (commit 事件流 + 异步派发)
- 真实 LLM mutation commit → `genome.committed` 事件经 InMemoryBus 派发 → `walk_ancestors` 关联
- **断言**:
  - `InMemoryBus::wait_for_drain()` 后, 事件队列含 `genome.committed` (per AGENTS.md Pattern #9 contract drain API)
  - 事件 payload 含 `genome_version` + `parent_version` + `mutation_diff`
  - `walk_ancestors(commit_result.genome_version)` 链 ≥2 (新版本 + 父版本)

#### Case 3 (gate denied 回滚, **确定性构造**)
- **关键 redesign (per dual-agent 共识)**: **不**依赖真实 LLM 生成非法 proposal (LLM 输出不可控, flaky 风险高)
- **用确定性非法输入构造** (per Oracle C2 + Metis AI 失败模式 2):
  - 构造缺必需字段的 proposal (e.g. 缺 `tools_add` / 缺 `prompt_delta`) 直接喂给 Gate 0-3
  - OR: 构造 `tools_add` 包含 ToolMetadata V3 白名单未注册的工具名 → Gate 0 即 deny
- **断言** (state consistency + zero state change):
  - Gate 返回 denied (确定性, 非 LLM 依赖)
  - `IGenomeRegistry` 零状态变更 (未调 `fork()`)
  - `genome.persist_failed` 事件触发 (per ADR-0068 Appendix A v2.3)
  - **不**触发 `genome.committed` 事件

#### Case 4 (drop_ratio descope, per dual-agent 共识)
- **关键 redesign**: 原提案 "drop_ratio ∈ 合理范围" 物理不可行 (per 2026-10-06 `l2-evolution-real-llm-provider` C4 deferred ack)
- **decision**: **descope** (不实现)
- 改进: 在 `specs/harness-mutation-commit-real-llm/spec.md` R4 Requirement 明确 descope rationale + 引用 R8.1 红线 5% 触发需 ≥20 mutations 物理不可行

### 辅助 fixture (per dual-agent 调研)

- **Recording Provider 守卫** (per AGENTS.md §5 守卫 CI 路径零防护):
  - `RecordingLLMProvider` 实现 `ILLMProvider` 三虚 (`generate` / `generate_stream` / `available_models`)
  - 录制真实 LLM req/payload, 返回固定 JSON (CI `HYDRAFORGE_SKIP_REAL_LLM=1` 下零防护 → 改用 Recording Provider 回放)
  - 注: 严格按 AGENTS.md §5 "Production code 传给 provider 的参数契约 (如 `react_once` 是否传空 `params.model`)" 模式, **不**改变 must_realllm 强制语义
- **hermetic HOME fixture** (per 2026-09-21 C1 genome.key fresh-deploy 教训):
  - 测试 setup/teardown 隔离 `~/.hydraforge/genome.key` 宿主状态
  - `setenv("HOME", "/tmp/hermetic_<uuid>", 1)` + `unsetenv("HYDRAFORGE_HOME")` 模式
  - 回归守卫: 全新部署 (无既存 host 状态) 路径不 IOError
- **InMemoryBus `wait_for_drain()`** (per AGENTS.md Pattern #9):
  - 事件派发 race 防御 (commit 返回后立即查事件可能 race)

### 范围外 (Out of Scope)

- 源代码改动 (Harness / IGenomeRegistry / MutationGate / Genome Registry 实现层全 ship, 0 改动)
- H5 反作弊 R9 真实 LLM (独立 `rsi-anti-cheat-real-llm` change 范围)
- Wave 3 Phase 2 D4 LoRA 训练管线 (待 D4 立项)
- 工具内部 secrets 处理 (per 工具 own 协议)
- Performance 优化 (本 change 仅补测试覆盖, 不优化路径)

### 依赖 (前置 ship, 全部已 ship)

- `tests/test_helpers/real_llm_env.h` `must_require_real_llm_env()` helper (2026-09-29 ship, AGENTS.md §REAL LLM TESTING)
- `tests/test_harness_rsi_pilot.cpp` baseline (22/22 mock 不变, per 2026-09-22 ship)
- `tests/test_genome_registry.cpp` baseline (13/13 mock 不变)
- `tests/test_harness_mutation_proposal_real_llm.cpp` baseline (1/1 real, 仅 smoke, 保留)
- `openspec/changes/archive/genome-wiring-harness-rsi-gepa-2026-09-22/` (闭环第 7 环)
- ADR-0068 Appendix A v2.3 (`genome.committed` + `genome.persist_failed` 主题注册, 2026-09-22)
- ADR-0083 (MutationGate 契约) + ADR-0086 (credit-assignment) + ADR-0088 (H-D-M transition) — 全 ship

### 6 pre_issues 必修正 (per `.rddf/state/.arch-handoff.json::trigger_pre_issues`)

1. **Case 1/3 断言 redesign** — 状态一致性而非固定路径 (per AGENTS.md Pattern #3) ✅ 已完成 (见上文 Case 1 断言)
2. **Case 3 gate denied 确定性构造** — 用确定性非法输入 (非 LLM 输出依赖) ✅ 已完成 (见上文 Case 3 redesign)
3. **Case 4 drop_ratio descope** ✅ 已完成 (见上文 Case 4 descope)
4. **Recording Provider 守卫** ✅ 已纳入 fixture 设计
5. **hermetic HOME fixture** ✅ 已纳入 fixture 设计
6. **must_realllm label + `must_require_real_llm_env()` helper** ✅ 复用既有 (2026-09-29 ship)

## Acceptance

- [ ] AC-1: `tests/test_harness_mutation_commit_real_llm.cpp` 新建并入 CTest (`tests/CMakeLists.txt::add_catch_test`)
- [ ] AC-2: `LABELS "must_realllm"` (per 2026-09-29 CTest label 机制, CI 强制验证, skip=1 触发 FAIL)
- [ ] AC-3: 4 cases 全部 PASS at real LLM (with `DEEPSEEK_API_KEY` set), DEEPSEEK_API_KEY 实测 2026-10-14 已设 (`sk-...`)
- [ ] AC-4: 4 cases 全部 `SUCCEED` (or `WARN` + reason) at `HYDRAFORGE_SKIP_REAL_LLM=1` (per AGENTS.md §3 短路不 FAIL)
- [ ] AC-5: Recording Provider 配对实现 (per AGENTS.md §5 守卫)
- [ ] AC-6: hermetic HOME fixture 实现 (per 2026-09-21 C1 教训)
- [ ] AC-7: Case 3 deterministic 构造 (非法 input 喂 gate) 通过 ≥3 iter 100% consistency (零 flaky)
- [ ] AC-8: 既有 `tests/test_harness_rsi_pilot.cpp` 22 cases 零回归
- [ ] AC-9: 既有 `tests/test_genome_registry.cpp` 13 cases 零回归
- [ ] AC-10: 既有 `tests/test_harness_mutation_proposal_real_llm.cpp` 1 case 零回归
- [ ] AC-11: `ctest --test-dir build -L must_realllm` 0 失败 (本地有 key)
- [ ] AC-12: TSan / ASan 0 新增 error (尤其 UAF / race 路径, hermetic HOME 触发 fresh-deploy)
- [ ] AC-13: ctest `--rerun-failed` 验证 flaky < 5% (per 2026-09-12 经验)
- [ ] AC-14: AGENTS.md §Recent Changes 段登记本 ship (含 R3 元指标实证 claim + Case 4 descope rationale)

## Cooling-Off

**24h cooling-off** (per AGENTS.md Single-Dev Mode default): rdd-builder P0 创建 OpenSpec change artifacts 后 24h, user 确认 ship.

**Override 机制**: 若用户判断紧急 (本次 R3 元指标需求), 可走 HARD pause override (per Wave 3 G2 precedent), 风险由用户承担, 审计记录在 builder-handoff-v1.5.json.

## Non-Goals (决策记录)

- **不**改源代码 (Harness / IGenomeRegistry / MutationGate 实现层全 ship)
- **不**实现 H5 反作弊 R9 真实 LLM (独立 G5 范围)
- **不**触发 Wave 3 Phase 2 D4 LoRA 启动 (待 D4 立项)
- **不**新增 IToolRegistry streaming API (per B1 真流式 V2 独立范围)
- **不**实现 Case 4 drop_ratio (per dual-agent 共识物理不可行 + R8.1 红线需 ≥20 mutations)
