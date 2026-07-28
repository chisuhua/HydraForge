// pdk/temporal_agent/src/namespace_manager.cpp
// 功能描述：NamespaceManager 实现 - 内存 CRUD + 校验
// 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md §7.5
//          .rddf/plans/pkgm-temporal-agent.md Task 4
// 线程安全：mutex 保护 namespaces_ map
// 作者：pkgm-temporal-agent Phase 2
// 最后修改日期：2026-07-28

#include "namespace_manager.h"

#include <stdexcept>

namespace pdk_temporal_agent {

NamespaceManager::NamespaceManager(std::string target)
    : target_(std::move(target)) {}

void NamespaceManager::create_namespace(const std::string& name,
                                         int retention_days) {
  if (retention_days < 0) {
    throw std::invalid_argument(
        "NamespaceManager: retention_days must be non-negative");
  }
  std::lock_guard<std::mutex> lock(mu_);
  namespaces_[name] = NamespaceInfo{
    .name = name,
    .retention_days = retention_days,
    .state = "ACTIVE"
  };
}

NamespaceInfo NamespaceManager::describe_namespace(const std::string& name) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = namespaces_.find(name);
  if (it == namespaces_.end()) {
    throw std::runtime_error(
        "NamespaceManager: namespace not found: " + name);
  }
  return it->second;
}

std::vector<std::string> NamespaceManager::list_namespaces() {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<std::string> result;
  result.reserve(namespaces_.size());
  for (const auto& [name, _] : namespaces_) {
    result.push_back(name);
  }
  return result;
}

void NamespaceManager::delete_namespace(const std::string& name) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = namespaces_.find(name);
  if (it == namespaces_.end()) {
    throw std::runtime_error(
        "NamespaceManager: namespace not found: " + name);
  }
  namespaces_.erase(it);
}

}  // namespace pdk_temporal_agent
