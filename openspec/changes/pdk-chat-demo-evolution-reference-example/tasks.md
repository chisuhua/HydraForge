# pdk-chat-demo-evolution-reference-example — Tasks

> **Status**: 🔍 Proposed Tasks (PLACEHOLDER 待 rdd-builder 实施阶段细化)
> **关联 Proposal**: [`./proposal.md`](./proposal.md)
> **关联 Design**: [`./design.md`](./design.md)
> **关联 Spec**: [`./specs/pdk-chat-demo-evolution/spec.md`](./specs/pdk-chat-demo-evolution/spec.md)

---

## Task Groups (TDD 5 步结构)

每组遵循 RED-GREEN-REFACTOR 5 步 (per AGENTS.md 模式):
1. **RED**: 写 failing test (用 mock LLM / capture-mode=None, exit non-zero 验证)
2. **GREEN**: minimal impl (复用现有 5-tier gate + ChatConfig + IGenomeRegistry, 不引新 contract)
3. **REFACTOR**: 公共 helper 抽 + schema 稳定化
4. **AUDIT**: Oracle dual-agent review (Metis 路径完整性 + Oracle 物理可行性)
5. **ARCHIVE**: 完整 4-file integrity per AGENTS.md Day 5 lesson

---

### T1: project skeleton (CMakeLists.txt + README + main.cpp stub)

**估时**: 0.5 天
**依赖**: 无

#### T1.1 [RED] CMakeLists.txt compile fail (no source files yet)
- [ ] 写 `examples/pdk_chat_demo_evolution/CMakeLists.txt` stub (target + test)
- [ ] 写 `examples/CMakeLists.txt` add_subdirectory hook
- [ ] 验证 `cmake -S . -B build -DAGENTICDSL_BUILD_EXAMPLES=ON` exit non-zero (no source yet)

#### T1.2 [GREEN] minimal main.cpp + skeleton files
- [ ] 写 `main.cpp` stub (5 lines: return 0)
- [ ] 写 `evolution_session.{h,cpp}` empty
- [ ] 写 `evolution_tracer.{h,cpp}` empty
- [ ] 验证 `cmake build` exit 0 + main binary 编译

#### T1.3 [REFACTOR] README + run_evolution_demo.sh
- [ ] 写 `README.md` (端到端使用说明 + 验收命令)
- [ ] 写 `run_evolution_demo.sh` (一键 mock/real-LLM 双模式)
- [ ] 写 `fixtures/golden_inputs.jsonl` (7 个 reference turn 输入)

#### T1.4 [AUDIT] Metis: 路径完整性
- [ ] Metis review: example/pdk_chat_demo 主体无变化 + 新独立 binary 边界清晰

#### T1.5 [ARCHIVE] commit T1 (worktree)
- [ ] commit: `feat(example): pdk_chat_demo_evolution project skeleton`

---

### T2: 6-phase demo TDD (核心 happy path)

**估时**: 1.5 天
**依赖**: T1
**目标**: mock mode 跑通 6 段端到端, 5-tier gate 全 pass

#### T2.1 [RED] test_evolution_session_mutation (Case 1)
- [ ] 写 `tests/test_evolution_session_mutation.cpp` 4 cases:
  - Case 1.1: apply_mutation(prompt_delta) → 期望 5-tier gate 全 PASS (G0/G1/G2/G2.5/G3)
  - Case 1.2: apply_mutation(tools_add) → 期望 SemanticLockedTools 空时不挡
  - Case 1.3: apply_mutation(无效 prompt_delta) → 期望 G0 fail (语法)
  - Case 1.4: apply_mutation(denied_tools) → 期望 G1 fail (policy)
- [ ] 验证 `ctest -R test_evolution_session_mutation` exit non-zero (RED)

#### T2.2 [GREEN] EvolutionSession::phase3_mutation impl
- [ ] 写 `evolution_session.cpp::phase3_mutation`
- [ ] 调用现有 `apply_harness_mutation` (C4 ship 接口), 通过 5-tier gate
- [ ] 返回 `AppliedMutation` + Genome
- [ ] 测试 PASS

#### T2.3 [RED] test_evolution_session_load (Case 2 — V2 缺口闭环 ⭐⭐)
- [ ] 写 `tests/test_evolution_session_load.cpp` 5 cases:
  - Case 2.1: `load(genome@N)` → 期望 Genome::to_chat_config() 返回 ChatConfig
  - Case 2.2: ChatSession 重新构造用新 ChatConfig → 期望装载成功
  - Case 2.3: baseline + reload 同 turn 输入 → 期望响应差异 (mutation 生效)
  - Case 2.4: V2 缺口路径 (mock-only) → 期望 5-tier gate result Genome 实例化
  - Case 2.5: broken genome → 期望 load fail (NotFound) 而非 crash
- [ ] 验证 RED

#### T2.4 [GREEN] EvolutionSession::phase4_reload_rerun impl
- [ ] 写 `phase4_reload_rerun` impl
- [ ] 通过 `IGenomeRegistry::load(name, version)` + `Genome::to_chat_config()` + `ChatSession` 重建
- [ ] mock mode 验证 PASS

#### T2.5 [REFACTOR] 5 段事件流 (phase1~phase6) 整合
- [ ] `run_6_phase_demo()` orchestration method
- [ ] phase1~phase5 各方法 refactor

#### T2.6 [AUDIT] Oracle dual-agent (Metis + Oracle)
- [ ] Metis: 5-tier gate 完整路径 + Phase 2 风险
- [ ] Oracle: V2 `load(genome@N)` 物理可行性 + ABI 不破坏

#### T2.7 [ARCHIVE] commit T2
- [ ] commit: `feat(example): pdk_chat_demo_evolution 6-phase demo end-to-end`

---

### T3: trace JSONL + capture-mode 集成 (ADR-0080 D10)

**估时**: 0.5 天
**依赖**: T2 (phase1~phase5 已可用)

#### T3.1 [RED] test_evolution_tracer_schema (Case 3)
- [ ] 写 `tests/test_evolution_tracer_schema.cpp` 4 cases:
  - Case 3.1: trace 4 段事件 → 期望 JSONL 8 字段 (phase/timestamp_iso8601/session_id/turn_input/response/tokens/cost_usd/meta)
  - Case 3.2: meta 含 genome_version (mutation 段) + eval_quality (compare 段)
  - Case 3.3: capture-mode=Training → 期望 IDistillationWriter 触发 (mock fixture)
  - Case 3.4: --trace-events flag 默认 close, 启用后 emit
- [ ] 验证 RED

#### T3.2 [GREEN] EvolutionTracer + capture-mode 集成
- [ ] 写 `evolution_tracer.cpp::subscribe_interaction_bus()`
- [ ] 写 `evolution_tracer.cpp::record_phase(json)` + `emit_jsonl()`
- [ ] 写 `main.cpp` argparse `--capture-mode={None|Training}`
- [ ] 测试 PASS

#### T3.3 [REFACTOR] JSONL schema 加 lock check
- [ ] 用 `nlohmann::json` (现成 dep) 替代手写 JSON 序列化
- [ ] schema 验证 helper: `assert_8_fields_consistent(json)`

#### T3.4 [AUDIT] Oracle: schema 稳定性 + IDistillationWriter 集成
- [ ] 验证 8 字段稳定性 (与 distill-source-survey 对齐)
- [ ] 验证 capture-mode fail-open 三重保护 (per ADR-0080 v1.2)

#### T3.5 [ARCHIVE] commit T3
- [ ] commit: `feat(example): pdk_chat_demo_evolution trace JSONL + capture-mode`

---

### T4: 实施期同步 3 份 SoT 文档

**估时**: 0.25 天
**依赖**: T2 + T3

#### T4.1 [GREEN] sync self-evolution-architecture §十一
- [ ] 加 row "L2 ✅ ship 2026-09-XX" (per design.md §九 cross-doc)
- [ ] 列: 端到端 6 段事件流
- [ ] 列: trace JSONL schema
- [ ] link to L2 README

#### T4.2 [GREEN] sync harness-architecture §十一
- [ ] 同上
- [ ] 标 `load(genome@N) → 重建 ChatSession → 1 turn` V2 缺口已闭环
- [ ] 例: 5-tier gate 在 L2 实例化

#### T4.3 [GREEN] sync rsi-architecture §十一
- [ ] 同上
- [ ] 标 Harness-RSI 在 L2 enabled
- [ ] 列: D7 stub provider 在 L2 端到端可用 (即 trace 选 providers 时能选)

#### T4.4 [GREEN] sync roadmap
- [ ] `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §十 Drift Log +1 L2 行
- [ ] §十一 Adjustment Log +1 L2 行

#### T4.5 [ARCHIVE] commit T4 (单独 atomic commit)
- [ ] commit: `docs(architecture+roadmap): sync L2 ship + SoT + roadmap cross-doc`

---

### T5: Oracle post-impl SHIP-with-fixes

**估时**: 0.5 天
**依赖**: T4

#### T5.1 [AUDIT] Oracle post-impl review
- [ ] 派 Oracle session: bg_x (待 rdd-builder 实施阶段生成)
- [ ] 期望 verdict: SHIP / SHIP-with-fixes / BLOCK
- [ ] 应用 Critical + Major fixes (如 SHIP-with-fixes)

#### T5.2 [GREEN] SHIP-with-fixes fixes
- [ ] 每次 fix commit atomic

#### T5.3 [ARCHIVE] 完整 archive (AGENTS.md Day 5 lesson)
- [ ] `git mv` 整个目录到 `openspec/changes/archive/pdk-chat-demo-evolution-reference-example/`
- [ ] 验证 4-file integrity (`.openspec.yaml + proposal.md + design.md + tasks.md + specs/<name>/spec.md`)

---

## Total Estimate

- T1: 0.5 天
- T2: 1.5 天
- T3: 0.5 天
- T4: 0.25 天 (cross-doc sync)
- T5: 0.5 天 (Oracle review + archive)
- **T6**: 0.5 天 (R8 反向指标门 + R9 反作弊测试, 2026-09-23 升级)
- **T6.7-T6.10**: 0.5 天 (R13 上下文驱动契约, 2026-09-23 升级 — ContextRequest 是 R8 反向指标门的源数据)
- **总计**: 4.25 天

**vs Master Plan 估时**: 2-3 天 (per proposal.md) → 1.25-2.25 天偏差 (R8 + R9 + R13 增量是 Cross-Doc Review 2026-09-23 后扩, 原估时未含)

---

### T6: R8 反向指标门 + R9 反作弊测试 (2026-09-23 升级)

**估时**: 0.5 天
**依赖**: T2 + T3 (主 demo 已可用, 注入 metrics 评估)

#### T6.1 [RED] test_reverse_indicators (R8.1 + R8.2 + R8.3)
- [ ] 写 `tests/test_reverse_indicators.cpp` 3 cases (与 spec.md R8.1/R8.2/R8.3 一一对应):
  - Case R8.1: drop_ratio > 5% 应 trigger exit non-zero + ack required
  - Case R8.2: 失败样本 trace 输出 4 字段 (`failure_event`, `rule_id`, `rule_shipped_commit`, `reproduce_in_new_task_demo`)
  - Case R8.3: ablation_report.json 3 段对照 (新/旧, 同任务/同 Harness, 拦截保留率)
- [ ] 验证 `ctest -R test_reverse_indicators` exit non-zero (RED)

#### T6.2 [GREEN] R8 metrics + flags 实施
- [ ] `evolution_session.cpp` 加 `--release-metrics` + `--regression-test-suite` + `--ablation-mode=full` flag
- [ ] 输出 `metrics.json` (new_up / new_down / old_up / old_down)
- [ ] 输出 `ablation_report.json` (3 段对照)
- [ ] 测试 PASS

#### T6.3 [RED] test_anti_cheat (R9.1 + R9.2 + R9.3)
- [ ] 写 `tests/test_anti_cheat_search_solution.cpp` 1 case (R9.1 — Poolside)
- [ ] 写 `tests/test_anti_cheat_metric_tampering.cpp` 1 case (R9.2 — 复旦马兴军)
- [ ] 写 `tests/test_anti_cheat_sandbox_escape.cpp` 1 case (R9.3 — ExploitGym)
- [ ] 验证 RED

#### T6.4 [GREEN] R9 反作弊测试用例实施
- [ ] Poolside test fixture (input 含 known baseline hint)
- [ ] 复旦 metric tampering test fixture (mutation gate 拒绝 evaluator schema write)
- [ ] ExploitGym sandbox test fixture (mutator 企图 outbound call → 拦截)
- [ ] 测试 PASS

#### T6.5 [AUDIT] Oracle: 反作弊测试充分性 + R8 双向指标覆盖
- [ ] Oracle session review R8.1-R8.3 + R9.1-R9.3 全部案例
- [ ] 应用 SHIP-with-fixes fixes (如 needed)

#### T6.6 [ARCHIVE] commit T6
- [ ] commit: `feat(example): pdk_chat_demo_evolution R8 + R9 (反向指标 + 反作弊)`

---

### T6.7-T6.10: R13 上下文驱动契约 (2026-09-23 升级)

**估时**: 0.5 天
**依赖**: T6.6 (R8 + R9 ship 后, ContextRequest 是 R8 反向指标门的"源数据")

#### T6.7 [RED] test_context_request_validation (R13.1 schema 校验)
- [ ] 写 `tests/test_context_request_validation.cpp` 5 cases:
  - Case R13.1.1: 缺 `context_id` → exit non-zero + 字段名
  - Case R13.1.2: `turn_input` 为空 → exit non-zero + context_id
  - Case R13.1.3: `task_class` 不在 enum → exit non-zero + 行号
  - Case R13.1.4: `invocation_mode` 不在枚举 → exit non-zero
  - Case R13.1.5: valid ContextRequest → 接受 + `meta.context_id` 进 trace
- [ ] 验证 `ctest -R test_context_request_validation` exit non-zero (RED)

#### T6.8 [GREEN] ContextRequest parser + L2 entrypoint flag 实施
- [ ] `evolution_session.cpp` 加 `--context-file <path.jsonl>` argparse
- [ ] ContextRequest JSONL parser (per spec.md R13.1 schema 8 字段)
- [ ] 启动无 `--context-file` → exit non-zero + stderr "ERROR: L2 零 hardcode..." (per S28)
- [ ] 任一 ContextRequest 字段缺失 → exit non-zero + 行号 + 字段名 (per S29-S30)
- [ ] 测试 PASS

#### T6.9 [RED] test_context_request_e2e (R13.3 ≥ 3 类 ContextRequest 实证)
- [ ] 写 `tests/test_context_request_e2e.cpp` 4 cases:
  - Case R13.3.1: `--context-file code-class.jsonl` (单类, 1 个 ContextRequest) → 期望 `accept-contexts` flag 输出单类 eval_quality, **不**能宣称 generalizable
  - Case R13.3.2: `--context-file code+research+debug-3class.jsonl` (3 类, 各 1 个) → 期望 `--accept-contexts` 输出 3 类对比
  - Case R13.3.3: `--context-file code+research+debug-3class.jsonl` + `--release-metrics` → R8.1 期望 ≥ 3 类对照矩阵
  - Case R13.3.4: ContextRequest `metadata.is_hidden=true` → L2 拒绝 + emit `hidden_context_rejected` 警告 (per E2 公开/隐藏集分离)
- [ ] 验证 RED

#### T6.10 [GREEN] `examples/contexts/` 3 类 reference ContextRequest + R13.4 / R13.6 集成
- [ ] 写 `examples/contexts/code-class-context.jsonl` (K8s YAML / Python test 2 个)
- [ ] 写 `examples/contexts/research-class-context.jsonl` (文献摘要 / 论文对比 2 个)
- [ ] 写 `examples/contexts/debug-class-context.jsonl` (日志分析 / 性能调优 2 个)
- [ ] 实施 `metadata.is_hidden` 字段处理 (L2 默认 false, true 时拒绝)
- [ ] R13.4 与 R8 + R9 集成: ContextRequest 进 trace `meta.context_id` + `failure_event` 字段引用 `context_id`
- [ ] 测试 PASS

#### T6.11 [AUDIT] Oracle: ContextRequest 零 hardcode + 跨 doc 一致性
- [ ] Oracle session review R13.1-R13.6 全部 case + L2 零 hardcode 验证
- [ ] 验证 trace JSONL `meta.context_id` 字段存在 + 反作弊 R9.2 (拒绝 mutation_metric_* ContextRequest) 实施
- [ ] 应用 SHIP-with-fixes fixes (如 needed)

#### T6.12 [ARCHIVE] commit T6.7-T6.10
- [ ] commit: `feat(example): pdk_chat_demo_evolution R13 上下文驱动契约 + ContextRequest`

---

## Out-of-Scope Tasks (deferred to future work)

- ❌ Wave 3 Phase 2 D4-D7 完整化 (独立立项)
- ❌ S4 Agent-Agent 协同进化 example (research 路径)
- ❌ 修改 `examples/pdk_chat_demo/` 主 demo (N1 强制)
- ❌ 引入新公共 API / Contract (N2 强制)
- ❌ L2 默认 ContextRequest (R13.2 零 hardcode 强制 — L2 不是 autonomous evaluator)

---

## Acceptance Criteria Checklist

L2 merge gate (含 R8 + R9 + R13 2026-09-23 升级):

- [ ] `cmake build -DAGENTICDSL_BUILD_EXAMPLES=ON -DAGENTICDSL_BUILD_TESTS=ON` exit 0
- [ ] `ctest -R test_evolution_session_mutation` exit 0 (4+ cases)
- [ ] `ctest -R test_evolution_session_load` exit 0 (5+ cases)
- [ ] `ctest -R test_evolution_tracer_schema` exit 0 (4+ cases)
- [ ] `ctest -R test_reverse_indicators` exit 0 (3 cases / R8.1+R8.2+R8.3)
- [ ] `ctest -R test_anti_cheat_search_solution` exit 0 (1 case / R9.1 Poolside)
- [ ] `ctest -R test_anti_cheat_metric_tampering` exit 0 (1 case / R9.2 复旦)
- [ ] `ctest -R test_anti_cheat_sandbox_escape` exit 0 (1 case / R9.3 ExploitGym)
- [ ] `ctest -R test_context_request_validation` exit 0 (5 cases / R13.1)
- [ ] `ctest -R test_context_request_e2e` exit 0 (4 cases / R13.3)
- [ ] `ctest -R pdk_chat_demo` (原 main demo) ZERO diff (N1 保证)
- [ ] 全量 ctest `-E pdk_chat_demo_evolution` baseline 211 零回归
- [ ] `./pdk_chat_demo_evolution` 启动无 `--context-file` → exit non-zero (R13.2 S28)
- [ ] `./pdk_chat_demo_evolution --context-file examples/contexts/code-class-context.jsonl` exit 0 + JSONL 8 字段 + `meta.context_id` 进 trace
- [ ] `./run_evolution_demo.sh --mock --release-metrics --ablation-mode=full --context-file examples/contexts/{code,research,debug}-class-context.jsonl` exit 0 (R8 metrics + ablation report + ≥ 3 类 ContextRequest)
- [ ] 3 份 SoT 文档 §十一 + §十二 + §12.9 (R13 引用) 同步 ship 行
- [ ] 4-file archive 完整 (proposal + design + tasks + specs/<name>/spec.md + .openspec.yaml)
- [ ] `examples/contexts/` 3 reference ContextRequest file ship (R13.3)
