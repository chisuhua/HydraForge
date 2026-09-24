// examples/pdk_chat_demo_evolution/context_request.cpp
// L2 ContextRequest JSONL parser impl (per R13 spec + P0'-1/P0'-2/P0-6 detection)
//
// Detection order (per P0'-1 invariant: prefix-checks-before-enum):
//   1. context_id presence (MissingField S29)
//   2. turn_input presence (MissingField)
//   3. turn_input empty (EmptyTurnInput S30)
//   4. turn_input hint regex (HintContained R9.1 → emit hint_containment_rejected)
//   5. turn_input network keyword (NetworkKeywordContained R9.3 → emit turn_input_network_keyword_rejected)
//   6. task_class presence (MissingField)
//   7. task_class prefix check (PrefixRejected R9.2 → emit mutation_metric_rejected)
//   8. task_class closed enum (InvalidTaskClass S31)
//   9. invocation_mode closed enum (InvalidInvocationMode)
//
// T6.8a closure gate: 4 ADR-0068 v2.4 events emitted via IInteractionBus when
// non-null.  See tasks.md T6.8a design_deviation block for full fix history.

#include "context_request.h"

#include <agenticdsl/contract/event_builder.h>
#include <agenticdsl/contract/iinteraction_bus.h>

#include <algorithm>
#include <fstream>
#include <random>
#include <regex>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace pdk_chat_demo_evolution {

namespace {

const std::vector<std::string> RervedPrefixes = {"mutation_metric_"};

const std::regex HINT_PATTERN(R"(the answer is \w+)");

const std::regex NETWORK_KEYWORD(R"(fetch http://)");

const std::set<std::string> ValidTaskClasses = {
    "code_gen", "research", "summary", "debug", "classify", "other"
};

const std::set<std::string> ValidInvocationModes = {
    "mock", "real_llm_deepseek", "real_llm_custom"
};

std::string generate_uuid_v4() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    uint64_t a = rng();
    uint64_t b = rng();
    a = (a & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    b = (b & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;
    char buf[37];
    std::snprintf(buf, sizeof(buf), "%08x-%04x-%04x-%04x-%012llx",
                  static_cast<uint32_t>(a >> 32),
                  static_cast<uint16_t>((a >> 16) & 0xFFFF),
                  static_cast<uint16_t>(a & 0xFFFF),
                  static_cast<uint16_t>((b >> 48) & 0xFFFF),
                  static_cast<unsigned long long>(b & 0xFFFFFFFFFFFFULL));
    return std::string(buf);
}

std::string truncate_preview(const std::string& s, size_t n) {
    return s.size() <= n ? s : s.substr(0, n);
}

void emit_l2_event(agenticdsl::IInteractionBus* bus,
                   const std::string& topic,
                   const nlohmann::json& args) {
    if (!bus) return;
    bus->emit(agenticdsl::EventBuilder(topic).args(args).build());
}

}  // namespace

std::vector<ContextRequest> load_context_file(const std::string& path,
                                              std::vector<LoadError>& errors,
                                              agenticdsl::IInteractionBus* bus) {
    std::vector<ContextRequest> out;
    std::ifstream f(path);
    if (!f) {
        errors.push_back({LoadResult::MissingContextFile,
                          "cannot open " + path, 0, ""});
        return out;
    }

    std::string line;
    size_t line_no = 0;
    while (std::getline(f, line)) {
        ++line_no;
        if (line.empty()) continue;

        if (line_no == 1 && line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF) {
            line = line.substr(3);
        }

        auto j = nlohmann::json::parse(line, nullptr, false);
        if (j.is_discarded()) {
            errors.push_back({LoadResult::MissingField, "invalid JSON", line_no, ""});
            continue;
        }

        ContextRequest r;

        // 1. context_id (S29)
        if (!j.contains("context_id") || !j["context_id"].is_string() ||
            j["context_id"].get<std::string>().empty()) {
            errors.push_back({LoadResult::MissingField, "context_id", line_no, ""});
            continue;
        }
        r.context_id = j["context_id"];

        // 2 + 3. turn_input presence + empty (S30)
        if (!j.contains("turn_input") || !j["turn_input"].is_string()) {
            errors.push_back({LoadResult::MissingField, "turn_input", line_no, r.context_id});
            continue;
        }
        r.turn_input = j["turn_input"];
        if (r.turn_input.empty()) {
            errors.push_back({LoadResult::EmptyTurnInput,
                              "turn_input empty", line_no, r.context_id});
            continue;
        }

        // 4. P0'-2 hint regex (R9.1) + emit hint_containment_rejected
        if (std::regex_search(r.turn_input, HINT_PATTERN)) {
            emit_l2_event(bus, "hint_containment_rejected", {
                {"context_id", r.context_id},
                {"turn_input_preview", truncate_preview(r.turn_input, 100)},
                {"matched_pattern", "the_answer_is_word"}
            });
            errors.push_back({LoadResult::HintContained,
                              "hint pattern detected", line_no, r.context_id});
            continue;
        }

        // 5. P0-6 network keyword (R9.3) + emit turn_input_network_keyword_rejected
        if (std::regex_search(r.turn_input, NETWORK_KEYWORD)) {
            emit_l2_event(bus, "turn_input_network_keyword_rejected", {
                {"context_id", r.context_id},
                {"turn_input_preview", truncate_preview(r.turn_input, 100)},
                {"keyword", "fetch_http"}
            });
            errors.push_back({LoadResult::NetworkKeywordContained,
                              "network keyword detected", line_no, r.context_id});
            continue;
        }

        // 6. task_class presence
        if (!j.contains("task_class") || !j["task_class"].is_string()) {
            errors.push_back({LoadResult::MissingField, "task_class", line_no, r.context_id});
            continue;
        }
        r.task_class = j["task_class"];

        // 7. P0'-1 prefix-rejection BEFORE enum (R9.2) + emit mutation_metric_rejected
        bool is_prefix_reserved = false;
        for (const auto& p : RervedPrefixes) {
            if (r.task_class.compare(0, p.size(), p) == 0) {
                is_prefix_reserved = true;
                emit_l2_event(bus, "mutation_metric_rejected", {
                    {"context_id", r.context_id},
                    {"task_class_preview", r.task_class},
                    {"reason", std::string("R9.2 prefix-rejection (reserved: ") + p + ")"}
                });
                errors.push_back({LoadResult::PrefixRejected,
                                  "reserved prefix: " + p, line_no, r.context_id});
                break;
            }
        }
        if (is_prefix_reserved) continue;

        // 8. task_class closed enum (S31)
        if (ValidTaskClasses.find(r.task_class) == ValidTaskClasses.end()) {
            errors.push_back({LoadResult::InvalidTaskClass,
                              r.task_class, line_no, r.context_id});
            continue;
        }

        // 9. invocation_mode (optional)
        if (j.contains("invocation_mode") && j["invocation_mode"].is_string()) {
            r.invocation_mode = j["invocation_mode"];
            if (ValidInvocationModes.find(r.invocation_mode) ==
                ValidInvocationModes.end()) {
                errors.push_back({LoadResult::InvalidInvocationMode,
                                  r.invocation_mode, line_no, r.context_id});
                continue;
            }
        }

        if (j.contains("expected_eval_quality") &&
            j["expected_eval_quality"].is_string()) {
            r.expected_eval_quality = j["expected_eval_quality"];
        }

        if (j.contains("metadata") && j["metadata"].is_object()) {
            const auto& m = j["metadata"];
            if (m.contains("domain") && m["domain"].is_string()) {
                r.metadata.domain = m["domain"];
            }
            if (m.contains("tags") && m["tags"].is_array()) {
                for (const auto& t : m["tags"]) {
                    if (t.is_string()) r.metadata.tags.push_back(t);
                }
            }
            if (m.contains("is_hidden") && m["is_hidden"].is_boolean()) {
                r.metadata.is_hidden = m["is_hidden"];
            }
            if (m.contains("sensitivity") && m["sensitivity"].is_string()) {
                r.metadata.sensitivity = m["sensitivity"];
            }

            // R13.4 (P0 + P2-3 MUST): is_hidden=true → accept + emit hidden_context_accepted_info
            if (r.metadata.is_hidden) {
                emit_l2_event(bus, "hidden_context_accepted_info", {
                    {"context_id", r.context_id},
                    {"task_class", r.task_class},
                    {"is_hidden", true},
                    {"bucket", "hidden"}
                });
            }
        }

        out.push_back(std::move(r));
    }
    return out;
}

nlohmann::json to_trace_meta(const ContextRequest& req) {
    return nlohmann::json{
        {"context_id", req.context_id},
        {"task_class", req.task_class},
        {"is_hidden", req.metadata.is_hidden},
        {"hidden_bucket", req.metadata.is_hidden},
        {"sensitivity", req.metadata.sensitivity},
        {"expected_eval_quality", req.expected_eval_quality.value_or("")},
        {"trace_id", generate_uuid_v4()},
        {"capture_mode", "None"}
    };
}

}  // namespace pdk_chat_demo_evolution