// pdk/provider_agent/src/provider_resolve.cpp
// Provider 注册 + 解析逻辑
// 关联: docs/examples/pdk_chat_demo/DESIGN.md §5.3

#include "provider_agent.h"

#include <algorithm>
#include <stdexcept>

namespace pdk_provider_agent {

ProviderRegistry& ProviderRegistry::instance() {
    static ProviderRegistry inst;
    return inst;
}

void ProviderRegistry::register_providers(const nlohmann::json& providers_config) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto it = providers_config.begin(); it != providers_config.end(); ++it) {
        const std::string& provider_id = it.key();
        const auto& pcfg = it.value();

        ProviderInfo info;
        info.id = provider_id;
        info.api_url = pcfg.value("api_url", "");
        info.api_key_env = pcfg.value("api_key_env", "");

        if (pcfg.contains("models") && pcfg["models"].is_object()) {
            for (auto mit = pcfg["models"].begin(); mit != pcfg["models"].end(); ++mit) {
                const std::string& model_id = mit.key();
                const auto& mcfg = mit.value();

                ModelConfig mc;
                mc.model = mcfg.value("model", model_id);
                mc.max_tokens = mcfg.value("max_tokens", 4096);
                mc.temperature = mcfg.value("temperature", 0.7);

                if (mcfg.contains("extra")) {
                    mc.extra = mcfg["extra"];
                }

                info.models[model_id] = std::move(mc);
            }
        }

        providers_[provider_id] = std::move(info);
    }
}

std::vector<std::string> ProviderRegistry::list_providers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    result.reserve(providers_.size());
    for (const auto& [id, _] : providers_) {
        result.push_back(id);
    }
    std::sort(result.begin(), result.end());
    return result;
}

nlohmann::json ProviderRegistry::resolve(
    const std::string& provider_id,
    const std::string& model_id
) const {
    // 先获取可用 provider 列表（不持锁），用于错误消息
    auto available_ids = [this]() {
        std::vector<std::string> ids;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ids.reserve(providers_.size());
            for (const auto& [id, _] : providers_) ids.push_back(id);
        }
        std::sort(ids.begin(), ids.end());
        return ids;
    }();

    std::lock_guard<std::mutex> lock(mutex_);

    auto pit = providers_.find(provider_id);
    if (pit == providers_.end()) {
        std::string s;
        for (const auto& id : available_ids) { if (!s.empty()) s += ", "; s += id; }
        throw std::runtime_error(
            "Unknown provider: " + provider_id + " (available: " + s + ")"
        );
    }

    const auto& info = pit->second;
    auto mit = info.models.find(model_id);
    if (mit == info.models.end()) {
        throw std::runtime_error(
            "Unknown model: " + provider_id + "/" + model_id
        );
    }

    const auto& mc = mit->second;

    nlohmann::json result;
    result["provider"] = info.id;
    result["model"] = mc.model;
    result["max_tokens"] = mc.max_tokens;
    result["temperature"] = mc.temperature;
    result["api_url"] = info.api_url;
    result["api_key_env"] = info.api_key_env;

    // 延迟解析 API key (不持久化)
    auto api_key = info.resolve_api_key();
    if (api_key.has_value()) {
        result["api_key"] = api_key.value();  // 仅在解析时存在于内存
    } else if (!info.api_key_env.empty()) {
        // API key env var 未设置 - 不报错，可能使用 mock 或其他方式认证
        result["api_key_status"] = "env_var_not_set";
    }

    if (!mc.extra.is_null()) {
        result["extra"] = mc.extra;
    }

    return result;
}

nlohmann::json ProviderRegistry::health(const std::string& provider_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = providers_.find(provider_id);
    if (it == providers_.end()) {
        return {{"ok", false}, {"error", "unknown_provider"}};
    }

    const auto& info = it->second;

    nlohmann::json result;
    result["ok"] = true;
    result["provider"] = info.id;
    result["model_count"] = info.models.size();
    result["api_url"] = info.api_url;

    // API key 检查
    auto api_key = info.resolve_api_key();
    if (info.api_key_env.empty()) {
        result["api_key_status"] = "not_required";
    } else if (api_key.has_value()) {
        result["api_key_status"] = "present";
    } else {
        result["api_key_status"] = "missing";
        result["ok"] = false;
        result["warning"] = "API key env var not set: " + info.api_key_env;
    }

    return result;
}

}  // namespace pdk_provider_agent