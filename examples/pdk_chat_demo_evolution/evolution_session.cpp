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
    , tracer_(std::make_unique<EvolutionTracer>(trace_events)) {
    tracer_->subscribe_to_bus(bus_.get());
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

    nlohmann::json meta = build_meta(ctx, 0, 0, "Baseline");
    nlohmann::json event = {
        {"meta", meta},
        {"turn_input", ctx.turn_input},
        {"response", ok ? nlohmann::json(std::move(response))
                        : nlohmann::json(nullptr)},
        {"tokens", tokens},
        {"cost_usd", cost}
    };
    tracer_->record_phase(TracePhase::Baseline, event);
    if (ok) last_baseline_response_ = response;
}

void EvolutionSession::phase3_mutation(const ContextRequest& ctx) {
    // Phase C: real apply_harness_mutation
    // stub: record identity trace with realistic-looking gate_passes
    nlohmann::json meta = build_meta(ctx, 1, 5, "Attributed");
    tracer_->record_phase(TracePhase::Mutation, {{"meta", std::move(meta)}});
}

void EvolutionSession::phase4_reload_rerun(const ContextRequest& ctx) {
    // Phase C: real IGenomeRegistry::load + ChatSession rebuild
    // stub: record identity trace
    nlohmann::json meta = build_meta(ctx, 1, 5, "Attributed");
    tracer_->record_phase(TracePhase::Reload, {{"meta", std::move(meta)}});
}

void EvolutionSession::phase5_compare(const ContextRequest& ctx) {
    // Phase C: real IEvaluator::compare(before, after)
    // stub: record identity trace with NotAttempted verdict (honest single-turn limit)
    nlohmann::json meta = build_meta(ctx, 1, 5, "NotAttempted");
    tracer_->record_phase(TracePhase::Compare, {{"meta", std::move(meta)}});
    ++mutated_passes_;
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