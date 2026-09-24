// examples/pdk_chat_demo_evolution/context_request.h
// L2 ContextRequest JSONL parser (per R13 spec + P0'-1/P0'-2/P0-6 detection)
//
// Schema (per R13.1 + spec/pdk-chat-demo-evolution/spec.md):
//   Top-level (6 fields):
//     - context_id (required, non-empty)
//     - turn_input (required, non-empty)
//     - task_class (required, enum)
//     - expected_eval_quality (optional)
//     - invocation_mode (optional, default "mock")
//     - metadata (optional object, 4 sub-fields)
//
// 4 validation gates:
//   G1: context_id + turn_input presence + non-empty
//   G2: turn_input content checks (P0'-2 hint regex / P0-6 network keyword)
//   G3: task_class prefix-rejection (P0'-1: mutation_metric_* BEFORE enum check)
//   G4: task_class + invocation_mode closed-enum validation
//
// 3 prefix/keyword rejections emit ADR-0068 v2.4 events:
//   - mutation_metric_rejected (R9.2 prefix-rejection)
//   - hint_containment_rejected (R9.1 parser-side regex)
//   - turn_input_network_keyword_rejected (R9.3 keyword-rejection, defer Wave 4 sandbox)
//   - hidden_context_accepted_info (R13.4 is_hidden=true accepted, MUST)

#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace pdk_chat_demo_evolution {

struct ContextRequest {
    std::string context_id;
    std::string turn_input;
    std::string task_class;
    std::optional<std::string> expected_eval_quality;
    std::string invocation_mode = "mock";

    struct Metadata {
        std::string domain;
        std::vector<std::string> tags;
        bool is_hidden = false;
        std::string sensitivity = "public";
    } metadata;
};

enum class LoadResult {
    Ok,
    MissingContextFile,
    MissingField,
    EmptyTurnInput,
    InvalidTaskClass,
    InvalidInvocationMode,
    PrefixRejected,           // R9.2 prefix (mutation_metric_*)
    HintContained,            // R9.1 parser-side regex
    NetworkKeywordContained,  // R9.3 keyword
};

struct LoadError {
    LoadResult kind;
    std::string message;
    size_t line_number = 0;
    std::string context_id;
};

// Load JSONL file line-by-line; return accepted ContextRequests + structured errors.
// Per R13: 3 prefix/keyword rejections run BEFORE closed-enum validation (P0'-1 invariant).
std::vector<ContextRequest> load_context_file(const std::string& path,
                                              std::vector<LoadError>& errors);

// Convert a ContextRequest into the trace JSONL meta field (8 top + meta fields per R3).
// Per R13.4 P2-3: emits dual is_hidden + hidden_bucket fields for hidden contexts.
nlohmann::json to_trace_meta(const ContextRequest& req);

}  // namespace pdk_chat_demo_evolution