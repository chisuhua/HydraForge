#include <catch_amalgamated.hpp>

#include <filesystem>
#include <string>
#include "core/session_manager.h"

namespace fs = std::filesystem;

TEST_CASE("SessionManager::list_all_nodes returns appended nodes", "[session-tree]") {
  fs::path dir = fs::temp_directory_path() / "session_tree_test_nodes";
  fs::remove_all(dir);
  agenticdsl::SessionManager sm(dir);
  sm.open("test");
  sm.append_to_branch("hello");
  sm.append_to_branch("world");
  auto nodes = sm.list_all_nodes();
  REQUIRE(nodes.size() == 2);
}

TEST_CASE("SessionManager::get_node_by_short_id matches unique prefix", "[session-tree]") {
  fs::path dir = fs::temp_directory_path() / "session_tree_test_short";
  fs::remove_all(dir);
  agenticdsl::SessionManager sm(dir);
  sm.open("test");
  auto id = sm.append_to_branch("hello");
  auto short_id = id.substr(0, 8);
  auto result = sm.get_node_by_short_id(short_id);
  REQUIRE(result.has_value());
  REQUIRE(result->id == id);
}

TEST_CASE("SessionManager::get_node_by_short_id returns nullopt on ambiguity", "[session-tree]") {
  fs::path dir = fs::temp_directory_path() / "session_tree_test_ambig";
  fs::remove_all(dir);
  agenticdsl::SessionManager sm(dir);
  sm.open("test");
  sm.append_to_branch("a");
  sm.append_to_branch("b");
  auto result = sm.get_node_by_short_id("");
  REQUIRE_FALSE(result.has_value());
}

TEST_CASE("SessionManager::get_branch_leaf_node returns latest leaf", "[session-tree]") {
  fs::path dir = fs::temp_directory_path() / "session_tree_test_leaf";
  fs::remove_all(dir);
  agenticdsl::SessionManager sm(dir);
  sm.open("test");
  sm.append_to_branch("a");
  auto leaf = sm.append_to_branch("b");
  auto result = sm.get_branch_leaf_node(sm.current_branch());
  REQUIRE(result.has_value());
  REQUIRE(result->first.branch_id == sm.current_branch());
  REQUIRE(result->second.id == leaf);
}
