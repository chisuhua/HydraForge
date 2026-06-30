// tests/test_resource_manager.cpp
// Sprint 16 Coverage Backfill: 测试 src/modules/scheduler/resource_manager.cpp
#include "catch_amalgamated.hpp"
#include "core/types/node.h"
#include "core/types/resource.h"
#include "scheduler/resource_manager.h"

using namespace agenticdsl;

TEST_CASE("ResourceManager registers resources by path", "[scheduler][resource]") {
  ResourceManager mgr;

  Resource r1;
  r1.path = "/resources/db";
  r1.resource_type = ResourceType::POSTGRES;
  r1.uri = "postgresql://localhost:5432/test";
  r1.scope = "global";
  mgr.register_resource(r1);

  REQUIRE(mgr.has_resource("/resources/db"));
  REQUIRE_FALSE(mgr.has_resource("/resources/unknown"));

  const Resource* retrieved = mgr.get_resource("/resources/db");
  REQUIRE(retrieved != nullptr);
  REQUIRE(retrieved->path == "/resources/db");
  REQUIRE(retrieved->resource_type == ResourceType::POSTGRES);
  REQUIRE(retrieved->uri == "postgresql://localhost:5432/test");
  REQUIRE(retrieved->scope == "global");
}

TEST_CASE("ResourceManager::get returns null for unknown path", "[scheduler][resource]") {
  ResourceManager mgr;
  const Resource* result = mgr.get_resource("/not/registered");
  REQUIRE(result == nullptr);
}

TEST_CASE("ResourceManager::get_resources_context produces valid JSON", "[scheduler][resource]") {
  ResourceManager mgr;

  Resource file_r;
  file_r.path = "/resources/file";
  file_r.resource_type = ResourceType::FILE;
  file_r.uri = "/tmp/data.json";
  file_r.scope = "local";
  mgr.register_resource(file_r);

  Resource api_r;
  api_r.path = "/resources/api";
  api_r.resource_type = ResourceType::API_ENDPOINT;
  api_r.uri = "https://api.example.com";
  api_r.scope = "global";
  mgr.register_resource(api_r);

  auto ctx = mgr.get_resources_context();
  REQUIRE(ctx.is_object());
  REQUIRE(ctx.contains("/resources/file"));
  REQUIRE(ctx.contains("/resources/api"));
  REQUIRE(ctx["/resources/file"]["uri"] == "/tmp/data.json");
  REQUIRE(ctx["/resources/file"]["scope"] == "local");
  REQUIRE(ctx["/resources/api"]["uri"] == "https://api.example.com");
}

TEST_CASE("ResourceManager overwriting resource by same path", "[scheduler][resource]") {
  ResourceManager mgr;

  Resource r1;
  r1.path = "/resources/same";
  r1.resource_type = ResourceType::FILE;
  r1.uri = "/tmp/v1.json";
  mgr.register_resource(r1);

  Resource r2;
  r2.path = "/resources/same";
  r2.resource_type = ResourceType::FILE;
  r2.uri = "/tmp/v2.json";
  mgr.register_resource(r2);

  // 后注册覆盖前注册
  const Resource* retrieved = mgr.get_resource("/resources/same");
  REQUIRE(retrieved != nullptr);
  REQUIRE(retrieved->uri == "/tmp/v2.json");
}