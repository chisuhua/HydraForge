// tests/test_context_engine.cpp
// Sprint 16 Coverage Backfill: 测试 src/modules/context/context_engine.cpp
#include "catch_amalgamated.hpp"
#include "context/context_engine.h"

using namespace agenticdsl;

TEST_CASE("ContextEngine::merge handles disjoint keys (last_write_wins)", "[context]") {
  Context target = {{"a", 1}, {"b", 2}};
  Context source = {{"c", 3}, {"d", 4}};
  ContextMergePolicy policy;
  policy.default_strategy = "last_write_wins";

  ContextEngine::merge(target, source, policy);

  REQUIRE(target.contains("a"));
  REQUIRE(target.contains("b"));
  REQUIRE(target.contains("c"));
  REQUIRE(target.contains("d"));
  REQUIRE(target["a"] == 1);
  REQUIRE(target["c"] == 3);
}

TEST_CASE("ContextEngine::merge overlapping keys applies policy", "[context]") {
  Context target = {{"x", "old"}, {"y", 100}};
  Context source = {{"x", "new"}, {"z", 999}};
  ContextMergePolicy policy;
  policy.default_strategy = "last_write_wins";

  ContextEngine::merge(target, source, policy);

  // x 被 source 覆盖, z 是新增
  REQUIRE(target["x"] == "new");
  REQUIRE(target["y"] == 100);
  REQUIRE(target["z"] == 999);
}

TEST_CASE("ContextEngine::merge with error_on_conflict strategy", "[context]") {
  Context target = {{"shared", "original"}};
  Context source = {{"shared", "duplicate"}};
  ContextMergePolicy policy;
  policy.default_strategy = "error_on_conflict";

  // error_on_conflict 在冲突时抛异常, 触发上层错误处理
  REQUIRE_THROWS(ContextEngine::merge(target, source, policy));
}

TEST_CASE("ContextEngine snapshot save and retrieve", "[context]") {
  ContextEngine engine;
  Context ctx = {{"step", "data"}};

  engine.save_snapshot("/snap/v1", ctx);
  const Context* retrieved = engine.get_snapshot("/snap/v1");
  REQUIRE(retrieved != nullptr);
  REQUIRE((*retrieved)["step"] == "data");
}

TEST_CASE("ContextEngine::get_snapshot returns null for unknown key", "[context]") {
  ContextEngine engine;
  REQUIRE(engine.get_snapshot("/not/exists") == nullptr);
}

TEST_CASE("ContextEngine snapshot count limit enforces FIFO", "[context]") {
  ContextEngine engine;
  engine.set_snapshot_limits(/*max_count=*/3, /*max_size_kb=*/1024);

  for (int i = 0; i < 5; ++i) {
    Context ctx = {{"i", i}};
    engine.save_snapshot("/snap/" + std::to_string(i), ctx);
  }

  // 超过 max_count 的快照被拒绝, 最早的 3 个保留
  REQUIRE(engine.get_snapshot("/snap/0") != nullptr);
  REQUIRE(engine.get_snapshot("/snap/1") != nullptr);
  REQUIRE(engine.get_snapshot("/snap/2") != nullptr);
  // 第 4、5 个因 count 限制被拒绝
  REQUIRE(engine.get_snapshot("/snap/3") == nullptr);
  REQUIRE(engine.get_snapshot("/snap/4") == nullptr);
}

TEST_CASE("ContextEngine snapshot size budget enforced on save", "[context]") {
  ContextEngine engine;
  // max_size_kb=1 — 极小限制, 大 snapshot 会被拒绝
  engine.set_snapshot_limits(/*max_count=*/100, /*max_size_kb=*/1);

  std::string big_str(2000, 'x');  // 2 KB > 1 KB 限制
  Context big_ctx = {{"data", big_str}};

  engine.save_snapshot("/snap/oversized", big_ctx);
  // 超过 max_size_kb 的 snapshot 被拒绝
  REQUIRE(engine.get_snapshot("/snap/oversized") == nullptr);
}