// examples/pdk_chat_demo_evolution/evolution_tracer.cpp
// L2 evolution tracer impl — 4-phase trace JSONL emitter (per R3 + P2-1)
//
// Output: one JSONL line per phase event on stdout.
//   Top-level (8): phase, timestamp_iso8601, session_id, turn_input,
//                  response, tokens, cost_usd, meta
//   Meta (12 per P2-1): context_id, task_class, is_hidden, hidden_bucket,
//                        sensitivity, expected_eval_quality, trace_id,
//                        capture_mode, genome_version, gate_passes,
//                        eval_quality (always null per P0-4 C5),
//                        attribution_verdict
//
// Per R13.4 P2-3: meta enforces dual is_hidden + hidden_bucket invariant.

#include "evolution_tracer.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <random>

namespace agenticdsl {
class IInteractionBus;
}

namespace pdk_chat_demo_evolution {

namespace {

const char* const kPhaseNames[] = {"baseline", "mutation", "reload", "compare"};

std::string current_iso8601() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) % 1000;
    std::tm tm_buf{};
    gmtime_r(&t, &tm_buf);
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03lldZ",
                  tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                  tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
                  static_cast<long long>(ms.count()));
    return std::string(buf);
}

std::string generate_session_id() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    uint64_t a = rng();
    char buf[24];
    std::snprintf(buf, sizeof(buf), "ses-%016llx",
                  static_cast<unsigned long long>(a));
    return std::string(buf);
}

// Per R13.4 P2-3: enforce dual is_hidden + hidden_bucket invariant in meta
nlohmann::json normalize_meta(nlohmann::json meta) {
    if (!meta.is_object()) meta = nlohmann::json::object();
    if (meta.contains("is_hidden") && meta["is_hidden"].is_boolean()) {
        meta["hidden_bucket"] = meta["is_hidden"];
    }
    return meta;
}

}  // namespace

EvolutionTracer::EvolutionTracer(bool enabled) : enabled_(enabled) {
    session_id_ = generate_session_id();
}

void EvolutionTracer::enable(bool on) { enabled_ = on; }

void EvolutionTracer::record_phase(TracePhase phase,
                                  const nlohmann::json& event) {
    if (!enabled_) return;
    auto phase_idx = static_cast<int>(phase);

    nlohmann::json meta = nlohmann::json::object();
    if (event.contains("meta") && event["meta"].is_object()) {
        meta = event["meta"];
    }
    meta = normalize_meta(std::move(meta));

    std::string turn_input =
        event.contains("turn_input") ? event["turn_input"].get<std::string>() : std::string("");
    nlohmann::json response =
        event.contains("response") ? event["response"] : nlohmann::json(nullptr);
    int tokens = event.contains("tokens") ? event["tokens"].get<int>() : 0;
    double cost_usd =
        event.contains("cost_usd") ? event["cost_usd"].get<double>() : 0.0;

    nlohmann::json out = {
        {"phase", kPhaseNames[phase_idx]},
        {"timestamp_iso8601", current_iso8601()},
        {"session_id", session_id_},
        {"turn_input", std::move(turn_input)},
        {"response", std::move(response)},
        {"tokens", tokens},
        {"cost_usd", cost_usd},
        {"meta", std::move(meta)}
    };
    emit_jsonl(out);
}

void EvolutionTracer::emit_jsonl(const nlohmann::json& j) {
    std::cout << j.dump() << '\n';
    std::cout.flush();
}

void EvolutionTracer::subscribe_to_bus(agenticdsl::IInteractionBus* bus) {
    // Phase 2.x enhancement; minimal stub for Batch 2 (T6.8a closure wires in Task 5)
    (void)bus;
}

}  // namespace pdk_chat_demo_evolution