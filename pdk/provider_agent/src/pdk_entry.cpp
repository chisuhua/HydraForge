// PDK Plugin entry — AgenticDSL v1 API (hydraforge namespace for PluginInfo, agenticdsl for IToolRegistry)
// 关联: openspec/changes/2026-07-17-pdk-chat-demo-buildable/

#include <stdexcept>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include <agenticdsl/contract/itool_registry.h>
#include <agenticdsl/plugin/plugin_info.h>

#include "provider_agent.h"

namespace {

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

inline std::string str_arg(const std::unordered_map<std::string, std::string>& args,
                           const std::string& key, const std::string& default_val = "") {
    auto it = args.find(key);
    return (it != args.end()) ? it->second : default_val;
}

}  // namespace

// --- pdk_plugin_info ---
extern "C" const hydraforge::PluginInfo pdk_plugin_info = {
    hydraforge::CURRENT_ABI_VERSION,                                // abi_version = 2
    "infra.provider",                                               // name[64]
    0, 1, 0,                                                        // semver major.minor.patch
    "Provider Agent - LLM provider registry and credential resolution", // description[256]
    "provider_register,provider_resolve,provider_list,provider_health", // capabilities[512]
    ""                                                              // dependencies[256]
};

// --- pdk_register_tools ---
extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry) {
    auto& reg = pdk_provider_agent::ProviderRegistry::instance();

    // 1. provider/register
    registry.register_tool_function(
        "provider/register",
        ::agenticdsl::ToolMetadata{
            .name = "provider/register",
            .description = "Register LLM provider",
            .domain = "provider",
            .category = ::agenticdsl::ToolCategory::StateModify,
            .min_layer = ::agenticdsl::LayerProfile::Workflow,
            .approval = ::agenticdsl::ApprovalPolicy{
                .requires_approval_in_plan = true,
                .requires_approval_in_agent = true,
                .requires_approval_in_yolo = false,
                .force_approval_always = true
            },
            .allowed_layers = {::agenticdsl::LayerProfile::Workflow}
        },
        [&reg](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
            nlohmann::json j = json_arg(args, "args");
            if (j.is_null()) j = nlohmann::json::object();
            reg.register_providers(j);
            return {{"ok", true}, {"count", reg.list_providers().size()}};
        }
    );

    // 2. provider/resolve
    registry.register_tool_function(
        "provider/resolve",
        ::agenticdsl::ToolMetadata{
            .name = "provider/resolve",
            .description = "Resolve provider and model",
            .domain = "provider",
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
        [&reg](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
            std::string provider_id = str_arg(args, "provider_id");
            std::string model_id = str_arg(args, "model_id");
            if (provider_id.empty()) {
                throw std::runtime_error("provider_id is required");
            }
            return reg.resolve(provider_id, model_id);
        }
    );

    // 3. provider/list
    registry.register_tool_function(
        "provider/list",
        ::agenticdsl::ToolMetadata{
            .name = "provider/list",
            .description = "List registered providers",
            .domain = "provider",
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
        [&reg](const std::unordered_map<std::string, std::string>& /*args*/) -> nlohmann::json {
            return {{"providers", reg.list_providers()}};
        }
    );

    // 4. provider/health
    registry.register_tool_function(
        "provider/health",
        ::agenticdsl::ToolMetadata{
            .name = "provider/health",
            .description = "Check provider health",
            .domain = "provider",
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
        [&reg](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
            std::string provider_id = str_arg(args, "provider_id");
            if (provider_id.empty()) {
                throw std::runtime_error("provider_id is required");
            }
            return reg.health(provider_id);
        }
    );
}