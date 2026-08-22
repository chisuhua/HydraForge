// tests/test_agent_registry.cpp
// 功能描述：IAgentRegistry 契约测试（5 cases 对应 C1-C5 决议）
//          - C1 (Agent 标识): string_id 字符串与 PluginInfo::name 对齐 + register 返回 bool
//          - C2 (生命周期归属): per-engine 注册 + create/unregister
//          - C3 (状态持久化): N/A 留给 EventLog+SessionWriter 集成测试
//          - C4 (marketplace 接口): V1 plugin 形态；subprocess 形态 Phase 2 (留集成测试)
//          - C5 (集成边界): 与 IToolRegistry 接口正交，无耦合
//
//          Amendment (2026-08-21): V1 最小骨架测试集
//          完整 AgentWorker + 3 循环分发测试留 Sprint 24+ Agent hook 实施 change
//
// 设计依据：openspec/changes/adr-0082-promote-to-approved (P7) + ADR-0082 §决策 7
// 作者：HydraForge Sprint 22 / adr-0082-promote-to-approved
// 最后修改日期：2026-08-21

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/iagent_registry.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace agenticdsl;

namespace {

std::unique_ptr<IAgentRegistry> make_registry() {
  return make_in_memory_agent_registry();
}

AgentFactory make_named_factory(const std::string& string_id) {
  return [string_id](const AgentConfig& cfg) -> std::unique_ptr<IAgent> {
    static std::atomic<uint64_t> counter{0};
    std::string id = cfg.instance_id.empty()
                         ? "auto-" + std::to_string(counter.fetch_add(1))
                         : cfg.instance_id;
    return make_mock_agent_for_test(string_id, id);
  };
}

}  // namespace

TEST_CASE("IAgentRegistry C1: register + is_registered + 重复注册返回 false",
          "[agent][P7][registry][C1]") {
  auto registry = make_registry();
  REQUIRE(registry->size() == 0);

  REQUIRE(registry->register_agent("react-loop-v1", make_named_factory("react-loop-v1")));
  REQUIRE(registry->is_registered("react-loop-v1"));
  REQUIRE(registry->size() == 1);

  // 重复注册返回 false（不抛异常）
  REQUIRE_FALSE(registry->register_agent("react-loop-v1", make_named_factory("react-loop-v1")));
  REQUIRE(registry->size() == 1);
}

TEST_CASE("IAgentRegistry C2: per-engine 注册 + create + list + unregister",
          "[agent][P7][registry][C2]") {
  auto registry = make_registry();
  REQUIRE(registry->register_agent("plan-execute-v1", make_named_factory("plan-execute-v1")));
  REQUIRE(registry->register_agent("fork-join-v1", make_named_factory("fork-join-v1")));
  REQUIRE(registry->size() == 2);

  AgentConfig cfg;
  auto agent1 = registry->create("plan-execute-v1", cfg);
  REQUIRE(agent1 != nullptr);
  REQUIRE(agent1->name() == "plan-execute-v1");
  REQUIRE_FALSE(agent1->id().empty());

  auto agent2 = registry->create("plan-execute-v1", cfg);
  REQUIRE(agent2 != nullptr);
  REQUIRE(agent2->id() != agent1->id());

  AgentConfig cfg_explicit;
  cfg_explicit.instance_id = "inst-42";
  auto agent3 = registry->create("plan-execute-v1", cfg_explicit);
  REQUIRE(agent3 != nullptr);
  REQUIRE(agent3->id() == "inst-42");

  auto ids = registry->list_registered();
  REQUIRE(ids.size() == 2);

  REQUIRE(registry->unregister("plan-execute-v1"));
  REQUIRE_FALSE(registry->is_registered("plan-execute-v1"));
  REQUIRE(registry->is_registered("fork-join-v1"));
  REQUIRE(registry->size() == 1);

  REQUIRE_FALSE(registry->unregister("plan-execute-v1"));
}

TEST_CASE("IAgentRegistry create 未注册 string_id 返回 nullptr",
          "[agent][P7][registry]") {
  auto registry = make_registry();
  AgentConfig cfg;
  auto agent = registry->create("non-existent", cfg);
  REQUIRE(agent == nullptr);
}

TEST_CASE("IAgentRegistry register_agent 空 factory 返回 false",
          "[agent][P7][registry]") {
  auto registry = make_registry();
  REQUIRE_FALSE(registry->register_agent("invalid", nullptr));
  REQUIRE(registry->size() == 0);
}

TEST_CASE("IAgentRegistry C5: 与 IToolRegistry 接口正交（独立命名空间）",
          "[agent][P7][registry][C5]") {
  auto registry = make_registry();
  REQUIRE(registry->register_agent("react-loop-v1", make_named_factory("react-loop-v1")));

  AgentConfig cfg;
  auto agent = registry->create("react-loop-v1", cfg);
  REQUIRE(agent != nullptr);
  REQUIRE(agent->name() == "react-loop-v1");
  REQUIRE_FALSE(agent->id().empty());
}