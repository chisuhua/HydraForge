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
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <agenticdsl/contract/ievaluator.h>
#include <agenticdsl/types/execution_trace.h>
#include <agenticdsl/cognitive/behavioral_equivalence_evaluator.h>
#include <agenticdsl/pdk/chat_session.h>
#include <agenticdsl/genome/genome.h>
#include <agenticdsl/evolution/harness_rsi.h>
#include <agenticdsl/types/attribution_record.h>
#include <modules/budget/budget_controller.h>

#include "context_request.h"

// Forward declarations for types used as unique_ptr members
namespace agenticdsl {
class DSLEngine;
}  // namespace agenticdsl

namespace pdk_chat_demo_evolution {

class EvolutionTracer;

namespace detail {
struct HermeticHomeGuard;

// L2 internal helper: redact trace fields per sensitivity level (R13.4)
nlohmann::json redact_trace_fields(nlohmann::json event, const std::string& sensitivity);
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

    // --release-metrics result accessor (for main.cpp)
    int baseline_total() const { return baseline_total_; }
    int baseline_failures() const { return baseline_failures_; }
    int mutated_passes() const { return mutated_passes_; }
    int mutated_failures() const { return mutated_failures_; }

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

    // Phase B: real wiring members
    std::unique_ptr<agenticdsl::DSLEngine> engine_;
    std::unique_ptr<hydraforge::pdk::ChatSession> chat_session_;
    hydraforge::pdk::AgentConfig agent_cfg_;
    std::string last_baseline_response_;

    // --release-metrics counters
    int baseline_total_ = 0;
    int baseline_failures_ = 0;
    int mutated_passes_ = 0;
    int mutated_failures_ = 0;

    // Phase C (finalization): compare members
    std::optional<agenticdsl::ExecutionTrace> last_baseline_exec_;
    std::optional<agenticdsl::ExecutionTrace> last_rerun_exec_;
    std::unique_ptr<agenticdsl::IEvaluator> evaluator_;
    uint64_t last_committed_genome_version_ = 0;

    // Phase C (finalization, T2): mutation wiring members
    std::unique_ptr<agenticdsl::genome::IGenomeRegistry> genome_registry_;
    agenticdsl::evolution::AttributionRecord bootstrap_attribution_;
    std::unique_ptr<agenticdsl::IBudgetController> budget_;
    std::vector<std::string> tool_names_snapshot_;
};

}  // namespace pdk_chat_demo_evolution