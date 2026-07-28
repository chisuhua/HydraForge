// tests/test_pdk_register_agent.cpp
// 功能描述：pdk_register_agent 测试 - AgentDescriptor 导出 + 符号验证
//          验证 get_agent_descriptor() 返回正确数据 + pdk_register_agent 符号导出
// 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md §7.7
//          .rddf/plans/pkgm-temporal-agent.md Task 7
// 作者：pkgm-temporal-agent Phase 2
// 最后修改日期：2026-07-28

#include "catch_amalgamated.hpp"

#include "pdk_entry.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace pdk_temporal_agent;

TEST_CASE("pdk_register_agent: AgentDescriptor has 5 capabilities",
          "[temporal_agent][agent_descriptor]") {
  auto desc = get_agent_descriptor();

  REQUIRE(desc.name == "temporal_agent");
  REQUIRE(desc.capabilities.size() == 5);
  REQUIRE(desc.version == "0.2.0");

  REQUIRE(std::find(desc.capabilities.begin(), desc.capabilities.end(),
                    "temporal/start_workflow") != desc.capabilities.end());
  REQUIRE(std::find(desc.capabilities.begin(), desc.capabilities.end(),
                    "temporal/start_async") != desc.capabilities.end());
  REQUIRE(std::find(desc.capabilities.begin(), desc.capabilities.end(),
                    "temporal/poll") != desc.capabilities.end());
  REQUIRE(std::find(desc.capabilities.begin(), desc.capabilities.end(),
                    "temporal/signal") != desc.capabilities.end());
  REQUIRE(std::find(desc.capabilities.begin(), desc.capabilities.end(),
                    "temporal/query") != desc.capabilities.end());
}

TEST_CASE("pdk_register_agent: extern C symbol returns same descriptor",
          "[temporal_agent][agent_descriptor]") {
  auto* desc = pdk_register_agent();

  REQUIRE(desc != nullptr);
  REQUIRE(desc->name == "temporal_agent");
  REQUIRE(desc->capabilities.size() == 5);
  REQUIRE(desc->version == "0.2.0");
}

TEST_CASE("pdk_register_agent: capability names match registered tool names",
          "[temporal_agent][agent_descriptor]") {
  auto desc = get_agent_descriptor();

  const std::vector<std::string> expected = {
    "temporal/start_workflow",
    "temporal/start_async",
    "temporal/poll",
    "temporal/signal",
    "temporal/query"
  };

  for (const auto& cap : expected) {
    REQUIRE(std::find(desc.capabilities.begin(), desc.capabilities.end(), cap)
            != desc.capabilities.end());
  }
}
