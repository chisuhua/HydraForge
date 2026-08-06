#include "provider_agent.h"

#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "agenticdsl/contract/iprovider_factory.h"
#include "common/llm/llm_provider_factory.h"

namespace pdk_provider_agent {

nlohmann::json invoke_switch_tool(agenticdsl::LLMProviderFactory& factory,
                                  const std::string& target) {
  if (target.empty()) {
    return nlohmann::json{{"ok", false},
                           {"error_code", "validation"},
                           {"warning", "target provider name is empty"}};
  }
  if (!factory.has_dynamic(target)) {
    return nlohmann::json{{"ok", false},
                           {"error_code", "unknown-provider"},
                           {"target", target},
                           {"current_default", factory.current_default()}};
  }
  if (!factory.switch_default(target)) {
    return nlohmann::json{{"ok", false},
                           {"error_code", "switch-failed"},
                           {"current_default", factory.current_default()}};
  }
  return nlohmann::json{{"ok", true},
                         {"current_default", factory.current_default()}};
}

}  // namespace pdk_provider_agent
