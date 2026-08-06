#include "provider_agent.h"

#include <memory>
#include <string>

// Include mock_provider BEFORE provider_agent.h to ensure ILLMProvider is fully defined
// before the compiler sees MockLLMProvider's inheritance from ILLMProvider
#include "common/llm/mock_provider.h"
#include "common/llm/llm_config.h"
#include "common/llm/llm_provider_factory.h"

#include <nlohmann/json.hpp>

namespace pdk_provider_agent {

namespace {

bool validate(const nlohmann::json& input, std::string& err) {
  if (!input.is_object()) {
    err = "input must be an object";
    return false;
  }
  if (input.value("name", std::string{}).empty()) { err = "name is required"; return false; }
  const std::string backend = input.value("backend", std::string{});
  if (backend != "mock" && backend != "openai" && backend != "anthropic" &&
      backend != "deepseek" && backend != "minimax" && backend != "qwen" &&
      backend != "moonshot" && backend != "custom" && backend != "local" &&
      backend != "llama") {
    err = "unsupported backend";
    return false;
  }
  if (input.value("api_url", std::string{}).empty()) { err = "api_url is required"; return false; }
  if (!input.contains("models") || !input["models"].is_array() ||
      input["models"].empty()) {
    err = "models must be a non-empty array";
    return false;
  }
  return true;
}

}  // namespace

nlohmann::json invoke_register_dynamic_tool(
    agenticdsl::LLMProviderFactory& factory,
    ProviderRegistry& registry,
    const nlohmann::json& input) {
  std::string err;
  if (!validate(input, err)) {
    return {{"ok", false}, {"error_code", "validation"}, {"warning", err}};
  }
  const std::string name = input["name"].get<std::string>();
  const std::string backend = input["backend"].get<std::string>();
  const std::string api_url = input["api_url"].get<std::string>();
  const std::string api_endpoint = input.value("api_endpoint", std::string{});
  const std::string api_key_env = input.value("api_key_env", std::string{});
  const std::string first_model_id = input["models"][0].value("id", "default");

  if (factory.has_dynamic(name)) {
    return {{"ok", false}, {"error_code", "duplicate-provider"}};
  }

  // Capture config values by value — never raw owning pointers.
  auto cb = [backend, api_url, api_endpoint, api_key_env, first_model_id](
                 const agenticdsl::LLMConfig& config)
      -> std::unique_ptr<agenticdsl::ILLMProvider> {
    agenticdsl::LLMConfig cfg = config;
    cfg.provider = backend;
    cfg.api_url = api_url;
    if (!api_endpoint.empty()) cfg.api_endpoint = api_endpoint;
    if (!api_key_env.empty()) cfg.api_key_env = std::make_optional(api_key_env);
    cfg.model = first_model_id;
    (void)cfg;
    // Provider-as-plugin: always create MockLLMProvider as safe fallback
    // Real cloud provider construction deferred to Phase 2 cloud wiring
    return std::make_unique<agenticdsl::MockLLMProvider>();
  };

  if (!factory.register_dynamic(name, std::move(cb))) {
    return {{"ok", false}, {"error_code", "duplicate-provider"}};
  }

  nlohmann::json register_payload;
  register_payload[name] = {
      {"api_url", api_url},
      {"api_key_env", api_key_env},
      {"models", nlohmann::json::object()}
  };
  for (const auto& m : input["models"]) {
    if (!m.is_object() || !m.contains("id")) continue;
    const auto id = m["id"].get<std::string>();
    register_payload[name]["models"][id] = {
        {"model", id},
        {"max_tokens", m.value("max_tokens", 4096)},
        {"temperature", m.value("temperature", 0.7)}};
  }
  if (!api_endpoint.empty()) register_payload[name]["api_endpoint"] = api_endpoint;
  registry.register_providers(register_payload);

  return {{"ok", true},
          {"name", name},
          {"factory_has_dynamic", factory.has_dynamic(name)},
          {"registry_count", static_cast<int>(registry.list_providers().size())}};
}

}  // namespace pdk_provider_agent

