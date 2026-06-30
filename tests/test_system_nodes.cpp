// tests/test_system_nodes.cpp
// Sprint 16 Coverage Backfill: 测试 src/modules/system/system_nodes.cpp
#include "catch_amalgamated.hpp"
#include "system/system_nodes.h"
#include "core/types/node.h"

using namespace agenticdsl;

TEST_CASE("create_system_nodes returns expected node count", "[system]") {
  auto nodes = create_system_nodes();
  REQUIRE(nodes.size() == 3);
  REQUIRE(nodes[0]->path == "/__system__/budget_exceeded");
  REQUIRE(nodes[1]->path == "/__system__/end_soft");
  REQUIRE(nodes[2]->path == "/__system__/noop");
}

TEST_CASE("System nodes have correct type and metadata", "[system]") {
  auto nodes = create_system_nodes();
  REQUIRE(nodes.size() == 3);

  // 所有 system nodes 都是 EndNode 类型
  for (const auto& node : nodes) {
    REQUIRE(node->type == NodeType::END);
  }

  // /__system__/budget_exceeded → hard 终止
  REQUIRE(nodes[0]->metadata.contains("termination_mode"));
  REQUIRE(nodes[0]->metadata["termination_mode"] == "hard");

  // 其余为 soft 终止
  REQUIRE(nodes[1]->metadata["termination_mode"] == "soft");
  REQUIRE(nodes[2]->metadata["termination_mode"] == "soft");
}