## Context

### 真实闭环缺口 (per AGENTS.md §Reverse Indicator Rule R3)

Genome wiring 闭环第 7 环 ship (`openspec/changes/archive/genome-wiring-harness-rsi-gepa-2026-09-22/`, 2026-09-22), 但**真实 LLM 验证**仅覆盖"突变提案"环节. 自进化 v1.5 9 段闭环 R3 元指标实证要求全链路真实 LLM 验证, 当前缺口:

```
LLM 生成 prompt_delta
  → MutationGate Gate 0-3 评分       ⚠️ mock only (H2)
    → IGenomeRegistry fork() persist  ⚠️ mock only (H3)
      → apply_harness_mutation 内存应用  ⚠️ mock only
        → genome.committed 事件          ⚠️ 事件发射存在, real LLM 验证缺
```

### dual-agent 调研产物 (2026-10-14)

- **报告**: `.rddf/audits/2026-10-14-p0-proposals-dual-agent-review.md` (8 节详尽分析)
- **Oracle session**: `ses_ec5f38a92ffeLuZb4aIXmev0j3`
- **Metis session**: `ses_ec5f36ffaffeT3KEsoFqAUPq43`
- **4 强交叉命中** (Oracle + Metis 独立识别):
  - A: 实现路径清晰, 前置依赖全 ship
  - B: "Gate 0-3 真实 LLM 评分" 是误称 — gate 是确定性规则, LLM 只参与提案生成
  - C: Case 4 drop_ratio 物理不可行 (L2 1-2 contexts 0/50/100% 二值)
  - D: 现有 Case 1/3 依赖固定路径断言, flaky 风险
- **4 独占发现** (与本 change 正交, 已立为独立 improvement):
  - E (Metis): env_vars secrets redaction
  - F (Metis): UTF-8 切片 bug
  - G (Oracle): sink pointer RAII 生命周期
  - H (Oracle): hermetic HOME 强制 (**本 change 前置**)

### 现有 baseline (前置 ship, 0 改动)

- `tests/test_harness_rsi_pilot.cpp` 22/22 mock (2026-09-22 ship)
- `tests/test_genome_registry.cpp` 13/13 mock
- `tests/test_harness_mutation_proposal_real_llm.cpp` 1/1 real LLM smoke
- `src/modules/cognitive/harness_rsi.cpp::apply_harness_mutation` (2026-09-22 ship, Gate 0-3 + fork)
- `src/core/genome/registry_filesystem.cpp::IGenomeRegistry::fork/load/walk_ancestors` (2026-09-22 ship)
- ADR-0068 Appendix A v2.3 `genome.committed` + `genome.persist_failed` 主题 (2026-09-22 ship)
- `tests/test_helpers/real_llm_env.h::must_require_real_llm_env()` (2026-09-29 ship)

## Goals / Non-Goals

### Goals

1. **G1**: 新增 `tests/test_harness_mutation_commit_real_llm.cpp` (must_realllm label, 4 cases redesign)
2. **G2**: 4 cases 全部 PASS at real LLM (DEEPSEEK_API_KEY set)
3. **G3**: 既有 baseline 零回归 (22 + 13 + 1 = 36 cases 不变)
4. **G4**: Recording Provider 守卫 (per AGENTS.md §5 守卫 CI 路径零防护)
5. **G5**: hermetic HOME fixture (per 2026-09-21 C1 教训)
6. **G6**: Case 4 drop_ratio descope (per dual-agent 共识物理不可行)
7. **G7**: AGENTS.md §Recent Changes 段登记本 ship (含 R3 元指标实证 claim)
8. **G8**: 必须_realllm label + `must_require_real_llm_env()` helper 复用既有机制

### Non-Goals

- **不**改源代码 (Harness / IGenomeRegistry / MutationGate / Genome Registry 实现层全 ship)
- **不**实现 H5 反作弊 R9 真实 LLM (独立 `rsi-anti-cheat-real-llm` change 范围)
- **不**触发 Wave 3 Phase 2 D4 LoRA 启动 (待 D4 立项)
- **不**新增 IToolRegistry streaming API (per `adr-0072-d1-stream-v2-true-streaming.md` 独立范围)
- **不**实现 Case 4 drop_ratio (per dual-agent 共识 descope)

## Decisions

### D1: Case 1/3 断言 redesign (state consistency)

**原提案 (mock 时代)**:
- Case 1: "commit → reload → 状态一致" (浅层断言, 含义模糊)
- Case 3: "失败回滚 → 零状态变更" (需构造 gate denied 路径, 真实 LLM 不可控)

**Decision**: 改 **state consistency 断言** (per AGENTS.md Pattern #3 分层):
- Case 1 断言:
  - `IGenomeRegistry::load(commit_result.genome_version)` 返回的 Genome 与 commit 前内存**逐字段一致** (`spec.harness` diff 工具比对)
  - `walk_ancestors` 链含父版本 + 新版本 (长度 ≥2)
  - `genome.committed` 事件 payload 含 `genome_version` 字段
- **不**断言 Gate 评分具体值 (LLM 输出不可控)
- **不**断言 commit 后某字段等于某值 (mock 时代陷阱)

**Rationale**: per Metis AI 失败模式 #1/#2 + Oracle C1 — 真实 LLM 输出不可控, 严格断言必 flaky. 状态一致性是"无论 LLM 输出什么, 行为契约成立" 的 robust 断言.

### D2: Case 3 gate denied 确定性构造

**原提案 (依赖 LLM)**:
- 真实 LLM mutation commit 失败回滚 → gate denied → 零状态变更
- **问题**: 真实 LLM 可能生成合法 proposal → Gate 通过 → Case 3 flaky

**Decision**: **不**依赖真实 LLM 生成非法 proposal. 改用**确定性非法输入**:
- 构造缺必需字段的 proposal (e.g. 缺 `tools_add` / 缺 `prompt_delta`) 直接喂给 Gate
- OR: 构造 `tools_add` 包含 ToolMetadata V3 白名单未注册的工具名 → Gate 0 即 deny
- 跑 ≥3 iter 验证 100% consistency (零 flaky)

**Rationale**: per Metis AI 失败模式 #2 + Oracle C2 — 真实 LLM 下无法确定性触发 denied, 必须分离"测试 deterministic deny"和"测试 LLM 提案生成"两个独立路径.

### D3: Case 4 drop_ratio descope

**原提案**:
- Case 4: 真实 LLM mutation commit + persist 后 reload, drop_ratio ∈ 合理范围

**Decision**: **descope** (不实现). 在 spec R4 Requirement 明确 descope rationale:

```
R4 (UPDATED per 2026-10-15 descope):
  drop_ratio 红线 5% 触发 (per AGENTS.md §R8.1) 在 L2 1-2 contexts 粒度物理不可行
  - 实测 0/50/100% 二值, 5% 阈值无法稳定构造 (per 2026-10-06 l2-evolution-real-llm-provider C4 deferred ack)
  - 需 ≥20 mutations 才能稳定触发
  - 红线验证属独立 OpenSpec change (per `.rddf/audits/2026-10-14-rddf-improvements-audit.md` "R8.1 红线 触发 验证" 提案)
  - 实施本 change 时, drop_ratio 计算路径**不**做断言, 仅做"metrics 字段存在 + 公式语义正确" 验证
```

**Rationale**: per dual-agent C 强命中 (L2 1-2 contexts 0/50/100% 二值). 强实施会 flaky + 物理不可行, descope 释放实施资源到 Case 1-3.

### D4: Recording Provider 守卫

**Decision**: 配对 `RecordingLLMProvider` (per AGENTS.md §5 守卫 CI 路径零防护):
- 实现 `ILLMProvider` 三虚 (`generate` / `generate_stream` / `available_models`)
- 录制真实 LLM req/payload 到内存 (录制一次, CI 回放)
- CI `HYDRAFORGE_SKIP_REAL_LLM=1` 下回放, 验证 contract 行为

**Rationale**: per AGENTS.md §5 + 2026-09-08 沉淀 — CI skip 模式不防护时, 真实 contract 回归无任何 CI 信号. Recording Provider 守 4 cases 全部契约断言在 CI 路径有效.

**注**: 严格遵循 AGENTS.md §5 "production code 传给 provider 的参数契约" 模式, **不**改 must_realllm 强制语义 (本地必须真 LLM, CI Recording 回放).

### D5: hermetic HOME fixture

**Decision**: 测试 setup/teardown 隔离 `~/.hydraforge/genome.key` 宿主状态 (per 2026-09-21 C1 genome.key fresh-deploy 教训):

```cpp
class HermeticGenomeHome {
  std::string saved_home_;
  std::string saved_hydraforge_home_;
public:
  HermeticGenomeHome() {
    saved_home_ = getenv("HOME") ? getenv("HOME") : "";
    saved_hydraforge_home_ = getenv("HYDRAFORGE_HOME") ? getenv("HYDRAFORGE_HOME") : "";
    auto tmpdir = fs::temp_directory_path() / ("hermetic_" + uuid_v4());
    fs::create_directories(tmpdir);
    setenv("HOME", tmpdir.c_str(), 1);
    setenv("HYDRAFORGE_HOME", tmpdir.c_str(), 1);
  }
  ~HermeticGenomeHome() {
    setenv("HOME", saved_home_.c_str(), 1);
    setenv("HYDRAFORGE_HOME", saved_hydraforge_home_.c_str(), 1);
    // fs::remove_all(tmpdir)
  }
};
```

**Rationale**: per 2026-09-21 C1 — 测试若依赖宿主 `~/.hydraforge/genome.key` 状态, fresh-deploy 路径永不触发, 真 production bug 被掩盖. hermetic fixture 强制 fresh-deploy 路径测试有效.

### D6: InMemoryBus wait_for_drain() 守卫

**Decision**: Case 2 commit 返回后, **不**立即查 `genome.committed` 事件, 先 `InMemoryBus::wait_for_drain()` 排空 (per AGENTS.md Pattern #9 contract drain API):

```cpp
executor.commit(mutation);
bus->wait_for_drain();  // 排空 in-flight 事件
auto events = bus->drain();
REQUIRE(events contains "genome.committed");
```

**Rationale**: per AGENTS.md Pattern #9 + Pattern #7 — commit 返回时事件可能仍 in-flight, 立即查询 race. wait_for_drain 排空保证断言稳定.

### D7: must_realllm label 复用既有机制

**Decision**: 新 test binary `LABELS "must_realllm"` (per 2026-09-29 ship 机制, 5 binary 用过):

```cmake
add_catch_test(test_harness_mutation_commit_real_llm
  SOURCES tests/test_harness_mutation_commit_real_llm.cpp
  LIBRARIES agenticdsl_core
)
set_tests_properties(test_harness_mutation_commit_real_llm PROPERTIES
  LABELS "must_realllm"
)
```

CI 强制验证: skip=1 触发 FAIL (per AGENTS.md §3 短路不 FAIL 语义 — Recording Provider 必须替代).

**Rationale**: per AGENTS.md §3 + 2026-09-29 改造 — must_realllm 标签确保本地真 LLM 验证 + CI 守卫双轨. 本 change 不发明新机制, 复用既有.

### D8: 0 源代码改动 + test only

**Decision**: 本 change **0 源代码改动**:
- `src/modules/cognitive/harness_rsi.cpp` 0 改动 (Gate 0-3 + apply_harness_mutation 已有)
- `src/core/genome/registry_filesystem.cpp` 0 改动 (fork/load/walk_ancestors 已有)
- `src/core/genome/genome.h` 0 改动 (Genome / AttributionRecord 已有)
- `src/common/llm/llm_types.h` 0 改动 (ILLMProvider 接口已有)
- 1 file 新增 (test) + 1 file CMake 改 (`tests/CMakeLists.txt`) + 2 file spec/doc (`openspec/specs/.../spec.md` + `AGENTS.md`)

**Rationale**: Genome wiring 闭环第 7 环已 ship (2026-09-22), 实施层完整. 本 change 仅补"测试覆盖缺口", 复用既有 22+13+1 = 36 mock baseline.

## Implementation Sketch (TDD 5-step)

### Step 1: RED 写失败测试
- `tests/test_harness_mutation_commit_real_llm.cpp` 4 cases 全部 fail (因 RecordingLLMProvider 未注册 + hermetic HOME 未隔离)
- 验证 4 cases 在 Recording Provider fixture 缺失时 fail, baseline 36 cases 不变

### Step 2: GREEN 最小实现
- `RecordingLLMProvider::RecordingLLMProvider(...)` 录制 req/payload 内存
- `RecordingLLMProvider::generate(req, token)` 调录制时 LLM response (CI 回放) 或真 LLM (本地)
- `HermeticGenomeHome::HermeticGenomeHome()` setenv/unsetenv 模式
- 4 cases 重设计为 state consistency 断言 (per D1)

### Step 3: REFACTOR 提取共享 helper
- `tests/test_helpers/recording_provider.h` 提取 RecordingLLMProvider
- `tests/test_helpers/hermetic_home.h` 提取 HermeticGenomeHome
- 既有用例 (若需) 可复用

### Step 4: 集成验证
- `cmake --build build --target test_harness_mutation_commit_real_llm` exit 0
- `ctest --test-dir build -L must_realllm -R test_harness_mutation_commit_real_llm` PASS
- 既有 36 cases `ctest --test-dir build -LE must_realllm` 全 PASS (零回归)

### Step 5: 文档 + 决策登记
- `AGENTS.md §Recent Changes` 段登记本 ship (含 R3 元指标实证 claim + Case 4 descope rationale)
- `docs/active-status.md` 同步
- `openspec/changes/2026-10-15-harness-mutation-commit-real-llm/` 6-file 完整性 (per Day 5 lesson)
- 24h cooling-off 后 archive

## References

- `.rddf/audits/2026-10-14-p0-proposals-dual-agent-review.md` (dual-agent 调研产物)
- `.rddf/state/.arch-handoff.json::trigger_pre_issues` (6 pre_issues 记录)
- `.rddf/state/.planner-handoff.json` (6 pre_issues_must_resolve_before_rdd_builder_p0)
- `.rddf/improvements/harness-mutation-commit-real-llm.md` (5-segment 修订完成)
- `openspec/changes/archive/genome-wiring-harness-rsi-gepa-2026-09-22/` (闭环第 7 环)
- `docs/architecture/harness-architecture-2026-09.md` §十二 H1-H6
- `docs/architecture/self-evolution-architecture-2026-08.md` §十二 R3 元指标
- `docs/adr/adr-0068-event-emission-contract.md` Appendix A v2.3 (genome.* 主题)
- `tests/test_helpers/real_llm_env.h` (must_realllm helper)
- AGENTS.md §REAL LLM TESTING §5 (Recording Provider 模式)
- AGENTS.md Pattern #3 (断言强度分层)
- AGENTS.md Pattern #7 (concurrent ctest race detection)
- AGENTS.md Pattern #9 (contract drain API)
- AGENTS.md Pattern #10 (hygiene drift)
- 2026-09-21 C1 genome.key fresh-deploy 教训
- 2026-10-06 l2-evolution-real-llm-provider C4 deferred ack
