// pdk/provider_agent/src/credential_store.cpp
// 凭据管理 - 安全地从环境变量解析 API key (不持久化存储)
// 关联: docs/adr/adr-0052-agent-plugin-manifest.md (trust_level + signature)

#include "provider_agent.h"

#include <cstdlib>
#include <stdexcept>

namespace pdk_provider_agent {

std::optional<std::string> ProviderInfo::resolve_api_key() const {
    if (api_key_env.empty()) {
        return std::nullopt;
    }

    const char* value = std::getenv(api_key_env.c_str());
    if (value == nullptr || *value == '\0') {
        return std::nullopt;
    }

    return std::string(value);
}

}  // namespace pdk_provider_agent