# l2-evolution-real-execution-chain — Tasks

> **Status**: 🔍 Proposed Tasks
> **关联 Proposal**: [`./proposal.md`](./proposal.md)
> **关联 Design**: [`./design.md`](./design.md)

---

## Task Groups (TDD 5步结构)

每组遵循 RED-GREEN-REFACTOR 结构。

### Phase B — Minimum viable wiring (phase1 + phase2)

#### B1 [GREEN] evolution_session.h 新增成员
- [ ] 新增 include: `<agenticdsl/pdk/chat_session.h>`, `<agenticdsl/genome/genome.h>`, `<core/engine.h>`
- [ ] 新增成员: `std::unique_ptr<agenticdsl::DSLEngine> engine_`, `std::unique_ptr<hydraforge::pdk::ChatSession> chat_session_`, `hydraforge::pdk::AgentConfig agent_cfg_`
- [ ] 验证: `cmake --build build --target pdk_chat_demo_evolution` 编译通过

#### B2 [GREEN] evolution_session.cpp phase1_init 真实 wiring
- [ ] 实现 DSLEngine 构造 + IInteractionBus 注入
- [ ] 实现 LLMProviderFactory::create({"mock"}) + set_llm_provider
- [ ] 实现 ChatSession 11-参 ctor (mock 模式)
- [ ] 验证: `test_evolution_session_mutation` baseline test PASS

**Success criteria**: `./build/examples/pdk_chat_demo_evolution/pdk_chat_demo_evolution --mock --context-file examples/pdk_chat_demo_evolution/fixtures/contexts/code-class-context.jsonl --trace-events` exit 0

#### B3 [GREEN] evolution_session.cpp phase2_baseline 真实 chat() 调用
- [ ] 替换硬编码 trace: 调用 `chat_session_->chat(ctx.turn_input)`
- [ ] 捕获 `result.response`, `result.total_tokens`, `result.cost_usd` 填充 trace
- [ ] 验证: trace JSONL 含 `"response"` 非空 + `"tokens"` > 0

**Success criteria**: `--mock --trace-events` 输出 JSONL 的 baseline 段 `response` 字段非空

#### B4 [GREEN] main.cpp 真实 --mock 路径走 ChatSession chat()
- [ ] 确保 `--mock` 下 phase2_baseline 不崩溃 (ChatSession 需要 AgentConfig 有效)
- [ ] 验证 ctest 9/9 PASS

**Note**: ChatSession 11-参 ctor 需要完整的 DSLEngine + 注册的 tool. mock 模式不需要 LoopAgent .so.

### Phase C — Full chain (phase3-5 + --release-metrics + redaction)

#### C1 [GREEN] phase3_mutation 真实 apply_harness_mutation
- [ ] include `<agenticdsl/evolution/harness_rsi.h>`
- [ ] 构造 GenomeMutations + MutationGateContext, 调用 apply_harness_mutation
- [ ] 从 mutation result 获取 committed_genome_version 填充 trace
- [ ] 验证: binary exit 0 + mutation trace 段 `genome_version` 非硬编码

#### C2 [GREEN] phase4_reload_rerun 真实 genome load + ChatSession 重建
- [ ] 创建 FilesystemGenomeRegistry (通过 create_filesystem, 用 hermetic HOME)
- [ ] 实现 genome_to_agent_config() helper (L2 内部, N2 兼容)
- [ ] phase3 成功后调用 registry->commit → phase4 load → to_agent_config → ChatSession 重建 → rerun
- [ ] 验证: binary exit 0 + reload trace `response` 非空

#### C3 [GREEN] phase5_compare 真实 IEvaluator::compare()
- [ ] include `<agenticdsl/contract/ievaluator.h>`
- [ ] 实例化 IEvaluator (需要具体实现类 — 检查现成的 BehavioralEquivalenceEvaluator)
- [ ] 调用 compare(before_trace, after_trace)
- [ ] 输出 attribution_verdict
- [ ] 验证: compare trace 段 `attribution_verdict` ∈ {Attributed,Confounded,Insufficient,NotAttempted}

#### C4 [GREEN] --release-metrics 真实 drop_ratio 计算
- [ ] 从 phase5 compare 收集 baseline_failures + mutated_passes
- [ ] 计算 drop_ratio = (baseline_failures - mutated_passes) / baseline_total
- [ ] 写 metrics.json
- [ ] >5% exit non-zero
- [ ] 验证: `--release-metrics` 后 metrics.json 在 CWD 中

#### C5 [GREEN] R13.4 sensitivity redaction
- [ ] 实现 `detail::redact_trace_fields(event, sensitivity)` 在 evolution_session.cpp
- [ ] phase2/4 调用 redact 前检查 metadata.sensitivity
- [ ] `internal` → redact turn_input + response
- [ ] `confidential` → also redact tags + domain in meta
- [ ] 验证: 带 `sensitivity=confidential` 的 context → trace 含 `[REDACTED-confidential]`

### Phase D — SoT docs sync

#### D1 [GREEN] harness-architecture-2026-09.md §十一 L2 ✅ ship 行
- [ ] 在 §十一 表格中添加 "L2 reference example (real execution chain)" 行
- [ ] grep 验证: `grep "L2.*ship" docs/architecture/harness-architecture-2026-09.md` = 1

#### D2 [GREEN] rsi-architecture-2026-09.md §十一 L2 ✅ ship 行
- [ ] 同上

#### D3 [GREEN] self-evolution-architecture-2026-08.md §十一 L2 ✅ ship 行
- [ ] 校订/添加现有 L2 行 (predecessor 可能已有 placeholder)
- [ ] 验证: `grep "L2.*ship" docs/architecture/self-evolution-architecture-2026-08.md` = 1

### Phase E — Atomic commit + AGENTS.md sync

#### E1 [COMMIT] 单个 atomic commit 含所有 Phase B/C/D
- [ ] 检查 git status: 仅有 planned files 变化
- [ ] git add + commit with [Reverse Indicator] block
- [ ] 验证: `git log --oneline -1` 显示完整提交信息

#### E2 [DOC] AGENTS.md Recent Changes 同步
- [ ] 在 Recent Changes 段顶部添加本 change 条目
- [ ] 含 Reverse Indicator block 引用 commit hash

---

## Acceptance Criteria

- [ ] B2: `--mock --trace-events` exit 0 + trace JSONL 含非空 `response`
- [ ] B3: trace JSONL `tokens` > 0
- [ ] B4: `ctest -L l2-evolution` 9/9 PASS (零回归)
- [ ] C1: mutation trace 段 `genome_version` != 1
- [ ] C2: reload trace 段 `response` 非空
- [ ] C3: compare trace 段 `attribution_verdict` ∈ 合法枚举
- [ ] C4: `--release-metrics` → `metrics.json` 含 `drop_ratio` 字段
- [ ] C5: redact → `[REDACTED-*]` 替换
- [ ] D1-D3: 3 SoT §十一 grep "L2.*ship" 各 1
- [ ] E1: atomic commit, amend 不使用
- [ ] E2: AGENTS.md 同步

---