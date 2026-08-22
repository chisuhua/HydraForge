// src/core/agent_registry.cpp
// 功能描述：IAgentRegistry 的内存参考实现 (ADR-0082 §决策 7)
//
//          V1 最小骨架：
//          - shared_mutex 并发模型 (read-shared / write-exclusive)
//          - string-keyed factory map (与 NodeFactoryRegistry 模式一致)
//          - register 返回 bool（不抛）— Metis 修正
//          - unregister 同步删除（pending 语义留 Sprint 24+）
//          - 实例计数（每实例 ID 自增）
//
// 设计依据：ADR-0082 §决策 7 + ADR-0022 per-engine 注册粒度
// 作者：HydraForge Sprint 22 / adr-0082-promote-to-approved
// 最后修改日期：2026-08-21

#include "agenticdsl/contract/iagent_registry.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace agenticdsl {

namespace {

std::string generate_instance_id() {
  static std::atomic<std::uint64_t> counter{0};
  auto n = counter.fetch_add(1);
  auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();
  return "agent-" + std::to_string(ts) + "-" + std::to_string(n);
}

}  // namespace

// 最小 IAgent 实现（V1 骨架）
class SimpleAgent : public IAgent {
 public:
  SimpleAgent(std::string name, std::string id)
      : name_(std::move(name)), id_(std::move(id)) {}

  const std::string& name() const override { return name_; }
  const std::string& id() const override { return id_; }

 private:
  std::string name_;
  std::string id_;
};

// 测试 helper：创建 SimpleAgent（暴露给 test_agent_registry.cpp）
std::unique_ptr<IAgent> make_mock_agent_for_test(const std::string& name,
                                                  const std::string& id) {
  return std::make_unique<SimpleAgent>(name, id);
}

class InMemoryAgentRegistry : public IAgentRegistry {
 public:
  bool register_agent(const std::string& string_id,
                      AgentFactory factory) override {
    if (!factory) return false;
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto [it, inserted] = factories_.try_emplace(string_id, std::move(factory));
    (void)it;
    return inserted;
  }

  std::unique_ptr<IAgent> create(const std::string& string_id,
                                  const AgentConfig& config) override {
    AgentFactory factory_copy;
    {
      std::shared_lock<std::shared_mutex> lock(mutex_);
      auto it = factories_.find(string_id);
      if (it == factories_.end()) return nullptr;
      factory_copy = it->second;
    }
    std::string instance_id = config.instance_id.empty()
                                  ? generate_instance_id()
                                  : config.instance_id;
    return factory_copy(config);
  }

  bool unregister(const std::string& string_id) override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    return factories_.erase(string_id) > 0;
  }

  std::vector<std::string> list_registered() const override {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<std::string> ids;
    ids.reserve(factories_.size());
    for (const auto& [id, _] : factories_) {
      ids.push_back(id);
    }
    return ids;
  }

  bool is_registered(const std::string& string_id) const override {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return factories_.find(string_id) != factories_.end();
  }

  size_t size() const override {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return factories_.size();
  }

 private:
  mutable std::shared_mutex mutex_;
  std::unordered_map<std::string, AgentFactory> factories_;
};

// 工厂函数：创建 InMemoryAgentRegistry 实例（与 ToolRegistry 模式一致）
std::unique_ptr<IAgentRegistry> make_in_memory_agent_registry() {
  return std::make_unique<InMemoryAgentRegistry>();
}

// 工厂函数：测试用 mock agent（V1 简化：name 固定为 "mock-agent"，
// 真实场景中 factory_fn 闭包会捕获 string_id 作为 name 字段）
std::unique_ptr<IAgent> make_mock_agent(const AgentConfig& config) {
  std::string id = config.instance_id.empty() ? generate_instance_id() : config.instance_id;
  return make_mock_agent_for_test("mock-agent", id);
}

// 工厂函数：测试用 named mock agent（捕获 string_id 作为 name）
// 用于验证 register(string_id) → create(string_id) 链路语义
std::unique_ptr<IAgent> make_named_mock_agent(const AgentConfig& config,
                                              const std::string& string_id) {
  std::string id = config.instance_id.empty() ? generate_instance_id() : config.instance_id;
  return make_mock_agent_for_test(string_id, id);
}

}  // namespace agenticdsl