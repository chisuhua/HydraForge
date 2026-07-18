// PDK Plugin entry — AgenticDSL v1 API (hydraforge namespace for PluginInfo, agenticdsl for IToolRegistry)
// 关联: openspec/changes/2026-07-17-pdk-chat-demo-buildable/

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#include <nlohmann/json.hpp>

#include <agenticdsl/contract/itool_registry.h>
#include <agenticdsl/plugin/plugin_info.h>

#include "session_agent.h"

namespace {

inline std::string str_arg(const std::unordered_map<std::string, std::string>& args,
                           const std::string& key, const std::string& default_val = "") {
    auto it = args.find(key);
    return (it != args.end()) ? it->second : default_val;
}

inline int int_arg(const std::unordered_map<std::string, std::string>& args,
                   const std::string& key, int default_val = 0) {
    auto it = args.find(key);
    if (it == args.end()) return default_val;
    try { return std::stoi(it->second); } catch (...) { return default_val; }
}

inline long long llong_arg(const std::unordered_map<std::string, std::string>& args,
                           const std::string& key, long long default_val = 0) {
    auto it = args.find(key);
    if (it == args.end()) return default_val;
    try { return std::stoll(it->second); } catch (...) { return default_val; }
}

inline nlohmann::json json_arg(const std::unordered_map<std::string, std::string>& args,
                                const std::string& key) {
    auto it = args.find(key);
    if (it == args.end()) return nlohmann::json();
    try {
        return nlohmann::json::parse(it->second);
    } catch (...) {
        return it->second;
    }
}

}  // namespace

// --- pdk_plugin_info ---
extern "C" const hydraforge::PluginInfo pdk_plugin_info = {
    hydraforge::CURRENT_ABI_VERSION,                              // abi_version = 2
    "infra.session",                                              // name[64]
    0, 1, 0,                                                      // semver major.minor.patch
    "Session Agent - session history/branch/compact/persist/search", // description[256]
    "session_history,session_branch,session_compact,session_persist,session_search", // capabilities[512]
    ""                                                            // dependencies[256]
};

// --- pdk_register_tools ---
extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry) {
    auto& store = pdk_session_agent::SessionStore::instance();

    // session/history
    registry.register_tool_function(
        "session/history",
        ::agenticdsl::ToolMetadata{
            .name = "session/history",
            .description = "Get session message history",
            .domain = "session",
            .category = ::agenticdsl::ToolCategory::ReadOnly,
            .min_layer = ::agenticdsl::LayerProfile::Workflow,
            .approval = ::agenticdsl::ApprovalPolicy{
                .requires_approval_in_plan = false,
                .requires_approval_in_agent = false,
                .requires_approval_in_yolo = false,
                .force_approval_always = false
            },
            .allowed_layers = {::agenticdsl::LayerProfile::Workflow}
        },
        [&store](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
            std::string session_id = str_arg(args, "session_id");
            if (session_id.empty()) {
                throw std::runtime_error("session_id is required");
            }
            const auto& s = store.get_or_create(session_id);
            nlohmann::json messages = nlohmann::json::array();
            for (const auto& m : s.messages) {
                nlohmann::json j;
                j["role"] = m.role;
                j["content"] = m.content;
                j["timestamp_ms"] = m.timestamp_ms;
                if (!m.meta.is_null()) j["meta"] = m.meta;
                messages.push_back(j);
            }
            return {{"session_id", session_id}, {"messages", messages}};
        }
    );

    // session/branch
    registry.register_tool_function(
"session/branch",
        ::agenticdsl::ToolMetadata{
            .name = "session/branch",
            .description = "Branch session",
            .domain = "session",
            .category = ::agenticdsl::ToolCategory::Execute,
            .min_layer = ::agenticdsl::LayerProfile::Workflow,
            .approval = ::agenticdsl::ApprovalPolicy{
                .requires_approval_in_plan = true,
                .requires_approval_in_agent = true,
                .requires_approval_in_yolo = false,
                .force_approval_always = false
            },
            .allowed_layers = {::agenticdsl::LayerProfile::Workflow}
        },
        [&store](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
            std::string src = str_arg(args, "session_id");
            size_t msg_idx = static_cast<size_t>(int_arg(args, "message_index", 0));
            if (src.empty()) {
                throw std::runtime_error("session_id is required");
            }
            std::string new_id = store.branch(src, msg_idx);
            return {{"new_session_id", new_id}};
        }
    );

    // session/compact
    registry.register_tool_function(
        "session/compact",
        ::agenticdsl::ToolMetadata{
            .name = "session/compact",
            .description = "Compact session history",
            .domain = "session",
            .category = ::agenticdsl::ToolCategory::Execute,
            .min_layer = ::agenticdsl::LayerProfile::Workflow,
            .approval = ::agenticdsl::ApprovalPolicy{
                .requires_approval_in_plan = true,
                .requires_approval_in_agent = true,
                .requires_approval_in_yolo = false,
                .force_approval_always = true
            },
            .allowed_layers = {::agenticdsl::LayerProfile::Workflow}
        },
        [&store](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
            std::string session_id = str_arg(args, "session_id");
            size_t keep = static_cast<size_t>(int_arg(args, "keep_recent", 10));
            if (session_id.empty()) {
                throw std::runtime_error("session_id is required");
            }
            auto compacted = store.compact(session_id, keep);
            nlohmann::json messages = nlohmann::json::array();
            for (const auto& m : compacted.messages) {
                messages.push_back({
                    {"role", m.role}, {"content", m.content},
                    {"timestamp_ms", m.timestamp_ms}
                });
            }
            return {{"session_id", session_id},
                    {"messages", messages},
                    {"compacted_count", compacted.messages.size()}};
        }
    );

    // session/persist
    registry.register_tool_function(
"session/persist",
        ::agenticdsl::ToolMetadata{
            .name = "session/persist",
            .description = "Persist session to JSONL",
            .domain = "session",
            .category = ::agenticdsl::ToolCategory::Execute,
            .min_layer = ::agenticdsl::LayerProfile::Workflow,
            .approval = ::agenticdsl::ApprovalPolicy{
                .requires_approval_in_plan = true,
                .requires_approval_in_agent = true,
                .requires_approval_in_yolo = false,
                .force_approval_always = true
            },
            .allowed_layers = {::agenticdsl::LayerProfile::Workflow}
        },
        [&store](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
            std::string session_id = str_arg(args, "session_id");
            if (session_id.empty()) {
                throw std::runtime_error("session_id is required");
            }
            // 如果 args 含 messages 则追加到 session
            nlohmann::json msgs_json = json_arg(args, "messages");
            long long last_timestamp_ms = 0;
            if (msgs_json.is_array()) {
                auto& s = store.get_or_create(session_id);
                for (const auto& m : msgs_json) {
                    pdk_session_agent::SessionMessage msg;
                    msg.role = m.value("role", "");
                    msg.content = m.value("content", "");
                    msg.timestamp_ms = m.value("timestamp_ms", 0LL);
                    if (m.contains("meta")) msg.meta = m["meta"];
                    last_timestamp_ms = msg.timestamp_ms;
                    s.messages.push_back(std::move(msg));
                }
                s.updated_at_ms = last_timestamp_ms;
            }
            bool ok = store.persist(session_id);
            return {{"ok", ok}, {"session_id", session_id}};
        }
    );

    // session/search
    registry.register_tool_function(
        "session/search",
        ::agenticdsl::ToolMetadata{
            .name = "session/search",
            .description = "Search session messages",
            .domain = "session",
            .category = ::agenticdsl::ToolCategory::ReadOnly,
            .min_layer = ::agenticdsl::LayerProfile::Workflow,
            .approval = ::agenticdsl::ApprovalPolicy{
                .requires_approval_in_plan = false,
                .requires_approval_in_agent = false,
                .requires_approval_in_yolo = false,
                .force_approval_always = false
            },
            .allowed_layers = {::agenticdsl::LayerProfile::Workflow}
        },
        [&store](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
            std::string session_id = str_arg(args, "session_id");
            std::string query = str_arg(args, "query");
            if (session_id.empty() || query.empty()) {
                throw std::runtime_error("session_id and query are required");
            }
            return {{"matches", store.search(session_id, query)}};
        }
    );
}