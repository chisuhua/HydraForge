# l2-evolution-real-execution-chain — Proposal

> **Change Slug**: `l2-evolution-real-execution-chain`
> **Status**: 🔍 Proposed → ✅ Archived 2026-09-28
> **Created**: 2026-09-26
> **Oracle Audit Session**: `ses_f259746caffe9yK0YoIQ6FOgWp` Path 1 Change 2
> **Supersedes**: `archive/2026-09-24-pdk-chat-demo-evolution-reference-example/`
> **Superseded by**: `archive/2026-09-26-l2-evolution-finalization/` (T2 + T3 + T5 ship + T6 archive; merge commit `f26e157` on main). Phase B (this change) is fully subsumed by Phase C; readers MUST consult finalization for current state. Per Oracle T6 plan M2 supersession pointer requirement (Day-5 4-file integrity).

---

## Why

### 当前问题

L2 reference binary (`pdk_chat_demo_evolution`) 的 6 阶段链 (phase1-5) **是 facade — trace 全部硬编码**, 经 Oracle session `ses_f259746caffe9yK0YoIQ6FOgWp` 审计确认:

1. **`phase1_init()` 空函数** (L64-67 注释自承 "Batch 4 (Task 10)" 待办, 但 Batch 4 commit 只写了 main.cpp 从未回填)
2. **`phase3_mutation` 不调用 `apply_harness_mutation`** — MutationGateContext/free function 零调用
3. **`phase4_reload_rerun` 不调用 `IGenomeRegistry::load`** — V2 缺口闭环未实测
4. **`phase5_compare` 不调用 `IEvaluator`** — BehavioralEquivalence 零实例化
5. **全部 trace 字段硬编码** (`gate_passes=5`, `attribution_verdict="Attributed"`, `genome_version=1`)
6. **`--release-metrics` 是 stub** — 永远 `drop_ratio=0%`, 从不写 `metrics.json`, R8.1 红线机制不存在
7. **R13.4 sensitivity redaction 未实现** — 全库 grep `redact`/`REDACTED` = 0 命中
8. **spec cross-doc-consistency 需求违反** — 3 份 SoT §十一 L2 行缺失

这让 R8/R3 验证毫无意义, 自进化闭环第 7 环"版本提交/发布"断裂。

### 现状证据

- Oracle 审计 `ses_f259746caffe9yK0YoIQ6FOgWp` 详细发现 (Critical 3/Major 3)
- 实测 `--mock --trace-events` 确认 `turn_input=""`, `response=null`, `tokens=0` 全 phase
- `git show cc8739d` 验证 Batch 4 commit 只写了 main.cpp 未填 evolution_session.cpp

---

## What (实施内容)

### Change 2 — Real execution chain for phases 1-5

1. **phase1_init**: 构造 `DSLEngine` + `InMemoryBus` + 加载 LoopAgent plugin (mock-fallback path) + `LLMProviderFactory::create("mock")` + 将 provider 注入 DSLEngine, 而非空函数
2. **phase2_baseline**: 构造 `ChatSession` (11参完整签名), 调用 `session->chat(turn_input)` 捕获真实 response/tokens/cost, 而非硬编码
3. **phase3_mutation**: 调用 `apply_harness_mutation` 5-参自由函数, 使用 hermetic HOME `FilesystemGenomeRegistry`, 而非硬编码
4. **phase4_reload_rerun**: `registry->load(genome@N)` → `genome_to_agent_config()` → ChatSession 重建 → rerun → 捕获差异
5. **phase5_compare**: 实例化 `IEvaluator::compare(before, after)` → 输出 `attribution_verdict`
6. **`--release-metrics`**: 从 phase5 compare 输出计算真实 `drop_ratio` + 写 `metrics.json` + `>5% exit non-zero`
7. **R13.4 redaction**: `detail::redact_trace_fields(trace_event, sensitivity)` 替换 turn_input/response/tags/domain 为 `[REDACTED-<level>]`
8. **3 SoT §十一 L2 ✅ ship rows**: 在 harness/rsi/self-evolution 三份 SoT 文档添加 ship 行

### 不修改的内容

- `examples/pdk_chat_demo/main.cpp` — N1 硬阻断
- `include/` 下任何头文件 — N2 硬阻断
- `pdk/chat_session/`, `pdk/loop_agent/`, `pdk/llama_engine/` — PDK API 稳定

---

## Capabilities (新能力)

| Capability ID | 描述 | 验证方式 |
|--------------|------|----------|
| **EVOL-REAL-1** | phase1_init 真实构造 DSLEngine + provider | `--mock` 下返回 mock LLM provider 而非崩溃 |
| **EVOL-REAL-2** | phase2_baseline 真实 chat() 调用 | trace JSONL `response` 字段非空, `tokens > 0` |
| **EVOL-REAL-3** | phase3_mutation 真实 apply_harness_mutation | binary exit 0 + mutation 段 `gate_passes` 非硬编码 |
| **EVOL-REAL-4** | phase4_reload_rerun 真实 registry load + rebuild | reload 后 ChatSession 用新 AgentConfig |
| **EVOL-REAL-5** | phase5_compare 真实 IEvaluator V2 | compare 段 `attribution_verdict` ∈ {Attributed,Confounded,Insufficient,NotAttempted} |
| **EVOL-REAL-6** | --release-metrics 输出 metrics.json + drop_ratio | >5% exit non-zero, metrics.json 含 real/orginal/baseline_failures/mutated_passes |
| **EVOL-REAL-7** | R13.4 redaction 生效 | `sensitivity=confidential` → trace 含 `[REDACTED-confidential]` |
| **EVOL-REAL-8** | 3 SoT §十一 ship 行 | grep 三份 SoT 文档确认 "L2 ✅ ship" |

---

## Impact (影响)

| 模块 | 影响 | 类型 |
|------|------|------|
| `examples/pdk_chat_demo_evolution/evolution_session.cpp` | 重写 phase1-5 全部 5 个方法 | rewrite |
| `examples/pdk_chat_demo_evolution/evolution_session.h` | 新增成员 (DSLEngine/ChatSession/GenomeRegistry/IEvaluator) | minor |
| `examples/pdk_chat_demo_evolution/main.cpp` | `--release-metrics` 实现, `--real-llm` 真实 wiring | minor |
| `docs/architecture/harness-architecture-2026-09.md` | §十一 新增 L2 ✅ ship 行 | doc |
| `docs/architecture/rsi-architecture-2026-09.md` | §十一 新增 L2 ✅ ship 行 | doc |
| `docs/architecture/self-evolution-architecture-2026-08.md` | §十一 校订/新增 L2 ✅ ship 行 | doc |
| `openspec/specs/pdk-chat-demo-evolution/spec.md` | 更新 spec 反映 real execution chain | spec |

### 验收标准

| 项 | 标准 |
|----|------|
| **Acceptance** | `./pdk_chat_demo_evolution --mock --context-file ... --trace-events` exit 0 + trace JSONL `response` 非空 + `tokens > 0` |
| **Backwards compatible** | `examples/pdk_chat_demo/` 零改动; 全量 ctest `-LE l2-evolution` 零回归 |
| **Docs** | 3 SoT §十一 L2 ✅ ship rows; harness-arch §3.1 API 表面校正 |

---

## 关联文档

- **Oracle Audit**: `ses_f259746caffe9yK0YoIQ6FOgWp`
- **SoT (Source of Truth)**:
  - `docs/architecture/self-evolution-architecture-2026-08.md` v1.5 §十一
  - `docs/architecture/harness-architecture-2026-09.md` v1.0 §十一
  - `docs/architecture/rsi-architecture-2026-09.md` v1.0 §十一
- **Spec**: `openspec/specs/pdk-chat-demo-evolution/spec.md`
- **Predecessor**: `openspec/changes/archive/2026-09-24-pdk-chat-demo-evolution-reference-example/`

---

## Out of Scope

- Wave 3 Phase 2 (D4-D7 LoRA + 完整 eval + AgenticMind + serving)
- S4 Agent-Agent co-evolution
- --regression-test-suite flag (per Oracle "or spec 降级")
- real-LLM 模式 (已有真实 wiring 基础设施, 但 deepseek API key 不在 sandbox)

---

## 时序与依赖

- **依赖**: Target B Change 1 (commit `f456336`, CLI contract fixes)
- **估时**: 2-4 天 (Oracle 对应 Path 1 Change 2 估时)
- **关键路径**: Phase A (OpenSpec) → Phase B (phase1-2 MVP) → Phase C (phase3-5 + metrics) → Phase D (tests + docs)

---