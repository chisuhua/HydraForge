// examples/pdk_chat_demo_evolution/evolution_tracer.h
// L2 evolution tracer — 4-phase trace JSONL emitter (per R3 + P2-1)
//
// Schema contract:
//   Top-level (8 fields): phase, timestamp_iso8601, session_id, turn_input,
//                         response, tokens, cost_usd, meta
//   Meta (12 fields per P2-1): context_id, task_class, is_hidden, hidden_bucket,
//                              sensitivity, expected_eval_quality, trace_id,
//                              capture_mode, genome_version, gate_passes,
//                              eval_quality (always null per P0-4 C5),
//                              attribution_verdict
//
// Output: one JSONL line per phase event on stdout (consumed by main.cpp shell pipeline).

#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace agenticdsl {
class IInteractionBus;
}

namespace pdk_chat_demo_evolution {

enum class TracePhase { Baseline, Mutation, Reload, Compare };

class EvolutionTracer {
public:
    EvolutionTracer(bool enabled = false);
    void enable(bool on);
    void record_phase(TracePhase phase, const nlohmann::json& event);
    void subscribe_to_bus(agenticdsl::IInteractionBus* bus);
private:
    bool enabled_ = false;
    std::string session_id_;
    void emit_jsonl(const nlohmann::json& j);
};

}  // namespace pdk_chat_demo_evolution