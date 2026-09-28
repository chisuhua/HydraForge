// examples/pdk_chat_demo_evolution/evolution_session.cpp
// L2 EvolutionSession — 6-phase orchestrator impl
//
// Phase B (2026-09-26): phase1_init + phase2_baseline real wiring per
// Oracle audit ses_f259746c (Path 1 Change 2).  phase1_init now creates
// real DSLEngine + ChatSession, phase2_baseline calls real chat().
// phase3-5 remain identity stubs until Phase C.

#include "evolution_session.h"

#include <agenticdsl/contract/inmemory_bus.h>
#include <agenticdsl/contract/itool_registry.h>
#include <core/engine.h>

#include <iostream>
#include <filesystem>
#include <utility>

#include "context_request.h"
#include "evolution_tracer.h"
#include "hermetic_home.h"

namespace pdk_chat_demo_evolution {

EvolutionSession::EvolutionSession(const std::string& provider_str,
                                     const std::string& capture_mode_str,
                                     bool trace_events)
    : provider_str_(provider_str)
    , capture_mode_str_(capture_mode_str)
    , bus_(std::make_shared<agenticdsl::InMemoryBus>())
    , tracer_(std::make_unique<EvolutionTracer>(trace_events))
    , evaluator_(std::make_unique<agenticdsl::BehavioralEquivalenceEvaluator>())
    , budget_(std::make_unique<agenticdsl::BudgetController>()) {
    tracer_->subscribe_to_bus(bus_.get());
    bootstrap_attribution_.verdict = agenticdsl::evolution::AttributionVerdict::Attributed;
    bootstrap_attribution_.method = agenticdsl::evolution::AttributionMethod::DirectComparison;
    bootstrap_attribution_.reason =
        "L2 bootstrap: first-cycle, no prior attribution";
}

EvolutionSession::~EvolutionSession() = default;

void EvolutionSession::set_contexts(std::vector<ContextRequest> contexts) {
    contexts_ = std::move(contexts);
}

void EvolutionSession::set_hermetic_home(detail::HermeticHomeGuard* g) {
    hermetic_guard_ = g;
}

int EvolutionSession::run_6_phase_demo() {
    if (contexts_.empty()) {
        std::cerr << "ERROR: L2 zero-hardcode, must provide ContextRequest via "
                     "--context-file" << std::endl;
        return 1;
    }
    phase0_load_contexts();
    phase1_init();
    for (const auto& ctx : contexts_) {
        phase2_baseline(ctx);
        phase3_mutation(ctx);
        phase4_reload_rerun(ctx);
        phase5_compare(ctx);
    }
    phase6_emit_jsonl();
    return 0;
}

void EvolutionSession::phase0_load_contexts() {
    // contexts_ already populated via set_contexts
}

void EvolutionSession::phase1_init() {
    // Phase B: real DSLEngine + ChatSession wiring
    engine_ = std::make_unique<agenticdsl::DSLEngine>();
    engine_->set_interaction_bus(bus_);

    // Register mock "loop/run" tool for ChatSession to work without LoopAgent .so
    agenticdsl::IToolRegistry& reg = engine_->get_tool_registry();
    reg.register_tool_function("loop/run",
        agenticdsl::ToolMetadata{
            .name = "loop/run",
            .description = "mock loop/run for evolution_session (Phase B)",
            .domain = "loop",
            .category = agenticdsl::ToolCategory::Execute,
            .min_layer = agenticdsl::LayerProfile::Workflow,
            .approval = agenticdsl::ApprovalPolicy{
                .requires_approval_in_plan = false,
                .requires_approval_in_agent = true,
                .requires_approval_in_yolo = false,
                .force_approval_always = false},
            .allowed_layers = {agenticdsl::LayerProfile::Workflow}
        },
        [](const std::unordered_map<std::string, std::string>&) -> nlohmann::json {
            return nlohmann::json{
                {"ok", true},
                {"success", true},
                {"response", "Mock evolution response"},
                {"steps", 1},
                {"tokens_used", 1},
                {"cost_usd", 0.0}};
        });

    agent_cfg_.provider = provider_str_.empty() ? "mock" : provider_str_;
    agent_cfg_.model = "test";
    agent_cfg_.system_prompt = "You are an L2 evolution evaluator.";

    // Create ChatSession with 11-param ctor (mock mode, no persistence)
    using hydraforge::pdk::ChatSession;
    using hydraforge::pdk::SessionConfig;
    chat_session_ = std::make_unique<ChatSession>(
        engine_.get(), bus_, &engine_->get_tool_registry(),
        agent_cfg_, SessionConfig{},
        nullptr,  // cancellation_registry
        nullptr,  // timer (D9 lazy: nullptr = no periodic wake-up)
        nullptr,  // input (default StdinInputSource — not used in programmatic mode)
        nullptr,  // logger (default StderrLogger)
        nullptr,  // session_manager (no JSONL persistence)
        std::nullopt  // resume (fresh session)
    );

    // T2: Genome registry at hermetic root (Gate 0 prerequisite). Per design D3,
    // create_filesystem(root) takes an explicit root path (NOT ~/.hydraforge/...);
    // HOME was redirected by setup_hermetic_home() before phase1_init.
    if (hermetic_guard_) {
        genome_registry_ = agenticdsl::genome::IGenomeRegistry::create_filesystem(
            hermetic_guard_->genome_dir);
    } else {
        genome_registry_ = agenticdsl::genome::IGenomeRegistry::create_filesystem(
            std::filesystem::temp_directory_path() / "l2-evolution-genome");
    }

    // Snapshot tool names from registry (used as Genome::spec.tools + Gate 3 final_tools)
    tool_names_snapshot_ = engine_->get_tool_registry().list_tools();

    // Seed genome commit (Gate 0: parent_version must be > 0 before first mutation)
    agenticdsl::genome::Genome root;
    root.metadata.name = "default";
    root.metadata.capture_mode = "mock";
    root.spec.harness = agent_cfg_.system_prompt;
    root.spec.tools = tool_names_snapshot_;
    auto seed = genome_registry_->commit(root);
    if (seed.has_value()) {
        last_committed_genome_version_ = seed.value().version;
        baseline_version_ = last_committed_genome_version_;
    } else {
        std::cerr << "ERROR: genome seed commit failed (mutation will fail Gate 0)"
                  << std::endl;
    }
}

void EvolutionSession::phase2_baseline(const ContextRequest& ctx) {
    ++baseline_total_;
    std::string response;
    int tokens = 0;
    double cost = 0.0;
    bool ok = false;

    if (chat_session_) {
        auto result = chat_session_->chat(ctx.turn_input);
        ok = result.success;
        if (!ok) ++baseline_failures_;
        response = result.response;
        tokens = result.total_tokens;
        cost = result.cost_usd;
    }

    // baseline phase is pre-attribution → NotAttempted per spec R4 enum
    nlohmann::json meta = build_meta(ctx, 0, 0, "NotAttempted");
    nlohmann::json event = {
        {"meta", meta},
        {"turn_input", ctx.turn_input},
        {"response", ok ? nlohmann::json(std::move(response))
                        : nlohmann::json(nullptr)},
        {"tokens", tokens},
        {"cost_usd", cost}
    };
    // R13.4: redact turn_input/response per sensitivity before emit
    if (!ctx.metadata.sensitivity.empty() &&
        (ctx.metadata.sensitivity == "internal" ||
         ctx.metadata.sensitivity == "confidential")) {
        event = detail::redact_trace_fields(std::move(event), ctx.metadata.sensitivity);
    }
    tracer_->record_phase(TracePhase::Baseline, event);
    if (ok) last_baseline_response_ = response;

    // Populate last_baseline_exec_ for Phase 5 compare
    agenticdsl::ToolResult tr;
    tr.ok = ok;
    tr.data = {{"response", response}, {"tokens", tokens}, {"cost_usd", cost}};
    std::string trace_id = "baseline-" + ctx.context_id;
    last_baseline_exec_ = agenticdsl::ExecutionTrace{std::move(tr), trace_id, {}};
}

void EvolutionSession::phase3_mutation(const ContextRequest& ctx) {
    if (!genome_registry_) {
        nlohmann::json meta = build_meta(ctx, 0, 0, "NotAttempted");
        tracer_->record_phase(TracePhase::Mutation, {{"meta", std::move(meta)}});
        return;
    }

    agenticdsl::evolution::MutationGateContext mctx;
    mctx.current = agenticdsl::evolution::EvolutionState::Harness;
    mctx.attribution = &bootstrap_attribution_;
    mctx.evaluator = evaluator_.get();
    mctx.budget = budget_.get();
    mctx.bus = bus_.get();
    mctx.policy = {};
    mctx.genome_registry = genome_registry_.get();
    mctx.genome_name = "default";
    mctx.parent_version = last_committed_genome_version_;
    mctx.trace_id = "l2-mutation-" + ctx.context_id;

    // Minimal honest test mutation: prompt_delta only (no tools add/remove → avoids
    // Gate 2.5 RegistryRejected on nonexistent tools).
    agenticdsl::evolution::GenomeMutations mutations;
    mutations.prompt_delta = "\n\n# Mutation: harness refinement from context "
                             + ctx.context_id;

    std::string sys_prompt = agent_cfg_.system_prompt;
    auto result = agenticdsl::evolution::apply_harness_mutation(
        mutations, sys_prompt, tool_names_snapshot_,
        engine_->get_tool_registry(), mctx);

    uint64_t new_version = last_committed_genome_version_;
    int gates_passed = 0;
    std::string verdict = "NotAttempted";
    if (result.has_value()) {
        new_version = result.value().committed_genome_version;
        gates_passed = 5;
        last_committed_genome_version_ = new_version;
        agent_cfg_.system_prompt = sys_prompt;
    } else {
        switch (result.error()) {
            case agenticdsl::evolution::MutationError::InvalidMutation:
                gates_passed = 0; break;
            case agenticdsl::evolution::MutationError::NotReady:
                gates_passed = 1; break;
            case agenticdsl::evolution::MutationError::GovernanceDenied:
                gates_passed = 2; break;
            case agenticdsl::evolution::MutationError::RegistryRejected:
                gates_passed = 3; break;
            case agenticdsl::evolution::MutationError::UnsupportedVariant:
                gates_passed = 0; break;
        }
    }

    nlohmann::json meta = build_meta(ctx, new_version, gates_passed, verdict);
    tracer_->record_phase(TracePhase::Mutation, {{"meta", std::move(meta)}});
}

void EvolutionSession::phase4_reload_rerun(const ContextRequest& ctx) {
    // Skip rebuild when no mutation actually applied (design D4 Risk 3 mitigation)
    if (last_committed_genome_version_ <= baseline_version_ || !genome_registry_) {
        nlohmann::json meta = build_meta(
            ctx, last_committed_genome_version_, /*gates*/0,
            /*verdict*/ "NotAttempted");
        tracer_->record_phase(TracePhase::Reload, {{"meta", std::move(meta)}});
        return;
    }

    auto genome_result = genome_registry_->load(
        "default", last_committed_genome_version_);
    if (!genome_result.has_value()) {
        nlohmann::json meta = build_meta(
            ctx, last_committed_genome_version_, /*gates*/3,
            /*verdict*/ "NotAttempted");
        meta["error"] = "load_failed";
        tracer_->record_phase(TracePhase::Reload, {{"meta", std::move(meta)}});
        return;
    }

    // Hand-mapped AgentConfig from Genome (Oracle M4: only system_prompt matches
    // AgentConfig; tools/budget/model_routing don't exist there → dropped)
    auto new_agent_cfg = agent_cfg_;
    new_agent_cfg.system_prompt = genome_result.value().spec.harness;

    // Build new ChatSession with mutated config
    auto session_v2 = std::make_unique<hydraforge::pdk::ChatSession>(
        engine_.get(), bus_, &engine_->get_tool_registry(),
        new_agent_cfg, session_cfg_,
        nullptr, nullptr, nullptr, nullptr, nullptr, std::nullopt);

    auto result = session_v2->chat(ctx.turn_input);

    nlohmann::json rerun_event = {
        {"meta", build_meta(ctx, last_committed_genome_version_,
                            /*gates*/5, "NotAttempted")},
        {"turn_input", ctx.turn_input},
        {"response", result.success ? nlohmann::json(result.response)
                                    : nlohmann::json(nullptr)},
        {"tokens", result.total_tokens},
        {"cost_usd", result.cost_usd}
    };

    // R13.4: redact turn_input/response per sensitivity
    if (ctx.metadata.sensitivity == "internal" ||
        ctx.metadata.sensitivity == "confidential") {
        rerun_event = detail::redact_trace_fields(
            std::move(rerun_event), ctx.metadata.sensitivity);
    }

    // Convert ChatResult → ExecutionTrace for phase5 compare
    agenticdsl::ToolResult tr;
    tr.ok = result.success;
    tr.data = {{"response", result.response}, {"tokens", result.total_tokens},
               {"cost_usd", result.cost_usd}};
    last_rerun_exec_ = agenticdsl::ExecutionTrace{
        std::move(tr), "rerun-" + ctx.context_id, {}};

    tracer_->record_phase(TracePhase::Reload, rerun_event);
}

void EvolutionSession::phase5_compare(const ContextRequest& ctx) {
    // Phase C (finalization): real IEvaluator::compare(before, after)
    // Per design.md D2: skip with NotAttempted when trace evidence missing
    //
    // Note on genome_version (per Oracle SHIP-with-fixes verdict 2026-09-28):
    //   last_committed_genome_version_ is init=0 in T1 (no seed commit yet).
    //   phase3_mutation + phase4_reload_rerun still hardcode `1` in build_meta
    //   (Phase B stubs; T2 will wire real apply_harness_mutation + seed commit
    //   per design.md D3, and T3.1 will add chain-link semantics per Oracle M6).
    //   The 0-vs-1 split is the designed intermediate state of T1/T2/T3
    //   decomposition; phase5's `0` is semantically honest ("no mutation
    //   committed yet"). Self-resolves at T2/T3 wire.
    if (!last_baseline_exec_.has_value() || !last_rerun_exec_.has_value()) {
        nlohmann::json meta = build_meta(
            ctx, last_committed_genome_version_, /*gates*/0,
            /*verdict*/ "NotAttempted");
        tracer_->record_phase(TracePhase::Compare, {{"meta", std::move(meta)}});
        return;
    }

    int cmp = evaluator_->compare(*last_baseline_exec_, *last_rerun_exec_);
    // Verdict mapping per Oracle decision (design.md D2):
    //   compare == 0  → "Attributed" (behaviors equivalent)
    //   compare != 0  → "Insufficient" (regression detected or inconclusive)
    // Confounded is unreachable in L2 single-turn scope (ADR-0086 v1.1)
    std::string verdict = (cmp == 0) ? "Attributed" : "Insufficient";

    if (cmp == 0) ++mutated_passes_;
    else ++mutated_failures_;

    nlohmann::json meta = build_meta(
        ctx, last_committed_genome_version_, /*gates*/5, verdict);
    tracer_->record_phase(TracePhase::Compare, {{"meta", std::move(meta)}});
}

void EvolutionSession::phase6_emit_jsonl() {
    // Per-context traces already flushed by tracer as they are emitted.
    // Future: write metrics.json here if --release-metrics.
}

nlohmann::json EvolutionSession::build_meta(const ContextRequest& ctx,
                                            int genome_version,
                                            int gate_passes,
                                            const std::string& verdict) const {
    return nlohmann::json{
        {"context_id", ctx.context_id},
        {"task_class", ctx.task_class},
        {"is_hidden", ctx.metadata.is_hidden},
        {"hidden_bucket", ctx.metadata.is_hidden},
        {"sensitivity", ctx.metadata.sensitivity},
        {"expected_eval_quality", ctx.expected_eval_quality.value_or("")},
        {"trace_id", generate_uuid_v4()},
        {"capture_mode", capture_mode_str_},
        {"genome_version", genome_version},
        {"gate_passes", gate_passes},
        {"eval_quality", nullptr},
        {"attribution_verdict", verdict}
    };
}

namespace detail {

nlohmann::json redact_trace_fields(nlohmann::json event, const std::string& sensitivity) {
    if (sensitivity.empty() || sensitivity == "none" || sensitivity == "public") {
        return event;
    }
    std::string level = (sensitivity == "confidential") ? "confidential" : "internal";
    if (event.contains("turn_input") && event["turn_input"].is_string()) {
        event["turn_input"] = "[REDACTED-" + level + "]";
    }
    if (event.contains("response") && !event["response"].is_null()) {
        event["response"] = "[REDACTED-" + level + "]";
    }
    if (sensitivity == "confidential" && event.contains("meta") && event["meta"].is_object()) {
        auto& m = event["meta"];
        if (m.contains("tags")) {
            m["tags"] = "[REDACTED-confidential]";
        }
        if (m.contains("domain")) {
            m["domain"] = "[REDACTED-confidential]";
        }
    }
    return event;
}

}  // namespace detail

}  // namespace pdk_chat_demo_evolution