// tests/test_context_fork_merge.cpp
// C2 Day 2-3 (2026-06-27, Sprint 12 P1, ADR-0030 V2 §决策 2)
#include "catch_amalgamated.hpp"

#include "core/types/context.h"
#include <nlohmann/json.hpp>

using agenticdsl::Context;
using agenticdsl::Value;
using nlohmann::json;

TEST_CASE("fork creates deep copy of context",
          "[context][fork][c2-day2]") {
    Context parent;
    parent["user"] = "alice";
    parent["data"] = {{"key1", 1}, {"key2", 2}};
    parent["nested"] = {{"deep", "value"}};

    Context child = agenticdsl::fork(parent);

    REQUIRE(child == parent);
    REQUIRE(child.is_object());
    REQUIRE(child["user"] == "alice");
    REQUIRE(child["data"]["key1"] == 1);
    REQUIRE(child["data"]["key2"] == 2);
    REQUIRE(child["nested"]["deep"] == "value");
}

TEST_CASE("fork returns independent copy (mutations don't affect parent)",
          "[context][fork][c2-day2]") {
    Context parent;
    parent["counter"] = 0;

    Context child = agenticdsl::fork(parent);
    child["counter"] = 42;
    child["new_key"] = "added";

    REQUIRE(parent["counter"] == 0);
    REQUIRE(!parent.contains("new_key"));
    REQUIRE(child["counter"] == 42);
    REQUIRE(child["new_key"] == "added");
}

TEST_CASE("merge child overrides parent values",
          "[context][merge][c2-day2]") {
    Context parent;
    parent["a"] = "parent_a";
    parent["b"] = "parent_b";
    parent["only_parent"] = 100;

    Context child;
    child["a"] = "child_a";
    child["c"] = "child_c";

    Context merged = agenticdsl::merge(child, parent);

    REQUIRE(merged["a"] == "child_a");
    REQUIRE(merged["b"] == "parent_b");
    REQUIRE(merged["c"] == "child_c");
    REQUIRE(merged["only_parent"] == 100);
}

TEST_CASE("merge preserves parent's unique keys when child has same type",
          "[context][merge][c2-day2]") {
    Context parent;
    parent["config"] = {{"timeout", 30}, {"retries", 3}};

    Context child;
    child["config"] = {{"timeout", 60}};

    Context merged = agenticdsl::merge(child, parent);

    REQUIRE(merged["config"]["timeout"] == 60);
    REQUIRE(merged["config"]["retries"] == 3);
}

TEST_CASE("fork + merge roundtrip preserves data",
          "[context][fork][merge][c2-day2]") {
    Context original;
    original["step1"] = "done";
    original["step2"] = {{"status", "ok"}};

    Context node_ctx = agenticdsl::fork(original);
    node_ctx["step2"]["new_field"] = "added_by_node";
    node_ctx["step3"] = "result";

    Context final_ctx = agenticdsl::merge(node_ctx, original);

    REQUIRE(final_ctx["step1"] == "done");
    REQUIRE(final_ctx["step2"]["status"] == "ok");
    REQUIRE(final_ctx["step2"]["new_field"] == "added_by_node");
    REQUIRE(final_ctx["step3"] == "result");
}