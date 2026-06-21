// include/agenticdsl/parser/node_factory.h
// Sprint 6 P2-6: 节点工厂注册表 (消除 create_node_from_json 216行 if-else 链)
// 设计依据: openspec/changes/tech-debt-cleanup-sprint-6/specs/node-factory-registry/spec.md
#pragma once

#include "core/types/node.h"  // Node, NodePath, NodeType

#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace agenticdsl {

class NodeFactoryRegistry {
 public:
  using Factory = std::function<std::unique_ptr<Node>(const NodePath&,
                                                     const nlohmann::json&)>;

  void register_factory(const std::string& type_name, Factory factory);
  std::unique_ptr<Node> create(const std::string& type_name,
                               const NodePath& path,
                               const nlohmann::json& node_json) const;
  bool has_factory(const std::string& type_name) const;
  size_t size() const;

  static NodeFactoryRegistry& global();

 private:
  NodeFactoryRegistry() = default;
  mutable std::shared_mutex mutex_;
  std::unordered_map<std::string, Factory> factories_;
};

}  // namespace agenticdsl