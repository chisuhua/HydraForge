// ADR-0074 C3 + design.md D-4 — measure_prompt_baseline mock-mode 输出 schema 测试
#include "catch_amalgamated.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>

#include <yaml-cpp/yaml.h>

namespace fs = std::filesystem;

// 由 tests/CMakeLists.txt 注入 (generator expression 解析为实际可执行路径)
#ifndef TEST_MEASURE_CLI_PATH
#define TEST_MEASURE_CLI_PATH "./build/tools/measure_prompt_baseline"
#endif

TEST_CASE("measure_prompt_baseline --mock-mode produces valid YAML output", "[measure][c3]") {
  fs::path output = "/tmp/measure_output_test.yaml";
  if (fs::exists(output)) fs::remove(output);

  std::string cmd = std::string(TEST_MEASURE_CLI_PATH)
                  + " --prompt V3 --output " + output.string()
                  + " --mock-mode --max-tasks 3 2>&1";
  int rc = std::system(cmd.c_str());
  REQUIRE(rc == 0);
  REQUIRE(fs::exists(output));

  YAML::Node result = YAML::LoadFile(output.string());

  // D-4 schema 合规校验
  REQUIRE(result["baseline_id"]);
  REQUIRE(result["prompt_version"].as<std::string>() == "V3");
  REQUIRE(result["golden_tasks_total"].as<int>() == 3);
  REQUIRE(result["parse_valid_rate"]);
  REQUIRE(result["task_success_rate"]);
  REQUIRE(result["task_success_rate"]["L1"]);
  REQUIRE(result["task_success_rate"]["L2"]);
  REQUIRE(result["task_success_rate"]["L3"]);
  REQUIRE(result["per_dimension"]);
  REQUIRE(result["per_dimension"]["parse_valid"]);
  REQUIRE(result["per_dimension"]["task_success"]);
  REQUIRE(result["per_dimension"]["budget_hit"]);
  REQUIRE(result["per_dimension"]["error_recovery"]);
  REQUIRE(result["confidence_interval"]);
  REQUIRE(result["confidence_interval"]["parse_valid"]);
  REQUIRE(result["mock_mode"].as<bool>() == true);
  REQUIRE(result["timestamp"]);

  fs::remove(output);
}

TEST_CASE("measure_prompt_baseline rejects empty --output", "[measure][c3]") {
  std::string cmd = std::string(TEST_MEASURE_CLI_PATH)
                  + " --prompt V1 --mock-mode --max-tasks 1 2>&1";
  int rc = std::system(cmd.c_str());
  REQUIRE(rc != 0);
}
