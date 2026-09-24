// examples/pdk_chat_demo_evolution/evolution_session.h
// L2 EvolutionSession — 6-phase orchestrator (per plan Task 5)
//
// Phase 0-6 pipeline (Batch 2 minimal scope):
//   Phase 0: load_contexts (via load_context_file with bus*, T6.8a closure)
//   Phase 1: init (DSLEngine + bus wiring — Batch 4 full impl)
//   Phase 2: baseline (ChatSession.run)
//   Phase 3: mutation (apply_harness_mutation 5-param)
//   Phase 4: reload_rerun (ChatSession 11-param rebuild + load(genome@N))
//   Phase 5: compare (IEvaluator V2 BehavioralEquivalence)
//   Phase 6: emit_jsonl (finalize + 4 events per context via tracer)
//
// Per plan: P2-2 namespace policy, hermetic HOME, FilesystemGenomeRegistry.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "context_request.h"

namespace agenticdsl {
class DSLEngine;
class IInteractionBus;
class IToolRegistry;
}  // namespace agenticdsl

namespace pdk_chat_demo_evolution {

class EvolutionTracer;

namespace detail {
struct HermeticHomeGuard;
}  // namespace detail

class EvolutionSession {
public:
    EvolutionSession(const std::string& provider_str,
                     const std::string& capture_mode_str,
                     bool trace_events);
    ~EvolutionSession();

    EvolutionSession(const EvolutionSession&) = delete;
    EvolutionSession& operator=(const EvolutionSession&) = delete;

    int run_6_phase_demo();

    void set_contexts(std::vector<ContextRequest> contexts);
    void set_hermetic_home(detail::HermeticHomeGuard* g);

    agenticdsl::IInteractionBus* bus() const { return bus_.get(); }

private:
    void phase0_load_contexts();
    void phase1_init();
    void phase2_baseline(const ContextRequest& ctx);
    void phase3_mutation(const ContextRequest& ctx);
    void phase4_reload_rerun(const ContextRequest& ctx);
    void phase5_compare(const ContextRequest& ctx);
    void phase6_emit_jsonl();

    nlohmann::json build_meta(const ContextRequest& ctx,
                              int genome_version,
                              int gate_passes,
                              const std::string& verdict) const;

    std::vector<ContextRequest> contexts_;
    std::string provider_str_;
    std::string capture_mode_str_;

    std::shared_ptr<agenticdsl::IInteractionBus> bus_;
    std::unique_ptr<EvolutionTracer> tracer_;

    detail::HermeticHomeGuard* hermetic_guard_ = nullptr;
};

}  // namespace pdk_chat_demo_evolution