#include "provider_agent.h"

#include <stdexcept>

namespace pdk_provider_agent {

namespace {

bool validate_item(const nlohmann::json& item) {
  return item.is_object() && item.contains("id") &&
         item["id"].is_string() && !item["id"].get<std::string>().empty();
}

nlohmann::json fetch_models_http(const ProviderInfo& info) {
  // Phase 1 fallback: real cpp-httplib call goes here.
  // Throw on network error so the caller can preserve old catalog.
  (void)info;
  throw std::runtime_error("real http transport not yet wired (Phase 1 stub)");
}

}  // namespace

RefreshResult ProviderRegistry::refresh(const std::string& provider_id) {
  ProviderInfo old_info;
  RefreshTransport transport;
  std::size_t prior_count = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = providers_.find(provider_id);
    if (it == providers_.end()) {
      return RefreshResult{false, provider_id, {}, {}, 0, {}, "unknown provider",
                           "unknown-provider"};
    }
    old_info = it->second;
    transport = refresh_transport_;
    prior_count = old_info.models.size();
  }

  nlohmann::json response;
  try {
    response = transport ? transport(old_info) : fetch_models_http(old_info);
  } catch (const std::exception& e) {
    return RefreshResult{false, provider_id, {}, {}, prior_count, old_info.last_refresh,
                         std::string("refresh failed: ") + e.what(), "retryable"};
  }

  if (!response.is_object() || !response.contains("data") ||
      !response["data"].is_array() || response["data"].empty()) {
    return RefreshResult{false, provider_id, {}, {}, prior_count, old_info.last_refresh,
                         "model catalog schema is invalid or empty", "validation"};
  }

  ProviderInfo candidate = old_info;
  candidate.models.clear();
  candidate.removed_models.clear();
  for (const auto& item : response["data"]) {
    if (!validate_item(item)) {
      return RefreshResult{false, provider_id, {}, {}, prior_count,
                           old_info.last_refresh,
                           "model catalog item missing valid id", "validation"};
    }
    const auto id = item["id"].get<std::string>();
    candidate.models.emplace(id, ModelConfig{id, 4096, 0.7, nlohmann::json::object()});
  }

  RefreshResult result;
  result.provider = provider_id;
  result.model_count = candidate.models.size();
  result.last_refresh = refresh_now_iso8601();
  for (const auto& [id, _] : candidate.models) {
    if (!old_info.models.count(id)) result.added.push_back(id);
  }
  for (const auto& [id, _] : old_info.models) {
    if (!candidate.models.count(id)) {
      result.removed.push_back(id);
      candidate.removed_models[id] = true;
    }
  }
  candidate.last_refresh = result.last_refresh;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = providers_.find(provider_id);
    if (it == providers_.end()) {
      return RefreshResult{false, provider_id, {}, {}, prior_count, old_info.last_refresh,
                           "provider vanished during refresh", "unknown-provider"};
    }
    it->second = std::move(candidate);
  }
  result.ok = true;
  return result;
}

}  // namespace pdk_provider_agent
