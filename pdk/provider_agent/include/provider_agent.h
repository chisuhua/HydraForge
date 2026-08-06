// pdk/provider_agent/include/provider_agent.h
// ProviderInfo 结构体 + 凭据管理 + refresh 工具 (provider-dynamic-discovery)
// 关联: docs/adr/adr-0021-pdk-design.md

#pragma once

#include <atomic>
#include <chrono>
#include <ctime>
#include <functional>
#include <iomanip>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include "common/llm/llm_provider_factory.h"

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
    std::map<std::string, bool> removed_models;  // 已下线的模型
    std::string last_refresh;      // ISO-8601 UTC 时间戳

    // 解析后的 API key (从 env var 读取，不存储)
    std::optional<std::string> resolve_api_key() const;
};

struct RefreshResult {
  bool ok = false;
  std::string provider;
  std::vector<std::string> added;
  std::vector<std::string> removed;
  std::size_t model_count = 0;
  std::string last_refresh;   // ISO-8601 UTC, "1970-01-01T00:00:00Z" on failure
  std::string warning;        // empty on success
  std::string error_code;     // empty on success
};

using RefreshTransport = std::function<nlohmann::json(const ProviderInfo&)>;

inline std::string refresh_now_iso8601() {
  using namespace std::chrono;
  const auto now = system_clock::now();
  const auto t = system_clock::to_time_t(now);
  std::tm tm{};
  gmtime_r(&t, &tm);
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

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

    // provider/refresh 工具 (provider-dynamic-discovery)
    RefreshResult refresh(const std::string& provider_id);
    std::vector<std::string> list_models(const std::string& provider_id) const;
    std::map<std::string, bool> removed_models(const std::string& provider_id) const;
    std::string last_refresh_for(const std::string& provider_id) const;
    bool has_provider(const std::string& provider_id) const;

    // Test-only hooks (untyped name documented, no production log path)
    void seed_for_test(std::map<std::string, ProviderInfo> seed);
    void set_refresh_transport_for_test(RefreshTransport t);

public:
    ProviderRegistry() = default;

private:

    mutable std::mutex mutex_;
    std::map<std::string, ProviderInfo> providers_;
    RefreshTransport refresh_transport_;  // empty = use HTTP default
};

nlohmann::json invoke_register_dynamic_tool(
    agenticdsl::LLMProviderFactory& factory,
    ProviderRegistry& registry,
    const nlohmann::json& input);

nlohmann::json invoke_switch_tool(
    agenticdsl::LLMProviderFactory& factory,
    const std::string& target);

inline std::unique_ptr<agenticdsl::LLMProviderFactory>& factory_slot() {
  static std::unique_ptr<agenticdsl::LLMProviderFactory> instance;
  return instance;
}

}  // namespace pdk_provider_agent
