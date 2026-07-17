// PDK Plugin entry — AgenticDSL v1 API (hydraforge namespace for PluginInfo, agenticdsl for IToolRegistry)
// 关联: openspec/changes/2026-07-17-pdk-chat-demo-buildable/

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include <agenticdsl/contract/itool_registry.h>
#include <agenticdsl/plugin/plugin_info.h>

namespace fs = std::filesystem;

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

// 安全检查：拒绝路径穿越
static bool is_path_safe(const fs::path& p) {
    for (const auto& component : p) {
        if (component == "..") return false;
    }
    return true;
}

// --- pdk_plugin_info ---
extern "C" const hydraforge::PluginInfo pdk_plugin_info = {
    hydraforge::CURRENT_ABI_VERSION,                  // abi_version = 2
    "tool.fs",                                        // name[64]
    0, 1, 0,                                          // semver major.minor.patch
    "FS Tools - file read/write/list/exists",         // description[256]
    "file_read,file_write,directory_list,file_exists", // capabilities[512]
    ""                                                // dependencies[256]
};

// --- pdk_register_tools ---
extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry) {
    // fs/read
    registry.register_tool_function(
        "fs/read",
        ::agenticdsl::ToolMetadata{
            .name = "fs/read",
            .description = "Read file content",
            .domain = "fs",
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
        [](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
            std::string path = str_arg(args, "path");
            if (path.empty()) throw std::runtime_error("path is required");
            fs::path p(path);
            if (!is_path_safe(p)) throw std::runtime_error("path traversal detected");
            if (!fs::exists(p)) throw std::runtime_error("file not found: " + path);
            if (!fs::is_regular_file(p)) throw std::runtime_error("not a regular file: " + path);

            std::ifstream f(p);
            if (!f.is_open()) throw std::runtime_error("cannot open: " + path);
            std::stringstream ss;
            ss << f.rdbuf();
            return {
                {"path", path},
                {"content", ss.str()},
                {"size_bytes", fs::file_size(p)}
            };
        }
    );

    // fs/write
    registry.register_tool_function(
        "fs/write",
        ::agenticdsl::ToolMetadata{
            .name = "fs/write",
            .description = "Write content to file",
            .domain = "fs",
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
        [](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
            std::string path = str_arg(args, "path");
            std::string content = str_arg(args, "content");
            if (path.empty()) throw std::runtime_error("path is required");
            fs::path p(path);
            if (!is_path_safe(p)) throw std::runtime_error("path traversal detected");

            // 创建父目录
            if (p.has_parent_path()) fs::create_directories(p.parent_path());

            std::ofstream f(p, std::ios::trunc);
            if (!f.is_open()) throw std::runtime_error("cannot open for write: " + path);
            f << content;
            return {
                {"ok", true},
                {"path", path},
                {"size_bytes", content.size()}
            };
        }
    );

    // fs/list
    registry.register_tool_function(
        "fs/list",
        ::agenticdsl::ToolMetadata{
            .name = "fs/list",
            .description = "List directory contents",
            .domain = "fs",
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
        [](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
            std::string path = str_arg(args, "path", ".");
            fs::path p(path);
            if (!is_path_safe(p)) throw std::runtime_error("path traversal detected");
            if (!fs::exists(p)) throw std::runtime_error("directory not found: " + path);
            if (!fs::is_directory(p)) throw std::runtime_error("not a directory: " + path);

            nlohmann::json entries = nlohmann::json::array();
            std::error_code ec;
            for (auto it = fs::directory_iterator(p, ec); it != fs::directory_iterator(); it.increment(ec)) {
                if (ec) break;
                const auto& entry = *it;
                nlohmann::json e;
                e["name"] = entry.path().filename().string();
                e["path"] = entry.path().string();
                e["is_directory"] = entry.is_directory();
                e["size_bytes"] = entry.is_regular_file() ? entry.file_size() : 0;
                entries.push_back(e);
            }
            return {{"path", path}, {"entries", entries}, {"count", entries.size()}};
        }
    );

    // fs/exists
    registry.register_tool_function(
        "fs/exists",
        ::agenticdsl::ToolMetadata{
            .name = "fs/exists",
            .description = "Check if file or directory exists",
            .domain = "fs",
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
        [](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
            std::string path = str_arg(args, "path");
            if (path.empty()) throw std::runtime_error("path is required");
            fs::path p(path);
            return {
                {"path", path},
                {"exists", fs::exists(p)},
                {"is_file", fs::is_regular_file(p)},
                {"is_directory", fs::is_directory(p)}
            };
        }
    );
}