// PDK Plugin entry — AgenticDSL v1 API (hydraforge namespace for PluginInfo, agenticdsl for IToolRegistry)
// 关联: openspec/changes/2026-07-17-pdk-chat-demo-buildable/

#include <stdexcept>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include <agenticdsl/contract/itool_registry.h>
#include <agenticdsl/plugin/plugin_info.h>

#include "budget_agent.h"

namespace {

inline std::string str_arg(const std::unordered_map<std::string, std::string>& args,
                           const std::string& key, const std::string& default_val = "") {
    auto it = args.find(key);
    return (it != args.end()) ? it->second : default_val;
}

inline double dbl_arg(const std::unordered_map<std::string, std::string>& args,
                      const std::string& key, double default_val = 0.0) {
    auto it = args.find(key);
    if (it == args.end()) return default_val;
    try { return std::stod(it->second); } catch (...) { return default_val; }
}

}  // namespace

// --- pdk_plugin_info ---
extern "C" const hydraforge::PluginInfo pdk_plugin_info = {
    hydraforge::CURRENT_ABI_VERSION,                              // abi_version = 2
    "infra.budget",                                               // name[64]
    0, 1, 0,                                                      // semver major.minor.patch
    "Budget Agent - budget query/set_limit/alerts/cost_breakdown", // description[256]
    "budget_query,budget_set_limit,budget_alerts,budget_cost_breakdown", // capabilities[512]
    ""                                                            // dependencies[256]
};

// --- pdk_register_tools ---
extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry) {
    auto& store = pdk_budget_agent::BudgetStore::instance();

    // budget/query
    registry.register_tool_function(
        "budget/query",
        ::agenticdsl::ToolMetadata{
            .name = "budget/query",
            .description = "Query budget status",
            .domain = "budget",
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
        [&store](const std::unordered_map<std::string, std::string>& /*args*/) -> nlohmann::json {
            return {
                {"limit_usd", store.limit_usd()},
                {"spent_usd", store.spent_usd()},
                {"remaining_usd", store.remaining_usd()}
            };
        }
    );

    // budget/set_limit
    registry.register_tool_function(
        "budget/set_limit",
        ::agenticdsl::ToolMetadata{
            .name = "budget/set_limit",
            .description = "Set budget limit",
            .domain = "budget",
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
            double new_limit = dbl_arg(args, "limit_usd", -1.0);
            if (new_limit < 0) {
                throw std::runtime_error("limit_usd must be >= 0");
            }
            store.set_global_limit(new_limit);
            return {{"ok", true}, {"new_limit_usd", new_limit}};
        }
    );

    // budget/alerts
    registry.register_tool_function(
        "budget/alerts",
        ::agenticdsl::ToolMetadata{
            .name = "budget/alerts",
            .description = "Register budget alert",
            .domain = "budget",
            .category = ::agenticdsl::ToolCategory::Execute,
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
            std::string subscriber_id = str_arg(args, "subscriber_id");
            double threshold = dbl_arg(args, "threshold", 0.5);
            std::string severity = str_arg(args, "severity", "warning");
            if (subscriber_id.empty()) {
                throw std::runtime_error("subscriber_id is required");
            }
            store.register_alert(subscriber_id, threshold, severity);
            return {{"ok", true}, {"triggered", store.triggered_alerts()}};
        }
    );

    // budget/cost_breakdown
    registry.register_tool_function(
        "budget/cost_breakdown",
        ::agenticdsl::ToolMetadata{
            .name = "budget/cost_breakdown",
            .description = "Get cost breakdown",
            .domain = "budget",
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
        [&store](const std::unordered_map<std::string, std::string>& /*args*/) -> nlohmann::json {
            return store.cost_breakdown();
        }
    );
}