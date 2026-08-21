// include/agenticdsl/contract/test_double_registry.h
// 功能描述：test-double AgentRegistry（P8 adr-0060-p2-p3-patterns）
//          解决 P8 对可注册 Agent 的依赖（ADR-0082 后续实施）
//          register_agent / create / unregister / list_registered
// 设计依据：openspec/changes/adr-0060-p2-p3-patterns (P8)
// 作者：HydraForge Sprint 22 P8 ship
// 最后修改日期：2026-08-20

#pragma once

#include "agenticdsl/contract/iagent_composition.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace agenticdsl {

// Mock Agent（test-double）
class MockAgent {
 public:
  explicit MockAgent(std::string id) : id_(std::move(id)) {}

  virtual ~MockAgent() = default;

  const std::string& id() const { return id_; }

  // 模拟执行：返回 JSON 字符串
  virtual std::string run(const std::string& args) {
    // 默认实现：echo
    return "{\"agent\":\"" + id_ + "\",\"echo\":\"" + args + "\"}";
  }

 private:
  std::string id_;
};

// test-double AgentRegistry（内存实现，无持久化）
class TestDoubleAgentRegistry {
 public:
  using FactoryFn = std::function<std::unique_ptr<MockAgent>(const std::string&)>;

  // 注册 agent 工厂
  void register_agent(const std::string& id, FactoryFn factory) {
    std::lock_guard<std::mutex> lock(mutex_);
    factories_[id] = std::move(factory);
  }

  // 创建 agent 实例
  std::unique_ptr<MockAgent> create(const std::string& id,
                                    const std::string& config = "") {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = factories_.find(id);
    if (it == factories_.end()) return nullptr;
    return it->second(config);
  }

  // 注销 agent
  void unregister(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    factories_.erase(id);
  }

  // 列出已注册的 agent
  std::vector<std::string> list_registered() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> ids;
    for (const auto& [id, _] : factories_) ids.push_back(id);
    return ids;
  }

 private:
  std::mutex mutex_;
  std::unordered_map<std::string, FactoryFn> factories_;
};

}  // namespace agenticdsl