// ADR-0074 C2 — Golden Suite 测试: 51 held-out tasks 完整性 + 难度分布
#include "catch_amalgamated.hpp"
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <map>
#include <set>
#include <string>

namespace fs = std::filesystem;

static const std::set<std::string> kValidDimensions = {
    "parse_valid", "task_success", "budget_hit", "error_recovery"};
static const std::set<std::string> kValidDifficulties = {"L1", "L2", "L3"};

TEST_CASE("golden suite has exactly 51 tasks", "[golden][c2]") {
  fs::path golden_dir = "lib/prompts/golden";
  REQUIRE(fs::exists(golden_dir));

  int count = 0;
  for (const auto& entry : fs::directory_iterator(golden_dir)) {
    if (entry.path().extension() == ".yaml") count++;
  }
  REQUIRE(count == 51);
}

TEST_CASE("golden tasks have 5 required fields", "[golden][c2]") {
  fs::path golden_dir = "lib/prompts/golden";
  std::set<std::string> seen_ids;

  for (const auto& entry : fs::directory_iterator(golden_dir)) {
    if (entry.path().extension() != ".yaml") continue;

    YAML::Node node = YAML::LoadFile(entry.path().string());
    REQUIRE(node["task_id"]);
    REQUIRE(node["input"]);
    REQUIRE(node["expected_output"]);
    REQUIRE(node["dimension"]);
    REQUIRE(node["difficulty"]);

    std::string id = node["task_id"].as<std::string>();
    REQUIRE(seen_ids.count(id) == 0);
    seen_ids.insert(id);

    REQUIRE(kValidDimensions.count(node["dimension"].as<std::string>()) == 1);
    REQUIRE(kValidDifficulties.count(node["difficulty"].as<std::string>()) == 1);
  }
}

TEST_CASE("golden difficulty distribution is L1=20 L2=20 L3=11", "[golden][c2]") {
  std::map<std::string, int> dist;
  for (const auto& entry : fs::directory_iterator("lib/prompts/golden")) {
    if (entry.path().extension() != ".yaml") continue;
    YAML::Node node = YAML::LoadFile(entry.path().string());
    dist[node["difficulty"].as<std::string>()]++;
  }
  REQUIRE(dist["L1"] == 20);
  REQUIRE(dist["L2"] == 20);
  REQUIRE(dist["L3"] == 11);
}
