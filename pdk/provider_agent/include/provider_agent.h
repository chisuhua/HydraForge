// pdk/provider_agent/include/provider_agent.h
// ProviderInfo 结构体 + 凭据管理
// 关联: docs/adr/adr-0021-pdk-design.md

#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace pdk_provider_agent {

struct ModelConfig {
    std::string model;            // 实际 LLM 模型名 (e.g. "gpt-4o")
    int max_tokens = 4096;
    double temperature = 0.7;
    nlohmann::json extra;         // 模型特定参数
};

struct ProviderInfo {
    std::string id;                // "openai", "anthropic", "mock"
    std::string api_url;
    std::string api_endpoint;       // API 端点路径 (默认 /v1/chat/completions)
    std::string api_key_env;       // 环境变量名 (延迟解析)
    std::map<std::string, ModelConfig> models;  // model_id -> config

    // 解析后的 API key (从 env var 读取，不存储)
    std::optional<std::string> resolve_api_key() const;
};

// 全局注册表 (线程安全)
class ProviderRegistry {
public:
    static ProviderRegistry& instance();

    // 注册 provider configs (从 JSON)
    void register_providers(const nlohmann::json& providers_config);

    // 列出所有 provider
    std::vector<std::string> list_providers() const;

    // 解析 provider_id + model_id -> LLMConfig JSON
    nlohmann::json resolve(const std::string& provider_id,
                           const std::string& model_id) const;

    // 健康检查 (ping api_url)
    nlohmann::json health(const std::string& provider_id) const;

private:
    ProviderRegistry() = default;

    mutable std::mutex mutex_;
    std::map<std::string, ProviderInfo> providers_;
};

}  // namespace pdk_provider_agent