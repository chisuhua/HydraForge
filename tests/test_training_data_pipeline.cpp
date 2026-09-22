// tests/test_training_data_pipeline.cpp
// 功能描述：Wave 3 Phase 1 D3 训练数据准备测试 (ADR-0078 D3 + ADR-0074 D6)
//          scripts/prepare_training_data.py — ADR-0074 D6 JSONL 加 source 字段 + 过滤
//          parse_valid && task_success (AC-5)
// 设计依据：openspec/changes/wave-3-finetune-base-model-pilot-phase1/spec.md R4
//           + design.md D3-1/D3-2/D3-3/D3-4
// 作者：HydraForge Wave 3 Phase 1 ship
// 最后修改日期：2026-09-23

#include "catch_amalgamated.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "nlohmann/json.hpp"

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

// 创建临时 ADR-0074 D6 JSONL fixture (3 records: 1 valid + 1 parse_invalid + 1 task_fail)
fs::path make_d6_fixture() {
  fs::path fixture = fs::temp_directory_path() / "wave3_d6_fixture.jsonl";
  std::ofstream out(fixture);
  // record 1: valid (parse_valid=true, task_success=true)
  out << json{{"prompt", "task 1"},
              {"response", "dsl output 1"},
              {"reward", 1.0},
              {"metadata", {{"task_id", "t1"}, {"domain", "auth"}, {"difficulty", "L1"}}}}
             .dump()
      << "\n";
  // record 2: parse_valid=false
  out << json{{"prompt", "task 2"},
              {"response", "dsl output 2"},
              {"reward", 0.0},
              {"metadata", {{"task_id", "t2"}, {"domain", "auth"}, {"difficulty", "L1"}}},
              {"parse_valid", false},
              {"task_success", true}}
             .dump()
      << "\n";
  // record 3: task_success=false
  out << json{{"prompt", "task 3"},
              {"response", "dsl output 3"},
              {"reward", 0.5},
              {"metadata", {{"task_id", "t3"}, {"domain", "math"}, {"difficulty", "L2"}}},
              {"parse_valid", true},
              {"task_success", false}}
             .dump()
      << "\n";
  out.close();
  return fixture;
}

}  // namespace

// ============================================================
// 1. prepare_training_data 加载 ADR-0074 D6 JSONL 并加 source 字段
// ============================================================
TEST_CASE("prepare_training_data loads ADR-0074 D6 JSONL and adds source field",
          "[training_data][wave3]") {
  fs::path fixture = make_d6_fixture();
  fs::path out = fs::temp_directory_path() / "wave3_train_out.jsonl";
  if (fs::exists(out)) fs::remove(out);

  std::string cmd = "python3 scripts/prepare_training_data.py"
                    " --input " + fixture.string() +
                    " --output " + out.string() +
                    " --source baseline 2>&1";
  int rc = std::system(cmd.c_str());
  REQUIRE(rc == 0);
  REQUIRE(fs::exists(out));

  std::ifstream in(out);
  std::string line;
  int lines = 0;
  while (std::getline(in, line)) {
    if (line.empty()) continue;
    json record = json::parse(line);
    REQUIRE(record.contains("source"));
    REQUIRE(record["source"] == "baseline");
    ++lines;
  }
  REQUIRE(lines >= 1);
  fs::remove(fixture);
  fs::remove(out);
}

// ============================================================
// 2. prepare_training_data 过滤 parse_valid && task_success
// ============================================================
TEST_CASE("prepare_training_data filters parse_valid && task_success",
          "[training_data][wave3]") {
  fs::path fixture = make_d6_fixture();
  fs::path out = fs::temp_directory_path() / "wave3_train_filtered.jsonl";
  if (fs::exists(out)) fs::remove(out);

  std::string cmd = "python3 scripts/prepare_training_data.py"
                    " --input " + fixture.string() +
                    " --output " + out.string() + " 2>&1";
  int rc = std::system(cmd.c_str());
  REQUIRE(rc == 0);
  REQUIRE(fs::exists(out));

  std::ifstream in(out);
  std::string line;
  int lines = 0;
  while (std::getline(in, line)) {
    if (line.empty()) continue;
    json record = json::parse(line);
    // 过滤后所有记录必须 parse_valid=true && task_success=true
    bool parse_valid = record.value("parse_valid", true);
    bool task_success = record.value("task_success", true);
    REQUIRE(parse_valid);
    REQUIRE(task_success);
    ++lines;
  }
  // 3 records 输入 → 仅 1 valid 通过
  REQUIRE(lines == 1);
  fs::remove(fixture);
  fs::remove(out);
}
