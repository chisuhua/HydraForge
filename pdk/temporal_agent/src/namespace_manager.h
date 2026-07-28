// pdk/temporal_agent/src/namespace_manager.h
// 功能描述：Temporal Namespace 管理 - CRUD + per-tenant 隔离
//          采用内存 CRUD 层 (零 gRPC 依赖), Task 5 (GrpcTemporalBackend) 可覆盖为真实 gRPC 调用。
// 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md §7.5
//          .rddf/plans/pkgm-temporal-agent.md Task 4
// 线程安全：mutex 保护 namespaces_ map
// 作者：pkgm-temporal-agent Phase 2
// 最后修改日期：2026-07-28

#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace pdk_temporal_agent {

struct NamespaceInfo {
  std::string name;
  int retention_days = 0;
  std::string state;
};

class NamespaceManager {
 public:
  explicit NamespaceManager(std::string target);

  void create_namespace(const std::string& name, int retention_days);
  NamespaceInfo describe_namespace(const std::string& name);
  std::vector<std::string> list_namespaces();
  void delete_namespace(const std::string& name);

 private:
  std::string target_;
  std::mutex mu_;
  std::unordered_map<std::string, NamespaceInfo> namespaces_;
};

}  // namespace pdk_temporal_agent
