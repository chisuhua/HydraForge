#include "common/tools/registry.h"

#include <stdexcept>
#include <limits>

#include "common/policy/layer_profile.h"  // C6: check_registration_permission

namespace agenticdsl {

ToolRegistry::ToolRegistry() {
  register_default_tools();
}

void ToolRegistry::register_default_tools() {
    register_tool("web_search", 
        ToolMetadata{"web_search", "Search the web", "builtin", ToolCategory::ReadOnly, LayerProfile::Workflow, ApprovalPolicy{true, true, false, false}},
        [](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
            auto it = args.find("query");
            std::string query = (it != args.end()) ? it->second : "default query";
            return nlohmann::json{{"results", "[MOCK] Search results for: " + query}};
        });

    register_tool("get_weather",
        ToolMetadata{"get_weather", "Get weather info", "builtin", ToolCategory::ReadOnly, LayerProfile::Workflow, ApprovalPolicy{true, true, false, false}},
        [](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
            auto it = args.find("location");
            std::string loc = (it != args.end()) ? it->second : "unknown";
            return nlohmann::json{
                {"location", loc},
                {"condition", "Sunny"},
                {"temperature_c", 22}
            };
        });

    register_tool("calculate",
        ToolMetadata{"calculate", "Calculate expression", "builtin", ToolCategory::ReadOnly, LayerProfile::Workflow, ApprovalPolicy{true, true, false, false}},
        [](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
            auto a_it = args.find("a");
            auto b_it = args.find("b");
            auto op_it = args.find("op");

            if (a_it == args.end() || b_it == args.end() || op_it == args.end()) {
                return nlohmann::json{{"error", "Missing arguments: a, b, op"}};
            }

            try {
                double a = std::stod(a_it->second);
                double b = std::stod(b_it->second);
                std::string op = op_it->second;

                if (op == "/" && b == 0.0) {
                    return nlohmann::json{{"error", "Division by zero"}};
                }

                double result = 0.0;
                if (op == "+") result = a + b;
                else if (op == "-") result = a - b;
                else if (op == "*") result = a * b;
                else if (op == "/") result = a / b;
                else return nlohmann::json{{"error", "Unsupported operator: " + op}};

                return nlohmann::json{{"result", result}};
            } catch (const std::exception& e) {
                return nlohmann::json{{"error", "Invalid number format"}};
            }
        });
}

bool ToolRegistry::has_tool(const std::string& name) const {
  return tools_.count(name) > 0 || llm_tools_.count(name) > 0;
}

nlohmann::json ToolRegistry::call_tool(const std::string& name, const std::unordered_map<std::string, std::string>& args) {
  auto it = tools_.find(name);
  if (it == tools_.end()) {
    return nlohmann::json{{"error", "Tool not found: " + name}};
  }

  try {
    return it->second(args);
  } catch (const std::exception& e) {
    return nlohmann::json{{"error", std::string("Tool execution failed: ") + e.what()}};
  }
}

std::vector<std::string> ToolRegistry::list_tools() const {
  std::vector<std::string> names;
  names.reserve(tools_.size() + llm_tools_.size());
  for (const auto& [name, _] : tools_) {
    names.push_back(name);
  }
  for (const auto& [name, _] : llm_tools_) {
    names.push_back(name);
  }
  return names;
}

void ToolRegistry::register_tool_function(std::string name, ToolMetadata meta, ToolFunc fn) {
    if (name.empty()) throw std::invalid_argument("ToolRegistry: tool name must not be empty");
    if (tools_.count(name)) throw std::invalid_argument("ToolRegistry: tool '" + name + "' already registered");
    
    bool no_plan_or_agent = !meta.approval.requires_approval_in_plan && !meta.approval.requires_approval_in_agent;
    bool dangerous = (meta.category == ToolCategory::Execute || meta.category == ToolCategory::Network || meta.category == ToolCategory::StateModify);
    if (no_plan_or_agent && dangerous) {
        throw std::invalid_argument("ToolRegistry: tool '" + name + "' has dangerous category but no plan or agent approval");
    }
    
    if (!meta.allowed_layers.empty()) {
        bool found = false;
        for (auto& l : meta.allowed_layers) { if (l == meta.min_layer) found = true; }
        if (!found) throw std::invalid_argument("ToolRegistry: min_layer not in allowed_layers for '" + name + "'");
    }
    
    tools_[name] = std::move(fn);
    tool_metadata_[name] = std::move(meta);
}

void ToolRegistry::register_llm_tool(std::string name, std::unique_ptr<ILLMTool> tool, const LLMParams& default_params) {
  llm_tools_[std::move(name)] = LLMToolEntry{std::move(tool), default_params};
}

bool ToolRegistry::is_llm_tool(const std::string& name) const {
  return llm_tools_.count(name) > 0;
}

const LLMParams& ToolRegistry::get_llm_params(const std::string& name) const {
  auto it = llm_tools_.find(name);
  if (it == llm_tools_.end()) {
    throw std::runtime_error("Not an LLM tool: " + name);
  }
  return it->second.default_params;
}

nlohmann::json ToolRegistry::call_llm_tool(const std::string& name, const std::string& prompt, const LLMParams& params) {
  auto it = llm_tools_.find(name);
  if (it == llm_tools_.end()) {
    return nlohmann::json{{"error", "LLM tool not found: " + name}};
  }

  try {
    const LLMParams kDefaults{};
    LLMParams merged_params = it->second.default_params;
    if (params.temperature != kDefaults.temperature) merged_params.temperature = params.temperature;
    if (params.max_tokens != kDefaults.max_tokens) merged_params.max_tokens = params.max_tokens;
    if (params.top_p != kDefaults.top_p) merged_params.top_p = params.top_p;
    if (params.n_ctx != kDefaults.n_ctx) merged_params.n_ctx = params.n_ctx;
    if (params.n_threads != kDefaults.n_threads) merged_params.n_threads = params.n_threads;
    if (!params.model.empty()) merged_params.model = params.model;

    auto result = it->second.tool->generate(prompt, merged_params);

    nlohmann::json json_result;
    json_result["success"] = result.success;
    if (result.success) {
      json_result["text"] = result.text;
      json_result["tokens_generated"] = result.tokens_generated;

      if (cost_callback_) {
        try {
          cost_callback_(result.tokens_generated, merged_params.model);
        } catch (...) {
        }
      }
    } else {
      json_result["error"] = result.error;
    }
    return json_result;
  } catch (const std::exception& e) {
    return nlohmann::json{{"error", std::string("LLM tool execution failed: ") + e.what()}};
  }
}

void validate_tool_metadata(const ToolMetadata& meta) {
  check_registration_permission(meta);
}

std::vector<std::pair<std::string, ToolMetadata>> ToolRegistry::list_metadata() const {
  std::vector<std::pair<std::string, ToolMetadata>> result;
  result.reserve(tool_metadata_.size());
  for (const auto& [name, meta] : tool_metadata_) result.emplace_back(name, meta);
  return result;
}

} // namespace agenticdsl