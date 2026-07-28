// tests/test_temporal_agent_namespace.cpp
// 功能描述：NamespaceManager 测试 - CRUD + per-tenant 隔离 + 校验
// 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md §7.5
//          .rddf/plans/pkgm-temporal-agent.md Task 4
// 作者：pkgm-temporal-agent Phase 2
// 最后修改日期：2026-07-28

#include "catch_amalgamated.hpp"

#include "namespace_manager.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace pdk_temporal_agent;

TEST_CASE("NamespaceManager: create / describe / list / delete",
          "[temporal_agent][namespace]") {
  NamespaceManager mgr("localhost:7233");

  mgr.create_namespace("tenant-a", 7);
  auto info = mgr.describe_namespace("tenant-a");
  REQUIRE(info.name == "tenant-a");
  REQUIRE(info.retention_days == 7);
  REQUIRE(info.state == "ACTIVE");

  auto list = mgr.list_namespaces();
  REQUIRE(std::find(list.begin(), list.end(), "tenant-a") != list.end());

  mgr.delete_namespace("tenant-a");
  REQUIRE_THROWS_AS(mgr.describe_namespace("tenant-a"), std::runtime_error);
}

TEST_CASE("NamespaceManager: list_namespaces empty initially",
          "[temporal_agent][namespace]") {
  NamespaceManager mgr("localhost:7233");
  auto list = mgr.list_namespaces();
  REQUIRE(list.empty());
}

TEST_CASE("NamespaceManager: describe non-existent throws",
          "[temporal_agent][namespace]") {
  NamespaceManager mgr("localhost:7233");
  REQUIRE_THROWS_AS(mgr.describe_namespace("no-such-ns"), std::runtime_error);
}

TEST_CASE("NamespaceManager: delete non-existent throws",
          "[temporal_agent][namespace]") {
  NamespaceManager mgr("localhost:7233");
  REQUIRE_THROWS_AS(mgr.delete_namespace("ghost"), std::runtime_error);
}

TEST_CASE("NamespaceManager: create same name twice updates retention",
          "[temporal_agent][namespace]") {
  NamespaceManager mgr("localhost:7233");
  mgr.create_namespace("dup-ns", 3);
  mgr.create_namespace("dup-ns", 14);
  auto info = mgr.describe_namespace("dup-ns");
  REQUIRE(info.retention_days == 14);
  REQUIRE(info.state == "ACTIVE");

  auto list = mgr.list_namespaces();
  REQUIRE(std::count(list.begin(), list.end(), "dup-ns") == 1);
}

TEST_CASE("NamespaceManager: negative retention_days rejected",
          "[temporal_agent][namespace]") {
  NamespaceManager mgr("localhost:7233");
  REQUIRE_THROWS_AS(mgr.create_namespace("bad-ns", -1), std::invalid_argument);
  REQUIRE_THROWS_AS(mgr.describe_namespace("bad-ns"), std::runtime_error);
}

TEST_CASE("NamespaceManager: multiple namespaces coexist independently",
          "[temporal_agent][namespace]") {
  NamespaceManager mgr("localhost:7233");
  mgr.create_namespace("tenant-x", 7);
  mgr.create_namespace("tenant-y", 30);
  mgr.create_namespace("tenant-z", 1);

  auto list = mgr.list_namespaces();
  REQUIRE(list.size() == 3);

  REQUIRE(mgr.describe_namespace("tenant-x").retention_days == 7);
  REQUIRE(mgr.describe_namespace("tenant-y").retention_days == 30);
  REQUIRE(mgr.describe_namespace("tenant-z").retention_days == 1);

  mgr.delete_namespace("tenant-y");
  REQUIRE_THROWS_AS(mgr.describe_namespace("tenant-y"), std::runtime_error);
  REQUIRE(mgr.describe_namespace("tenant-x").retention_days == 7);
  REQUIRE(mgr.describe_namespace("tenant-z").retention_days == 1);

  auto list_after = mgr.list_namespaces();
  REQUIRE(list_after.size() == 2);
}
