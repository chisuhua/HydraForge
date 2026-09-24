// examples/pdk_chat_demo_evolution/evolution_session.cpp
// L2 EvolutionSession — 6-phase orchestrator impl (per plan Task 5)
//
// Batch 2 minimal scope: bus injection (T6.8a closure gate), tracer wiring,
// hermetic HOME.  ChatSession + apply_harness_mutation + IEvaluator wiring
// deferred to Batch 4 (Task 10 main.cpp + run_evolution_demo.sh).

#include "evolution_session.h"

#include <agenticdsl/contract/inmemory_bus.h>

#include <iostream>
#include <utility>

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
    // contexts_ already populated via set_contexts (set by main.cpp after
    // load_context_file with bus*).  Future: re-load via load_context_file
    // here for self-contained session mode.
}

void EvolutionSession::phase1_init() {
    // Batch 4 (Task 10): construct DSLEngine, set_llm_provider, get_tool_registry,
    // wire CancellationRegistry, PluginLoader for IDistillationWriter, etc.
}

void EvolutionSession::phase2_baseline(const ContextRequest& ctx) {
    nlohmann::json event = {{"meta", build_meta(ctx, 0, 0, "Baseline")}};
    tracer_->record_phase(TracePhase::Baseline, event);
}

void EvolutionSession::phase3_mutation(const ContextRequest& ctx) {
    nlohmann::json event = {{"meta", build_meta(ctx, 1, 5, "Attributed")}};
    tracer_->record_phase(TracePhase::Mutation, event);
}

void EvolutionSession::phase4_reload_rerun(const ContextRequest& ctx) {
    nlohmann::json event = {{"meta", build_meta(ctx, 1, 5, "Attributed")}};
    tracer_->record_phase(TracePhase::Reload, event);
}

void EvolutionSession::phase5_compare(const ContextRequest& ctx) {
    nlohmann::json event = {{"meta", build_meta(ctx, 1, 5, "Attributed")}};
    tracer_->record_phase(TracePhase::Compare, event);
}

void EvolutionSession::phase6_emit_jsonl() {
    // Per-context traces flushed by tracer as they are emitted.
    // Batch 4 (Task 10): also write to file via IDistillationWriter when
    // capture_mode_str_ == "Training".
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
        {"trace_id", std::string("trace-") + ctx.context_id},
        {"capture_mode", capture_mode_str_},
        {"genome_version", genome_version},
        {"gate_passes", gate_passes},
        {"eval_quality", nullptr},
        {"attribution_verdict", verdict}
    };
}

}  // namespace pdk_chat_demo_evolution