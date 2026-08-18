// tests/test_few_shot_examples.cpp
// ADR-0074 C1 — 32 个 few-shot examples 数据完整性测试 (4 维度 × 8)
#include "catch_amalgamated.hpp"
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <set>
#include <string>

namespace fs = std::filesystem;

static const std::set<std::string> kValidDimensions = {
    "parse_valid", "task_success", "budget_hit", "error_recovery"
};

TEST_CASE("few-shot has 8 examples per dimension", "[few-shot][c1]") {
    fs::path fewshot_dir = "lib/prompts/fewshot";
    REQUIRE(fs::exists(fewshot_dir));

    for (const auto& dim : kValidDimensions) {
        int count = 0;
        for (const auto& entry : fs::directory_iterator(fewshot_dir)) {
            if (entry.path().extension() == ".yaml") {
                std::string filename = entry.path().stem().string();
                if (filename.find(dim + "_") == 0) count++;
            }
        }
        REQUIRE(count == 8);
    }
}

TEST_CASE("few-shot examples have 4 required fields", "[few-shot][c1]") {
    fs::path fewshot_dir = "lib/prompts/fewshot";
    for (const auto& entry : fs::directory_iterator(fewshot_dir)) {
        if (entry.path().extension() != ".yaml") continue;

        YAML::Node node = YAML::LoadFile(entry.path().string());
        REQUIRE(node["dimension"]);
        REQUIRE(node["input"]);
        REQUIRE(node["output"]);
        REQUIRE(node["rationale"]);
        REQUIRE(kValidDimensions.count(node["dimension"].as<std::string>()) == 1);
        REQUIRE(node["rationale"].as<std::string>().size() >= 30);
    }
}

TEST_CASE("few-shot yaml-cpp parses all files", "[few-shot][c1]") {
    fs::path fewshot_dir = "lib/prompts/fewshot";
    int parsed = 0;
    for (const auto& entry : fs::directory_iterator(fewshot_dir)) {
        if (entry.path().extension() == ".yaml") {
            REQUIRE_NOTHROW(YAML::LoadFile(entry.path().string()));
            parsed++;
        }
    }
    REQUIRE(parsed == 32);
}
