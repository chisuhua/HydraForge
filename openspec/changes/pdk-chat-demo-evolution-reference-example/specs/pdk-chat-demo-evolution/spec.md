# pdk-chat-demo-evolution spec — Requirements

> **Status**: 🔍 Proposed Spec (PLACEHOLDER 待 rdd-builder 实施阶段细化)
> **关联 Proposal**: [`../../proposal.md`](../../proposal.md)
> **关联 Design**: [`../../design.md`](../../design.md)

---

## 范围

本规范定义 `examples/pdk_chat_demo_evolution/` 子项目 (本 change L2 创生) 的功能需求。

**核心命题 (proposal §Why.4)**: HydraForge 自进化方向架构以 3 份 Source of Truth 文档 (`self-evolution-architecture-2026-08.md` v1.5 + `harness-architecture-2026-09.md` v1.0 + `rsi-architecture-2026-09.md` v1.0) 为一致基线。本 change 提供 L2 reference example, 让用户能 clone 项目后实际跑通"5-tier gate 端到端 + trace + reload 真实装载"的 mutation 路径。

---

## 需求 (Requirements)

### R1: 独立 binary + 双模式

**The system shall** provide a standalone binary `examples/pdk_chat_demo_evolution/main.cpp` with arguments:
1. `--mock` — Mock LLM mode (CI/CT 验证, 5 秒内 exit 0)
2. `--real-llm <provider>` — Real LLM mode (需 `DEEPSEEK_API_KEY`, 30 秒内 exit 0)
3. `--capture-mode={None|Training}` — Default `None`, `Training` 启用 IDistillationWriter
4. `--trace-events` — Default off, 启用后 4 段事件 emit 到 stdout
5. `--context-file <path.jsonl>` — **必填 (R13 唯一输入入口)**, 用户提供 ContextRequest

**The binary shall** exit 0 on success + emit JSONL trace per `--trace-events`. Exit non-zero on any 5-tier gate failure. **Exit non-zero if `--context-file` 缺失** (per R13.2 S28).

#### 验收场景
- **S1**: `./run_evolution_demo.sh --mock --context-file examples/contexts/code-class-context.jsonl` exit 0 + trace JSONL 8 字段 ✓
- **S2**: `DEEPSEEK_API_KEY=... ./run_evolution_demo.sh --real-llm deepseek --trace-events --context-file ...` 在 30 秒内生成 4 段事件 ✓

---

### R2: 6 段端到端 demo 链

**The system shall** execute in order these 6 phases (per design.md §3.2). **turn_input 全部来自 ContextRequest** (C2 修复 2026-09-23, per R13.2 第 3 条 "所有 6 段事件流的源头都是 ContextRequest 行"). `fixtures/golden_inputs.jsonl` 仅作为 `examples/contexts/` reference file 供用户 clone + modify, **不是** L2 自动运行的输入源.

1. **Phase 0 (Load Contexts)**: `ContextRequest::load(--context-file)` → schema 校验 (S28-S31) → contexts_ vector
2. **Phase 1 (Init)**: 实例化 `ChatSession` + 6 Agent Plugin (Chat/Loop/Provider/Session/Budget/FS/Shell) + 注册 `evolution_tracer` 订阅 `IInteractionBus`
3. **Phase 2 (Baseline)**: 跑 `ContextRequest[0].turn_input` → record `{phase:"baseline", context_id:..., response:..., tokens:..., cost_usd:...}`
4. **Phase 3 (Mutation)**: 调 `apply_harness_mutation(prompt_delta + tools_add)` (C4 ship 6 字段接口) → 走 5-tier gate → emit `{phase:"mutation", context_id:..., gate_passes:["G0","G1","G2","G2.5","G3"], genome_version:..., applied_tools:...}`
5. **Phase 4 (Reload + Rerun)** ⭐: `IGenomeRegistry::load(name, version)` + `Genome::to_chat_config()` (L2 新代码, M3) + `ChatSession` 重建 → 同一 ContextRequest turn_input → record `{phase:"reload", context_id:..., genome_version:..., response:...}`
6. **Phase 5 (Compare)**: 调 `IEvaluator` V2 (`BehavioralEquivalence` + `Composite`) → emit `{phase:"compare", context_id:..., verdict:"approved|denied", eval_quality:"Acceptable|Poor|Excellent", attribution_verdict:"Attributed|Confounded|Insufficient|NotAttempted"}`
7. **Phase 6 (Emit JSONL)**: stdout (per design.md §3.3 D5) → 循环下一行 ContextRequest[i+1], 全部处理完 exit 0

#### 验收场景
- **S3**: mock 模式 6 段 phase0~phase6 各 1 次调用 (ContextRequest[0] 驱动), exit 0
- **S4**: V2 缺口路径 `load(genome@N) → 重建 ChatSession → 1 turn` 真实装载 (Phase 4, 用 ContextRequest turn_input)

---

### R3: 5-Tier Gate Sequence

**The system shall** execute 5-tier gate in order (per design.md §3.2 Phase 3):

- **Gate 0 (Syntax)**: `MutationRequest` 字段必填检查 + types
- **Gate 1 (Policy + 3 Signals)**:
  - `policy.semantic_locked_tools` 不在 `mutation.tools_add` 范围内
  - `policy.denied_tools` 不在 `mutation.tools_add` 范围内
  - `attribution_verdict == "Attributed"` (per ADR-0086 v1.1 `judge_data_freshness()`)
  - `eval_quality != "Poor"` (per ADR-0083 V2 `Composite`)
  - `budget_state != "exceeded"`
- **Gate 2 (Load + Freshness)**:
  - `IGenomeRegistry::load(name, parent_version)` 成功
  - `judge_data_freshness(data, current, registry)` 返回 "Attributed" (per C3 follow-up)
- **Gate 2.5 (Partial-Apply)**: 收集 `AppliedMutation` 快照 (prompt_snapshot + tools_snapshot), 准备内存 apply (无副作用)
- **Gate 3 (Persist-Before-Apply)**:
  - `fork(name, parent_version, final_spec)` 持久化到 IGenomeRegistry (per G4)
  - commit 失败 → `RegistryRejected` + 零状态变更
  - commit 成功 → `genome.committed` 事件 + 在 chat session 装载

#### 验收场景
- **S5**: Case 1.1 — mock apply_mutation(prompt_delta) → 5-tier gate 全 PASS (per T2.1)
- **S6**: Case 1.3 — apply_mutation(invalid prompt_delta) → Gate 0 fail + exit non-zero
- **S7**: Case 1.4 — apply_mutation(denied_tools) → Gate 1 fail + emit `mutation.denied` 事件

---

### R4: trace JSONL Schema Stability

**The system shall** emit JSONL 4 段事件 to stdout (per design.md §3.3):

```json
{
  "phase": "baseline | mutation | reload | compare",
  "timestamp_iso8601": "2026-09-23T...",
  "session_id": "<uuid>",
  "turn_input": "<string>",
  "response": "<string | null>",
  "tokens": 0,
  "cost_usd": 0.0,
  "meta": {
    "genome_version": "<name>@<version> | null",
    "gate_passes": ["G0","G1",...],
    "eval_quality": "Acceptable|Poor|Excellent | null",
    "attribution_verdict": "Attributed|Confounded|Insufficient|NotAttempted | null",
    "trace_id": "<uuid>",
    "capture_mode": "None | Training",
    "context_id": "<uuid>",                  // ← M2 修复 (R13 S31): 来源于 ContextRequest, 必填
    "task_class": "<enum>",                  // ← M2 修复 (R13): 来源于 ContextRequest
    "is_hidden": <bool>,                     // ← M2 修复 (R13): 来源于 ContextRequest
    "sensitivity": "<public|internal|confidential>"   // ← M2 修复 (R13): 来源于 ContextRequest
  }
}
```

**Schema 稳定性保证**: 任何字段不能在未升级 spec.md 前删除. 加字段向后兼容, 改字段名破坏 consumer. **R13 集成 (M2 修复)**: trace JSONL **必含** `meta.context_id` (per R13.2 第 5 条 + S31). 顶层 8 字段不变, meta 内部 4+4=8 字段 (原 4 + R13 新增 4).

#### 验收场景
- **S8**: trace 4 段事件各 8 顶层字段 + meta 内 8 字段 (含 context_id/task_class/is_hidden/sensitivity) (per T3.1 Case 3.1 + T6.10 R13 集成)
- **S9**: meta 字段含 capture_mode (与 R5 集成) + context_id (与 R13 集成)

---

### R5: Capture-Mode Integration (ADR-0080 D10)

**The system shall** support `--capture-mode=Training` flag, which enables `IDistillationWriter` to write Session JSONL to disk (per distill-source-survey-2026-08.md 推荐路径).

#### 验收场景
- **S10**: `--capture-mode=Training` 启用后, `IDistillationWriter` 写盘至少 1 个 `DistillationRecord` (per T3.1 Case 3.3)
- **S11**: `--capture-mode=None` (默认) 不写盘
- **S12**: capture-mode=Training fail-open 三重保护 (per ADR-0080 v1.2): Online → Training 降级 audit `event_log.capture_mode_downgrade` 事件

---

### R6: Backwards Compatibility (N1 强制)

**The system shall** NOT modify `examples/pdk_chat_demo/` main binary. The two binaries (`pdk_chat_demo` + `pdk_chat_demo_evolution`) shall coexist.

#### 验收场景
- **S13**: `git diff examples/pdk_chat_demo/` (pre-L2 vs post-L2-merge) = 0 lines (per T1.4 N1 guarantee)
- **S14**: `examples/pdk_chat_demo/pdk_chat_demo --mock --help` 行为零变化
- **S15**: 全量 `ctest -E pdk_chat_demo_evolution` baseline 211 零回归 (L2 add 6 new test binaries; 实测 `grep '^add_test' build/tests/CTestTestfile.cmake | wc -l` = 211, per Mi3 校准 2026-09-23)

---

### R7: 公共 API Contract Freeze (N2 强制)

**The system shall** NOT introduce new public API or Contract. L2 shall仅 consume:
- `ChatSession` ctor (4 参)
- `ChatConfig::override_*` 5 method
- `IGenomeRegistry` 接口 (commit/load/fork/walk_ancestors)
- `apply_harness_mutation` 6 字段签名
- `LLMProviderFactory::register_dynamic` (Wave 3 Phase 1 D7 stub 复用)
- `IDistillationWriter` 接口 (D10 Capture)

#### 验收场景
- **S16**: `git diff include/` (pre-L2 vs post-L2-merge) = 0 lines (公共接口 freeze)
- **S17**: L2 主 demo binary 通过 `nm` / `ldd` 检查仅依赖既有 .so

---

## 跨文档一致性

| SoT 文档 | L2 实施后更新 |
|---------|--------|
| [`docs/architecture/self-evolution-architecture-2026-08.md`](../../../../architecture/self-evolution-architecture-2026-08.md) §十一 / §十二 | 加 "L2 ✅ ship" row + 6 段事件流注 + §十二 反向指标门引用 |
| [`docs/architecture/harness-architecture-2026-09.md`](../../../../architecture/harness-architecture-2026-09.md) §十一 / §十二 | 加 "L2 ✅ ship" row + V2 缺口闭环注 + §十二 5-tier gate 反向校验引用 |
| [`docs/architecture/rsi-architecture-2026-09.md`](../../../../architecture/rsi-architecture-2026-09.md) §十一 / §十二 | 加 "L2 ✅ ship" row + D7 stub 端到端注 + §十二 真 RSI 三判据引用 |
| [`docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md`](../../../../roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md) §十 / §十一 | +1 row each (L2 ship + cross-doc consistency) |

---

## R8: 反向指标门 (Reverse Indicator Gate)

> **来源**: per Cross-Doc Review 2026-09-23, 基于三阶段文档评审 Gap #2 + 用户提交 16 模块 E6 + 反向指标精神 ("只报涨不报掉属选择性披露，不予通过")
> **核心命题**: 任何"能力 ship"的证据必须同时输出**正向 + 反向**指标, 缺一不予 merge.
> **生效日期**: L2 merge 时一并生效, 后续任何 ship gate 强制.

### R8.1 能力退化测试结果 (Capability Regression Report)

**The system shall** 在任何"能力 ship"事件 (新 capability / 新 mutation gate / 新 trace event) 中, 输出双向指标:
- **正向指标 (新涨)**: 新能力覆盖 scope 的提升量 (e.g., eval_quality Acceptable % 从 80% → 90%)
- **反向指标 (旧掉)**: 已 ship 的旧能力在同一改动后的退化量 (e.g., 旧 prompt delta 路径 Acceptance 从 85% → 75% — drop ratio 必须 ≤ 5%)

**Any-drop > threshold block**: 反向指标 drop ratio > **5%** 自动 block, 需显式 ack (Single-Dev 模式 = author ack in commit message).

#### 验收场景
- **S36**: L2 `--release-metrics` flag 输出 4 字段 (new_up / new_down / old_up / old_down) + drop_ratio 阈值校验. drop > 5% → exit non-zero
- **S18**: L2 `--regression-test-suite` flag 跑 N 个 pre-ship acceptance suite, 输出 drop matrix

---

### R8.2 失败 → 约束可追溯 (Failure → Constraint Traceability)

**The system shall** 对任意 failure sample 必须能 trace 到:
1. 触发它的 contract / hook / event 名称
2. 该 contract 何时 ship + 关联 commit hash
3. 同类 failure 在 **全新任务** 上的复现拦截 (per Failure Mode Verification)

**Acceptance**: 任何 commit message 含 `failure samples traced to: <contract-id>@<sha>`.

#### 验收场景
- **S19**: L2 demo 包含 3 个 failure case, 每个 case 输出 trace 行 (`failure_event:`, `rule_id:`, `rule_shipped_commit:`, `reproduce_in_new_task_demo:`)
- **S20**: failure sample 0% 失配 (任何 failure 都必须能 trace)

---

### R8.3 消融实验 (Ablation Experiment)

**The system shall** 对任何 Harness 变更 (ChatConfig.override_* / apply_harness_mutation / Genome 切换) 提供对照数据:
- **A 路径**: 新 Harness (新 mutation / 新 Genome version)
- **B 路径**: baseline Harness (同模型, 仅换 Harness)

Acceptance 必须输出 3 段对照:
1. **同任务, 不同 Harness**: baseline_response vs post_mutation_response (eval_quality diff)
2. **同 Harness, 不同任务**: baseline task vs mutation task (eval_quality 一致性)
3. **失败样本 drop ratio**: 旧失败样本在新 Harness 下保留拦截率

#### 验收场景
- **S21**: L2 `--ablation-mode=full` flag 跑 3 段对照并出 JSON 输出 (per [`docs/architecture/{*}-architecture-*.md §十二` 反向指标段](../../../../architecture/))
- **S22**: 输出 `ablation_report.json` 必须含 3 段 + eval_quality diff + 拦截保留率

---

## R9: 反作弊测试 (Anti-Cheat Test Suite)

> **来源**: per Cross-Doc Review 2026-09-23, Gap #1 + 用户提交 16 模块 R1-R4 + 反作弊 3 模式 (Poolside / Terminal-Bench 2.0 搜现成答案 / 复旦马兴军团队 修改评判指标 / OpenAI ExploitGym 串谋外部平台)
> **核心命题**: L2 demo 不能 ship 出"自证型"评估 — 测试套件必须包括 3 类已知失效模式.
> **生效日期**: 同 R8. L2 merge 时一并生效.

### R9.1 防"搜现成答案" (No Search-of-Existing-Solution)

**The system shall** 在 evaluation suite 中包含:
- (a) 一个**故意**包含"已知 baseline 答案 hint" 的 Golden input (e.g., 输入文本中嵌入"the answer is X")
- (b) 一个**没有 hint** 的对照 Golden input

**Acceptance**:
- Agent 收到带 hint input 时**不应直接复述 hint** (eval_quality 必须 < 接受阈值, 表示未"复制答案"行为)
- 收到无 hint input 时**应正常解题** (eval_quality ≥ Acceptable)

#### 验收场景
- **S23**: tests/test_anti_cheat_search_solution.cpp Case 3.1 — hint input / non-hint input 两组, eval_quality diff 应 > -10% (hint 不应触发"作弊高")

---

### R9.2 防"修改评判指标" (No Metric Tampering)

**The system shall** 验证评估者本身**无权**修改 metric:
- Mutation gate 拒绝任何尝试修改 `evaluate_quality` / `attribution_verdict` / `genome.committed` payload schema 的 mutation
- audit 链 (ADR-0080) 记录任何 `metric.*` schema write 企图 + emit `evaluation.tampering_attempt` 事件
- evaluator 自身是只读 (IEvaluator V2 已 ship, 无 write 接口)

#### 验收场景
- **S24**: tests/test_anti_cheat_metric_tampering.cpp Case 3.2 — 模拟 evaluator 尝试 mutate eval_quality schema → 期望 Gate 1 fail + `evaluation.tampering_attempt` 事件
- **S25**: IEvaluator 接口 grep verify 无 `set_*_metric` 或 write 方法

---

### R9.3 防"串谋外部平台" (No External Collusion / Sandbox Escape)

**The system shall** 在 sandbox (SkillInterpreter / Docker Backend per ADR-0075) 边界上:
- 限制 outbound network 到纯 whitelisted endpoint (DEEPSEEK API)
- 检测 zero-day RCE 模式 (per OpenAI ExploitGym research) — sandbox 模型串外部平台拿答案
- 失败 → `sandbox.escape_attempt` 事件 + 自动回滚到 baseline state
#### 验收场景
- **S26**: tests/test_anti_cheat_sandbox_escape.cpp Case 3.3 — 模拟 agent 企图 outbound call → 期望 sandbox 拦截 + 事件
- **S27**: docker backend 在 **L2 sandbox 模式** 下 `network_mode=none` (per ADR-0075 D2 EnvBackend 允许配置; L2 场景显式 opt-in none, 非通用默认) — outbound 全部拒绝除非显式 opt-in

---

## R13: 上下文驱动契约 (Context-Driven Contract, 2026-09-23)

> **来源**: 用户原话 "L2 只是提供了用户交互的设施, 具体还要用户提供一个具体上下文请求, 这个上下文请求创建的目标才能做 harness/自进化/rsi 的验证"
> **核心命题**: L2 是 **reference example 入口设施**, **不**是 autonomous evaluator. 任何 harness-rsi / data-rsi / model-rsi 验证**必须** 由用户提供 ContextRequest 触发, 不是 L2 demo 自身自动跑.
> **生效日期**: L2 merge 时一并生效.

### R13.1 ContextRequest Schema (L2 唯一入口契约)

**The system shall** 接受 `--context-file <path.jsonl>` flag, 每行为一个 ContextRequest JSON object, 含 6 顶层字段 (含 metadata 对象, metadata 内 4 子字段):

```json
{
  "context_id": "<uuid>",                              // 1. 用户定义的上下文 ID (用于 trace 关联)
  "turn_input": "<string>",                             // 2. 用户给 ChatSession 的输入 (e.g., "write K8s nginx YAML")
  "task_class": "<enum>",                               // 3. 任务类型 (code_gen | research | summary | debug | classify | ...)
  "expected_eval_quality": "<Acceptable|Poor|Excellent|null>",  // 4. 用户对 baseline 的期望 (per E2 公开集/隐藏集 — 标注 is_best_of_known)
  "invocation_mode": "<mock|real_llm_deepseek|real_llm_custom>", // 5. 调用模式 (per §六 Backwards Compat)
  "metadata": {                                        // 6. 可选元数据
    "domain": "<string>",                              // e.g., "k8s", "auth", "data-flow"
    "tags": ["<string>", ...],                         // e.g., ["baseline", "Wave-3-Phase-2-candidate"]
    "is_hidden": <bool>,                              // 7. per E2 公开/隐藏集分离 (false = 公开, true = 隐藏)
    "sensitivity": "<public|internal|confidential>"   // 8. per H2 凭证隔离
  }
}
```

#### Schema 字段约束

| 字段 | 必填 | 约束 |
|------|------|------|
| `context_id` | ✅ | UUID v4 唯一; **必须由用户提供**, L2 不自动生成 |
| `turn_input` | ✅ | non-empty string; L2 **不**做内容验证 (用户全责) |
| `task_class` | ✅ | enum 限定; L2 仅 echo, 不分类 (用户全责) |
| `expected_eval_quality` | ⚠️ optional | 若提供, 即"用户标注的 baseline 期望"; 用于 R8.2 失败可追溯 (`failure_event: user_expected_X_but_got_Y`) |
| `invocation_mode` | ✅ | mock / real_llm_deepseek / real_llm_custom; 默认 mock |
| `metadata.domain` | ⚠️ | 推荐填写, 用于 cross-context comparison (e.g., "k8s" vs "auth") |
| `metadata.is_hidden` | ⚠️ | 默认 false (公开集); 显式 true 时为隐藏集 (per E2 红线) |
| `metadata.sensitivity` | ⚠️ | 默认 public; internal/confidential 时 L2 不写盘 + redact trace |

### R13.2 L2 行为契约 (零 hardcode)

**The system shall** 在 ContextRequest 处理上:
1. **只接受 JSONL 输入** — `--context-file <path>` 必填; 无 `--context-file` flag 时 exit non-zero + 提示 "L2 零 hardcode, 必须用户提供 ContextRequest"
2. **不提供任何 default ContextRequest** — 不同于 `golden_inputs.jsonl` (那是 reference 示例, 不是 hardcode)
3. **L2 内部不生成 ContextRequest** — 所有 6 段事件流的源头都是 ContextRequest 行
4. **ContextRequest 100% 由用户负责** — L2 不检查 `turn_input` 是否"有意义", 不强制 `task_class` 分类, 不决定 `expected_eval_quality` 是否"实际合理" (per E1 红线 — L2 不做 Agent 自动提炼)
5. **trace JSONL 必须含 `context_id` 字段** — 用于跨 ContextRequest 对比 + 失败可追溯

#### 验收场景
- **S28**: L2 binary 启动时无 `--context-file` flag → exit non-zero + stderr "ERROR: L2 零 hardcode, 必须提供 ContextRequest via --context-file"
- **S29**: ContextRequest JSONL 任一字段缺失必填 → exit non-zero + 行号 + 字段名
- **S30**: ContextRequest 任一 `turn_input` 为空 → exit non-zero + context_id + 行号
- **S31**: trace JSONL 4 段事件均含 `meta.context_id` 字段 (与 ContextRequest 对应)

### R13.3 三类 ContextRequest 必要性 (per 用户 R3 红线)

**任何"实现自进化"声称** (per rsi §11.8.4 + §12.1.1) 必须由 **≥ 3 类 ContextRequest** 实证:

| ContextRequest 类 | 用户须提供 | L2 验证 |
|------------------|-----------|----------|
| **Code 类** (`task_class: code_gen`) | K8s YAML / Python 测试 / SQL 查询 | harness-rsi 5-tier gate + eval_quality 反向指标 |
| **Research 类** (`task_class: research`) | 文档摘要 / 文献对比 / 论文摘要 | data-rsi capture-mode=Training + IDistillationWriter |
| **Debug 类** (`task_class: debug`) | 日志分析 / 错误诊断 / 性能调优 | model-rsi provider 选择 + eval_quality 对比 |

**Acceptance**: ≥ 3 类 ContextRequest 实证. **单类不构成 generalizable 自进化声明**.

#### 验收场景
- **S32**: L2 提供 `examples/contexts/{code,research,debug}-class-context.jsonl` 3 个 reference ContextRequest file (供 user clone + modify)
- **S33**: L2 demo `--accept-contexts` flag 显示 3 类 ContextRequest 的 eval_quality 对比 (per R8.1 反向指标)
- **S34**: 用户可提供 `metadata.tags: ["baseline", "Wave-3-Phase-2-candidate"]` 自定义 tag, L2 echo 进 trace (不解释)

### R13.4 与 R8/R9 的关系

**R13 与 R8 (反向指标门) 的关系**:
- R8.1 (能力退化测试) 必须跨 ContextRequest 类 (≥ 3 类) 比较 — 单类退化 ≠ generalizable 退化
- R8.2 (失败可追溯) 必须 trace 到 `context_id` — L2 输出 `failure_event: <event> context_id: <uuid> rule_id: <contract>`
- R8.3 (消融实验) 必须跨 ContextRequest 类 (≥ 3 类) — 单类消融 ≠ generalizable 消融

**R13 与 R9 (反作弊) 的关系**:
- R9.1 (搜现成答案) — 用户可隐式提供 ContextRequest hint (per L2 test fixture); 但 production ContextRequest 必须无 hint
- R9.2 (修改评判指标) — L2 拒绝任何 `mutation_metric_*` ContextRequest (即"明确列名让 Agent 改 metric")
- R9.3 (串谋外部平台) — sandbox 默认 `network_mode=none` + ContextRequest 内 `turn_input` 含网络调用关键字时 (e.g., "fetch http://...") 自动拒绝 + 警告

### R13.5 Cross-doc 一致性

| SoT 文档 | R13 §新修订 |
|---------|-----------|
| [`docs/architecture/self-evolution-architecture-2026-08.md` §12.9`](../../../../architecture/self-evolution-architecture-2026-08.md) | (NEW) 9 段闭环任一段的评估必须由 ContextRequest 触发 |
| [`docs/architecture/harness-architecture-2026-09.md` §12.9`](../../../../architecture/harness-architecture-2026-09.md) | (NEW) H1-H6 红线验证必须由 ContextRequest 触发 |
| [`docs/architecture/rsi-architecture-2026-09.md` §12.9 + §11.8.8`](../../../../architecture/rsi-architecture-2026-09.md) | (NEW) R3 元指标实证必须 ≥ 3 类 ContextRequest + R4 四权分离明示 |
| [AGENTS.md "Reverse Indicator Rule"](../../../../AGENTS.md) | (R13 引用) commit `[Reverse Indicator]` 段必须含 `context_ids: <uuid list>` |

### R13.6 L2 默认行为 (零 ContextRequest)

L2 ship 时默认行为:
- 启动无 `--context-file` → exit non-zero (per S28)
- 无任何 hardcode ContextRequest
- 唯一 ContextRequest 来源: 用户

**L2 与生产 Main Demo (`examples/pdk_chat_demo/`) 的区别**:
- Main Demo: 用户自由输入, 无 ContextRequest schema (自由 chat)
- L2 Demo: **必须** 提供 ContextRequest (受 schema 约束), 用于 harness/自进化/rsi 验证

#### 验收场景
- **S35**: L2 binary 与 `examples/pdk_chat_demo/pdk_chat_demo` binary 并存, 入口不同 (后者无 `--context-file` flag)

---

## 关联文档

- **Proposal**: [`../../proposal.md`](../../proposal.md) (Why/What/Capabilities/Impact)
- **Design**: [`../../design.md`](../../design.md) (D1-D7 decisions + 5-tier gate detail + R13 ContextRequest flow)
- **Tasks**: [`../../tasks.md`](../../tasks.md) (TDD 5 步 +6 task groups, T6 含 R13)
- **SoT (L1 traceback + R13)**:
  - [self-evolution §十一 + §12.9](../../../../architecture/self-evolution-architecture-2026-08.md)
  - [harness §十一 + §12.9](../../../../architecture/harness-architecture-2026-09.md)
  - [rsi §十一 + §12.9 + §11.8.8](../../../../architecture/rsi-architecture-2026-09.md)
- **Decisions / Audits**:
  - [Decision Record V2 缺口 §3 第 6 项](../../../../audits/2026-09-21-harness-rsi-pilot-go-no-go.md)
  - [Distill Source Survey 2026-08](../../../../architecture/pdk-chat-demo-distill-source-survey-2026-08.md)
- **AGENTS.md**:
  - [Reverse Indicator Rule](../../../../AGENTS.md) (commit [Reverse Indicator] 段必填 + R13 引用)
- **R8 + R9 引用**:
  - [§R8 反向指标门](#r8-反向指标门-reverse-indicator-gate) (本 spec)
  - [§R9 反作弊测试](#r9-反作弊测试-anti-cheat-test-suite) (本 spec)

- **Proposal**: [`../../proposal.md`](../../proposal.md) (Why/What/Capabilities/Impact)
- **Design**: [`../../design.md`](../../design.md) (D1-D7 decisions + 5-tier gate detail)
- **Tasks**: [`../../tasks.md`](../../tasks.md) (TDD 5 步 +5 task groups)
- **SoT (L1 traceback)**:
  - [self-evolution §十一](../../../../architecture/self-evolution-architecture-2026-08.md)
  - [harness §十一](../../../../architecture/harness-architecture-2026-09.md)
  - [rsi §十一](../../../../architecture/rsi-architecture-2026-09.md)
- **Decisions / Audits**:
  - [Decision Record V2 缺口 §3 第 6 项](../../../../audits/2026-09-21-harness-rsi-pilot-go-no-go.md)
  - [Distill Source Survey 2026-08](../../../../architecture/pdk-chat-demo-distill-source-survey-2026-08.md)
