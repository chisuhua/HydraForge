# pdk-chat-demo-evolution-reference-example — Proposal

> **Change Slug**: `pdk-chat-demo-evolution-reference-example`
> **Status**: 🔍 Proposed (PLACEHOLDER — 待 rdd-builder 实施阶段填充完整 4 件套)
> **Created**: 2026-09-23
> **关联 SoT**: [`docs/architecture/self-evolution-architecture-2026-08.md` v1.5 ✅](../../architecture/self-evolution-architecture-2026-08.md) + [`docs/architecture/harness-architecture-2026-09.md` v1.0 ✅](../../architecture/harness-architecture-2026-09.md) + [`docs/architecture/rsi-architecture-2026-09.md` v1.0 ✅](../../architecture/rsi-architecture-2026-09.md)

---

## Why (动机)

### 当前问题

1. **三方 SoT 架构抽象, 落地路径不直观**: 3 份 Source of Truth 文档 (`self-evolution` / `harness` / `rsi`) 已 ship, 但每段架构概念在 pdk_chat_demo 落地存在两种状态 — (a) **已 ship** (如 ChatConfig.override_*, SessionManager JSONL, model_command.cpp) (b) **未来待 L2/Phase 2 启用** (如 `apply_harness_mutation` V2 wire, `load(genome@N) → 重建 ChatSession → 1 turn` 等). L1 traceback 已加 §十一 (本文档化为锚点), 但**缺乏可运行 reference example**.

2. **V2 端到端 "load(genome@N) → 重建 ChatSession → 1 turn" 是 Wave 4 必须子**: C4 Decision Record `docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md` §3 第 6 项明确要求 V2 立项必备 harness mutation → chat session 装载回路. 当前 mock 已测过, 真实缺.

3. **Data-RSI / Model-RSI 端到端 reference 缺失**: Trajectory IR + IDistillationWriter + Wave 3 Phase 1 D7 stub provider 都已 ship, 但**没有真实 wired-to-ChatSession 的端到端 example**. Per `docs/architecture/pdk-chat-demo-distill-source-survey-2026-08.md` 已 ship 推荐 SessionWriter JSONL 作为过渡数据源, 但缺 demo.

4. **新人 onboarding**: 现有 `examples/pdk_chat_demo` 是项目唯一 reference implementation, 但只有"chat interactive"路径. 缺乏"以 chat session 为 baseline 跑通 evolution demo" 的入口.

### 现状证据

- **`docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §一.4**: 4 个 Bug ✅ FIXED + Bridge fix (commit `0b0da50`)
- **`docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md` §3**: 4 项摩擦 (eval_quality:Unknown / add-remove 不对称 / bus nullptr / spec 漂移) + post-hoc closure gate
- **`docs/architecture/pdk-chat-demo-distill-source-survey-2026-08.md`**: Session JSONL 路径 + 推荐方案
- **`docs/architecture/harness-architecture-2026-09.md` §十一.7**: L2 立项必要性 (本文 L2 正式立项)
- **`docs/architecture/rsi-architecture-2026-09.md` §十一.6**: pdk_chat_demo 完整 RSI runtime target (本文 L2 立项)

---

## What (实施内容)

### 1. 新建 `examples/pdk_chat_demo_evolution/`

独立 sub-example project (不修改 `examples/pdk_chat_demo/` 主体), 通过**实例化** + **外部 trace** 实现 reuse:

```text
examples/pdk_chat_demo_evolution/
├── CMakeLists.txt                  # 独立 target, AGENTICDSL_BUILD_EXAMPLES ON 启用
├── README.md                       # 端到端使用说明
├── main.cpp                        # 入口: 默认 capture-mode=Training, --trace-events flag
├── evolution_session.{h,cpp}      # 包装 ChatSession + 事件 trace + Genome 操作
├── evolution_tracer.{h,cpp}       # 接收 BusEvent, 序列化为 JSONL stdout (per distill-source-survey)
├── run_evolution_demo.sh           # 一键 mock/real LLM 双模式端到端
├── fixtures/
│   └── golden_inputs.jsonl         # 7 个"reference turns" (供 behavior 等价性 + 评估)
├── tests/
│   ├── test_evolution_session_mutation.cpp     # Case 1: mutation → ChatSession 装载 → 1 turn 后真实 LLM 响应变化
│   ├── test_evolution_session_load.cpp          # Case 2: load(genome@N) → 重建 ChatSession → 1 turn (V2 缺口闭环)
│   └── test_distillation_capture_mode.cpp        # Case 3: capture-mode=Training → JSONL → IDistillationWriter
└── data/
    └── REFERENCE_GENOMES/          # ship 时 3 个示例 genome (default / safe / aggressive)
```

### 2. 6 段端到端演示链

参考 [`./specs/pdk-chat-demo-evolution/spec.md`](./specs/pdk-chat-demo-evolution/spec.md) R1:

```
1. 启动 (main.cpp)
   └─ capture-mode=Training + --trace-events flag
2. Session 初始化 (evolution_session.cpp)
   └─ ChatSession ctor + 注册 6 Agent + trace 订阅
3. Run baseline (golden input 1)
   └─ tracer.record(baseline_response)
4. Apply mutation (Genome::fork(name, parent, final_spec) + IGenomeRegistry::commit)
   └─ 5-tier gate (G0 语法 → G1 policy + 3 信号 → G2 load + freshness → G2.5 partial-apply → G3 persist-before-apply)
5. Reload + Run same golden input (golden input 1)
   └─ ChatSession ctor (V2 缺口闭环 via `load(genome@N)` 端点)
6. Compare traces
   └─ eval_quality + BehavioralRegressionGate → Approve/Deny
```

### 3. 不修改主 `examples/pdk_chat_demo/`

L2 仅 `examples/pdk_chat_demo_evolution/` 新增, pdk_chat_demo 主体零改动. 理由:
- 主 demo 是"chat interactive" reference, evolution demo 是"chat + mutation" reference, 两者分离
- 减小 blast radius
- evolution 端到端路径不要影响主 demo 的 release cadence

---

## Capabilities (新能力)

本 change 实施后, HydraForge 项目**新增**以下能力:

| Capability ID | 描述 | 验证方式 |
|--------------|------|----------|
| **EVOL-DEMO-1** | `examples/pdk_chat_demo_evolution/main.cpp --mock --trace-events` 在 5 秒内跑通 6 段端到端 | `./run_evolution_demo.sh --mock` exit 0 |
| **EVOL-DEMO-2** | `examples/pdk_chat_demo_evolution/main.cpp --real-llm DEEPSEEK_API_KEY=... --trace-events` 真实 LLM 跑通 baseline + mutation + reload + compare | per Golden input 1 的 response 差异 ≤ eval_quality Acceptance 阈值 |
| **EVOL-DEMO-3** | `tests/test_evolution_session_mutation.cpp` RED-GREEN 闭环 (Case 1) | ctest PASS, 5 case / ≥ 8 assertion |
| **EVOL-DEMO-4** | `tests/test_evolution_session_load.cpp` RED-GREEN (Case 2 V2 缺口闭环) | ctest PASS, 5 case / ≥ 8 assertion |
| **EVOL-DEMO-5** | `tests/test_distillation_capture_mode.cpp` (Case 3) | ctest PASS, 4 case / ≥ 6 assertion |
| **EVOL-DEMO-6** | trace JSONL schema 稳定: 4 段事件 (baseline/mutation/reload/compare) + 8 字段 | per `tests/test_evolution_tracer_schema.cpp` |
| **EVOL-DEMO-7** (R8 反向指标门, 2026-09-23 升级) | L2 demo 暴露 `--release-metrics` + `--regression-test-suite` + `--ablation-mode=full` flag, 输出 metrics.json + ablation_report.json, drop_ratio > 5% 自动 block | ctest `test_reverse_indicators` 3 cases PASS |
| **EVOL-DEMO-8** (R9.1 防搜现成答案, 2026-09-23 升级) | L2 demo 含 Poolside 失效模式测试 — input 嵌入 baseline hint, Agent 不应直接复述 hint (eval_quality diff > -10%) | ctest `test_anti_cheat_search_solution` 1 case PASS |
| **EVOL-DEMO-9** (R9.2 + R9.3 防改评判 + 防串谋, 2026-09-23 升级) | L2 demo 含复旦改评判指标 + ExploitGym 串谋外部平台失效模式测试 — Mutation gate 拒绝 evaluator schema write + sandbox 默认 network_mode=none | ctest `test_anti_cheat_metric_tampering` + `test_anti_cheat_sandbox_escape` 各 1 case PASS |
| **EVOL-DEMO-10** (R13 上下文驱动契约, 2026-09-23 升级) | L2 binary 仅接受用户 `--context-file <path.jsonl>`, 零 hardcode; 任一 ContextRequest 字段缺失 = exit non-zero; ≥ 3 类 ContextRequest (code/research/debug) 实证才构成 generalizable 自进化声称 | `--context-file` flag + `examples/contexts/{code,research,debug}-class-context.jsonl` 3 reference file ship; ctest `test_context_request_validation` 5 cases + `test_context_request_e2e` 4 cases PASS; trace JSONL `meta.context_id` 字段进每段事件 |

---

## Impact (影响)

### 受影响模块

| 模块 | 影响 | 类型 |
|------|------|------|
| `examples/CMakeLists.txt` | + 1 sub-directory add | minor |
| `examples/pdk_chat_demo/` | **零改动** | n/a |
| `src/evolution/harness_rsi.cpp` | **零改动** (C4 ship 接口不变) | n/a |
| `src/core/genome/registry_filesystem.cpp` | **零改动** (G4 wiring 接口不变) | n/a |
| `examples/pdk_chat_demo_evolution/` | 新建 (主目录) | new |
| `tests/` | + 6 new test binaries (22+ cases / ≥ 30 assertions) (R4 修复 2026-09-23: 与 §5.4 6 binary + spec S15 对齐) | minor (build) |

### API 兼容性

| API | 影响 |
|-----|------|
| `ChatSession` ctor | 不变 (L2 通过已知 4 参 ctor 实例化) |
| `ChatConfig::override_*` 5 方法 | 不变 (L2 调用, 不修改签名) |
| `IGenomeRegistry::commit/load/fork` | 不变 (L2 调用, 不修改签名) |
| `apply_harness_mutation` 6 字段签名 | 不变 (L2 调用, 不修改签名) |
| `LLMProviderFactory::register_dynamic` | 不变 (L2 复用 Wave 3 Phase 1 stub) |

### 验收标准

| 项 | 标准 |
|----|------|
| **Acceptance** | (a) `./run_evolution_demo.sh --mock --context-file ...` exit 0 + trace JSONL 8 字段 + meta.context_id + 6 test binaries 22+/22+ cases PASS + ctest 零回归 |
| **Backwards compatible** | (a) `examples/pdk_chat_demo/` 主体 binary 行为零变化 + (b) 全量 ctest `-E pdk_chat_demo_evolution` 仍 211 (baseline, 不增测试数)|
| **Docs** | (a) `examples/pdk_chat_demo_evolution/README.md` 完整 (b) `docs/architecture/{self-evolution,harness,rsi}-architecture-*.md` §十一新增行 "L2 已 ship" 段 (post-merge sync)|
| **Cross-doc consistency** | 三方 SoT 文档 ↔ L2 README 路径一致 |

---

## 关联文档

- **SoT (Source of Truth)**:
  - [`docs/architecture/self-evolution-architecture-2026-08.md`](../../architecture/self-evolution-architecture-2026-08.md) v1.5 ✅ §十一
  - [`docs/architecture/harness-architecture-2026-09.md`](../../architecture/harness-architecture-2026-09.md) v1.0 ✅ §十一
  - [`docs/architecture/rsi-architecture-2026-09.md`](../../architecture/rsi-architecture-2026-09.md) v1.0 ✅ §十一
- **Decisions / Audits**:
  - [`docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md`](../../audits/2026-09-21-harness-rsi-pilot-go-no-go.md) §3 第 6 项 (post-hoc closure gate for V2)
  - [`docs/architecture/pdk-chat-demo-distill-source-survey-2026-08.md`](../../architecture/pdk-chat-demo-distill-source-survey-2026-08.md) (Session JSONL 推荐方案)
- **Roadmap**:
  - [`docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md`](../../roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md) (master plan + Sprint 36)

---

## Out of Scope (不在 v1 范围)

- 修改 `examples/pdk_chat_demo/` 主体 (本 change 不动主 demo)
- 新增 ChatSession 公共 API (L2 仅消费现有 API)
- 新增 Genome / Mutation / Provider 公共契约 (同上)
- Wave 3 Phase 2 (D4 LoRA + D5 评估 + D6 AgenticMind + D7 serving 完整化, 独立立项)
- S4 Agent-Agent 协同进化 (research 路径, 独立 spike)

---

## 时序与依赖

- **依赖**:
  - C2 + C3 + C4 + G1 + G2 + G3 + G4 + Wave 3 Phase 1: **全部 ✅ ship** (2026-09-21~22 验证)
  - 24h Wave 3 cooling-off 不阻塞 L2 (L2 不修 ADR-0078, 仅 reference example)
- **不依赖**:
  - Wave 3 Phase 2 (D4-D7) - 独立路径
  - Phase 7a 启动 (Phase 7 Gated, L2 不涉及)
- **关键路径**:
  - V1 (`load(genome@N) → 重建 ChatSession → 1 turn`) 由本 change V1 实例化 (per C4 Decision Record §3 第 6 项)
  - V2 (model serving upgrade) 由 Wave 3 Phase 2 立项 (D7 完整化)

---

## 关联 ADR (无冲突验证)

- ✅ ADR-0078 (Fine-tune) — 与本文 Model-RSI Phase 1 stub 重用, 不冲突
- ✅ ADR-0088 (H→D→M) — 与本文 5-tier gate 一致, L2 启用到 chat session 实例
- ✅ ADR-0086 v1.0+v1.1 (Credit Assignment) — 与本文 evaluation + decision 一致
- ✅ ADR-0084 (MutationGovernance) — L2 实例化现成 6 字段 MutationContext
- ✅ ADR-0083 (IEvaluator) — L2 启用现成 V2 (BehavioralEquivalence + Composite)
- ✅ ADR-0080 (AppendOnlyEventLog) — 与本文 trace → JSONL 路径一致 (per distill-source-survey)

---

## 决策依赖 (待 rdd-planner/rdd-builder 阶段确认)

- [ ] **承诺 L2 独立而非修改主 demo**: 主 demo 不动, evolution demo 独立. (本文 §What.3)
- [ ] **承诺 L2 复用现有 5-tier gate**: 不引新 contract, 仅消费 C4 ship 接口. (本文 §What.2)
- [ ] **承诺 L2 V1 不扩展 Wave 3 Phase 2 scope**: D7 stub 已注册, 不接真实 finetune provider.
- [ ] **承诺 L2 文档锁**: implementation 期同步更新 3 份 SoT 文档 §十一 段 (L2 已 ship 行).

---

**Phase 0 评估**: 本 change 4 件套已起草, 待 rdd-builder P0 决策 (auto-decision complex / handoff / par-agree per ADR-0049). 估时 2-3 天实施 + 0.5 天 review. 启动前需 24h cooling-off (本 change 由 Wave 3 收口 + SoT 升档触发的衍生 change, 非 cold-start).
