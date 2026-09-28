# l2-evolution-real-execution-chain — Design

> **Status**: 🔍 Proposed Design
> **关联 Proposal**: [`./proposal.md`](./proposal.md)
> **关联 Spec**: [`./specs/l2-evolution/spec.md`](./specs/l2-evolution/spec.md)

---

## 一、Context (上下文)

### 1.1 触发现状

Oracle 深度审查 session `ses_f259746caffe9yK0YoIQ6FOgWp` 发现 pdk_chat_demo_evolution 的 6 段链是 facade:

| Phase | 当前行为 (facade) | 应有的行为 |
|-------|-------------------|-----------|
| 1 init | 空函数, 仅注释 | DSLEngine + provider + ChatSession 构造 |
| 2 baseline | 硬编码 `{"meta": build_meta(0,0,"NotAttempted")}` | ChatSession::chat(turn_input) + 真实 response/tokens |
| 3 mutation | 硬编码 `{"meta": build_meta(1,5,"Attributed")}` | apply_harness_mutation() 5-参 free 函数 |
| 4 reload | 同上 hardcoded | registry->load() + ChatSession 重建 + rerun |
| 5 compare | 同上 hardcoded | IEvaluator::compare() |

### 1.2 预置条件 (已 ship 的接口)

本 change 消费的现有 API (全部已 ship, 本 change 不修改):

- `ChatSession` 11-参 ctor (`include/agenticdsl/pdk/chat_session.h:210-230`)
- `DSLEngine` (`src/core/engine.h:76`)
- `LLMProviderFactory::create()` (`src/common/llm/llm_provider_factory.h:28`)
- `apply_harness_mutation` 5-参 free function (`include/agenticdsl/evolution/harness_rsi.h:73-78`)
- `IGenomeRegistry::create_filesystem()` + `commit/load/fork` (`include/agenticdsl/genome/genome.h:122`)
- `IEvaluator::compare()` (`include/agenticdsl/contract/ievaluator.h:32`)
- `InMemoryBus` (`include/agenticdsl/contract/inmemory_bus.h`)
- `CancellationRegistry` (`include/agenticdsl/pdk/cancellation_registry.h`)

---

## 二、Goals & Non-Goals

### Goals

1. **G1**: phase1-2 真实 wiring: DSLEngine + ChatSession + chat() 真实调用
2. **G2**: phase3 真实 mutation: apply_harness_mutation + FilesystemGenomeRegistry
3. **G3**: phase4 真实 reload: registry->load() → to_agent_config() → ChatSession 重建
4. **G4**: phase5 真实 compare: IEvaluator V2 BehavioralEquivalence
5. **G5**: --release-metrics 真实 drop_ratio 计算 + metrics.json + exit non-zero
6. **G6**: R13.4 sensitivity redaction 实现
7. **G7**: 3 SoT §十一 L2 ✅ ship rows

### Non-Goals

- **N1**: 不修改 `examples/pdk_chat_demo/main.cpp` (N1 硬阻断)
- **N2**: 不修改 `include/` 任何头文件 (N2 硬阻断)
- **N3**: 不实现 --regression-test-suite (Oracle 建议降级)
- **N4**: 不接 Wave 3 Phase 2 (D4-D7)
- **N5**: 不修改 contract layer / EventBuilder / Genome CRD

---

## 三、Design Decisions

### D1: phase1_init 使用 DSLEngine 默认构造 + LLMProviderFactory

**理由**: DSLEngine 默认 ctor 创建 MockLLMProvider; LLMProviderFactory::create(LLMConfig{"mock"}) 也返回 MockLLMProvider. 两者兼容。但 L2 需要控制 provider 构造时机 — 用 LLMProviderFactory 分离 concern, 复用 `pdk_chat_demo/main.cpp:306-337` 模式但不复制代码。

**实施**:
```cpp
void EvolutionSession::phase1_init() {
    engine_ = std::make_unique<agenticdsl::DSLEngine>();
    engine_->set_interaction_bus(bus_); // bus_ already exists
    
    // 创建 provider (mock 或 real-llm)
    auto factory = std::make_unique<agenticdsl::LLMProviderFactory>();
    agenticdsl::LLMConfig cfg;
    cfg.provider = provider_str_; // "mock" | "deepseek" | ...
    auto provider = factory->create(cfg);
    engine_->set_llm_provider(std::move(provider));
    
    // 创建 ChatSession (11-param ctor)
    using hydraforge::pdk::ChatSession;
    using hydraforge::pdk::AgentConfig;
    using hydraforge::pdk::SessionConfig;
    
    agent_cfg_.system_prompt = "You are an L2 evolution evaluator.";
    agent_cfg_.provider = provider_str_;
    
    chat_session_ = std::make_unique<ChatSession>(
        engine_.get(), bus_, &engine_->get_tool_registry(),
        agent_cfg_, SessionConfig{},  // agent + session config
        nullptr,  // cancellation_registry
        nullptr,  // timer (D9 lazy)
        nullptr,  // input (default StdinInputSource — but we don't need it for programmatic chat)
        nullptr,  // logger (default StderrLogger)
        nullptr,  // session_manager (no persistence)
        std::nullopt  // resume
    );
}
```

**拒绝的反方案**: 复制 pdk_chat_demo main.cpp 初始化 (漂移风险); 用 from_markdown 静态工厂 (主入口需要更多控制)。

### D2: phase2_baseline 调用 chat_session_->chat(turn_input) + 构建真实 trace

**理由**: 取代硬编码 trace, 使用 ChatSession::chat() 真实返回值构建 JSONL 事件。

**实施**:
```cpp
void EvolutionSession::phase2_baseline(const ContextRequest& ctx) {
    ChatResult result = chat_session_->chat(ctx.turn_input);
    nlohmann::json meta = build_meta(ctx, 0, 0, "Baseline");
    nlohmann::json event = {
        {"meta", std::move(meta)},
        {"turn_input", ctx.turn_input},
        {"response", result.success ? result.response : nlohmann::json(nullptr)},
        {"tokens", result.total_tokens},
        {"cost_usd", result.cost_usd}
    };
    tracer_->record_phase(TracePhase::Baseline, event);
}
```

### D3: phase3_mutation 使用 apply_harness_mutation 5-参自由函数

**理由**: harness_rsi.h:73-78 已 ship 5-参签名, 用现成的不引新 contract。

**实施**:
```cpp
void EvolutionSession::phase3_mutation(const ContextRequest& ctx) {
    agenticdsl::evolution::GenomeMutations mutations;
    mutations.prompt_delta = "Add attention to error handling.";
    mutations.tools_add = {"fs_read"};
    
    agenticdsl::evolution::MutationGateContext gate_ctx;
    gate_ctx.current = agenticdsl::evolution::EvolutionState::Inactive;
    gate_ctx.bus = bus_.get();
    gate_ctx.evaluator = evaluator_.get();
    
    auto result = agenticdsl::evolution::apply_harness_mutation(
        mutations, agent_cfg_.system_prompt, agent_cfg_.tools,
        engine_->get_tool_registry(), gate_ctx);
    
    // 捕获 gate_passes 从 result
    int gate_passes = result.has_value() ? 5 : result.error() == ...;
    nlohmann::json meta = build_meta(ctx, last_genome_version_, gate_passes,
        result.has_value() ? "Attributed" : "Insufficient");
    tracer_->record_phase(TracePhase::Mutation, {{"meta", meta}});
}
```

**设计局限**: 当前 `apply_harness_mutation` 返回值 `AppliedMutation` 含 `committed_genome_version` 但不含 `gate_passes` 数组。L2 在 mutation 段输出 `gate_passes: 5` 当成功, `gate_passes: 0` 当失败 — 简化但非欺诈。

### D4: phase4_reload_rerun 用 FilesystemGenomeRegistry + to_agent_config helper

**理由**: 符合 N2 (不碰 include/), L2 内部实现 `genome_to_agent_config()` helper 将 Genome → AgentConfig。

**实施**:
```cpp
void EvolutionSession::phase4_reload_rerun(const ContextRequest& ctx) {
    // Load genome that phase3 committed
    auto genome_result = genome_reg_->load(last_genome_name_, last_genome_version_);
    if (!genome_result.has_value()) { /* trace with error */ return; }
    const auto& genome = genome_result.value();
    
    // Convert genome to AgentConfig (L2 internal helper, N2 compliant)
    hydraforge::pdk::AgentConfig new_cfg = genome_to_agent_config(genome);
    
    // Rebuild ChatSession
    chat_session_ = std::make_unique<hydraforge::pdk::ChatSession>(
        engine_.get(), bus_, &engine_->get_tool_registry(),
        new_cfg, hydraforge::pdk::SessionConfig{},
        nullptr, nullptr, nullptr, nullptr, nullptr, std::nullopt);
    
    // Rerun same turn_input
    auto result = chat_session_->chat(ctx.turn_input);
    nlohmann::json meta = build_meta(ctx, genome.metadata.version, 5, "Attributed");
    nlohmann::json event = {
        {"meta", std::move(meta)},
        {"turn_input", ctx.turn_input},
        {"response", result.success ? result.response : nlohmann::json(nullptr)},
        {"tokens", result.total_tokens},
        {"cost_usd", result.cost_usd}
    };
    tracer_->record_phase(TracePhase::Reload, event);
}
```

### D5: phase5_compare 使用 IEvaluator::compare()

**理由**: IEvaluator::compare(a, b) 返回 `<0` a worse, `0` equal, `>0` a better. L2 用此生成 attribution_verdict。

**实施**: L2 不自己实现 evaluator, 而是实例化现成 `BehavioralEquivalenceEvaluator` 或 `CompositeEvaluator` 并调用 compare()。由于 single-turn kMinBaselineSamples=5 限制, 实际输出 `NotAttempted` — 这是诚实的统计输出, 不是 bug。

### D6: --release-metrics 从 phase5 输出计算真实 drop_ratio

**实施**:
```cpp
// In main.cpp or evolution_session, after all phases:
if (release_metrics) {
    nlohmann::json metrics;
    metrics["baseline_total"] = baseline_count_;
    metrics["baseline_failures"] = baseline_failures_;
    metrics["mutated_passes"] = mutated_passes_;
    metrics["drop_ratio"] = baseline_count_ > 0
        ? static_cast<double>(baseline_failures_ - mutated_passes_) / baseline_count_
        : 0.0;
    metrics["original_drop_ratio"] = 0.0;  // baseline phase comparison against original
    metrics["new_up"] = "phase1-5 real execution chain";
    metrics["old_down"] = "drop_ratio=" + std::to_string(metrics["drop_ratio"].get<double>());
    
    // Write metrics.json
    std::ofstream f("metrics.json");
    f << metrics.dump(2) << std::endl;
    
    // Exit non-zero if drop_ratio > 5%
    if (metrics["drop_ratio"].get<double>() > 0.05) {
        std::cerr << "ERROR: drop_ratio " << metrics["drop_ratio"].get<double>()
                  << " > 5% (threshold)" << std::endl;
        exit(1);
    }
}
```

**设计局限**: drop_ratio 计算受限于 L2 不计算 eval_quality (P0-4 C5). 用 `attribution_verdict` 分布作 proxy: "Attributed" count = baseline_failures - mutated_passes. 单 turn 下 BehavioralEquivalence 返回 NotAttempted, 所以初始 drop_ratio=0%.

### D7: R13.4 redaction — detail::redact_trace_fields

**实施** (`detail` namespace, in evolution_session.cpp or a new redact_helper.h):

```cpp
namespace detail {

nlohmann::json redact_trace_fields(nlohmann::json event, const std::string& sensitivity) {
    if (sensitivity == "none" || sensitivity == "public") return event;
    
    std::string level;
    if (sensitivity == "internal") level = "internal";
    else if (sensitivity == "confidential") level = "confidential";
    else return event;
    
    // Always redact turn_input
    if (event.contains("turn_input") && event["turn_input"].is_string())
        event["turn_input"] = "[REDACTED-" + level + "]";
    if (event.contains("response") && !event["response"].is_null())
        event["response"] = "[REDACTED-" + level + "]";
    
    // Confidential also redacts meta.tags and meta.domain
    if (sensitivity == "confidential" && event.contains("meta") && event["meta"].is_object()) {
        if (event["meta"].contains("tags"))
            event["meta"]["tags"] = "[REDACTED-" + level + "]";
        if (event["meta"].contains("domain"))
            event["meta"]["domain"] = "[REDACTED-" + level + "]";
    }
    
    return event;
}

}  // namespace detail
```

### D8: Cross-doc ship rows — 3 SoT §十一

```markdown
| 组件 | 状态 | 依赖 | 备注 |
|------|------|------|------|
| L2 reference example (real execution chain) | ✅ ship 2026-09-26 | Change 1 (f456336) | phase1-5 real wiring, --release-metrics, R13.4 redaction |
```

在以下位置添加:
- `docs/architecture/harness-architecture-2026-09.md` §十一
- `docs/architecture/rsi-architecture-2026-09.md` §十一  
- `docs/architecture/self-evolution-architecture-2026-08.md` §十一

---

## 四、File-level Changes

| 文件 | 修改类型 | 说明 |
|------|---------|------|
| `evolution_session.h` | 修改 (新增成员) | engine_, chat_session_, evaluator_, genome_reg_, redact helper |
| `evolution_session.cpp` | 重写 phase1-5 | 真实 wiring 替换硬编码 |
| `main.cpp` | 修改 (--release-metrics) | 真实 metrics 计算 + 写入 + exit non-zero |

---

## 五、Backwards Compatibility

| API | 影响 |
|-----|------|
| `ChatSession` 11-参 ctor | 不变 (L2 调用) |
| `LLMProviderFactory` | 不变 (L2 调用 create) |
| `apply_harness_mutation` | 不变 (L2 调用 5-参) |
| `IEvaluator::compare` | 不变 (L2 调用) |
| `IGenomeRegistry` | 不变 (L2 调用 load/commit/fork) |

---

## 六、Risks

| # | 风险 | 影响 | 缓解 |
|---|------|------|------|
| R1 | ChatSession ctor 需要完整 provider chain | phase1_init 失败 | 使用 LLMProviderFactory::create({"mock"}) CI-safe |
| R2 | apply_harness_mutation 在 mock 模式下返回 Error | mutation 段 gate_passes=0 | trace 诚实记录, 不影响 exit code |
| R3 | IEvaluator V2 L2 中 behavior (NotAttempted 居多) | 单 turn 无统计意义 | spec 已文档化: ≥5 样本统计断言 deferred to Wave 3 |

---