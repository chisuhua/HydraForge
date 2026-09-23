# pdk-chat-demo-evolution-reference-example — Design

> **Status**: 🔍 Proposed Design (PLACEHOLDER 待 rdd-builder 实施阶段细化)
> **关联 Proposal**: [`./proposal.md`](./proposal.md)
> **关联 Spec**: [`./specs/pdk-chat-demo-evolution/spec.md`](./specs/pdk-chat-demo-evolution/spec.md)

---

## 一、Context (上下文)

### 1.1 触发现状

本 change 直接源于用户决策 (2026-09-23):
> "用 pdk-chat-demo 作为承载例, 给三方 SoT 架构做可运行的 reference example"

+ 3 份 SoT 文档 (`self-evolution` v1.5 / `harness` v1.0 / `rsi` v1.0) 已 ship + §十一 traceback 已加 + 5 阶段 ship 实证就位.

### 1.2 核心缺口 (per Decision Record)

**`docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md` §3 第 6 项**:
> "变异必须经 IGenomeRegistry 持久化 (版本锚点存在)" — C4 GO 回注为 "GO with post-hoc closure gate (genome-wiring)" → **Wave 3 Phase 2 D7 "load(genome@N) → 重建 ChatSession → 1 turn" 待 L2 启动**.

本 change 是这个 post-hoc closure gate 的**首实例化**.

### 1.3 已 ship 但未端到端验证的接口

- `IGenomeRegistry::commit(genome)` (C2 ship)
- `apply_harness_mutation(mutations, ...)` (C4 ship)
- `IGenomeRegistry::load(name, version)` (C3 follow-up + G4 wiring ship)
- `ChatConfig::override_*` (Sprint 19 ship)
- `LLMProviderFactory::register_dynamic` (Wave 3 Phase 1 D7 ship)
- `IDistillationWriter` (ADR-0061-13 ship)

L2 的**唯一任务**: 实例化一个真实 demo 把这些接口**端到端串起来** — 既验证接口可组合性, 又给三方 SoT 提供 reference example.

---

## 二、Goals & Non-Goals

### Goals

1. **G1**: 提供 `examples/pdk_chat_demo_evolution/` 独立 binary, 一键跑通 mock + 真实 LLM 双模式 (5 秒 mock / 30 秒 真实)
2. **G2**: 实例化 6 段端到端链 (启动 → session → baseline → mutation → reload → compare), 走完 5-tier gate
3. **G3**: 把 V2 端到端缺口 `load(genome@N) → 重建 ChatSession → 1 turn` 在 L2 主路径实现并测
4. **G4**: trace JSONL schema 稳定 (4 段事件 × 8 字段), 给 distill-source-survey 提供新 consumption 路径
5. **G5**: 三方 SoT 文档 §十一 同步标 "L2 已 ship"
6. **G6**: Backwards compatible — `examples/pdk_chat_demo/` 主体零改动

### Non-Goals

- **N1**: 不修改 `examples/pdk_chat_demo/` 主体 (单方向 reference, 不能 reverse-impact)
- **N2**: 不引入新公共 API (L2 仅消费现有 API)
- **N3**: 不接 Wave 3 Phase 2 (D4-D7 完整化, 独立立项)
- **N4**: 不实现 S4 Agent-Agent 协同进化 (research 路径)
- **N5**: 不修改三层合同 (Contract layer / EventBuilder / Genome CRD) — 全 freeze, L2 仅消费

---

## 三、High-level Design

### 3.1 组件布局

```
                          ┌────────────────────────────────┐
                          │ examples/pdk_chat_demo_evolution/ │
                          │  (本 change 新建, N1 = 零改动主 demo) │
                          │                                  │
   ┌─────────────┐       │  ┌──────────────────────────────┐ │
   │ main.cpp    │───────│─→│ evolution_session           │ │
   │ --mock      │       │  │ 包装 ChatSession + 6 Agent   │ │
   │ --real-llm  │       │  │ + Genome 操作               │ │
   │ --trace     │       │  └──────────────┬───────────────┘ │
   └─────────────┘       │                 │                   │
                          │                 ▼                   │
                          │  ┌──────────────────────────────┐ │
                          │  │ evolution_tracer             │ │
                          │  │ 订阅 IInteractionBus,        │ │
                          │  │ 序列化为 JSONL stdout         │ │
                          │  └──────────────┬───────────────┘ │
                          │                 │                   │
                          │  ┌──────────────┴───────────────┐ │
                          │  │ 6 段事件流 (按 proposal.md) │ │
                          │  │ baseline / mutation /        │ │
                          │  │ reload / compare / trace/    │ │
                          │  └──────────────────────────────┘ │
                          └────────────────────────────────┘
                                         │
                                         ▼  外部调用 (5-tier gate)
                          ┌────────────────────────────────┐
                          │ src/evolution/harness_rsi.cpp  │  (C4 ship)
                          │ src/core/genome/registry_*     │  (C2 + G4 ship)
                          │ src/core/chat_session.cpp       │  (Sprint 19 ship)
                          │ src/modules/distillation/*      │  (ADR-0061-13 ship)
                          └────────────────────────────────┘
```

### 3.2 6 段端到端 (核心 happy path)

```
[用户]  ./run_evolution_demo.sh --mock
                                 │
                                 ▼
[1] main::main(argc, argv)
    args: --mock | --real-llm <provider> --trace-events --capture-mode={None|Training}
                                 │
                                 ▼
[2] EvolutionSession::run(args)
    ├── capture_mode = args.capture_mode (默认 None; --capture-mode training 启用 IDistillationWriter)
    ├── ChatSession ctor (4 参: DSLEngine*, AgentConfig{ChatConfig{}}, ILLMProvider*, ITimerService*)
    ├── register 6 Agent: Chat / Loop / Provider / Session / Budget / FS / Shell (复用 pdk_chat_demo main.cpp 模式)
    └── Subscribe evolution_tracer to IInteractionBus (per event_log W3.P1 主题)

[3] baseline_run(turn_input="write hello world in C++")  // fixture: fixtures/golden_inputs.jsonl Case 1
    ├── ChatSession::handle_input(turn_input)
    ├── tracer.record({"phase":"baseline", "response":<content>, "tokens":..., "cost_usd":...})
    └── 返回 baseline_response

[4] MutationGateChain::apply(mutations)  // 6 字段 apply_harness_mutation
    ├── G0 syntax: MutationRequest grammar check
    ├── G1 policy: is_tool_allowed(meta, policy) + semantic_locked_tools + denied_tools
    ├── G2 load: IGenomeRegistry::load(parent_version) + judge_data_freshness
    ├── G2.5 partial-apply: collect AppliedMutation (prompt_snapshot + tools_snapshot)
    ├── G3 persist-before-apply: fork(name, parent_version, final_spec) + commit(genome) (失败零状态变更)
    ├── tracer.record({"phase":"mutation", "gate_passes":[...], "genome_version":..., "applied_tools":...})
    └── 返回 AppliedMutation

[5] reload_and_rerun(genome_version, turn_input="write hello world in C++")  // **V2 缺口闭环**
    ├── IGenomeRegistry::load(name, version) → Genome
    ├── Genome::to_chat_config(loaded) → ChatConfig
    ├── ChatSession ctor (新 session, 用新 ChatConfig)  ←──── **L2 V2 关键**
    ├── ChatSession::handle_input(turn_input)
    ├── tracer.record({"phase":"reload", "genome_version":..., "response":<content>})
    └── 返回 post_mutation_response

[6] compare_traces(baseline, post_mutation)
    ├── BehavioralRegressionGate (IEvaluator V2 评估)
    ├── eval_quality 字段接受 Po/Por/Acpt (transition Guard G2 强依赖)
    ├── tracer.record({"phase":"compare", "verdict":"approved|denied", "eval_quality":"...", "attribution_verdict":...})
    └── 返回 CompareResult

[7] output JSONL stdout
    └── 4 段事件 (baseline + mutation + reload + compare), 各 8 字段
```

### 3.3 Trace JSONL Schema (稳定性保证)

**8 字段统一 schema** (per capture-mode-and-distillation-writer-v1 spec dist/record schema):

```json
{
  "phase": "baseline | mutation | reload | compare",
  "timestamp_iso8601": "2026-09-23T...",
  "session_id": "<uuid>",
  "turn_input": "<string>",
  "response": "<string | null>",        // baseline/reload 非空, mutation 为 null
  "tokens": 0,                          // int, real-LLM 模式
  "cost_usd": 0.0,                      // float, real-LLM 模式
  "meta": {
    "genome_version": "<name>@<version> | null",
    "gate_passes": ["G0","G1",...],     // mutation 段才有
    "eval_quality": "Acceptable|Poor|Excellent | null",  // compare 段
    "attribution_verdict": "Attributed|Confounded|Insufficient|NotAttempted | null",  // compare 段
    "trace_id": "<uuid>",
    "capture_mode": "None | Training"
  }
}
```

> Schema 稳定性: **任何字段不能在未升档 spec.md 前删除**. 加字段可以 (向后兼容), 改字段名不可以 (破坏 consumer).

---

## 四、Design Decisions

### D1: 不修改 pdk_chat_demo 主体 (单方向 dependency)

**理由**:
- 主 demo 是项目 reference implementation, 不应被 L2 演化的实验波动污染
- evolution demo 反而是 reference-for-evolution, 不污染 main release cadence

**实施**:
- L2 仅 `examples/pdk_chat_demo_evolution/` 新增
- `examples/CMakeLists.txt` 加 `add_subdirectory(pdk_chat_demo_evolution)` (sub-dir 独立性)
- main `examples/pdk_chat_demo/` 零改动 (0 diff line)

**拒绝的反方案**: 
- ❌ 在 `examples/pdk_chat_demo/main.cpp` 加 `--evolution` flag (会污染 main demo)

### D2: 通过 ChatConfig override_* 实例化 harness (不引新 contract)

**理由**:
- Sprint 19 ship 的 `ChatConfig::override_system_prompt` + `override_tools` + `override_*` 已具备完整接口
- L2 端到端 wire 已够用
- 不引新 contract = 不破坏 contract-layer freeze (per Self-Review Checklist)

**实施**:
- L2 mutation 实质 = `apply_harness_mutation` 的 L1 调用, 但 V2 reload 阶段用 `ChatConfig::override_*` 实例化
- 也就是说: mutation 生成 `Genome`, Genome → ChatConfig 路径用现有 reverse engineering (per C4 的 `Genome::to_chat_config()`)

**拒绝的反方案**:
- ❌ 新增 `Genome::materialize(ChatSession*)` 公共 API (N2 = break contracts)

### D3: 复用 ChatSession 主 demo 的 6 Agent + DSL 装载模式 (不复制代码)

**理由**:
- pdk_chat_demo main.cpp 已经过 12+ 集成测试验证 (per test_chat_session_*.cpp)
- L2 直接复用同一 ChatSession ctor pattern + 6 Agent Plugin 注册, 不写新 wrapper

**实施**:
- L2 的 `evolution_session.cpp::init()` 复用 `examples/pdk_chat_demo/main.cpp:240-310` 的初始化序列 (作为 `static` reference, 不 include main.cpp)
- 通过 `Agent` 接口统一装载 (Plugin 体系)

**拒绝的反方案**:
- ❌ L2 复制 pdk_chat_demo main.cpp 6 Agent 装载代码 (drift 风险)

### D4: capture-mode 三态可显式启用 (per distill-source-survey 推荐方案)

**理由**:
- distill-source-survey-2026-08 推荐 SessionWriter JSONL 作为过渡数据源
- L2 默认 capture-mode=None, `--capture-mode training` 启用 IDistillationWriter

**实施**:
- `event_handler.cpp::set_capture_mode(CaptureMode::Training)`
- 进 IInteractionBus 后事件自动 JSONL 化 + IDistillationWriter 写盘
- 与 ADR-0080 v1.2 amendment (CaptureMode 三态 + Training fail-open) 对齐

### D5: trace 输出默认 stdout (而非 file)

**理由**:
- 简单, 无 path 管理
- 与 Unix pipeline 友好 (可以 `./run_evolution_demo.sh | jq` 直接消费)
- 真实项目用 IDistillationWriter 写盘 (与 L2 无关, 是 ADR-0080 范畴)

**实施**:
- L2 `evolution_tracer.cpp::emit(json)` 直接 `std::cout << json << "\n"`
- `--trace-events` flag 启用, 默认 close (避免污染 `--mock` demo 输出)

### D6: mutation 默认是 prompt_delta (不触发 workflow_patch V1 defer)

**理由**:
- `workflow_patch` L3 V1 defer (Oracle bg_1f291bc4 DEAL-BREAKER)
- L2 默认 mutation = `prompt_delta` + `tools_add` (L1+L2 安全 path)
- `workflow_patch` 演示用例可启用作为 opt-in flag `--mutation-tier=L3` (但默认 disable, V2 立项后启用)

### D7: Evaluation 嵌入现成 IEvaluator V2

**理由**:
- IEvaluator V2 ship (BehavioralEquivalence + Composite)
- L2 用现成 interface, 不重新发明 eval_quality 字段

**实施**:
- `evolution_tracer.cpp::compare_phases()` 调现成 `IEvaluator` V2 实例
- 输出 `eval_quality: agenticdsl::evaluation::Quality` (复用 C2 G2 wiring 的 `agenticdsl::evaluation::quality_name` helper)

---

## 五、File-level Design

### 5.1 `examples/pdk_chat_demo_evolution/main.cpp`

```cpp
int main(int argc, char** argv) {
  // 1. 解析 args (复用 pdk_chat_demo cli_args_parser 部分)
  bool trace_events = false;
  std::string capture_mode_str = "None";  // 默认 None (训练模式用 --capture-mode training)
  std::string provider_str = "mock";       // --mock | --real-llm deepseek
  
  // 2. 实例化 EvolutionSession (封装 ChatSession + 6 Agent + tracer)
  EvolutionSession session(provider_str, capture_mode_str, trace_events);
  
  // 3. 跑 6 段端到端
  return session.run_6_phase_demo();
}
```

**估行数**: 60-80 行.

### 5.2 `evolution_session.{h,cpp}` (核心)

```cpp
class EvolutionSession {
public:
  EvolutionSession(provider, capture_mode, trace_events);
  int run_6_phase_demo();  // 入口
  
private:
  // 6 段方法
  void phase1_init();          // ChatSession + 6 Agent + tracer 注册
  void phase2_baseline(turn);  // ChatSession::handle_input, tracer.record
  void phase3_mutation(mutations);  // 5-tier gate
  void phase4_reload_rerun(genome_version, turn);  // V2 缺口闭环核心
  void phase5_compare();      // IEvaluator V2 + tracer.record
  void phase6_emit_jsonl();   // stdout (per D5)
  
  // Members
  std::unique_ptr<DSLEngine> dsl_;
  std::unique_ptr<ILLMProvider> provider_;
  std::unique_ptr<ITimerService> timer_;
  std::unique_ptr<EvolutionTracer> tracer_;
  std::unique_ptr<IEvaluator> evaluator_;  // V2 实例
  ChatConfig current_config_;
  std::optional<GenenomeVersion> last_genome_;
};
```

**估行数**: h (~80 行) + cpp (~250 行).

### 5.3 `evolution_tracer.{h,cpp}` (输出)

```cpp
class EvolutionTracer {
public:
  EvolutionTracer();  // subscribe IInteractionBus 内部
  void enable(bool on);  // --trace-events flag
  void record_phase(event);  // 4 段事件入口
private:
  bool enabled_;
  void emit_jsonl(const json&);  // D5: stdout
  json collect_meta();  // 8 字段统一 schema
};
```

**估行数**: h (~50) + cpp (~150).

### 5.4 测试 (3 个 binary / 12 cases)

- `tests/test_evolution_session_mutation.cpp` — Case 1: mock mutation → 期望 6 gate 全 pass
- `tests/test_evolution_session_load.cpp` — Case 2: V2 缺口 load(genome@N) → ChatSession ctor → 期望真实装载
- `tests/test_distillation_capture_mode.cpp` — Case 3: capture-mode=Training → JSONL + IDistillationWriter 路径

每个 binary 4 cases / 6 assertions minimum.

### 5.5 `CMakeLists.txt`

```cmake
add_executable(pdk_chat_demo_evolution
  main.cpp
  evolution_session.cpp
  evolution_tracer.cpp
)
target_link_libraries(pdk_chat_demo_evolution PRIVATE agenticdsl_core agenticdsl_common)
target_include_directories(pdk_chat_demo_evolution PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# tests
if(AGENTICDSL_BUILD_TESTS)
  add_executable(test_evolution_session_mutation tests/test_evolution_session_mutation.cpp)
  add_test(NAME test_evolution_session_mutation COMMAND test_evolution_session_mutation)
  # ... 类比 3 个 test
endif()
```

---

## 六、Backwards Compatibility (向后兼容)

### 6.1 公共 API 兼容

| API | 影响 |
|-----|------|
| `ChatSession` ctor (4 参) | **不变** |
| `ChatConfig::override_*` 5 方法 | **不变** |
| `IGenomeRegistry::commit/load/fork/walk_ancestors` | **不变** |
| `apply_harness_mutation` 6 字段签名 | **不变** |
| `LLMProviderFactory::register_dynamic` | **不变** |
| `IDistillationWriter` 接口 | **不变** |

### 6.2 Binary 兼容

- `examples/pdk_chat_demo/pdk_chat_demo` binary 行为零变化 (N1 强制保证)
- 全量 ctest `-E pdk_chat_demo_evolution` 不增测试数 (baseline 211, post-L2-merge 仍 211 + 3 new test binaries counted as 3 added)

### 6.3 数据兼容

- 不改 `~/.hydraforge/genomes/` 现有 layout (C2 ship)
- L2 写盘的 genome 与 Wave 3 Phase 1 D7 stubs 完全兼容
- trace JSONL 是 L2 新增, 不写盘 (per D5)

---

## 七、Risks

| # | 风险 | 影响 | 缓解 |
|---|------|------|------|
| R1 | ChatSession ctor 与 ChatConfig 结合出现 lifecycle 问题 (C4 5-tier gate 已 ship, 但 V2 reload 路径未 ship) | V2 缺口闭环实现可能 wave 5-tier gate 已经处理, 但 reload → 重建 ChatSession 仍是新路径 (per Decision Record §3 第 6 项) | L2 严格 follow 5-tier gate + `--strict` mode (任何 gate fail → exit non-zero) |
| R2 | `--real-llm` 模式需要 DEEPSEEK_API_KEY (sandbox 无 key) | 真实 LLM 验证受限 | 默认 `--mock` + Gate 验证; 单 dev developer local 跑 `--real-llm` |
| R3 | D7 stub (Wave 3 Phase 1) 与 L2 stub provider 命名冲突 | 命名 alias 风险 | L2 用 `_demo_evolution_<uuid>` prefix 避免冲突 |
| R4 | L2 demo 跑通路径 5-tier gate 中, G2 load + G3 commit 都涉及 IGenomeRegistry::commit, 频繁操作 file system | Disk 写入锁 contention | L2 默认 `--mock` 模式不写盘 (mem-only registry), 仅 `--real-llm` 启用 FilesystemGenomeRegistry |
| R5 | trace JSONL 输出可能被误认为 IDistillationWriter 写盘 (混淆) | 用户误解 trace 行为 | README 明确: "L2 trace = stdout, IDistillationWriter = 单独 `(per distill-source-survey)` flag 启用" |

---

## 八、Test Strategy

### 8.1 测试设计 (TDD 5 步 + per AGENTS.md)

**RED (写 failing tests)**:
- `test_evolution_session_mutation` Case 1 mock apply_mutation → 期望 5-tier gate 全部 pass + chat session mutation 应用 + eval_quality OK
- `test_evolution_session_load` Case 2 mock load(genome@N) → 期望 ChatSession 重新构造 + 响应差异 (baseline vs reload)
- `test_distillation_capture_mode` Case 3 capture-mode=Training → 期望 JSONL 路径触发 + IDistillationWriter 写盘

**GREEN (minimal impl)**:
- 复用 5-tier gate + ChatConfig + IGenomeRegistry 现成接口
- 唯一需要新写: `Genome::to_chat_config()` 反向映射 (in `src/core/genome/` per C4 决策, 现成)

**REFACTOR (清理)**:
- 抽公共 helper (EvolutionTracer::record_phase)
- D5 JSONL 输出统一 schema

**Oracle dual-agent review**:
- Metis: 路径完整性 + 边界
- Oracle: 设计物理可行性 + ABI / V2 缺口闭合

**archive**:
- 完整 4-file integrity per AGENTS.md Day 5 lesson

### 8.2 验收场景 (per spec.md §R1)

- **S1** (mock): `./run_evolution_demo.sh --mock` exit 0 + 4 段事件 + 8 字段 ✓
- **S2** (real-LLM): DEEPSEEK_API_KEY set, `--real-llm deepseek` 在 30 秒内生成 4 段事件 ✓
- **S3** (V2 缺口闭环): `phase4_reload_rerun` 真实装载 (load → ChatConfig → ChatSession) ✓
- **S4** (capture-mode=Training): trace JSONL + IDistillationWriter 写盘 1 个 record ✓
- **S5** (zero regression): `examples/pdk_chat_demo/main.cpp` `git diff` 0 行 ✓

---

## 九、Cross-doc Consistency (实施期同步)

**L2 实施期 commit 前**:
- [ ] `docs/architecture/self-evolution-architecture-2026-08.md` §十一 → 加 "L2 已 ship 行"
- [ ] `docs/architecture/harness-architecture-2026-09.md` §十一 → 加 "L2 已 ship 行"
- [ ] `docs/architecture/rsi-architecture-2026-09.md` §十一 → 加 "L2 已 ship 行"

**L2 实施期 commit 后**:
- [ ] `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §十 Drift Log → "+1 L2 row"
- [ ] `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §十一 Adjustment Log → "+1 L2 row"
- [ ] `docs/README.md` 索引表 (如有 reference 一段) → 新增 L2 行

---

## 十、Linkage

- **SoT (上游)**:
  - [`docs/architecture/self-evolution-architecture-2026-08.md` v1.5 §十一 + §十二](../../architecture/self-evolution-architecture-2026-08.md) (v1.5 + Verification Matrix 反向指标门 + 反作弊)
  - [`docs/architecture/harness-architecture-2026-09.md` v1.0 §十一 + §十二](../../architecture/harness-architecture-2026-09.md) (v1.0 + Verification Matrix 5-tier gate 反向校验)
  - [`docs/architecture/rsi-architecture-2026-09.md` v1.0 §十一 + §十二](../../architecture/rsi-architecture-2026-09.md) (v1.0 + Verification Matrix 真 RSI 三判据 + R8/R9 引用)
- **决策 (上游)**:
  - [`docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md` §3 第 6 项](../../audits/2026-09-21-harness-rsi-pilot-go-no-go.md) (post-hoc closure gate)
  - [`docs/architecture/pdk-chat-demo-distill-source-survey-2026-08.md`](../../architecture/pdk-chat-demo-distill-source-survey-2026-08.md) (推荐数据源)
- **下游 (本 change 创生)**:
  - `openspec/specs/pdk-chat-demo-evolution/spec.md` (R1-R9 需求: R1-R7 主需求 + R8 反向指标门 + R9 反作弊测试)
  - `openspec/changes/pdk-chat-demo-evolution-reference-example/tasks.md` (TDD 5 步 tasks + T6 R8/R9)

### 10.7 R8 反向指标门 + R9 反作弊测试设计 (2026-09-23 升级)

**R8 Reverse Indicator Gate (per spec.md §R8)**:
- L2 主流程需暴露 `--release-metrics` + `--regression-test-suite` + `--ablation-mode=full` 三个 flag
- 双向指标 (R8.1) + 失败可追溯 (R8.2) + 消融实验 (R8.3) 三档必输出
- drop_ratio > 5% 自动 block (R8.1 红线), 失败样本 100% 可 trace (R8.2 红线)

**R9 Anti-Cheat Test Suite (per spec.md §R9)**:
- L2 测试用例必须包含 3 类已知失效模式:
  - R9.1 搜现成答案 (Poolside / Terminal-Bench 2.0 失效模式) — eval_quality diff 应 > -10%
  - R9.2 修改评判指标 (复旦马兴军团队实测失效模式) — MutationGate 拦截 evaluator schema write
  - R9.3 串谋外部平台 (OpenAI ExploitGym 失效模式) — sandbox network_mode=none 防御

**Cross-doc consistency**: R8/R9 与 3 份 SoT 文档 §十二 Verification Matrix 同步, 任何 change 在 SoT §十二 + L2 spec.md 必须双向引用.

- **Roadmap**:
  - [`docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §五 Sprint 36](../../roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md) (V2 L2 立项窗口)

---

**Design phase 0**: 设计完整, 待 rdd-builder P1 (plan gen) + P2 (execute) 进入实施. 不需要 Phase 0 case 1 - 4 另起 (per ADR-0049 auto-decision complex branch).
